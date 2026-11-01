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

A function is matched between the trees by its // 0xADDR comment first (a
renamed function is still the function) and by name second; the address is
read from the signature's own line or the nearest line above with text on it
(comment and blank lines between the marker and the signature are skipped, as
labels.definitions skips them), the convention of both trees. An address two functions of one tree carry (shared thunks and
unknown addresses get stamped alike) is no key; those functions match by
name. Bodies are compared brace-matched, whitespace and comments
stripped: rough but symmetric, so the noise lands on both sides alike.
Overloads within one file collapse to the last one. The other tree is read
with git show and nothing else.

funcs(text) parses one file, classify(theirs, ours, base) sorts one file's
functions, report(base_ref, theirs_ref, ours_ref, out_json) runs the whole
comparison. Standard library only.
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


def files(ref):
    out = subprocess.run(['git','ls-tree','-r','--name-only',ref,'src'],capture_output=True,text=True).stdout.split()
    return {f for f in out if f.endswith('.cpp')}


def show(ref, path):
    r = subprocess.run(['git','show',f'{ref}:{path}'],capture_output=True,text=True,errors='replace')
    return r.stdout if r.returncode==0 else ''


sig = re.compile(r'^(?:[A-Za-z_][\w:<>,*&\s]*?\s+)?([A-Za-z_]\w*(?:::[~A-Za-z_]\w*)+|[A-Za-z_]\w*::operator\S*)\s*\([^;{]*\)\s*(?:const)?\s*\{', re.M)
addr = re.compile(r'//\s*(0x[0-9A-Fa-f]+)')


def blank_comments(text):
    """Comments as spaces of the same length: matching and brace counting see
    past them and every offset still points into the original text."""
    text = re.sub(r'/\*.*?\*/', lambda m: ' ' * len(m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), text)


def signature_addr(text, masked, m):
    """The // 0xADDR comment of one signature match: on its own line, or on
    the nearest line above with text on it — comment and blank lines between
    the marker and the signature are skipped, as labels.definitions skips
    them. Read from the unmasked text."""
    line = masked.rfind('\n', 0, m.start()) + 1
    windows = [(line, m.end())]
    while line:
        start = masked.rfind('\n', 0, line - 1) + 1
        windows.append((start, line - 1))
        if masked[start:line - 1].strip():
            break
        line = start
    for a, b in windows:
        hit = addr.search(text[a:b])
        if hit:
            return hit.group(1)
    return None


def funcs(text):
    """name -> (address, body): the // 0xADDR comment at the signature (None
    without one) and the body brace-matched, whitespace and comments
    stripped. Rough but symmetric; overloads collapse to the last."""
    masked = blank_comments(text)
    out = {}
    for m in sig.finditer(masked):
        i = m.end()-1; depth = 0; j = i
        while j < len(masked):
            c = masked[j]
            if c=='{': depth+=1
            elif c=='}':
                depth-=1
                if depth==0: break
            j+=1
        out[m.group(1)] = (signature_addr(text, masked, m), re.sub(r'\s+','',masked[i:j+1]))
    return out


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


def report(base_ref, theirs_ref, ours_ref, out_json):
    """The Counter over all shared src/*.cpp files; writes out_json with one
    row per function, sorted by category, then total length descending."""
    tf, of = files(theirs_ref), files(ours_ref)
    tot = collections.Counter(); rows = []
    for p in sorted(tf & of):
        T, O, B = funcs(show(theirs_ref,p)), funcs(show(ours_ref,p)), funcs(show(base_ref,p))
        for name, a, cat, tlen, olen in pairs(T, O, B):
            tot[cat] += 1
            rows.append({'file': p, 'name': name, 'address': a, 'category': cat,
                         'their_len': tlen, 'our_len': olen})
    rows.sort(key=lambda r: (CATEGORIES.index(r['category']), -(r['their_len'] + r['our_len']),
                             r['file'], r['name']))
    if out_json:
        d = os.path.dirname(out_json)
        if d: os.makedirs(d, exist_ok=True)
        with open(out_json, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(rows, f, indent=1)
            f.write('\n')
    return tot


def main(argv=None):
    ap = argparse.ArgumentParser(description='Compare two recoveries of src/*.cpp, function by function.')
    ap.add_argument('base', help='the commit both trees descend from')
    ap.add_argument('theirs', help='the other tree (read-only, git show only)')
    ap.add_argument('ours', help='our tree')
    ap.add_argument('--json', metavar='OUT', help='also write the per-function rows there')
    args = ap.parse_args(argv)
    tot = report(args.base, args.theirs, args.ours, args.json)
    tf, of = files(args.theirs), files(args.ours)
    print("shared .cpp files:", len(tf & of), "| theirs-only files:", len(tf-of), "| ours-only files:", len(of-tf))
    for k in CATEGORIES:
        print(f"  {k:26} {tot[k]:6}")
    print("\ntheirs-only files: functions (real bodies) they recovered that we don't have at all:")
    n = sum(1 for p in sorted(tf-of) for v in funcs(show(args.theirs,p)).values() if not stubby(v[1]))
    print("  ", n, "functions across", len(tf-of), "files")
    if args.json:
        print("\nwrote", args.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
