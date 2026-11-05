"""0x5295D0 CProjectileBAM::SetVidCell(this, CResRef) -- destroy + rebuild.

CProjectileBAM layout drifts (vanilla m_animation at +0x192, ours at
+0x1A8), so the case is field_mapped and every this-relative access goes
through ctx['off'][side].  The body is:

    if (m_animation) { m_animation->~CVidCell(); operator delete(p, n); }
    p = operator new(sizeof); new (p) CVidCell(resref, flag, ?);
    m_animation = p;

All four callees are stubbed (they touch real vidcell internals):
  - ~CVidCell: thiscall, ECX = the preseeded old pointer -> same heap norm
    on both sides, so NOT inline_ok.
  - operator delete: ours is the sized form (nargs 2 vs 1) and both take
    a size argument -> inline_ok.
  - operator new: stubbed PLAIN returning m._bump(0xE0), inline_ok.  The
    real sizes drift (vanilla 0xDA vs ours 0xDC) and bump_new would
    zero-write different lengths, which is codegen drift, not behavior;
    the fixed-size bump keeps the store target's heap offset identical.
  - CVidCell::CVidCell: ret 'ecx' (returns the new pointer), NOT
    inline_ok -- its ECX/ret is the heap address that lands in
    m_animation at the per-side offset via ctx.

Returns 'void' — `void SetVidCell(CResRef resRef)` (src/CProjectileBAM.h:27);
EAX ignored (both bodies leave the stored pointer in it before `ret`).
"""

import struct

CASE = {
    'name': 'CProjectileBAM::SetVidCell', 'short': 'CProjectileBAM::SetVidCell',
    'exe': 0x5295D0, 'ours': 'CProjectileBAM::SetVidCell',
    'convention': 'thiscall', 'this_class': 'CProjectileBAM', 'this_size': 0x2B0,
    'members': ('m_animation',), 'field_mapped': True,
    'extra': {'dtor_orig': 0x7AD0F0, 'delete_orig': 0x7FC984,
              'new_orig': 0x7FC95B, 'ctor_orig': 0x7ACE50,
              'dtor_ours': ('map', '??1CVidCell@@QAE@XZ'),
              'delete_ours': ('map', '??3@YAXPAXI@Z'),
              'new_ours': ('map', '??2@YAPAXI@Z'),
              'ctor_ours': ('map', '??0CVidCell@@QAE@VCResRef@@H@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(CASE['this_size'])
    d0 = blk_alloc(4)
    d1 = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, CASE['this_size'],
                [('ref_d0', d0, 4, 'val'), ('ref_d1', d1, 4, 'val')])
    m.set_arg('ref_d0', 0, struct.pack('<I', inp['ref_d0']))
    m.set_arg('ref_d1', 0, struct.pack('<I', inp['ref_d1']))
    off_anim = ctx['off'][side]['m_animation']
    if inp['old_vidcell']:
        old = m.preseed_heap(0xE0, b'\xCC' * 0x10, 1)
        m.set_this(off_anim, struct.pack('<I', old))
    m.add_stub({'name': 'CVidCell::~CVidCell', 'kind': 'code',
                'addr': addrs['dtor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'operator delete', 'kind': 'code',
                'addr': addrs['delete_' + side], 'conv': 'cdecl',
                'nargs': 2 if side == 'ours' else 1, 'pop': 0,
                'action': 'free', 'inline_ok': True})
    m.add_stub({'name': 'operator new', 'kind': 'code',
                'addr': addrs['new_' + side], 'conv': 'cdecl',
                'nargs': 1, 'pop': 0, 'inline_ok': True,
                'default': {'ret': m._bump(0xE0)}})
    m.add_stub({'name': 'CVidCell::CVidCell', 'kind': 'code',
                'addr': addrs['ctor_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12, 'ret': 'ecx'})
    return m


SETS = {
    'set1-fresh': dict(old_vidcell=None, ref_d0=0x11223344, ref_d1=0x005566AA),
    'set2-replace': dict(old_vidcell=True, ref_d0=0x11223344, ref_d1=0x005566AA),
}
