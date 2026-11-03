"""Decide open wave rows by measurement: a purity check, oracle case drafts, and
a reader for the oracle's report.

A function that is pure (arithmetic on its arguments only) can be decided by
calling the original exe's function and ours over swept inputs and comparing
the results. is_pure reads the evidence file's Decompiled (Ghidra) section;
draft_case and write_cases turn a pure function into an oracle case in the
shape of oracle/cases/rand.cpp; probe_verdicts reads what `oracle.py run`
reported for those cases. Nothing here runs the oracle, the game or any exe.

Some rows are decided by tracing instead: hook_table builds one Frida hook table
per target from the rows' signatures and our build's .map, frida_run drives
tools/frida/probe.py (its subprocess injectable; nothing here launches a game
on its own), and frida_verdicts compares the two traces with trace_diff.

run_oracle builds and runs a batch's drafted cases (subprocesses injectable);
calibrate measures both paths over rows whose verdicts were already ruled, and
`truth.py calibrate` writes docs/wso/truth-calibration.json for the driver.

Python 3 standard library only; run under `python3 -I`.
"""
import glob
import hashlib
import json
import os
import random
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES_DIR = os.path.join(ROOT, 'oracle', 'cases')
FRIDA_DIR = os.path.join(ROOT, 'tools', 'frida')
sys.path.insert(0, FRIDA_DIR)
import trace_diff  # noqa: E402
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ghidra'))
import labels  # noqa: E402  the `// 0xADDR` markers of src, for our_names

SECTION = '## Decompiled (Ghidra)'

KEYWORDS = frozenset({
    'return', 'sizeof', 'if', 'while', 'for', 'switch', 'else', 'do', 'case',
    'default', 'break', 'continue', 'goto', 'const', 'static', 'inline',
    'volatile', 'register', 'signed', 'unsigned', 'true', 'false', 'NULL',
    'typedef'})
QUALIFIERS = frozenset({'const', 'static', 'volatile', 'register', 'signed', 'unsigned'})
# C and Ghidra type names (a cast or a local's declaration).
TYPE_NAMES = frozenset({
    'void', 'bool', 'char', 'short', 'int', 'long', 'float', 'double',
    'uchar', 'ushort', 'uint', 'ulong', 'longlong', 'ulonglong', 'byte',
    'sbyte', 'word', 'dword', 'qword', 'size_t', 'intptr_t', 'uintptr_t',
    'wchar_t', 'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t',
    'uint16_t', 'uint32_t', 'uint64_t',
    # Win32 integer typedefs (no HANDLE or other pointer types).
    'DWORD', 'BOOL', 'UINT', 'WORD', 'BYTE', 'LONG', 'ULONG', 'SHORT',
    'USHORT', 'CHAR', 'UCHAR', 'INT'})
TYPE_PATTERN = re.compile(r'^(?:(?:u?int|undefined|float|code)\d*)$')
# Ghidra pseudo-operations: pure arithmetic helpers, not calls.
PSEUDO_OPS = re.compile(
    r'^(?:CONCAT|SUB|ZEXT|SEXT|SBORROW|SCARRY|CARRY|PIECE|MULT)\d*$'
    r'|^(?:POPCOUNT|LZCOUNT|abs|ROL\d*|ROR\d*|ABS|FLOAT2INT|INT2FLOAT|NAN'
    r'|TRUNC|CEIL|FLOOR|ROUND|SQRT)$')
IDENT = re.compile(r'[A-Za-z_]\w*(?:::[A-Za-z_~]\w*)*')
NUMBER = re.compile(r'\b\d[\w.]*')


def _is_type(word):
    return word in TYPE_NAMES or bool(TYPE_PATTERN.match(word))


def _section_code(text):
    """The body of the Decompiled (Ghidra) section as bare code, or None."""
    start = text.find(SECTION)
    if start < 0:
        return None
    rest = text[start + len(SECTION):]
    nxt = re.search(r'^## ', rest, re.M)
    if nxt:
        rest = rest[:nxt.start()]
    lines = [ln for ln in rest.split('\n') if not ln.strip().startswith('```')]
    return '\n'.join(lines)


def _is_cast(inner):
    """Whether the text between parentheses is a bare type (a cast)."""
    words = re.findall(r'[A-Za-z_]\w*|\*', inner)
    return bool(words) and all(w == '*' or w in KEYWORDS or _is_type(w) for w in words) \
        and any(w != '*' for w in words)


def _is_deref(line):
    """Whether the line has a unary `*` (a pointer read or write), including
    one right after a (type) cast."""
    for i, ch in enumerate(line):
        if ch != '*':
            continue
        before = line[:i].rstrip()
        if not before:
            return True
        last = before[-1]
        if last == ')':
            depth, j = 0, len(before) - 1
            while j >= 0:
                depth += (before[j] == ')') - (before[j] == '(')
                if depth == 0:
                    break
                j -= 1
            if j >= 0 and re.search(r'\bsizeof\s*$', before[:j]):
                continue  # sizeof(T) * x is a multiplication, not a cast and a read
            if j >= 0 and _is_cast(before[j + 1:-1]):
                return True
            continue
        if last == ']' or last.isalnum() or last == '_':
            word = re.search(r'[A-Za-z_]\w*$', before)
            if word and word.group(0) in ('return', 'else', 'case', 'do'):
                return True
            continue
        return True
    return False


def _signature_params(signature):
    names = set()
    start, end = signature.find('('), signature.rfind(')')
    if start < 0 or end < start:
        return names
    for part in signature[start + 1:end].split(','):
        words = re.findall(r'[A-Za-z_]\w*', part)
        if words and words != ['void']:
            names.add(words[-1])
    return names


def _body_locals(body):
    """Names declared in the body: statements that start with type words."""
    names = set()
    for stmt in re.split(r'[;{}]', body):
        stmt = stmt.strip()
        if not stmt or not (stmt[0].isalpha() or stmt[0] == '_'):
            continue  # a cast or expression, not a declaration
        if '(' in stmt:
            continue  # a call or cast in it: its commas and names are not declarators
        words = re.findall(r'[A-Za-z_]\w*|\*|,|=', stmt)
        i = 0
        while i < len(words) and (words[i] in QUALIFIERS or _is_type(words[i])):
            i += 1
        if i == 0 or i >= len(words) or all(w in QUALIFIERS for w in words[:i]) \
                and not any(w in ('signed', 'unsigned') for w in words[:i]):
            continue
        for piece in re.split(r',', ' '.join(words[i:])):
            piece = piece.split('=', 1)[0]  # an initializer's names are not declared
            m = re.match(r'^[\s*]*([A-Za-z_]\w*)', piece)
            if m:
                names.add(m.group(1))
    return names


def _offence(line, allowed, pure_names):
    """The first reason this body line is impure, or None."""
    if re.fullmatch(r'LAB_\w+:', line):
        return None
    line = re.sub(r'\bgoto\s+LAB_\w+\s*;', ';', line)
    m = re.search(r'DAT_[0-9a-fA-F]+', line)
    if m:
        return 'global %s' % m.group(0)
    m = re.search(r'\b(?:in|unaff|extraout)_\w+', line)
    if m:
        return 'register state %s' % m.group(0)
    if re.search(r'\bthis\b', line):
        return 'this member access'
    if '->' in line:
        return 'member access through ->'
    if _is_deref(line):
        return 'pointer dereference'
    if re.search(r'[\w)]\s*\[', line):
        return 'indexed memory access'
    text = NUMBER.sub(' ', line)
    for m in IDENT.finditer(text):
        name = m.group(0)
        if name in allowed or name in KEYWORDS or name in pure_names \
                or _is_type(name) or PSEUDO_OPS.match(name):
            continue
        if re.match(r'\s*\(', text[m.end():]):
            return 'call to %s' % name
        return 'identifier %s is not a parameter, local or known type' % name
    return None


def is_pure(evidence_text, pure_names=frozenset()):
    """(pure, reason) for an evidence file's decompiled function.

    An allowlist: every identifier in the body must be a parameter from the
    signature, a local declared in the body, a C keyword or type name
    (Ghidra's included), a Ghidra pseudo-op (CONCAT*, SUB*, ZEXT*, ...), or in
    pure_names. Anything else (a global, a class static, a call) is impure,
    as is any `*` dereference (also after a cast), `->`, this, and in_/
    unaff_/extraout_ register state. The signature is not read as a call. The
    reason names the first offending line."""
    code = _section_code(evidence_text)
    if code is None:
        return False, 'no Decompiled (Ghidra) section'
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    code = re.sub(r'//[^\n]*', '', code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    brace = code.find('{')
    if brace < 0:
        return False, 'no function body in the Decompiled (Ghidra) section'
    body = code[brace + 1:]
    allowed = _signature_params(code[:brace]) | _body_locals(body)
    for line in body.split('\n'):
        stripped = line.strip()
        if not stripped:
            continue
        why = _offence(stripped, allowed, pure_names)
        if why:
            return False, '%s in: %s' % (why, stripped)
    return True, 'arithmetic on its arguments only'


# ---- case drafts ------------------------------------------------------------

# Ghidra and C spellings of integer types -> (C++ spelling, bits, signed).
INT_TYPES = {
    'int': ('int', 32, True), 'uint': ('unsigned int', 32, False),
    'unsigned int': ('unsigned int', 32, False), 'long': ('long', 32, True),
    'ulong': ('unsigned long', 32, False), 'unsigned long': ('unsigned long', 32, False),
    'short': ('short', 16, True), 'ushort': ('unsigned short', 16, False),
    'unsigned short': ('unsigned short', 16, False),
    'char': ('char', 8, True), 'uchar': ('unsigned char', 8, False),
    'unsigned char': ('unsigned char', 8, False), 'byte': ('unsigned char', 8, False),
    'bool': ('bool', 8, False), 'undefined4': ('unsigned int', 32, False),
    'undefined2': ('unsigned short', 16, False), 'undefined1': ('unsigned char', 8, False),
    'int32_t': ('int', 32, True), 'uint32_t': ('unsigned int', 32, False),
}
MAX_PARAMS = 4
CONVENTIONS = {'__cdecl': '__cdecl', '__stdcall': '__stdcall'}
SIGNATURE = re.compile(r'^\s*(?P<head>.*?)\s*(?P<name>[A-Za-z_~][\w:~]*)\s*\((?P<params>.*)\)\s*;?\s*$')


def _skip(address, name, reason):
    return '// skipped %s (%s): %s\n' % (address, name, reason)


def _norm_type(text):
    return re.sub(r'\s+', ' ', text.replace('const ', '')).strip()


def _parse_signature(signature):
    """(convention, return type, [(type, name)]) or a reason string."""
    m = SIGNATURE.match(signature)
    if not m:
        return 'the signature does not parse'
    head = m.group('head')
    conv = '__cdecl'
    for key in list(CONVENTIONS) + ['__thiscall', '__fastcall']:
        if key in head:
            conv = key
            head = head.replace(key, ' ')
    if conv in ('__thiscall', '__fastcall'):
        return 'member or fastcall convention (%s) needs a this/register setup' % conv
    ret = _norm_type(head)
    if ret not in INT_TYPES:
        return 'return type %r has no honest comparison here (integer returns only)' % ret
    params = []
    raw = m.group('params').strip()
    if raw and raw != 'void':
        for i, part in enumerate(raw.split(',')):
            part = part.strip()
            if not part or '*' in part or '&' in part or '...' in part:
                return 'parameter %r is not integer-like (pointers need a type-appropriate table)' % part
            pm = re.match(r'^(.*?)(?:\s+([A-Za-z_]\w*))?$', part)
            ptype, pname = _norm_type(pm.group(1)), pm.group(2)
            if ptype not in INT_TYPES:
                # "uint" alone parses as type uint with no name; "foo bar"
                # with an unknown type is skipped.
                if part in INT_TYPES:
                    ptype, pname = part, None
                else:
                    return 'parameter type %r is not integer-like (no sweep table)' % ptype
            params.append((ptype, pname or 'p%d' % i))
    if len(params) > MAX_PARAMS:
        return 'more than %d parameters: the sweep product is too large' % MAX_PARAMS
    return conv, ret, params


def draft_case(address, name, signature, batch_id, thiscall=False, header=None):
    """A C++ oracle case for one pure function, in the shape of
    oracle/cases/rand.cpp, or a one-line skip comment naming the reason.

    Each integer parameter is swept over {min, -1, 0, 1, 7, max} of its type
    (every combination) and the return values are compared. Other parameter
    types, non-integer returns and member calls (a non-static Class::Method
    needs __thiscall and a this: pass thiscall=True) are skipped."""
    addr = address[2:] if address.lower().startswith('0x') else address
    addr = addr.upper()
    if not re.match(r'^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$', name or ''):
        return _skip(address, name, 'name is not a callable C++ identifier')
    if re.match(r'^(?:FUN_|thunk_)', name.rsplit('::', 1)[-1]) and not header:
        return _skip(address, name, 'an unnamed Ghidra function: ours has no knowable name (give the row a header)')
    if '::' not in name and not header:
        return _skip(address, name, 'no Class:: in the name and no header: no header for ours is knowable')
    if thiscall:
        return _skip(address, name, 'non-static member: __thiscall needs a this the draft cannot build')
    parsed = _parse_signature(signature)
    if isinstance(parsed, str):
        return _skip(address, name, parsed)
    conv, ret, params = parsed
    cret = INT_TYPES[ret][0]
    ptypes = ', '.join('%s %s' % (INT_TYPES[t][0], n) for t, n in params)
    sig_types = ', '.join(INT_TYPES[t][0] for t, _ in params)
    args = ', '.join(n for _, n in params)
    fn = 'Case_%s' % addr
    out = []
    out.append('// %s at 0x%s, the original\'s against ours over swept integer arguments.' % (name, addr))
    out.append('// Drafted by tools/truth.py for batch %s; each argument takes {min, -1, 0, 1, 7, max}.' % batch_id)
    out.append('using Fn_%s = %s(%s*)(%s);' % (addr, cret, conv, sig_types))
    out.append('constexpr uintptr_t kOriginal_%s = 0x%s;' % (addr, addr))
    out.append('')
    out.append('%s Original_%s(%s)' % (cret, addr, ptypes))
    out.append('{')
    out.append('    return reinterpret_cast<Fn_%s>(kOriginal_%s)(%s);' % (addr, addr, args))
    out.append('}')
    out.append('')
    out.append('void %s(CaseContext& ctx)' % fn)
    out.append('{')
    indent = '    '
    for t, n in params:
        ct = INT_TYPES[t][0]
        out.append('%sfor (const %s %s : Sweep<%s>()) {' % (indent, ct, n, ct))
        indent += '    '
    shown = ' + ", " + '.join('std::to_string(%s)' % n for _, n in params) or '""'
    label = '"%s(" + %s + ")"' % (name, shown) if params else '"%s()"' % name
    out.append('%sctx.SetInputs(%s);' % (indent, label))
    out.append('%s%s original = 0;' % (indent, cret))
    out.append('%sctx.CallOriginal([&] { original = Original_%s(%s); });' % (indent, addr, args))
    out.append('%sconst %s ours = %s(%s);' % (indent, cret, name, args))
    out.append('%sctx.CountInputs(1);' % indent)
    out.append('%soracle::Compare(ctx, original, ours);' % indent)
    for _ in params:
        indent = indent[:-4]
        out.append('%s}' % indent)
    out.append('}')
    out.append('')
    out.append('ORACLE_CASE("wave/%s/%s", %s);' % (batch_id, addr, fn))
    return '\n'.join(out) + '\n'


PRELUDE = '''// Drafted by tools/truth.py for wave batch %(batch)s: pure functions swept over
// integer arguments, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>
// (compat's win_types.h defines the same names): it reaches the original only
// through CallOriginal and the addresses below.
%(includes)s
#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

template <typename T>
std::vector<T> Sweep()
{
    return { std::numeric_limits<T>::min(), static_cast<T>(-1), static_cast<T>(0), static_cast<T>(1),
        static_cast<T>(7), std::numeric_limits<T>::max() };
}

'''


def _split_case(src):
    """(body inside the namespace, registration line) of one drafted case."""
    body, _, reg = src.rpartition('ORACLE_CASE(')
    return body.rstrip() + '\n', 'ORACLE_CASE(' + reg


def write_cases(batch_id, rows, cases_dir=CASES_DIR):
    """Write wave-<batch>.cpp into cases_dir from rows (dicts with address,
    name, signature and optionally thiscall) and return its path. A row the
    draft cannot call honestly becomes a skip comment naming the reason.
    Adding the file to the oracle target is a separate step (the case files
    are listed explicitly in oracle/CMakeLists.txt)."""
    drafts = []
    skips = []
    includes = set()
    for row in rows:
        try:
            src = draft_case(row['address'], row['name'], row['signature'], batch_id,
                             row.get('thiscall', False), row.get('header'))
        except Exception as e:  # one bad row must not break the wave file
            src = _skip(row.get('address', '?'), row.get('name', '?'),
                        'the row could not be drafted (%s: %s)' % (type(e).__name__, e))
        if src.startswith('// skipped'):
            skips.append(src)
            continue
        drafts.append(_split_case(src))
        if row.get('header'):
            includes.add('#include "%s"' % row['header'])
        else:
            includes.add('#include "%s.h"' % row['name'].rsplit('::', 1)[0].split('::')[-1])
    text = PRELUDE % {'batch': batch_id, 'includes': '\n'.join(sorted(includes))}
    text += '\n'.join(body for body, _ in drafts)
    text += '\n} // namespace\n\n'
    text += ''.join(reg for _, reg in drafts)
    if skips:
        text += '\n' + ''.join(skips)
    os.makedirs(cases_dir, exist_ok=True)
    path = os.path.join(cases_dir, 'wave-%s.cpp' % batch_id)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return path


# ---- the oracle's report ------------------------------------------------------

WAVE_CASE = re.compile(r'^wave/([^/]+)/([0-9A-Fa-f]+)$')


def _status(case):
    """(status, detail) of one report case. The real report names a case by
    case['case'] (oracle/runner/report.cpp); tools/oracle.py case_ok is the
    pass test for a declared-pass case, kept in step here (importing oracle.py
    would pull in its whole toolchain)."""
    if case.get('crashed'):
        crash = case.get('crash') or {}
        return 'crash', '%s (%s)' % (crash.get('kind') or 'crashed', crash.get('inputs', ''))
    if case.get('failure_count'):
        return 'fail', 'failure_count %d' % case['failure_count']
    if not case.get('iterations'):
        return 'pending', 'ran no input' + (' (%s)' % case['pending'] if case.get('pending') else '')
    return 'pass', '%d inputs' % case['iterations']


def probe_verdicts(report_json, batch_id=None):
    """{'0x<ADDR>': (status, detail)} for the wave/<batch>/<ADDR> cases of an
    oracle.py run report (a JSON string or the parsed dict), the keys in the
    disagreements.json format. status is 'pass', 'fail' (ran and disagreed),
    'crash' (crashed: inputs outside the domain, say a divide by zero) or
    'pending' (ran nothing). With batch_id only that batch's cases count;
    without it, when two batches probe one address the last in report order
    wins. Other cases are not probes and are ignored."""
    report = json.loads(report_json) if isinstance(report_json, (str, bytes)) else report_json
    verdicts = {}
    for case in report.get('cases', []):
        m = WAVE_CASE.match(case.get('case', ''))
        if m and (batch_id is None or m.group(1) == batch_id):
            verdicts['0x' + m.group(2).upper()] = _status(case)
    return verdicts


# ---- hook tables and the batched Frida run -------------------------------------

HOOK_MAX = 200
FRIDA_TYPES = {
    'int': 's32', 'long': 's32', 'int32_t': 's32', 'uint': 'u32', 'ulong': 'u32',
    'uint32_t': 'u32', 'dword': 'u32', 'undefined4': 'u32', 'short': 's16',
    'int16_t': 's16', 'ushort': 'u16', 'uint16_t': 'u16', 'word': 'u16',
    'undefined2': 'u16', 'char': 'u8', 'uchar': 'u8', 'byte': 'u8', 'bool': 'u8',
    'undefined1': 'u8', 'uint8_t': 'u8',
}
# Types the hook schema cannot log honestly: 8-byte values take two stack
# slots (args) or edx:eax (returns), floats come back in st0 or as raw bits.
WIDE_TYPES = frozenset({
    'double', 'float', 'longlong', 'ulonglong', 'undefined8', 'int64_t', 'uint64_t',
    'qword', '__int64', 'int64', 'uint64', 'float10', 'undefined6'})
FRIDA_CONVS = {'__thiscall': 'thiscall', '__cdecl': 'cdecl', '__stdcall': 'stdcall'}
MAP_LOAD = re.compile(r'Preferred load address is\s+([0-9A-Fa-f]+)')
MAP_PUBLIC = re.compile(r'^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8,16})\b')
LOADED_NAME = 'CScreenWorld::EngineActivated'   # orig.py's loaded signal (0x6869C0 in the original)


def hook_name(address):
    """The hook (and trace tag) name of a row: the same on both targets."""
    addr = address[2:] if address.lower().startswith('0x') else address
    return 'fn_' + addr.upper()


def _frida_type(text):
    """(frida type, None) or (None, reason)."""
    text = re.sub(r'\bconst\b', '', text).strip()
    if text == 'BOOL':
        return 's32', None  # the Win32 BOOL is a 4-byte int (lowercase bool is a byte)
    if '*' in text or '&' in text:
        return 'ptr', None
    words = [w.lower() for w in text.split()]
    if any(w in WIDE_TYPES for w in words):
        return None, '%s is 8 bytes or floating point (the hook schema logs 32-bit values)' % text
    if words[:1] in (['unsigned'], ['signed']) and len(words) > 1:
        base = FRIDA_TYPES.get(words[1], 'u32')
        return (base if words[0] == 'signed' else base.replace('s', 'u')), None
    return FRIDA_TYPES.get(' '.join(words), 'u32'), None


def decorated_convention(decorated):
    """The calling convention a decorated MSVC name encodes, or None: a member
    function code [AEIMQU] (private, protected or public, plain or virtual),
    then the `this` qualifier [ABCD] (none, const, volatile, const volatile),
    then E, is thiscall (regex [AEIMQU][ABCD]E); [CKSY]A is cdecl, [CKSY]G
    stdcall and [CKSY]I fastcall (skipped by the hooks); a plain _name is
    cdecl and _name@N stdcall. Special members (??0 constructor, ??1
    destructor, ??_G and the like) carry the same tail."""
    m = re.match(r'^\?(?:\?[0-9_A-Z]?)?[^@?]+(?:@[^@]+)*?@@(.*)$', decorated or '')
    if m:
        tail = m.group(1)
        if re.match(r'[AEIMQU][ABCD]E', tail):
            return '__thiscall'
        for code, conv in (('A', '__cdecl'), ('G', '__stdcall'), ('I', '__fastcall')):
            if re.match(r'[CKSY]' + code, tail):
                return conv
        return None
    if re.match(r'^_\w+@\d+$', decorated or ''):
        return '__stdcall'
    if re.match(r'^_\w+$', decorated or ''):
        return '__cdecl'
    return None


def _hook_signature(signature, decorated=None):
    """(conv, args, ret or None) of a C signature, or a reason string. The
    convention is the signature's; real evidence signatures carry none, so it
    then comes from the decorated name in our map (decorated_convention). The
    implicit this is dropped for thiscall; void returns log no value. A
    convention neither source gives, a non-(cdecl, stdcall, thiscall) one, and
    any 8-byte or floating-point argument or return give a reason instead."""
    m = SIGNATURE.match(signature or '')
    if not m:
        return 'the signature does not parse'
    head = m.group('head')
    convs = re.findall(r'__\w+', head)
    others = [c for c in convs if c not in FRIDA_CONVS]
    if others:
        return 'calling convention %s is not hookable (cdecl, stdcall and thiscall only)' % others[0]
    if not convs:
        named = decorated_convention(decorated)
        if named is None:
            return 'no calling convention in the signature or in our map\'s decorated name'
        if named not in FRIDA_CONVS:
            return 'calling convention %s is not hookable (cdecl, stdcall and thiscall only)' % named
        convs = [named]
    conv = FRIDA_CONVS[convs[0]]
    for key in FRIDA_CONVS:
        head = head.replace(key, ' ')
    head = re.sub(r'\s+', ' ', head).strip()
    ret = None
    if head not in ('void', ''):
        ret, why = _frida_type(head)
        if why:
            return 'return type: ' + why
    raw = m.group('params').strip()
    args = []
    if raw and raw != 'void':
        for i, part in enumerate(raw.split(',')):
            part = part.strip()
            if part == '...':
                return 'a variadic signature cannot be hooked by position'
            pm = re.match(r'^(.*?)(?:\s*\b([A-Za-z_]\w*))?$', part)
            ptype = pm.group(1).strip()
            if not ptype or (ptype.lower() not in FRIDA_TYPES and '*' not in ptype
                             and ' ' not in ptype and ptype.lower() not in WIDE_TYPES):
                ptype = part  # a lone type name such as "uint"
            if conv == 'thiscall' and i == 0 and pm.group(2) == 'this':
                continue
            t, why = _frida_type(ptype)
            if why:
                return 'parameter: ' + why
            args.append(t)
    return conv, args, ret


def parse_map(text):
    """{decorated name: rva} from an MSVC link map: the Rva+Base column of the
    publics minus the preferred load address."""
    base = 0x400000
    m = MAP_LOAD.search(text)
    if m:
        base = int(m.group(1), 16)
    out = {}
    for line in text.split('\n'):
        m = MAP_PUBLIC.match(line)
        if m:
            out.setdefault(m.group(1), int(m.group(2), 16) - base)
    return out


def _map_matches(name, publics):
    """{rva: decorated name} of every public that is Class::Method (decorated
    ?Method@Class@@...), Class::Class (??0Class@@...), Class::~Class
    (??1Class@@...; the scalar deleting destructor ??_G is another symbol) or
    a plain name."""
    parts = name.split('::')
    if len(parts) >= 2 and parts[-1] == '~' + parts[-2]:
        prefix = '??1' + '@'.join(reversed(parts[:-1])) + '@@'      # destructor
    elif len(parts) >= 2 and parts[-1] == parts[-2]:
        prefix = '??0' + '@'.join(reversed(parts[:-1])) + '@@'      # constructor
    else:
        prefix = '?' + '@'.join(reversed(parts)) + '@@'
    found = {}
    for decorated, rva in publics.items():
        if decorated.startswith(prefix) or (len(parts) == 1 and decorated in (name, '_' + name)):
            found.setdefault(rva, decorated)
    return found


PROTO_TYPES = {
    'byte': 'u8', 'boolean': 'u8', 'bool': 'u8', 'uchar': 'u8', 'char': 'u8', 'uint8_t': 'u8', 'u8': 'u8',
    'short': 's16', 'int16_t': 's16', 'word': 'u16', 'ushort': 'u16', 'uint16_t': 'u16',
    'int': 's32', 'long': 's32', 'int32_t': 's32',
    'dword': 'u32', 'uint': 'u32', 'ulong': 'u32', 'uint32_t': 'u32', 'size_t': 'u32',
    # MFC/Win32 typedefs (x86: every pointer-sized integer is 32 bits)
    'resid': 'u32', 'colorref': 'u32', 'wparam': 'u32', 'uint_ptr': 'u32', 'dword_ptr': 'u32',
    'lparam': 's32', 'lresult': 's32', 'int_ptr': 's32', 'long_ptr': 's32',
}
_CORE_PROTO = frozenset(PROTO_TYPES) - frozenset(
    'resid colorref wparam uint_ptr dword_ptr lparam lresult int_ptr long_ptr'.split())
for _name in ('POSITION HANDLE HWND HDC HINSTANCE HMODULE HBITMAP HPALETTE HRGN HFONT HGDIOBJ HGLOBAL '
              'LPVOID LPCVOID LPSTR LPCSTR LPBYTE LPDWORD LPWORD PVOID PBYTE RESREF FARPROC PROC').split():
    PROTO_TYPES[_name.lower()] = 'ptr'   # RESREF: a BYTE array parameter decays to a pointer

_PROTO_MODIFIERS = frozenset({'const', 'volatile', 'struct', 'class', 'enum', 'virtual', 'static',
                              'inline', 'explicit', 'extern', 'friend', 'constexpr'})
_PROTO_SIGN = frozenset({'unsigned', 'signed'})


def _proto_type(text):
    """The Frida type of a C++ type as our headers spell it (references and
    pointers are ptr, `void` is 'void'), or None for one the hooks cannot log
    honestly from the spelling alone (a class by value, an enum, a float)."""
    if '*' in text or '&' in text:
        return 'ptr'
    words = [w for w in re.split(r'\s+', text.strip()) if w and w not in _PROTO_MODIFIERS]
    if words == ['void']:
        return 'void'
    if words == ['BOOL']:   # the Win32 int; checked before lowercasing, where it would read as bool (a byte)
        return 's32'
    sign = [w for w in words if w in _PROTO_SIGN]
    base = [w.lower() for w in words if w not in _PROTO_SIGN]
    if any(w in WIDE_TYPES for w in base):
        return None
    base = [w for w in base if w != 'int'] or ['int']
    if base == ['long', 'long']:
        return None
    key = base[-1]
    t = PROTO_TYPES.get(key)
    if t is None:
        return None
    if sign:
        t = ('u' if sign[0] == 'unsigned' else 's') + t[1:]
    return t


def _strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', ' ', text)
    return re.sub(r'(?m)^\s*#[^\n]*', ' ', text)


def _statements(body):
    """The top-level statements of a C++ scope: split at ';', with nested
    {...} blocks (inline bodies, nested types) skipped and ending a statement."""
    out, cur, depth = [], [], 0
    for ch in body:
        if ch == '{':
            if depth == 0:
                out.append(''.join(cur))
                cur = []
            depth += 1
        elif ch == '}':
            depth = max(0, depth - 1)
        elif depth == 0:
            if ch == ';':
                out.append(''.join(cur))
                cur = []
            else:
                cur.append(ch)
    out.append(''.join(cur))
    return [re.sub(r'^\s*(?:public|private|protected)\s*:', '', x).strip() for x in out if x.strip()]


def _class_body(text, cls):
    m = re.search(r'\b(?:class|struct)\s+%s\b[^;{]*\{' % re.escape(cls), text)
    if not m:
        return None
    depth, i = 1, m.end()
    while i < len(text) and depth:
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        i += 1
    return text[m.end():i - 1]


def _split_params(raw):
    parts, depth, cur = [], 0, []
    for ch in raw:
        if ch in '(<[':
            depth += 1
        elif ch in ')>]':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(''.join(cur))
            cur = []
        else:
            cur.append(ch)
    parts.append(''.join(cur))
    return [p.strip() for p in parts if p.strip()]


def _param_type(part):
    part = re.sub(r'=.*$', '', part, flags=re.S).strip()
    words = re.findall(r'[A-Za-z_]\w*|[*&]', part)
    # a typedef spelled last is the type only when nothing but modifiers precedes it (`const RESREF`)
    is_type = bool(words) and words[-1].lower() in _CORE_PROTO or (
        bool(words) and words[-1].lower() in PROTO_TYPES
        and all(w in _PROTO_MODIFIERS or w in _PROTO_SIGN for w in words[:-1]))
    if len(words) >= 2 and re.match(r'[A-Za-z_]', words[-1]) and not is_type \
            and words[-1] not in _PROTO_SIGN and words[-1] not in _PROTO_MODIFIERS \
            and words[-1] not in ('void',):
        words = words[:-1]
    return ' '.join(words)


def _header_prototypes(texts, name):
    """[(ret type text, [param type texts])] of every declaration of
    Class::Method (or a free function) named `name` in the header texts."""
    parts = name.split('::')
    found = []
    for text in texts:
        text = _strip_comments(text)
        if len(parts) == 1:
            scope = text
        else:
            scope = _class_body(text, parts[-2])
            if scope is None:
                continue
        for st in _statements(scope):
            m = re.search(r'(?<![\w~])%s\s*\((.*)\)\s*(?:const\b)?\s*(?:override\b)?\s*(?:=\s*0)?\s*$' % re.escape(parts[-1]),
                          st, flags=re.S)
            if not m or st.lstrip().startswith(('typedef', 'using', 'friend')):
                continue
            ret = re.sub(r'\b(?:__thiscall|__cdecl|__stdcall|__fastcall)\b', ' ', st[:m.start()]).strip()
            ret = ' '.join(w for w in ret.split() if w not in _PROTO_MODIFIERS - {'const'} or w == 'const')
            if not ret or '(' in ret or '=' in ret:
                continue  # a constructor, a destructor, a macro or a data member
            params = _split_params(m.group(1))
            if params == ['void']:
                params = []
            found.append((ret, [_param_type(p) for p in params]))
        if found:
            break
    return found


def _typed_from_headers(texts, name, ghidra_argc, conv, ghidra_args=None, ghidra_ret=None):
    """(conv, args, ret or None, slots, mixed) typed from our prototype, or
    None when no header declares it (unambiguously) or a type cannot be
    logged honestly. With several overloads, the ones with the evidence
    signature's argument count are kept, and the typing stands only when
    they all agree. A parameter (or return) whose spelling _proto_type does
    not know takes the Ghidra type of its slot (ghidra_args/ghidra_ret, as
    _hook_signature returns them) when the prototype has as many parameters
    as ghidra_args. slots is 'ours' or 'ghidra' per argument; mixed is True
    when any slot or the return came from Ghidra."""
    protos = _header_prototypes(texts, name)
    if len(protos) > 1 and ghidra_argc is not None:
        protos = [p for p in protos if len(p[1]) == ghidra_argc]
    typed = set()
    for ret, params in protos:
        r = _proto_type(ret)
        a = [_proto_type(p) for p in params]
        if 'void' in a:
            return None
        slots = ['ours'] * len(a)
        ret_ghidra = False
        if ghidra_args is not None and len(ghidra_args) == len(a):
            for i, t in enumerate(a):
                if t is None:
                    a[i], slots[i] = ghidra_args[i], 'ghidra'
        if None in a:
            return None
        if r is None:
            if ghidra_args is None or len(ghidra_args) != len(a) or ghidra_ret is None:
                return None
            r, ret_ghidra = ghidra_ret, True
        typed.add((tuple(a), None if r == 'void' else r, tuple(slots), ret_ghidra))
    if len(typed) != 1:
        return None
    args, ret, slots, ret_ghidra = next(iter(typed))
    return conv, list(args), ret, list(slots), ('ghidra' in slots or ret_ghidra)


def hook_table(rows, signatures, our_map, scenarios, headers=None, names=None):
    """The hook tables for one batch: {'original': {'hooks': [...]}, 'ours':
    {'hooks': [...]}, 'scenarios': [...], 'skipped': {addr: reason},
    'ours_loaded_rva': '0x..' or None}.

    rows are dicts with address and name; signatures maps address to a C
    signature; headers (header text or a list of them, optional) are our
    src/*.h: a function found there is typed from our prototype (args and
    return; typed_from[addr] is 'ours', or 'mixed' when a slot the header
    spells with an unknown type took Ghidra's), otherwise from the evidence
    signature ('ghidra'); typed_slots[addr] says per argument which; our_map
    is the build's .map text (or a {name: rva} dict). names ({int address:
    our name}, optional; our_names builds it) resolves a row to our current
    name at the row's address -- the address is the identity, and the row's
    recorded name may predate a rename or a class split -- with row['name']
    as the fallback; the resolved name is the one looked up in the map,
    typed from the headers and named in a skip reason.
    Each row gets one hook per target, named fn_<ADDR> so the traces line up: the
    original's by absolute addr, ours by rva. A row is listed in skipped, with
    no hook on either side, when its signature does not parse or has a
    convention or type the hooks cannot log, when our map has no entry, or
    when it has several overloads at different rvas (Ghidra's types cannot
    pick the decorated name, and hooking the wrong one would give a false
    equal). ours_loaded_rva is CScreenWorld::EngineActivated's rva in the map,
    for probe.py's --loaded-rva."""
    from_text = isinstance(our_map, str)
    publics = parse_map(our_map) if from_text else dict(our_map)
    texts = [headers] if isinstance(headers, str) else list(headers or [])
    names = names or {}
    original, ours, skipped, typed, slots = [], [], {}, {}, {}
    for row in rows:
        address = row['address']
        key = '0x' + address[2:].upper() if address.lower().startswith('0x') else '0x' + address.upper()
        name = names.get(int(address, 16), row['name'])
        if from_text:
            found = _map_matches(name, publics)
        else:
            found = {publics[name]: None} if name in publics else {}
        rvas = sorted(found)
        decorated = found[rvas[0]] if len(rvas) == 1 else None
        sig = _hook_signature(signatures.get(address) or signatures.get(key), decorated)
        typed_from = 'ghidra'
        slot_src = None
        if texts and len(rvas) == 1:
            named = decorated_convention(decorated)
            conv_name = None
            if not isinstance(sig, str):
                conv_name = sig[0]
            elif named in FRIDA_CONVS:
                conv_name = FRIDA_CONVS[named]
            if conv_name:
                argc = len(sig[1]) if not isinstance(sig, str) else None
                mine = _typed_from_headers(
                    texts, name, argc, conv_name,
                    sig[1] if not isinstance(sig, str) else None,
                    sig[2] if not isinstance(sig, str) else None)
                if mine:
                    sig, typed_from = mine[:3], 'mixed' if mine[4] else 'ours'
                    slot_src = mine[3]
        if isinstance(sig, str) and len(rvas) != 1:
            sig = None  # ambiguity is the more useful reason; reported below
        if isinstance(sig, str):
            skipped[key] = sig
            continue
        if not rvas:
            skipped[key] = 'no entry for %s in our map' % name
            continue
        if len(rvas) > 1:
            skipped[key] = 'ambiguous: %d overloads of %s in our map' % (len(rvas), name)
            continue
        typed[key] = typed_from
        conv, args, ret = sig
        slots[key] = slot_src if slot_src is not None else ['ghidra'] * len(args)
        hook = {'name': hook_name(key), 'conv': conv, 'args': args}
        if ret:
            hook['ret'] = ret
        hook['max'] = HOOK_MAX
        original.append(dict(hook, addr=key))
        ours.append(dict(hook, rva=hex(rvas[0])))
    loaded = sorted(_map_matches(LOADED_NAME, publics)) if from_text else (
        [publics[LOADED_NAME]] if LOADED_NAME in publics else [])
    return {'original': {'hooks': original}, 'ours': {'hooks': ours},
            'scenarios': list(scenarios), 'skipped': skipped, 'typed_from': typed, 'typed_slots': slots,
            'ours_loaded_rva': hex(loaded[0]) if len(loaded) == 1 else None}


def _read_events(path):
    events = []
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        row = json.loads(line)
                    except ValueError:
                        continue
                    if isinstance(row, dict):
                        events.append(row)
    except OSError:
        pass
    return events


SCENARIO_TIMEOUT = re.compile(r'^#\s*timeout:\s*(\d+)', re.M)
DEFAULT_SCENARIO_TIMEOUT = 180


def _scenario_timeout(root, scenario):
    """The scenario's `# timeout: S` header (scenarios/<name>.txt under root),
    else 180: the value probe.py itself would resolve."""
    try:
        with open(os.path.join(root, 'scenarios', scenario + '.txt'), encoding='utf-8',
                  errors='replace') as f:
            m = SCENARIO_TIMEOUT.search(f.read())
        if m:
            return int(m.group(1))
    except OSError:
        pass
    return DEFAULT_SCENARIO_TIMEOUT


def _read_status(path):
    """(RESULT: verdict word or None, {hook: total hits}, final) of a sidecar
    status file (`HITS <name> <n>` lines, `HITS-FINAL yes|no`, then `RESULT:
    <verdict> ...`); final is True when the totals are the exit-time message's
    (a file without the line is not final); (None, None, False) when there is
    no file."""
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            lines = f.read().splitlines()
    except OSError:
        return None, None, False
    verdict, hits, final = None, {}, False
    for line in lines:
        parts = line.split()
        if len(parts) == 3 and parts[0] == 'HITS' and parts[2].isdigit():
            hits[parts[1]] = int(parts[2])
        elif parts[:1] == ['HITS-FINAL']:
            final = parts[1:] == ['yes']
        elif parts[:1] == ['RESULT:'] and len(parts) > 1:
            verdict = parts[1]
    return verdict, hits, final


_ORACLE = None


def _oracle():
    """tools/oracle.py, loaded by path (tools/ is not on sys.path under -I):
    the replay tooling's dump comparison."""
    global _ORACLE
    if _ORACLE is None:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'truth_oracle', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'oracle.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _ORACLE = module
    return _ORACLE


SCHEMA_HINTS = {
    'original': ('the recorder.dll that ran is out of step with the schema (built before it '
                 'changed, or the tree under test predates a schema change: merge first into '
                 'it); rebuild it from the tree under test (tools/oracle.py build, or the '
                 'frida phase\'s build of the batch checkout) and run again'),
    'ours': ('our state dumper is out of step with the schema (or the tree under test predates '
             'a schema change: merge first into it); fix it and run again'),
}


NO_LOADED_RVA = 'ours cannot signal loaded'


def infrastructure_failure(traces):
    """'' or why a frida_run's traces are an infrastructure failure, not a
    verdict on the rows: any scenario that could not run for want of our
    loaded RVA (a build property), or every scenario failed with no status on
    either side or failed the same way on both (uninformative_reason)."""
    runs = [(sc, t) for sc, t in sorted(traces.items()) if isinstance(t, dict)]
    missing = [(sc, t) for sc, t in runs if NO_LOADED_RVA in str(t.get('failed') or '')]
    if missing:
        return 'a build property, not the rows: %s' % '; '.join(
            '%s: %s' % (sc, t['failed']) for sc, t in missing)
    if not runs or len(runs) != len(traces) or not all(
            t.get('failed') and (not any((t.get('result') or {}).values())
                                 or uninformative_reason(t)) for _sc, t in runs):
        return ''
    return ('every Frida scenario failed on infrastructure (no side produced a status or '
            'both sides failed alike): ' + '; '.join('%s: %s' % (sc, t['failed']) for sc, t in runs))


def _schema_failures(oracle, folder):
    """(invalid dump count, dump count, first 'file: error') of a replay
    folder under oracle.check_dumps and schema.json, the check replay record
    makes on every run (finish_replay); (0, 0, '') without the folder."""
    if not os.path.isdir(folder):
        return 0, 0, ''
    with open(oracle.REPLAY_SCHEMA, encoding='utf-8') as f:
        schema = json.load(f)
    dumps = oracle.check_dumps(folder, schema)
    bad = sorted((name, errors) for name, errors in dumps.items() if errors)
    return len(bad), len(dumps), ('%s: %s' % (bad[0][0], bad[0][1][0]) if bad else '')


def compare_dumps(orig_dir, ours_dir, script):
    """{'match': True | False | None, 'detail': text}: whether the state
    dumps both sides of one scenario wrote agree, with the replay tooling's
    own semantics end to end.  First the check `oracle.py replay record`
    makes on every run before anything compares it (finish_replay:
    oracle.check_dumps against oracle/replay/schema.json): a side with a dump
    the schema refuses gets no verdict (match None, 'invalid': {side: count},
    the detail names the side, the first refusal and the likely cause -- the
    b014/i001 batches ran a recorder.dll built before the schema grew
    world.enemy_creatures and party morale, which read as 4224 "differences"
    while our engine was right).  Then the compare `oracle.py replay compare`
    makes: oracle.compare_checkpoints over the scenario copy's checkpoint
    lines and oracle.compare_frames over the periodic dumps both folders
    hold, with the screens.json screens and the known differences; both skip
    oracle.TIMING_FIELDS and oracle.OURS_ONLY_FIELDS (field_differences).  A
    missing checkpoint dump is a difference; None also means nothing on a
    compared screen was there to compare (no verdict either way)."""
    oracle = _oracle()
    try:
        actions = oracle.load_actions(script)
    except (OSError, SystemExit, ValueError) as e:
        return {'match': None, 'detail': 'scenario copy %s unreadable: %s' % (script, e)}
    invalid, why = {}, []
    for side, folder, owner in (('original', orig_dir, "the original's"),
                                ('ours', ours_dir, 'our')):
        bad, total, first = _schema_failures(oracle, folder)
        if bad:
            invalid[side] = bad
            why.append('%s dumps fail oracle/replay/schema.json (%d of %d; first: %s): %s'
                       % (owner, bad, total, first, SCHEMA_HINTS[side]))
    if invalid:
        return {'match': None, 'detail': '; '.join(why), 'invalid': invalid}
    names = [a['name'] for a in actions if a.get('op') == 'checkpoint']
    screens, known = oracle.load_screens(), oracle.load_known()
    counts = {}
    lines, failures, _ = oracle.compare_checkpoints(orig_dir, ours_dir, names, screens, known, counts)
    frame_lines, frame_failures, _ = oracle.compare_frames(orig_dir, ours_dir, screens, known, counts)
    failures += frame_failures
    if failures:
        every = [l.strip() for l in lines + frame_lines]
        where = next((l for l in every if 'DIFFERENT' in l or 'MISSING' in l
                      or 'UNREADABLE' in l), '')
        field = next((l for l in every if l.startswith('DIFF ')), '')
        first = '; '.join(x for x in (where, field) if x)
        return {'match': False, 'detail': '%d difference(s)%s' % (
            failures, '; first: %s' % first if first else '')}
    if not counts.get('enabled') and not counts.get('frames'):
        return {'match': None, 'detail': 'no dump on a compared screen'}
    return {'match': True, 'detail': '%d checkpoints and %d periodic dumps match' % (
        counts.get('enabled', 0), counts.get('frames', 0))}


def _home_of(done, root, out):
    """(run folder as printed by probe.py, path of the trace file `out` in
    it; its status file is that path + '.status') of a finished probe
    process; (None, None) without a folder."""
    folder = None
    for line in (getattr(done, 'stdout', '') or '').splitlines():
        if line.startswith('probe: ') and line[7:].startswith('runs'):
            folder = line[7:].strip()
    return folder, (os.path.join(root, folder, 'replay', out) if folder else None)


def frida_run(batch_id, table, scenarios, run=subprocess.run, root=ROOT, loaded_rva=None,
              clicks=3, hit_min=1, timeout=None, attempt_reason=None, build_checkout=None,
              dumps=compare_dumps):
    """{scenario: {'orig': events, 'ours': events, 'orig_path': path or None,
    'ours_path': path or None, 'orig_exit': int, 'ours_exit': int,
    '<side>_retried': True when that side was launched a second time because
    its first run came home with no status (exit 2, no RESULT), 'final':
    {'orig': bool, 'ours': bool} (the totals are the exit-time ones),
    'result': {'orig': verdict word, 'ours': verdict word}, 'hits': {'orig':
    {hook: total}, 'ours': {...}} (None per side without a status file),
    'measured': bool, 'failed': reason or None, 'dumps': {'match', 'detail'} or
    None}}: for each scenario, both
    targets run through `probe.py run` (one sandbox each, one at a time) the
    way a hand run does: --clicks (the table's `clicks`, an int or a
    {scenario: n}, else the clicks argument), --hit-min (no --hit: any hook
    counts), --timeout (the argument, else the scenario's `# timeout:`
    header, else 180) and --new-attempt "driver frida <batch> <scenario>".
    The traces and `.status` files they leave under runs/<stamp>/replay/ are
    read back (paths are absolute; the trace_source hashes are of these
    files). A scenario is failed (not measured) when either probe exits
    non-zero (a side that proves nothing exits 2) or leaves no trace file;
    frida_verdicts leaves failed scenarios out and reports them. Both sides
    write checkpoint and periodic state dumps (ours under IWD2R_STATE_DUMP,
    the original under the recorder) into their replay folders: for a
    scenario that did not fail, dumps(orig replay folder, ours replay folder,
    the scenario copy) (default compare_dumps, the replay tooling's compare)
    says whether they agree; frida_verdicts measures nothing in a scenario
    whose dumps do not match, and the driver writes trace evidence only from
    one whose dumps match.

    Ours must pass --loaded-rva for a world scenario, else orig.py would
    hook the original's EngineActivated address inside iwd2r.exe: loaded_rva
    (or the table's ours_loaded_rva, read from the map) names
    CScreenWorld::EngineActivated in our build. With neither, every world
    scenario fails with that reason and nothing is run (infrastructure_failure
    calls it an infrastructure failure). A UI scenario runs --no-loaded on
    both sides: orig.py then installs no loaded hook at all. attempt_reason(scenario, 'orig'|'ours'), when given, replaces the
    default --new-attempt text. build_checkout (a WSL path) is the Windows
    checkout both sides run from (probe.py --build-checkout): ours its x86
    exe, the batch worktree's, so the bodies under test are the ones the
    map's RVAs name; the original its oracle build's injector and recorder,
    so the recorder that writes the original's dumps comes from the same tree
    as the dumper that writes ours and both follow one schema.json (the
    driver's checkout held a recorder.dll older than the schema: every dump
    guard of b014 and i001 failed on it). Without it probe.py runs this
    checkout's builds. `run` is injectable
    (the tests never launch anything); the real call is made by hand on the
    Windows machine under the one-launch rules."""
    import tempfile
    probe = os.path.join(FRIDA_DIR, 'probe.py')
    loaded_rva = loaded_rva or table.get('ours_loaded_rva')
    traces = {}
    with tempfile.TemporaryDirectory() as tmp:
        tables = {}
        for target in ('original', 'ours'):
            path = os.path.join(tmp, 'hooks-%s-%s.json' % (batch_id, target))
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                json.dump(table[target], f, indent=1)
            tables[target] = path
        for scenario in scenarios:
            if not loaded_rva and scenario not in FRIDA_UI_SCENARIOS:
                traces[scenario] = {'orig': [], 'ours': [], 'orig_path': None, 'ours_path': None,
                                    'orig_exit': None, 'ours_exit': None,
                                    'result': {'orig': None, 'ours': None},
                                    'hits': {'orig': None, 'ours': None}, 'measured': False,
                                    'failed': ('no %s in our map: %s (pass loaded_rva)'
                                               % (LOADED_NAME, NO_LOADED_RVA))}
                continue
            own = table.get('clicks', clicks)
            n_clicks = own.get(scenario, clicks) if isinstance(own, dict) else own
            limit = timeout or _scenario_timeout(root, scenario)
            result = {'failed': None, 'result': {}, 'hits': {}, 'dumps': None}
            homes = {}
            for target, key in (('original', 'orig'), ('ours', 'ours')):
                out = 'frida-%s-%s-%s.jsonl' % (batch_id, scenario, target)
                cmd = [sys.executable, '-I', probe, 'run', '--target', target,
                       '--probe', tables[target], '--scenario', scenario, '--out', out,
                       '--clicks', str(n_clicks), '--hit-min', str(hit_min),
                       '--timeout', str(limit),
                       '--new-attempt', (attempt_reason(scenario, key) if attempt_reason else
                                         'driver frida %s %s' % (batch_id, scenario))]
                if scenario in FRIDA_UI_SCENARIOS:
                    # a UI-only scenario never reaches the world screen: both
                    # sides drop the loaded observation (orig.py reads CLEAN
                    # when the process ends without a crash)
                    cmd += ['--no-loaded']
                elif target == 'ours':
                    cmd += ['--loaded-rva', loaded_rva]
                if build_checkout:
                    cmd += ['--build-checkout', build_checkout]
                done = run(cmd, capture_output=True, text=True, cwd=root)
                folder, out_path = _home_of(done, root, out)
                if getattr(done, 'returncode', 0) == 2 and folder and \
                        _read_status(out_path + '.status')[0] is None:
                    # a flake (no status came home): the side runs once more
                    i = cmd.index('--new-attempt') + 1
                    cmd[i] = (attempt_reason(scenario, key) + ' retry: no status'
                              if attempt_reason else
                              'driver frida %s %s retry: %s side came home with no status'
                              % (batch_id, scenario, target))
                    result[key + '_retried'] = True
                    done = run(cmd, capture_output=True, text=True, cwd=root)
                folder, path = _home_of(done, root, out)
                homes[key] = os.path.join(root, folder) if folder else None
                if path and not os.path.isfile(path):
                    path = None
                result[key + '_path'] = path
                result[key] = _read_events(path) if path else []
                verdict, hits, final = _read_status(os.path.join(root, folder, 'replay', out + '.status')
                                                    ) if folder else (None, None, False)
                result['result'][key] = verdict
                result['hits'][key] = hits
                result.setdefault('final', {})[key] = final
                rc = getattr(done, 'returncode', 0)
                result[key + '_exit'] = rc
                if result['failed'] is None and (rc != 0 or path is None):
                    result['failed'] = '%s side %s%s' % (
                        target, 'exited %s' % rc if rc != 0 else 'left no trace file',
                        ' (%s)' % verdict if verdict else '')
            result['measured'] = result['failed'] is None
            if result['measured'] and homes.get('orig') and homes.get('ours'):
                copy = os.path.join(homes['orig'], 'scenario.txt')
                if not os.path.isfile(copy):
                    copy = os.path.join(homes['ours'], 'scenario.txt')
                result['dumps'] = dumps(os.path.join(homes['orig'], 'replay'),
                                        os.path.join(homes['ours'], 'replay'), copy)
            traces[scenario] = result
    return traces


def _ptr_returns(events):
    """Return events with a pointer-looking `ret` (hooks.py logs ptr returns
    as '0x...' strings) reduced to <ptr>/<null>: heap addresses differ between
    processes, so the raw value can never compare equal."""
    out = []
    for e in events:
        if str(e.get('tag', '')).endswith('_ret') and isinstance(e.get('ret'), str) \
                and trace_diff.HEX_POINTER.fullmatch(e['ret'].strip()):
            e = dict(e, ret='<null>' if int(e['ret'].strip(), 16) == 0 else '<ptr>')
        out.append(e)
    return out


def _row_events(events, tag):
    """The events of one hooked function (entry and return, pointer returns
    reduced to presence), plus any terminal failure event (a crash never
    passes)."""
    keep = {tag, tag + '_ret'}
    return _ptr_returns([e for e in events if str(e.get('tag', '')) in keep
                         or str(e.get('tag', '')).lower() in trace_diff.TERMINAL_FAILURE_TAGS])


def _row_crash(orig, ours):
    return any(str(e.get('tag', '')).lower() in trace_diff.TERMINAL_FAILURE_TAGS for e in orig + ours)


def _first_difference(orig, ours):
    """A short description of the first differing event pair, or of a crash."""
    try:
        o, r = trace_diff.normalize(orig), trace_diff.normalize(ours)
    except trace_diff.TraceInputError as e:
        return 'malformed event: %s' % e
    for side, events in (('original', o), ('ours', r)):
        for e in events:
            if str(e.get('tag', '')).lower() in trace_diff.TERMINAL_FAILURE_TAGS:
                return '%s logged %s' % (side, json.dumps(e, sort_keys=True))
    for i in range(max(len(o), len(r))):
        a = o[i] if i < len(o) else None
        b = r[i] if i < len(r) else None
        if a != b:
            return 'event %d: original %s, ours %s' % (
                i + 1, json.dumps(a, sort_keys=True), json.dumps(b, sort_keys=True))
    return 'same events in a different order'


def _as_scenario(value):
    """A frida_run scenario result, or a bare (orig, ours) pair."""
    if isinstance(value, dict):
        return value
    return {'orig': value[0], 'ours': value[1], 'failed': None}


def uninformative_reason(t):
    """Why a failed scenario result measured nothing, or '' when it did (or
    did not fail): it failed on BOTH sides with the same non-crash outcome
    (the same RESULT word, both NOT-LOADED or both the same OBSERVER-ERROR,
    or both no status, and the same exit code), and neither side logged a
    crash.  Both exit codes must be non-zero (a side that exited 0 but left
    no trace file is a one-sided failure).  A one-sided failure (the original CLEAN and ours exited, ours
    crashed) is evidence and is not uninformative."""
    if not isinstance(t, dict) or not t.get('failed'):
        return ''
    result = t.get('result') or {}
    a, b = result.get('orig'), result.get('ours')
    if a != b or str(a).upper() == 'CRASH' or not t.get('orig_exit') \
            or t.get('orig_exit') != t.get('ours_exit'):
        return ''
    if any(str(e.get('tag', '')).lower() in trace_diff.TERMINAL_FAILURE_TAGS
           for e in list(t.get('orig') or ()) + list(t.get('ours') or ())):
        return ''
    if a is None:
        return 'both sides exited %s with no status' % t.get('orig_exit')
    return 'both sides %s' % a


EQUAL_PREFIX = 'identical in '


def _prefix_side(orig, ours, tag):
    """How two event-different row traces relate: 'orig' or 'ours' when that
    side's events are a strict prefix of the other side's and it logged fewer
    calls than the hooks' cap (its trace is complete: that side just made
    fewer calls), 'one side' when only one side logged a call at all, None
    when they differ at a call both made (or a crash event is there)."""
    if any(str(e.get('tag', '')).lower() in trace_diff.TERMINAL_FAILURE_TAGS
           for e in orig + ours):
        return None
    calls = lambda events: sum(1 for e in events if e.get('tag') == tag)
    if not calls(orig) or not calls(ours):
        return 'one side' if calls(orig) or calls(ours) else None
    short, other, side = (orig, ours, 'orig') if len(orig) < len(ours) else (ours, orig, 'ours')
    if len(short) == len(other) or calls(short) >= HOOK_MAX:
        return None
    return side if trace_diff.compare(short, other[:len(short)]) == 0 else None


ADDRESS_LIKE = (0x00010000, 0x7FFFFFFF)


def _address_like_difference(orig, ours, tag, sources):
    """When every differing field of the row's two traces is an argument slot
    typed from Ghidra (sources: 'ours'/'ghidra' per argument) holding a
    value in ADDRESS_LIKE on both sides, the detail naming the first such
    slot; otherwise None. Same event counts and order are required."""
    if not sources:
        return None
    try:
        o, r = trace_diff.normalize(orig), trace_diff.normalize(ours)
    except trace_diff.TraceInputError:
        return None
    if len(o) != len(r) or any(e.get('tag') != f.get('tag') for e, f in zip(o, r)) \
            or any(str(e.get('tag', '')).lower() in trace_diff.TERMINAL_FAILURE_TAGS for e in o + r):
        return None
    first = None
    for e, f in zip(o, r):
        left = {k: v for k, v in e.items() if k != 'tag'}
        right = {k: v for k, v in f.items() if k != 'tag'}
        for field, a, b in trace_diff._value_diffs(left, right, '', 0.0):
            m = re.match(r'^a(\d+)_\w+$', field)
            if not m or int(m.group(1)) >= len(sources) or sources[int(m.group(1))] != 'ghidra':
                return None
            if not all(isinstance(v, int) and not isinstance(v, bool)
                       and ADDRESS_LIKE[0] <= v <= ADDRESS_LIKE[1] for v in (a, b)):
                return None
            if first is None:
                first = 'address-like untyped argument a%s (%s vs %s); type it from the header' % (
                    m.group(1), a, b)
    return first


def frida_verdicts(traces, rows, skipped=None, info=None):
    """{'0x<ADDR>': (status, detail)} from the {scenario: ...} traces frida_run
    returns (a bare (orig, ours) pair per scenario is accepted too) and the
    rows, with the hook table's `skipped` mapping. status is one of:
    'equal' (detail `identical in <scenarios>`, the scenarios whose compare
    returned 0 first); 'different' (an event-level difference: an argument
    or a return at a call both sides made, or a crash event; detail carries
    the first differing pair); 'not measured' (a compared scenario without
    final HITS totals on both sides where they are needed, one whose state
    dumps do not match (the dump guard), one where Frida could not hook the
    row on a side (orig.py's {tag: 'unhooked'} event: that side saw nothing,
    so neither its calls nor their absence count), or a row called on one
    side only);
    'scenario failed'; 'not-exercised'; 'skipped' (the row had no hook;
    detail is the reason).

    A row is exercised in a scenario when either side logged one of its
    hooks. A failed scenario (a probe exited non-zero or left no trace) that
    shows the row is never equal (a crash event is a difference, a partial
    trace is 'scenario failed'); one that does not show it counts as 'not
    hit' only when its ORIGINAL side completed (exit 0, HITS present, 0 for
    that hook), otherwise the row is 'scenario failed' too. A scenario that
    failed on both sides the same way without a crash (both NOT-LOADED, both
    no status, the same OBSERVER-ERROR) is uninformative
    (uninformative_reason): left out of every row, reported in
    info['uninformative'] {scenario: reason}. A scenario whose
    dumps (frida_run's) do not match measures nothing.

    Frequency is a caller's business (the final review's ruling): when every
    captured event compares equal but the final totals differ (both sides
    have final totals), the row is still equal in that scenario; so is the
    prefix case, one side's events a strict prefix of the other's with that
    side under the hooks' cap, when both sides have final totals (without
    them it is not measured: a killed run's shorter trace proves nothing).
    The frequency difference travels out of band, never as a status: info (a
    dict, optional) receives 'notes' {address: [text]} for those, and
    'exact' {address: [scenarios whose compare returned 0]}, the only
    scenarios a `compare=0` trace_source may cite. Equal events without
    final totals on both sides are not measured too (the hooks cap what they
    log, and the timer's totals lose a run-dependent tail); a bare pair
    carries no totals and compares on its events alone.

    Across the scenarios any difference wins, then a failed scenario, then
    not measured, then equal; a row exercised nowhere is not-exercised.
    Pointer returns are compared by presence."""
    skipped = skipped or {}
    info = {} if info is None else info
    # A scenario that failed the same way on both sides measured nothing: it
    # is left out of every row's verdict (info['uninformative']).
    uninformative = {sc: why for sc, v in traces.items()
                     if (why := uninformative_reason(_as_scenario(v)))}
    info['uninformative'] = uninformative
    notes_out = info.setdefault('notes', {})
    exact_out = info.setdefault('exact', {})
    verdicts = {}
    for row in rows:
        address = row['address']
        key = '0x' + (address[2:] if address.lower().startswith('0x') else address).upper()
        if key in skipped:
            verdicts[key] = ('skipped', skipped[key])
            continue
        tag = hook_name(key)
        different, exact, prefix, partial, crashed = [], [], [], [], []
        unmeasured, dumped, one_sided, notes, blind = [], [], [], [], []
        addrlike = []
        sources = (info.get('typed_slots') or {}).get(key)
        if sources is None and (info.get('typed_from') or {}).get(key) == 'ghidra':
            sources = ['ghidra'] * 64
        for scenario, value in traces.items():
            if scenario in uninformative:
                continue
            t = _as_scenario(value)
            if t['failed']:
                # A partial trace that saw this row can never read equal.
                orig, ours = _row_events(t['orig'], tag), _row_events(t['ours'], tag)
                if [e for e in orig + ours if str(e.get('tag', '')).startswith(tag)]:
                    terminal = [(side, e) for side, evs in (('original', t['orig']), ('ours', t['ours']))
                                for e in evs if str(e.get('tag', '')).lower()
                                in trace_diff.TERMINAL_FAILURE_TAGS]
                    if terminal:
                        crashed.append('%s (%s): %s logged %s' % (
                            scenario, t['failed'], terminal[0][0],
                            json.dumps(terminal[0][1], sort_keys=True)))
                    else:
                        partial.append('%s: %s' % (scenario, t['failed']))
                else:
                    ohits = (t.get('hits') or {}).get('orig')
                    if not (t.get('orig_exit') == 0 and ohits and tag in ohits and ohits[tag] == 0):
                        partial.append('%s: %s' % (scenario, t['failed']))
                continue
            refused = [(side, e) for side, events in (('the original', t['orig']), ('ours', t['ours']))
                       for e in events if e.get('tag') == 'unhooked' and e.get('name') == tag]
            if refused:
                # Frida refused the site on that side (a body too short for its
                # jump): that side's trace is blind to the row, called or not.
                blind.append('Frida could not hook it on %s in %s (%s)' % (
                    refused[0][0], scenario, refused[0][1].get('error') or 'no reason given'))
                continue
            orig, ours = _row_events(t['orig'], tag), _row_events(t['ours'], tag)
            if not [e for e in orig + ours if str(e.get('tag', '')).startswith(tag)]:
                continue
            # The dump guard: a scenario whose two runs did not keep the same
            # state (frida_run's dumps) measures nothing.
            dumps = t.get('dumps', {'match': True})
            if not (isinstance(dumps, dict) and dumps.get('match') is True):
                why = (dumps or {}).get('detail') or 'not compared'
                dumped.append('%s in %s (%s)' % (
                    'dumps differ' if isinstance(dumps, dict) and dumps.get('match') is False
                    else 'dumps not compared', scenario, why))
                continue
            code = trace_diff.compare(orig, ours)
            # Only the exit-time (final) totals are complete; the timer's
            # last message loses a run-dependent tail.
            totals = t.get('hits')
            final = t.get('final') or {}
            have_final = bool(totals and totals.get('orig') and totals.get('ours')
                              and final.get('orig') and final.get('ours'))
            counts = ((totals['orig'].get(tag, 0), totals['ours'].get(tag, 0))
                      if have_final else None)
            if code == 0:
                if totals is not None and not have_final:
                    unmeasured.append(scenario)
                    continue
                exact.append(scenario)
                if counts and counts[0] != counts[1]:
                    notes.append('%s: frequency only: original %d calls, ours %d (final totals); '
                                 'every captured event equal' % ((scenario,) + counts))
                continue
            side = _prefix_side(orig, ours, tag)
            if side is None and not _row_crash(orig, ours) and \
                    (why := _address_like_difference(orig, ours, tag, sources)):
                addrlike.append(why)
            elif side is None:
                different.append('%s: %s' % (scenario, _first_difference(orig, ours)))
            elif not have_final:
                unmeasured.append(scenario)
            elif side == 'one side':
                one_sided.append(scenario)
                notes.append('%s: frequency only: original %d calls, ours %d (final totals); '
                             'called on one side only, nothing to compare' % ((scenario,) + counts))
            else:
                prefix.append(scenario)
                notes.append('%s: frequency only: original %d calls, ours %d (final totals); '
                             "the %s trace is a prefix of the other's" % (
                                 (scenario,) + counts + ('original' if side == 'orig' else 'ours',)))
        if different or crashed:
            verdicts[key] = ('different', 'differs in ' + '; '.join(different + crashed))
        elif partial:
            verdicts[key] = ('scenario failed', '; '.join(partial))
        elif unmeasured or dumped or blind or addrlike:
            verdicts[key] = ('not measured', '; '.join(
                (['no final HITS totals in %s' % ', '.join(unmeasured)] if unmeasured else [])
                + dumped + blind + addrlike[:1]))
        elif exact or prefix:
            verdicts[key] = ('equal', EQUAL_PREFIX + ', '.join(exact + prefix))
        elif one_sided:
            verdicts[key] = ('not measured', 'called on one side only in %s' % ', '.join(one_sided))
        else:
            # Hit by no scenario at all (a failed scenario's trace showed nothing of it
            # either): not exercised, not a failure.
            verdicts[key] = ('not-exercised', 'no scenario hit it: %s' % (', '.join(traces) or 'any scenario'))
        if notes and verdicts[key][0] in ('equal', 'not measured'):
            notes_out[key] = notes
        exact_out[key] = exact
    return verdicts


def _file_hash(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def trace_source(probe, orig_path, ours_path):
    """The trace_source of a `kind: trace` evidence document, in the form
    tools/check_evidence.py accepts: frida <probe> orig:<sha256>
    ours:<sha256> compare=0, the sha256 of the two trace files' bytes (the
    paths frida_run returns). Only meaningful for an equal verdict."""
    return 'frida %s orig:%s ours:%s compare=0' % (
        probe, _file_hash(orig_path), _file_hash(ours_path))


trace_source_line = trace_source


# ---- the oracle path, end to end -------------------------------------------------

CMAKE_ANCHOR = 'cases/store_spell.cpp'   # the last case file of oracle/CMakeLists.txt's runner
# The six scenarios promoted on 2026-10-23 (MATCH at every checkpoint).
PROMOTED_SCENARIOS = ('action_bar', 'equip_shots', 'inventory', 'save_load_screens',
                      'level_up', 'world_map')
# The world scenarios of the Frida phase.  The phase does not run all nine per
# batch: driver._frida_phase picks the ones the coverage map says hit a row.
FRIDA_SCENARIOS = ('world_entry', 'first_fight', 'walk_and_talk') + PROMOTED_SCENARIOS
# UI-only scenarios (no world play): candidates when a row's file is a screen.
FRIDA_UI_SCENARIOS = ('biography',)
# Scenarios held out of the driver's phase (driver.Driver.held_out applies it).
# biography returned with Task 18 (--no-loaded on both sides).  save_load_screens
# stays out until one real original/ours pair confirms matching dumps now that
# every probe run starts from an empty save folder (probe.saves_of_its_own);
# the coordinator runs that pair, then removes it from this tuple.
PHASE_HELD_OUT = ('save_load_screens',)
UI_FILE_PREFIXES = ('src/CScreen', 'src/CUI')


def scenarios_for(rows, files):
    """The Frida phase's candidate scenarios for a batch: FRIDA_SCENARIOS,
    plus FRIDA_UI_SCENARIOS when any file name (files: the rows' file names;
    a row's own 'file' counts too) is a UI screen (src/CScreen*, src/CUI*)."""
    names = list(files or ())
    names += [r.get('file') for r in (rows or ()) if isinstance(r, dict)]
    ui = any(isinstance(n, str) and n.replace('\\', '/').startswith(UI_FILE_PREFIXES)
             for n in names)
    return tuple(FRIDA_SCENARIOS) + (tuple(FRIDA_UI_SCENARIOS) if ui else ())


class OracleError(RuntimeError):
    """The oracle could not be built or run (not a verdict)."""


def evidence_signature(evidence_text):
    """The function signature line of an evidence file's Decompiled (Ghidra)
    section (everything before the opening brace, joined on one line), or
    None."""
    code = _section_code(evidence_text)
    return _signature_head(code) if code is not None else None


def _signature_head(code):
    """The text before the first `{` of decompiled code, comments stripped and
    whitespace joined on one line; None without a brace or an empty head."""
    if '{' not in code:
        return None
    head = code[:code.find('{')]
    head = re.sub(r'/\*.*?\*/', '', head, flags=re.S)
    head = re.sub(r'//[^\n]*', '', head)
    head = ' '.join(head.split())
    return head or None


def exports_default():
    """The ghidra-exports folder tools/ghidra/export_decompiles.py writes by
    default (IWD2R_HOME aware). The four lines are repeated here on purpose:
    loading that module puts tools/ on sys.path and imports its Ghidra
    helpers; the test suite keeps the two in step."""
    home = (os.environ.get('IWD2R_HOME')
            or os.path.join(os.path.expanduser('~'), '.local', 'share', 'iwd2r'))
    return os.path.join(home, 'ghidra-exports')


def export_signature(address, exports_dir=None):
    """The one-line signature head of the Ghidra decompile export's
    <exports_dir>/decompile/<ADDR>.c (ADDR: `address` without 0x, uppercase),
    or None when the file is missing, unreadable or has no `{`. exports_dir
    None is exports_default()."""
    if exports_dir is None:
        exports_dir = exports_default()
    addr = str(address).strip()
    if addr[:2].lower() == '0x':
        addr = addr[2:]
    try:
        with open(os.path.join(exports_dir, 'decompile', addr.upper() + '.c'),
                  encoding='utf-8', errors='replace') as f:
            code = f.read()
    except OSError:
        return None
    return _signature_head(code)


_check_evidence = None
_fixpoint_cache = {}


def rules_math_names(read_evidence, addresses=None):
    """The pure helpers among the rules-math set (tools/check_evidence.py
    RULES_MATH, keyed by address; read_evidence takes '0x<ADDR>'). RULES_MATH
    lists the functions whose correctness needs the oracle, not pure ones, so
    its names are not trusted: this is the fixpoint of is_pure over the
    helpers' own evidence. Start from the empty set, add each helper whose
    evidence passes is_pure(text, current set), repeat until nothing changes.
    The name of a helper is the `name:` of its evidence file; one with no
    evidence file is left out. Cached per process, by the evidence read."""
    global _check_evidence
    if addresses is None:
        if _check_evidence is None:
            import importlib.util
            path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'check_evidence.py')
            spec = importlib.util.spec_from_file_location('truth_check_evidence', path)
            _check_evidence = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(_check_evidence)
        addresses = _check_evidence.RULES_MATH
    helpers = {}
    for address in sorted(addresses):
        text = read_evidence('0x%X' % address)
        m = re.search(r'^name:\s*(\S+)\s*$', text or '', re.M)
        if m:
            helpers[m.group(1)] = text
    cache_key = hashlib.sha256(json.dumps(sorted(helpers.items())).encode('utf-8')).hexdigest()
    if cache_key not in _fixpoint_cache:
        pure = frozenset()
        while True:
            grown = pure | {n for n, t in helpers.items() if n not in pure and is_pure(t, pure)[0]}
            if grown == pure:
                break
            pure = grown
        _fixpoint_cache[cache_key] = pure
    return _fixpoint_cache[cache_key]


def oracle_candidates(rows, read_evidence, pure_names=None):
    """([row dict for write_cases], {address: why not}) over the rows (dicts
    with address and name). read_evidence(address) is the evidence file's text
    or None. A row is a candidate when its evidence is_pure and draft_case
    does not skip it. pure_names defaults to the rules-math set."""
    if pure_names is None:
        pure_names = rules_math_names(read_evidence)
    keep, why = [], {}
    for row in rows:
        address = row['address']
        text = read_evidence(address)
        if text is None:
            why[address] = 'no evidence file'
            continue
        pure, reason = is_pure(text, pure_names)
        if not pure:
            why[address] = 'not pure: ' + reason
            continue
        signature = evidence_signature(text)
        if signature is None:
            why[address] = 'no signature'
            continue
        cand = {'address': address, 'name': row['name'], 'signature': signature}
        if row.get('header'):
            cand['header'] = row['header']
        draft = draft_case(address, row['name'], signature, 'probe')
        if draft.startswith('// skipped'):
            why[address] = draft.split('): ', 1)[-1].strip()
            continue
        keep.append(cand)
    return keep, why


def wire_cases(cmake_path, batch_id):
    """Add cases/wave-<batch>.cpp to the oracle runner's sources in
    oracle/CMakeLists.txt (after the last listed case file). Idempotent;
    returns True when it changed the file."""
    entry = 'cases/wave-%s.cpp' % batch_id
    with open(cmake_path, encoding='utf-8') as f:
        text = f.read()
    if entry in text:
        return False
    if CMAKE_ANCHOR not in text:
        raise OracleError('%s lists no %s to add %s after' % (cmake_path, CMAKE_ANCHOR, entry))
    text = text.replace(CMAKE_ANCHOR, CMAKE_ANCHOR + '\n        ' + entry, 1)
    with open(cmake_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return True


def oracle_build(worktree, run=subprocess.run):
    """`oracle.py build` in the worktree (the x86 runner, through WSL interop)."""
    done = run([sys.executable, '-I', os.path.join('tools', 'oracle.py'), 'build'],
               cwd=worktree, capture_output=True, text=True)
    if getattr(done, 'returncode', 0) != 0:
        raise OracleError('oracle.py build exited %s: %s' % (
            done.returncode, ((done.stdout or '') + (done.stderr or '')).strip()[-400:]))


def run_oracle(batch_id, worktree, cases, build=oracle_build, run=subprocess.run):
    """{'0x<ADDR>': (status, detail)} for the drafted cases (row dicts for
    write_cases): the cases are written into the worktree's oracle/cases, the
    file is added to its oracle/CMakeLists.txt, `build(worktree)` runs, then
    `oracle.py run --file wave-<batch>` and its report is read with
    probe_verdicts. OracleError when it cannot build or leaves no report.
    Case files stay in place as the functions' oracle cases."""
    if not cases:
        return {}
    write_cases(batch_id, cases, os.path.join(worktree, 'oracle', 'cases'))
    wire_cases(os.path.join(worktree, 'oracle', 'CMakeLists.txt'), batch_id)
    build(worktree)
    done = run([sys.executable, '-I', os.path.join('tools', 'oracle.py'), 'run',
                '--file', 'wave-%s' % batch_id],
               cwd=worktree, capture_output=True, text=True)
    out = (done.stdout or '') + (done.stderr or '')
    m = re.search(r'^report: (\S+)\s*$', done.stdout or '', re.M)
    if not m:
        raise OracleError('oracle.py run exited %s with no report: %s' % (
            done.returncode, out.strip()[-400:]))
    try:
        with open(os.path.join(worktree, m.group(1)), encoding='utf-8') as f:
            report = json.load(f)
    except (OSError, ValueError) as e:
        raise OracleError('cannot read the oracle report %s: %s' % (m.group(1), e)) from None
    return probe_verdicts(report, batch_id)


# ---- calibration -------------------------------------------------------------------

CALIBRATION_REL = os.path.join('docs', 'wso', 'truth-calibration.json')
SAMPLE_SIZE = 10
DUMP_CASE = re.compile(r'^(?:dump|readout)-')


def _key(address):
    a = address[2:] if address.lower().startswith('0x') else address
    return '0x' + a.upper()


def calibration_rows(state, seed, sample_size=SAMPLE_SIZE):
    """(rows, sample): rows are every closed-batch verdict whose side is fork
    or neither (the standing ruling's probe rows) plus a seeded sample of
    closed `ours` verdicts decided by a dump reading (dump-* or readout-*
    case). Each row is {address, batch, name, side, case, sample}. The
    sample is drawn with random.Random(seed) from the candidates sorted by
    (batch, address); sample is the sorted list of its addresses."""
    rows, candidates = [], []
    for bid in sorted(state['batches']):
        b = state['batches'][bid]
        if b.get('status') != 'closed':
            continue
        names = {_key(r['address']): r.get('name', '') for r in b.get('rows', [])}
        for address in sorted(b.get('verdicts', {}), key=_key):
            v = b['verdicts'][address]
            row = {'address': _key(address), 'batch': bid, 'name': names.get(_key(address), ''),
                   'side': v['side'], 'case': v.get('case', ''), 'sample': False}
            if v['side'] in ('fork', 'neither'):
                rows.append(row)
            elif v['side'] == 'ours' and DUMP_CASE.match(v.get('case', '')):
                candidates.append(row)
    picked = random.Random(seed).sample(candidates, min(sample_size, len(candidates)))
    picked.sort(key=lambda r: (r['batch'], r['address']))
    for r in picked:
        r['sample'] = True
    return rows + picked, [r['address'] for r in picked]


def _path_counts(statuses, measured_set, agree_status, rows):
    measured = agree = disagree = 0
    for row in rows:
        got = statuses.get(row['address'])
        status = got[0] if isinstance(got, (tuple, list)) else got
        if status in measured_set:
            measured += 1
            if status == agree_status:
                agree += 1
            else:
                disagree += 1
    return {'measured': measured, 'agree': agree, 'disagree': disagree, 'of': len(rows)}


def calibrate(state, rows, results, seed=None, sample=None):
    """The calibration record: {seed, sample, paths: {oracle, frida}, frida_default}.

    rows come from calibration_rows; results is {'oracle': {address: (status,
    detail)}, 'frida': {...} or None}. A path measures a row when it gives
    pass or fail (oracle) or equal or different (Frida); it agrees on pass or
    equal and disagrees on fail or different. frida_default is true only when
    Frida measured at least a fifth of the sample and agreed at least as often as it disagreed (else `note` says why). A None Frida result means
    the path was deferred: measured 0, marked deferred. (state is accepted for
    the interface; the rows already carry their recorded verdicts.)"""
    sample = list(sample if sample is not None else [r['address'] for r in rows if r.get('sample')])
    rows = [dict(r, address=_key(r['address'])) for r in rows]
    norm = lambda d: {_key(a): v for a, v in (d or {}).items()}
    oracle = _path_counts(norm(results.get('oracle')), ('pass', 'fail'), 'pass', rows)
    frida = _path_counts(norm(results.get('frida')), ('equal', 'different'), 'equal', rows)
    if results.get('frida') is None:
        frida['deferred'] = True
    in_sample = [r for r in rows if r['address'] in sample]
    frida_in_sample = _path_counts(norm(results.get('frida')), ('equal', 'different'), 'equal',
                                   in_sample)['measured']
    enough = bool(sample) and frida_in_sample * 5 >= len(sample)
    on = enough and frida['agree'] >= frida['disagree']
    cal = {'seed': seed, 'sample': sample,
           'paths': {'oracle': oracle, 'frida': frida},
           'frida_default': on}
    if not on:
        if results.get('frida') is None:
            cal['note'] = 'frida off: the path was deferred (not run).'
        elif not enough:
            cal['note'] = ('frida off: measured %d of %d, agree %d, disagree %d: fewer than a fifth of the '
                           '%d sample rows were measured.' % (frida['measured'], frida['of'], frida['agree'],
                                                            frida['disagree'], len(sample)))
        else:
            cal['note'] = ('frida off: measured %d of %d, agree %d, disagree %d: a path that contradicts '
                           'replay-verified rows more often than it agrees is miscalibrated.' % (
                               frida['measured'], frida['of'], frida['agree'], frida['disagree']))
    return cal


def calibration_text(cal):
    """The scorecard paragraph for a calibration record."""
    paths = cal['paths']

    def line(name, label):
        p = paths[name]
        text = '%s: measured %d of %d rows, agreed %d, disagreed %d' % (
            label, p['measured'], p['of'], p['agree'], p['disagree'])
        return text + (' (deferred: not run)' if p.get('deferred') else '')
    return ('Truth calibration (seed %s, sample %s, tools/truth.py over the %d ruling rows with side fork or neither '
            'plus a sample of %d dump-decided ours rows; a path measures a row when it gives pass/fail '
            '(oracle) or equal/different (Frida) and agrees on pass or equal): %s; %s; Frida on by '
            'default: %s.\n' % (
                cal.get('seed'), ', '.join(cal.get('sample', [])) or 'none', paths['oracle']['of'] - len(cal.get('sample', [])),
                len(cal.get('sample', [])), line('oracle', 'oracle probes'),
                line('frida', 'Frida trace diff'), 'yes' if cal.get('frida_default') else 'no'))


def frida_default(path=os.path.join(ROOT, CALIBRATION_REL)):
    """Whether the Frida path is on by default: truth-calibration.json says so;
    a missing or unreadable file means no."""
    try:
        with open(path, encoding='utf-8') as f:
            return json.load(f).get('frida_default') is True
    except (OSError, ValueError, AttributeError):
        return False


def frida_batch(batch_id, rows, signatures, map_text, scenarios=FRIDA_SCENARIOS,
                run_frida=frida_run, headers=None, info=None, names=None):
    """{'0x<ADDR>': (status, detail)} for the rows through one batched Frida
    differential run: the hook table from the signatures and our .map text,
    run_frida(batch_id, table, scenarios) for the traces, frida_verdicts to
    compare. headers (our src/*.h texts; default this checkout's) type the
    hooks from our prototypes; names ({int address: our name}; default this
    checkout's markers, see our_names) resolves each row to our name at its
    address when the row's recorded name is stale; info, a dict, receives
    typed_from and
    frida_verdicts' notes (frequency-only differences) and exact (the
    scenarios a compare=0 citation may name). run_frida is injectable (the
    default launches the game)."""
    table = hook_table(rows, signatures, map_text, list(scenarios),
                       _our_headers() if headers is None else headers,
                       our_names() if names is None else names)
    if info is not None:
        info['typed_from'] = dict(table.get('typed_from', {}))
        info['typed_slots'] = {k: list(v) for k, v in table.get('typed_slots', {}).items()}
        info['skipped'] = dict(table.get('skipped', {}))
    traces = run_frida(batch_id, table, list(scenarios)) if table['original']['hooks'] else {}
    return frida_verdicts(traces, rows, table['skipped'], info=info)


def _scenario_summary(traces):
    out = {}
    for name, t in traces.items():
        t = _as_scenario(t)
        out[name] = {k: t.get(k) for k in ('orig_exit', 'ours_exit', 'failed', 'result', 'hits', 'final',
                                           'dumps')}
        out[name]['trace'] = {side: os.path.relpath(t[side + '_path'], ROOT) if t.get(side + '_path') else None
                              for side in ('orig', 'ours')}
    return out


def _our_headers(root=None):
    """The texts of our src/*.h (under root, default this checkout), for the
    hook tables' prototypes."""
    texts = []
    for path in sorted(glob.glob(os.path.join(root or ROOT, 'src', '*.h'))):
        with open(path, encoding='utf-8', errors='replace') as f:
            texts.append(f.read())
    return texts


def our_names(root=None):
    """{int address: our current name} of every `// 0xADDR` marker above a
    function definition in src/**/*.cpp (under root, default this checkout;
    labels.ours over definitions(), the scan re_status goes by too), the
    first marker winning for an address carried twice. The Frida phases
    resolve a row's function by this name: the address is the identity, and
    a row may carry a name xrec recorded before we renamed or split its
    class."""
    found = {}
    for address, name in labels.ours(os.path.join(root or ROOT, 'src')):
        found.setdefault(address, name)
    return found


def _calibrate_frida(rows, read_evidence, map_path):
    """({address: (status, detail)}, {'scenarios': summary, 'error': text}):
    one batched Frida run over every row. frida_run is looked up at call time
    (tests patch it); each side's --new-attempt reason is `calibrate
    <scenario> <side>`. A failure of the run itself leaves every row
    unmeasured and is recorded under `error`."""
    sigs = {}
    for r in rows:
        text = read_evidence(r['address'])
        sig = evidence_signature(text) if text else None
        if sig:
            sigs[r['address']] = sig
    captured, info = {}, {}

    def run_frida(batch_id, table, scenarios):
        captured.update(frida_run(batch_id, table, scenarios,
                                  attempt_reason=lambda scenario, side: 'calibrate %s %s' % (
                                      scenario, 'original' if side == 'orig' else 'ours')))
        return captured
    try:
        with open(map_path, encoding='utf-8', errors='replace') as f:
            map_text = f.read()
        got = frida_batch('cal', rows, sigs, map_text, run_frida=run_frida, info=info)
        error = infrastructure_failure(captured) or None
        if error:
            got = {}
            error = error[:300]
    except Exception as e:
        got, error = {}, repr(e)[:300]
    got = {_key(a): v for a, v in got.items()}
    for r in rows:
        got.setdefault(_key(r['address']), ('not measured', error or 'no result'))
    return got, {'scenarios': _scenario_summary(captured), 'error': error,
                 'typed_from': info.get('typed_from', {}), 'notes': info.get('notes', {})}


def _calibrate_main(args):
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    state_path = os.path.abspath(args.state) if args.state else os.path.join(root, 'docs', 'wso', 'wave-state.json')
    with open(state_path, encoding='utf-8') as f:
        state = json.load(f)
    rows, sample = calibration_rows(state, args.seed)
    reader_root = args.evidence_root or root

    def read_evidence(address):
        try:
            with open(os.path.join(reader_root, 'docs', 'evidence', _key(address)[2:] + '.md'),
                      encoding='utf-8') as f:
                return f.read()
        except OSError:
            return None
    cases, why = oracle_candidates(rows, read_evidence)
    print('calibrate: %d rows (%d sample), %d oracle candidates' % (len(rows), len(sample), len(cases)))
    for address, reason in sorted(why.items()):
        print('  not probed %s: %s' % (address, reason))
    oracle = {}
    if cases:
        if not args.worktree:
            print('truth.py calibrate: %d oracle candidate(s); pass --worktree <a disposable clone>: '
                  'the cases are written and CMake is edited there, never in the real checkout'
                  % len(cases), file=sys.stderr)
            return 2
        # One case file per calibration run; the batch id inside the case ids is `cal`.
        oracle = run_oracle('cal', args.worktree, cases)
    for address, (status, detail) in sorted(oracle.items()):
        print('  probe %s: %s (%s)' % (address, status, detail))
    frida, frida_info = None, {}
    if not args.no_frida:
        if not args.frida_map:
            print('truth.py calibrate: pass --frida-map <our iwd2r.map> (or --no-frida for the deferred record)',
                  file=sys.stderr)
            return 2
        frida, frida_info = _calibrate_frida(rows, read_evidence, args.frida_map)
    cal = calibrate(state, rows, {'oracle': oracle, 'frida': frida}, seed=args.seed, sample=sample)
    if frida is not None:
        cal['frida_rows'] = {a: list(v) for a, v in sorted(frida.items())}
        cal['frida_scenarios'] = frida_info.get('scenarios', {})
        cal['frida_typed_from'] = frida_info.get('typed_from', {})
        # Frequency-only differences: caller findings, never a verdict.
        cal['frida_notes'] = {a: list(n) for a, n in sorted(frida_info.get('notes', {}).items())}
        if frida_info.get('error'):
            cal['paths']['frida']['error'] = frida_info['error']
        for a, (status, detail) in sorted(frida.items()):
            print('  frida %s: %s (%s)' % (a, status, detail))
    cal['oracle_rows'] = {a: list(v) for a, v in sorted(oracle.items())}
    out = args.out or os.path.join(root, CALIBRATION_REL)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(cal, f, indent=1, sort_keys=True)
        f.write('\n')
    print(calibration_text(cal), end='')
    print('wrote %s' % out)
    return 0


def main(argv=None):
    import argparse
    p = argparse.ArgumentParser(prog='truth.py', description=__doc__.split('\n')[0])
    sub = p.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('calibrate', help='run the measurement paths over already-ruled rows')
    c.add_argument('--seed', default='2026-10-22', help='sample seed (default %(default)s)')
    c.add_argument('--state', help='the wave-state file to read (default docs/wso/wave-state.json)')
    c.add_argument('--worktree', help='where the oracle cases are written, wired and built '
                                      '(required when a row is probed; use a disposable clone)')
    c.add_argument('--evidence-root', help='checkout whose docs/evidence to read (default this one)')
    c.add_argument('--no-frida', action='store_true',
                   help='skip the Frida path: record it as deferred (default: run it, needs --frida-map)')
    c.add_argument('--frida-map', help='the x86 build\'s iwd2r.map (ours-side RVAs for the Frida run)')
    c.add_argument('--out', help='calibration file to write (default docs/wso/truth-calibration.json)')
    args = p.parse_args(argv)
    return _calibrate_main(args)


if __name__ == '__main__':
    sys.exit(main())
