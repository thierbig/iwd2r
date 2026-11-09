"""Require decompiler evidence for every engine function a change touches.

Usage: python3 -I tools/check_evidence.py [--base REV] [--repo DIR] [--worktree]

A touched function is a labelled function (a `// 0xADDR` comment directly
above a definition, the rule of tools/ghidra/labels.py) in a src/**/*.cpp file
whose lines changed in `git diff <base>...HEAD`. A function spans from its
address comment to the first line after its definition that starts with `}`.
Changes outside every span (includes, globals, comments between functions),
changes in headers, and deleted functions or files are ignored; files new in
the branch count in full. Only committed changes are compared.

--worktree audits the working tree instead: the diff runs from the base's
merge point to the working tree, so the staged and unstaged edits and the
untracked src/*.cpp files (counting in full, the way files new in the
branch do) are covered too, and the source spans and the evidence files
are read from the working tree, so an untracked evidence file counts. One
diff also means a function that a committed and a dirty hunk both touch
is reported once. A pre-push run wants --worktree: the committed-only
check on a dirty tree reads `0 touched function(s)` while the edits sit
uncommitted.

Each touched function needs docs/evidence/<ADDR>.md in the revision under test
(at HEAD normally, or the working tree with --worktree; ADDR in upper-case
hex without 0x, docs/evidence/README.md has the format) whose front matter has
a non-empty `oracle_case` or `exemption`, or is `kind: trace` with a
`trace_source` that checks out (below), which needs neither. Prints each offending function and
exits 1, else exits 0. A base that is not a commit or shares no history with
HEAD (a force push, or a `before` missing from the clone) is reported on
stderr and skipped with exit 0.

For the same touched functions the checker also reports how many evidence
files carry each `kind` (`oracle`, `replay`, `read-only` or `trace`), and
warns about the files whose `kind` is missing or not one of those four.
Outside the rules-math set the kind is not enforced: a missing one is only a
warning. A `kind: trace` document — the function evidenced by tracing, the
kind the WSO fork's imports carry — must say where the trace came from in
`trace_source`: a citation of the fork's recovery note
(`WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md`, which must name
the function: its address, or its name as a whole word together with its
class's name), of a frida capture of our own
binary (`frida <probe> orig:<sha256> ours:<sha256> compare=0`), or of a
differential-emulator case (`emu c<addr> orig:<sha256> ours:<sha256>
at:<commit> sets=<n> equal=<n>`: the case module's address lower-case hex
without 0x, both exes' sha256, the commit our exe was built at, and the set
counts, which need `n >= 1` and `sets == equal` — an all-EQUAL run measured
the function, a DIFF or ERROR is a finding, not evidence); anything
else fails like a missing citation.
Functions in RULES_MATH below — the rules-math set, whose correctness is the
D&D 3e rules themselves (attack rolls, AC, saving throws, damage
resolution, effect application; triaged in docs/evidence/rules-math-triage.md)
— must have `kind: oracle`: a replay, a read-only exemption or a trace
fails, and an untagged document counts as not-oracle instead of a warning.
The report also prints the per-kind split of the touched rules-math
functions.
"""

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                'ghidra'))
import labels  # noqa: E402

EVIDENCE_DIR = 'docs/evidence'
HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@')
KINDS = ('oracle', 'replay', 'read-only', 'trace')
TRACE_SOURCES = (
    # The fork's recovery note: repo@sha, then the note's path in it. Only
    # docs/<name>-recovery.md is a note; his source files are not traces.
    re.compile(r'WillScarlettOhara/iwd2-re@([0-9A-Fa-f]{7,40})\s+'
               r'(docs/[A-Za-z0-9_.+-]+-recovery\.md)$'),
    # A frida capture of our own binary against the original, the line the
    # compare harness prints (plan @8ce297d7, Task 5b).
    re.compile(r'frida\s+\S+\s+orig:[0-9A-Fa-f]{64}\s+'
               r'ours:[0-9A-Fa-f]{64}\s+compare=0$'),
    # A differential-emulator case (Task 74): the case module's address in
    # lower-case hex without 0x (tools/emu/cases/c<addr>.py), both exes'
    # sha256, the commit our exe was built at, and the set counts -- which
    # must all be equal, checked after the match by emu_counts_ok below
    # (a regex cannot compare two numbers).
    re.compile(r'emu\s+c(?P<addr>[0-9a-f]+)\s+'
               r'orig:(?P<orig>[0-9A-Fa-f]{64})\s+'
               r'ours:(?P<ours>[0-9A-Fa-f]{64})\s+'
               r'at:(?P<at>[0-9A-Fa-f]{7,40})\s+'
               r'sets=(?P<sets>\d+)\s+equal=(?P<equal>\d+)$'),
)


def emu_counts_ok(match):
    """Whether an emu trace_source's counts make it a measurement: at least
    one input set, and every set equal (a DIFF or ERROR is a finding, not
    evidence)."""
    return int(match.group('sets')) >= 1 and \
        int(match.group('sets')) == int(match.group('equal'))

# The rules-math set: functions whose correctness IS a D&D 3e rule — the
# to-hit and AC math, saving throws, damage resolution, and the
# effect-application paths that roll saves and apply damage. Keyed by
# function address (the `// 0xADDR` comment, the same address the evidence
# file name uses), so the set survives renames. Hand-maintained and
# conservative: an address enters only when the function computes or applies
# a rule; callers, orchestration and display stay out. Each entry says what
# the function computes and lists the ones considered and left out with the
# reasons. A diff touching one of these addresses requires evidence with
# `kind: oracle` — an exemption, a replay or a trace does not pass.
RULES_MATH = frozenset({
    # Attack rolls, to-hit modifiers and AC.
    0x723CC0,  # CGameSprite::GetAC: base 10, armor-capped DEX, blind penalty,
               # monk WIS bonus
    0x723DB0,  # touch AC: GetAC minus the armor and shield components
    0x723EA0,  # DEX-only AC bonus (touch attacks)
    0x723F60,  # monk WIS AC bonus (unarmored, no shield)
    0x73B760,  # CGameSprite::Hit: the d20 to-hit, nat 20/1, flat-footed,
               # iteratives, crit confirm, concealment
    0x73C490,  # concealment / miss-chance roll against an attack
    0x73CD00,  # bane weapon attack bonus
    0x73CF10,  # weapon proficiency attack bonus/penalty
    0x73D080,  # two-weapon fighting attack penalties
    0x73D1B0,  # situational attack modifiers (flanking, prone, charging...)
    0x547020,  # CRuleTables::GetIterativeAttackBonus (src/CRuleTables.cpp):
               # max(0, bonus - step*attack)
    0x4E9840,  # CItem critical threat range (src/CItem.cpp): base 20, item
               # types, Keen
    # Saving throws.
    0x4A42F0,  # CGameEffect::CheckSave (src/CGameEffect.cpp): the save d20
               # vs DC 10 + level + saveMod, racial/class save bonuses
    0x4A9E30,  # CGameEffectDamage::CheckSave (src/CGameEffect.cpp): save
               # (for half) against a damage effect
    0x547040,  # CRuleTables::GetSaveAdjustmentForSourceSprite
               # (src/CRuleTables.cpp)
    0x547620,  # CRuleTables::sub_547620 (src/CRuleTables.cpp): SPIN108
               # animal-empathy save adjustment
    0x4A3310,  # CGameEffect::CheckAdd (src/CGameEffect.cpp): the
               # pre-application gate — immunity, probability, spell
               # resistance penetration
    # Damage resolution.
    0x73D560,  # CGameSprite::Damage: weapon dice, STR (×1.5 two-handed,
               # half off-hand), sneak attack and uncanny dodge, crit
               # multiplier, damage-type mapping
    0x4A7900,  # CGameEffectDamage::ApplyEffect (src/CGameEffect.cpp): dice
               # roll, resistance subtraction, damage reduction, HP floor
    0x5845D0,  # IcewindMisc::Roll (src/IcewindMisc.cpp): sum of
               # rand()%sides+1 — the dice primitive
    0x447C20,  # CDamageReduction::Apply (src/CDerivedStats.cpp)
    0x447E30,  # CDamageReduction::GetEffectiveAmount
               # (src/CDerivedStats.cpp)
    0x448130,  # CDamageReductionList::GetBestGeneric
               # (src/CDerivedStats.cpp)
    0x4481A0,  # CDamageReductionList::GetBest (src/CDerivedStats.cpp)
    0x448250,  # CDamageReductionList::Apply (src/CDerivedStats.cpp): the
               # DR X/enchantment rule
    # Effect application paths that carry rules math.
    0x72DE60,  # CGameSprite::ProcessEffectList (src/CGameSprite.cpp): the
               # per-tick effect pass; rolls the four save d20s and the d100
    0x4A3030,  # CGameEffect::ResolveEffect (src/CGameEffect.cpp): effect
               # duration units (rounds ×15 ticks) and expiry
    0x733200,  # CGameSprite::HandlePersistantEffects
               # (src/CGameSprite.cpp): persistent-effect timing
})

RULES_MATH_NOTE = ' (rules math: requires kind: oracle)'


def git(repo, *args, check=True):
    return subprocess.run(['git', '-c', 'core.quotePath=false', *args],
                          cwd=repo, check=check, capture_output=True,
                          text=True)


def diffable(repo, base):
    """Whether base is a commit sharing history with HEAD."""
    r = git(repo, 'rev-parse', '--verify', '--quiet', f'{base}^{{commit}}',
            check=False)
    if r.returncode != 0:
        return False
    return git(repo, 'merge-base', base, 'HEAD', check=False).returncode == 0


def front_matter(text):
    """The key/value pairs of a leading `---` block ({} if there is none).
    Values lose surrounding whitespace and one pair of matching quotes."""
    lines = text.splitlines()
    if not lines or lines[0].strip() != '---':
        return {}
    result = {}
    for line in lines[1:]:
        if line.strip() == '---':
            return result
        key, sep, value = line.partition(':')
        if not sep or not key.strip():
            continue
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in '"\'':
            value = value[1:-1]
        result[key.strip()] = value
    return {}


def evidence_name(address):
    return f'{address:X}.md'


def _evidence_key(address):
    """0x78A2F0 / '0X78a2f0' / 0x78A2F0 -> '78a2f0': one spelling."""
    if isinstance(address, int):
        return f'{address:x}'
    return address.strip().lower().removeprefix('0x')


def misspelt_in(names, address):
    """The entries of `names` (file names, not paths) that spell `address`'s
    evidence file other than `<ADDR>.md` (caps, no 0x): `0x78A2F0.md`,
    `0X78A2F0.md`, `78a2f0.md`, `0x78a2f0.MD`, any case mix."""
    key = _evidence_key(address)
    canonical = f'{key.upper()}.md'
    out = []
    for name in sorted(names):
        stem, dot, ext = name.rpartition('.')
        if (dot and ext.lower() == 'md' and name != canonical
                and stem.lower().removeprefix('0x') == key):
            out.append(name)
    return out


def misspelt_evidence(repo, address):
    """Repo-relative paths of the misspelt evidence files for `address` in
    the working tree (the canonical one is not among them)."""
    folder = os.path.join(repo, EVIDENCE_DIR)
    names = os.listdir(folder) if os.path.isdir(folder) else []
    return [f'{EVIDENCE_DIR}/{n}' for n in misspelt_in(names, address)
            if os.path.isfile(os.path.join(folder, n))]


def canonical_evidence(repo, address):
    """(canonical path, misspelt sibling or None): `docs/evidence/<ADDR>.md`
    (caps, no 0x) for `address` (an int or any spelling of it), and, when
    that file is not in the working tree `repo` but a misspelt sibling is,
    the sibling's repo-relative path (the one to rename; the first when
    several).  Never renames: the driver does (Driver._check), the gate only
    reports."""
    rel = f'{EVIDENCE_DIR}/{_evidence_key(address).upper()}.md'
    if os.path.isfile(os.path.join(repo, rel)):
        return rel, None
    siblings = misspelt_evidence(repo, address)
    return rel, (siblings[0] if siblings else None)


def spans(lines):
    """(first, last, address, name) line indexes of each labelled function."""
    defs = labels.definitions(lines)
    result = []
    for k, (start, def_line, address, name) in enumerate(defs):
        limit = defs[k + 1][0] if k + 1 < len(defs) else len(lines)
        end = limit - 1
        for j in range(def_line + 1, limit):
            if lines[j].startswith('}'):
                end = j
                break
        result.append((start, end, address, name))
    return result


def changed_lines(diff):
    """{path: [(first, last, deletion)]} of 0-based new-file line ranges.
    A pure deletion after new line c becomes (c-1, c, True): the two lines
    around the gap, which touch a function only when both lie inside it (so
    deleting a whole function touches nothing)."""
    result = {}
    path = None
    for line in diff.splitlines():
        if line.startswith('+++ '):
            target = line[4:].rstrip('\t')
            path = target[2:] if target.startswith('b/') else None
            if path is not None:
                result.setdefault(path, [])
            continue
        m = HUNK.match(line)
        if not m or path is None:
            continue
        start, count = int(m.group(1)), int(m.group(2) or '1')
        if count:
            result[path].append((start - 1, start + count - 2, False))
        elif start > 0:
            result[path].append((start - 1, start, True))
    return result


def _source_lines(repo, path, worktree=False):
    """The lines of a source file: HEAD's copy, or the working tree's
    (None when the working tree no longer has it)."""
    if not worktree:
        return git(repo, 'show', f'HEAD:{path}').stdout.splitlines()
    try:
        with open(os.path.join(repo, path), encoding='utf-8',
                  errors='replace') as f:
            return f.read().splitlines()
    except OSError:
        return None


def touched(repo, base, worktree=False):
    """(path, address, name) of each labelled function changed since base.

    The default compares only the committed changes (`git diff
    base...HEAD`) and reads the spans at HEAD. worktree=True audits the
    working tree: one diff from the base's merge point to the working
    tree covers the committed changes, the staged and unstaged edits and
    the untracked src/*.cpp files, and the spans are read from the
    working tree -- a dirty tree cannot hide a touched function (the
    failure this closes: workers ran the committed-only check against
    dirty trees and read `0 touched function(s)`). One diff, so a
    function that a committed and a dirty hunk both touch is reported
    once."""
    # --minimal: git's default diff gives up on optimality in large files
    # with big insertions and reports untouched neighbouring functions.
    untracked = frozenset()
    if worktree:
        # `git diff <merge-base>`: the working tree against the same point
        # base...HEAD diffs from, tracked files only (staged and unstaged
        # alike); the untracked src files are added below.
        point = git(repo, 'merge-base', base, 'HEAD').stdout.strip()
        diff = git(repo, 'diff', '--unified=0', '--no-color', '--no-ext-diff',
                   '--minimal', '-M', point, '--', 'src/').stdout
        untracked = frozenset(
            p for p in git(repo, 'ls-files', '--others',
                           '--exclude-standard', '--', 'src/').stdout.splitlines()
            if p.endswith('.cpp'))
    else:
        diff = git(repo, 'diff', '--unified=0', '--no-color', '--no-ext-diff',
                   '--minimal', '-M', f'{base}...HEAD', '--', 'src/').stdout
    ranges = changed_lines(diff)
    for path in untracked:
        ranges.setdefault(path, [])
    result = []
    for path, changed in sorted(ranges.items()):
        if not path.endswith('.cpp') or (not changed and path not in untracked):
            continue
        lines = _source_lines(repo, path, worktree)
        if lines is None:
            continue
        for first, last, address, name in spans(lines):
            if path in untracked or any(
                    first <= lo and hi <= last if deletion
                    else lo <= last and first <= hi
                    for lo, hi, deletion in changed):
                result.append((path, address, name))
    return result


def body_digest(repo, address, path=None):
    """sha256 of the committed (HEAD) source text of the function labelled
    `// 0xADDRESS`, looked for in `path` or else every src/*.cpp; None when
    it is not there.  (A digest of the source slice: the Frida phase records
    it, the check and the close compare it.)"""
    import hashlib
    want = int(address, 16) if isinstance(address, str) else address
    paths = [p for p in git(repo, 'ls-tree', '-r', '--name-only', 'HEAD', '--', 'src/',
                            check=False).stdout.splitlines() if p.endswith('.cpp')]
    if path:
        paths = [path] + [p for p in paths if p != path]
    for rel in paths:
        r = git(repo, 'show', f'HEAD:{rel}', check=False)
        if r.returncode != 0:
            continue
        lines = r.stdout.splitlines()
        for first, last, addr, _name in spans(lines):
            if addr == want:
                text = '\n'.join(lines[first:last + 1])
                return hashlib.sha256(text.encode('utf-8')).hexdigest()
    return None


def fork_note_mentions(sha, path, address, name, run=subprocess.run):
    """(ok, reason): whether the fork's note `path` at `sha` (read with
    `git show sha:path`, never checked out or run) names the function: its
    address in hex (any case, with or without 0x), or its unqualified name
    as a whole word (case-sensitive) and, for a member, its class's name
    too: a short name alone ("Render", "Fetch") names a method of every
    class, and a note whose citation keeps a row unmeasured must name this
    one.  An unreadable note, an unfetched sha included, is not ok."""
    r = run(['git', 'show', f'{sha}:{path}'], capture_output=True)
    if r.returncode != 0:
        return False, (f'cannot read the fork note {path} at {sha} (git show '
                       'failed; is the fork fetched, remote wso? an unread '
                       'note is a failure, not a pass)')
    # Notes are not guaranteed UTF-8 (a stray 0x97 is in the real ones);
    # addresses and names are ASCII, so replacement characters cost nothing.
    text = (r.stdout or b'').decode('utf-8', errors='replace')
    if re.search(r'(?<![0-9A-Fa-f])(?:0[xX])?0*%X(?![0-9A-Fa-f])'
                 % address, text, re.IGNORECASE):
        return True, ''
    parts = (name or '').split('::')
    word = lambda w: bool(w) and re.search(
        r'(?<![A-Za-z0-9_])%s(?![A-Za-z0-9_])' % re.escape(w), text)
    if word(parts[-1]) and (len(parts) == 1 or word(parts[-2])):
        return True, ''
    return False, (f'the fork note {path} at {sha} names neither the address '
                   f'0x{address:X} nor the name {name}'
                   + (f' (its class {parts[-2]} with {parts[-1]})' if len(parts) > 1 else '')
                   + '; cite the note that covers this function')


def _evidence_names(repo, worktree=False):
    """The evidence directory's file names: at HEAD, or in the working
    tree."""
    if worktree:
        folder = os.path.join(repo, EVIDENCE_DIR)
        return os.listdir(folder) if os.path.isdir(folder) else []
    if git(repo, 'rev-parse', '--verify', '--quiet', f'HEAD:{EVIDENCE_DIR}',
           check=False).returncode != 0:
        return []
    return git(repo, 'ls-tree', '--name-only', f'HEAD:{EVIDENCE_DIR}',
               check=False).stdout.splitlines()


def problem(repo, address, name='', run=None, worktree=False):
    """(why, fields): why the evidence for address is insufficient, else
    None with the file's front matter.  The evidence is read at HEAD, or
    with worktree=True from the working tree, so an untracked evidence
    file counts (and a dirty deletion of one does not pass)."""
    rel = f'{EVIDENCE_DIR}/{evidence_name(address)}'
    if worktree:
        try:
            with open(os.path.join(repo, rel), encoding='utf-8',
                      errors='replace') as f:
                text = f.read()
        except OSError:
            text = None
    else:
        r = git(repo, 'show', f'HEAD:{rel}', check=False)
        text = r.stdout if r.returncode == 0 else None
    if text is None:
        odd = misspelt_in(_evidence_names(repo, worktree), address)
        if odd:
            return (f'no {rel}, but {EVIDENCE_DIR}/{odd[0]} is spelt wrong: '
                    f'name it {rel} (caps, no 0x)'), None
        return f'no {rel}; write {rel} (caps, no 0x)', None
    fields = front_matter(text)
    recorded = fields.get('address', '')
    try:
        if recorded and int(recorded, 16) != address:
            return f'{rel} records address {recorded}', fields
    except ValueError:
        return f'{rel} has a malformed address {recorded!r}', fields
    if fields.get('kind') == 'trace':
        # A trace whose source checks out is its own evidence: it needs
        # neither oracle_case nor exemption.
        source = fields.get('trace_source') or ''
        matches = [p.fullmatch(source) for p in TRACE_SOURCES]
        if not any(matches):
            return (f'{rel} has kind: trace without a trace_source citing a '
                    "fork recovery note (docs/<name>-recovery.md), a frida "
                    'capture or an emulator case'), fields
        if matches[0]:
            sha, note = matches[0].groups()
            ok, why = fork_note_mentions(
                sha, note, address, name,
                run=run or (lambda argv, **kw: subprocess.run(
                    argv, cwd=repo, **kw)))
            if not ok:
                return f'{rel}: {why}', fields
        if matches[2] and not emu_counts_ok(matches[2]):
            return (f'{rel}: trace_source emu counts must all be equal: '
                    f'sets={matches[2].group("sets")} '
                    f'equal={matches[2].group("equal")} '
                    '(a DIFF or ERROR is a finding, not a measurement)',
                    fields)
        return None, fields
    if not fields.get('oracle_case') and not fields.get('exemption'):
        return f'{rel} has neither oracle_case nor exemption', fields
    return None, fields


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--base', default='origin/main')
    ap.add_argument('--repo', default='.')
    ap.add_argument('--worktree', action='store_true',
                    help='audit the working tree (staged, unstaged and '
                         'untracked files), not just committed changes')
    args = ap.parse_args(argv)

    if not diffable(args.repo, args.base):
        print(f'check_evidence: cannot diff against {args.base}; skipping',
              file=sys.stderr)
        return 0
    if args.worktree:
        print('check_evidence: --worktree: auditing the working tree '
              '(staged, unstaged and untracked files)')
    errors = []
    kinds = {}
    untagged = []
    rules_kinds = {}
    rules_found = 0
    found = touched(args.repo, args.base, worktree=args.worktree)
    for path, address, name in found:
        rules = address in RULES_MATH
        why, fields = problem(args.repo, address, name,
                              worktree=args.worktree)
        if rules:
            rules_found += 1
        if why:
            if rules:
                why += RULES_MATH_NOTE
            errors.append(f'{path}: 0x{address:X} {name}: {why}')
            continue
        kind = fields.get('kind')
        if kind in KINDS:
            kinds[kind] = kinds.get(kind, 0) + 1
            if rules:
                rules_kinds[kind] = rules_kinds.get(kind, 0) + 1
        elif not rules:
            # Inside the set an untagged document is the rules-math error
            # below; outside it stays a warning.
            rel = f'{EVIDENCE_DIR}/{evidence_name(address)}'
            if kind:
                untagged.append(f'warning: {rel} has kind: {kind!r}; expected '
                                f'one of {", ".join(KINDS)}')
            else:
                untagged.append(f'warning: {rel} has no kind: '
                                f'({", ".join(KINDS)})')
        if rules and kind != 'oracle':
            current = f'kind: {kind}' if kind else 'no kind'
            errors.append(f'{path}: 0x{address:X} {name}: rules math: '
                          f'evidence has {current}; requires kind: oracle '
                          f'(an exemption, a replay or a trace does not pass)')
    for line in errors:
        print(line)
    if errors:
        print(f'{len(errors)} touched function(s) lack evidence; '
              'see docs/evidence/README.md')
    else:
        print(f'{len(found)} touched function(s), all with evidence')
    counted = sum(kinds.values())
    if counted:
        print('evidence kinds: '
              + ', '.join(f'{k} {kinds.get(k, 0)}' for k in KINDS)
              + f' ({counted} of {len(found)} touched function(s))')
    if rules_found:
        print('rules math: '
              + ', '.join(f'{k} {rules_kinds.get(k, 0)}' for k in KINDS)
              + f' ({rules_found} of {len(found)} touched function(s); '
              'set and triage in docs/evidence/rules-math-triage.md)')
    for line in untagged:
        print(line)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
