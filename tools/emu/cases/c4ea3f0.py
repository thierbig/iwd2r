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

CItem's layout drifts (vanilla m_res/m_resref/m_nAbilities at +8/+0xC/+0x14
vs ours +4/+0x8/+0x10).  m_nAbilities comes from layout.py (this_class
'CItem').  The exe side cannot map m_res/m_resref: they live in the
CResHelper<CResItem...> base template, whose header has no /* XXXX */
comments, so those two stay literal on the vanilla side (+8/+0xC -- that
exe never changes); ours reads the PDB-named pRes/cResRef via layout.py.

Codegen drift handled:
  - CResRef::operator!= compares against an IMAGE constant; vanilla and
    ours negate the comparison differently, so the stub is inline_ok with
    a PER-SIDE ret (orig 0, ours 1) chosen so both take the equal branch.
  - GetDemands/GetAbility/GetAbilityEffect stay visible (NOT inline_ok):
    their shapes are comparable -- ECX is the same preseeded block and the
    scripted returns are the same block addresses.

Noreturn (fix 3): when GetDemands() <= 0 both images call the assertion
reporter -- vanilla 0x780C00 (AssertionFailed: suspends threads, calls
ShutDown via the vtable, jumps to abort), ours CUtil::UtilAssert
(src/CUtil.cpp) -- and neither returns; MSVC even pads after the call
with int3 (0x63C315), VC6 with a dead `add esp,0x10` (0x4EA43C).  The
stub is noreturn with nargs 0: all four pushed words (file/line/message
image constants) are addresses inside each image and so incomparable.
Both sides terminating at the assert is EQUAL (set5).

Returns 'u32' — `INT GetMaxEffectSpellLevel()` (src/CItem.h:43); EAX is the
best level or 0, compared masked to 32 bits.
"""

import struct

CASE = {
    'name': 'CItem::GetMaxEffectSpellLevel', 'short': 'CItem::GMESL',
    'exe': 0x4EA3F0, 'ours': 'CItem::GetMaxEffectSpellLevel',
    'convention': 'thiscall', 'this_class': 'CItem', 'this_size': 0x18,
    'members': ('m_nAbilities',), 'field_mapped': False,
    'extra': {'opneq_orig': 0x78ABB0, 'getdemands_orig': 0x77E550,
              'getability_orig': 0x4015F0, 'geteffect_orig': 0x4E9680,
              'assert_orig': 0x780C00,
              'opneq_ours': ('map', '??9CResRef@@QBEHPBD@Z'),
              'getdemands_ours': ('map', '?GetDemands@CRes@@QAEHXZ'),
              'getability_ours': ('map', '?GetAbility@CResItem@@QAEPAUItem_ability_st@@H@Z'),
              'geteffect_ours': ('map', '?GetAbilityEffect@CItem@@QAEPAVCGameEffect@@HHPAVCGameObject@@@Z'),
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(CASE['this_size'])
    off = ctx['off'][side]
    res_off = 0x8 if side == 'orig' else off['pRes']
    resref_off = 0xC if side == 'orig' else off['cResRef']
    m = Machine(side, pe, addrs[side], 'thiscall', this, CASE['this_size'], [])
    pres = blk_alloc(0x40)
    p = blk_alloc(8)                       # CResItemFeat* shape
    m.uc.mem_write(p, b'\x01' + b'\x00' * 7)         # *P != 0
    e1 = blk_alloc(0x18)                   # CEffectStruct* shape
    m.uc.mem_write(e1 + 0x14, struct.pack('<I', inp['effect_level']))
    m.set_this(res_off, struct.pack('<I', pres))
    m.set_this(resref_off, b'ITMRINGS')
    m.set_this(off['m_nAbilities'], struct.pack('<I', inp['count']))
    tok = {'P': p, 'E1': e1}              # SETS name blocks; rets need ints

    m.add_stub({'name': 'CResRef::operator!=', 'kind': 'code',
                'addr': addrs['opneq_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'inline_ok': True,
                'default': {'ret': 0 if side == 'orig' else 1}})
    m.add_stub({'name': 'CRes::GetDemands', 'kind': 'code',
                'addr': addrs['getdemands_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0,
                'default': {'ret': inp.get('demands', 2)}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 0, 'pop': 0, 'noreturn': True})
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
    # fix 3: demands <= 0 -> both images call the assert and never return
    'set5-demands-zero': dict(count=1, effect_level=3, getability_rets=[0],
                              geteffect_rets=[0], demands=0),
}
