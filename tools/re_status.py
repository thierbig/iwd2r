"""Recovery status: how much of the engine is recovered and how much of that
is verified against the original, kept in docs/re-status.json and in the
README's `### Recovery status` block.

Usage: python3 -I tools/re_status.py compute | --check | --markdown
                                     [--repo DIR] [--exports DIR]

compute   reads the Ghidra function table (<exports>/functions.tsv, tab
          separated `addr name size`), src/**/*.cpp, docs/evidence/*.md and
          scenarios/, then writes docs/re-status.json and the README block.
--check   exit 1 when the README block differs from the rendering of the
          committed docs/re-status.json (needs no exports; the gate runs it).
--markdown prints the block for the committed docs/re-status.json.

Denominator: the table minus `Unwind@`, `Catch@`, `Finally@` funclets and
`thunk_` thunks. Recovered: a standalone // 0xADDR marker in src/*.cpp or *.h
precedes a definition whose body is real by four rules, in order (classify):
a body whose only code is a constant return or nothing stays a stub when it
carries a TODO, NotImplemented or assert(false) (its raw text, comments
included), and NotImplemented or assert(false) in the code itself is a stub
beside real statements too; any statement after masking comments is real;
an empty body is real when the exe's function
(functions.tsv) is at most 16 bytes; an empty constructor or destructor body
is real when every callee (callgraph.tsv) is a constructor, destructor,
`operator new`/`operator delete`, a `_Construct`/`_Destruct`-style helper or
one of the compiler's member-array helpers (`construct_array`,
`array_destruct`, the `eh vector` constructor/destructor iterators) of
a base or member class. A destructor may additionally call what the exe
destructor of one of its base classes calls (its inlined base destructor:
the base from our class declaration in src/*.h, the base destructor's
address from our `// 0xADDR` marker above it, up the chain); `operator
delete` and `array_destruct` never count on their own for that path, and a
constructor still keeps `operator new` (a member allocation our empty body
would miss). The rest stay stubs; rule 4's leftovers are the
porting queue compute prints. Verified: recovered
and the evidence file's kind is oracle, replay or trace. A scenario counts as
matched when scenarios/<name>.txt has a <name>.expect.json beside it, the
checkpoints of the original's state dumps (scenarios/pending/ is not counted).
"""
import datetime
import glob
import importlib.util
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BEGIN = '<!-- re-status:begin -->'
END = '<!-- re-status:end -->'
FUNCLETS = ('Unwind@', 'Catch@', 'Finally@')
KINDS = ('oracle', 'replay', 'trace')


def _load(name):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(HERE, name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _norm(address):
    a = str(address).strip()
    if a[:2].lower() == '0x':
        a = a[2:]
    return a.upper().lstrip('0') or '0'


def read_table(path):
    rows = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 3:
                continue
            try:
                size = int(parts[2])
            except ValueError:
                continue  # header
            rows.append((_norm(parts[0]), parts[1], size))
    return rows


def read_graph(path):
    """{normalised caller: [normalised callee, ...]} from callgraph.tsv, one
    line per function: the caller, then each callee address."""
    graph = {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if parts and parts[0]:
                graph[_norm(parts[0])] = [_norm(p) for p in parts[1:] if p]
    return graph


MARKER = re.compile(r'^\s*//\s*(0x[0-9A-Fa-f]+)\s*$')
OPERATOR = re.compile(r'operator\s*(?:\(\)|[-+*/%^&|!<>=\[\]~,]+)')
INIT = re.compile(r'(?<!:)[:,]\s*[\w:<>]+\s*$')
ASSIGN = re.compile(r'[^=!<>]=[^=]')
CTOR = re.compile(r'(\w+)::(~?)\1$')
CLASSDECL = re.compile(r'^class\s+(\w+)\s*:\s*(.+)$')
# One `return <constant>;`, whitespace squeezed out: a literal (number, char
# or string), TRUE/FALSE, NULL/nullptr, or an enum/static constant such as
# CGameAIBase::ACTION_ERROR (qualified, or an ALL_CAPS name).
RETURN_CONST = re.compile(
    r'return(?:"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    r'|[-+]?(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d*)?(?:[eE][-+]?\d+)?)[uUlLfF]{0,3}'
    r'|NULL|nullptr'
    r'|\w+(?:::\w+)+'
    r'|[A-Z][A-Z0-9_]*);')


def _mask(text):
    """Comments and string/char literals as spaces, offsets kept."""
    pat = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
    return pat.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)


def _callee_name(address, table, ours):
    """A callee's name: functions.tsv's, else our marker's, else None."""
    row = table.get(address) if table else None
    if row and row[0]:
        return row[0]
    return (ours or {}).get(address)


def _member_op(name):
    """True for a callee that only constructs or destroys a base or member:
    a constructor, destructor, `operator new`/`operator delete`, a
    `_Construct`/`_Destruct`-style helper or one of the compiler's
    member-array helpers (`construct_array`, `array_destruct`, the `eh
    vector` constructor/destructor iterators), judged by its name (Ghidra's
    or our marker's); thunk_ and j_ thunks count as their target."""
    if not name:
        return False
    while name.startswith(('thunk_', 'j_')):
        name = name[name.index('_') + 1:]
    if name.startswith(('??0', '??1')):
        return True
    if re.search(r'operator\s*(?:new|delete)\b', name):
        return True
    if name in ('construct_array', 'array_destruct'):
        return True
    lowered = name.lower()
    if 'eh vector' in lowered and ('construct' in lowered
                                   or 'destruct' in lowered):
        return True
    return (bool(CTOR.search(name)) or name.startswith('~')
            or name.endswith(('_Construct', '_Destruct')))


def _bases(text):
    """{class: [base class, ...]} from our `class X : public A, public B`
    declarations (one line each, comments masked first). Template bases
    (`CResHelper<CResBitmap, 1>`) count as their template's name; their
    destructor has no marker, so the chain simply stops there."""
    out = {}
    for ln in _mask(text).split('\n'):
        m = CLASSDECL.match(ln.strip())
        if not m:
            continue
        heads = []
        for b in m.group(2).strip().rstrip('{ \t').split(','):
            b = re.sub(r'^(?:virtual\s+)?(?:public|protected|private)\s+',
                       '', b.strip()).split('<')[0].strip()
            if re.fullmatch(r'\w+', b):
                heads.append(b)
        if heads:
            out[m.group(1)] = heads
    return out


def _dtor_class(name):
    """The class of a destructor name ('CButton::~CButton' -> 'CButton',
    '~CButton' -> 'CButton'), None for a constructor or anything else."""
    m = CTOR.search(name)
    if m and m.group(2):
        return m.group(1)
    if name.startswith('~'):
        return name[1:].split('::')[0]
    return None


def _base_callees(cls, bases, dtors, graph, seen=frozenset()):
    """Every callee of the exe destructors of cls's bases, transitively up
    the chain: the addresses a derived destructor may call because its base
    destructor's body is inlined into it (`dtors` maps a class to the
    address of our destructor marker for it)."""
    out = set()
    for base in bases.get(cls, ()):
        addr = dtors.get(base)
        if addr is None or addr in seen:
            continue
        seen = seen | {addr}
        out |= set(graph.get(addr, ()))
        out |= _base_callees(base, bases, dtors, graph, seen)
    return out


def classify(body, name='', address=None, table=None, graph=None, ours=None,
             bases=None, dtors=None, raw=None):
    """(stub, why) for one masked, whitespace-free body, by the four rules of
    the 2026-10-30 census, in order; a body that matches none stays a stub.
    `raw` is the same span of the body unmasked (comments kept), whitespace
    squeezed out like `body`; without it the TODO half of rule 1 sees only
    the masked code, i.e. no comments. Rule 1 (2026-11-02 ruling): a body
    whose masked code is a constant return or nothing stays a stub when its
    raw text carries TODO, NotImplemented or assert(false), and NotImplemented
    or assert(false) in the masked code itself is a stub whatever else the
    body holds. `name` is our name for the definition, `address` the marker's;
    `table` maps a normalised address to (name, size) in functions.tsv and
    `graph` a caller to its callee addresses in callgraph.tsv. Rules 3 and 4
    need the exports, so with no table an empty body stays a stub. A
    destructor may additionally call what the exe destructor of one of its
    base classes calls (`bases` maps a class to its base classes, `dtors` a
    class to the address of our destructor marker for it): the inlined base
    destructor."""
    inner = body[1:-1] if body[:1] == '{' and body[-1:] == '}' else body
    if raw is None:
        raw = body
    if ('NotImplemented' in inner
            or 'assert(false' in inner.replace(' ', '')):
        return True, 'todo'  # in the code itself, whatever else it holds
    if ('TODO' in raw or 'NotImplemented' in raw
            or 'assert(false' in raw.replace(' ', '')):
        if not inner or RETURN_CONST.fullmatch(inner):
            return True, 'todo'  # only a constant return or nothing
    if inner:  # rule 2: any statement at all, comments masked out
        return False, 'statement'
    if table is None or address is None:
        return True, 'empty'
    row = table.get(address)
    if row and row[1] <= 16:  # rule 3: the exe's function is as empty as ours
        return False, 'size'
    if CTOR.search(name) or name.startswith('~'):
        if graph is None:
            return True, 'ctor-nograph'
        allowed = ()
        cls = _dtor_class(name)
        if cls and bases and dtors:  # the base path is for destructors only;
            allowed = _base_callees(cls, bases, dtors, graph)  # a ctor keeps
        callee = next((c for c in graph.get(address, [])  # its operator_new
                       if not (_member_op(_callee_name(c, table, ours))
                               or c in allowed)), None)
        if callee is None:  # rule 4: only base/member construction/destruction
            return False, 'ctor-callees'
        return True, 'ctor-callee:' + (_callee_name(callee, table, ours)
                                       or '0x' + callee)
    return True, 'empty'


def _was_stub(body):
    """The pre-Task-55 judgement, kept only so compute can report what the
    new rules add or demote."""
    if not (len(body) < 40 or 'TODO' in body or 'NotImplemented' in body
            or 'assert(false' in body.replace(' ', '')):
        return False
    if ('TODO' in body or 'NotImplemented' in body
            or 'assert(false' in body.replace(' ', '')):
        return True
    return not (len(body) < 40 and ('return' in body or ASSIGN.search(body)))


def _name(head):
    """Our function name from a definition head (text up to its `{`)."""
    head = head.split('(', 1)[0]
    m = re.search(r'((?:[~\w]+::)*operator\s*\S+|[~\w:]+)\s*$', head.strip())
    return m.group(1).replace(' ', '') if m else ''


def _bodies(text):
    """[(address, our name, body, raw)] for every standalone // 0xADDR marker
    that precedes a function definition. A marker followed by another marker,
    or by a declaration or data definition (`;` or `=` before any body), is
    none. `body` is the masked, whitespace-free text between the braces and
    `raw` the same span unmasked, comments kept (whitespace squeezed out of
    both): rule 1's TODO check reads `raw`."""
    lines = text.split('\n')
    masked = _mask(text)
    starts = []
    pos = 0
    for ln in lines:
        starts.append(pos)
        pos += len(ln) + 1
    mlines = masked.split('\n')
    scan = OPERATOR.sub(lambda m: 'operator' + ' ' * (len(m.group(0)) - 8), masked)
    out = []
    for i, ln in enumerate(lines):
        m = MARKER.match(ln)
        if not m:
            continue
        j = i + 1
        while j < len(lines) and not mlines[j].strip() and not MARKER.match(lines[j]):
            j += 1
        if j >= len(lines) or MARKER.match(lines[j]):
            continue
        k, paren = starts[j], 0
        end = len(masked)
        while k < end:
            c = scan[k]
            if c == '(':
                paren += 1
            elif c == ')':
                paren -= 1
            elif paren == 0 and c in ';=':
                k = -1
                break
            elif paren == 0 and c == '{':
                if INIT.search(scan[starts[j]:k]):  # member initializer {..}
                    d = 0
                    while k < end:
                        d += (scan[k] == '{') - (scan[k] == '}')
                        if d == 0:
                            break
                        k += 1
                else:
                    break
            k += 1
        if k < 0 or k >= end:
            continue
        depth, e = 0, k
        while e < end:
            if masked[e] == '{':
                depth += 1
            elif masked[e] == '}':
                depth -= 1
                if depth == 0:
                    break
            e += 1
        body = re.sub(r'\s+', '', masked[k:e + 1])
        out.append((m.group(1), _name(masked[starts[j]:k]), body,
                    re.sub(r'\s+', '', text[k:e + 1])))
    return out


def markers(text, table=None, graph=None, ours=None, bases=None, dtors=None):
    """[(address, our name, stub?)] for every marker _bodies finds, judged by
    classify."""
    return [(a, name, classify(body, name, _norm(a), table, graph, ours,
                               bases, dtors, raw)[0])
            for a, name, body, raw in _bodies(text)]


def scan(repo, table=None, graph=None):
    """(real, added, demoted, kept) over every marker in src/**/*.{cpp,h}.
    `real` maps a normalised address to our name when classify calls the body
    real; `added` counts, per rule, the addresses no marker of theirs was real
    under the pre-Task-55 rule; `demoted` counts the reverse (addresses only
    their comments made look long); `kept` lists [(address, name, exe size,
    callee)] of empty
    constructors and destructors rule 4 left as stubs, with the first callee
    that is not a base/member constructor, destructor, operator new/delete
    or member-array helper — the porting queue."""
    bodies = []
    bases = {}
    for ext in ('cpp', 'h'):
        for path in sorted(glob.glob(os.path.join(repo, 'src', '**', '*.' + ext),
                                     recursive=True)):
            with open(path, encoding='utf-8', errors='replace') as f:
                text = f.read()
            if ext == 'h':
                bases.update(_bases(text))
            bodies.extend(_bodies(text))
    ours = {}
    for a, name, _, _ in bodies:
        ours.setdefault(_norm(a), name)
    dtors = {}
    for a, name in ours.items():
        cls = _dtor_class(name)
        if cls and cls not in dtors:  # first marker wins, like ours above
            dtors[cls] = a
    old_real = {_norm(a) for a, _, body, _ in bodies if not _was_stub(body)}
    real = {}
    added = {'statement': 0, 'size': 0, 'ctor-callees': 0}
    demoted, kept, seen = 0, [], set()
    for a, name, body, raw in bodies:
        a = _norm(a)
        if a in seen:  # two markers for one address: the first one wins
            continue
        seen.add(a)
        stub, why = classify(body, name, a, table, graph, ours, bases, dtors,
                             raw)
        if not stub:
            real.setdefault(a, name)
            if a not in old_real:
                added[why] += 1
        elif a in old_real:
            demoted += 1
        elif why.startswith('ctor-callee'):
            row = (table or {}).get(a)
            kept.append((a, name, row[1] if row else None,
                         why[len('ctor-callee:'):]))
    return real, added, demoted, kept


def recovered_functions(repo, table=None, graph=None):
    """{normalised address: our name} of every marker with a real body."""
    return scan(repo, table, graph)[0]


def evidence_kinds(repo):
    ce = _load('check_evidence')
    out = {}
    for path in glob.glob(os.path.join(repo, 'docs', 'evidence', '*.md')):
        with open(path, encoding='utf-8', errors='replace') as f:
            fm = ce.front_matter(f.read())
        kind = fm.get('kind')
        if kind == 'trace' and not fm.get('trace_source', '').startswith('frida '):
            kind = 'fork-trace'  # the fork's measurement, not ours
        out[_norm(os.path.basename(path)[:-3])] = kind
    return out


def scenarios(repo):
    d = os.path.join(repo, 'scenarios')
    return sorted(os.path.basename(p)[:-4]
                  for p in glob.glob(os.path.join(d, '*.txt'))
                  if os.path.exists(p[:-4] + '.expect.json'))


def compute(repo, exports):
    table = os.path.join(exports, 'functions.tsv')
    if not os.path.isfile(table):
        raise SystemExit(f're_status: no Ghidra function table at {table}; '
                         'run tools/ghidra/export_decompiles.py or pass --exports DIR')
    rows = read_table(table)
    funclets = sum(1 for _, n, _ in rows if n.startswith(FUNCLETS))
    thunks = sum(1 for _, n, _ in rows
                 if n.startswith('thunk_') and not n.startswith(FUNCLETS))
    den = [(a, s) for a, n, s in rows
           if not n.startswith(FUNCLETS) and not n.startswith('thunk_')]
    table = {a: (n, s) for a, n, s in rows}
    graph_path = os.path.join(exports, 'callgraph.tsv')
    graph = read_graph(graph_path) if os.path.isfile(graph_path) else None
    if graph is None:
        print('warning: no callgraph.tsv next to functions.tsv; rule 4 keeps '
              'every empty constructor and destructor a stub', file=sys.stderr)
    recfn, added, demoted, kept = scan(repo, table, graph)
    rec = set(recfn)
    names = {a: n for a, n, _ in rows}
    mismatched = sorted(
        (a, recfn[a], names[a]) for a in rec & set(names)
        if recfn[a] and recfn[a] != names[a]
        and not names[a].startswith(('FUN_', 'sub_')))
    if mismatched:
        print('warning: %d recovered addresses whose function name differs '
              'from the Ghidra name (address, ours, Ghidra):' % len(mismatched),
              file=sys.stderr)
        for a, ours, theirs in mismatched:
            print(f'  0x{a}  {ours}  {theirs}', file=sys.stderr)
    kinds = evidence_kinds(repo)
    recovered = [(a, s) for a, s in den if a in rec]
    verified = [(a, s) for a, s in recovered if kinds.get(a) in KINDS]
    by_kind = {k: sum(1 for a, _ in verified if kinds[a] == k) for k in KINDS}
    print('re_status: the rules recovered %d more than the old count '
          '(rule 2, any statement: %d; rule 3, empty and exe at most 16 '
          'bytes: %d; rule 4, empty ctor/dtor with base/member callees only: '
          '%d) and demoted %d addresses only their comments made look long; '
          'rule 4 kept %d empty constructors/destructors as stubs:'
          % (sum(added.values()) - demoted, added['statement'], added['size'],
             added['ctor-callees'], demoted, len(kept)), file=sys.stderr)
    for a, name, size, callee in kept:
        print(f'  rule 4 kept 0x{a}  {name}  (exe {size if size is not None else "?"} '
              f'bytes)  callee {callee}', file=sys.stderr)
    return {
        'at': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%d'),
        'ghidra': {'functions': len(den), 'bytes': sum(s for _, s in den),
                   'excluded': {'seh_funclets': funclets, 'thunks': thunks}},
        'recovered': {'functions': len(recovered),
                      'bytes': sum(s for _, s in recovered)},
        'verified': {'functions': len(verified),
                     'bytes': sum(s for _, s in verified), 'by_kind': by_kind},
        'scenarios': scenarios(repo),
    }


def _pct(n, d):
    return f'{100.0 * n / d:.1f}%' if d else '0.0%'


def render(s):
    g, r, v = s['ghidra'], s['recovered'], s['verified']
    k = v['by_kind']
    ex = g['excluded']
    names = ', '.join(f'`{n}`' for n in s['scenarios'])
    return '\n'.join([
        BEGIN,
        f"Generated by `tools/re_status.py compute` on {s['at']} UTC; "
        "`--check` in the gate keeps it current.",
        '',
        '| Measure | Value | What it counts |',
        '|---|---|---|',
        f"| `.text` code recovered | **{_pct(r['bytes'], g['bytes'])}** "
        f"({r['bytes']:,} / {g['bytes']:,} bytes) | Byte-weighted, over the "
        f"{g['functions']:,} functions Ghidra lists minus {ex['seh_funclets']:,} "
        f"SEH funclets and {ex['thunks']:,} thunks. |",
        f"| Functions recovered | **{_pct(r['functions'], g['functions'])}** "
        f"({r['functions']:,} / {g['functions']:,}) | By count, same "
        "denominator; a function counts once its body is more than a stub: a body whose only code is a constant return or nothing stays a stub when it carries a TODO, NotImplemented or assert(false) (as does any body with NotImplemented or assert(false) in its code), any statement counts, and an empty body counts when the exe's function is at most 16 bytes or is a constructor/destructor calling only base/member constructors, destructors and the compiler's member construction/destruction helpers (`operator new`/`operator delete`, `construct_array`, `array_destruct`, the `eh vector` iterators); a destructor may also call what the exe destructor of its base classes calls (the inlined base destructor). |",
        f"| Verified against the original | "
        f"**{_pct(v['functions'], g['functions'])}** ({v['functions']:,} / "
        f"{g['functions']:,}; {v['bytes']:,} bytes) | Recovered functions whose "
        f"evidence is an oracle case ({k['oracle']:,}), a replay checkpoint "
        f"({k['replay']:,}) or a Frida trace of our build against the original ({k['trace']:,}); the rest "
        "depend on a reading of the decompile. |",
        f"| Scenarios matched | {len(s['scenarios'])} | {names}: scenarios "
        "with recorded original state dumps (`scenarios/*.expect.json`), "
        "replayed identically at every checkpoint. |",
        END])


def _snapshot(repo):
    path = os.path.join(repo, 'docs', 're-status.json')
    try:
        with open(path, encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def _splice(text, block):
    """README text with the block in place; None when there is no block."""
    a, b = text.find(BEGIN), text.find(END)
    if a < 0 or b < a:
        return None
    return text[:a] + block + text[b + len(END):]


SECTION = '### Recovery status\n\n'


def _insert(text, block):
    i = text.find('### Roadmap')
    if i < 0:
        raise SystemExit('re_status: README.md has no "### Roadmap" heading '
                         'to put the Recovery status block before')
    return text[:i] + SECTION + block + '\n\n' + text[i:]


def main(argv):
    args = list(argv)
    opts = {'--repo': os.path.dirname(HERE), '--exports': None}
    for o in opts:
        if o in args:
            i = args.index(o)
            opts[o] = args[i + 1]
            del args[i:i + 2]
    repo, exports = opts['--repo'], opts['--exports']
    if len(args) != 1 or args[0] not in ('compute', '--check', '--markdown'):
        print(__doc__.split('\n\n')[0], file=sys.stderr)
        return 2
    cmd = args[0]
    readme = os.path.join(repo, 'README.md')
    if cmd == 'compute':
        if exports is None:
            exports = _load('truth').exports_default()
        snap = compute(repo, exports)
        os.makedirs(os.path.join(repo, 'docs'), exist_ok=True)
        with open(os.path.join(repo, 'docs', 're-status.json'), 'w') as f:
            json.dump(snap, f, indent=2)
            f.write('\n')
        with open(readme, encoding='utf-8') as f:
            text = f.read()
        block = render(snap)
        new = _splice(text, block) or _insert(text, block)
        with open(readme, 'w', encoding='utf-8') as f:
            f.write(new)
        print(block)
        return 0
    snap = _snapshot(repo)
    if snap is None:
        print('docs/re-status.json is missing or unreadable; run '
              'python3 -I tools/re_status.py compute', file=sys.stderr)
        return 1
    if cmd == '--markdown':
        print(render(snap))
        return 0
    try:
        with open(readme, encoding='utf-8') as f:
            text = f.read()
    except OSError:
        text = ''
    if _splice(text, render(snap)) != text:
        print('README.md: the Recovery status block is stale or missing; run '
              'python3 -I tools/re_status.py compute', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
