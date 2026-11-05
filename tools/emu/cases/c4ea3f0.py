"""0x4EA3F0 CItem::GetMaxEffectSpellLevel(this).

    if (m_resref == EMPTY_RESREF) return 0;             [image constant]
    if (m_res->GetDemands() <= 0) report;               [stubbed, ret 2]
    for (i = 0; i < m_abCount; ++i) {
        P = m_res->GetAbility(i);                       [stubbed, scripted]
        if (!P || !*P) continue;
        E1 = GetAbilityEffect(P, i, ?);                 [stubbed, scripted]
        if (!E1) continue;
        best = max(best, E1->level);                    [+0x14 dword]
    }

CItem's own layout drifts (vanilla m_res/m_resref/m_abCount at
+8/+0xC/+0x14 vs ours +4/+0x8/+0x10); the class is not one of the mapped
headers, so the offsets are hardcoded per side rather than field_mapped.

Codegen drift handled:
  - CResRef::operator!= compares against an IMAGE constant; vanilla and
    ours negate the comparison differently, so the stub is inline_ok with
    a PER-SIDE ret (orig 0, ours 1) chosen so both take the equal branch.
  - GetDemands/GetAbility/GetAbilityEffect stay visible (NOT inline_ok):
    their shapes are comparable -- ECX is the same preseeded block and the
    scripted returns are the same block addresses.

Finding (reported, not worked around): when GetDemands() <= 0, vanilla
calls the report stub at 0x780C00 and continues, ours int3s at 0x406901.
All sets keep GetDemands at 2 to stay off that path.
"""

import struct

CASE = {
    'name': 'CItem::GetMaxEffectSpellLevel', 'short': 'CItem::GMESL',
    'exe': 0x4EA3F0, 'ours': 'CItem::GetMaxEffectSpellLevel',
    'expect_rva': 0x23C270,
    'convention': 'thiscall', 'this_class': None, 'this_size': 0x18,
    'members': (), 'field_mapped': False,
    'extra': {'opneq_orig': 0x78ABB0, 'getdemands_orig': 0x77E550,
              'getability_orig': 0x4015F0, 'geteffect_orig': 0x4E9680,
              'opneq_ours': 0x40A268, 'getdemands_ours': 0x4119C8,
              'getability_ours': 0x40B0E6, 'geteffect_ours': 0x404CAF},
    'resolve': {},
}

OFF = {'orig': {'res': 0x8, 'resref': 0xC, 'count': 0x14},
       'ours': {'res': 0x4, 'resref': 0x8, 'count': 0x10}}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(CASE['this_size'])
    off = OFF[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, CASE['this_size'], [])
    pres = blk_alloc(0x40)
    p = blk_alloc(8)                       # CResItemFeat* shape
    m.uc.mem_write(p, b'\x01' + b'\x00' * 7)         # *P != 0
    e1 = blk_alloc(0x18)                   # CEffectStruct* shape
    m.uc.mem_write(e1 + 0x14, struct.pack('<I', inp['effect_level']))
    m.set_this(off['res'], struct.pack('<I', pres))
    m.set_this(off['resref'], b'ITMRINGS')
    m.set_this(off['count'], struct.pack('<I', inp['count']))
    tok = {'P': p, 'E1': e1}              # SETS name blocks; rets need ints

    m.add_stub({'name': 'CResRef::operator!=', 'kind': 'code',
                'addr': addrs['opneq_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'inline_ok': True,
                'default': {'ret': 0 if side == 'orig' else 1}})
    m.add_stub({'name': 'CRes::GetDemands', 'kind': 'code',
                'addr': addrs['getdemands_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 2}})
    m.add_stub({'name': 'CResItem::GetAbility', 'kind': 'code',
                'addr': addrs['getability_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4,
                'rets': [tok.get(v, v) for v in inp['getability_rets']]})
    m.add_stub({'name': 'CItem::GetAbilityEffect', 'kind': 'code',
                'addr': addrs['geteffect_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12,
                'rets': [tok.get(v, v) for v in inp['geteffect_rets']]})
    return m


SETS = {
    'set1-one-ability': dict(count=1, effect_level=3, getability_rets=['P'],
                             geteffect_rets=['E1']),
    'set2-count0': dict(count=0, effect_level=3, getability_rets=[0],
                        geteffect_rets=[0]),
    'set3-resref-mismatch': dict(count=1, effect_level=3, getability_rets=[0],
                                 geteffect_rets=[0]),
    'set4-ability-null': dict(count=1, effect_level=3, getability_rets=[0],
                              geteffect_rets=[0]),
}
