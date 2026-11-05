"""0x543770 CRuleTables::GetMaxDruidShapeshifts(this, CREFileHeader&, level).

    if (!cre.m_flags[0x86]) return 0;
    cell = &this->m_table[level - 1][0];               (C2DArray @ +0x588)
    n = atoi(cell->str);
    return n > 0 ? n + cre.m_bonus[0x1E6] : n;

The creature offsets (+0x86 flag, +0x1E6 bonus) are identical in both
images; the CRuleTables object itself is only READ, and the table data is
preseeded per side:

  vanilla: this+0x5B0 = data ptr, this+0x5B4 = default-str ptr,
           this+0x5B8 = width 2 (word), this+0x5BA = height 2 (word).
           Cell index is row-major: level L reads data + 4*w*(L-1), so
           level 2 -> data+8.  An out-of-range level (L >= h) reads the
           DEFAULT cell at [this+0x5B4] instead.
  ours: the table access is factored into C2DArray::GetAt (thiscall,
           callee-pops, inlined bounds check lives THERE) called with
           {col 0, row L-1} -- the caller has no bounds check.  GetAt is
           stubbed inline_ok to return a per-set cell block holding the
           same string pointer vanilla would read (the default string for
           the out-of-range set).

atol is stubbed with a per-set return (NOT inline_ok): its argument is
the string pointer, the same block address on both sides, so the shapes
match -- vanilla calls VC6's atol at 0x7E7B03 as a plain cdecl function,
ours goes through the atol IAT slot (kind 'iat').  Truth (fix 2): the
source calls atol (src/CRuleTables.cpp:2325), the slot is
api-ms-win-crt-convert-l1-1-0!atol resolved from our import table at run
time (0xFC5B04 in i077 -- which an earlier draft of this case mislabeled
'atoi'; the real atoi slot there is 0xFC5AF0).

Returns 'u32' — `INT GetMaxDruidShapeshifts(...) const` (src/CRuleTables.h:397);
EAX is the atol-derived count (n, n+bonus, or 0), compared masked to 32 bits.
"""

import struct

CASE = {
    'name': 'CRuleTables::GetMaxDruidShapeshifts',
    'short': 'GetMaxDruidShapeshifts',
    'exe': 0x543770, 'ours': 'CRuleTables::GetMaxDruidShapeshifts',
    'convention': 'thiscall', 'this_class': None, 'this_size': 0x600,
    'members': (), 'field_mapped': False,
    'extra': {'atol_orig': 0x7E7B03,
              'atol_ours': ('import', 'api-ms-win-crt-convert-l1-1-0.dll!atol'),
              'getat_ours': ('map', '?GetAt@C2DArray@@QBEABVCString@@ABVCPoint@@@Z')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(CASE['this_size'])
    cre = blk_alloc(0x200)
    lvl = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, CASE['this_size'],
                [('cre', cre, 0x200, 'ref'), ('level', lvl, 4, 'val')])
    m.set_arg('level', 0, struct.pack('<i', inp['level']))
    m.set_arg('cre', 0x86, bytes([inp['flag']]))
    m.set_arg('cre', 0x1E6, bytes([inp['bonus']]))

    pstr = blk_alloc(8)                   # cell string "3"
    m.uc.mem_write(pstr, inp['str'] + b'\x00' * (8 - len(inp['str'])))
    pdef = blk_alloc(8)                   # default string
    m.uc.mem_write(pdef, inp['dstr'] + b'\x00' * (8 - len(inp['dstr'])))

    if side == 'orig':
        data = blk_alloc(0x10)
        m.uc.mem_write(data, struct.pack('<III', pdef, pdef, pstr))
        m.set_this(0x5B0, struct.pack('<I', data))
        m.set_this(0x5B4, struct.pack('<I', pdef))
        m.set_this(0x5B8, struct.pack('<HH', 2, 2))    # width, height
        m.add_stub({'name': 'atol', 'kind': 'code', 'addr': addrs['atol_orig'],
                    'conv': 'cdecl', 'nargs': 1, 'pop': 0,
                    'default': {'ret': inp['atol']}})
    else:
        cell = blk_alloc(4)               # what GetAt hands back
        m.uc.mem_write(cell, struct.pack('<I', pdef if inp['oob'] else pstr))
        m.add_stub({'name': 'C2DArray::GetAt', 'kind': 'code',
                    'addr': addrs['getat_ours'], 'conv': 'thiscall',
                    'nargs': 1, 'pop': 4, 'inline_ok': True,
                    'default': {'ret': cell}})
        m.add_stub({'name': 'atol', 'kind': 'iat', 'addr': addrs['atol_ours'],
                    'conv': 'cdecl', 'nargs': 1, 'pop': 0,
                    'default': {'ret': inp['atol']}})
    return m


SETS = {
    'set1-off': dict(flag=0, level=2, bonus=5, atol=3, oob=False,
                     str=b'3', dstr=b'0'),
    'set2-hit': dict(flag=1, level=2, bonus=5, atol=3, oob=False,
                     str=b'3', dstr=b'0'),
    'set3-atolzero': dict(flag=1, level=2, bonus=5, atol=0, oob=False,
                          str=b'0', dstr=b'0'),
    'set4-oob-default': dict(flag=1, level=3, bonus=5, atol=3, oob=True,
                             str=b'3', dstr=b'3'),
}
