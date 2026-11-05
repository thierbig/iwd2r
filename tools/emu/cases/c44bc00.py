"""0x44BC00 TString::SetLength(this, UINT length).

    m_nLen = length;                       (this + 8)
    m_pData[length] = '\\0';                    (terminator, no clamp)

Five instructions, no calls, fully self-contained: the write set is
exactly m_nLen plus one NUL byte at m_pData[length].  There is NO range
check -- the caller owns keeping length inside the reservation (Task 72
established Grow as the range-owning path) -- so the sets probe the
arithmetic edges the body actually has: shrink inside the buffer,
length 0 (NUL at the first byte), length == m_nRes (terminator at the
last reserved byte), and the inline-union shape where m_pData = this+1
(rc byte 0xFF at this+0, buffer inside the object) so both writes land
in the `this` region.

TString is layout-identical both sides (16 bytes: union head 0,
m_pData 4, m_nLen 8, m_nRes 0xC -- src/TString.h), so offsets stay
literal; no stubs, no watches, no allocator involvement.

Returns 'void' -- `void SetLength(UINT length)` (src/TString.h:84);
EAX keeps whatever it held (codegen noise, not a contract).
"""

import struct

CASE = {
    'name': 'TString::SetLength', 'short': 'TString::SetLength',
    'exe': 0x44BC00, 'ours': 'TString::SetLength',
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(16)
    length = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16,
                [('length', length, 4, 'val')])
    m.set_arg('length', 0, struct.pack('<I', inp['length']))
    if inp.get('inline'):
        m.set_this(0, b'\xFF')                       # inline rc marker
        m.set_this(1, inp['content'])                # inline buffer
        m.set_this(4, struct.pack('<I', this + 1))   # m_pData -> this+1
        m.set_this(8, struct.pack('<i', inp['old_len']))
        m.set_this(0xC, struct.pack('<i', inp['res']))
    elif inp.get('preseed'):
        ps = inp['preseed']
        data = m.preseed_heap(ps['size'], ps['content'], ps['refcount'])
        m.set_this(4, struct.pack('<I', data))       # m_pData
        m.set_this(8, struct.pack('<i', ps['len']))  # m_nLen
        m.set_this(0xC, struct.pack('<i', ps['res']))
    return m


SETS = {
    'set1-shrink': dict(length=3, old_len=5, res=0x1F, inline=True,
                        content=b'hello\0'),
    'set2-zero': dict(length=0, preseed=dict(size=0x21, content=b'hello\0',
                                             refcount=0, len=5, res=0x1F)),
    'set3-res-edge': dict(length=0x1F,
                          preseed=dict(size=0x21, content=b'hello\0',
                                       refcount=0, len=5, res=0x1F)),
    'set4-grow-len': dict(length=7,
                          preseed=dict(size=0x21, content=b'hello\0',
                                       refcount=0, len=5, res=0x1F)),
}
