"""0x44BE10 TString::Grow(this, nLen)

Ported from the Task 72 spike (tools/emu/emu_diff.py build_grow).

TString is layout-identical on both sides (16 bytes: union head at 0,
m_pData 4, m_nLen 8, m_nRes 0xC -- see src/TString.h), so no layout.py map
is needed and the member offsets stay literal below.  operator new[] /
delete[] are stubbed with a deterministic bump allocator; ours calls
memcpy and TString::Tidy as real functions (watched / stubbed) where the
VC6 original inlines them (rep movsd / folded loop) -- both tagged
inline_ok so one-sided appearances are ignored in the call diff.

Preseed shapes exercise the three Grow paths: empty string (nLen 0, no
buffer), exclusive buffer (refcount 0 -> freed, copied into a fresh
allocation), shared buffer (refcount 2 -> detached without freeing), and
an nLen near UINT_MAX edge.

Returns 'void' — `void Grow(UINT nLen)` (src/TString.h:79); EAX is ignored.
"""

import struct

CASE = {
    'name': 'TString::Grow',
    'short': 'TString::Grow',
    'exe': 0x44BE10,
    'ours': 'TString::Grow',
    'convention': 'thiscall',
    'this_class': None,              # layout identical both sides; see module docstring
    'this_size': 16,
    'members': (),
    'field_mapped': False,
    'extra': {
        # ours-side sites resolve from the map / import table at run time.
        'new_orig': 0x7FC95B, 'delete_orig': 0x7FC984,
        'new_ours': ('map', '??_U@YAPAXI@Z'),
        'delete_ours': ('map', '??_V@YAXPAX@Z'),
        'memcpy_ours': ('import', 'memcpy'),
        'tidy_ours': ('map', 'TString::Tidy'),
    },
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import Machine, blk_alloc
    this = blk_alloc(16)
    nlen = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16, [('nLen', nlen, 4, 'val')])
    m.set_arg('nLen', 0, struct.pack('<I', inp['arg_nlen']))
    if inp.get('preseed'):
        ps = inp['preseed']
        data = m.preseed_heap(ps['size'], ps['content'], ps['refcount'])
        m.set_this(4, struct.pack('<I', data))          # m_pData
        m.set_this(8, struct.pack('<i', ps['len']))     # m_nLen
        m.set_this(0xC, struct.pack('<i', ps['res']))   # m_nRes
    m.add_stub({'name': 'operator new[]', 'kind': 'code', 'addr': addrs['new_' + side],
                'conv': 'cdecl', 'nargs': 1, 'pop': 0, 'action': 'bump_new'})
    m.add_stub({'name': 'operator delete[]', 'kind': 'code', 'addr': addrs['delete_' + side],
                'conv': 'cdecl', 'nargs': 1, 'pop': 0, 'action': 'free'})
    if side == 'ours':      # original inlines memcpy as rep movs; ours
        # calls it through the IAT slot (import-lib thunk jmp [slot])
        m.add_stub({'name': 'memcpy', 'kind': 'iat', 'addr': addrs['memcpy_ours'],
                    'conv': 'cdecl', 'nargs': 3, 'pop': 0, 'action': 'copy',
                    'inline_ok': True})
    if side == 'ours':      # original inlines Tidy into Grow; ours calls it
        m.add_watch({'name': 'TString::Tidy', 'addr': addrs['tidy_ours'],
                     'conv': 'thiscall', 'nargs': 1, 'inline_ok': True})
    return m


SETS = {
    'set1-empty': dict(arg_nlen=0, preseed=None),
    'set2-grow-free': dict(arg_nlen=0x25, preseed=dict(size=0x21, content=b'hello\0', refcount=0, len=5, res=0x1F)),
    'set3-grow-shared': dict(arg_nlen=0x30, preseed=dict(size=0x21, content=b'share\0', refcount=2, len=5, res=0x1F)),
    'set4-edge-overflow': dict(arg_nlen=0xFFFFFFFE, preseed=dict(size=0x21, content=b'abcd\0', refcount=0, len=5, res=0x1F)),
}
