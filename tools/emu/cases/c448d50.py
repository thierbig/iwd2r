"""0x448D50 TString::Tidy(this, BOOL used) -- release / reset in place.

Layout is identical on both sides (TString: +0 head union, +4 m_pData,
+8 m_nLen, +0xC m_nRes), so no field map.  The interesting behavior is the
refcount byte in front of the shared buffer:

    used==0            : no rc traffic, just the three zero writes
    used && !m_pData   : three zero writes
    rc in {0, 0xFF}    : exclusive buffer -> delete[](pdata-1), no rc write
    rc otherwise       : shared buffer -> dec byte at pdata-1, no free

delete[] is stubbed 'free' and NOT inline_ok: its argument is the preseeded
heap pointer, which normalizes to the same heap offset on both sides.  The
rc dec is a real heap write at the same offset both sides (preseed_heap is
deterministic and called in the same order).

Returns 'void' — `void Tidy(BOOLEAN bDealloc)` (src/TString.h:80); EAX is
ignored (both bodies leave free-path scratch in it).
"""

import struct

CASE = {
    'name': 'TString::Tidy', 'short': 'TString::Tidy',
    'exe': 0x448D50, 'ours': 'TString::Tidy',
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {'new_orig': 0x7FC95B, 'delete_orig': 0x7FC984,
              'new_ours': ('map', '??_U@YAPAXI@Z'),
              'delete_ours': ('map', '??_V@YAXPAX@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(16)
    used = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16,
                [('used', used, 4, 'val')])
    m.set_arg('used', 0, struct.pack('<I', inp['used']))
    if inp.get('preseed'):
        ps = inp['preseed']
        data = m.preseed_heap(ps['size'], ps['content'], ps['refcount'])
        m.set_this(4, struct.pack('<I', data))
        m.set_this(8, struct.pack('<i', ps['len']))
        m.set_this(0xC, struct.pack('<i', ps['res']))
    m.add_stub({'name': 'operator delete[]', 'kind': 'code',
                'addr': addrs['delete_' + side], 'conv': 'cdecl',
                'nargs': 1, 'pop': 0, 'action': 'free'})
    return m


def _ps(rc, content=b'hello\0'):
    return dict(size=0x21, content=content, refcount=rc, len=5, res=0x1F)


SETS = {
    'set1-used0': dict(used=0, preseed=_ps(2)),
    'set2-nulldata': dict(used=1, preseed=None),
    'set3-rc0-free': dict(used=1, preseed=_ps(0)),
    'set4-rcff-free': dict(used=1, preseed=_ps(0xFF)),
    'set5-rc2-dec': dict(used=1, preseed=_ps(2)),
}
