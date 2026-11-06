"""0x551520 CVEFVidCell::SetDirection(this, pos, oldPos, targetPos, nDirection).

m_directionFlags & 1: pos == targetPos keeps m_direction, else
CGameSprite::GetDirection (static, cdecl, two CPoint refs scaled to
y*4/3 -- vanilla 0x706DD0, ours the map-named static overload).
m_directionFlags & 2: m_newDirection = nDirection.  Neither bit:
oldPos == pos keeps m_direction, else GetDirection(oldPos, pos).
The single observable write is m_newDirection at the field-mapped
offset (vanilla 0x33C), a dword store (LONG member) of a sign-extended
SHORT on the call paths (vanilla 0x5515D3, 0x5515FC, 0x551626).

nDirection is pushed as a dword and read as a word (vanilla
[esp+0x2C]); the three CPoints ride as refs, so 'val' vs 'ref' is
(4-byte val, ref, ref, ref).  The GetDirection stub returns the
per-set gdir; its two stack args are refs to the scaled temporaries
('STACK*' both sides).

Returns 'void' — `void SetDirection(...)` (src/CVisualEffect.h:100);
EAX ignored (vanilla `ret 0x10` at 0x551646).
"""

import struct

CASE = {
    'name': 'CVEFVidCell::SetDirection',
    'short': 'VidCell::SetDirection',
    'exe': 0x551520,
    'ours': 'CVEFVidCell::SetDirection',
    'convention': 'thiscall',
    'this_class': 'CVEFVidCell',
    'this_size': 0x348,
    'members': ('m_pos', 'm_direction', 'm_directionFlags', 'm_newDirection'),
    'field_mapped': True,
    'extra': {'gdir_orig': 0x706DD0,
              'gdir_ours': ('map', '?GetDirection@CGameSprite@@SAFABVCPoint@@0@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    off = ctx['off'][side]
    this = blk_alloc(0x348)
    pos = blk_alloc(8)
    old = blk_alloc(8)
    target = blk_alloc(8)
    ndir = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x348,
                [('pos', pos, 8, 'ref'), ('oldPos', old, 8, 'ref'),
                 ('targetPos', target, 8, 'ref'), ('nDirection', ndir, 4, 'val')])
    m.set_arg('pos', 0, struct.pack('<ii', *inp['pos']))
    m.set_arg('oldPos', 0, struct.pack('<ii', *inp['oldPos']))
    m.set_arg('targetPos', 0, struct.pack('<ii', *inp['targetPos']))
    m.set_arg('nDirection', 0, struct.pack('<i', inp['nDirection']))

    m.set_this(off['m_pos'] + 0, struct.pack('<i', inp['mpos'][0]))
    m.set_this(off['m_pos'] + 4, struct.pack('<i', inp['mpos'][1]))
    m.set_this(off['m_direction'], struct.pack('<i', inp['mdirection']))
    m.set_this(off['m_directionFlags'], struct.pack('<I', inp['flags']))
    m.set_this(off['m_newDirection'], struct.pack('<i', inp['mnew']))

    m.add_stub({'name': 'CGameSprite::GetDirection', 'kind': 'code',
                'addr': addrs['gdir_' + side], 'conv': 'cdecl',
                'nargs': 2, 'pop': 0, 'default': {'ret': inp['gdir']}})
    return m


SETS = {
    # &1 with pos == targetPos: keeps m_direction (7), no call
    'set1-bit0-stationary': dict(flags=1, pos=(100, 50), oldPos=(7, 9),
                                 targetPos=(100, 50), nDirection=3,
                                 mpos=(0, 0), mdirection=7, mnew=0, gdir=5),
    # &1 recomputing through the static (y*4/3 on both scaled refs)
    'set2-bit0-recompute': dict(flags=1, pos=(10, -3000), oldPos=(0, 0),
                                targetPos=(-5000, 12345), nDirection=0,
                                mpos=(1, 1), mdirection=-1, mnew=0, gdir=5),
    # &2 takes nDirection verbatim, negative edge
    'set3-bit1-direct': dict(flags=2, pos=(100, 50), oldPos=(0, 0),
                             targetPos=(100, 50), nDirection=-2,
                             mpos=(9, 9), mdirection=4, mnew=0, gdir=5),
    # no bits, oldPos == pos: keeps m_direction (-1)
    'set4-none-stationary': dict(flags=0, pos=(0, 0), oldPos=(0, 0),
                                 targetPos=(5, 5), nDirection=9,
                                 mpos=(0, 0), mdirection=-1, mnew=0, gdir=5),
    # no bits, recompute from (oldPos, pos): SHORT_MIN return edge
    'set5-none-recompute-min': dict(flags=0, pos=(1, 1), oldPos=(-1, -1),
                                    targetPos=(2, 2), nDirection=0,
                                    mpos=(3, 3), mdirection=0, mnew=0,
                                    gdir=-32768),
    # all bits set (&1 wins), recompute, SHORT_MAX return
    'set6-allbits-recompute-max': dict(flags=0xFFFFFFFF, pos=(10, -3000),
                                       oldPos=(0, 0),
                                       targetPos=(-5000, 12345), nDirection=0,
                                       mpos=(1, 1), mdirection=-1, mnew=0,
                                       gdir=32767),
}
