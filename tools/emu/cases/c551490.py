"""0x551490 CVEFVidCell::CalculatePos(this, pos)

Ported from the Task 72 spike (tools/emu/emu_diff.py build_calcpos); the
per-side field offsets now come from layout.py instead of hand tables
(CALCPOS_OFF: m_offset 0x8E/0x98, m_offsetFlags 0x96/0xA0, m_newDirection
0x33C/0x344 -- reproduced exactly by the derived maps).

The function also writes the rotated position back through `this` at the
CGameObject base (m_pos, exe 0x06 vs ours 0x08), so the diff runs with the
full derived CVEFVidCell byte map -- a superset of the spike's hand map
{0x6: 0x8, 0xA: 0xC}, which the layout acceptance test reproduces exactly.
No callees to stub.  m_offset is a CPoint: offx at +0, offy at +4.

Returns 'void' — `void CalculatePos(const CPoint& pos)` (src/CVisualEffect.h:99);
EAX ignored.
"""

import struct

CASE = {
    'name': 'CVEFVidCell::CalculatePos',
    'short': 'CalculatePos',
    'exe': 0x551490,
    'ours': 'CVEFVidCell::CalculatePos',
    'convention': 'thiscall',
    'this_class': 'CVEFVidCell',
    'this_size': 0x348,
    'members': ('m_offset', 'm_offsetFlags', 'm_newDirection'),
    'field_mapped': True,
    'extra': {},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import Machine, blk_alloc
    off = ctx['off'][side]
    posarg = blk_alloc(8)
    this = blk_alloc(0x348)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x348,
                [('pos', posarg, 8, 'ref')])
    m.set_arg('pos', 0, struct.pack('<ii', *inp['pos']))
    m.set_this(off['m_offset'] + 0, struct.pack('<i', inp['offset'][0]))
    m.set_this(off['m_offset'] + 4, struct.pack('<i', inp['offset'][1]))
    m.set_this(off['m_offsetFlags'], struct.pack('<I', inp['flags']))
    m.set_this(off['m_newDirection'], struct.pack('<h', inp['newdir']))
    return m


SETS = {
    'set1-no-rotation': dict(flags=0, offset=(7, -9), newdir=3, pos=(100, 50)),
    'set2-rotate-dir0': dict(flags=1, offset=(1000, -1000), newdir=0, pos=(0, 0)),
    'set3-rotate-dir7-negdiv': dict(flags=1, offset=(-383, 707), newdir=7, pos=(-5000, 12345)),
    'set4-rotate-dir15-zerooffset': dict(flags=1, offset=(0, 0), newdir=15, pos=(-1, -1)),
}
