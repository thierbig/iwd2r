"""Tests for tools/emu/layout.py and the tools/emu/cases/* module contract.

Run from the repository root: python3 -I tests/test_emu.py

Everything here runs on the stdlib alone: header and PDB parsing go against
the small text fixtures under tests/fixtures/emu/, the PDB cache is exercised
with a stubbed llvm-pdbutil, and case modules are only imported (they import
machine.py, which needs Unicorn, inside build()).  The one Unicorn-needing
test skips cleanly unless run with .venv-emu/bin/python.
"""

import io
import json
import subprocess
import sys
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
EMU = ROOT / 'tools' / 'emu'
FIX = Path(__file__).resolve().parent / 'fixtures' / 'emu'
sys.path.insert(0, str(EMU))

import layout                                               # noqa: E402


HEADER = (FIX / 'emu_shapes.h').read_text()
PDB_TEXT = (FIX / 'types_emu.txt').read_text()

EXE_OFF_TFOO = {            # from the /* XXXX */ comments, pack(2)
    'm_flags': 0x0, 'm_barShort': 0x4,          # TBar base-relative, shifted
    'm_tag': 0x8, 'm_count': 0xA, 'm_bytes': 0xE, 'm_bar': 0x12,
    'm_exeOnly': 0x16, 'm_kind': 0x1A,
}
OURS_TFOO = {               # from the PDB, natural alignment
    'm_flags': (0x0, 4), 'm_barShort': (0x4, 2),
    'm_tag': (0x8, 1), 'm_count': (0xC, 4), 'm_bytes': (0x10, 4),
    'm_bar': (0x14, 4), 'm_pdb_only': (0x18, 2), 'm_kind': (0x1C, 4),
}


class HeaderParseTests(unittest.TestCase):
    """exe-side parsing of the /* XXXX */ offset comments."""

    def test_parse_header_classes(self):
        recs = layout.parse_header_classes(HEADER)
        self.assertEqual(recs['TBar']['bases'], [])
        self.assertEqual(recs['TFoo']['bases'], ['TBar'])
        self.assertTrue(recs['TFoo']['virtual'])
        self.assertEqual(                                     # methods/statics dropped
            [(hex(o), t, n, d) for o, t, n, d in recs['TFoo']['members']],
            [(hex(0x8), 'char', 'm_tag', []),
             (hex(0xA), 'int', 'm_count', []),
             (hex(0xE), 'char', 'm_bytes', ['4']),
             (hex(0x12), 'TBar*', 'm_bar', []),
             (hex(0x16), 'RESID', 'm_exeOnly', []),
             (hex(0x1A), 'TFooKind', 'm_kind', [])])

    def test_exe_offsets_flat(self):
        err = io.StringIO()          # m_kind's type is unknown by design
        with redirect_stderr(err):
            got = layout.exe_offsets(HEADER, 'TFoo')
        self.assertEqual(got, EXE_OFF_TFOO)
        self.assertIn("unknown member type 'TFooKind'", err.getvalue())

    def test_exe_offsets_base_from_other_header(self):
        """The base lives in another header, pulled via the `headers` index."""
        foo = HEADER[HEADER.index('class TFoo'):]
        with redirect_stderr(io.StringIO()):
            got = layout.exe_offsets(foo, 'TFoo', headers={'TBar': HEADER})
        self.assertEqual(got, EXE_OFF_TFOO)

    def test_exe_offsets_missing_base_warns_not_raises(self):
        """A base with no header (MFC CObject, template bases) is assumed
        empty; derived members keep their absolute comments."""
        bar = HEADER[:HEADER.index('class TFoo')]
        foo = 'class TDeep : public TMissing {\npublic:\n' \
              '    /* 0010 */ int m_deep;\n};\n'
        err = io.StringIO()
        with redirect_stderr(err):
            got = layout.exe_offsets(foo + bar, 'TDeep', headers={'TBar': HEADER})
        self.assertEqual(got, {'m_deep': 0x10})
        self.assertIn("base 'TMissing' has no header", err.getvalue())

    def test_exe_class_size_pack2(self):
        recs = layout.parse_header_classes(HEADER)
        resolve = lambda n: recs.get(n)
        # TBar: virtual (vfptr 4) + m_flags 4 + m_barShort 2, pack(2) -> 6
        self.assertEqual(layout.exe_class_size(recs['TBar'], resolve, str), 6)


class PdbParseTests(unittest.TestCase):
    """our-side parsing of `llvm-pdbutil dump -types` output."""

    @classmethod
    def setUpClass(cls):
        cls.table = layout._parse_pdb_types(PDB_TEXT)

    def test_forward_ref_loses_to_definition(self):
        self.assertEqual(self.table['classes']['TBar'], 0x1003)

    def test_our_flat_offsets_and_sizes(self):
        """LF_INDEX continuations, LF_BCLASS shifts, inline basic types,
        LF_POINTER/LF_ARRAY/LF_ENUM sizes."""
        err = io.StringIO()
        with redirect_stderr(err):
            flat = layout._our_flat(self.table, 'TFoo', str)
        self.assertEqual({n: (o, s) for n, o, s in flat}, OURS_TFOO)
        self.assertEqual(err.getvalue(), '')

    def test_tsize_forward_class_resolves_by_name(self):
        self.assertEqual(layout._tsize(self.table, 0x1002, str), 8)   # fwd TBar
        self.assertEqual(layout._tsize(self.table, 0x1007, str), 4)   # enum
        self.assertEqual(layout._tsize(self.table, 0x1000, str), 4)   # pointer
        self.assertEqual(layout._tsize(self.table, 0x1006, str), 4)   # array


class FieldMapTests(unittest.TestCase):
    """the exe <-> ours join."""

    def test_field_map_join(self):
        table = layout._parse_pdb_types(PDB_TEXT)
        with mock.patch.object(layout, 'load_pdb_types', return_value=table):
            fm = layout.field_map('TFoo', FIX, FIX / 'types_emu.txt', verbose=False)
        expected = {}
        for eo, oo, n in [(0x0, 0x0, 4), (0x4, 0x4, 2), (0x8, 0x8, 1),
                          (0xA, 0xC, 4), (0xE, 0x10, 4), (0x12, 0x14, 4)]:
            for i in range(n):
                expected[eo + i] = oo + i
        self.assertEqual(fm.map, expected)          # byte-granular, drifted
        self.assertEqual(fm.exe_only, ['m_exeOnly'])
        self.assertEqual(fm.ours_only, ['m_pdb_only'])
        self.assertNotIn(0x1A, fm.map)              # m_kind: exe size unknown


class CacheTests(unittest.TestCase):
    """the $XDG_CACHE_HOME/iwd2r/emu cache: key, hit, version invalidation."""

    def test_cache_path_keyed_by_size_and_mtime(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.dict('os.environ', {'XDG_CACHE_HOME': tmp}):
                pdb = Path(tmp) / 'fake.pdb'
                pdb.write_bytes(b'x' * 10)
                st = pdb.stat()
                p = layout._cache_path(pdb)
                self.assertEqual(p.parent, Path(tmp) / 'iwd2r' / 'emu')
                self.assertEqual(p.name, f'types-{st.st_size}-{int(st.st_mtime)}.json')

    def test_cache_hit_skips_pdbutil(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.dict('os.environ', {'XDG_CACHE_HOME': tmp}):
                pdb = Path(tmp) / 'fake.pdb'
                pdb.write_bytes(b'x')
                cache = layout._cache_path(pdb)
                cache.parent.mkdir(parents=True)
                cache.write_text(json.dumps({'_v': layout.CACHE_VERSION,
                                             'classes': {'TFoo': 0x1008}}))
                boom = mock.Mock(side_effect=AssertionError('pdbutil ran'))
                with mock.patch.object(layout.subprocess, 'run', boom):
                    table = layout.load_pdb_types(pdb)
                self.assertEqual(table['classes'], {'TFoo': 0x1008})

    def test_version_mismatch_invalidates_and_rewrites(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.dict('os.environ', {'XDG_CACHE_HOME': tmp}):
                pdb = Path(tmp) / 'fake.pdb'
                pdb.write_bytes(b'x')
                cache = layout._cache_path(pdb)
                cache.parent.mkdir(parents=True)
                cache.write_text(json.dumps({'_v': layout.CACHE_VERSION - 1}))
                done = subprocess.CompletedProcess([], 0, stdout=PDB_TEXT)
                with mock.patch.object(layout.subprocess, 'run',
                                       mock.Mock(return_value=done)) as ran:
                    table = layout.load_pdb_types(pdb)
                    self.assertEqual(ran.call_count, 1)
                    layout.load_pdb_types(pdb)          # now served from cache
                    self.assertEqual(ran.call_count, 1)
                self.assertEqual(table['_v'], layout.CACHE_VERSION)
                self.assertEqual(table['classes']['TFoo'], 0x1008)
                self.assertEqual(json.loads(cache.read_text())['_v'],
                                 layout.CACHE_VERSION)


class CaseContractTests(unittest.TestCase):
    """every tools/emu/cases/c*.py honors the runner's module contract.
    Needs no Unicorn: case modules import machine only inside build()."""

    REQUIRED = ('name', 'short', 'exe', 'ours', 'expect_rva', 'convention',
                'this_class', 'this_size', 'members', 'field_mapped',
                'extra', 'resolve', 'returns')

    def test_case_modules(self):
        import run                                                   # tools/emu
        cases = run.discover(EMU / 'cases')
        self.assertGreaterEqual(len(cases), 3)
        for _addr, name, path in cases:
            mod = run.load_case(name, path)
            for key in self.REQUIRED:
                self.assertIn(key, mod.CASE, f'{name}: CASE[{key!r}]')
            self.assertIsInstance(mod.CASE['exe'], int)
            self.assertIn(mod.CASE['returns'], ('void', 'u8', 'u16', 'u32', 'ptr'),
                          f"{name}: CASE['returns'] must declare the EAX contract")
            self.assertIn(mod.CASE['convention'], ('thiscall', 'cdecl'))
            self.assertIsInstance(mod.CASE['this_size'], int)
            self.assertGreater(mod.CASE['this_size'], 0)
            self.assertIsInstance(mod.CASE['members'], tuple)
            self.assertTrue(all(isinstance(m, str) for m in mod.CASE['members']))
            for key, (cname, rva) in mod.CASE['resolve'].items():
                self.assertIsInstance(cname, str, f'{name}: resolve[{key!r}]')
                self.assertIsInstance(rva, int, f'{name}: resolve[{key!r}]')
            self.assertTrue(mod.SETS, f'{name}: SETS empty')
            self.assertTrue(callable(mod.build), f'{name}: build')


class UnicornGatedTests(unittest.TestCase):
    """machine.py needs Unicorn, which lives only in .venv-emu: run with
    .venv-emu/bin/python -I tests/test_emu.py.  Skips cleanly otherwise."""

    def setUp(self):
        try:
            import unicorn                                        # noqa: F401
        except ImportError:
            self.skipTest('unicorn only in .venv-emu')

    def test_machine_imports_and_unicorn_runs(self):
        import machine
        for name in ('IMAGE_BASE', 'STACK_LO', 'BLK_LO', 'RET_MAGIC'):
            self.assertIsInstance(getattr(machine, name), int)
        self.assertEqual(set(machine.SENT), {'anim', 'shadow', 'vidcell'})
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
        from unicorn.x86_const import UC_X86_REG_EAX
        base = machine.RET_MAGIC + 0x100000      # spare page, same machine map
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, 0x1000)
        uc.mem_write(base, b'\xb8\x05\x00\x00\x00')          # mov eax, 5
        uc.emu_start(base, base + 5)
        self.assertEqual(uc.reg_read(UC_X86_REG_EAX), 5)

    ORIG = Path.home() / '.local' / 'share' / 'iwd2r' / 'vanilla' / 'iwd2.exe'

    def test_stub_ret_ecx_and_rets_sequence(self):
        """ret:'ecx' hands back the caller's this; 'rets' scripts a per-call
        return sequence that exhausts to 0.  Drives _apply_stub directly."""
        import struct
        import machine
        from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EAX
        if not self.ORIG.exists():
            self.skipTest('original exe not present')
        m = machine.Machine('orig', machine.PeImage(self.ORIG), machine.STUB_PAGE,
                            'cdecl', machine.BLK_LO, 0x40, [])
        esp = machine.ESP0 - 0x10
        m.uc.mem_write(esp, struct.pack('<I', machine.RET_MAGIC))
        m.uc.reg_write(UC_X86_REG_ESP, esp)
        m.uc.reg_write(UC_X86_REG_ECX, 0x1234)
        m.add_stub({'name': 'ctor', 'kind': 'code', 'addr': machine.STUB_PAGE,
                    'conv': 'thiscall', 'nargs': 0, 'pop': 0, 'ret': 'ecx'})
        spec = m.stubs[machine.STUB_PAGE]
        m._apply_stub(spec)
        self.assertEqual(m.calls[-1][2], '0x00001234')       # logged ret = ECX
        self.assertEqual(m.uc.reg_read(UC_X86_REG_EAX), 0x1234)
        m.add_stub({'name': 'get', 'kind': 'code', 'addr': machine.STUB_PAGE + 0x10,
                    'conv': 'cdecl', 'nargs': 1, 'pop': 0, 'rets': [7, 8]})
        spec2 = m.stubs[machine.STUB_PAGE + 0x10]
        rets = []
        for _ in range(3):
            m._apply_stub(spec2)
            rets.append(m.calls[-1][2])
        self.assertEqual(rets, ['0x00000007', '0x00000008', '0x00000000'])


if __name__ == '__main__':
    unittest.main()
