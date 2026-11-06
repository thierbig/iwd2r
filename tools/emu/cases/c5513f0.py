"""0x5513F0 CVEFVidCell::FrameAdvance(this, LONG nCounter).

Advance gate (vanilla 0x5513F7-0x55143D): m_animationSpeed > 15 (the
const is the file-backed 0x84EDF8 == 30, no seeding needed) advances
when nCounter % (30/(30-speed)) != 0, else advances when
nCounter % (30/speed) == 0.  Sets keep speed in {-1, 2, 16, 20, 25}:
speed 0 and 30 are #DE on both sides (idiv by zero), so no set can
exercise them.  On advance: unless m_bltFlagsExtra & 1,
CVidCell::IsEndOfSequence(FALSE) runs on m_cVidCell as a direct call
(vanilla 0x7AE010, ours map-named) and a nonzero return sets m_done;
then the virtual slot-0 FrameAdvance fires on m_cVidCell and, when
m_cShadowVidCell != NULL, on the shadow cell.  The tail writes m_done
when nCounter > m_nDuration > 0.

The two virtual calls dispatch through fake vtables: each cell block
carries a vptr to a shared blk vtable whose slot 0 is a code stub at
the hand-picked STUB_PAGE+0x100 (same target both sides; the recorded
`this` keeps the two calls distinct).  m_done is the only `this`
write, at the field-mapped offset (vanilla 0x338).

Returns 'void' — `void FrameAdvance(LONG nCounter)` (src/CVisualEffect.h:98);
EAX ignored (vanilla `ret 4` at 0x551468).
"""

import struct

CASE = {
    'name': 'CVEFVidCell::FrameAdvance',
    'short': 'VidCell::FrameAdvance',
    'exe': 0x5513F0,
    'ours': 'CVEFVidCell::FrameAdvance',
    'convention': 'thiscall',
    'this_class': 'CVEFVidCell',
    'this_size': 0x348,
    'members': ('m_animationSpeed', 'm_bltFlagsExtra', 'm_cVidCell',
                'm_cShadowVidCell', 'm_nDuration', 'm_done'),
    'field_mapped': True,
    'extra': {'eos_orig': 0x7AE010,
              'eos_ours': ('map', '?IsEndOfSequence@CVidCell@@QAEHE@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, STUB_PAGE, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    off = ctx['off'][side]
    this = blk_alloc(0x348)
    counter = blk_alloc(4)
    vc = blk_alloc(0x10)
    sh = blk_alloc(0x10)
    vtbl = blk_alloc(0x20)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x348,
                [('nCounter', counter, 4, 'val')])
    m.set_arg('nCounter', 0, struct.pack('<i', inp['nCounter']))
    m.set_this(off['m_animationSpeed'], struct.pack('<i', inp['speed']))
    m.set_this(off['m_bltFlagsExtra'], struct.pack('<I', inp['flags']))
    m.set_this(off['m_cVidCell'], struct.pack('<I', vc))
    m.set_this(off['m_cShadowVidCell'],
               struct.pack('<I', sh if inp['shadow'] else 0))
    m.set_this(off['m_nDuration'], struct.pack('<i', inp['duration']))
    m.set_this(off['m_done'], bytes([inp['done']]))

    m.uc.mem_write(vtbl, struct.pack('<I', STUB_PAGE + 0x100))  # slot 0
    m.uc.mem_write(vc, struct.pack('<I', vtbl))
    m.uc.mem_write(sh, struct.pack('<I', vtbl))

    m.add_stub({'name': 'CVidCell::IsEndOfSequence', 'kind': 'code',
                'addr': addrs['eos_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': inp['eos']}})
    m.add_stub({'name': 'CVidCell::FrameAdvance', 'kind': 'code',
                'addr': STUB_PAGE + 0x100, 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    return m


SETS = {
    # slow branch, tick 15: 15 % 15 == 0 -> advance, EOS ran, shadow ran
    'set1-slow-advance': dict(speed=2, nCounter=15, flags=0, eos=0,
                              shadow=1, duration=-1, done=0),
    # slow branch, no advance: no calls, no writes
    'set2-slow-hold': dict(speed=2, nCounter=7, flags=0, eos=1,
                           shadow=1, duration=100, done=0),
    # fast branch, 4 % 3 != 0 -> advance, EOS sets m_done, duration 0
    'set3-fast-eos-done': dict(speed=20, nCounter=4, flags=0, eos=1,
                               shadow=1, duration=0, done=0),
    # fast branch, 2 % 2 == 0 -> no advance; flags&1 skips EOS; the
    # duration tail alone sets m_done
    'set4-duration-done': dict(speed=16, nCounter=2, flags=1, eos=1,
                               shadow=1, duration=1, done=0),
    # negative speed: 30/-1 = -30, 0 % -30 == 0 -> advance, no shadow
    'set5-negative-speed': dict(speed=-1, nCounter=0, flags=0, eos=0,
                                shadow=0, duration=5, done=1),
    # negative counter, slow branch: -15 % 15 == 0 -> advance
    'set6-negative-counter': dict(speed=2, nCounter=-15, flags=0, eos=1,
                                  shadow=0, duration=-9, done=0),
}
