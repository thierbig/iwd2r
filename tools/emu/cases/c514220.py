"""0x514220 CAIScript::SetResRef(this, CResRef) -- pass-by-value CResRef.

The CResRef argument is eight bytes on the stack (two dwords: two 'val'
arg blocks, ref_d0 first so [esp+4] is its low dword).  Both images
immediately call CResRef::operator= -- neither caller reloads ECX, and the
callee body writes through [ecx], so the destination CResRef rides in ECX
(= the caller's `this`); the two pushed dwords are callee-popped (the
vanilla caller does `add esp,8` AFTER a ret-8 callee, ours relies on
`mov esp,ebp`).  The stack shapes normalize to 'STACK*' identically, then
both return 8.  Ours additionally runs its (real) security cookie epilogue.

operator= is stubbed ret 0 and NOT inline_ok: the argument shapes match
exactly, and keeping the call visible pins that both sides copy rather
than fuse the assignment.  No kept writes at all, so the verdict should be
a plain EQUAL.
"""

import struct

CASE = {
    'name': 'CAIScript::SetResRef', 'short': 'CAIScript::SetResRef',
    'exe': 0x514220, 'ours': 'CAIScript::SetResRef', 'expect_rva': 0x76670,
    'convention': 'thiscall', 'this_class': None, 'this_size': 4,
    'members': (), 'field_mapped': False,
    'extra': {'opeq_orig': 0x78AC30, 'opeq_ours': 0x409CCD},
    'resolve': {},
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(4)
    d0 = blk_alloc(4)
    d1 = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 4,
                [('ref_d0', d0, 4, 'val'), ('ref_d1', d1, 4, 'val')])
    m.set_arg('ref_d0', 0, struct.pack('<I', inp['ref_d0']))
    m.set_arg('ref_d1', 0, struct.pack('<I', inp['ref_d1']))
    m.add_stub({'name': 'CResRef::operator=', 'kind': 'code',
                'addr': addrs['opeq_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0}})
    return m


SETS = {
    'set1-copy': dict(ref_d0=0x11223344, ref_d1=0x005566AA),
}
