"""0x4F6B60 CMessageUnsummon::ctor(summonedId, caller, target).

Vanilla is a flat thiscall ret 0xC writing four dwords, the base CMessage
ctor fully inlined by VC6 (its two LONG stores folded in):
    [this+8]=arg2 (caller)  [this+4]=arg3 (target)
    [this+0xC]=arg1 (summonedId)  [this]=vptr 0x84C3A8
Our rebuild delegates to the real CMessage base ctor (thunk 0x40C3F6
runs for real, same as c4f6bc0/c4f6c50) and then stores m_summonedId and
its own vptr -- same final state, so the expected verdict is a
state-level equal ('EQUAL(state,layout)') rather than write-order EQUAL:
codegen drift, not a behavioral difference (the c4f6bc0 precedent).

vptr_norm is on because the two images necessarily store different
own-image vptr values into this+0 (vanilla 0x84C3A8, ours 0xBFC7D0).
m_targetId/m_sourceId/m_summonedId sit at identical offsets 0x4/0x8/0xC
on both sides (identity field map, like c4f6bc0); arg order is the
declaration order (src/CMessage.h:1519): (summonedId, caller, target)
so that [esp+4]=summonedId as vanilla expects.

Returns 'ptr' -- a ctor has no declared return, but both images hand
`this` back in EAX (orig `mov eax,ecx` at 0x4F6B64, ours `mov eax,esi`
at 0x644B2B), so the case declares 'ptr': both sides must return the
same normalized `this`.
"""

import struct

CASE = {
    'name': 'CMessageUnsummon::CMessageUnsummon',
    'short': 'CMessageUnsummon::ctor',
    'exe': 0x4F6B60, 'ours': 'CMessageUnsummon::CMessageUnsummon',
    'convention': 'thiscall', 'this_class': 'CMessageUnsummon',
    'this_size': 0x10,
    'members': ('m_targetId', 'm_sourceId', 'm_summonedId'),
    'field_mapped': True, 'vptr_norm': True,
    'extra': {},
    'returns': 'ptr',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x10)
    a1 = blk_alloc(4)
    a2 = blk_alloc(4)
    a3 = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x10,
                [('summonedId', a1, 4, 'val'),
                 ('caller', a2, 4, 'val'),
                 ('target', a3, 4, 'val')])
    m.set_arg('summonedId', 0, struct.pack('<i', inp['summonedId']))
    m.set_arg('caller', 0, struct.pack('<i', inp['caller']))
    m.set_arg('target', 0, struct.pack('<i', inp['target']))
    return m


SETS = {
    'set1-plain': dict(summonedId=0x111, caller=0x222, target=0x333),
    'set2-negative': dict(summonedId=-1, caller=-700, target=-3),
}
