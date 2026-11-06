"""0x551670 CVEFVidCell::GetDirection(pos, targetPos, nDirection) [static].

pos == targetPos returns nDirection untouched; otherwise the static
CGameSprite::GetDirection on (pos, targetPos) scaled to y*4/3 decides
(vanilla call at 0x55169C, cdecl, two refs, `add esp,8`; ours the
map-named static overload).  Pure function: no writes, no `this` --
the pos block anchors the machine.  nDirection is a SHORT pushed as a
dword and read as a word (vanilla [esp+0x24]).

Returns 'u16' — `static SHORT GetDirection(...)` (src/CVisualEffect.h:101):
both sides return in AX (vanilla leaves ax from the nDirection move at
0x5516A5 or the call's return), so the case compares the low 16 EAX
bits plus the call list.
"""

import struct

CASE = {
    'name': 'CVEFVidCell::GetDirection',
    'short': 'VidCell::GetDirection',
    'exe': 0x551670,
    'ours': 'CVEFVidCell::GetDirection',
    'convention': 'cdecl',
    'this_class': None,
    'this_size': 8,
    'members': (),
    'field_mapped': False,
    'extra': {'gdir_orig': 0x706DD0,
              'gdir_ours': ('map', '?GetDirection@CGameSprite@@SAFABVCPoint@@0@Z')},
    'returns': 'u16',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    pos = blk_alloc(8)
    target = blk_alloc(8)
    ndir = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'cdecl', pos, 8,
                [('pos', pos, 8, 'ref'), ('targetPos', target, 8, 'ref'),
                 ('nDirection', ndir, 4, 'val')])
    m.set_arg('pos', 0, struct.pack('<ii', *inp['pos']))
    m.set_arg('targetPos', 0, struct.pack('<ii', *inp['targetPos']))
    m.set_arg('nDirection', 0, struct.pack('<i', inp['nDirection']))

    m.add_stub({'name': 'CGameSprite::GetDirection', 'kind': 'code',
                'addr': addrs['gdir_' + side], 'conv': 'cdecl',
                'nargs': 2, 'pop': 0, 'default': {'ret': inp['gdir']}})
    return m


SETS = {
    # stationary: returns nDirection, no call
    'set1-keep': dict(pos=(100, 50), targetPos=(100, 50), nDirection=9,
                      gdir=5),
    # recompute through the scaled static
    'set2-recompute': dict(pos=(-5000, 12345), targetPos=(10, -3000),
                           nDirection=0, gdir=5),
    # y*4/3 extremes on both points, SHORT_MIN return
    'set3-scale-edges': dict(pos=(0, 30000), targetPos=(0, -30000),
                             nDirection=0, gdir=-32768),
    # stationary at the origin with SHORT_MIN nDirection
    'set4-keep-min': dict(pos=(0, 0), targetPos=(0, 0), nDirection=-32768,
                          gdir=7),
}
