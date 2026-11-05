#!/usr/bin/env .venv-emu/bin/python
"""Differential emulation runner -- executes tools/emu/cases/* against the
original VC6 iwd2.exe and our MSVC rebuild and prints one verdict line per
case and input set:

    0xADDR Name set-N: EQUAL(state)|EQUAL(layout)|DIFF|ERROR <detail>

Must run under the emulation venv (Unicorn lives only in .venv-emu):
tools/oracle.py's `emu` subcommand relays here through
.venv-emu/bin/python -I; running it directly is fine too.

Exit status: 0 all sets equal, 1 any DIFF, 2 any ERROR or setup failure.
"""

import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent          # tools/emu
sys.path.insert(0, str(HERE))                   # machine, layout, cases
sys.path.insert(0, str(HERE.parent))            # tools/ for truth.py

DEFAULT_ORIG = Path('/home/thier/.local/share/iwd2r/vanilla/iwd2.exe')
DEFAULT_OURS = Path('/mnt/c/Users/thier/iwd2r-build-p3-wso-i077/build/windows-msvc-x86/RelWithDebInfo')
VENV_SETUP = ('python3 -m venv .venv-emu && .venv-emu/bin/pip install -r tools/emu/requirements.txt')


# ---------------------------------------------------------------- cases

def discover(cases_dir):
    """[(exe_addr, module_name, path)] sorted by address; one module per
    function, named c<hex-va>.py."""
    out = []
    for p in cases_dir.glob('c*.py'):
        try:
            addr = int(p.stem[1:], 16)
        except ValueError:
            continue
        out.append((addr, p.stem, p))
    return sorted(out)


def load_case(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ---------------------------------------------------------------- helpers

class CaseError(Exception):
    pass


def map_addr(machine, truth, publics, name):
    """Map-symbol VA for `name`.  Fix 2: nothing on our side may be a
    build-specific constant -- the lookup IS the resolution, so absence or
    ambiguity is a clean per-case ERROR, not an assert."""
    found = truth._map_matches(name, publics)
    if not found:
        raise CaseError(f'not in this build: {name}')
    if len(found) > 1:
        cands = ', '.join(f'{rva:#x} {dec}' for rva, dec in sorted(found.items()))
        raise CaseError(f'not in this build: {name} (ambiguous: {cands})')
    (rva, _dec), = found.items()
    return machine.IMAGE_BASE + rva


def import_addr(pe_u, name):
    """IAT-slot VA for 'dll!symbol' (dll lowercased) or a bare 'symbol'
    that exactly one DLL imports."""
    slots = pe_u.import_slots()
    for key in (name, name.lower()):
        if key in slots:
            return slots[key]
    raise CaseError(f'not in this build: {name} (import)')


def resolve(spec, machine, truth, publics, pe_u):
    """Case address spec (fix 2): int stays (vanilla literal / shared
    constant), str is a map name, ('map', name) / ('import', 'dll!sym')
    resolve at run time, [spec, ...] is a multi-site hook list."""
    if isinstance(spec, int):
        return spec
    if isinstance(spec, str):
        return map_addr(machine, truth, publics, spec)
    if isinstance(spec, list):
        return [resolve(s, machine, truth, publics, pe_u) for s in spec]
    if (isinstance(spec, tuple) and len(spec) == 2
            and spec[0] in ('map', 'import') and isinstance(spec[1], str)):
        if spec[0] == 'map':
            return map_addr(machine, truth, publics, spec[1])
        return import_addr(pe_u, spec[1])
    raise CaseError(f'bad address spec {spec!r}: int | str | '
                    '("map"|"import", name) | [spec, ...]')


_LAYOUTS = {}


def class_layout(cls, field_mapped, src_dir, pdb_path):
    """(orig {member: off}, ours {member: off}, field_map or None), memoized."""
    key = (cls, field_mapped, src_dir, pdb_path)
    if key not in _LAYOUTS:
        import layout
        off_o = layout.exe_offsets_dir(cls, src_dir)
        off_u = {n: o for n, (o, _s) in layout.our_offsets(pdb_path, cls).items()}
        fmap = layout.field_map(cls, src_dir, pdb_path).map if field_mapped else None
        _LAYOUTS[key] = (off_o, off_u, fmap)
    return _LAYOUTS[key]


def short_detail(text, limit=140):
    """First interesting lines of a run_case report, for the verdict line."""
    keep, past = [], False
    for line in text.splitlines():
        if line.startswith('  verdict:'):
            past = True
            continue
        if past and line.strip():
            keep.append(line.strip())
        if len(keep) >= 2:
            break
    out = ' ; '.join(keep)
    return (out[:limit] + '...') if len(out) > limit else out


# ---------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('addrs', nargs='*', metavar='ADDR',
                    help='case addresses to run (hex, e.g. 0x578970); default all')
    ap.add_argument('--list', action='store_true', help='list cases and exit')
    ap.add_argument('--orig', type=Path, default=DEFAULT_ORIG, help='original iwd2.exe')
    ap.add_argument('--ours', type=Path, default=DEFAULT_OURS,
                    help='our RelWithDebInfo build directory (iwd2r.exe/.map/.pdb)')
    ap.add_argument('--src', type=Path, default=HERE.parent.parent / 'src',
                    help='src/ with /* XXXX */ offset comments')
    ap.add_argument('--pdb', type=Path, default=None,
                    help='our .pdb (default OURS/iwd2r.pdb)')
    ap.add_argument('--json', type=Path, default=None, help='write results as JSON')
    ap.add_argument('--strict', action='store_true', help='keep stack/TIB writes in the diff')
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args(argv)

    cases = discover(HERE / 'cases')
    if args.list:
        for addr, name, _p in cases:
            mod = load_case(name, HERE / 'cases' / f'{name}.py')
            print(f'0x{addr:X} {mod.CASE["name"]} ({name})')
        return 0

    want = []
    for a in args.addrs:
        try:
            want.append(int(a, 16))
        except ValueError:
            print(f'emu: bad case address {a!r}', file=sys.stderr)
            return 2
    selected = [(a, n, p) for a, n, p in cases if not want or a in want]
    if want and len(selected) != len(want):
        known = {a for a, _n, _p in cases}
        print(f'emu: no case for {", ".join(hex(a) for a in want if a not in known)}',
              file=sys.stderr)
        return 2

    try:
        import machine
        import truth
        import layout                                    # noqa: F401 (sanity)
    except ImportError as e:
        print(f'emu: {e}\nUnicorn lives only in .venv-emu; set it up with:\n  {VENV_SETUP}',
              file=sys.stderr)
        return 2

    ours_exe = args.ours / 'iwd2r.exe'
    try:
        pe_o = machine.PeImage(args.orig)
        pe_u = machine.PeImage(ours_exe)
        publics = truth.parse_map((args.ours / 'iwd2r.map').read_text(errors='replace'))
        build_id = 'iwd2r.exe sha256:' + hashlib.sha256(ours_exe.read_bytes()).hexdigest()[:12]
    except (OSError, AssertionError) as e:
        print(f'emu: setup failure: {e}', file=sys.stderr)
        return 2
    pdb_path = args.pdb or (args.ours / 'iwd2r.pdb')

    counts = {'EQUAL': 0, 'DIFF': 0, 'ERROR': 0}
    json_out = {}

    for addr, name, path in selected:
        mod = load_case(name, path)
        case = mod.CASE
        try:
            for key, spec in case['extra'].items():
                if key.endswith('_ours') and isinstance(spec, int):
                    raise CaseError(f'extra[{key!r}]: ours-side addresses must be '
                                    '("map"|"import", name) specs, not literals (fix 2)')
            addrs = {k: resolve(v, machine, truth, publics, pe_u)
                     for k, v in case['extra'].items()}
            addrs['orig'] = case['exe']
            addrs['ours'] = resolve(case['ours'], machine, truth, publics, pe_u)
            if case.get('returns') not in ('void', 'u8', 'u16', 'u32', 'ptr'):
                raise CaseError('CASE["returns"] missing or invalid: declare '
                                "'void'|'u8'|'u16'|'u32'|'ptr' (fix 1)")
            ctx = {'sent': machine.SENT, 'off': None, 'field_map': None}
            if case['this_class']:
                off_o, off_u, fmap = class_layout(case['this_class'], case['field_mapped'],
                                                  args.src, pdb_path)
                missing = [m for m in case['members']
                           if m not in off_o or m not in off_u]
                if missing:
                    raise CaseError(f'{case["this_class"]}: members missing from '
                                    f'layout: {missing}')
                ctx['off'] = {'orig': off_o, 'ours': off_u}
                ctx['field_map'] = fmap
        except Exception as e:                           # setup failure for this case
            detail = f'{type(e).__name__}: {e}'
            print(f'0x{addr:X} {case.get("short", name)}: ERROR {detail}')
            counts['ERROR'] += len(mod.SETS)
            json_out[f'0x{addr:X}'] = {'name': case['name'], 'build': build_id,
                                       'sets': [{'set': k, 'verdict': 'ERROR', 'detail': detail}
                                                for k in mod.SETS]}
            continue

        set_results = []
        for setname, inp in mod.SETS.items():
            machine.BLK_PTR[0] = machine.BLK_LO
            buf = io.StringIO()
            verdict, detail = None, ''
            try:
                def b(side, i, pe, _b=mod.build, _a=addrs, _c=ctx):
                    return _b(side, i, pe, _a, _c)
                with contextlib.redirect_stdout(buf):
                    verdict = machine.run_case(f'{case["name"]} {setname}', b, inp,
                                               pe_o, pe_u, args.strict, args.verbose,
                                               field_map=ctx['field_map'],
                                               norm_vptr=case.get('vptr_norm', False),
                                               returns=case['returns'])
            except Exception as e:
                verdict, detail = 'ERROR', f'{type(e).__name__}: {e}'
            if verdict == 'ERROR' and not detail:
                detail = short_detail(buf.getvalue()) or 'see log above'
            if verdict.startswith('EQUAL'):
                counts['EQUAL'] += 1
            elif verdict == 'DIFF':
                counts['DIFF'] += 1
            else:
                counts['ERROR'] += 1
            set_results.append({'set': setname, 'verdict': verdict, 'detail': detail})
            if not verdict.startswith('EQUAL') or args.verbose:
                for line in buf.getvalue().splitlines():
                    print('  ' + line)
            line = f'0x{addr:X} {case["short"]} {setname}: {verdict}'
            if not verdict.startswith('EQUAL'):
                line += ' ' + (detail or short_detail(buf.getvalue()))
            print(line)
        json_out[f'0x{addr:X}'] = {'name': case['name'], 'build': build_id,
                                   'sets': set_results}

    total = sum(counts.values())
    print(f'emu: {len(selected)} cases, {total} sets: '
          f'{counts["EQUAL"]} EQUAL, {counts["DIFF"]} DIFF, {counts["ERROR"]} ERROR')
    if args.json:
        args.json.write_text(json.dumps(json_out, indent=1) + '\n')
        print(f'emu: wrote {args.json}')
    return 2 if counts['ERROR'] else (1 if counts['DIFF'] else 0)


if __name__ == '__main__':
    sys.exit(main())
