#!/usr/bin/env python3
"""Class-layout reconciliation between the original VC6 exe and our MSVC build.

The headers carry the original exe's member offsets as `/* XXXX */` comments
on every data member (base-class members are base-relative, derived members
absolute).  Our build's PDB carries the same layout, at MSVC's natural
alignment instead of VC6's pack(2).  field_map() joins the two into
{exe_offset: our_offset} so the differential emulator can compare `this`
writes across the layout drift; members that exist on only one side are
reported, never guessed.

  exe_offsets(header_text, cls)          -> {member: exe_offset}
  our_offsets(pdb_path, cls)             -> {member: (our_offset, size)}
  field_map(cls, header_dir, pdb_path)   -> FieldMap(map, exe_only, ours_only)

The llvm-pdbutil-18 `dump -types` parse is cached (parsed, not raw) under
$XDG_CACHE_HOME/iwd2r/emu/ (default ~/.cache/iwd2r/emu/), keyed by pdb
size+mtime.  This module needs no third-party packages.
"""

import json
import os
import re
import subprocess
import sys
from collections import namedtuple
from pathlib import Path

PDBUTIL = 'llvm-pdbutil-18'

# bump when the parsed-table shape changes (e.g. synthetic basic records);
# caches with a different version are treated as absent
CACHE_VERSION = 2

# --------------------------------------------------------------- type sizes

# VC6/Win32 scalar and MFC types appearing as data members in src/*.h.
HEADER_BUILTIN = {
    'void': 0, 'char': 1, 'signed char': 1, 'unsigned char': 1, 'BYTE': 1,
    'BOOLEAN': 1, 'bool': 1, 'short': 2, 'SHORT': 2, 'unsigned short': 2,
    'USHORT': 2, 'WORD': 2, 'wchar_t': 2, 'int': 4, 'signed int': 4,
    'unsigned int': 4, 'UINT': 4, 'unsigned': 4, 'long': 4, 'LONG': 4,
    'unsigned long': 4, 'ULONG': 4, 'DWORD': 4, 'BOOL': 4, 'float': 4,
    'HRESULT': 4, 'SCODE': 4, '__int64': 8, 'unsigned __int64': 8,
    'LONG64': 8, 'ULONGLONG': 8, 'LONGLONG': 8, 'double': 8,
    'CPoint': 8, 'CSize': 8, 'CRect': 16, 'CString': 4, 'POSITION': 4,
    'STRREF': 4, 'ULONG32': 4, 'UINT32': 4, 'INT32': 4, 'UINT64': 8,
    'LONG32': 4,
}

# Names as llvm-pdbutil prints basic types inline: `0x0074 (int)`.
PDB_BUILTIN = {
    'void': 0, 'char': 1, 'unsigned char': 1, 'signed char': 1, 'bool': 1,
    'char16_t': 2, 'short': 2, 'unsigned short': 2, 'signed short': 2,
    'wchar_t': 2, 'char32_t': 4, 'int': 4, 'unsigned': 4, 'unsigned int': 4,
    'signed int': 4, 'long': 4, 'unsigned long': 4, 'signed long': 4,
    'float': 4, 'double': 8, 'long double': 8, '__int64': 8,
    'unsigned __int64': 8, 'long long': 8, 'unsigned long long': 8,
    'signed __int64': 8,
}

FieldMap = namedtuple('FieldMap', 'map exe_only ours_only')

# ------------------------------------------------------------ header parsing

_CLASS_RE = re.compile(r'^\s*(?:class|struct)\s+([A-Za-z_]\w*)\s*'
                       r'(?::\s*([^{;]+))?\s*\{', re.MULTILINE)
_MEMBER_RE = re.compile(r'/\*\s*(?:0[xX])?([0-9A-Fa-f]{3,8})\s*\*/\s*([^;{}]+);')
# `Type  name`, `Type  name[N]`, `Type  name = init`, `Type* name`
_DECL_RE = re.compile(r'^(?P<type>.+?)\s*\b(?P<name>[A-Za-z_]\w*)\s*'
                      r'(?P<dims>(?:\[[^\]]*\])*)\s*(?:=\s*.+)?$')
_DIM_RE = re.compile(r'\[(\d+)\]')

_ACCESS = {'public', 'private', 'protected', 'virtual'}


def _align2(n):
    return (n + 1) & ~1


def parse_header_classes(text):
    """{class name: {'bases': [name], 'virtual': bool,
                     'members': [(offset, type, name, dims)]}} for one header."""
    out = {}
    for m in _CLASS_RE.finditer(text):
        name, bases_decl = m.group(1), (m.group(2) or '')
        # body = up to the matching close brace (inner braces all balance)
        depth, i = 1, m.end()
        while i < len(text) and depth:
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
            i += 1
        body = text[m.end():i]
        bases = []
        for part in bases_decl.split(','):
            toks = [t for t in part.replace('\n', ' ').split() if t not in _ACCESS]
            if toks:
                bases.append(toks[-1].strip())
        members, has_virtual = [], False
        for mm in _MEMBER_RE.finditer(body):
            off, decl = int(mm.group(1), 16), mm.group(2).strip()
            if 'virtual' in decl.split() or 'override' in decl.split():
                has_virtual = True
                continue                       # vtable slot (has its own comment)
            if '(' in decl or 'static' in decl.split():
                if 'static' not in decl.split():
                    has_virtual = has_virtual or 'virtual' in decl
                continue                       # method / function pointer
            dm = _DECL_RE.match(decl)
            if not dm:
                continue
            dims = _DIM_RE.findall(dm.group('dims') or '')
            members.append((off, dm.group('type').strip(), dm.group('name'), dims))
        if 'virtual' in body or 'override' in body:
            has_virtual = True
        out[name] = {'bases': bases, 'virtual': has_virtual, 'members': members}
    return out


def _exe_sizeof(type_str, dims, resolve, warn, where):
    """Data-member size on the exe side, from the declared type."""
    if dims:
        elem = _exe_sizeof(type_str, [], resolve, warn, where)
        n = 1
        for d in dims:
            n *= int(d)
        return None if elem is None else elem * n
    t = type_str.strip()
    while t.startswith('const '):
        t = t[6:].strip()
    if t.endswith('*') or t.endswith('&'):
        return 4
    if t in HEADER_BUILTIN:
        return HEADER_BUILTIN[t]
    if t.startswith('enum '):
        return 4                            # all our enums are int-sized
    rec = resolve(t)
    if rec is not None:
        return exe_class_size(rec, resolve, warn)
    warn(f'{where}: unknown member type {t!r}')
    return None


def exe_class_size(rec, resolve, warn):
    """exe-side sizeof(rec): align_up(max member end, 2); vfptr counts 4."""
    end = 4 if rec['virtual'] else 0
    ms = sorted(rec['members'])
    for i, (off, t, name, dims) in enumerate(ms):
        nxt = ms[i + 1][0] if i + 1 < len(ms) else None
        size = (nxt - off) if nxt is not None else \
            _exe_sizeof(t, dims, resolve, warn, f'{name}')
        if size is not None:
            end = max(end, off + size)
        for b in rec['bases']:
            brec = resolve(b)
            if brec is not None:
                end = max(end, exe_class_size(brec, resolve, warn))
    return _align2(end)


def _exe_flat(texts, cls, warn, seen=None):
    """[(member, abs_exe_off, exe_size)] for cls, inherited members included.

    Derived-member comments are already absolute; base-member comments are
    base-relative and get shifted by the running base placement (bases are
    laid out in declaration order, each 2-aligned after the previous).
    """
    seen = seen or set()
    if cls in seen:
        warn(f'{cls}: inheritance cycle')
        return []
    seen = seen | {cls}
    rec = texts.get(cls)
    if rec is None:
        warn(f'{cls}: class not found in headers')
        return []

    def resolve(name):
        return texts.get(name)

    out = []
    base_start = 0
    for b in rec['bases']:
        brec = texts.get(b)
        if brec is None:
            # base declared but no header under src/ (MFC CObject, template
            # bases like CResHelper<CResItem, 1005>): treat as size 0.  The
            # base's own member comments are unknown either way, so only the
            # derived members (absolute comments) are affected -- and they
            # are not shifted by base_start to begin with.
            warn(f'{cls}: base {b!r} has no header; assumed empty')
            continue
        for name, off, size in _exe_flat(texts, b, warn, seen):
            out.append((name, base_start + off, size))
        bsz = exe_class_size(brec, resolve, warn)
        base_start = _align2(base_start + bsz)

    ms = sorted(rec['members'])
    for i, (off, t, name, dims) in enumerate(ms):
        nxt = ms[i + 1][0] if i + 1 < len(ms) else None
        size = (nxt - off) if nxt is not None else \
            _exe_sizeof(t, dims, resolve, warn, f'{cls}.{name}')
        out.append((name, off, size))
    return out


def exe_offsets(header_text, cls, headers=None):
    """{member: exe_offset} for cls within header_text.

    headers: optional {class: header_text} index (or callable class->text)
    used to follow base classes declared in other files.  Unknown members
    are reported on stderr, never guessed.
    """
    warn = lambda msg: print(f'exe_offsets: {msg}', file=sys.stderr)

    def texts_for(names):
        d = {}
        for n in names:
            if callable(headers):
                t = headers(n)
            elif isinstance(headers, dict):
                t = headers.get(n)
            else:
                t = None
            if t is not None:
                d.update(parse_header_classes(t))
        return d

    texts = parse_header_classes(header_text)
    if headers is not None:
        # keep pulling headers for unseen base classes until closure
        # (bounded rounds; parse_header_classes is cycle-safe downstream)
        for _ in range(8):
            need = set()
            for rec in texts.values():
                need |= {b for b in rec['bases'] if b not in texts}
            if not need:
                break
            texts.update(texts_for(need))
    return {name: off for name, off, _size in _exe_flat(texts, cls, warn)}


def exe_offsets_dir(cls, header_dir):
    """{member: exe_offset} for cls over every header under header_dir."""
    warn = lambda msg: print(f'exe_offsets_dir: {msg}', file=sys.stderr)
    return {name: off
            for name, off, _s in _exe_flat(header_index(header_dir), cls, warn)}


# --------------------------------------------------------------- PDB parsing

def _cache_path(pdb_path):
    base = os.environ.get('XDG_CACHE_HOME') or os.path.expanduser('~/.cache')
    d = Path(base) / 'iwd2r' / 'emu'
    st = pdb_path.stat()
    return d / f'types-{st.st_size}-{int(st.st_mtime)}.json'


def _basic_size(paren):
    """Size of an inline-printed basic type: `int`, `unsigned`, `void*`, ..."""
    t = paren.strip()
    while 'const' in t.split():
        t = ' '.join(w for w in t.split() if w != 'const')
    if '*' in t or '&' in t:
        return 4                                 # x86 pointers/references
    return PDB_BUILTIN.get(t)


def _parse_pdb_types(text):
    """Parse `llvm-pdbutil dump -types` output.

    Returns {class name: [(offset, size, member)] ...} flattened?  No --
    returns the raw table {'classes': {name: def_index}, 'types': {...}}
    with sizes resolved, compact enough to cache as JSON.
    """
    types = {}          # index(int) -> record dict
    classes = {}        # name -> defining index (non-forward)
    cur = None          # current record index being read
    pending_cont = None

    def new(idx, rec):
        types[idx] = rec
        return rec

    lines = text.splitlines()
    i = 0
    hdr = re.compile(r'^\s+0x([0-9A-Fa-f]+) \| LF_(\w+)(?: \[size = \d+\])?(.*)$')
    while i < len(lines):
        line = lines[i]
        m = hdr.match(line)
        if not m:
            i += 1
            continue
        idx, kind, rest = int(m.group(1), 16), m.group(2), m.group(3)
        i += 1
        if kind in ('CLASS', 'STRUCTURE', 'UNION', 'INTERFACE'):
            nm = re.match(r'\s*`([^`]+)`', rest)
            name = nm.group(1) if nm else ''
            rec = new(idx, {'kind': 'class', 'name': name, 'sizeof': 0,
                            'fieldlist': None})
            while i < len(lines) and lines[i].startswith('        ') \
                    and ' | LF_' not in lines[i]:
                l = lines[i]
                fm = re.search(r'field list: (0x[0-9A-Fa-f]+|<no type>)', l)
                if fm:
                    rec['fieldlist'] = None if fm.group(1) == '<no type>' \
                        else int(fm.group(1), 16)
                sm = re.search(r'sizeof (\d+)', l)
                if sm:
                    rec['sizeof'] = int(sm.group(1))
                i += 1
            if rec['fieldlist'] is not None:
                classes.setdefault(name, idx)
        elif kind == 'ENUM':
            nm = re.match(r'\s*`([^`]+)`', rest)
            name = nm.group(1) if nm else ''
            rec = new(idx, {'kind': 'enum', 'name': name, 'underlying': 4})
            while i < len(lines) and lines[i].startswith('        ') \
                    and ' | LF_' not in lines[i]:
                um = re.search(r'underlying type: 0x[0-9A-Fa-f]+ \(([^)]+)\)', lines[i])
                if um:
                    rec['underlying'] = PDB_BUILTIN.get(um.group(1).strip(), 4)
                i += 1
        elif kind == 'FIELDLIST':
            rec = new(idx, {'kind': 'fieldlist', 'members': [], 'bclasses': [],
                            'cont': None})
            # entries run until the next record header; unhandled entry kinds
            # (STMEMBER/ONEMETHOD/METHOD and their continuation lines) are
            # skipped line-by-line
            while i < len(lines) and not hdr.match(lines[i]):
                l = lines[i].strip()
                mm = re.match(r'- LF_MEMBER \[name = `([^`]+)`, '
                              r'Type = (0x[0-9A-Fa-f]+)'
                              r'(?: \(([^)]*)\))?, '
                              r'offset = (\d+)', l)
                if mm:
                    ti, paren = int(mm.group(2), 16), mm.group(3)
                    if paren is not None and ti < 0x1000 \
                            and str(ti) not in types:
                        # basic types print inline; low TPI indices are never
                        # record headers, so a synthetic record is safe
                        types[str(ti)] = {'kind': 'basic',
                                          'size': _basic_size(paren)}
                    rec['members'].append((int(mm.group(4)), ti, mm.group(1)))
                    i += 1
                    continue
                if l == '- LF_BCLASS':
                    nx = lines[i + 1].strip() if i + 1 < len(lines) else ''
                    tm = re.match(r'type = 0x([0-9A-Fa-f]+), offset = (\d+)', nx)
                    if tm:
                        rec['bclasses'].append((int(tm.group(2)),
                                                int(tm.group(1), 16)))
                    i += 2                        # entry spans two lines
                    continue
                cm = re.match(r'- LF_INDEX continuation = 0x([0-9A-Fa-f]+)', l)
                if cm:
                    rec['cont'] = int(cm.group(1), 16)
                i += 1
        elif kind == 'POINTER':
            new(idx, {'kind': 'pointer', 'size': 4})       # x86: always ptr32
        elif kind == 'ARRAY':
            while i < len(lines) and ' | LF_' not in lines[i]:
                sm = re.match(r'\s*size: (\d+),', lines[i])
                if sm:
                    new(idx, {'kind': 'array', 'size': int(sm.group(1))})
                    i += 1
                    break
                i += 1
        elif kind == 'MODIFIER':
            rec = new(idx, {'kind': 'modifier', 'referent': None})
            while i < len(lines) and lines[i].startswith('        ') \
                    and ' | LF_' not in lines[i]:
                rm = re.match(r'\s*referent = (0x[0-9A-Fa-f]+)', lines[i])
                if rm:
                    rec['referent'] = int(rm.group(1), 16)
                i += 1
        elif kind == 'TYPEDEF':
            rec = new(idx, {'kind': 'typedef', 'referent': None})
            while i < len(lines) and lines[i].startswith('        ') \
                    and ' | LF_' not in lines[i]:
                rm = re.search(r'type = (0x[0-9A-Fa-f]+)', lines[i])
                if rm:
                    rec['referent'] = int(rm.group(1), 16)
                i += 1
        # other kinds (arglists, procedures, ...) are irrelevant to layout
    return {'classes': classes, 'types': {str(k): v for k, v in types.items()}}


def load_pdb_types(pdb_path, verbose=False):
    """Parsed type table for pdb_path, cached by pdb size+mtime."""
    pdb_path = Path(pdb_path)
    cache = _cache_path(pdb_path)
    if cache.exists():
        try:
            data = json.loads(cache.read_text())
            if data.get('_v') == CACHE_VERSION:
                return data
        except (ValueError, OSError):
            pass
    out = subprocess.run([PDBUTIL, 'dump', '-types', str(pdb_path)],
                         capture_output=True, text=True, check=True)
    table = _parse_pdb_types(out.stdout)
    table['_v'] = CACHE_VERSION
    try:
        cache.parent.mkdir(parents=True, exist_ok=True)
        tmp = cache.with_suffix('.tmp')
        tmp.write_text(json.dumps(table))
        tmp.replace(cache)
    except OSError as e:
        if verbose:
            print(f'layout: cache write failed: {e}', file=sys.stderr)
    return table


def _tsize(table, ti, warn, seen=None):
    """Size in bytes of PDB type index ti, or None if unresolvable."""
    seen = seen or frozenset()
    if ti in seen:
        return None
    rec = table['types'].get(str(ti))
    if rec is None:
        # basic types appear inline as `0x0074 (int)` and have no record
        return None
    seen = seen | {ti}
    k = rec['kind']
    if k == 'pointer':
        return 4
    if k == 'array':
        return rec['size']
    if k == 'modifier' or k == 'typedef':
        return None if rec.get('referent') is None else \
            _tsize(table, rec['referent'], warn, seen)
    if k == 'enum':
        return rec.get('underlying', 4)
    if k == 'class':
        if rec['sizeof']:
            return rec['sizeof']
        # forward ref: resolve by name to the full definition's record
        full = table['classes'].get(rec.get('name'))
        if full is not None and full != ti:
            frec = table['types'].get(str(full))
            if frec and frec.get('sizeof'):
                return frec['sizeof']
        return None
    if k == 'basic':
        return rec.get('size')
    return None


def _fieldlist_chain(table, ti):
    """All field-list records reachable from ti via LF_INDEX continuations."""
    out, seen = [], set()
    while ti is not None and ti not in seen:
        seen.add(ti)
        rec = table['types'].get(str(ti))
        if not rec or rec.get('kind') != 'fieldlist':
            break
        out.append(rec)
        ti = rec.get('cont')
    return out


def _our_flat(table, cls, warn, seen=None, base_off=0):
    """[(member, our_off, size)] for cls, inherited members included."""
    if seen and cls in seen:
        warn(f'{cls}: inheritance cycle')
        return []
    seen = (seen or frozenset()) | {cls}
    ti = table['classes'].get(cls)
    if ti is None:
        warn(f'{cls}: class not found in PDB')
        return []
    rec = table['types'][str(ti)]
    out = []
    for fl in _fieldlist_chain(table, rec.get('fieldlist')):
        for off, bti in fl['bclasses']:
            brec = table['types'].get(str(bti), {})
            bname = brec.get('name') or f'<type 0x{bti:X}>'
            out += _our_flat(table, bname, warn, seen, base_off + off)
        for off, mti, name in fl['members']:
            size = _tsize(table, mti, warn)
            if size is None:
                warn(f'{cls}.{name}: unresolved member type 0x{mti:X}')
            out.append((name, base_off + off, size))
    return out


def our_offsets(pdb_path, cls, verbose=False):
    """{member: (our_offset, size)} for cls from the build's PDB."""
    warn = lambda msg: print(f'our_offsets: {msg}', file=sys.stderr)
    table = load_pdb_types(pdb_path, verbose=verbose)
    return {name: (off, size)
            for name, off, size in _our_flat(table, cls, warn)}


# ------------------------------------------------------------------- join

def header_index(header_dir):
    """{class name: parsed record} for every class declared under header_dir.

    Each header file is parsed once; a class declared in several files keeps
    its first (declaration-order) occurrence.
    """
    idx = {}
    for p in sorted(Path(header_dir).glob('*.h')):
        for name, rec in parse_header_classes(p.read_text(errors='replace')).items():
            idx.setdefault(name, rec)
    return idx


def field_map(cls, header_dir, pdb_path, verbose=False):
    """{exe_offset: our_offset} (byte-granular per member) for cls.

    Divergent offsets not covered by a shared member pass through unchanged
    in run_case and surface as state diffs.  exe_only/ours_only list members
    found on just one side.
    """
    warns = []

    def warn(msg):
        warns.append(msg)
        if verbose:
            print(f'field_map: {msg}', file=sys.stderr)

    texts = header_index(header_dir)
    exe = {n: (o, s) for n, o, s in _exe_flat(texts, cls, warn)}
    table = load_pdb_types(pdb_path, verbose=verbose)
    ours = {n: (o, s) for n, o, s in _our_flat(table, cls, warn)}
    m = {}
    for name, (eo, es) in exe.items():
        if name not in ours:
            continue
        oo, os_ = ours[name]
        if es is None or not os_:
            warn(f'{cls}.{name}: unknown size on one side '
                 f'(exe={es}, ours={os_}); skipped')
            continue
        for i in range(min(es, os_)):
            m[eo + i] = oo + i
    exe_only = sorted(set(exe) - set(ours))
    ours_only = sorted(set(ours) - set(exe))
    return FieldMap(m, exe_only, ours_only)
