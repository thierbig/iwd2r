#!/usr/bin/env python3
"""wave.py -- drive the disagreement batches and the wave closes.

The coordinator's wave loop, one command each::

    python3 tools/wave.py next                 # pick, cut and triage the next batch
    python3 tools/wave.py brief b001           # the text to dispatch verbatim
    python3 tools/wave.py verdict b001 0x643CC0 ours probe-9
    python3 tools/wave.py close b001           # gates, then ff `first`
    python3 tools/wave.py resolve 0x716E80 ours dump-716E80   # a probe-list row into its batch
    python3 tools/wave.py undecide b016 0x5D79B0 fork <reason>  # a batch row onto the probe list
    python3 tools/wave.py unpark 0x5152C0 [--reason TEXT]     # a parked import row back to the queue
    python3 tools/wave.py requeue [--dry-run] [--reason TEXT] # every rule-1 row parked only for
                                                # lack of coverage back to the queue (rule 3)
    python3 tools/wave.py close b002 --kind import   # waves 2-5
    python3 tools/wave.py scorecard            # the tally into xrec-report.md
    python3 tools/wave.py remeasure (--rows 0xA 0xB .. | --read-only-measured |
        --dump-decided [--batch bNNN]) [--chunk 40] [--dry] [--branch NAME]
                                                # Frida-only pass over closed rows: trace
                                                # evidence on a p3/remeasure-<stamp> branch
    python3 tools/wave.py status
    python3 tools/wave.py unmeasured-imports   # every import no scenario exercised (rule 1)
    python3 tools/wave.py recompare [--tip SHA] [--no-fetch]   # queue what Will's tip adds
    python3 tools/wave.py run [--queue wave1|import|probes] [--pool SPEC]... [--budget C] [--seed S]   # the driver
    python3 tools/wave.py next --queue import   # cut an import batch i<NNN>: rule-1 rows all
                                                # offered (unmeasured import when uncovered);
                                                # base-body rows only when covered, the rest
                                                # kept by default (rule 2)
    python3 tools/wave.py next --queue probes   # cut a probe batch p<NNN> from the probe list: the worker
                                                # adopts the fork body, the Frida phase decides
    python3 tools/wave.py close p001 --kind probes
    python3 tools/wave.py coverage [--scenarios a,b] [--chunk N] [--addresses FILE] [--dry]
    python3 tools/wave.py coverage --pending [--chunk N] [--dry]   # measure what recompare queued
                                                # original-exe hit map per scenario
    python3 tools/wave.py addresses [--dry]    # an address for every address-less row (sub-name,
                                                # Ghidra symbol, our label, the fork's header
                                                # comment), written back into the file
    python3 tools/wave.py run --dry-run --state COPY.json   # one batch, fake worker
    python3 tools/wave.py run --clear b011      # the coordinator's retry of an exception
    python3 tools/wave.py run --clear b011 --rerun   # a checks/review exception re-runs the worker (plain --clear re-checks the last report)
    python3 tools/wave.py run --queue import --clear i011 --rerun-frida   # a record/close
                                                # exception re-runs the batch's Frida phase
                                                # on its worktree (measuring or marking what
                                                # close asks for, a parked row the batch
                                                # imported included), then records and closes;
                                                # the worker and the review are not run again;
                                                # a plain --clear keeps R71 (no phase rerun)

Rows come from docs/wso/disagreements.json (tools/xrec.py); batch state
lives in docs/wso/wave-state.json, created by the first `next`.  Nothing
is written by hand: `next` picks the rows, cuts the batch's branch and
worktree from `first` and applies docs/wso/triage.json (auto verdicts for
the classes calibration enables, ours-stub rows to
docs/wso/import-list.json), `close` checks the batch's worktree itself
(verdicts, evidence lines, clean tree, an unmodified gate, gates), then
fast-forwards `first` to the batch branch.

Exit codes: 0 ok, 1 refusal (close lists every reason), 2 usage error or
nothing left to assign.  `run` (tools/driver.py) exits 0 when every row
is assigned or decided or at the budget stop, 1 when an exception that is
not a batch's own (cut, push) stopped it, it halted (a launch failure, an
auth or billing API error, the breaker), the status file cannot be read
or another driver runs on it, 3 when parked, 4 when stalled (the rows
left wait on files owned by assigned batches).  Every state write is atomic (temp file in the
same directory, then os.replace), so a killed `next` cannot corrupt it.
"""
from __future__ import annotations

import datetime
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'wave-state.json')
DISAGREEMENTS_PATH = os.path.join(ROOT, 'docs', 'wso', 'disagreements.json')
TEMPLATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')
REPORT_PATH = os.path.join(ROOT, 'docs', 'wso', 'xrec-report.md')
TRIAGE_PATH = os.path.join(ROOT, 'docs', 'wso', 'triage.json')
CALIBRATION_PATH = os.path.join(ROOT, 'docs', 'wso', 'truth-calibration.json')
# Relative to the repo `next` is given: where ours-stub rows go when they
# leave wave 1.
IMPORT_LIST_REL = os.path.join('docs', 'wso', 'import-list.json')
# Rows a worker could not decide (ruling R73): they wait for a probe.
PROBE_LIST_REL = os.path.join('docs', 'wso', 'probe-list.json')
# Import rows a batch dropped (no scenario measured them, the fork body was
# refused, ...): the body edited back, the evidence deleted, and one file per
# row here, written by the worker (one file per row, so parallel batches never
# conflict); `close --kind import` accepts them as dropped.
IMPORT_PARKED_DIR = 'docs/wso/import-parked'
# The single list it replaced; a batch that still carries it is refused.
RETIRED_PARKED_REL = 'docs/wso/import-parked.json'
DISAGREEMENTS_REL = os.path.join('docs', 'wso', 'disagreements.json')
# The fork is read by pinned sha, never by branch name (wso/main moves).
DEFAULT_FORK_BASE = 'e8dd16a750966af53f24daaf49dcb4fb13594c13'
# The common ancestor of both trees; the base of every xrec comparison.
COMMON_BASE = '4fa36bcd'
FORK_REMOTE = 'wso'
WSO_DELTA_HEAD = '## WSO delta'

DEFAULT_SIZE = 20
DEFAULT_CATEGORY = 'both_changed_differ'
# The import queue's sources, in the order `next --queue import` draws
# from them: rows where ours is a stub, then rows where ours is still the
# base and his fork changed it, then (Task 22) rows only his tree has.
# IMPORT_CATEGORY (the first) stays as an alias for what imports it.
IMPORT_CATEGORIES = ('theirs_real_ours_stub', 'theirs_changed_ours_base', 'only_theirs')
IMPORT_CATEGORY = IMPORT_CATEGORIES[0]
# Rule 1 (2026-10-26 ruling): where we lack the body the import needs no
# coverage.  These are the categories whose parked-only-for-coverage rows
# `requeue` returns to the queue.
RULE1_CATEGORIES = (IMPORT_CATEGORIES[0], IMPORT_CATEGORIES[2])
REQUEUE_NEEDS_END = 'not exercised'
REQUEUE_REASON = 'rule 1: no coverage requirement (2026-10-26 ruling)'
CATEGORY_LABEL = {'theirs_real_ours_stub': 'ours is a stub',
                  'theirs_changed_ours_base': 'ours is the base body',
                  'only_theirs': 'ours has no body'}
IMPORT_STATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'import-state.json')
IMPORT_TEMPLATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'import-brief-template.md')
# The two queues (ruling R11).  wave1 cuts the both_changed_differ rows,
# triaged, as b<NNN>; import cuts the theirs_real_ours_stub rows plus the
# import list, untriaged, as i<NNN>, so branches and worktrees never
# collide.  Both use the wave-state schema.  `status` is the driver's
# status file name beside its ledger (ruling R42); `kind` is what the
# driver closes with.
QUEUES = {
    'wave1': {'state': STATE_PATH, 'category': DEFAULT_CATEGORY,
              'prefix': 'b', 'triage': True, 'kind': 'verdicts',
              'template': TEMPLATE_PATH, 'status': 'status.json'},
    'import': {'state': IMPORT_STATE_PATH, 'category': IMPORT_CATEGORY,
               'categories': IMPORT_CATEGORIES,
               'prefix': 'i', 'triage': False, 'kind': 'import',
               'template': IMPORT_TEMPLATE_PATH,
               'status': 'status-import.json'},
}
# The third queue (Task 28): batches p<NNN> of probe-list rows.  The worker
# adopts the fork body per row, the driver's Frida phase decides it (equal:
# verdict fork through resolve_probe; different: a finding; not exercised:
# the row stays on the list).  Its rows are the probe list's entries.
PROBE_STATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'probe-state.json')
PROBE_TEMPLATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'probe-brief-template.md')
PROBE_CATEGORY = 'probe_list'
QUEUES['probes'] = {'state': PROBE_STATE_PATH, 'category': PROBE_CATEGORY,
                    'prefix': 'p', 'triage': False, 'kind': 'probes',
                    'template': PROBE_TEMPLATE_PATH, 'status': 'status-probes.json'}
SIDES = ('fork', 'ours', 'neither')
# Scorecard display order, strongest verification first (the same kinds
# check_evidence.py knows).
DISPLAY_ORDER = ('oracle', 'replay', 'read-only', 'trace')
# `close --kind import` accepts trace or better; wave-1 closes read the
# body phrases instead (EVIDENCE_PHRASES).
IMPORT_KINDS = ('trace', 'replay', 'oracle')
EVIDENCE_PHRASES = ('disagreed with WSO', 'corroborated by WSO')
EVIDENCE_DIR = 'docs/evidence'
# Rule 1 of the 2026-10-26 ruling: where we lack the body (only_theirs,
# theirs_real_ours_stub) a row no scenario exercises is imported, not parked.
# Its evidence file stays `kind: read-only` and its `exemption:` starts with
# this marking (the driver writes it, never the worker, like the frida
# trace_source line); check_evidence accepts it as a non-empty exemption,
# `close --kind import` accepts it through _import_bar, and `unmeasured-imports`
# lists every file that carries it.
UNMEASURED_WSO = 'unmeasured WSO import'
KIND_RE = re.compile(r'^kind:\s*(\S+)\s*$', re.M)
# A batch branch must not change the gate that certifies it; `close`
# refuses when the branch's diff against the base touches either path.
GATE_PATHS = ('tools/gates.sh', 'tools/parity')


def _now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec='seconds')


def _addr_key(address):
    """0x643CC0 / 643cc0 -> 643cc0: one spelling per address."""
    return (address or '').removeprefix('0x').lower()


def _evidence_rel(address):
    return f'{EVIDENCE_DIR}/{_addr_key(address).upper()}.md'


def load_state(path=STATE_PATH):
    try:
        with open(path, encoding='utf-8') as f:
            state = json.load(f)
    except FileNotFoundError:
        return {'batches': {}}
    if not isinstance(state, dict) or not isinstance(state.get('batches', {}), dict):
        raise ValueError(f'{path}: not a wave-state file')
    state.setdefault('batches', {})
    return state


def _save_json(path, data):
    """Write JSON atomically: a temp file in the same directory, then
    os.replace."""
    d = os.path.dirname(os.path.abspath(path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.wave-', suffix='.json')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
            f.write('\n')
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def save_state(path, state):
    _save_json(path, state)


def _load_list(path, what, missing_ok=False):
    """(entries, None) for a JSON list of objects, or (None, error).  A
    missing file is an empty list when missing_ok."""
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
    except FileNotFoundError:
        if missing_ok:
            return [], None
        return None, f'no {what} {path}\n'
    except (OSError, ValueError) as e:
        return None, f'{what} {path}: {e}\n'
    if not isinstance(data, list) or not all(
            isinstance(e, dict) and 'address' in e for e in data):
        return None, f'{what} {path}: not a list of rows with an address\n'
    return data, None


_triage_module = None


def _triage():
    """tools/triage.py, loaded by path: under -I tools/ is not on sys.path."""
    global _triage_module
    if _triage_module is None:
        spec = importlib.util.spec_from_file_location(
            'triage', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   'triage.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _triage_module = module
    return _triage_module


_coverage_module = None


def _coverage():
    """tools/coverage.py, loaded by path (tools/ is not on sys.path under -I)."""
    global _coverage_module
    if _coverage_module is None:
        spec = importlib.util.spec_from_file_location(
            'wave_coverage', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                          'coverage.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _coverage_module = module
    return _coverage_module


_truth_module = None


def _truth():
    """tools/truth.py, loaded by path: under -I tools/ is not on sys.path."""
    global _truth_module
    if _truth_module is None:
        spec = importlib.util.spec_from_file_location(
            'truth', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  'truth.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _truth_module = module
    return _truth_module


_driver_module = None


def _driver():
    """tools/driver.py, loaded by path: under -I tools/ is not on sys.path."""
    global _driver_module
    if _driver_module is None:
        spec = importlib.util.spec_from_file_location(
            'driver', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   'driver.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _driver_module = module
    return _driver_module


_check_evidence_module = None


def _check_evidence():
    """tools/check_evidence.py, loaded by path (tools/ is not on sys.path
    under -I)."""
    global _check_evidence_module
    if _check_evidence_module is None:
        spec = importlib.util.spec_from_file_location(
            'check_evidence', os.path.join(
                os.path.dirname(os.path.abspath(__file__)), 'check_evidence.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _check_evidence_module = module
    return _check_evidence_module


def merge_base_sha(worktree, base):
    """The full sha where the worktree's HEAD left base, or None."""
    r = subprocess.run(['git', '-C', worktree, 'merge-base', base, 'HEAD'],
                       capture_output=True, text=True)
    return r.stdout.strip() or None if r.returncode == 0 else None


ORACLE_CASE_RE = re.compile(r'ORACLE_CASE(?:_[A-Z]+)?\(\s*"([^"]+)"')


def _oracle_case_names(worktree):
    """Every case id registered by an oracle/cases/*.cpp of the worktree
    that oracle/CMakeLists.txt lists, whatever the spelling of the family
    (ORACLE_CASE, ORACLE_CASE_EXPECT, ORACLE_CASE_PENDING — a pending case
    stands as written): a case file the runner does not build never runs,
    so its ids count for nothing."""
    names = set()
    folder = os.path.join(worktree, 'oracle', 'cases')
    try:
        with open(os.path.join(worktree, 'oracle', 'CMakeLists.txt'), encoding='utf-8',
                  errors='replace') as f:
            cmake = '\n'.join(line.split('#', 1)[0] for line in f.read().splitlines())
    except OSError:
        return names
    wired = set(re.findall(r'(?<![\w/.-])cases/([A-Za-z0-9_.+-]+\.cpp)(?![\w.])', cmake))
    for name in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
        if name.endswith('.cpp') and name in wired:
            with open(os.path.join(folder, name), encoding='utf-8', errors='replace') as f:
                names.update(ORACLE_CASE_RE.findall(f.read()))
    return names


def frida_trace(text):
    """True when an evidence text is `kind: trace` with a frida
    trace_source (check_evidence.TRACE_SOURCES[1]): the line only the
    driver's Frida phase writes."""
    ce = _check_evidence()
    fields = ce.front_matter(text or '')
    return fields.get('kind') == 'trace' and bool(
        ce.TRACE_SOURCES[1].fullmatch(fields.get('trace_source') or ''))


def _frida_probe(text):
    """The probe name of an evidence text's frida trace_source line ('' when
    it has none): `frida <probe> orig:... ours:... compare=0`."""
    ts = _check_evidence().front_matter(text or '').get('trace_source') or ''
    parts = ts.split()
    return parts[1] if len(parts) > 1 and parts[0] == 'frida' else ''


def other_batch_trace(text, batch_id):
    """True when an evidence text is a frida trace another batch's phase
    wrote (its probe is not `<batch_id>-...`).  It measured the body that
    batch had, which may not be the one on this branch (nothing ties the
    trace to a body), so it does not stand for a kept row: the phase
    measures the row again (Task 19, round 4)."""
    return frida_trace(text) and not _frida_probe(text).startswith(f'{batch_id}-')


def kept_evidence(worktree, merge_base, address, name, text):
    """Why an import row's evidence stands as it is written, or None when
    the Frida phase must measure it (the whole-branch review's ruling):
    `kind: oracle` with an oracle_case that a case file in the worktree's
    oracle/cases registers and oracle/CMakeLists.txt wires; `kind: trace`
    citing a fork recovery note that names the function
    (check_evidence.fork_note_mentions, the note read with git show); or an evidence file unchanged since the merge base whose
    kind is trace or better.  Anything else a worker wrote (read-only, a
    declared replay, an oracle without its case, a trace citing anything
    else) is measured; a frida line is the driver's alone."""
    ce = _check_evidence()
    fields = ce.front_matter(text or '')
    kind = fields.get('kind')
    case = fields.get('oracle_case')
    if kind == 'oracle' and case and case in _oracle_case_names(worktree):
        return f'oracle case {case}'
    m = ce.TRACE_SOURCES[0].fullmatch(fields.get('trace_source') or '')
    if kind == 'trace' and m:
        try:
            addr = int(_addr_key(address), 16)
        except ValueError:
            addr = None
        ok, _why = ce.fork_note_mentions(
            m.group(1), m.group(2), addr, name,
            run=lambda argv, **kw: subprocess.run(argv, cwd=worktree, **kw)) \
            if addr is not None else (False, '')
        if ok:
            return f'fork note {m.group(2)}'
    if merge_base and kind in IMPORT_KINDS:
        at = subprocess.run(['git', '-C', worktree, 'show',
                             f'{merge_base}:{_evidence_rel(address)}'],
                            capture_output=True, text=True)
        if at.returncode == 0 and _without_waiver(at.stdout) == _without_waiver(text):
            return 'unchanged since the merge base'
    return None


def _without_waiver(text):
    """An evidence text minus its front matter's parity_waiver line: a waiver
    records a parity reading, not a measurement, so a batch that adds or
    rewords one leaves evidence that stood at the merge base standing
    (2026-10-30: i051/i052 added waivers to kind-replay ExecuteAction files)."""
    head, sep, rest = text.partition('\n---\n')
    if not sep:
        return text
    lines = [l for l in head.split('\n') if not l.startswith('parity_waiver:')]
    return '\n'.join(lines) + sep + rest


def _evidence_at(repo, ref, address):
    """An evidence file's text as `ref` holds it (git show), or None when
    git cannot read it (no repo there, no such branch, no such file)."""
    at = subprocess.run(['git', '-C', repo, 'show', f'{ref}:{_evidence_rel(address)}'],
                        capture_output=True, text=True)
    return at.stdout if at.returncode == 0 else None


def _oracle_backed(repo, base, address, case_names):
    """Why the evidence `base` holds measures an import row on its own:
    `kind: oracle` with an oracle_case base's oracle/cases registers (the
    case IS the row's measurement, so no scenario need hit it).  A case
    that recorded a mismatch left no kind behind, so it backs nothing
    here.  `case_names` is _oracle_case_names of base's worktree; empty
    (base is not checked out) backs nothing."""
    text = _evidence_at(repo, base, address)
    if text is None:
        return None
    fields = _check_evidence().front_matter(text)
    case = fields.get('oracle_case')
    if fields.get('kind') == 'oracle' and case and case in case_names:
        return f'oracle case {case}'
    return None


def _mismatch_finding(repo, base, address):
    """The Finding note a mismatch-recorded evidence file at `base` carries
    (an oracle case that ran and disagreed: no kind, a `Finding (runs/...)
    ` paragraph under ## Notes), or None.  Task 39: the worker who takes
    such a row must see what the case found — ours is wrong there."""
    text = _evidence_at(repo, base, address) if address else None
    if text is None or _check_evidence().front_matter(text).get('kind'):
        return None
    m = re.search(r'^## Notes\s*$(.*?)(?=^## |\Z)', text, re.M | re.S)
    f = m and re.search(r'^(Finding \(runs/[^)]+\):.*?)(?:\n\n|\Z)',
                        m.group(1), re.M | re.S)
    return ' '.join(f.group(1).split()) if f else None


def unmeasured_wso(text):
    """True when an evidence text carries the unmeasured WSO import marking
    (rule 1 of the 2026-10-26 ruling): an `exemption:` whose value starts
    with UNMEASURED_WSO.  Only the driver writes it, so a frida trace or an
    oracle case never needs it."""
    ex = _check_evidence().front_matter(text or '').get('exemption') or ''
    return isinstance(ex, str) and ex.strip().startswith(UNMEASURED_WSO)


def _import_bar(worktree, merge_base, address, name, rel, text, batch_id=None, want=None,
                file=None):
    """The close's problem with an import function's evidence, or None:
    trace or better, and either the driver's frida trace or evidence that
    stands as written (kept_evidence).  With `batch_id`, a frida trace another
    batch wrote measured that batch's body: it passes only while the body
    here is the one this batch's phase recorded (`want`, a measured_body or
    measured_deps digest), never as evidence that stands."""
    if unmeasured_wso(text):
        # The driver marked this row an unmeasured WSO import (rule 1): the
        # marking stands as its evidence, like the two forms kept_evidence
        # knows, so the close holds it to the build, checks and gate only.
        return None
    m = KIND_RE.search(text)
    have = m.group(1) if m else 'none'
    if have not in IMPORT_KINDS:
        return (f'{rel}: kind {have} is not trace or better; let the Frida phase '
                f'measure it, or park it ({parked_rel(address)})')
    if batch_id and other_batch_trace(text, batch_id):
        if want and _check_evidence().body_digest(worktree, address, file) == want:
            return None
        return (f'{rel}: its frida trace is another batch\'s ({_frida_probe(text)}): it measured the '
                f'body that batch had; let the Frida phase measure it here, or park it '
                f'({parked_rel(address)})')
    if frida_trace(text) or kept_evidence(worktree, merge_base, address, name, text):
        return None
    return (f'{rel}: kind {have} was written in this batch and nothing measured it: only '
            f'the driver\'s frida trace, an oracle case registered in a case file '
            f'oracle/CMakeLists.txt wires, a fork '
            f'recovery note that names the function, or evidence unchanged since the merge '
            f'base stands; let the Frida phase measure it, or park it ({parked_rel(address)})')


def parked_rel(address):
    """docs/wso/import-parked/<ADDR>.json: where one parked row's entry lives."""
    return f'{IMPORT_PARKED_DIR}/{_addr_key(address).upper()}.json'


def parked_stem_key(name):
    """The address key of a parked file's name (`0x401000.json`, `401000.JSON`
    and `401000.json` all name 401000): the one spelling rule of the folder."""
    stem = name[:-len('.json')] if name.lower().endswith('.json') else name
    return _addr_key(stem.lower())


def parked_path_key(rel):
    """The address key of a repo path under docs/wso/import-parked/, else None."""
    head, _, name = rel.rpartition('/')
    return parked_stem_key(name) if head == IMPORT_PARKED_DIR and name.lower().endswith(
        '.json') else None


def _parked_rows(worktree):
    """({address key: entry}, problem or None) of the worktree's
    docs/wso/import-parked/<ADDR>.json files: the rows import batches
    dropped, one JSON object per file ({address, name, file, batch, needs}),
    named after its row.  A file that is not such an object, or names
    another address, is a problem; so is the retired single list."""
    out, problems = {}, []
    if os.path.exists(os.path.join(worktree, RETIRED_PARKED_REL)):
        problems.append(f'{RETIRED_PARKED_REL} is retired: write one file per row, '
                        f'{IMPORT_PARKED_DIR}/<ADDR>.json, and delete it')
    folder = os.path.join(worktree, IMPORT_PARKED_DIR)
    names = sorted(os.listdir(folder)) if os.path.isdir(folder) else []
    for name in names:
        rel = f'{IMPORT_PARKED_DIR}/{name}'
        stem = name[:-len('.json')] if name.endswith('.json') else None
        if not stem or not re.fullmatch(r'(?:0[xX])?[0-9A-Fa-f]+', stem):
            problems.append(f'{rel}: not a parked row; the folder holds only <ADDR>.json files')
            continue
        try:
            with open(os.path.join(folder, name), encoding='utf-8') as f:
                entry = json.load(f)
        except (OSError, ValueError) as e:
            problems.append(f'{rel} is unreadable ({e!r}); it is one JSON object '
                            f'{{"address", "name", "file", "batch", "needs"}}')
            continue
        if not isinstance(entry, dict) or not isinstance(entry.get('address'), str):
            problems.append(f'{rel}: no string address ({entry!r}); it is one JSON object '
                            f'{{"address", "name", "file", "batch", "needs"}}')
            continue
        if _addr_key(entry['address'].lower()) != _addr_key(stem.lower()):
            problems.append(f'{rel} records address {entry["address"]}; name the file '
                            f'after its row ({parked_rel(entry["address"])})')
            continue
        out[parked_stem_key(name)] = entry
    return out, ('; '.join(problems) or None)


_assemble_module = None


def _assemble_bundle(address):
    """assemble_context.bundle, loaded by path (tools/ is not on sys.path
    under -I)."""
    global _assemble_module
    if _assemble_module is None:
        spec = importlib.util.spec_from_file_location(
            'assemble_context',
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         'assemble_context.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _assemble_module = module
    return _assemble_module.bundle(address)


# -- next ---------------------------------------------------------------

def default_worktree_root(repo=ROOT):
    """Where batch worktrees live: the main worktree's path (the first one
    `git worktree list` names) plus '-worktrees'.  None outside a repo."""
    r = subprocess.run(['git', '-C', repo, 'worktree', 'list', '--porcelain'],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if line.startswith('worktree '):
            return line[len('worktree '):] + '-worktrees'
    return None


def _cut_worktree(repo, branch, base, worktree_root=None):
    """The batch branch's worktree: the one it already has, else a new one
    at <worktree_root>/<branch, / as -> with the branch cut from `base` (or
    checked out as it is, when the branch exists without a worktree).
    (path, how) or (None, error)."""
    wt = worktree_for(repo, branch)
    if wt:
        return wt, 'its existing worktree'
    root = worktree_root or default_worktree_root(repo)
    if root is None:
        return None, f'no main worktree found for {repo}'
    path = os.path.join(root, branch.replace('/', '-'))
    def rev(ref):
        r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify',
                            '--quiet', ref], capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else None

    exists = rev(f'refs/heads/{branch}') is not None
    if exists:
        cmd = ['git', '-C', repo, 'worktree', 'add', '-q', path, branch]
    else:
        start = rev(f'{base}^{{commit}}')
        if start is None:
            return None, f'{base} not found in {repo}'
        cmd = ['git', '-C', repo, 'worktree', 'add', '-q', '-b', branch, path,
               start]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        if not exists:
            # git creates the branch before it fails on the path; drop it,
            # only while it still points where this call put it.
            subprocess.run(['git', '-C', repo, 'update-ref', '-d',
                            f'refs/heads/{branch}', start],
                           capture_output=True, text=True)
        return None, (f'git worktree add {path} {branch} failed: '
                      f'{r.stderr.strip()}')
    return path, ('the existing branch, new worktree' if exists
                  else f'new branch from {base}')


def _filelock():
    spec = importlib.util.spec_from_file_location(
        'filelock', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 'filelock.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def next_batch(disagreements_path=DISAGREEMENTS_PATH, state_path=STATE_PATH,
               *args, claims_path=None, **kw):
    """Cut the next batch (see _next_batch).  The read of every queue's
    state and the save of this one happen under the claims file lock, the
    one tools/claims.py takes, so two drivers on the two queues cannot
    both pick the same file (ruling R64)."""
    if claims_path is None:
        claims_path = os.path.join(
            os.path.dirname(os.path.abspath(state_path)), 'claims.json')
    with _filelock().hold(claims_path + '.lock', 'wave next'):
        return _next_batch(disagreements_path, state_path, *args,
                           claims_path=claims_path, **kw)


def probe_hits(row, disagreements_path, coverage_path=None, coverage_root=None, repo=None):
    """The scenarios the coverage map says hit a probe row (the candidates of
    the phase for it, no hold-out filter), sorted; [] without a usable map."""
    cov_mod = _coverage()
    cpath = coverage_path or os.path.join(
        os.path.dirname(os.path.abspath(disagreements_path)), 'coverage.json')
    try:
        cov, _why = cov_mod.check(cpath, coverage_root or repo or ROOT)
        if cov is None:
            return []
        return sorted(cov_mod.covered(cov, row['address'], cov_mod.row_scenarios(cov, [row])))
    except (TypeError, AttributeError, ValueError, KeyError, OSError):
        return []


def _probe_reoffer(row, disagreements_path, coverage_path, coverage_root, repo):
    """Whether a probe row a closed batch released may be cut again.  The
    phase stamped the entry (`probed`: batch, scenarios that ran, `hits` the
    scenarios the map gave the row then, pin, `hand` for a neither / hand-ruling
    / found-different row).  Offered again when the pin moved, or (never for a
    `hand` row) when the map now has a hitting scenario in neither `hits` nor
    `scenarios`.  An unstamped row,
    or one with no usable map and the same pin, is not."""
    stamp = (row or {}).get('probed')
    if not isinstance(stamp, dict) or not stamp:
        return False
    if stamp.get('pin') != row.get('fork_base'):
        return True
    if stamp.get('hand'):
        return False
    seen = set(stamp.get('hits') or ()) | set(stamp.get('scenarios') or ())
    now = probe_hits(row, disagreements_path, coverage_path, coverage_root, repo)
    return any(sc not in seen for sc in now)


def _import_pool(rows, category, decided, owned, importing, disagreements_path,
                 coverage_path, coverage_root, repo, include_uncovered, base='first'):
    """(eligible rows, rows without an address, coverage note, kept by
    default, None) for one category, or ([], [], None, None, (rc, message))
    when it offers nothing.

    The coverage requirement is per category (2026-10-26 ruling): where we
    lack the body (theirs_real_ours_stub, only_theirs -- rule 1) every
    candidate is offered, the ones no scenario exercises too, ordered
    covered-first; where we both have the body (theirs_changed_ours_base --
    rule 2) only the rows the map exercises are cut, and the rest are
    returned as kept by default for the state to record (ours stays, no
    worker).  With no usable map the candidate order is unchanged, and
    include_uncovered (kept for the CLI it still parses) no longer changes
    either cut."""
    def in_category(r):
        return r.get('category') == category

    no_addr = [r for r in rows if in_category(r) and not r.get('address')]
    candidates = []
    seen = set()
    for r in rows:
        if (in_category(r) and r.get('address')
                and _addr_key(r['address']) not in decided
                and _addr_key(r['address']) not in seen):
            seen.add(_addr_key(r['address']))
            candidates.append(r)
    eligible = [r for r in candidates if r['file'] not in owned]
    skip_note = None
    if importing and category == 'only_theirs' and repo:
        # An only-theirs row whose address our src labels at the base is
        # present, not an import: ours already carries the body, and
        # recompare reclassifies it.  The pool offers the rest.
        present = {_addr_key(r['address']) for r in eligible
                   if _labelled_at(repo, base, r['address'])}
        if present:
            eligible = [r for r in eligible
                        if _addr_key(r['address']) not in present]
            skip_note = ('%d only_theirs row(s) skipped: ours already carries '
                         'the body (recompare will reclassify)' % len(present))
            if not eligible:
                return [], [], None, None, (2, skip_note + '\n')
    if not eligible:
        if candidates:
            return [], [], None, None, (2, 'nothing left to assign: %d row%s wait%s on files '
                                 'owned by assigned batches or claimed by streams\n'
                                 % (len(candidates), 's' if len(candidates) != 1 else '',
                                    '' if len(candidates) != 1 else 's'))
        return [], [], None, None, (2, f'nothing left: every {category} row is assigned or decided\n')
    eligible.sort(key=lambda r: (-(r['their_len'] + r['our_len']),
                                 r['file'], r['name']))
    coverage_note = None
    kept_default = None            # rule 2 fills this for base-body pools
    if importing:
        # The coverage map (tools/coverage.py): covered rows first, most
        # scenarios first.  Rule 1 offers the unexercised rows too (they
        # import unmeasured); rule 2 keeps base-body ones by default.
        cov_mod = _coverage()
        cpath = coverage_path or os.path.join(
            os.path.dirname(os.path.abspath(disagreements_path)), 'coverage.json')
        cov, why = cov_mod.check(cpath, coverage_root or repo or ROOT)
        if cov is None:
            coverage_note = 'coverage: %s; candidate order unchanged' % why
        else:
            try:
                # per row: its candidates (UI screens for CScreen/CUI files, plus
                # any UI scenario the map says hits it), the phase's own rule
                state_of = {id(r): cov_mod.status_of(cov, r['address'],
                                                     cov_mod.row_scenarios(cov, [r]))
                            for r in eligible}
                # a row Frida refuses everywhere queues as it did before the
                # status existed (unknown); so does one refused in some
                # scenarios and dead in the rest
                state_of = {id(r): ('unknown' if state_of[id(r)] == 'unhookable'
                                    or (state_of[id(r)] == 'dead' and cov_mod.refused_in(
                                        cov, r['address'], cov_mod.row_scenarios(cov, [r])))
                                    else state_of[id(r)]) for r in eligible}
                count = {id(r): len(cov_mod.covered(cov, r['address'])) for r in eligible}
            except (TypeError, AttributeError, ValueError, KeyError) as e:
                state_of = None
                coverage_note = 'coverage: map unreadable (%r); candidate order unchanged' % (e,)
            if state_of is not None:
                # Task 39: a registered oracle case measures a row on its own,
                # so such a row is covered whatever the map says and sorts
                # with the covered rows; a mismatch record (no kind) is not.
                base_wt = worktree_for(repo, base) if repo else None
                case_names = _oracle_case_names(base_wt) if base_wt else set()
                oracle_rows = []
                if case_names:
                    oracle_rows = [r for r in eligible if state_of[id(r)] != 'hit'
                                   and _oracle_backed(repo, base, r['address'], case_names)]
                oracle = {id(r) for r in oracle_rows}
                hit = [r for r in eligible if state_of[id(r)] == 'hit']
                # stale hits rank above unknown but never decide: such a row is
                # unknown for the exercises-it decision
                stale_hit = [r for r in eligible if state_of[id(r)] == 'unknown'
                             and cov_mod.covered(cov, r['address'], include_stale=True)
                             and id(r) not in oracle]
                unknown = [r for r in eligible if state_of[id(r)] == 'unknown'
                           and r not in stale_hit and id(r) not in oracle]
                dead = [r for r in eligible if state_of[id(r)] == 'dead'
                        and id(r) not in oracle]
                hit.sort(key=lambda r: -count[id(r)])   # stable: size order within a count
                notes = []
                if category == 'theirs_changed_ours_base':
                    # Rule 2: where we both have the body, ours stays by
                    # default, so only the rows a scenario exercises are cut;
                    # the rest go back to the caller as kept by default.
                    kept_default = stale_hit + unknown + dead
                    eligible = hit + oracle_rows
                    if kept_default:
                        notes.append('coverage: %d base-body row(s) kept by default: no '
                                     'scenario exercises them' % len(kept_default))
                else:
                    # Rule 1: where we lack the body there is no coverage
                    # requirement, so every candidate is offered, the ones no
                    # scenario exercises too (imported unmeasured, marked by
                    # the Frida phase) and the unknowns in between.
                    eligible = (hit + oracle_rows + stale_hit + unknown + dead)
                    unmeasured = len(stale_hit) + len(unknown) + len(dead)
                    if unmeasured:
                        notes.append('coverage: %d row(s) exercised by no scenario: rule 1 '
                                     'imports them unmeasured' % unmeasured)
                notes.append('coverage: %d scenario-covered row(s), %d oracle-backed'
                             % (len(hit), len(oracle_rows)))
                coverage_note = '\n'.join(notes) or None
                if not eligible and kept_default:
                    return [], [], None, kept_default, (
                        2, 'nothing left: every remaining %s row is kept by default '
                           '(%d recorded: no scenario exercises them)\n'
                           % (category, len(kept_default)))
    if skip_note:
        coverage_note = skip_note if coverage_note is None else (
            coverage_note + '\n' + skip_note)
    return eligible, no_addr, coverage_note, kept_default, None


def _record_kept_default(state, rows):
    """Record rule-2 kept-by-default rows in `state` (the import state the
    batches live in -- no second state file): merge `rows` in, keeping an
    earlier `at` for a row already recorded, and drop entries whose row has
    since entered a batch (a coverage pass exercised it and it was cut)."""
    kept = state.setdefault('kept_default', {})
    in_batch = {_addr_key(r['address'])
                for b in state['batches'].values()
                for r in b['rows'] if r.get('address')}
    for r in rows:
        k = _addr_key(r['address'])
        if k in in_batch:
            continue
        if k not in kept:
            kept[k] = {'address': r['address'], 'name': r.get('name', ''),
                       'file': r.get('file', ''), 'at': _now()}
    for k in [k for k in kept if k in in_batch]:
        del kept[k]


def _next_batch(disagreements_path=DISAGREEMENTS_PATH, state_path=STATE_PATH,
               size=DEFAULT_SIZE, category=DEFAULT_CATEGORY, *,
               triage_path=None, repo=None, fork_base=None,
               worktree_root=None, import_path=None, base='first',
               claims_path=None, queue='wave1', probe_path=None,
               coverage_path=None, coverage_root=None, include_uncovered=False):
    """Rank the unclaimed rows, take the biggest `size`, record the batch.

    With triage_path (the CLI passes docs/wso/triage.json and ROOT) it
    also skips rows already on the import list, cuts the batch's branch
    p3/wso-<id> from `base` and its worktree under worktree_root (default
    default_worktree_root(repo)) unless the branch has one, and then runs
    apply_triage with fork_base (default: the state's).  Nothing is
    recorded when the worktree cannot be cut.  Without triage_path only
    the state file is written.  Files held in claims_path (default:
    claims.json beside the state file) are skipped like those of assigned
    batches.

    queue='import' (ruling R11) takes the rows of the import categories
    (`theirs_real_ours_stub` first; once none of those is undecided,
    `theirs_changed_ours_base`, then `only_theirs`) plus the import list's entries (completed
    from disagreements.json by address), applies no triage, cuts ids
    i<NNN> and still cuts the worktree.  Files held by the assigned
    batches of the other queue's state file, beside this one, are skipped
    like those of this queue's own."""
    if queue not in QUEUES:
        return 2, f'unknown queue {queue}\n'
    q = QUEUES[queue]
    importing = queue == 'import'
    probing = queue == 'probes'
    with open(disagreements_path, encoding='utf-8') as f:
        rows = json.load(f)
    state = load_state(state_path)
    decided = set()
    owned = set()
    cut = triage_path is not None or importing or probing
    if cut:
        repo = repo or ROOT
        if import_path is None:
            import_path = os.path.join(repo, IMPORT_LIST_REL)
        imported, err = _load_list(import_path, 'import list', missing_ok=True)
        if err:
            return 2, err
    if triage_path is not None:
        _, err = _load_list(triage_path, 'triage file')
        if err:
            return 2, err
    if triage_path is not None and not importing:
        # Rows on the import list have left wave 1.
        decided.update(_addr_key(e['address']) for e in imported)
        # So have the rows a worker left undecided for a probe (R73).
        if probe_path is None:
            probe_path = os.path.join(repo, PROBE_LIST_REL)
        probes, err = _load_list(probe_path, 'probe list', missing_ok=True)
        if err:
            return 2, err
        decided.update(_addr_key(e['address']) for e in probes)
    if importing:
        known = {_addr_key(r.get('address')): r for r in rows
                 if r.get('address')}
        for e in imported:
            full = dict(known.get(_addr_key(e['address']), e))
            full.setdefault('their_len', 0)
            full.setdefault('our_len', 0)
            full['category'] = q['category']
            rows.append(full)
    if probing:
        # The probe list's entries, completed from disagreements.json by
        # address (the sizes the pool sorts by); a row a probe batch holds is
        # not cut again.  A closed batch releases the rows it left on the
        # list (`released`); a released row is offered again only when the
        # coverage map now gives it a scenario that neither ran nor hit it
        # when the entry was stamped (`probed`: scenarios, hits), or its pin
        # moved; a `hand` row (neither, or a fork body found different) on a
        # pin move only.
        if probe_path is None:
            probe_path = os.path.join(repo, PROBE_LIST_REL)
        probes, err = _load_list(probe_path, 'probe list', missing_ok=True)
        if err:
            return 2, err
        known = {_addr_key(r.get('address')): r for r in rows if r.get('address')}
        wave_state = {'batches': {}}
        try:
            wave_state = load_state(os.path.join(
                os.path.dirname(os.path.abspath(state_path)),
                os.path.basename(STATE_PATH)))
        except (OSError, ValueError):
            pass
        rows = []
        for e in probes:
            full = dict(known.get(_addr_key(e['address']), {}))
            full.update({k: e[k] for k in ('address', 'file', 'name', 'reading',
                                           'reason', 'at', 'source', 'probed') if k in e})
            full.setdefault('their_len', 0)
            full.setdefault('our_len', 0)
            full['origin'] = e.get('batch')
            origin = (wave_state.get('batches', {}).get(e.get('batch') or '') or {})
            # the pin the row was compared at: its batch's, else the one the
            # early batches were cut at (never the state's current pin)
            full['fork_base'] = (origin.get('fork_base') if isinstance(origin, dict)
                                 else None) or DEFAULT_FORK_BASE
            full['category'] = q['category']
            rows.append(full)
        probe_rows = {_addr_key(r['address']): r for r in rows}
        reoffer_cache = {}

        def reoffer(k):
            if k not in reoffer_cache:
                reoffer_cache[k] = _probe_reoffer(
                    probe_rows.get(k), disagreements_path, coverage_path,
                    coverage_root, repo)
            return reoffer_cache[k]
    for b in state['batches'].values():
        for r in b['rows']:
            if r.get('address'):
                if (probing and b['status'] == 'closed' and r.get('released')
                        and _addr_key(r['address']) in probe_rows
                        and reoffer(_addr_key(r['address']))):
                    continue
                decided.add(_addr_key(r['address']))
        if b['status'] == 'assigned':
            owned.update(b['files'])
    for other in QUEUES.values():
        sibling = os.path.join(os.path.dirname(os.path.abspath(state_path)),
                               os.path.basename(other['state']))
        if os.path.abspath(sibling) == os.path.abspath(state_path):
            continue
        try:
            for b in load_state(sibling)['batches'].values():
                if b.get('status') == 'assigned':
                    owned.update(b.get('files', []))
        except (OSError, ValueError) as e:
            return 2, f'state file {sibling}: {e}\n'
    if claims_path is None:
        claims_path = os.path.join(os.path.dirname(os.path.abspath(state_path)),
                                   'claims.json')
    try:
        with open(claims_path, encoding='utf-8') as f:
            for c in json.load(f).values():
                owned.update(c.get('files', []))
    except FileNotFoundError:
        pass
    except (OSError, ValueError, AttributeError) as e:
        return 2, f'claims file {claims_path}: {e}\n'

    cats = tuple(q.get('categories', ())) if importing else (
        (q['category'],) if probing else (category,))
    last_failure = None
    kept_note = None
    pool_cache = {}

    def pool_for(cat):
        # one pool per category however often this call reaches it: the
        # pre-loop rule-2 recording below shares the loop's work
        if cat not in pool_cache:
            pool_cache[cat] = _import_pool(
                rows, cat, decided, owned, importing, disagreements_path,
                coverage_path, coverage_root, repo, include_uncovered, base=base)
        return pool_cache[cat]

    if importing:
        # Rule 2 (2026-10-26 ruling): where we both have the body ours is
        # the default, so a base-body row no scenario exercises never
        # reaches a worker.  Pool that category here, before the loop, so
        # those rows are recorded kept by default even when this call cuts
        # no batch or a stub category wins the cut; the loop then reuses
        # the pool, and the stub-first category order is unchanged.
        _, _, _, base_kept, _ = pool_for('theirs_changed_ours_base')
        if base_kept:
            _record_kept_default(state, base_kept)
            kept_note = (f'kept by default: {len(base_kept)} base-body row(s) no '
                         f'scenario exercises (rule 2); recorded in the import '
                         f'state, out of later cuts until a coverage pass '
                         f'exercises them')
    for category in cats:
        eligible, no_addr, coverage_note, kept_default, failure = pool_for(category)
        if eligible:
            break
        last_failure = failure      # the last category's message carries the hint that matters
    if not eligible:
        if kept_note:
            # the recording survives a call that cuts nothing: the state
            # file is the record, the note says it happened
            save_state(state_path, state)
            rc, msg = last_failure
            last_failure = (rc, msg.rstrip('\n') + '\n' + kept_note + '\n')
        return last_failure
    chosen = eligible[:size]
    pre = q['prefix']
    nid = 1 + max((int(m.group(1)) for bid in state['batches']
                   for m in [re.fullmatch(pre + r'(\d+)', bid)] if m),
                  default=0)
    batch_id = f'{pre}{nid:03d}'
    files = _files_of(chosen)
    branch = f'p3/wso-{batch_id}'
    if cut:
        wt, how = _cut_worktree(repo, branch, base, worktree_root)
        if wt is None:
            return 1, f'{batch_id} not assigned: {how}\n'
    state['batches'][batch_id] = {
        'rows': chosen,
        'files': files,
        'branch': branch,
        'status': 'assigned',
        'assigned_at': _now(),
        'closed_at': None,
        'verdicts': {},
    }
    pin = fork_base or state.get('fork_base')
    if probing:
        pin = fork_base or chosen[0].get('fork_base') or DEFAULT_FORK_BASE
    if pin:
        # The rows were compared at this pin; a later recompare moves the
        # state's pin, never this batch's.
        state['batches'][batch_id]['fork_base'] = pin
    save_state(state_path, state)
    lines = [f'{batch_id} assigned: {len(chosen)} rows, {len(files)} files, '
             f'branch {branch}',
             f'files: {", ".join(files)}']
    if importing:
        lines.append(f'category: {category}')
        if kept_note:
            lines.append(kept_note)
    for i, r in enumerate(chosen, 1):
        if probing:
            lines.append(f'{i:2}. {r["address"]}  {r["name"]}  {r["file"]}  '
                         f'{r.get("reading", "?")} reading, from {r.get("origin")}')
            continue
        lines.append(f'{i:2}. {r["address"]}  {r["name"]}  {r["file"]}  '
                     f'theirs {r["their_len"]} / ours {r["our_len"]}')
    global _NOADDR_LOGGED
    if no_addr:
        lines.append(f'(skipped {len(no_addr)} row'
                     f'{"s" if len(no_addr) != 1 else ""} without an address)')
        if not _NOADDR_LOGGED:      # the list once per process, the count every cut
            _NOADDR_LOGGED = True
            lines.append(unaddressed_text(no_addr).rstrip('\n'))
    if triage_path is None and not importing and not probing:
        return 0, '\n'.join(lines) + '\n'
    lines.append(f'worktree {wt} ({how})')
    if importing or probing:
        if coverage_note:
            lines.append(coverage_note)
        return 0, '\n'.join(lines) + '\n'
    rc, out = apply_triage(
        state_path, repo, batch_id, triage_path,
        pin or DEFAULT_FORK_BASE, import_path=import_path)
    return rc, '\n'.join(lines) + '\n' + out


def _files_of(rows):
    """The rows' files, first-seen order."""
    files = []
    for r in rows:
        if r['file'] not in files:
            files.append(r['file'])
    return files


def _under_heading(text, heading, line):
    """text with `line` added at the end of the `heading` section (before
    the next # or ## heading outside a code fence), or with the section
    appended when the file has none."""
    lines = text.split('\n')
    fence = False
    start = end = None
    for i, l in enumerate(lines):
        if l.lstrip().startswith('```'):
            fence = not fence
            continue
        if fence:
            continue
        if start is None:
            if l.rstrip() == heading:
                start = i
        elif re.match(r'#{1,2} ', l):
            end = i
            break
    if start is None:
        body = text.rstrip('\n')
        return (body + '\n\n' if body else '') + heading + '\n\n' + line + '\n'
    if end is None:
        end = len(lines)
    section = lines[start + 1:end]
    while section and not section[-1].strip():
        section.pop()
    rest = lines[end:]
    out = '\n'.join(lines[:start + 1] + section + ['', line]
                    + ([''] + rest if rest else []))
    return out if out.endswith('\n') else out + '\n'


def write_triage_evidence(worktree, address, cls, fork_base):
    """Add the auto verdict's line, `disagreed with WSO at <fork_base>:
    <cls>; case <CASE_IDS[cls]> decides`, under '## WSO delta' in the
    row's evidence file in `worktree`, and commit that file alone on the
    worktree's branch.  The front matter (kind included) is untouched.  A
    file that already carries the line, committed, is left as it is.
    Returns None, or why it failed."""
    rel = _evidence_rel(address)
    path = os.path.join(worktree, rel)
    line = (f'{EVIDENCE_PHRASES[0]} at {fork_base}: {cls}; '
            f'case {_triage().CASE_IDS[cls]} decides')
    try:
        with open(path, encoding='utf-8') as f:
            text = f.read()
        if line not in text:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(_under_heading(text, WSO_DELTA_HEAD, line))
    except OSError as e:
        return f'cannot edit {rel}: {e}'

    def git(*args):
        return subprocess.run(['git', '-C', worktree, *args],
                              capture_output=True, text=True)

    r = git('add', '--', rel)
    if r.returncode != 0:
        return f'git add {rel} failed: {r.stderr.strip()}'
    if git('diff', '--cached', '--quiet', '--', rel).returncode == 0:
        return None
    r = git('commit', '-q', '-m', f'triage: {cls} verdict for {address}',
            '--', rel)
    if r.returncode != 0:
        return f'git commit of {rel} failed: {(r.stderr or r.stdout).strip()}'
    return None


def apply_triage(state_path, repo, batch_id, triage_path, fork_base,
                 import_path=None):
    """Triage one assigned batch from triage.json.

    A row's class is triage.json's auto_class when present, else its
    class.  The enabled classes come from triage.calibrate over the same
    entries and the state's closed batches, so the gate re-evaluates as
    batches close.  A row of an enabled class whose evidence file exists
    in the batch worktree gets write_triage_evidence (committed first),
    then an `ours` verdict with case CASE_IDS[class], and keeps its class.
    An ours-stub row leaves the batch (its files are recomputed) and is
    appended to the import list (default <repo>/docs/wso/import-list.json,
    created when missing).  Every other row stays for a worker, classed
    'ambiguous'; the message says why for each row and ends with
    `triage: n auto, n import, n ambiguous`.  Exit 0, 1 when an evidence
    commit failed or the batch is not assigned, 2 on bad input."""
    tri = _triage()
    entries, err = _load_list(triage_path, 'triage file')
    if err:
        return 2, err
    if not all('class' in e for e in entries):
        return 2, f'triage file {triage_path}: a row has no class\n'
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch['status'] != 'assigned':
        return 1, (f'batch {batch_id} is {batch["status"]}; triage is for '
                   f'assigned batches\n')
    if import_path is None:
        import_path = os.path.join(repo, IMPORT_LIST_REL)
    imported, err = _load_list(import_path, 'import list', missing_ok=True)
    if err:
        return 2, err
    fork_base = batch.get('fork_base') or fork_base
    enabled = tri.enabled_classes(tri.calibrate(state, entries))
    by_addr = {_addr_key(e['address']): e for e in entries}
    branch = batch['branch']
    wt = worktree_for(repo, branch)
    lines = [f'triage {batch_id} (calibration enables: '
             f'{", ".join(sorted(enabled)) or "none"}):']
    classes = {}
    moved = []
    failed = False
    for row in batch['rows']:
        addr = row['address']
        if addr in batch['verdicts']:
            classes[addr] = row.get('class', 'ambiguous')
            lines.append(f'  {addr}: already has a verdict')
            continue
        entry = by_addr.get(_addr_key(addr))
        if entry is None:
            classes[addr] = 'ambiguous'
            lines.append(f'  {addr}: no triage entry; left for a worker')
            continue
        cls = entry.get('auto_class', entry['class'])
        rel = _evidence_rel(addr)
        if cls == 'ours-stub':
            moved.append(row)
            lines.append(f'  {addr} ours-stub: moved to the import list')
            continue
        if cls not in tri.AUTO_OURS:
            why = 'left for a worker'
        elif cls not in enabled:
            why = 'class not enabled by calibration; left for a worker'
        elif wt is None:
            why = f'no worktree for {branch}; left for a worker'
        elif not os.path.isfile(os.path.join(wt, rel)):
            why = f'no evidence file {rel}; left for a worker'
        else:
            case = tri.CASE_IDS[cls]
            err = write_triage_evidence(wt, addr, cls, fork_base)
            if err is None:
                rc, out = record_verdict(state_path, repo, batch_id, addr,
                                         'ours', case)
                if rc != 0:
                    err = out.strip()
            if err is None:
                classes[addr] = cls
                lines.append(f'  {addr} {cls}: auto verdict ours, case {case}')
                continue
            failed = True
            why = f'{err}; left for a worker'
        classes[addr] = 'ambiguous'
        lines.append(f'  {addr} {cls}: {why}')
    # The import list first: a crash between the two writes leaves the row
    # in both places (the next run de-duplicates), never in neither.
    if moved:
        have = {_addr_key(e['address']) for e in imported}
        imported += [{'address': r['address'], 'file': r['file'],
                      'name': r['name'], 'source': 'triage'}
                     for r in moved if _addr_key(r['address']) not in have]
        _save_json(import_path, imported)
    # record_verdict wrote the state; re-read it before the class update.
    state = load_state(state_path)
    batch = state['batches'][batch_id]
    gone = {r['address'] for r in moved}
    batch['rows'] = [r for r in batch['rows'] if r['address'] not in gone]
    for r in batch['rows']:
        r['class'] = classes.get(r['address'], r.get('class', 'ambiguous'))
    batch['files'] = _files_of(batch['rows'])
    save_state(state_path, state)
    auto = sum(1 for r in batch['rows'] if r['class'] in tri.AUTO_OURS)
    lines.append(f'triage: {auto} auto, {len(moved)} import, '
                 f'{len(batch["rows"]) - auto} ambiguous')
    return (1 if failed else 0), '\n'.join(lines) + '\n'


# -- brief --------------------------------------------------------------

def render_brief(template_path=TEMPLATE_PATH, state_path=STATE_PATH,
                 batch_id=None, bundle_dir=None, bundle_fn=_assemble_bundle,
                 repo=None, base='first'):
    """Fill brief-template.md for one batch; the coordinator dispatches
    the result verbatim.  With bundle_dir, a context bundle is written
    per row the worker must decide (class ambiguous, or no class and no
    verdict yet) and {{bundle}} lists the paths.  With repo, a row whose
    evidence at `base` records an oracle mismatch carries its finding."""
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    try:
        with open(template_path, encoding='utf-8') as f:
            template = f.read()
    except OSError as e:
        return 2, f'no template {template_path}: {e}\n'
    def row_line(i, r):
        if r.get('reading'):
            # a probe row (Task 28): the worker's earlier reading and reason, quoted
            pin = str(r.get('fork_base') or fork_base)
            return (f'{i}. `{r["address"]}` **{r["name"]}** ({r["file"]}; earlier reading '
                    f'`{r["reading"]}`, left by {r.get("origin") or "?"}; fork pin `{pin}`)\n'
                    f'   - reason: {r.get("reason", "")}')
        # an import row says what ours is: a stub, or the base body
        label = CATEGORY_LABEL.get(r.get('category'))
        tail = f'; {label}' if label else ''
        line = (f'{i}. `{r["address"]}` **{r["name"]}** ({r["file"]}, '
                f'theirs {r["their_len"]} / ours {r["our_len"]} chars{tail})')
        if repo:
            finding = _mismatch_finding(repo, base, r.get('address'))
            if finding:
                # the oracle case found ours wrong; a kept row cannot stand
                line += (f'\n   - oracle mismatch at {base} (ours is wrong there; import '
                         f'the fork body or park this row): {finding}')
        return line
    fork_base = str(batch.get('fork_base') or state.get('fork_base')
                    or DEFAULT_FORK_BASE)
    rows = '\n'.join(row_line(i, r) for i, r in enumerate(batch['rows'], 1))
    files = '\n'.join(f'- `{f}`' for f in batch['files'])
    bundle = '(no bundles)'
    if bundle_dir:
        decided = batch.get('verdicts', {})
        paths = []
        for r in batch['rows']:
            cls = r.get('class')
            if cls != 'ambiguous' and not (
                    cls is None and r['address'] not in decided):
                continue
            os.makedirs(bundle_dir, exist_ok=True)
            path = os.path.join(bundle_dir, f'{r["address"]}.md')
            with open(path, 'w', encoding='utf-8') as f:
                f.write(bundle_fn(int(r['address'], 16)))
            paths.append(f'- `{path}`')
        bundle = '\n'.join(paths) or '(no bundles)'
    brief = (template
             .replace('{{id}}', batch_id)
             .replace('{{branch}}', batch['branch'])
             .replace('{{fork_base}}', fork_base)
             .replace('{{fork_base7}}', fork_base[:7])
             .replace('{{size}}', str(len(batch['rows'])))
             .replace('{{files}}', files)
             .replace('{{rows}}', rows)
             .replace('{{bundle}}', bundle))
    base_rows = any(r.get('category') == 'theirs_changed_ours_base' for r in batch['rows'])
    only_rows = any(r.get('category') == 'only_theirs' for r in batch['rows'])
    brief = (brief.replace('{{only_theirs}}', ONLY_THEIRS_PARAGRAPH if only_rows else '')
             .replace('{{base_body}}', BASE_BODY_PARAGRAPH.replace('{{id}}', batch_id)
                                if base_rows else '')
             .replace('{{kept_report}}', KEPT_REPORT_PARAGRAPH if base_rows else '')
             .replace('{{kept_refusal}}', KEPT_REFUSAL if base_rows else '')
             .replace('{{kept_measured}}', KEPT_MEASURED if base_rows else '')
             .replace('{{kept_rule4}}', KEPT_RULE4_EXCEPTION if base_rows else '')
             .replace('{{kept_close}}', KEPT_CLOSE if base_rows else '')
             .replace('{{refusal_rule}}', REFUSAL_MIXED if base_rows else REFUSAL_STUB))
    return 0, brief


# -- verdict ------------------------------------------------------------

def worktree_for(repo, branch):
    """The checked-out path of `branch`, or None (git worktree list)."""
    r = subprocess.run(['git', '-C', repo, 'worktree', 'list', '--porcelain'],
                       capture_output=True, text=True)
    want = f'refs/heads/{branch}'
    current = found = None
    for line in r.stdout.splitlines():
        if line.startswith('worktree '):
            current = line[len('worktree '):]
        elif line.startswith('branch ') and line[len('branch '):] == want:
            found = current
    return found


def _read_evidence(repo, branch, address):
    """The evidence file's text: the branch's worktree copy when one is
    checked out (it may hold uncommitted work), else the branch tip."""
    rel = _evidence_rel(address)
    wt = worktree_for(repo, branch)
    if wt:
        path = os.path.join(wt, rel)
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as f:
                return f.read(), None
    r = subprocess.run(['git', '-C', repo, 'show', f'{branch}:{rel}'],
                       capture_output=True, text=True)
    if r.returncode == 0:
        return r.stdout, None
    return None, (f'no evidence file {rel} on {branch}: write it '
                  f'({EVIDENCE_DIR}/<ADDRESS>.md), commit it, then re-run '
                  f'the verdict')


def _evidence_kind(repo, branch, address):
    text, err = _read_evidence(repo, branch, address)
    if text is None:
        return None, err
    m = KIND_RE.search(text)
    if not m:
        return None, (f'{_evidence_rel(address)} on {branch}: no kind: in '
                      f'the front matter')
    return m.group(1), ''


def _verdict_entry(repo, batch, address, side, case, losing_kind=None):
    """(verdict dict, None) or (None, error): the losing-kind rule and the
    shape of a recorded verdict, shared by `verdict` and `resolve`."""
    losing = None
    if side == 'ours':
        losing = 'trace'   # his evidence is always a trace of the exe
    elif side == 'fork':
        losing, err = (losing_kind, None) if losing_kind else _evidence_kind(
            repo, batch['branch'], address)
        if losing is None:
            return None, err
    return {'side': side, 'case': case, 'losing_kind': losing}, None


def record_verdict(state_path=STATE_PATH, repo=ROOT, batch_id=None,
                   address=None, side=None, case=None, measured=None, losing_kind=None):
    """Record one row's outcome.  `side` says who won: fork (his body is
    adopted, ours' verification is a trace), ours (his verification wins,
    ours' evidence kind is the losing kind), neither (parser noise).
    `measured` ('probe', 'frida' or 'replay') says a measurement decided or
    confirmed it; absent, the scorecard counts the verdict as 'none'."""
    if side not in SIDES:
        return 2, f'side must be one of {"|".join(SIDES)}\n'
    if measured is not None and measured not in MEASURED_ORDER[:-1]:
        return 2, f'measured must be one of {"|".join(MEASURED_ORDER[:-1])}\n'
    if not case:
        return 2, 'the deciding case id is required (probe-N, replay checkpoint, ...)\n'
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch['status'] != 'assigned':
        return 1, (f'batch {batch_id} is {batch["status"]}; verdicts are '
                   f'for assigned batches\n')
    row = next((r for r in batch['rows']
                if _addr_key(r.get('address')) == _addr_key(address)), None)
    if row is None:
        return 1, f'{address} is not one of batch {batch_id}\'s rows\n'
    entry, err = _verdict_entry(repo, batch, row['address'], side, case, losing_kind)
    if entry is None:
        return 1, err + '\n'
    losing = entry['losing_kind']
    batch['verdicts'][row['address']] = entry
    if measured is not None:
        entry['measured'] = measured
    save_state(state_path, state)
    kind = f', losing kind: {losing}' if losing else ''
    return 0, f'verdict {batch_id} {row["address"]} {side} (case {case}{kind})\n'


# -- close ---------------------------------------------------------------

def _probe_close_problems(batch, batch_id, repo, wt, base, state_path):
    """(problems, tally) of `close --kind probes` (Task 28).  Every row is
    resolved (off the probe list, a fork verdict with a frida-<batch>- case
    in the wave state, its body adopted and unchanged since the Frida phase
    measured it, evidence kind trace written by that phase) or still on the
    list with our body: its function untouched against the merge base and its
    evidence file the merge base's.  The branch changes no other function
    and no other evidence file.  tally: {'resolved', 'kept', 'findings'}."""
    problems = []
    tally = {'resolved': 0, 'kept': 0, 'findings': 0}
    probes, err = _load_list(os.path.join(repo, PROBE_LIST_REL), 'probe list',
                             missing_ok=True)
    if err:
        return [err.strip()], tally
    on_list = {_addr_key(e['address']) for e in probes}
    try:
        wave_state = load_state(os.path.join(os.path.dirname(os.path.abspath(state_path)),
                                             os.path.basename(STATE_PATH)))
    except (OSError, ValueError) as e:
        return [f'wave state: {e}'], tally
    verdicts = {}
    for b in wave_state['batches'].values():
        for a, v in (b.get('verdicts') or {}).items():
            verdicts[_addr_key(a)] = v
    ce = _check_evidence()
    try:
        touched = {_addr_key('0x%X' % a): n for _p, a, n in ce.touched(wt, base)}
    except Exception as e:
        return [f'cannot tell which bodies the batch touched ({e!r})'], tally
    mb = merge_base_sha(wt, base)
    rows = set()
    for row in batch['rows']:
        addr = row['address']
        k = _addr_key(addr)
        rows.add(k)
        rel = _evidence_rel(addr)
        path = os.path.join(wt, rel)
        text = None
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as f:
                text = f.read()
        if row.get('outcome') == 'found':
            tally['findings'] += 1
        if k in on_list and row.get('outcome') == 'fork-equal':
            problems.append(f'{addr}: decided (case {row.get("case")}) but not recorded; retry '
                            f'at the record stage')
            continue
        if k not in on_list:
            v = verdicts.get(k) or {}
            if not (v.get('side') == 'fork'
                    and str(v.get('case', '')).startswith(f'frida-{batch_id}-')):
                problems.append(f'{addr}: off the probe list but no fork verdict with a '
                                f'frida-{batch_id}-<scenario> case is recorded for it; '
                                f'put the entry back on docs/wso/probe-list.json or record '
                                f'it through resolve_probe')
                continue
            tally['resolved'] += 1
            if k not in touched:
                problems.append(f'{addr}: resolved for the fork but its body is unchanged '
                                f'against {mb[:12] if mb else base}')
            if text is None:
                problems.append(f'no evidence file {rel}')
                continue
            if not (KIND_RE.search(text) and KIND_RE.search(text).group(1) == 'trace'
                    and frida_trace(text) and _frida_probe(text).startswith(f'{batch_id}-')):
                problems.append(f'{rel}: a resolved row needs kind: trace from this batch\'s '
                                f'Frida phase (a frida {batch_id}-<scenario> trace_source)')
            if row.get('measured_body') and ce.body_digest(
                    wt, addr, row.get('file')) != row['measured_body']:
                problems.append(f'{addr}: body changed after the Frida phase; edit it back to '
                                f'the measured body')
            continue
        tally['kept'] += 1
        if k in touched:
            problems.append(f'{addr}: still on the probe list but its body is changed against '
                            f"{mb[:12] if mb else base}; edit that function back to the merge "
                            f"base's body")
        base_text = _git_show(wt, mb, rel) if mb else ''
        if text is None:
            if base_text:
                problems.append(f'{addr}: still on the probe list but {rel} is deleted; put the '
                                f"merge base's copy back")
        elif text != base_text:
            problems.append(f'{addr}: still on the probe list but {rel} differs from the merge '
                            f"base's"
                            + ('' if base_text else ' (which has none)') + '; '
                            + (f"put the merge base's copy back (`git show {mb[:12]}:{rel}`)"
                               if base_text else f'delete {rel}'))
    for k, name in sorted(touched.items()):
        if k not in rows:
            problems.append(f'0x{k.upper()} ({name}): changed against {base} but not a row of '
                            f'this batch; a probe batch changes only its resolved rows\' '
                            f'functions')
    if mb:
        names = subprocess.run(['git', '-C', wt, 'diff', '--name-only', mb, 'HEAD'],
                               capture_output=True, text=True).stdout.split()
        for rel in names:
            m = re.fullmatch(re.escape(EVIDENCE_DIR) + r'/([0-9A-Fa-f]+)\.md', rel)
            if m and _addr_key(m.group(1)) not in rows:
                problems.append(f'{rel}: evidence of a function outside this batch changed; put '
                                f"the merge base's copy back")
    return problems, tally


def _tally(batch):
    sides = {s: 0 for s in SIDES}
    kinds = {}
    for v in batch.get('verdicts', {}).values():
        sides[v['side']] += 1
        k = v.get('losing_kind')
        if k:
            kinds[k] = kinds.get(k, 0) + 1
    return sides, kinds


def _tally_line(batch):
    sides, kinds = _tally(batch)
    if not any(sides.values()):
        return ''   # an import wave: no verdicts to tally
    ks = ', '.join(f'{k} {kinds[k]}' for k in DISPLAY_ORDER if k in kinds)
    line = (f' -- fork {sides["fork"]}, ours {sides["ours"]}, '
            f'neither {sides["neither"]}')
    if ks:
        line += f'; losing kinds: {ks}'
    return line


def _sync_base(repo, base):
    """None, or why not: `git pull --no-rebase --ff-only` in the worktree that has
    `base` checked out, so a merge into it starts from what the upstream
    holds (ruling R17).  Nothing to pull when no worktree has `base`
    checked out or the branch has no upstream."""
    wt = worktree_for(repo, base)
    if wt is None:
        return None
    up = subprocess.run(['git', '-C', wt, 'rev-parse', '--abbrev-ref',
                         f'{base}@{{upstream}}'],
                        capture_output=True, text=True)
    if up.returncode != 0:
        return None
    r = subprocess.run(['git', '-C', wt, 'pull', '--no-rebase', '--ff-only', '-q'],
                       capture_output=True, text=True)
    if r.returncode != 0:
        msg = ' '.join(((r.stderr or '') + (r.stdout or '')).split())
        return f'git pull --ff-only failed in {wt}: {msg}'
    return None


def _fast_forward(repo, base, branch):
    """Move `base` to `branch`, only as a fast-forward.  When a worktree
    has `base` checked out it is moved with `git merge --ff-only`, so the
    checkout follows the ref (ruling R17); else update-ref, atomically."""
    def rev(ref):
        r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify', ref],
                           capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else None
    old, new = rev(base), rev(branch)
    if old is None:
        return f'{base} not found in {repo}'
    if new is None:
        return f'{branch} not found in {repo}'
    if old == new:
        return None
    r = subprocess.run(['git', '-C', repo, 'merge-base', '--is-ancestor',
                        base, branch], capture_output=True, text=True)
    if r.returncode != 0:
        return f'not a fast-forward: {base} is not an ancestor of {branch}'
    wt = worktree_for(repo, base)
    if wt is not None:
        r = subprocess.run(['git', '-C', wt, 'merge', '--ff-only', '-q',
                            branch], capture_output=True, text=True)
        if r.returncode != 0:
            msg = ' '.join(((r.stderr or '') + (r.stdout or '')).split())
            return f'merge --ff-only failed in {wt}: {msg}'
        return None
    r = subprocess.run(['git', '-C', repo, 'update-ref',
                        f'refs/heads/{base}', new, old],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return f'update-ref failed: {r.stderr.strip()}'
    return None


def _ruling_line(ledger_path, ruling):
    """The first line of `ledger_path` starting `Ruling <ruling>` (the id
    ends at a non-id character), or None."""
    try:
        with open(ledger_path, encoding='utf-8') as f:
            for line in f:
                m = re.match(r'Ruling (\S+?)[:.,;)]*(\s|$)', line)
                if m and m.group(1) == ruling:
                    return line.rstrip('\n')
    except OSError:
        return None
    return None


def evidence_paragraphs(text):
    """The logical lines of an evidence file: a run of consecutive non-empty
    lines is one Markdown paragraph, joined with single spaces (workers
    hard-wrap prose); a heading and each line of a fenced code block stay
    their own entries; a blank line separates."""
    out, run, fence = [], [], False

    def flush():
        if run:
            out.append(' '.join(run))
            del run[:]
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith('```') or line.startswith('~~~'):
            flush()
            out.append(raw.rstrip())
            fence = not fence
        elif fence:
            out.append(raw.rstrip())
        elif not line or line == '---':
            flush()
            if line:
                out.append(line)
        elif line.startswith('#'):
            flush()
            out.append(line)
        else:
            run.append(line)
    flush()
    return out


def _cites_pin(text, pin):
    """True when an evidence phrase names `pin` (either may be the
    abbreviation of the other, 7 hex digits at least)."""
    for para in evidence_paragraphs(text):
        for p in EVIDENCE_PHRASES:
            for m in re.finditer(re.escape(p) + r'\s+at\s+([0-9a-fA-F]{7,40})',
                                 para):
                sha = m.group(1).lower()
                if pin.lower().startswith(sha) or sha.startswith(pin.lower()):
                    return True
    return False


def gate_kind_of(output):
    """'docs-only' when tools/gates.sh's summary says it took the docs-only
    fast path, 'full' when it says `gates: full gate`, else 'unknown'."""
    lines = output.splitlines()
    if 'gates: docs-only fast path' in lines:
        return 'docs-only'
    return 'full' if 'gates: full gate' in lines else 'unknown'


def close_batch(state_path=STATE_PATH, repo=ROOT, batch_id=None,
                kind='verdicts', base='first', gate=True, allow_dump=None,
                ledger_path=None):
    """Close a batch: every reason checked, one refusal naming them all;
    on success fast-forward `base` to the batch branch.  kind='verdicts'
    (wave 1) requires a verdict and a phrase-bearing evidence file per
    row; kind='probes' (Task 28, _probe_close_problems) takes the rows of a
    probe batch: resolved or back on the list with our body; kind='import' (waves 2-5) requires evidence kind trace or
    better instead, or the row parked (docs/wso/import-parked/<ADDR>.json)
    with its body back at the merge base's and its evidence deleted.  A branch that modified tools/gates.sh or tools/parity
    is refused outright -- it would be certified by its own gate.  The
    gate runs only when everything else is clean: it costs minutes.
    gate=False skips tools/gates.sh (tests only).  A fork or neither
    verdict decided by a dump reading is refused (probe before import)
    unless allow_dump names a `Ruling <id>` line of ledger_path."""
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch['status'] == 'closed':
        return 1, (f'batch {batch_id} is already closed '
                   f'({batch.get("closed_at")})\n')
    branch = batch['branch']
    problems = []
    wt = worktree_for(repo, branch)
    if wt is None:
        r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify', branch],
                           capture_output=True, text=True)
        if r.returncode == 0:
            problems.append(f'{branch} exists but has no worktree '
                            f'(git worktree add <path> {branch})')
        else:
            problems.append(f'branch {branch} does not exist yet')
    probe_tally = None
    if kind == 'verdicts':
        missing = [r['address'] for r in batch['rows']
                   if r['address'] not in batch.get('verdicts', {})]
        problems += [f'no verdict for {a}' for a in missing]
    used = None
    if allow_dump is not None:
        used = (_ruling_line(ledger_path, allow_dump) if ledger_path
                else None)
        if used is None:
            problems.append(
                f'--allow-dump {allow_dump}: no line starting '
                f'"Ruling {allow_dump}" in {ledger_path or "(no --ledger)"}')
    for addr, v in batch.get('verdicts', {}).items():
        if (v.get('side') in ('fork', 'neither')
                and deciding_kind(v.get('case')) == 'dump reading'
                and used is None):
            problems.append(_dump_refusal(
                addr, v.get('case'),
                'probe before import, or --allow-dump <ruling id>'))
    if wt is not None:
        st = subprocess.run(['git', '-C', wt, 'status', '--porcelain', '-uall'],
                            capture_output=True, text=True)
        dirty = [l for l in st.stdout.splitlines() if l.strip()]
        if dirty:
            problems.append(f'uncommitted paths in {wt}:')
            problems += ['  ' + l for l in dirty[:20]]
        d = subprocess.run(
            ['git', '-C', repo, 'diff', '--name-only', f'{base}..{branch}',
             '--', *GATE_PATHS], capture_output=True, text=True)
        changed = [l for l in d.stdout.splitlines() if l.strip()]
        if changed:
            problems.append(f'these paths differ from {base}; a batch must '
                            f'not change the gate that certifies it:')
            problems += ['  ' + p for p in changed]
        parked = {}
        if kind == 'probes':
            more, probe_tally = _probe_close_problems(batch, batch_id, repo, wt, base,
                                                      state_path)
            problems += more
        if kind not in ('verdicts', 'probes'):
            parked, bad = _parked_rows(wt)
            if bad:
                problems.append(bad)
            try:
                touched = {_addr_key('0x%X' % a): n for _p, a, n in
                           _check_evidence().touched(wt, base)}
            except Exception as e:
                touched = {}
                problems.append(f'cannot tell which bodies the batch touched ({e!r})')
            mb = merge_base_sha(wt, base)
        for row in (batch['rows'] if kind != 'probes' else ()):
            rel = _evidence_rel(row['address'])
            path = os.path.join(wt, rel)
            text = None
            if os.path.isfile(path):
                with open(path, encoding='utf-8') as f:
                    text = f.read()
            k = _addr_key(row['address'])
            if k in parked:
                if text is not None:
                    problems.append(f'{row["address"]}: parked ({parked_rel(k)}) '
                                    f'but {rel} still exists; delete it')
                if k in touched:
                    problems.append(f'{row["address"]}: parked ({parked_rel(k)}) '
                                    f'but its body is still changed against {base}; '
                                    f"edit that function back to the merge base's body")
                continue
            if text is None:
                problems.append(f'no evidence file {rel}' + (
                    f' (and {row["address"]} is not parked: no {parked_rel(k)})'
                    if kind != 'verdicts' else ''))
                continue
            if kind == 'verdicts':
                if not any(p in para for para in evidence_paragraphs(text)
                           for p in EVIDENCE_PHRASES):
                    problems.append(
                        f'{rel}: neither {" nor ".join(EVIDENCE_PHRASES)} '
                        f'in the body')
                elif batch.get('fork_base') and not _cites_pin(
                        text, batch['fork_base']):
                    problems.append(
                        f'{rel}: no "disagreed with WSO at <sha>" / '
                        f'"corroborated by WSO at <sha>" line names this '
                        f'batch\'s pin {batch["fork_base"][:7]}')
                if (wt is not None and frida_trace(text)
                        and _frida_probe(text).startswith(f'{batch_id}-')
                        and _check_evidence().body_digest(
                            wt, row['address'], row.get('file')) != row.get('measured_body')):
                    # the driver's trace of this batch measured a body (measured_body);
                    # a different one, or none recorded, is not what it measured
                    problems.append(f'{row["address"]}: body changed after the Frida phase; '
                                    f"edit the body back to the one the driver's 'frida: trace "
                                    f"evidence' commit measured, or put the evidence file back to "
                                    f'its pre-phase content so the verdict stands on the reading')
            else:
                if (wt is not None and row.get('measured_body') and frida_trace(text)
                        and _check_evidence().body_digest(
                            wt, row['address'], row.get('file')) != row['measured_body']):
                    problems.append(f'{row["address"]}: body changed after the Frida phase; '
                                    f'park it or edit it back to the measured body')
                    continue
                # Imported means changed: a body left (or put back) as the
                # stub cannot close on any evidence, and only touched
                # functions have their trace_source checked by the gate.
                present = (row.get('category') == 'only_theirs'
                           and wt is not None
                           and _labelled_at(wt, mb, row['address']))
                if present and row.get('outcome') == 'kept' and k in touched:
                    # Present (Task 40): ours already carries the body, so a
                    # kept present row must keep it unchanged like any other
                    # kept row; a change is an import, not a keep.
                    problems.append(f'{row["address"]}: kept but its body changed against '
                                    f'{mb[:12] if mb else base}; keep the change and move the '
                                    f'row to ## Imported (take it off ## Kept), or edit it '
                                    f'back and park it ({parked_rel(k)})')
                    continue
                if (k not in touched and row.get('outcome') == 'kept'
                        and (row.get('category') == 'theirs_changed_ours_base'
                             or present)):
                    # Kept (Task 19): ours already matches the original, so the
                    # body is untouched; what the row needs instead is the
                    # Frida phase's measurement of it, kind: trace.  A present
                    # row (Task 40) closes the same way: ours already has the
                    # body, so it is kept, never imported.
                    bar = _import_bar(wt, mb, row['address'], row.get('name', ''), rel, text,
                                      batch_id, row.get('measured_body'), row.get('file'))
                    this_batch = frida_trace(text) and _frida_probe(text).startswith(
                        f'{batch_id}-')
                    stands = kept_evidence(wt, mb, row['address'], row.get('name', ''), text)
                    if stands and other_batch_trace(text, batch_id):
                        # another batch's frida trace measured the body that
                        # batch had: it stands only for the body this batch's
                        # phase recorded (measured_body), still the one here
                        want = row.get('measured_body')
                        stands = bool(want) and _check_evidence().body_digest(
                            wt, row['address'], row.get('file')) == want
                    if bar or not (this_batch or stands):
                        problems.append(f'{row["address"]}: kept but not measured; park it or '
                                        f'import it')
                    continue
                if k not in touched:
                    problems.append(f'row {row["address"]} not imported: body unchanged '
                                    f'against {mb[:12] if mb else base}; import it or park '
                                    f'it ({parked_rel(k)})')
                bar = _import_bar(wt, mb, row['address'], row.get('name', ''), rel, text,
                                  batch_id, row.get('measured_body'), row.get('file'))
                if bar:
                    problems.append(bar)
        if kind not in ('verdicts', 'probes') and wt is not None:
            # A stub dependency imported with a row (the brief allows it in
            # the batch's own files) meets the same bar: trace or better, or
            # edited back and parked.
            rows = {_addr_key(r['address']) for r in batch['rows']}
            for k, name in sorted(touched.items()):
                if k in rows:
                    continue
                rel = _evidence_rel(k)
                label = f'0x{k.upper()} ({name}, imported with the batch)'
                if k in parked:
                    problems.append(f'{label}: parked ({parked_rel(k)}) but its body is still '
                                    f"changed against {base}; edit that function back to the "
                                    f"merge base's body")
                    continue
                path = os.path.join(wt, rel)
                if not os.path.isfile(path):
                    problems.append(f'{label}: no evidence file {rel}; the Frida phase measures '
                                    f'it like a row, or edit it back and park it')
                    continue
                with open(path, encoding='utf-8') as f:
                    dep_text = f.read()
                want = (batch.get('measured_deps') or {}).get(k)
                if (want and frida_trace(dep_text)
                        and _check_evidence().body_digest(wt, '0x' + k) != want):
                    problems.append(f'{label}: body changed after the Frida phase; park it or '
                                    f'edit it back to the measured body')
                    continue
                bar = _import_bar(wt, mb, '0x' + k, name, rel, dep_text, batch_id,
                                  (batch.get('measured_deps') or {}).get(k))
                if bar:
                    problems.append(f'{label}: {bar}')
    gate_kind = None
    if gate and not problems and wt is not None:
        g = subprocess.run(['bash', 'tools/gates.sh', '--base', base],
                           cwd=wt, capture_output=True, text=True)
        if g.returncode != 0:
            problems.append(f'tools/gates.sh --base {base} failed in {wt} '
                            f'(exit {g.returncode}):')
            problems += ['  ' + l
                         for l in g.stdout.splitlines()[-10:]]
        else:
            gate_kind = gate_kind_of(g.stdout)
    if problems:
        return 1, (f'batch {batch_id} not closed:\n'
                   + '\n'.join('  - ' + p for p in problems) + '\n')
    err = _sync_base(repo, base) or _fast_forward(repo, base, branch)
    if err:
        return 1, f'batch {batch_id} not closed:\n  - {err}\n'
    batch['status'] = 'closed'
    batch['closed_at'] = _now()
    if gate_kind:
        batch['gate'] = gate_kind
    if kind == 'probes':
        # the rows left on the list are released: `next` offers them again
        # only per the entry's probed stamp (_probe_reoffer)
        left, _err = _load_list(os.path.join(repo, PROBE_LIST_REL), 'probe list',
                                missing_ok=True)
        still = {_addr_key(e['address']) for e in left or ()}
        for r in batch['rows']:
            if _addr_key(r['address']) in still:
                r['released'] = True
    save_state(state_path, state)
    new = subprocess.run(['git', '-C', repo, 'rev-parse', base],
                         capture_output=True, text=True).stdout.strip()
    if probe_tally is not None:
        tail = (f' -- resolved {probe_tally["resolved"]}, kept on list '
                f'{probe_tally["kept"]}, findings {probe_tally["findings"]}')
    else:
        tail = _tally_line(batch)
    return 0, (f'{batch_id} closed: {len(batch["rows"])} rows'
               f'{tail}' + (f' (gate: {gate_kind})' if gate_kind else '') + f'\n{base} fast-forwarded to {new} '
               f'({branch})\n'
               + (f'dump-decided verdicts allowed by: {used}\n'
                  if used and any(
                      v.get('side') in ('fork', 'neither')
                      and deciding_kind(v.get('case')) == 'dump reading'
                      for v in batch['verdicts'].values()) else ''))


# -- recompare ------------------------------------------------------------

def remeasure_select(state_path=STATE_PATH, repo=ROOT, mode='rows', addresses=(),
                     batch=None, base='first'):
    """(picked, skipped) for `wave.py remeasure`: the closed wave-1 rows to
    measure again, and (address, why) for the ones asked for or considered
    and left out.  Never writes anything.

    mode 'rows': the addresses named; 'read-only-measured': closed rows whose
    verdict says measured frida but whose evidence on `base` is kind:
    read-only; 'dump-decided': closed rows whose verdict case is a dump reading (deciding_kind: dump-*, readout-*),
    with no `measured`, evidence read-only (optionally one batch).  Only a
    verdict whose body is on the base is measured (side ours, or fork, whose
    adopted body landed with the close); `neither` is skipped; evidence that
    is oracle, replay or trace stands and is listed.  A picked entry is
    {row, side, case, batch, measured, kind}."""
    state = load_state(state_path)
    want = {_addr_key(a): a for a in addresses}
    closed = {}
    for bid in sorted(state['batches']):
        b = state['batches'][bid]
        if b.get('status') != 'closed' or (batch and bid != batch):
            continue
        verdicts = b.get('verdicts') or {}
        vkeys = {_addr_key(a): v for a, v in verdicts.items()}
        for r in b.get('rows', []):
            v = vkeys.get(_addr_key(r['address']))
            if v is not None:
                closed[_addr_key(r['address'])] = (bid, r, v)
    picked, skipped = [], []
    for k, (bid, r, v) in sorted(closed.items()):
        side, case = v.get('side'), v.get('case') or ''
        measured = v.get('measured')
        if mode == 'rows':
            if k not in want:
                continue
        elif mode == 'read-only-measured':
            if measured != 'frida':
                continue
        elif mode == 'dump-decided':
            if deciding_kind(case) != 'dump reading' or measured not in (None, '', 'none'):
                continue
        else:
            raise ValueError('unknown remeasure mode %r' % mode)
        if side not in ('ours', 'fork'):
            skipped.append((r['address'], 'verdict %s: ours is not claimed right, nothing to confirm' % side))
            continue
        kind, _err = _evidence_kind(repo, base, r['address'])
        if kind != 'read-only':
            skipped.append((r['address'], 'evidence is %s' % (kind or 'missing')
                            + ('; it stands' if kind else '')))
            continue
        picked.append({'row': dict(r), 'side': side, 'case': case, 'batch': bid,
                       'measured': measured, 'kind': kind})
    if mode == 'rows':
        found = set(closed)
        skipped += [(a, 'not a row of a closed wave-1 batch%s' % (
            ' (batch %s)' % batch if batch else '')) for k, a in sorted(want.items())
            if k not in found]
    return picked, skipped


def _resolve_tip(repo, tip):
    """The full sha of `tip` in repo, or None."""
    r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify', '--quiet',
                        tip + '^{commit}'], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 and r.stdout.strip() else None


_labelled_cache = {}      # (repo, sha) -> {address key: file} over all src/*.cpp


def _labelled_at(repo, ref, address):
    """True when repo's `ref` carries a `// 0xADDR` header comment (our label
    convention) for `address` in a src/*.cpp: our tree already has the
    function, so an only_theirs row for it is present, not an import.  The
    scan is cached per (repo, sha); False when the ref does not resolve or
    git cannot read it (no repo, no commits)."""
    if not ref or not address:
        return False
    sha = _resolve_tip(repo, ref) if repo else None
    if sha is None:
        return False
    ce = _check_evidence()
    key = (repo, sha)
    if key not in _labelled_cache:
        r = ce.git(repo, 'ls-tree', '-r', '--name-only', sha, '--', 'src/',
                   check=False)
        if r.returncode != 0:
            return False
        labels = {}
        for rel in r.stdout.splitlines():
            if not rel.endswith('.cpp'):
                continue
            text = ce.git(repo, 'show', f'{sha}:{rel}', check=False)
            if text.returncode != 0:
                continue
            for _first, _last, a, _name in ce.spans(text.stdout.splitlines()):
                labels.setdefault(_addr_key('0x%X' % a), rel)
        _labelled_cache[key] = labels
    return _addr_key(address) in _labelled_cache[key]


def _xrec_rows(repo, base, theirs, ours):
    """tools/xrec.py's rows for base/theirs/ours, run from repo (it reads
    the trees with git show only)."""
    fd, out = tempfile.mkstemp(prefix='recompare-', suffix='.json')
    os.close(fd)
    try:
        r = subprocess.run(
            [sys.executable, '-I', os.path.join(ROOT, 'tools', 'xrec.py'),
             base, theirs, ours, '--json', out],
            cwd=repo, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError('xrec failed: ' + (r.stderr or r.stdout).strip())
        with open(out, encoding='utf-8') as f:
            return json.load(f)
    finally:
        os.unlink(out)


def _write_rows(path, rows):
    """xrec's format: indent 1, keys as given, a trailing newline, atomic."""
    d = os.path.dirname(os.path.abspath(path)) or '.'
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.wave-', suffix='.json')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(rows, f, indent=1)
            f.write('\n')
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


_labels_module = None
_NOADDR_LOGGED = False      # `next` has listed the address-less rows in this process


def _labels():
    """tools/ghidra/labels.py (the `// 0x<ADDRESS>` header-comment parser
    check_evidence and gen_map rest on), loaded by path."""
    global _labels_module
    if _labels_module is None:
        spec = importlib.util.spec_from_file_location(
            'wave_labels', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        'ghidra', 'labels.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _labels_module = module
    return _labels_module


_xrec_module = None


def _xrec():
    """tools/xrec.py (funcs/stubby for reading a body back), loaded by path."""
    global _xrec_module
    if _xrec_module is None:
        spec = importlib.util.spec_from_file_location(
            'wave_xrec', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                      'xrec.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _xrec_module = module
    return _xrec_module


_addresses_module = None


def _addresses():
    """tools/addresses.py (the address-less rows of disagreements.json),
    loaded by path (tools/ is not on sys.path under -I)."""
    global _addresses_module
    if _addresses_module is None:
        spec = importlib.util.spec_from_file_location(
            'wave_addresses', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                           'addresses.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _addresses_module = module
    return _addresses_module


def address_from_header(source_text, name):
    """'0x<ADDR>' of the function `name` (qualified) defined in
    source_text under a `// 0x<ADDR>` header comment, or None when it is
    not there or two definitions carry the name (an overload: ambiguous)."""
    lines = source_text.splitlines()
    lab = _labels()
    found = {a for _, _, a, n in lab.definitions(lines) if n == name}
    # an overload without a header comment makes the one that has one
    # ambiguous: count every definition of the name, commented or not
    if sum(1 for l in lines if lab._definition_name(l) == name) > 1:
        return None
    return '0x%X' % found.pop() if len(found) == 1 else None


def fill_addresses(rows, source_of):
    """Fill `address` in place on each row that has none, from the header
    comment of the row's function in OUR source file (source_of(file) is
    the file's text, '' when absent).  Returns (filled, still): the rows
    that got an address and the rows that still have none."""
    cache, filled, still = {}, [], []
    for r in rows:
        if r.get('address'):
            continue
        f = r.get('file')
        if f not in cache:
            cache[f] = (source_of(f) or '') if f else ''
        a = address_from_header(cache[f], r.get('name') or '') if cache[f] else None
        if a:
            r['address'] = a
            filled.append(r)
        else:
            still.append(r)
    return filled, still


def unaddressed(rows, categories=None):
    """The rows without an address (in `categories` when given)."""
    return [r for r in rows if not r.get('address')
            and (categories is None or r.get('category') in categories)]


def unaddressed_text(rows):
    """The listing recompare and next print for rows that cannot be
    batched; '' when there are none."""
    if not rows:
        return ''
    return ('rows without an address (not batchable): %d\n' % len(rows)
            + ''.join('  %s  %s\n' % (r.get('name'), r.get('file')) for r in rows))


def pending_candidates(rows, known, decided, at):
    """The coverage pending entries for the candidate-category rows with
    an address in the exe's .text that the map (`known` addresses) has no
    opinion on and no verdict decides."""
    cov = _coverage()
    out, seen = [], set()
    for r in rows:
        if r.get('category') not in cov.CATEGORIES or not r.get('address'):
            continue
        try:
            a = cov.norm(r['address'])
        except (TypeError, ValueError):
            continue
        if (a in seen or a in known or _addr_key(a) in decided
                or not cov.TEXT_START <= int(a, 16) < cov.TEXT_END):
            continue
        seen.add(a)
        out.append({'address': a, 'name': r.get('name'), 'file': r.get('file'),
                    'category': r.get('category'), 'at': at})
    return out


def recompare(state_path=STATE_PATH, disagreements_path=DISAGREEMENTS_PATH,
              repo=ROOT, tip=FORK_REMOTE + '/main', fetch=True,
              base=COMMON_BASE, ours='first', category=DEFAULT_CATEGORY,
              triage_path=None, triage_show=None, source_of=None,
              coverage_path=None, pending_path=None):
    """Compare our tree with the fork's tip and queue what is new.

    Fetches the fork remote (fetch=True), resolves tip, runs xrec
    base..tip..ours and appends to the disagreements file the `category`
    rows whose address is in no batch and has no verdict, replacing an
    older row for the same address.  The state's fork_base becomes the tip
    (batches already cut keep theirs).  Prints `recompare: N new rows at
    <tip7> (M already queued, K decided)`: N rows were not in the file,
    M were already queued (a row in the file or in an assigned batch
    without a verdict), K are decided.  Exit 0, 2 when the tip does not
    resolve.  Nothing is written then.  The fork is only read.

    Task 23: when triage.json exists (triage_path, default beside the
    disagreements file) it is regenerated through triage.regen_triage at the
    new fork_base: closed rows keep their classes, an open row of a class
    the calibration now disables becomes ambiguous.  The calibration and the
    demotions are appended to the message; the caller commits the file.

    Task 27: a row without an address (new from xrec or already in the file)
    gets it from the `// 0x<ADDRESS>` header comment of its function in our
    source (source_of(file), default the file at `ours`); the rest are
    listed (`rows without an address (not batchable)`), never dropped.  The
    candidate-category rows the coverage map (coverage_path, default
    coverage.json beside the file) has no opinion on are recorded in
    pending_path (default coverage.pending.json beside it), merged with
    what is there; no map, nothing recorded."""
    if fetch:
        r = subprocess.run(['git', '-C', repo, 'fetch', '--quiet', FORK_REMOTE],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return 2, (f'recompare: git fetch {FORK_REMOTE} failed: '
                       f'{(r.stderr or r.stdout).strip()}\n')
    sha = _resolve_tip(repo, tip)
    if sha is None:
        return 2, f'recompare: {tip} does not resolve in {repo}\n'
    try:
        theirs = _xrec_rows(repo, base, sha, ours)
    except (RuntimeError, OSError, ValueError) as e:
        return 2, f'recompare: {e}\n'
    if source_of is None:
        source_of = lambda f: _git_show(repo, ours, f)
    filled, _ = fill_addresses(theirs, source_of)
    # Task 40: an only_theirs row our src labels at `ours` is not theirs-only:
    # xrec matched per-file, so ours may already carry the body.  Read the
    # fork's body at the tip and say what the pair really is; the import
    # queue never cuts such a row.
    reclassified = []
    for r in theirs:
        if (r.get('category') == 'only_theirs' and r.get('address')
                and _labelled_at(repo, ours, r['address'])):
            body = _func_body(repo, sha, r.get('file'), r['address'])
            if body is None:
                continue
            stub = _xrec().stubby(body)
            r['category'] = ('ours_real_theirs_stub' if stub
                             else 'both_changed_differ')
            reclassified.append((r, stub))
    with _filelock().hold(os.path.join(
            os.path.dirname(os.path.abspath(state_path)), 'claims.json.lock'),
            'wave recompare'):
        state = load_state(state_path)
        try:
            with open(disagreements_path, encoding='utf-8') as f:
                rows = json.load(f)
        except (OSError, ValueError) as e:
            return 2, f'recompare: {disagreements_path}: {e}\n'
        decided, in_batch = set(), set()
        for b in state['batches'].values():
            for r in b['rows']:
                a = _addr_key(r.get('address'))
                in_batch.add(a)
                if b.get('status') == 'closed' or r['address'] in b.get(
                        'verdicts', {}):
                    decided.add(a)
        # Rows that left wave 1 for the import or probe list are decided too.
        for rel in (IMPORT_LIST_REL, PROBE_LIST_REL):
            listed, err = _load_list(os.path.join(repo, rel), rel,
                                     missing_ok=True)
            if err:
                return 2, err
            decided.update(_addr_key(e['address']) for e in listed)
        def cuttable(r):
            return r.get('category') == category and r.get('address')

        # The row `next` would cut for an address: the first in-category
        # one; matched by (address, name) first, else that one.
        have = {}
        for i, r in enumerate(rows):
            if r.get('address'):
                have.setdefault(_addr_key(r['address']), []).append(i)

        def slot(a, name):
            idx = have.get(a, [])
            for i in idx:
                if rows[i].get('name') == name:
                    return i
            for i in idx:
                if cuttable(rows[i]):
                    return i
            return idx[0] if idx else None

        new, queued, done, seen = 0, 0, 0, set()
        for r in theirs:
            a = _addr_key(r.get('address'))
            if r.get('category') != category or not a or a in seen:
                continue
            seen.add(a)
            if a in decided:
                done += 1
                continue
            if a in in_batch:
                queued += 1
                continue
            i = slot(a, r.get('name'))
            if i is None:
                new += 1
                have.setdefault(a, []).append(len(rows))
                rows.append(r)
            else:
                # A row that was not cuttable before (another category)
                # is new to the queue; the replacement is in place.
                if cuttable(rows[i]):
                    queued += 1
                else:
                    new += 1
                rows[i] = r
        # A new row nothing could name stays in the file (next lists it);
        # it is not a "new row" for the count, it cannot be batched.
        have_nameless = {(r.get('name'), r.get('file')) for r in rows
                         if not r.get('address')}
        for r in theirs:
            if (not r.get('address') and r.get('category') in _coverage().CATEGORIES
                    and (r.get('name'), r.get('file')) not in have_nameless):
                have_nameless.add((r.get('name'), r.get('file')))
                rows.append(r)
        # Rows already in the file without an address (an earlier recompare
        # could not name them): the same resolution, in place.
        filled += fill_addresses(rows, source_of)[0]
        listed = unaddressed(rows, _coverage().CATEGORIES)
        docs_dir = os.path.dirname(os.path.abspath(disagreements_path))
        cov_path = coverage_path or os.path.join(docs_dir, 'coverage.json')
        pend_path = pending_path or os.path.join(docs_dir, 'coverage.pending.json')
        added, pending_total = [], None
        cov_map = _coverage().read(cov_path)
        if cov_map is not None:
            cov_mod = _coverage()
            have_pending = cov_mod.read_pending(pend_path)
            queued_p = {cov_mod.norm(e['address']) for e in have_pending}
            added = [e for e in pending_candidates(
                theirs + rows, cov_mod.known_addresses(cov_map), decided,
                _now()) if e['address'] not in queued_p]
            pending_total = len(have_pending) + len(added)
        tri_out, tri_lines = None, []
        tri_path = triage_path or os.path.join(
            os.path.dirname(os.path.abspath(disagreements_path)), 'triage.json')
        if os.path.isfile(tri_path):
            tri = _triage()
            show = triage_show or (lambda ref, path: _git_show(repo, ref, path))
            try:
                with open(tri_path, encoding='utf-8') as f:
                    committed = json.load(f)
                # a row without an address cannot be classified
                rows_a = [r for r in rows if r.get('address')]
                cal = tri.calibrate(state, tri.calibration_triage(
                    state, rows_a, sha, show=show))
                tri_out = tri.regen_triage(
                    state, rows_a, committed, sha, ours, cal, show=show,
                    log=tri_lines.append)
            except (OSError, ValueError, KeyError) as e:
                return 2, f'recompare: triage regeneration failed: {e}\n'
            tri_lines = [tri.calibration_text(cal)] + tri_lines
        # Batches in flight keep the pin their rows were compared at.
        for b in state['batches'].values():
            if b.get('status') != 'closed':
                b.setdefault('fork_base',
                             state.get('fork_base') or DEFAULT_FORK_BASE)
        # xrec's own format, so an unchanged file stays byte-identical.
        _write_rows(disagreements_path, rows)
        if added:
            _write_rows(pend_path, have_pending + added)
        state['fork_base'] = sha
        save_state(state_path, state)
        if tri_out is not None:
            tri._write_atomic(tri_path, tri_out)
    text = (f'recompare: {new} new rows at {sha[:7]} '
            f'({queued} already queued, {done} decided)\n')
    if filled:
        text += (f'address from our header comment: {len(filled)} row(s): '
                 + ', '.join(f'{r["name"]} {r["address"]}' for r in filled) + '\n')
    for r, stub in reclassified:
        text += ('reclassified %s %s: ours already carries the body; theirs is '
                 '%s -> %s\n' % (r['address'], r.get('name', ''),
                                 'a stub' if stub else 'not a stub', r['category']))
    text += unaddressed_text(listed)
    if added:
        text += (f'coverage: {len(added)} new candidate(s) recorded in '
                 f'{os.path.basename(pend_path)} ({pending_total} pending); '
                 f'run `python3 -I tools/wave.py coverage --pending`\n')
    if tri_out is not None:
        text += (f'triage: regenerated {os.path.basename(tri_path)} at '
                 f'{sha[:7]} ({len(tri_out)} rows)\n'
                 + '\n'.join(tri_lines) + '\n')
    return 0, text


def _git_show(repo, ref, path):
    r = subprocess.run(['git', '-C', repo, 'show', f'{ref}:{path}'],
                       capture_output=True, text=True, errors='replace')
    return r.stdout if r.returncode == 0 else ''


def _func_body(repo, ref, path, address):
    """The stripped body xrec.funcs() reads for the function `address` labels
    in repo's `ref` (path first, else every other src/*.cpp), or None when no
    such function is there."""
    ce = _check_evidence()
    r = ce.git(repo, 'ls-tree', '-r', '--name-only', ref, '--', 'src/',
               check=False)
    if r.returncode != 0:
        return None
    paths = [p for p in r.stdout.splitlines() if p.endswith('.cpp')]
    if path:
        paths = [path] + [p for p in paths if p != path]
    want = _addr_key(address)
    for rel in paths:
        for a, body in _xrec().funcs(_git_show(repo, ref, rel)).values():
            if a is not None and _addr_key(a) == want:
                return body
    return None


# -- resolve -------------------------------------------------------------

def _dump_refusal(addr, case, remedy):
    """The refusal close and resolve share for a fork/neither verdict
    decided by a dump reading; `remedy` is the command's own hint."""
    return (f'{addr}: a fork/neither verdict decided by a dump reading '
            f'({case}); {remedy}')


def _evidence_decided_by(text, case):
    """True when a physical line or a logical line (paragraph, see
    evidence_paragraphs) of an evidence file carries one of EVIDENCE_PHRASES and ends with
    `case <case> decides`."""
    tail = f'case {case} decides'
    return any(any(p in line for p in EVIDENCE_PHRASES)
               and line.rstrip().rstrip('.').endswith(tail)
               for line in text.splitlines() + evidence_paragraphs(text))


def resolve_probe(address, side, case, state_path=STATE_PATH, repo=ROOT,
                  measured='none', losing_kind=None, evidence_root=None):
    """Record the verdict of a row on the probe list (ruling R73): the row
    returns to the batch it was moved out of, with the verdict `verdict`
    would record, and leaves the list.  Exit 1 refuses (a dump-decided
    fork/neither verdict, or no evidence line naming the case).

    Evidence is read from two places: the phrase check reads
    <repo>/docs/evidence/<ADDR>.md, while a fork/neither resolve takes the
    losing kind from the batch branch (its worktree or tip), like `verdict`,
    so that branch must still exist and carry the evidence file.

    The probe queue (Task 28) resolves a row before its branch lands: it
    passes `evidence_root` (the batch worktree, where the phrase check reads
    the evidence), `losing_kind` (the evidence kind before the adoption, so
    no branch is read) and `measured` ('frida')."""
    if side not in SIDES:
        return 2, f'side must be one of {"|".join(SIDES)}\n'
    if not case:
        return 2, 'the deciding case id is required\n'
    if measured != 'none' and measured not in MEASURED_ORDER[:-1]:
        return 2, f'measured must be one of none|{"|".join(MEASURED_ORDER[:-1])}\n'
    probe_path = os.path.join(repo, PROBE_LIST_REL)
    probes, err = _load_list(probe_path, 'probe list', missing_ok=True)
    if err:
        return 2, err
    entry = next((e for e in probes
                  if _addr_key(e['address']) == _addr_key(address)), None)
    if entry is None:
        return 2, f'{address} is not on the probe list\n'
    state = load_state(state_path)
    batch_id = entry.get('batch')
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id} (named by the probe list entry)\n'
    addr = entry['address']
    if side in ('fork', 'neither') and deciding_kind(case) == 'dump reading':
        return 1, _dump_refusal(
            addr, case,
            'a probe or replay checkpoint is the only way to record it') + '\n'
    rel = _evidence_rel(addr)
    try:
        with open(os.path.join(evidence_root or repo, rel), encoding='utf-8') as f:
            text = f.read()
    except FileNotFoundError:
        text = None
    if text is None or not _evidence_decided_by(text, case):
        return 1, (f'{rel}: no line with {" or ".join(EVIDENCE_PHRASES)} '
                   f'ending "case {case} decides"\n')
    if not any(_addr_key(r.get('address')) == _addr_key(addr)
               for r in batch['rows']):
        rows, err = _load_list(os.path.join(repo, DISAGREEMENTS_REL),
                               'disagreements')
        if err:
            return 2, err
        row = next((dict(r) for r in rows
                    if _addr_key(r['address']) == _addr_key(addr)), None)
        if row is None:
            return 2, f'{addr} is not in {DISAGREEMENTS_REL}\n'
        # Only rows triage left for a worker reach the probe list, and
        # apply_triage classes every such row ambiguous.
        row['class'] = 'ambiguous'
        batch['rows'].append(row)
        batch['files'] = _files_of(batch['rows'])
    verdict, err = _verdict_entry(repo, batch, addr, side, case, losing_kind)
    if verdict is None:
        return 1, err + '\n'
    verdict['measured'] = measured
    batch['verdicts'][addr] = verdict
    remaining = [e for e in probes if e is not entry]
    save_state(state_path, state)
    _save_json(probe_path, remaining)
    return 0, (f'resolved {addr} in {batch_id}: {side} (case {case}); '
               f'probe list now {len(remaining)} entries\n')


def unpark(addresses, reason=None, state_path=IMPORT_STATE_PATH, repo=ROOT):
    """Return parked import rows to the queue.  Each address needs its
    docs/wso/import-parked/<ADDR>.json and a row in a closed batch of the
    import state; the file goes, the row moves from the batch's `rows` to
    its `unparked` list with `at`, `reason` and `needs`, so `next --queue
    import` offers it again.  Any refusal (exit 1) writes nothing.  No git:
    the caller commits."""
    if not addresses:
        return 2, 'unpark: at least one address is required\n'
    reason = reason or 'needs met'
    parked, problem = _parked_rows(repo)
    if problem:
        return 2, f'{problem}\n'
    # the file each row was read from, under whatever spelling it carries
    # (<ADDR>.json, 0x<ADDR>.json, lower case): that is the file to remove
    folder = os.path.join(repo, IMPORT_PARKED_DIR)
    files = {}
    for name in (sorted(os.listdir(folder)) if os.path.isdir(folder) else []):
        stem = name[:-len('.json')] if name.endswith('.json') else None
        if stem and re.fullmatch(r'(?:0[xX])?[0-9A-Fa-f]+', stem):
            files.setdefault(_addr_key(stem.lower()), os.path.join(folder, name))
    state = load_state(state_path)
    plan, refusals, seen = [], [], set()
    for address in addresses:
        key = _addr_key(str(address).lower())      # the parked keys are lower case
        if key in seen:
            continue
        seen.add(key)
        entry = parked.get(key)
        if entry is None:
            refusals.append(f'{address}: not parked')
            continue
        hit = next(((bid, b, r) for bid, b in sorted(state['batches'].items())
                    for r in b.get('rows', [])
                    if _addr_key(r.get('address')) == key), None)
        if hit is None:
            refusals.append(f'{address}: in no import batch')
        elif hit[1].get('status') != 'closed':
            refusals.append(f'{address}: in assigned batch {hit[0]}; '
                            f'close or clear it first')
        else:
            plan.append((key, entry, *hit))
    if refusals:
        return 1, '\n'.join(refusals) + '\n'
    lines, now = [], _now()
    for key, entry, bid, batch, row in plan:
        batch['rows'] = [r for r in batch['rows'] if r is not row]
        moved = dict(row, at=now, reason=reason, needs=entry.get('needs'))
        batch.setdefault('unparked', []).append(moved)
        lines.append(f'unparked 0x{key.upper()} {row.get("name")} '
                     f'(was {entry.get("needs")}) from {bid}')
    save_state(state_path, state)
    stuck = []
    for key, *_ in plan:
        try:
            os.unlink(files[key])
        except OSError as e:
            stuck.append(f'{os.path.relpath(files[key], repo)} ({e.strerror or e})')
    lines.append(f'commit {os.path.relpath(state_path, repo)} and {IMPORT_PARKED_DIR}')
    if stuck:
        # the row is back in the queue but still reads as parked until the
        # file is gone; nothing else to redo
        lines.append('the state is saved but these parked files remain; delete them '
                     'by hand and commit: ' + '; '.join(stuck))
        return 1, '\n'.join(lines) + '\n'
    return 0, '\n'.join(lines) + '\n'


def requeue(state_path=IMPORT_STATE_PATH, repo=ROOT, dry_run=False, reason=None):
    """Re-queue the parked rule-1 rows whose `needs` ends in `not
    exercised` (2026-10-26 ruling): where we lack the body the import no
    longer needs coverage, so a row parked only for that re-enters the
    queue.  A row parked for any other reason -- a dependency, an exe
    contradiction, a failed check, ours matching the decompile -- and a
    base-body row (rule 2 keeps those ours) stay parked, as does a row in
    a batch that is not closed or one no batch carries.  The move is
    unpark's, with the ruling as the reason; dry_run lists and moves
    nothing."""
    parked, problem = _parked_rows(repo)
    if problem:
        return 2, f'{problem}\n'
    state = load_state(state_path)
    picks = []
    for key, entry in sorted(parked.items()):
        if not (entry.get('needs') or '').rstrip().endswith(REQUEUE_NEEDS_END):
            continue
        hit = next(((bid, b, r) for bid, b in sorted(state['batches'].items())
                    for r in b.get('rows', [])
                    if _addr_key(r.get('address')) == key), None)
        if (hit is None or hit[2].get('category') not in RULE1_CATEGORIES
                or hit[1].get('status') != 'closed'):
            continue
        picks.append((key, entry, hit[0]))
    tail = 'would re-enter the queue' if dry_run else 're-enter the queue'
    lines = [f'requeue: {len(picks)} parked rule-1 row(s) whose needs ends '
             f'in "{REQUEUE_NEEDS_END}" {tail}']
    lines += [f'  0x{key.upper()} {e.get("name", "")} from {bid}'
              for key, e, bid in picks]
    if dry_run:
        return 0, '\n'.join(lines) + '\n'
    if not picks:
        return 0, f'requeue: no parked rule-1 row ends its needs in "{REQUEUE_NEEDS_END}"\n'
    rc, out = unpark([f'0x{key}' for key, _, _ in picks],
                     reason=reason or REQUEUE_REASON, state_path=state_path, repo=repo)
    return (rc, out) if rc else (0, '\n'.join(lines) + '\n' + out)


# -- scorecard -----------------------------------------------------------

SCORECARD_HEAD = '## Scorecard'
# How a verdict was measured (verdict['measured'], default none).
MEASURED_ORDER = ('probe', 'frida', 'replay', 'none')
# Deciding-evidence buckets, in display order -- strongest verification
# first, the way DISPLAY_ORDER orders the losing kinds; triage (an auto
# verdict from the two bodies' text) comes last.  A verdict's free-form
# case id picks exactly one, by deciding_kind(); `losing_kind` is a
# different thing (the loser's kind, not what decided).
DECIDING_ORDER = ('oracle probe', 'replay checkpoint', 'dump reading',
                  'trace', 'triage')
# Bare case ids that name a replay scenario: b005 decided rows with the
# scenario name alone; later replay cases are scenario/checkpoint, which
# the / rule already catches.  Membership is exact, and the classifier
# stays a pure function -- no probing scenarios/ on disk.
LEGACY_REPLAY_CASES = frozenset({'world_entry'})


def deciding_kind(case):
    """The evidence kind that decided a verdict, from its case id:
    triage-* (apply_triage's auto verdicts) triage, probe-N an oracle
    probe, replay-*, any scenario/checkpoint id (a / in it) or a bare
    legacy scenario name a replay checkpoint, dump-N / readout-ADDR a
    dump reading, anything else (frida-* included) a trace."""
    c = case or ''
    if c.startswith('triage-'):
        return 'triage'
    if c.startswith('frida-'):
        return 'trace'   # frida-<batch>-<scenario>: a scenario id has a / in it
    if c.startswith('probe'):
        return 'oracle probe'
    if (c.startswith('replay') or '/' in c
            or c in LEGACY_REPLAY_CASES):
        return 'replay checkpoint'
    if c.startswith('dump-') or c.startswith('readout-'):
        return 'dump reading'
    return 'trace'


BASE_BODY_PARAGRAPH = (
    "For a row where ours is the base body, the same rules apply with one\n"
    "addition: the base body may be right and his change wrong. Read the\n"
    "original. If his body is the one it confirms, import it. If ours already\n"
    "matches the original, do NOT park the row and do NOT touch its body: write\n"
    "(or keep) `docs/evidence/<ADDRESS>.md` (`kind: read-only`, with an\n"
    "`exemption:` that gives your reading, what you checked against the\n"
    "disassembly; no verdict phrase and no frida line; an existing oracle,\n"
    "replay or trace file stays untouched), commit it, and list the\n"
    "row under `## Kept` in your report, one line `kept <0xADDR> <one-line\n"
    "reason>`. The Frida phase then measures our body against the original and,\n"
    "when they are equal, the driver rewrites that file to `kind: trace`. If his\n"
    "body contradicts the original, ours stays and the row goes under `## Kept`\n"
    "with the reason `fork body contradicts the exe: <what differs>`; a row you\n"
    "leave as ours is never parked by you for that. Park the row only when ours\n"
    "is wrong too (`needs` says what), when his body is the one to import but it\n"
    "depends on a function outside the batch (a kept row imports nothing, so\n"
    "what ours calls never parks it), or when you cannot settle it.\n"
    "A stub row is never kept.\n\n")

# The template's `## Kept` pieces, rendered for batches that hold base-body rows
# only (a stub batch has no `## Kept` section to write).
KEPT_REFUSAL = (" For a base-body row that refusal is a `## Kept` line with that reason,\n"
                "   not a park: keep or write its `docs/evidence/<ADDR>.md` (`kind:\n"
                "   read-only`, `exemption:` with what the exe shows); do not delete it\n"
                "   (the delete instruction above is for stub rows only).")
KEPT_MEASURED = (
    " For a base-body row you listed under `## Kept`, when the driver finds OUR\n"
    "   body behaves differently from the original, the finding names the scenario\n"
    "   and the difference: park the row with `needs` of `\"ours differs from the\n"
    "   exe (<scenario>): <difference>; import his body in a later batch\"` (the\n"
    "   body was never changed, so there is nothing to edit back: delete its\n"
    "   evidence file, write the parked file and take the row off `## Kept`). An import in the fix round\n"
    "   cannot be measured (the phase runs once), so it is not offered. A base-body\n"
    "   row you imported that comes back different parks as any import does.")
KEPT_RULE4_EXCEPTION = ", except a base-body row you list under `## Kept`"
KEPT_CLOSE = ", or kept (measured by the driver, or standing on oracle/replay evidence)"
REFUSAL_STUB = (" A row whose body you leave as ours because you refused his body is\n"
                "   parked too, with `needs` saying why.")
REFUSAL_MIXED = (" A row whose body you leave as ours because you refused his body: for\n"
                 "   a STUB row (ours is a stub) refusing still parks it, with `needs`\n"
                 "   saying why; for a BASE-BODY row, refusing his body means ours stays\n"
                 "   and the row goes under `## Kept` with the reason (`fork body\n"
                 "   contradicts the exe: <what differs>`), never parked by you.")

# Rendered only for batches that hold rows his tree alone has (Task 22).
ONLY_THEIRS_PARAGRAPH = (
    "For a row where ours has no body, the function does not exist in our tree:\n"
    "add it. Read his through `git show <fork sha>:<path>` (git plumbing only; his\n"
    "tree is untrusted), put it in the same `src/<file>.cpp` his tree has it in, at\n"
    "the position the address order of the file implies, with the `// 0x<ADDRESS>`\n"
    "header comment our files use. Declare it in the class's header under `src/`\n"
    "when the class exists there (a declaration is the only header edit allowed,\n"
    "and only in `src/`). The commit carries the `Imported-From:` trailer. Write\n"
    "`docs/evidence/<ADDRESS>.md` with `kind: read-only` and an `exemption:` giving\n"
    "your reading (for 0x78A2F0 the file is `docs/evidence/78A2F0.md`); never write\n"
    "a frida line. Parking such a row means REMOVING the function you added (and\n"
    "its declaration), so the file is back to the merge base's text for it; delete\n"
    "the evidence file and write the parked JSON. If the exe contradicts his body,\n"
    "park the row with `needs` of `\"fork body contradicts the exe: <what\n"
    "differs>\"`. An only-theirs row is never kept: it never goes under `## Kept`.\n\n")

KEPT_REPORT_PARAGRAPH = (
    "Base-body rows you keep as ours go under a section headed `## Kept`, one line\n"
    "per row, never also under `## Imported`:\n"
    "\n"
    "    kept <0xADDR> <one-line reason>\n"
    "\n"
    "An only-theirs row whose address our src already labels (ours carries the\n"
    "body) is reported there too, as `kept <0xADDR> ours already has the body:\n"
    "<reason>`; it is never imported, never parked.\n")


def _import_tally(state, import_state=None, parked=None):
    """'imports: stubs N imported / K kept / M parked; base-body N imported /
    K kept / M parked' over the rows of the closed import batches (ids
    i<NNN>) of `state` and `import_state`, split by each row's category
    (none: a stub); a row with a docs/wso/import-parked entry (`parked`,
    address keys) is parked, a row recorded outcome kept (the driver marks it
    for a base-body row untouched against the merge base, and for an
    only-theirs row ours already carries a body for, and close lets it close
    only on standing evidence or this batch's trace) is kept, any other
    closed row imported.  The import state's kept_default rows (rule 2:
    base-body rows never cut because no scenario exercises them) close
    the tally when there are any.  None without any."""
    parked = parked or {}
    seen = {}
    for st in (state, import_state):
        for bid, b in (st or {}).get('batches', {}).items():
            if not re.fullmatch(r'i\d+', bid) or b.get('status') != 'closed':
                continue
            for r in b.get('rows', []):
                seen[(bid, _addr_key(r.get('address')))] = r
    if not seen:
        return None
    n = {'stubs': [0, 0, 0], 'base-body': [0, 0, 0], 'only-theirs': [0, 0, 0]}
    for (bid, key), r in seen.items():
        which = ('base-body' if r.get('category') == 'theirs_changed_ours_base'
                 else 'only-theirs' if r.get('category') == 'only_theirs' else 'stubs')
        n[which][2 if key in parked else 1 if r.get('outcome') == 'kept' else 0] += 1
    text = ('imports: stubs %d imported / %d kept / %d parked; '
            'base-body %d imported / %d kept / %d parked' % (*n['stubs'], *n['base-body']))
    if any(n['only-theirs']):
        # kept appears once a present only-theirs row has closed (ours
        # already carried the body); a history without one reads as before
        kept = '; %d kept' % n['only-theirs'][1] if n['only-theirs'][1] else ''
        text += '; only-theirs %d imported%s / %d parked' % (
            n['only-theirs'][0], kept, n['only-theirs'][2])
    default_kept = (import_state or {}).get('kept_default') or {}
    if default_kept:
        closed = {k for (_, k) in seen}
        nkept = sum(1 for k in default_kept if k not in closed)
        if nkept:
            text += '; %d kept by default' % nkept
    return text


def _probe_tally(probe_state, probes):
    """'probes: resolved N, on list M' over the closed batches (ids p<NNN>) of
    the probe queue's state and the probe list's entries; None when the queue
    has never had a batch.  A row is resolved when its closed batch recorded
    it so (rows[].outcome, set by the driver's Frida phase)."""
    batches = (probe_state or {}).get('batches') or {}
    if not batches:
        return None
    resolved = sum(1 for b in batches.values() if b.get('status') == 'closed'
                   for r in b.get('rows', []) if r.get('outcome') == 'resolved')
    return 'probes: resolved %d, on list %d' % (resolved, len(probes or []))


def scorecard_text(state, import_state=None, parked=None, probes=None):
    """The '## Scorecard' section: per-side tally, split by losing kind
    and by deciding evidence kind, plus one line per batch."""
    sides = {s: 0 for s in SIDES}
    kinds = {s: {} for s in SIDES}
    deciding = {s: {} for s in SIDES}
    every = {}
    measured = {s: {} for s in SIDES}
    total = 0
    batches = []
    for bid, b in sorted(state['batches'].items()):
        for v in b.get('verdicts', {}).values():
            total += 1
            sides[v['side']] += 1
            k = v.get('losing_kind')
            if k:
                kinds[v['side']][k] = kinds[v['side']].get(k, 0) + 1
            d = deciding_kind(v.get('case', ''))
            deciding[v['side']][d] = deciding[v['side']].get(d, 0) + 1
            every[d] = every.get(d, 0) + 1
            m = v.get('measured', 'none')
            measured[v['side']][m] = measured[v['side']].get(m, 0) + 1
        pool = f', pool {b["pool"]}' if b.get('pool') else ''
        batches.append(f'{bid} {b["status"]} ({len(b["rows"])} rows{pool})')

    def split(side):
        ks = kinds[side]
        if not ks:
            return ''
        return (' (losing kind: '
                + ', '.join(f'{k} {ks[k]}' for k in DISPLAY_ORDER if k in ks)
                + ')')

    def by_deciding(side):
        ks = deciding[side]
        if not sides[side] or not ks:
            return ''
        return (' -- deciding evidence: '
                + ', '.join(f'{k} {ks[k]}' for k in DECIDING_ORDER if k in ks))

    def by_measured(side):
        if not sides[side]:
            return ''
        return ('; measured: '
                + ', '.join(f'{k} {measured[side].get(k, 0)}'
                            for k in MEASURED_ORDER))

    lines = [
        SCORECARD_HEAD,
        '',
        'Per-side tally across every batch in docs/wso/wave-state.json, '
        'split by deciding evidence kind (regenerate with '
        '`python3 tools/wave.py scorecard`):',
        '',
        f'Decided for the fork: {sides["fork"]}{split("fork")}'
        f'{by_deciding("fork")}{by_measured("fork")}',
        f'Decided for ours: {sides["ours"]}{split("ours")}'
        f'{by_deciding("ours")}{by_measured("ours")}',
        f'Decided for neither: {sides["neither"]}{split("neither")}'
        f'{by_deciding("neither")}{by_measured("neither")}',
        f'Deciding evidence across all {total} verdicts: '
        + ', '.join(f'{k} {every.get(k, 0)}' for k in DECIDING_ORDER),
    ]
    if batches:
        lines += ['', 'Batches: ' + ', '.join(batches)]
    tally = _import_tally(state, import_state, parked)
    if tally:
        lines += ['', tally]
    if probes:
        lines += ['', probes]
    return '\n'.join(lines) + '\n'


AUDIT_PROBES = ('replay', 'oracle', 'park')
AUDIT_RESULTS = ('match', 'flip', 'parked')


def audit_text(audit):
    """(paragraph, None) for the rule-3 audit sample (docs/wso/audit.json),
    or (None, one-line error)."""
    try:
        rows = audit['rows']
        if not isinstance(rows, list) or not all(
                isinstance(r, dict) for r in rows):
            raise TypeError
        probes = [r['probe'] for r in rows]
        results = [r['result'] for r in rows]
        head = (f'seed {audit["seed"]} over {audit["pool"]} '
                f'dump-decided ours rows at {audit["state"]}')
        drawn = audit['drawn']
    except (KeyError, TypeError):
        return None, ('audit file: needs an object with seed, pool, state, '
                      'drawn and a list of rows\n')
    if (any(p not in AUDIT_PROBES for p in probes)
            or any(r not in AUDIT_RESULTS for r in results)):
        return None, (f'audit file: every row needs probe in '
                      f'{"|".join(AUDIT_PROBES)} and result in '
                      f'{"|".join(AUDIT_RESULTS)}\n')
    return (f'Audit sample (rule 3): {head}; {drawn} drawn: '
            f'{probes.count("replay") + probes.count("oracle")} probed '
            f'(replay {probes.count("replay")}, '
            f'oracle {probes.count("oracle")}), '
            f'{results.count("parked")} parked, '
            f'{results.count("flip")} flips.\n'), None


def coverage_line(folder=None):
    """`Coverage: <covered> of <candidates> candidate functions exercised by
    <k> scenarios (<date>)` from coverage.json beside the state file, or
    `Coverage: not measured`."""
    folder = folder or os.path.dirname(DISAGREEMENTS_PATH)
    cov_mod = _coverage()
    path = os.path.join(folder, 'coverage.json')
    cov, why = cov_mod.check(path, os.path.dirname(os.path.dirname(folder)) or ROOT)
    if cov is None:
        return 'Coverage: not measured' + ('' if why.startswith('no map') else ' (%s)' % why)
    try:
        cands = cov_mod.candidates(os.path.join(folder, 'disagreements.json'))
        hit = sum(1 for a in cands if cov_mod.covered(cov, a))
        stale = [s for s in cov.get('stale', ()) if s in cov['scenarios']]
        return 'Coverage: %d of %d candidate functions exercised by %d scenarios%s (%s)' % (
            hit, len(cands), len(cov['scenarios']) - len(stale),
            ' (%d stale)' % len(stale) if stale else '', str(cov.get('at', ''))[:10])
    except (OSError, ValueError, TypeError, AttributeError, KeyError):
        return 'Coverage: not measured (map unreadable)'


def write_scorecard(state_path=STATE_PATH, report_path=REPORT_PATH,
                    triage_path=TRIAGE_PATH, calibration_path=CALIBRATION_PATH,
                    audit_path=None, repo=None):
    """Replace (or append) the '## Scorecard' section of the report.
    Idempotent: the same state writes the same bytes.  When triage.json
    exists the section ends with a labelled calibration block (the
    separate '## Triage calibration' section stays triage.py's); when
    truth-calibration.json exists it ends with the truth calibration
    paragraph; when docs/wso/audit.json (beside the state file, or
    audit_path) exists it ends with the rule-3 audit sample paragraph."""
    state = load_state(state_path)
    docs = os.path.dirname(os.path.abspath(state_path))
    imp_path = os.path.join(docs, os.path.basename(IMPORT_STATE_PATH))
    imp_state = None
    if os.path.abspath(imp_path) != os.path.abspath(state_path) and os.path.exists(imp_path):
        try:
            imp_state = load_state(imp_path)
        except (OSError, ValueError) as e:
            return 2, f'import state {imp_path}: {e}\n'
    parked = {}
    try:
        parked, _ = _parked_rows(repo or os.path.dirname(os.path.dirname(docs)))
    except OSError:
        pass
    probe_line = None
    probe_path = os.path.join(docs, os.path.basename(PROBE_STATE_PATH))
    if os.path.abspath(probe_path) != os.path.abspath(state_path) and os.path.exists(probe_path):
        try:
            plist, perr = _load_list(
                os.path.join(repo, PROBE_LIST_REL) if repo
                else os.path.join(docs, os.path.basename(PROBE_LIST_REL)),
                'probe list', missing_ok=True)
            probe_line = _probe_tally(load_state(probe_path), [] if perr else plist)
        except (OSError, ValueError) as e:
            return 2, f'probe state {probe_path}: {e}\n'
    section = scorecard_text(state, imp_state, parked, probe_line)
    section += '\n' + coverage_line(os.path.dirname(os.path.abspath(state_path))) + '\n'
    try:
        with open(os.path.join(docs, 'disagreements.json'), encoding='utf-8') as f:
            noaddr = len(unaddressed(json.load(f), _coverage().CATEGORIES))
    except (OSError, ValueError, TypeError, AttributeError):
        noaddr = 0
    if noaddr:
        section += (f'\nWave 1 rows without an address (not batchable): {noaddr}\n')
    if audit_path is None:
        audit_path = os.path.join(
            os.path.dirname(os.path.abspath(state_path)), 'audit.json')
    audit_para = None
    if os.path.exists(audit_path):
        try:
            with open(audit_path, encoding='utf-8') as f:
                audit = json.load(f)
        except (OSError, ValueError) as e:
            return 2, f'audit file {audit_path}: {e}\n'
        audit_para, err = audit_text(audit)
        if err:
            return 2, err
    if os.path.exists(triage_path):
        rows, err = _load_list(triage_path, 'triage')
        if err:
            return 2, err
        tri = _triage()
        section += ('\nCalibration, computed from triage.json classes over '
                    'closed verdicts:\n\n'
                    + tri.calibration_text(tri.calibrate(state, rows)))
        if not section.endswith('\n'):
            section += '\n'
    if calibration_path and os.path.exists(calibration_path):
        try:
            with open(calibration_path, encoding='utf-8') as f:
                cal = json.load(f)
            para = _truth().calibration_text(cal)
        except (OSError, ValueError, KeyError, TypeError) as e:
            return 2, f'cannot read {calibration_path}: {e!r}\n'
        section += '\n' + para
    if audit_para:
        section += '\n' + audit_para
    try:
        with open(report_path, encoding='utf-8') as f:
            text = f.read()
    except FileNotFoundError:
        text = ''
    head = text.find(SCORECARD_HEAD)
    if head != -1:
        nxt = text.find('\n## ', head + 1)
        body = (text[:head] + section if nxt == -1
                else text[:head] + section + '\n' + text[nxt + 1:])
    elif text.strip():
        body = text.rstrip('\n') + '\n\n' + section
    else:
        body = section
    d = os.path.dirname(os.path.abspath(report_path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.wave-', suffix='.md')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            f.write(body)
        os.replace(tmp, report_path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
    n = sum(len(b.get('verdicts', {})) for b in state['batches'].values())
    return 0, f'wrote {report_path}: {SCORECARD_HEAD} over {n} verdicts\n'


# -- status / main -------------------------------------------------------

def unmeasured_imports(repo=ROOT):
    """Every unmeasured WSO import by address (rule 1): (rc, lines), one line
    per evidence file under docs/evidence/ whose `exemption:` starts with the
    UNMEASURED_WSO marking, sorted by address, then a count line.  A later
    verification pass runs this one command to list them all."""
    root = os.path.join(repo, EVIDENCE_DIR)
    if not os.path.isdir(root):
        return 2, [f'unmeasured-imports: no {EVIDENCE_DIR} under {repo}']
    ce = _check_evidence()
    hits = []
    for fn in sorted(os.listdir(root)):
        if not fn.endswith('.md') or not re.fullmatch(r'[0-9A-Fa-f]+', fn[:-3]):
            continue
        try:
            with open(os.path.join(root, fn), encoding='utf-8',
                      errors='replace') as f:
                fields = ce.front_matter(f.read())
        except OSError:
            continue
        ex = fields.get('exemption') or ''
        if isinstance(ex, str) and ex.strip().startswith(UNMEASURED_WSO):
            hits.append((fn[:-3], fields.get('name') or ''))
    lines = [f'0x{stem.upper()} {name}'.rstrip() for stem, name in hits]
    lines.append(f'{len(hits)} unmeasured WSO import(s)')
    return 0, lines


def status(state_path=STATE_PATH):
    state = load_state(state_path)
    if not state['batches']:
        return 0, 'no batches\n'
    lines = []
    for bid, b in sorted(state['batches'].items()):
        v = len(b.get('verdicts', {}))
        n = len(b['rows'])
        extra = f', {v}/{n} verdicts' if b['status'] == 'assigned' else ''
        lines.append(f'{bid} {b["status"]} {b["branch"]} {n} rows{extra}')
    kept = state.get('kept_default') or {}
    if kept:
        # rule 2: base-body rows recorded ours by default because no
        # scenario exercises them; they leave this map only when cut
        lines.append(f'kept by default: {len(kept)} row(s) no scenario exercises')
    return 0, '\n'.join(lines) + '\n'


def undecide(batch_id, address, reading, reason, state_path=STATE_PATH,
             repo=ROOT):
    """Move a row of an assigned batch to the probe list by hand, the move
    the driver makes for a worker's `## Undecided` row (ruling R73); the
    reverse of `resolve`.  Exit 2 refuses and nothing is written."""
    if reading not in ('fork', 'neither'):
        return 2, 'reading must be fork or neither\n'
    if not reason or not reason.strip():
        return 2, 'a reason is required\n'
    state = load_state(state_path)
    batch = state['batches'].get(batch_id)
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch.get('status') != 'assigned':
        return 2, (f'{batch_id} is closed; a closed row is reopened through '
                   f'the probe list by hand\n')
    row = next((r for r in batch['rows']
                if _addr_key(r.get('address')) == _addr_key(address)), None)
    if row is None:
        return 2, f'{address} is not a row of {batch_id}\n'
    probe_path = os.path.join(repo, PROBE_LIST_REL)
    probes, err = _load_list(probe_path, 'probe list', missing_ok=True)
    if err:
        return 2, err
    if any(_addr_key(e['address']) == _addr_key(address) for e in probes):
        return 2, f'{row["address"]} is already on the probe list\n'
    addr = row['address']
    verdicts = batch.setdefault('verdicts', {})
    dropped = verdicts.pop(addr, None)
    if dropped is None:
        dropped = next((verdicts.pop(k) for k in list(verdicts)
                        if _addr_key(k) == _addr_key(addr)), None)
    batch['rows'] = [r for r in batch['rows'] if r is not row]
    batch['files'] = _files_of(batch['rows'])
    probes.append({'address': addr, 'at': _now(), 'batch': batch_id,
                   'file': row.get('file'), 'name': row.get('name'),
                   'reading': reading, 'reason': reason.strip(),
                   'source': 'coordinator'})
    # The list first: a crash between the writes leaves the row in both.
    _save_json(probe_path, probes)
    save_state(state_path, state)
    out = (f'moved {addr} out of {batch_id} to the probe list ({reading}); '
           f'probe list now {len(probes)} entries\n')
    if dropped is not None:
        out += f'dropped its verdict: {json.dumps(dropped, sort_keys=True)}\n'
    out += (f'the {batch["branch"]} branch must now hold: our body for that '
            f'function (the merge base\'s), and an evidence file without a '
            f'deciding sentence (the checks skip rows without a verdict '
            f'line).  `wave.py resolve {addr} <side> <case>` brings it back.\n')
    return 0, out


def _line_buffer():
    """Flush each line when stdout/stderr are not a tty, so a redirected
    long run shows progress as it happens."""
    for stream in (sys.stdout, sys.stderr):
        try:
            if hasattr(stream, 'reconfigure') and not stream.isatty():
                stream.reconfigure(line_buffering=True)
        except (ValueError, OSError):      # a closed or replaced stream
            pass


def main(argv=None):
    _line_buffer()
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__.strip())
        return 0 if argv else 2
    cmd, rest = argv[0], argv[1:]
    if cmd == 'next':
        size, category, queue = DEFAULT_SIZE, DEFAULT_CATEGORY, 'wave1'
        include = False
        it = iter(rest)
        for a in it:
            if a == '--include-uncovered':
                include = True
            elif a == '--queue':
                queue = next(it, '')
                if queue not in QUEUES:
                    print('wave next: --queue is one of '
                          + ', '.join(QUEUES), file=sys.stderr)
                    return 2
            elif a == '--size':
                try:
                    size = int(next(it))
                except (StopIteration, ValueError):
                    print('wave next: --size needs a number', file=sys.stderr)
                    return 2
            elif a == '--category':
                try:
                    category = next(it)
                except StopIteration:
                    print('wave next: --category needs a value',
                          file=sys.stderr)
                    return 2
            else:
                print(f'wave next: unknown option {a}', file=sys.stderr)
                return 2
        if include and queue != 'import':
            print('wave next: --include-uncovered only applies to --queue import',
                  file=sys.stderr)
            return 2
        extra = {} if queue == 'wave1' else {
            'state_path': QUEUES[queue]['state'], 'queue': queue,
            'include_uncovered': include}
        rc, out = next_batch(
            size=size, category=category,
            triage_path=TRIAGE_PATH if QUEUES[queue]['triage'] else None,
            repo=ROOT, **extra)
    elif cmd == 'brief':
        if len(rest) != 1:
            print('usage: wave.py brief <batch-id>', file=sys.stderr)
            return 2
        rc, out = render_brief(batch_id=rest[0], repo=ROOT)
    elif cmd == 'verdict':
        if len(rest) != 4:
            print('usage: wave.py verdict <batch-id> <address> '
                  'fork|ours|neither <case>', file=sys.stderr)
            return 2
        rc, out = record_verdict(batch_id=rest[0], address=rest[1],
                                 side=rest[2], case=rest[3])
    elif cmd == 'close':
        kind, args, allow, ledger = 'verdicts', [], None, None
        it = iter(rest)
        for a in it:
            if a == '--kind':
                kind = next(it, 'import')
            elif a == '--allow-dump':
                allow = next(it, '')
            elif a == '--ledger':
                ledger = next(it, '')
            else:
                args.append(a)
        if len(args) != 1 or (allow is not None) != (ledger is not None):
            print('usage: wave.py close <batch-id> [--kind import|probes] '
                  '[--allow-dump RULING --ledger PATH]', file=sys.stderr)
            return 2
        extra = {'state_path': PROBE_STATE_PATH} if kind == 'probes' else {}
        rc, out = close_batch(batch_id=args[0], kind=kind,
                              allow_dump=allow, ledger_path=ledger, **extra)
    elif cmd == 'resolve':
        if len(rest) != 3:
            print('usage: wave.py resolve <address> fork|ours|neither <case>',
                  file=sys.stderr)
            return 2
        rc, out = resolve_probe(rest[0], rest[1], rest[2])
    elif cmd == 'undecide':
        if len(rest) < 4:
            print('usage: wave.py undecide <batch> <address> fork|neither '
                  '<reason...>', file=sys.stderr)
            return 2
        rc, out = undecide(rest[0], rest[1], rest[2], ' '.join(rest[3:]))
    elif cmd == 'unpark':
        addrs, reason, it = [], None, iter(rest)
        for a in it:
            if a == '--reason':
                reason = next(it, None)
                if reason is None:
                    print('wave unpark: --reason needs a value', file=sys.stderr)
                    return 2
            elif a == '--queue':
                if next(it, '') != 'import':
                    print('wave unpark: --queue is import', file=sys.stderr)
                    return 2
            else:
                addrs.append(a)
        if not addrs:
            print('usage: wave.py unpark <address> [<address> ...] '
                  '[--reason TEXT] [--queue import]', file=sys.stderr)
            return 2
        rc, out = unpark(addrs, reason)
    elif cmd == 'requeue':
        dry, reason, stray = False, None, []
        it = iter(rest)
        for a in it:
            if a == '--dry-run':
                dry = True
            elif a == '--reason':
                reason = next(it, None)
                if reason is None:
                    print('wave requeue: --reason needs a value', file=sys.stderr)
                    return 2
            else:
                stray.append(a)
        if stray:
            print('usage: wave.py requeue [--dry-run] [--reason TEXT]', file=sys.stderr)
            return 2
        rc, out = requeue(dry_run=dry, reason=reason)
    elif cmd == 'recompare':
        kw, it = {}, iter(rest)
        for a in it:
            if a == '--no-fetch':
                kw['fetch'] = False
            elif a in ('--tip', '--state', '--disagreements', '--repo'):
                v = next(it, None)
                if v is None:
                    print(f'wave recompare: {a} needs a value', file=sys.stderr)
                    return 2
                kw[{'--tip': 'tip', '--state': 'state_path',
                    '--disagreements': 'disagreements_path',
                    '--repo': 'repo'}[a]] = v
            else:
                print(f'wave recompare: unknown option {a}', file=sys.stderr)
                return 2
        rc, out = recompare(**kw)
    elif cmd == 'addresses':
        return _addresses().main(rest)
    elif cmd == 'coverage':
        return _coverage().main(rest)
    elif cmd == 'scorecard':
        rc, out = write_scorecard()
    elif cmd == 'status':
        rc, out = status()
    elif cmd == 'unmeasured-imports':
        if rest and (len(rest) != 2 or rest[0] != '--repo'):
            print('usage: wave.py unmeasured-imports [--repo PATH]',
                  file=sys.stderr)
            return 2
        rc, lines = unmeasured_imports(repo=rest[1] if rest else ROOT)
        out = '\n'.join(lines) + '\n'
    elif cmd == 'run':
        return _driver().main(rest)
    elif cmd == 'remeasure':
        return _driver().remeasure_main(rest)
    elif cmd == 'selfcheck':
        return _driver().selfcheck_main(rest)
    else:
        print(f'wave.py: unknown command {cmd}', file=sys.stderr)
        return 2
    sys.stdout.write(out)
    return rc


if __name__ == '__main__':
    sys.exit(main())
