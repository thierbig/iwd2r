"""Function labels for the vanilla iwd2.exe Ghidra project.

ours(src_dir) reads the `// 0xADDR` comments above function definitions in
src/**/*.cpp, extra(path) reads tools/ghidra/extra_labels.txt (functions we
identified but have not ported yet), ieex(db_path) reads IEex's
function_names.db, and merge() combines them: our names first, then the extra
names, then IEex's. definitions(lines) gives the
line positions of the labelled definitions; tools/check_evidence.py uses it
so both tools agree on what a labelled function is.
"""

import os
import re
import sys

ADDRESS = re.compile(r'^// 0x([0-9A-Fa-f]+)\s*$')
OPERATOR = re.compile(r'((?:[\w~]+::)*operator\s*(?:\(\)|[^\s(]+))\s*\(')
PLAIN = re.compile(r'((?:\w+::)*~?\w+)\s*$')
def _definition_name(line):
    """The function name defined on line, or None if it is not a definition.

    A first signature line may end in `,` (a signature that wraps) when the
    line is top-level — checked by the caller, a top-level line has no
    leading whitespace — and its `(` is unclosed; the name still comes from
    that line. Everything else a data line (`;`, `=` before any `(`) stays
    rejected."""
    if not line or line[0].isspace() or line.startswith(('//', '/*', '#', '{', '}')):
        return None
    text = line.rstrip()
    if text.endswith(';') or '(' not in text:
        return None
    if text.endswith(',') and text.count('(') <= text.count(')'):
        return None
    m = OPERATOR.search(text)
    if m:
        return m.group(1).replace(' ', '')
    head = text[:text.index('(')]
    if '=' in head:
        return None
    m = PLAIN.search(head)
    return m.group(1) if m else None


def definitions(lines):
    """(address_line, definition_line, address, name) for each `// 0xADDR`
    line directly above a function definition (0-based line indexes). Any
    number of `//` comment (and blank) lines may sit between the marker and
    the definition; the scan still stops at another marker, a `{`/`}` line
    or any other non-comment line."""
    found = []
    for i, line in enumerate(lines):
        m = ADDRESS.match(line)
        if not m:
            continue
        for j in range(i + 1, len(lines)):
            follow = lines[j]
            if ADDRESS.match(follow) or follow.startswith(('{', '}')):
                break
            name = _definition_name(follow)
            if name:
                found.append((i, j, int(m.group(1), 16), name))
                break
            if follow.strip() and not follow.lstrip().startswith('//'):
                break
    return found


def _scan(lines):
    return [(addr, name) for _, _, addr, name in definitions(lines)]


def ours(src_dir):
    """(address, name) for each `// 0xADDR` line directly above a function
    definition in the .cpp files under src_dir, in file order."""
    result = []
    for root, dirs, files in os.walk(src_dir):
        dirs.sort()
        for name in sorted(files):
            if name.endswith('.cpp'):
                with open(os.path.join(root, name), encoding='utf-8',
                          errors='replace') as f:
                    result.extend(_scan(f.read().splitlines()))
    return result


def ieex(db_path):
    """(address, name) from IEex's `0xADDR NAME` lines, without FUN_ names.

    IEex is optional: a missing database means no IEex names, said once on
    stderr with the path it looked at, not an error."""
    result = []
    try:
        db = open(db_path, encoding='utf-8', errors='replace')
    except FileNotFoundError:
        print(f'labels: no IEex database at {db_path}; '
              'continuing without IEex names', file=sys.stderr)
        return result
    with db:
        for line in db:
            parts = line.split()
            if len(parts) != 2 or parts[1].startswith('FUN_'):
                continue
            result.append((int(parts[0], 16), parts[1]))
    return result


def extra(path):
    """(address, name) from `0xADDR NAME` lines; blank lines and lines
    starting with # (the evidence for each name) are skipped."""
    result = []
    with open(path, encoding='utf-8') as f:
        for number, line in enumerate(f, 1):
            text = line.strip()
            if not text or text.startswith('#'):
                continue
            parts = text.split()
            if len(parts) != 2 or not re.fullmatch(r'0x[0-9A-Fa-f]+', parts[0]):
                raise ValueError('%s:%d: expected "0xADDR NAME": %r'
                                 % (path, number, text))
            result.append((int(parts[0], 16), parts[1]))
    return result


def merge(ours, ieex, extra=()):
    """One name per address, sorted by address. Our names win, then the extra
    names; an IEex name equal to one of those names elsewhere gets the prefix
    ieex_."""
    merged = {}
    for addr, name in ours:
        merged.setdefault(addr, name)
    for addr, name in extra:
        merged.setdefault(addr, name)
    taken = set(merged.values())
    for addr, name in ieex:
        if addr in merged:
            continue
        merged[addr] = 'ieex_' + name if name in taken else name
    return sorted(merged.items())
