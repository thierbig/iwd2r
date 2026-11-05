"""0x55AB20 IcewindCProjectileTargetMap::Strike(this, std::list<LONG>&).

Walks the target list: for each id, if m_bSkipSource and the id matches
the owner's source id -> skip; else if m_nStrikes >= m_maxStrikesTotal ->
m_bDone = TRUE and stop; else DeliverStrike(id), m_nStrikes++.

IcewindCProjectileTargetMap drifts as a whole (vanilla 0x28/0x2C/0x30/0x31
vs ours 0x20/0x24/0x28/0x29 for max/strikes/done/skip), so the case is
field_mapped and all four members are preseeded via ctx['off'][side].
m_pOwner sits at +4 on BOTH sides; the owner identity the code compares is
read at a per-compiler offset, so the owner block carries the id dword at
both +0x72 and +0x7C.

std::list layout differs by runtime: VC6 keeps the anchor's _Next at
list+4, MSVC at list+0.  The empty list therefore points the head at
itself at that per-side offset, and node links chain through [node+0]
with the value at [node+8] on both.  DeliverStrike is stubbed thiscall
(pop 4) with the list VALUE as its argument -- an int that must match on
both sides -- so it is NOT inline_ok.

Returns 'void' — `void Strike(std::list<LONG>&)` (src/IcewindCProjectileTargetMap.h:31);
EAX ignored (orig `ret 4` at 0x55AB68, ours at 0x8136F4, both leave loop scratch).
"""

import struct

CASE = {
    'name': 'IcewindCProjectileTargetMap::Strike',
    'short': 'ProjTargetMap::Strike',
    'exe': 0x55AB20, 'ours': 'IcewindCProjectileTargetMap::Strike',
    'convention': 'thiscall', 'this_class': 'IcewindCProjectileTargetMap',
    'this_size': 0x38,
    'members': ('m_maxStrikesTotal', 'm_nStrikes', 'm_bDone', 'm_bSkipSource'),
    'field_mapped': True,
    'extra': {'strike_orig': 0x55AB80,
              'strike_ours': ('map', '?DeliverStrike@IcewindCProjectileTargetMap@@AAEXH@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(CASE['this_size'])
    lst = blk_alloc(0x10)
    m = Machine(side, pe, addrs[side], 'thiscall', this, CASE['this_size'],
                [('targets', lst, 0x10, 'ref')])
    off = ctx['off'][side]
    m.set_this(off['m_maxStrikesTotal'], struct.pack('<I', inp['max']))
    m.set_this(off['m_nStrikes'], struct.pack('<I', inp['count']))
    m.set_this(off['m_bDone'], bytes([inp['done']]))
    m.set_this(off['m_bSkipSource'], bytes([inp['skip']]))

    owner = blk_alloc(0x80)
    m.uc.mem_write(owner + 0x72, struct.pack('<I', inp['owner_id']))
    m.uc.mem_write(owner + 0x7C, struct.pack('<I', inp['owner_id']))
    m.set_this(4, struct.pack('<I', owner))         # m_pOwner, both sides

    head = 4 if side == 'orig' else 0               # anchor _Next offset
    anchor = lst + head
    prev = anchor
    for value in inp['targets']:
        node = blk_alloc(0x10)
        m.uc.mem_write(node + 0, struct.pack('<I', prev))    # -> back link
        m.uc.mem_write(prev, struct.pack('<I', node))        # -> next
        m.uc.mem_write(node + 8, struct.pack('<I', value))   # LONG value
        prev = node
    m.uc.mem_write(prev, struct.pack('<I', anchor))          # close ring

    m.add_stub({'name': 'IcewindCProjectileTargetMap::DeliverStrike',
                'kind': 'code', 'addr': addrs['strike_' + side],
                'conv': 'thiscall', 'nargs': 1, 'pop': 4,
                'default': {'ret': 0}})
    return m


SETS = {
    'set1-empty': dict(max=5, count=0, done=0, skip=0, owner_id=0, targets=()),
    'set2-strike': dict(max=5, count=0, done=0, skip=0, owner_id=0,
                        targets=(0x1234,)),
    'set3-at-max': dict(max=5, count=5, done=0, skip=0, owner_id=0,
                        targets=(0x1234,)),
    'set4-filter': dict(max=5, count=0, done=0, skip=1, owner_id=0x1234,
                        targets=(0x1234,)),
    'set5-two': dict(max=5, count=0, done=0, skip=0, owner_id=0,
                     targets=(0x1234, 0x5678)),
}
