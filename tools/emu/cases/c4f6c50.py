"""0x4F6C50 CMessage97::ctor(caller, target, nScript, const CResRef&).

Vanilla is a flat thiscall ret 0x10 writing five dwords, the base CMessage
ctor fully inlined (VC6 -- its two LONG stores are folded in):
    [this+8]=arg1 (caller)  [this+0xC]=arg3 (nScript)
    [this+4]=arg2 (target)  [this]=vptr 0x84D398
    [this+0x10]/[this+0x14] = the two resref dwords (arg4, by reference)
Our rebuild delegates to the real CMessage base ctor (thunk 0x40C3F6
runs for real, same as c4f6bc0) and then stores m_nScript, its own vptr
and the resref copy -- same final state, so the expected verdict is a
state-level equal ('EQUAL(state,layout)') rather than write-order EQUAL:
codegen drift, not a behavioral difference (the c4f6bc0 precedent).

vptr_norm is on because the two images necessarily store different
own-image vptr values into this+0 (vanilla 0x84D398, ours 0xBFC8C0).
m_targetId/m_sourceId/m_nScript/m_resScript sit at identical offsets
0x4/0x8/0xC/0x10 on both sides (identity field map, like c4f6bc0);
arg order is the declaration order: (caller, target, nScript, resref)
so that [esp+4]=caller as vanilla expects.

Returns 'ptr' -- a ctor has no declared return, but both images hand
`this` back in EAX (orig `mov eax,ecx` folded into the store order at
0x4F6C54, ours `mov eax,esi` at 0x641ACC), so the case declares 'ptr':
both sides must return the same normalized `this`.

The resref rides as an 8-byte 'ref' arg block (by reference, as the
signature says): vanilla loads both dwords from [esp+0x10] (a pointer),
ours from [ebp+0x14] (same pointer); the block is only ever read, so its
write region stays empty.
"""

import struct

CASE = {
    'name': 'CMessage97::CMessage97',
    'short': 'CMessage97::ctor',
    'exe': 0x4F6C50, 'ours': 'CMessage97::CMessage97',
    'convention': 'thiscall', 'this_class': 'CMessage97',
    'this_size': 0x18,
    'members': ('m_targetId', 'm_sourceId', 'm_nScript', 'm_resScript'),
    'field_mapped': True, 'vptr_norm': True,
    'extra': {},
    'returns': 'ptr',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x18)
    a1 = blk_alloc(4)
    a2 = blk_alloc(4)
    a3 = blk_alloc(4)
    a4 = blk_alloc(8)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x18,
                [('caller', a1, 4, 'val'),
                 ('target', a2, 4, 'val'),
                 ('nScript', a3, 4, 'val'),
                 ('resref', a4, 8, 'ref')])
    m.set_arg('caller', 0, struct.pack('<i', inp['caller']))
    m.set_arg('target', 0, struct.pack('<i', inp['target']))
    m.set_arg('nScript', 0, struct.pack('<i', inp['nScript']))
    m.set_arg('resref', 0, inp['resref'])
    return m


SETS = {
    'set1-plain': dict(caller=0x222, target=0x333, nScript=0x2A5A5,
                       resref=b'GUIClient'),
    'set2-negative': dict(caller=-1, target=-700, nScript=-3,
                          resref=b'SPWIN9\xff\xff'),
}
