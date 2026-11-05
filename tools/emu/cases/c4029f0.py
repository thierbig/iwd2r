"""0x4029F0 CResBIO::Demand(this) -- the P/flag cache dance.

    eax = CRes::Demand()                      [stubbed, per-set ret]
    if (m_bParsed && eax) {
        if (CRes::GetDemands() > 1) return eax;      [stubbed, per-set ret]
    }
    if (eax) m_bParsed = TRUE;
    return m_bParsed ? eax : 0;

CResBIO layout drifts between the images (vanilla m_bParsed at +0x50, ours
at +0x64) so the case is field_mapped and the flag is preseeded via
ctx['off'][side].  Both callees are thiscall leaf getters; they are NOT
inline_ok because their logged shapes ('this' ECX, int ret) are identical
on both sides -- the interesting thing this case pins is the branch logic
around them.
"""

CASE = {
    'name': 'CResBIO::Demand', 'short': 'CResBIO::Demand',
    'exe': 0x4029F0, 'ours': 'CResBIO::Demand', 'expect_rva': 0x295070,
    'convention': 'thiscall', 'this_class': 'CResBIO', 'this_size': 0x68,
    'members': ('m_bParsed',), 'field_mapped': True,
    'extra': {'demand_orig': 0x77E390, 'getdemands_orig': 0x77E550,
              'demand_ours': 0x40B744, 'getdemands_ours': 0x4119C8},
    'resolve': {},
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(CASE['this_size'])
    m = Machine(side, pe, addrs[side], 'thiscall', this, CASE['this_size'], [])
    m.set_this(ctx['off'][side]['m_bParsed'], bytes([inp['flag']]))
    m.add_stub({'name': 'CRes::Demand', 'kind': 'code',
                'addr': addrs['demand_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': inp['demand']}})
    m.add_stub({'name': 'CRes::GetDemands', 'kind': 'code',
                'addr': addrs['getdemands_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': inp['demands']}})
    return m


SETS = {
    'set1-notloaded': dict(demand=0, demands=0, flag=0),
    'set2-parse': dict(demand=0x11223344, demands=0, flag=0),
    'set3-shared': dict(demand=0x11223344, demands=1, flag=5),
    'set4-cached': dict(demand=0x11223344, demands=2, flag=5),
}
