"""0x44BD40 TString::Detach(this) -- COW make-exclusive.

    if (!m_pData) return 0;
    rc = m_pData[-1];
    if (rc == 0 || rc == 0xFF) return m_pData;        already exclusive
    // shared: make a private copy
    vanilla: Grow(len+1)-path inline (real new[]/memcpy/delete[] via
             stubs), zero this inline, Tidy(1) runs for real
    ours:    PrepareAssign/SetLength run for real (memcpy stubbed), then
             Tidy(0) runs for real and is watched (inline_ok) only on ours

Everything inside TString runs REAL on both sides; only the allocator
primitives are stubbed.  new[] is bump_new inline_ok on both sides: the
allocation ARGUMENT may drift (vanilla asks for len+1 rounded, ours
through MaxSize) -- that is codegen drift and exactly what this case is
keeping an eye on; if the zeroed regions or the copied bytes diverge in a
way that survives, it is a reportable finding, not a stub problem.

Sets keep len far below 0xFFFFFFFD to stay off the vanilla throw path.
"""

import struct

CASE = {
    'name': 'TString::Detach', 'short': 'TString::Detach',
    'exe': 0x44BD40, 'ours': 'TString::Detach', 'expect_rva': 0x42C110,
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {'new_orig': 0x7FC95B, 'delete_orig': 0x7FC984,
              'new_ours': 0x4084E0, 'delete_ours': 0x413E99,
              'memcpy_ours': 0xB6DCA5},
    'resolve': {'tidy_ours': ('TString::Tidy', 0x42C3D0)},
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(16)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16, [])
    if inp.get('preseed'):
        ps = inp['preseed']
        data = m.preseed_heap(ps['size'], ps['content'], ps['refcount'])
        m.set_this(4, struct.pack('<I', data))
        m.set_this(8, struct.pack('<i', ps['len']))
        m.set_this(0xC, struct.pack('<i', ps['res']))
    m.add_stub({'name': 'operator new', 'kind': 'code',
                'addr': addrs['new_' + side], 'conv': 'cdecl',
                'nargs': 1, 'pop': 0, 'action': 'bump_new',
                'inline_ok': True})
    m.add_stub({'name': 'operator delete[]', 'kind': 'code',
                'addr': addrs['delete_' + side], 'conv': 'cdecl',
                'nargs': 1, 'pop': 0, 'action': 'free'})
    if side == 'ours':
        m.add_stub({'name': 'memcpy', 'kind': 'code',
                    'addr': addrs['memcpy_ours'], 'conv': 'cdecl',
                    'nargs': 3, 'pop': 0, 'action': 'copy',
                    'inline_ok': True})
        m.add_watch({'name': 'TString::Tidy', 'addr': addrs['tidy_ours'],
                     'conv': 'thiscall', 'nargs': 1, 'inline_ok': True})
    return m


def _ps(rc):
    return dict(size=0x21, content=b'hello\0', refcount=rc, len=5, res=0)


SETS = {
    'set1-null': dict(preseed=None),
    'set2-rc0': dict(preseed=_ps(0)),
    'set3-rcff': dict(preseed=_ps(0xFF)),
    'set4-shared': dict(preseed=_ps(2)),
}
