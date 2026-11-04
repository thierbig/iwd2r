#!/usr/bin/env python3
"""The function-independent Unicorn core of the differential emulator.

Moved from the Task 72 spike (tools/emu/emu_diff.py) unchanged: for one
function it maps a whole PE image at its ImageBase, gives it a stack, a
deterministic bump scratch heap, a zeroed `this` block and arg blocks, sets
the arguments per calling convention, runs until return, and records

  (a) every memory write as (region, offset, size, value),
  (b) every outgoing call (stubbed, or watched while really running) with its
      args per the callee's convention,
  (c) the return value (EAX).

run_case() runs the same case on both images and prints the verdict.  The
paths (original exe, our build) are parameters here; cases live in
tools/emu/cases/, and tools/oracle.py's `emu` subcommand drives this module
through the .venv-emu interpreter.
"""

import struct

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UcError
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP,
    UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS, UC_X86_REG_FS,
    UC_X86_REG_GDTR,
)

IMAGE_BASE = 0x400000
# scratch regions sit above the largest image (ours spans ~0x102c000)
STACK_LO, STACK_SZ = 0x10000000, 0x00100000
HEAP_LO, HEAP_SZ = 0x11000000, 0x00100000
BLK_LO, BLK_SZ = 0x12000000, 0x00010000
TIB_LO = 0x7FF9E000
RET_MAGIC = 0x60000000       # emu_start() until-address; never executed
STUB_PAGE = 0x61000000       # address space for IAT-slot stub redirects
GDT_LO = 0x62000000          # x86-32 has no FS_BASE reg; point FS at the TIB via a GDT
ESP0 = STACK_LO + STACK_SZ - 0x1000
HEAP_BUMP0 = HEAP_LO + 0x1000
BLK_PTR = [BLK_LO]           # bump cursor for this/args/sentinel blocks

# fixed sentinel object addresses (inputs; never dereferenced by stubs)
SENT = {'anim': BLK_LO + 0x8000, 'shadow': BLK_LO + 0x9000, 'vidcell': BLK_LO + 0xA000}


def blk_alloc(size):
    addr = (BLK_PTR[0] + 0xF) & ~0xF
    BLK_PTR[0] = addr + size
    assert BLK_PTR[0] < SENT['anim'], 'block area exhausted'
    return addr


# ---------------------------------------------------------------- PE loading

class PeImage:
    def __init__(self, path):
        self.path = path
        self.data = path.read_bytes()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        assert d[pe:pe + 4] == b'PE\0\0', f'{path}: not a PE'
        machine, nsec, _, _, _, opt_size, _ = struct.unpack_from('<HHIIIHH', d, pe + 4)
        assert machine == 0x14C, f'{path}: not i386'
        opt = pe + 24
        magic = struct.unpack_from('<H', d, opt)[0]
        assert magic == 0x10B, f'{path}: not PE32'
        self.image_base = struct.unpack_from('<I', d, opt + 28)[0]
        assert self.image_base == IMAGE_BASE, hex(self.image_base)
        self.sections = []
        so = opt + opt_size
        for i in range(nsec):
            name, vsize, va, rsize, roff = struct.unpack_from('<8sIIII', d, so + 40 * i)
            self.sections.append((name.rstrip(b'\0').decode(), va, vsize, rsize, roff))
        self.span = max(va + max(vsize, rsize) for _, va, vsize, rsize, _ in self.sections)
        self.span = (self.span + 0xFFF) & ~0xFFF

    def section_of(self, va):
        for name, sva, vsize, rsize, _ in self.sections:
            if sva <= va < sva + max(vsize, rsize):
                return name
        return None


# ---------------------------------------------------------------- one machine

class Machine:
    """One Unicorn instance running one side (original or ours) of one case.

    arg_blocks: [(name, addr, size, mode)] where mode is 'ref' (the 4-byte
    address of a memory block is pushed) or 'val' (the block holds one dword
    that is pushed by value).  Every x86 argument push here is 4 bytes.
    """

    def __init__(self, side, pe, entry, conv, this_addr, this_size, arg_blocks):
        self.side, self.pe, self.entry, self.conv = side, pe, entry, conv
        self.this_addr, self.this_size = this_addr, this_size
        self.arg_blocks = arg_blocks
        self.writes = []                      # (region, offset, size, value)
        self.calls = []                       # [name, [norm args], ret, inline_ok]
        self.ret = None
        self.error = None
        self.bump = HEAP_BUMP0
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self._build()

    # -- setup ------------------------------------------------------------
    @staticmethod
    def _gdt_desc(base, limit, access, gran):
        b = bytearray(8)
        b[0] = limit & 0xFF
        b[1] = (limit >> 8) & 0xFF
        b[2] = base & 0xFF
        b[3] = (base >> 8) & 0xFF
        b[4] = (base >> 16) & 0xFF
        b[5] = access
        b[6] = ((limit >> 16) & 0x0F) | (gran & 0xF0)
        b[7] = (base >> 24) & 0xFF
        return bytes(b)

    def _build(self):
        uc = self.uc
        base = self.pe.image_base
        uc.mem_map(base, self.pe.span)
        uc.mem_write(base, self.pe.data[:0x1000])          # headers page
        for _, va, vsize, rsize, roff in self.pe.sections:
            if rsize:
                uc.mem_write(base + va, self.pe.data[roff:roff + rsize])
        uc.mem_map(STACK_LO, STACK_SZ)
        uc.mem_map(HEAP_LO, HEAP_SZ)
        uc.mem_map(BLK_LO, BLK_SZ)
        uc.mem_map(TIB_LO, 0x1000)
        uc.mem_write(TIB_LO, b'\xff' * 4)                  # SEH chain end
        uc.mem_map(RET_MAGIC, 0x1000)
        uc.mem_map(STUB_PAGE, 0x1000)
        uc.mem_map(GDT_LO, 0x1000)
        flat = self._gdt_desc(0, 0xFFFFF, 0x9A, 0xC0)       # [1] code, base 0, 32-bit
        flat += self._gdt_desc(0, 0xFFFFF, 0x92, 0xC0)      # [2] data/stack, base 0
        flat += self._gdt_desc(TIB_LO, 0xFFFFF, 0x92, 0xC0)  # [3] fs -> TIB
        uc.mem_write(GDT_LO, self._gdt_desc(0, 0, 0, 0) + flat)
        uc.reg_write(UC_X86_REG_GDTR, (0, GDT_LO, 31, 0))   # selector, base, limit, flags
        uc.reg_write(UC_X86_REG_CS, 0x08)
        uc.reg_write(UC_X86_REG_DS, 0x10)
        uc.reg_write(UC_X86_REG_ES, 0x10)
        uc.reg_write(UC_X86_REG_SS, 0x10)                   # B=1: 32-bit stack, no 64K wrap
        uc.reg_write(UC_X86_REG_FS, 0x18)                   # selector for entry 3
        for _, addr, size, _ in self.arg_blocks:
            uc.mem_write(addr, b'\0' * size)
        uc.mem_write(self.this_addr, b'\0' * self.this_size)
        self.heap_init = bytes(uc.mem_read(HEAP_LO, HEAP_SZ))
        self.block_init = {n: bytes(uc.mem_read(a, s)) for n, a, s, _ in self.arg_blocks}
        self.this_init = bytes(uc.mem_read(self.this_addr, self.this_size))

        self.stubs = {}                                    # hook addr -> spec
        self.watch = {}                                    # addr -> spec
        self._retseq = {}                                  # hook addr -> next 'rets' index
        self.icount = 0
        uc.hook_add(UC_HOOK_CODE, self._on_code)
        uc.hook_add(UC_HOOK_MEM_WRITE, self._on_write)

    def set_this(self, off, data):
        self.uc.mem_write(self.this_addr + off, data)

    def set_arg(self, name, off, data):
        addr = next(a for n, a, _s, _m in self.arg_blocks if n == name)
        self.uc.mem_write(addr + off, data)

    def preseed_heap(self, size, content, refcount):
        """One deterministic pre-run allocation; returns the data pointer."""
        addr = self._bump(size)
        buf = bytes([refcount & 0xFF]) + content
        self.uc.mem_write(addr, buf.ljust(size, b'\0'))
        return addr + 1

    def _bump(self, size):
        addr = (self.bump + 7) & ~7
        self.bump = addr + size
        assert self.bump < HEAP_LO + HEAP_SZ, 'scratch heap exhausted'
        return addr

    def add_stub(self, spec):
        if spec.get('kind') == 'iat':                      # call goes through [slot]
            magic = STUB_PAGE + 0x10 * (1 + len(self.stubs))
            self.uc.mem_write(spec['addr'], struct.pack('<I', magic))
            spec = dict(spec, hook_addr=magic)
        else:
            spec = dict(spec, hook_addr=spec['addr'])
        self.stubs[spec['hook_addr']] = spec

    def add_watch(self, spec):
        self.watch[spec['addr']] = spec

    # -- run ----------------------------------------------------------------
    def start(self):
        uc = self.uc
        esp = ESP0
        # push args right-to-left (each is one 4-byte push), then sentinel ret
        for name, addr, _size, mode in reversed(self.arg_blocks):
            v = addr if mode == 'ref' else struct.unpack('<I', uc.mem_read(addr, 4))[0]
            esp -= 4
            uc.mem_write(esp, struct.pack('<I', v))
        esp -= 4
        uc.mem_write(esp, struct.pack('<I', RET_MAGIC))
        uc.reg_write(UC_X86_REG_ESP, esp)
        self.entry_esp = esp                                # ESP at entry: [ESP]=ret, [ESP+4]=arg0
        if self.conv == 'thiscall':
            uc.reg_write(UC_X86_REG_ECX, self.this_addr)
        uc.reg_write(UC_X86_REG_EBP, ESP0)
        try:
            uc.emu_start(self.entry, RET_MAGIC, count=2_000_000)
        except UcError as e:
            self.error = f'{e} at eip=0x{uc.reg_read(UC_X86_REG_EIP):08x} (icount={self.icount})'
            return
        if uc.reg_read(UC_X86_REG_EIP) != RET_MAGIC:
            self.error = f'instruction budget exhausted (icount={self.icount})'
            return
        self.ret = uc.reg_read(UC_X86_REG_EAX)

    # -- hooks ---------------------------------------------------------------
    def _region(self, addr):
        if STACK_LO <= addr < STACK_LO + STACK_SZ:
            return 'stack', addr - self.entry_esp
        if self.this_addr <= addr < self.this_addr + self.this_size:
            return 'this', addr - self.this_addr
        for n, a, s, _m in self.arg_blocks:
            if a <= addr < a + s:
                return 'arg:' + n, addr - a
        if HEAP_LO <= addr < HEAP_LO + HEAP_SZ:
            return 'heap', addr - HEAP_LO
        if TIB_LO <= addr < TIB_LO + 0x1000:
            return 'tib', addr - TIB_LO
        if self.pe.image_base <= addr < self.pe.image_base + self.pe.span:
            return 'image', addr - self.pe.image_base
        return 'other', addr

    def _rec_write(self, addr, size, value):
        region, off = self._region(addr)
        self.writes.append((region, off, size, value & ((1 << (size * 8)) - 1)))

    def _rec_write_bytes(self, addr, data):
        """Record a synthetic (stub-emulated) write of arbitrary bytes."""
        for i in range(0, len(data), 4):
            chunk = data[i:i + 4].ljust(4, b'\0')
            self._rec_write(addr + i, 4, struct.unpack('<I', chunk)[0])
        if len(data) % 4:
            pass  # tail handled by the ljust chunk above (over-recorded by <=3 bytes)

    def _on_write(self, uc, access, addr, size, value, ud):
        self._rec_write(addr, size, value)

    def _norm(self, v):
        if STACK_LO <= v < STACK_LO + STACK_SZ:
            return 'STACK*'
        if HEAP_LO <= v < HEAP_LO + HEAP_SZ:
            return 'heap+0x%x' % (v - HEAP_LO)
        if BLK_LO <= v < BLK_LO + BLK_SZ:
            if v == self.this_addr:
                return 'this'
            for n, a, _s, _m in self.arg_blocks:
                if v == a:
                    return 'arg:' + n
            for k, s in SENT.items():
                if v == s:
                    return 'sent:' + k
            return 'blk+0x%x' % (v - BLK_LO)
        return '0x%08x' % v

    def _stack_args(self, esp, n):
        return [struct.unpack('<I', self.uc.mem_read(esp + 4 + 4 * i, 4))[0] for i in range(n)]

    def _on_code(self, uc, addr, size, ud):
        self.icount += 1
        if addr == RET_MAGIC:
            uc.emu_stop()
            return
        spec = self.stubs.get(addr)
        if spec is not None:
            self._apply_stub(spec)
            return
        w = self.watch.get(addr)
        if w is not None:
            esp = uc.reg_read(UC_X86_REG_ESP)
            raw = ([uc.reg_read(UC_X86_REG_ECX)] if w['conv'] == 'thiscall' else []) \
                + self._stack_args(esp, w['nargs'])
            self.calls.append([w['name'], [self._norm(a) for a in raw], None,
                               w.get('inline_ok', False)])

    def _apply_stub(self, spec):
        uc = self.uc
        esp = uc.reg_read(UC_X86_REG_ESP)
        retaddr = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        stack_args = self._stack_args(esp, spec['nargs'])
        raw = ([uc.reg_read(UC_X86_REG_ECX)] if spec['conv'] == 'thiscall' else []) + stack_args
        action = spec.get('action', 'plain')
        retval = spec.get('ret', 0)

        if action == 'bump_new':
            size = stack_args[0]
            ptr = self._bump(size)
            if size:
                uc.mem_write(ptr, b'\0' * size)
                self._rec_write_bytes(ptr, b'\0' * size)
            retval = ptr
        elif action == 'copy':
            dst, src, n = stack_args[0], stack_args[1], stack_args[2]
            data = bytes(uc.mem_read(src, n)) if n else b''
            if n:
                uc.mem_write(dst, data)
                self._rec_write_bytes(dst, data)
            retval = dst
        elif action == 'free':
            retval = 0
        elif action == 'write_args':
            # e.g. SetRect(lprc, lprc+1..): store args[1:] as dwords through args[0]
            ptr = stack_args[0]
            for i, dw in enumerate(stack_args[1:5]):
                uc.mem_write(ptr + 4 * i, struct.pack('<I', dw & 0xFFFFFFFF))
                self._rec_write(ptr + 4 * i, 4, dw)
            retval = spec.get('ret', 1)
        else:
            resp = spec.get('ret_by_ecx')
            if resp is not None:
                r = resp.get(uc.reg_read(UC_X86_REG_ECX), spec.get('default', {}))
            else:
                r = spec.get('default', {})
            retval = r.get('ret', 0)
            for slot, dwords in r.get('writes', []):
                ptr = stack_args[slot]
                for i, dw in enumerate(dwords):
                    uc.mem_write(ptr + 4 * i, struct.pack('<I', dw & 0xFFFFFFFF))
                    self._rec_write(ptr + 4 * i, 4, dw)

        # spec['ret'] == 'ecx': hand back whatever ECX holds now -- for a
        # thiscall stub that is the caller's `this`, i.e. "ctor returns this".
        if spec.get('ret') == 'ecx':
            retval = uc.reg_read(UC_X86_REG_ECX)
        # spec['rets'] = [v1, v2, ...]: scripted per-call return sequence
        # (exhausts to 0), e.g. a GetAbility(i) that must vary with i.
        seq = spec.get('rets')
        if seq is not None:
            i = self._retseq.get(spec['hook_addr'], 0)
            retval = seq[i] if i < len(seq) else 0
            self._retseq[spec['hook_addr']] = i + 1

        self.calls.append([spec['name'], [self._norm(a) for a in raw], self._norm(retval),
                           spec.get('inline_ok', False)])
        uc.reg_write(UC_X86_REG_ESP, esp + 4 + spec.get('pop', 0))
        uc.reg_write(UC_X86_REG_EAX, retval & 0xFFFFFFFF)
        uc.reg_write(UC_X86_REG_EIP, retaddr)


# ---------------------------------------------------------------- diff engine

def final_state(m, writes=None):
    """Replay the write log onto the initial region snapshots."""
    snaps = [('this', m.this_init)] + [('arg:' + n, b) for n, b in m.block_init.items()] \
        + [('heap', m.heap_init)]
    writes = m.writes if writes is None else writes
    out = {}
    for name, init in snaps:
        buf = bytearray(init)
        for region, off, size, value in writes:
            if region != name:
                continue
            for i in range(size):
                if 0 <= off + i < len(buf):
                    buf[off + i] = (value >> (8 * i)) & 0xFF
        out[name] = bytes(buf)
    return out


def first_diffs(a, b, n=6):
    out = []
    for i in range(max(len(a), len(b))):
        x = a[i] if i < len(a) else None
        y = b[i] if i < len(b) else None
        if x != y:
            out.append((i, x, y))
            if len(out) >= n:
                break
    return out


def state_diff(sa, sb):
    diffs = []
    for k in sa:
        if sa[k] != sb[k]:
            offs = [i for i in range(len(sa[k])) if sa[k][i] != sb[k][i]][:8]
            diffs.append((k, offs))
    return diffs


def run_case(name, build, inp, pe_o, pe_u, strict=False, verbose=False, field_map=None,
             norm_vptr=False):
    """field_map: {orig this-offset: ours this-offset} — reconcile known class-layout
    divergence between the VC6 original and our MSVC rebuild before comparing.
    norm_vptr: constructors of virtual classes store their own image's vtable
    pointer into `this`; the two images can never agree on that value, so zero
    any own-image pointer written into `this` — the comparison then asks "was
    a vptr stored, and at which offset", not "is it the same address"."""
    print(f'--- {name} ---')
    mo, mu = build('orig', inp, pe_o), build('ours', inp, pe_u)
    mo.start()
    mu.start()
    for m, tag in ((mo, 'orig'), (mu, 'ours')):
        if m.error:
            print(f'  {tag}: ERROR {m.error} writes={len(m.writes)} calls={len(m.calls)}')
            return 'ERROR'

    co_f = [c for c in mo.calls if not c[3]]
    cu_f = [c for c in mu.calls if not c[3]]
    inline_notes = sorted({c[0] for c in mo.calls + mu.calls if c[3]})
    calls_eq = co_f == cu_f

    def keep(w):
        return True if strict else w[0] not in ('stack', 'tib')
    wo = [w for w in mo.writes if keep(w)]
    wu = [w for w in mu.writes if keep(w)]
    if norm_vptr:
        def zap(m, w):
            r, o, s, v = w
            if r == 'this' and m.pe.image_base <= v < m.pe.image_base + m.pe.span:
                return (r, o, s, 0)
            return w
        wo = [zap(mo, w) for w in wo]
        wu = [zap(mu, w) for w in wu]
    if field_map:
        wo = [(r, field_map.get(o, o) if r == 'this' else o, s, v) for r, o, s, v in wo]
    seq_eq = wo == wu

    so, su = final_state(mo, wo), final_state(mu, wu)
    sd = state_diff(so, su)
    if field_map and not sd and calls_eq:
        verdict = 'EQUAL(layout)' if seq_eq else 'EQUAL(state,layout)'
    else:
        verdict = ('EQUAL' if seq_eq else 'EQUAL(state)') if (not sd and calls_eq) else 'DIFF'

    print(f'  verdict: {verdict}   writes orig={len(mo.writes)} ours={len(mu.writes)} '
          f'(kept {len(wo)}/{len(wu)}{" strict" if strict else ""})   '
          f'calls {len(co_f)}/{len(cu_f)}   icount {mo.icount}/{mu.icount}')
    print(f'  ret: orig=0x{mo.ret:08x} ours=0x{mu.ret:08x}')
    if inline_notes:
        print(f'  inline-ok one-sided (ignored): {inline_notes}')
    if not calls_eq:
        print('  call diff (first 6):')
        for i, x, y in first_diffs(co_f, cu_f):
            print(f'    [{i}] orig {x}')
            print(f'        ours {y}')
    if not seq_eq:
        print('  write-seq diff (first 6):')
        for i, x, y in first_diffs(wo, wu):
            print(f'    [{i}] orig {x}')
            print(f'        ours {y}')
    if sd:
        print('  final-state diff:')
        for k, offs in sd:
            print(f'    {k}: differing bytes at {["0x%x" % o for o in offs]}')
            print(f'      orig writes({k}): {[w for w in mo.writes if w[0] == k][:12]}')
            print(f'      ours writes({k}): {[w for w in mu.writes if w[0] == k][:12]}')
    if verbose:
        print('  orig calls:')
        for c in mo.calls:
            print('   ', c)
        print('  ours calls:')
        for c in mu.calls:
            print('   ', c)
        print('  orig writes (kept):')
        for w in wo:
            print('   ', w)
        print('  ours writes (kept):')
        for w in wu:
            print('   ', w)
    return verdict
