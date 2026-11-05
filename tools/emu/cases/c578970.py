"""0x578970 CProjectileSingleTarget::CalculateFxRect(this, rFx, ptReference, pVidCell)

Ported from the Task 72 spike (tools/emu/emu_diff.py build_fxrect); the
per-side field offsets now come from layout.py instead of hand tables
(FXRECT_OFF: anim 0x192/0x1A8, shadow 0x196/0x1AC, shadow_on 0x1D4/0x1E0,
posz 0x0E/0x10 -- reproduced exactly by the derived maps).

All writes land in the rFx/ptReference argument blocks, never in `this`,
so no field_map is needed for the diff.  GetCurrentCenterPoint /
GetCurrentFrameSize are stubbed with ret_by_ecx response tables keyed by
ECX (the animation / shadow / vidcell sentinel); the original calls
User32.SetRect through its IAT while ours inlines it (compat/win_types.h).

Returns 'void' — `void CalculateFxRect(CRect& rFx, CPoint& ptReference,
CVidCell* pVidCell)` (src/CProjectileSingleTarget.h:29); EAX ignored.
"""

import struct

from cases import SENT

CASE = {
    'name': 'CProjectileSingleTarget::CalculateFxRect',
    'short': 'CalculateFxRect',
    'exe': 0x578970,
    'ours': 'CProjectileSingleTarget::CalculateFxRect',
    'convention': 'thiscall',
    'this_class': 'CProjectileSingleTarget',
    'this_size': 0x1F0,
    'members': ('m_animation', 'm_shadow', 'field_1D4', 'm_posZ'),
    'field_mapped': False,
    # ours-side sites resolve from the map at run time (fix 2): the call
    # goes through an early-.text ILT thunk, and hooking the map-named
    # body catches it either way.  Vanilla bodies stay literal.
    'extra': {
        'center_orig': [0x7AD520], 'fsize_orig': [0x7AD800],
        'center_ours': [('map', '?GetCurrentCenterPoint@CVidCell@@QAEHAAVCPoint@@E@Z')],
        'fsize_ours': [('map', '?GetCurrentFrameSize@CVidCell@@QAEHAAVCSize@@E@Z')],
    },
    'returns': 'void',
}


def resp_of(inp):
    """ret_by_ecx response tables for the two stubbed size/point getters."""
    return {
        'center': {SENT['anim']: {'ret': 1, 'writes': [(0, list(inp['center']))]},
                   SENT['shadow']: {'ret': 1, 'writes': [(0, list(inp['shadow_center']))]},
                   SENT['vidcell']: {'ret': 1, 'writes': [(0, list(inp['center']))]}},
        'size': {SENT['anim']: {'ret': 1, 'writes': [(0, list(inp['framesize']))]},
                 SENT['shadow']: {'ret': 1, 'writes': [(0, list(inp['shadow_size']))]},
                 SENT['vidcell']: {'ret': 1, 'writes': [(0, list(inp['framesize']))]}},
    }


def build(side, inp, pe, addrs, ctx):
    from machine import Machine, blk_alloc
    off = ctx['off'][side]
    rfx = blk_alloc(16)
    ptref = blk_alloc(8)
    pvc = blk_alloc(4)
    this = blk_alloc(0x1F0)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x1F0,
                [('rFx', rfx, 16, 'ref'), ('ptReference', ptref, 8, 'ref'),
                 ('pVidCell', pvc, 4, 'val')])
    m.set_arg('rFx', 0, b'\xEF\xBE\xAD\xDE' * 4)             # garbage to prove overwrite
    m.set_arg('ptReference', 0, b'\x11\x22\x33\x44\x55\x66\x77\x88')
    m.set_this(off['m_animation'], struct.pack('<I', SENT['anim']))
    m.set_this(off['m_shadow'], struct.pack('<I', SENT['shadow']))
    m.set_this(off['field_1D4'], struct.pack('<I', inp['shadow_on']))
    m.set_this(off['m_posZ'], struct.pack('<i', inp['posz']))
    m.set_arg('pVidCell', 0, struct.pack('<I', inp['vidcell'] or 0))
    resp = resp_of(inp)
    for a in addrs['center_' + side]:
        m.add_stub({'name': 'GetCurrentCenterPoint', 'kind': 'code', 'addr': a,
                    'conv': 'thiscall', 'nargs': 2, 'pop': 8,
                    'ret_by_ecx': resp['center'], 'default': {'ret': 0}})
    for a in addrs['fsize_' + side]:
        m.add_stub({'name': 'GetCurrentFrameSize', 'kind': 'code', 'addr': a,
                    'conv': 'thiscall', 'nargs': 2, 'pop': 8,
                    'ret_by_ecx': resp['size'], 'default': {'ret': 0}})
    if side == 'orig':      # ours inlines SetRect (compat/win_types.h)
        m.add_stub({'name': 'SetRect', 'kind': 'iat', 'addr': 0x8474CC,
                    'conv': 'stdcall', 'nargs': 5, 'pop': 0x14,
                    'action': 'write_args', 'ret': 1, 'inline_ok': True})
    return m


SETS = {
    'set1-no-shadow-minimal': dict(shadow_on=0, posz=5, vidcell=None,
                                   center=(0, 0), framesize=(0, 0), shadow_center=(0, 0), shadow_size=(0, 0)),
    'set2-no-shadow': dict(shadow_on=0, posz=0, vidcell=SENT['vidcell'],
                           center=(37, -15), framesize=(128, 96), shadow_center=(0, 0), shadow_size=(0, 0)),
    'set3-shadow': dict(shadow_on=1, posz=17, vidcell=SENT['vidcell'],
                        center=(100, 200), framesize=(64, 32), shadow_center=(120, 90), shadow_size=(48, 72)),
    'set4-shadow-dominant': dict(shadow_on=1, posz=-9, vidcell=None,
                                 center=(500, 5), framesize=(10, 600), shadow_center=(900, 800), shadow_size=(700, 20)),
}
