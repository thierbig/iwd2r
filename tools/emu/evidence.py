"""Turn all-EQUAL differential-emulator runs into evidence files (Task 74).

`oracle.py emu --build --write-evidence` runs every case (tools/emu/run.py)
against a build of THIS worktree and, for each case whose every input set
came out EQUAL, rewrites docs/evidence/<ADDR>.md so the run counts as a
measurement:

- an `exemption:` line that opens with UNMEASURED is dropped, `kind:` becomes
  `trace` and a `trace_source:` line (the third form check_evidence.py
  accepts) is added -- every other field and the body stay as they are;
- a file whose exemption says something else (a kept row's reason) keeps its
  front matter untouched and only gets its `## Emulation` section.

A case with any DIFF or ERROR is a finding, never a write: the caller prints
it and exits nonzero.  Only stdlib is used, so oracle.py (system python3 -I)
and the tests can import this module directly.
"""

import hashlib
import os
import re
import subprocess

# The rule-1 exemption marking (check_evidence.py's wave counts these); a
# front matter whose exemption opens with this is the convertible kind.
UNMEASURED = 'unmeasured WSO import'

MODULE_REL = 'tools/emu/cases/c%x.py'


def trace_source(addr, orig_sha, ours_sha, commit, sets, equal):
    """The check_evidence.py trace_source line for one all-EQUAL case.

    addr is the case address as an int (0x4029F0); the source names the case
    module in lower-case hex without 0x, as the gate's third form requires.
    """
    assert int(sets) >= 1 and int(sets) == int(equal), \
        'a trace_source must be a measurement: sets=%r equal=%r' % (sets,
                                                                   equal)
    assert re.fullmatch(r'[0-9A-Fa-f]{7,40}', commit), commit
    return 'emu c%x orig:%s ours:%s at:%s sets=%d equal=%d' % (
        addr, orig_sha, ours_sha, commit, int(sets), int(equal))


def _strip_quotes(value):
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in '"\'':
        return value[1:-1]
    return value


def convert(text, source):
    """Rewrite `text`'s front matter into a measured trace document.

    Drops the UNMEASURED exemption line, sets `kind: trace` and puts
    `source` in `trace_source:` (an emu trace_source already there is
    replaced, so a re-run updates the counts).  Any other front matter --
    a kept row's exemption, a frida trace_source, no exemption at all --
    is left exactly as it is.  Returns (new_text, converted).
    """
    lines = text.split('\n')
    if not lines or lines[0].strip() != '---':
        return text, False
    try:
        end = lines[1:].index('---') + 1
    except ValueError:
        return text, False
    head = lines[:end + 1]
    # the body after the closing delimiter, taken before `end` moves with
    # the edits below (slicing with the moved `end` repeated the delimiter)
    rest = lines[end + 1:]
    exemption_at = kind_at = trace_at = None
    for i, line in enumerate(head[1:end], start=1):
        name = line.split(':', 1)[0].strip()
        if name == 'exemption':
            exemption_at = i
        elif name == 'kind':
            kind_at = i
        elif name == 'trace_source':
            trace_at = i
    old_trace = _strip_quotes(head[trace_at].split(':', 1)[1]) \
        if trace_at is not None else None
    if exemption_at is not None and _strip_quotes(
            head[exemption_at].split(':', 1)[1]).startswith(UNMEASURED):
        pass  # the convertible kind: the exemption goes, the trace arrives
    elif old_trace is not None and old_trace.startswith('emu '):
        pass  # already measured by an emu case: refresh the trace_source
    else:
        return text, False
    if exemption_at is not None:
        # delete before any insert: an insert above the exemption line would
        # shift it and the wrong line would go (the 0x4EA3F0 shape, where
        # kind: precedes exemption:)
        del head[exemption_at]
        end -= 1
        if kind_at is not None and exemption_at < kind_at:
            kind_at -= 1
        if trace_at is not None and exemption_at < trace_at:
            trace_at -= 1
    if trace_at is not None:
        head[trace_at] = 'trace_source: %s' % source
    if kind_at is not None:
        head[kind_at] = 'kind: trace'
        if trace_at is None:
            head.insert(kind_at + 1, 'trace_source: %s' % source)
    else:
        head.insert(end, 'trace_source: %s' % source)
        head.insert(end, 'kind: trace')
    return '\n'.join(head + rest), True


def section(text, sets, module_rel):
    """Set the `## Emulation` section: one line per set, the case module.

    An existing section is replaced in full (from its heading to the next
    `## ` heading or end of file); everything outside it is untouched.
    """
    body = ('## Emulation\n\n'
            + ''.join('- `%s`: %s\n' % (s['set'], s['verdict'])
                      for s in sets)
            + '\nCase module: %s\n' % module_rel)
    if not text.endswith('\n'):
        text += '\n'
    out = []
    mode = 'copy'
    for line in text.split('\n'):
        if mode == 'in_section':
            if line.startswith('## '):
                mode = 'copy'
                out.append(line)
            continue
        if line == '## Emulation':
            mode = 'in_section'
            out.append(body)
            out.append('')  # blank line after the section
            continue
        out.append(line)
    if mode == 'copy':
        if out and out[-1] != '':
            out.append('')
        out.append(body)
    # a trailing '' from the split('\n') keeps the file newline-terminated
    return '\n'.join(out)


def _sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(), b''):
            h.update(chunk)
    return h.hexdigest()


def write_all(results, repo, orig_exe, ours_exe, run=None, echo=print,
              check=None):
    """Write evidence for every all-EQUAL case in `results` (run.py --json).

    results maps '0x<ADDR>' to {'name', 'sets': [{'set', 'verdict',
    'detail'}]}.  A case with a DIFF set is printed as `finding:`, one with
    an ERROR set as `error:`, and neither is written; a missing evidence
    file is an `error:`.  `check`, when given, is the check_evidence module:
    every written trace_source is validated against its gate first.  Returns
    0 when every case was written, else 1.
    """
    if run is None:
        run = lambda argv, **kw: subprocess.run(argv, cwd=repo, **kw)
    commit = run(['git', 'rev-parse', '--short=12', 'HEAD'],
                 capture_output=True, text=True, check=True).stdout.strip()
    orig_sha = _sha256(orig_exe)
    ours_sha = _sha256(ours_exe)
    status = 0
    for key in sorted(results, key=lambda k: int(k, 16)):
        addr = int(key, 16)
        case = results[key]
        name, sets = case['name'], case['sets']
        equal = [s for s in sets if s['verdict'].startswith('EQUAL')]
        if len(equal) != len(sets):
            status = 1
            for s in sets:
                if not s['verdict'].startswith('EQUAL'):
                    kind = 'finding' if s['verdict'] == 'DIFF' else 'error'
                    echo('%s: %s %s %s: %s' % (kind, key, name, s['set'],
                                               s['detail']))
            continue
        rel = 'docs/evidence/%X.md' % addr
        path = os.path.join(repo, rel)
        if not os.path.isfile(path):
            status = 1
            echo('error: %s not found (%s %s)' % (rel, key, name))
            continue
        source = trace_source(addr, orig_sha, ours_sha, commit,
                              len(sets), len(equal))
        if check is not None:
            m = check.TRACE_SOURCES[2].match(source)
            if not m or not check.emu_counts_ok(m):
                status = 1
                echo('error: %s trace_source rejected by the gate: %s'
                     % (rel, source))
                continue
        with open(path) as f:
            text = f.read()
        text, converted = convert(text, source)
        text = section(text, sets, MODULE_REL % addr)
        with open(path, 'w') as f:
            f.write(text)
        if converted:
            echo('evidence: %s: kind: trace, %s exemption dropped'
                 % (rel, UNMEASURED))
        else:
            echo('evidence: %s: front matter kept, Emulation section written'
                 % rel)
    return status
