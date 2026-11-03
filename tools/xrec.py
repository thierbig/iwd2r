"""Cross-recovery comparison: per C++ function, in THEIRS vs OURS for src/*.cpp.

Usage: python3 -I tools/xrec.py BASE THEIRS OURS [--json OUT]

Both recoveries descend from BASE. Every function of every shared file lands
in one of ten categories: same / same_stub (identical bodies, real or stub),
theirs_real_ours_stub / ours_real_theirs_stub (one side recovered, the other
still a stub), theirs_changed_ours_base / ours_changed_theirs_base (one side
recovered since BASE), both_changed_differ (the two recoveries disagree, the
cross-check candidates), both_stub_diff, only_theirs / only_ours. The command
prints the counts and, with --json, writes one row per function,
[{"file", "name", "address", "category", "their_len", "our_len"}], sorted by
category then total body length descending: the ranked disagreement list the
integration waves work from.

An address-gap pass (task 69) then appends one only_theirs row per address
theirs carries and ours never has: every function definition of any
src/*.cpp of theirs — a theirs-only file, a free function (no ::) or an
overload the shared-file pairing collapses — whose marker address no row of
that pairing already carries and that no marker line of our src/**/*.cpp or
src/**/*.h names, stubs skipped. An address that only ever appears as a
removed marker line in our own history (one `git log -p` pass over src), or
that theirs carries only in a file ours deleted wholesale (a path of ours
that shows in one `git log --diff-filter=D` pass and is gone at the ours
ref), is one we deleted on purpose: it makes no row and is counted in
main()'s summary line as `removed by us`.

A function is matched between the trees by its // 0xADDR comment first (a
renamed function is still the function) and by name second; the address is
read from the nearest line above that is a marker line — a comment that
starts with the address, 5 to 8 hex digits, followed by either nothing or
one parenthesised annotation that ends the line: `// 0x5A1234` and Will's
annotated `// 0x5A1234 (virtual)` both count (comment and blank lines
between the marker and the signature are skipped; a comment line that
merely mentions a hex number never supplies it). That rule is this tool's
alone, per the task 68b ruling; labels.definitions and our own tree keep
the strict whole-line `// 0xADDR` form. The gap pass shares the marker rule
and makes the address the identity: one row per address, the first real
body of theirs wins, and a free function counts only directly under a
marker at brace depth 0 (file scope). An address two functions of one tree carry (shared thunks and
unknown addresses get stamped alike) is no key; those functions match by
name. Bodies are compared brace-matched, whitespace and comments
stripped: rough but symmetric, so the noise lands on both sides alike.
Overloads within one file collapse to the last one. The other tree is read
with git show and nothing else.

funcs(text) parses one file, classify(theirs, ours, base) sorts one file's
functions, report(base_ref, theirs_ref, ours_ref, out_json) runs the whole
comparison, address_gap(theirs_ref, ours_ref, carried) is the gap pass.
Standard library only.
"""

import argparse
import collections
import json
import os
import re
import subprocess
import sys

CATEGORIES = ['same', 'same_stub', 'only_theirs', 'only_ours',
              'theirs_real_ours_stub', 'ours_real_theirs_stub',
              'theirs_changed_ours_base', 'ours_changed_theirs_base',
              'both_changed_differ', 'both_stub_diff']

# Task 68b ruling: in xrec only, a marker line is a comment that starts with
# the address — 5 to 8 hex digits — then either nothing or one parenthesised
# annotation that ends the line. Accepts `// 0x5A1234` and the annotated
# style Will's tree writes: `// 0x5A1234 (virtual)`, `// 0x5A1234 (vtable
# slot 3)`, `// 0x40A0D0 (folded by /OPT:ICF with 0x40A0E0)`. Rejects
# `// 0x44-0x48). text`, `// 0x44DC10 case 0xA1 - ...`, `// see 0x5A0000`,
# `// 0xADDR` (not hex) and `// 0x44` (too short). tools/ghidra/labels.py
# (labels.ADDRESS, labels.definitions) and our own tree keep the strict
# whole-line rule; this loosening never leaves xrec.
MARKER = re.compile(r'^\s*//\s*0x([0-9A-Fa-f]{5,8})\s*(\(.*\))?\s*$')


def files(ref):
    out = subprocess.run(['git','ls-tree','-r','--name-only',ref,'src'],capture_output=True,text=True).stdout.split()
    return {f for f in out if f.endswith('.cpp')}


def show(ref, path):
    r = subprocess.run(['git','show',f'{ref}:{path}'],capture_output=True,text=True,errors='replace')
    return r.stdout if r.returncode==0 else ''


sig = re.compile(r'^(?:[A-Za-z_][\w:<>,*&\s]*?\s+)?([A-Za-z_]\w*(?:::[~A-Za-z_]\w*)+|[A-Za-z_]\w*::operator\S*)\s*\([^;{]*\)\s*(?:const)?\s*\{', re.M)

# The address-gap pass's free functions (no ::): `<type> name(params)` then
# `{`, same line or the next. The optional return type's class rejects `:`,
# so a qualified name (`CAIAction::CAIAction`) can only match `sig`, never
# twice. brace depth 0 at the match keeps in-class and in-function lines out.
FREE = re.compile(r'^(?:[A-Za-z_][\w<>,*&\s]*?\s+)?([A-Za-z_]\w*)\s*\([^;{]*\)\s*(?:const\s*)?\{', re.M)


def blank_comments(text):
    """Comments as spaces of the same length: matching and brace counting see
    past them and every offset still points into the original text."""
    text = re.sub(r'/\*.*?\*/', lambda m: ' ' * len(m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), text)


def signature_addr(text, masked, m):
    """The // 0xADDR comment of one signature match: the nearest line above
    that is a marker line — MARKER, the task 68b ruling (a comment starting
    with the address, bare or with one parenthesised annotation; xrec only,
    labels.definitions stays strict). Comment and blank lines between the
    marker and the signature are skipped, and skipping a comment line never
    takes its hex (`// 0x44-0x48). text` and `// see 0x5A0000` are comments,
    not markers). Read from the unmasked text."""
    line = masked.rfind('\n', 0, m.start()) + 1
    while line:
        start = masked.rfind('\n', 0, line - 1) + 1
        if masked[start:line - 1].strip():
            break  # a non-comment line sits between the signature and any marker
        hit = MARKER.match(text[start:line - 1])
        if hit:
            return '0x' + hit.group(1)
        line = start
    return None


def _body(masked, m):
    """The definition's body from its opening brace (a signature match's
    last character), brace-matched, whitespace and comments stripped."""
    i = m.end()-1; depth = 0; j = i
    while j < len(masked):
        c = masked[j]
        if c=='{': depth+=1
        elif c=='}':
            depth-=1
            if depth==0: break
        j+=1
    return re.sub(r'\s+','',masked[i:j+1])


def funcs(text):
    """name -> (address, body): the // 0xADDR comment at the signature (None
    without one) and the body brace-matched, whitespace and comments
    stripped. Rough but symmetric; overloads collapse to the last."""
    masked = blank_comments(text)
    out = {}
    for m in sig.finditer(masked):
        out[m.group(1)] = (signature_addr(text, masked, m), _body(masked, m))
    return out


def marked_defs(text):
    """(name, address, body) for every marker-carrying definition of one
    file, in document order, overloads separate: what `funcs()` pairs (its
    `sig` rule, the last overload aside) plus the free functions `sig` never
    sees (`FREE`, at brace depth 0 directly under a marker). The gap pass's
    view of a file."""
    masked = blank_comments(text)
    out = {}  # opening-brace offset -> definition; also the overload dedupe
    for m in sig.finditer(masked):
        a = signature_addr(text, masked, m)
        if a is not None:
            out[m.end()-1] = (m.group(1), a, _body(masked, m))
    for m in FREE.finditer(masked):
        before = masked[:m.start()]
        if before.count('{') != before.count('}'):
            continue  # inside a class or namespace: not file scope
        a = signature_addr(text, masked, m)
        if a is not None:
            out.setdefault(m.end()-1, (m.group(1), a, _body(masked, m)))
    return [out[k] for k in sorted(out)]


def stubby(body):  # TODO-only / empty bodies
    return len(body) < 40 or 'TODO' in body or 'NotImplemented' in body or 'assert(false' in body.replace(' ','')


def category(t, o, b):
    """One of the ten keys for a matched pair; t, o, b are (address, body)."""
    if t[1] == o[1]:
        return 'same_stub' if stubby(t[1]) else 'same'
    ts, os_ = stubby(t[1]), stubby(o[1])
    if ts and not os_: return 'ours_real_theirs_stub'
    if os_ and not ts: return 'theirs_real_ours_stub'
    if ts and os_: return 'both_stub_diff'
    if b is not None and t[1] == b[1]: return 'ours_changed_theirs_base'
    if b is not None and o[1] == b[1]: return 'theirs_changed_ours_base'
    return 'both_changed_differ'


def pairs(theirs, ours, base):
    """(name, address, category, their_len, our_len) for every function of one
    file: an address both trees carry is one function whatever its names, the
    rest match by name; a function only one tree has gets 0 for the other
    side. An address two functions of one tree carry (shared thunks and
    unknown addresses get stamped with one comment) is no key. The row keeps
    our name for a renamed pair. The base function is looked up by the
    pair's address first and by name second, in both loops."""
    used = set()
    def unique(fs):
        n = collections.Counter(a for a, b in fs.values() if a is not None)
        return {a: name for name, (a, b) in fs.items() if a is not None and n[a] == 1}
    t_at, o_at = unique(theirs), unique(ours)
    bn = collections.Counter(a for a, b in base.values() if a is not None)
    b_at = {v[0]: v for v in base.values() if v[0] is not None and bn[v[0]] == 1}
    for a in t_at.keys() & o_at.keys():
        tn, on = t_at[a], o_at[a]
        used.add(tn); used.add(on)
        t, o = theirs[tn], ours[on]
        b = b_at.get(a) or base.get(tn) or base.get(on)
        yield on, a, category(t, o, b), len(t[1]), len(o[1])
    free_t = {n for n in theirs if n not in used}
    free_o = {n for n in ours if n not in used}
    for n in sorted(free_t & free_o):
        t, o = theirs[n], ours[n]
        a = t[0] if t[0] is not None else o[0]
        yield n, a, category(t, o, b_at.get(a) or base.get(n)), len(t[1]), len(o[1])
    for n in sorted(free_t - free_o):
        yield n, theirs[n][0], 'only_theirs', len(theirs[n][1]), 0
    for n in sorted(free_o - free_t):
        yield n, ours[n][0], 'only_ours', 0, len(ours[n][1])


def classify(theirs, ours, base):
    """Counter over the ten keys for one file's functions."""
    return collections.Counter(p[2] for p in pairs(theirs, ours, base))


def _canon(address):
    return address if address is None else '0x' + address[2:].upper()


def our_markers(ours_ref):
    """Every address a marker line of ours names at ours_ref, over
    src/**/*.cpp and src/**/*.h: the functions (and data) our tree carries,
    headers included — an inline in a header is still ours."""
    out = set()
    ls = subprocess.run(['git','ls-tree','-r','--name-only',ours_ref,'src'],
                        capture_output=True,text=True,errors='replace').stdout.split()
    for p in ls:
        if p.endswith('.cpp') or p.endswith('.h'):
            for line in show(ours_ref, p).splitlines():
                m = MARKER.match(line)
                if m:
                    out.add(_canon('0x' + m.group(1)))
    return out


def removed_markers(ours_ref, ours_at):
    """The addresses ours deleted on purpose: every marker that ever appeared
    as a removed line of our own src history — one `git log -p` pass, no
    per-address git log -S — and that ours carries no more."""
    out = set()
    r = subprocess.run(['git','log','-p','--format=',ours_ref,'--','src'],
                       capture_output=True,text=True,errors='replace')
    for line in r.stdout.splitlines():
        m = MARKER.match(line[1:]) if line.startswith('-') else None
        if m:
            out.add(_canon('0x' + m.group(1)))
    return out - ours_at


def deleted_files(ours_ref):
    """The src/*.cpp paths ours deleted wholesale: every path of ours that
    shows in one `git log --diff-filter=D --name-only` pass over src and is
    gone at the ours ref — no per-file git log. A file we once had and
    removed (e.g. the multiplayer screen) takes every address of theirs in
    it with it, marker or no marker in our copy: removed by us."""
    out = set()
    r = subprocess.run(['git','log','--diff-filter=D','--name-only','--format=',ours_ref,'--','src'],
                       capture_output=True,text=True,errors='replace')
    for p in r.stdout.split():
        if p.endswith('.cpp'):
            out.add(p)
    return out - files(ours_ref)


def address_gap(theirs_ref, ours_ref, carried):
    """(rows, n_removed_skipped) for the pass: one only_theirs row per
    address theirs carries and ours never has — every marker-carrying
    definition of any src/*.cpp of theirs (shared and theirs-only files,
    free functions and each overload separately), stubs skipped — that no
    shared-file row of `report()` already carries (addresses in `carried`)
    and that no marker of ours names. An address ours deleted on purpose —
    a removed marker line of ours, or a file of theirs ours deleted
    wholesale — makes no row and is counted in n_removed_skipped. The
    address is the identity: the first definition per address wins."""
    ours_at = our_markers(ours_ref)
    removed = removed_markers(ours_ref, ours_at)
    gone = deleted_files(ours_ref)
    skip = {_canon(a) for a in carried if a}
    rows, seen, n_removed = [], set(), 0
    for p in sorted(files(theirs_ref)):
        for name, a, body in marked_defs(show(theirs_ref, p)):
            if a is None or stubby(body):
                continue  # marker-less: not the pass's to say, and no address
            key = _canon(a)
            if key in seen or key in skip or key in ours_at:
                continue
            seen.add(key)
            if p in gone or key in removed:
                n_removed += 1
                continue
            rows.append({'file': p, 'name': name, 'address': key,
                         'category': 'only_theirs',
                         'their_len': len(body), 'our_len': 0})
    return rows, n_removed


def report(base_ref, theirs_ref, ours_ref, out_json):
    """(Counter, n_gap, n_removed) over the shared src/*.cpp files plus the
    address-gap rows (their only_theirs count is part of the Counter);
    writes out_json with one row per function, sorted by category, then
    total length descending."""
    tf, of = files(theirs_ref), files(ours_ref)
    tot = collections.Counter(); rows = []
    for p in sorted(tf & of):
        T, O, B = funcs(show(theirs_ref,p)), funcs(show(ours_ref,p)), funcs(show(base_ref,p))
        for name, a, cat, tlen, olen in pairs(T, O, B):
            tot[cat] += 1
            rows.append({'file': p, 'name': name, 'address': a, 'category': cat,
                         'their_len': tlen, 'our_len': olen})
    gap, n_removed = address_gap(theirs_ref, ours_ref, [r['address'] for r in rows])
    tot['only_theirs'] += len(gap)
    rows += gap
    rows.sort(key=lambda r: (CATEGORIES.index(r['category']), -(r['their_len'] + r['our_len']),
                             r['file'], r['name']))
    if out_json:
        d = os.path.dirname(out_json)
        if d: os.makedirs(d, exist_ok=True)
        with open(out_json, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(rows, f, indent=1)
            f.write('\n')
    return tot, len(gap), n_removed


def main(argv=None):
    ap = argparse.ArgumentParser(description='Compare two recoveries of src/*.cpp, function by function.')
    ap.add_argument('base', help='the commit both trees descend from')
    ap.add_argument('theirs', help='the other tree (read-only, git show only)')
    ap.add_argument('ours', help='our tree')
    ap.add_argument('--json', metavar='OUT', help='also write the per-function rows there')
    args = ap.parse_args(argv)
    tot, n_gap, n_removed = report(args.base, args.theirs, args.ours, args.json)
    tf, of = files(args.theirs), files(args.ours)
    print("shared .cpp files:", len(tf & of), "| theirs-only files:", len(tf-of), "| ours-only files:", len(of-tf))
    for k in CATEGORIES:
        print(f"  {k:26} {tot[k]:6}")
    print(f"\naddress-gap rows: {n_gap} (removed by us: {n_removed} skipped)")
    print("\ntheirs-only files: functions (real bodies) they recovered that we don't have at all:")
    n = sum(1 for p in sorted(tf-of) for v in funcs(show(args.theirs,p)).values() if not stubby(v[1]))
    print("  ", n, "functions across", len(tf-of), "files")
    if args.json:
        print("\nwrote", args.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
