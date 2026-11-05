"""0x4F8880 CMessageAddEffect::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    The inverse of c4f8470's Marshal: parse a 0x5C-byte wire image into the
    message, resolve two (player, remote) id pairs, DecodeEffect a payload,
    and gate a sprite-path effect through CGameObjectArray::GetDeny.

    cursor seeds at SPEC_MSG_HEADER_LENGTH = 3 (.data vanilla 0x85E670 /
    ours 0xBFD9F0), so wire[0..2] are the message sub-header and every field
    below is at its ABSOLUTE wire offset:

      +0x03 player DWORD      +0x3F special DWORD      (damage branch ->
      +0x07 remote DWORD      +0x43 source CPoint (2D)  eff+0x44 when the
      +0x0B effectID WORD     +0x4B player2 DWORD       effectID word == 0xC)
      +0x0D targetType BYTE   +0x4F remote2 DWORD
      +0x0E spellLevel BYTE   +0x53 target CPoint (2D) marker: player2 == 0
      +0x0F effectAmount DWD  +0x5B field188 BYTE       && remote2 == image
      +0x13 dwFlags DWORD     +0x5C end; assert cnt        INVALID_INDEX -1
      +0x17 durationType WRD      (vanilla 0x851878 /      -> localId := -1,
      +0x19 duration DWORD         ours 0xBF386C) both      skip Find#2
      +0x1D probUpper BYTE     sides -> localId := -1, skip Find#2
      +0x1E probLower BYTE
      +0x1F res[8]            +0x27 sourceRes[8]
      +0x2F numDice DWORD     +0x33 diceSize DWORD
      +0x37 savingThrow DWORD +0x3B saveMod DWORD

    Flow (both sides instruction-matched; vanilla 0x4F8880-0x4F8C95, ours
    0x665570-0x6659FF): Find#1(player, remote, &localId) -> m_targetId;
    parse effectID..source CPoint; ids2; Find#2(player2, remote2, &localId2)
    unless marker; target CPoint + field188; assert cnt == dwSize (never
    fires -- 0x5C); DecodeEffect(effect_local, source_pt, localId2,
    target_pt) cdecl -> m_effect = ret; damage write eff+0x44;
    CResRef::CResRef(sourceRes) copy ctor (KEPT); then the CSpell fetch
    region -- pure inline_ok asymmetry: vanilla default-ctor + operator=(
    "DEFAULT") + CSpell::SetResRef, ours CSpell ctor + CResRef::operator!=
    (ret 0 -> fetch skipped; the real body would run CancelRequest /
    ReleaseResObject) ; GetDeny retry loop while SHARED(4)/DENIED(5);
    SUCCESS(0) -> virtual GetObjectType x2 (vtable slot +4 on a fake
    CGameSprite -- c4f8470's vtbl/vstub trick), TYPE_AIBASE mask (image
    byte vanilla 0x84C501 / ours 0x84C501-equiv), sprite 0x51 ->
    GetCasterLevel(spell) -> eff+0xC4 (movsx) and GetFeatValue(0x40) ->
    eff+0xC8 (ret << 1); non-sprite / mask-fail / terminal GetDeny ->
    eff+0xC4 = 1; SUCCESS -> ReleaseDeny; final CResRef::operator=
    (BYTE*) this = eff+0x90 <- sourceRes; eff+0x188 <- field188; dead tail
    gated on spell.m_pRes (0 both sides -- vanilla explicit zero, ours
    zero-filled stack under the no-write CSpell ctor stub: the
    fetch-success path is unreachable under stubbing, documented gap);
    return TRUE (0 on either Find miss).

    Game alignment, task-76b layout rule: NO ours-side member offset
    appears in this file.  Vanilla reads its chitin member +0x1C54 to get
    the CInfGame G, then Find this = G+0x375A, GetDeny/ReleaseDeny this =
    G+0x372C as REAL loads.  Ours reaches the game through accessors --
    GetObjectGame/GetObjectArray -- which are stubbed inline_ok with
    scripted returns game+0x52 (derived: 0x375A - ours' lea 0x3708, so the
    REAL `lea ecx,[eax+0x3708]` lands at G+0x375A) and game+0x372C, so the
    KEPT stubs record identical this values on both sides.

    eff-block offsets (+0x44/+0x90/+0xC4/+0xC8/+0x10C/+0x188) are
    code-identical immediates in both disassemblies -- not layout-derived;
    the this-class members (m_targetId +4, m_effect +0xC) go through
    ctx['off'][side] (tools/emu/layout.py field map).

    KEPT stubs (identical multiset both sides): Find, DecodeEffect,
    CResRef(BYTE*) ctor, GetDeny, GetObjectType@vstub, GetCasterLevel,
    GetFeatValue, ReleaseDeny, final operator=.  inline_ok: vanilla-only
    CResRef()/operator=(char*)/SetResRef; ours-only CSpell ctor /
    operator!= / both accessors.  Dead-tail callees left unstubbed on
    purpose -- an m_pRes != 0 surprise ERRORS loudly instead of passing.

    Recorded-shape notes (report findings, no behavioral diff): Find#1
    tests `cmp eax,1` where Find#2 tests `test eax,eax` -- unobservable
    for 0/1 BOOL stubs; SetResRef granularity (vanilla 1 call vs ours
    inlined 8) and UtilAssert's cdecl-returns vs noreturn-thunk shape are
    the usual c4f8470-class codegen asymmetries.
"""

import struct

WIRE_SIZE = 0x5C

# vanilla-only inline_ok callees (ours has no call at these sites)
VAN_CTOR0 = 0x78A960        # CResRef::CResRef()
VAN_OPCHAR = 0x78AD40       # CResRef::operator=(char const*)  "DEFAULT"
VAN_SETRESREF = 0x4699F0    # CSpell::SetResRef(...)
VAN_GAME_MEMBER = 0x1C54    # chitin member holding CInfGame* (read, not called)

CASE = {
    'name': 'CMessageAddEffect::UnmarshalMessage',
    'short': 'AddEffect::Unmarshal',
    'exe': 0x4F8880,
    'ours': 'CMessageAddEffect::UnmarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageAddEffect',
    'this_size': 0x10,
    'members': ('m_targetId', 'm_effect'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'find_orig': 0x59B300,
              'find_ours': ('map',
                            '?Find@CGameRemoteObjectArray@@QAEHIHAAH@Z'),
              'decode_orig': 0x48C800,
              'decode_ours': ('map',
                              '?DecodeEffect@CGameEffect@@SAPAV1@PAUItem_effect_st@@ABVCPoint@@H1@Z'),
              'ctorcopy_orig': 0x78A9D0,
              'ctorcopy_ours': ('map', '??0CResRef@@QAE@PAE@Z'),
              'getdeny_orig': 0x599C70,
              'getdeny_ours': ('map',
                               '?GetDeny@CGameObjectArray@@QAEEHEPAPAVCGameObject@@I@Z'),
              'releasedeny_orig': 0x59A010,
              'releasedeny_ours': ('map',
                                   '?ReleaseDeny@CGameObjectArray@@QAEEHEI@Z'),
              'caster_orig': 0x75E940,
              'caster_ours': ('map',
                              '?GetCasterLevel@CGameSprite@@QAEFPAVCSpell@@EI@Z'),
              'feat_orig': 0x762E20,
              'feat_ours': ('map',
                            '?GetFeatValue@CGameSprite@@QAEHI@Z'),
              'finalop_orig': 0x78AC50,
              'finalop_ours': ('map', '??4CResRef@@QAE?AV0@PAE@Z'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z'),
              # ours-only inline_ok: the accessors replace vanilla's inline
              # chitin reads; rets are scripted in build()
              'gog_ours': ('map',
                           '?GetObjectGame@CBaldurChitin@@QAEPAVCInfGame@@XZ'),
              'goa_ours': ('map',
                           '?GetObjectArray@CInfGame@@QAEPAVCGameObjectArray@@XZ'),
              'spellctor_ours': ('map', '??0CSpell@@QAE@XZ'),
              'opneq_ours': ('map', '??9CResRef@@QBEHABV0@@Z')},
    'returns': 'u32',
}


def _wire(inp):
    b = bytearray(WIRE_SIZE)
    b[0:3] = b'\xAA\xBB\xCC'                    # message sub-header (skipped)
    d = lambda v: v & 0xFFFFFFFF
    struct.pack_into('<I', b, 0x03, d(inp['player']))
    struct.pack_into('<I', b, 0x07, d(inp['remote']))
    struct.pack_into('<H', b, 0x0B, inp['effectID'] & 0xFFFF)
    b[0x0D] = inp['targetType'] & 0xFF
    b[0x0E] = inp['spellLevel'] & 0xFF
    struct.pack_into('<I', b, 0x0F, d(inp['effectAmount']))
    struct.pack_into('<I', b, 0x13, d(inp['dwFlags']))
    struct.pack_into('<H', b, 0x17, inp['durationType'] & 0xFFFF)
    struct.pack_into('<I', b, 0x19, d(inp['duration']))
    b[0x1D] = inp['probUpper'] & 0xFF
    b[0x1E] = inp['probLower'] & 0xFF
    b[0x1F:0x27] = inp['res']
    b[0x27:0x2F] = inp['sourceRes']
    struct.pack_into('<I', b, 0x2F, d(inp['numDice']))
    struct.pack_into('<I', b, 0x33, d(inp['diceSize']))
    struct.pack_into('<I', b, 0x37, d(inp['savingThrow']))
    struct.pack_into('<I', b, 0x3B, d(inp['saveMod']))
    struct.pack_into('<I', b, 0x3F, d(inp['special']))
    struct.pack_into('<I', b, 0x43, d(inp['srcx']))
    struct.pack_into('<I', b, 0x47, d(inp['srcy']))
    struct.pack_into('<I', b, 0x4B, d(inp['player2']))
    struct.pack_into('<I', b, 0x4F, d(inp['remote2']))
    struct.pack_into('<I', b, 0x53, d(inp['tgtx']))
    struct.pack_into('<I', b, 0x57, d(inp['tgty']))
    b[0x5B] = inp['field188'] & 0xFF
    return bytes(b)


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x10)
    eff = blk_alloc(0x18C)                  # fake CGameEffect (DecodeEffect ret)
    vtbl = blk_alloc(0x20)                  # fake vtable, GetObjectType at +4
    vstub = blk_alloc(4)                    # scratch dispatch target
    wire = blk_alloc(WIRE_SIZE)
    dwsize = blk_alloc(4)
    chitin = blk_alloc(VAN_GAME_MEMBER + 4)
    game = blk_alloc(0x3900)
    pobj = blk_alloc(0x78)                  # fake CGameSprite (GetDeny out)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x10,
                [('pData', wire, WIRE_SIZE, 'ref'),
                 ('dwSize', dwsize, 4, 'val')])
    off = ctx['off'][side]
    m.set_this(off['m_targetId'], struct.pack('<I', 0x11111111))  # <- localId
    m.set_this(off['m_effect'], struct.pack('<I', 0x33333333))    # <- eff
    m.uc.mem_write(dwsize, struct.pack('<I', WIRE_SIZE))
    m.uc.mem_write(wire, _wire(inp))
    m.uc.mem_write(chitin + VAN_GAME_MEMBER, struct.pack('<I', game))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))
    # eff seeds: +0x10C is READ (GetDeny/ReleaseDeny sourceId arg); the rest
    # are sentinels for the code's writes (offsets code-identical both sides)
    m.uc.mem_write(eff + 0x10C, struct.pack('<i', inp['sourceId']))
    m.uc.mem_write(eff + 0x44, b'\x44' * 4)    # damage <- wire special
    m.uc.mem_write(eff + 0x90, b'\x55' * 8)    # final op= this
    m.uc.mem_write(eff + 0xC4, b'\x66' * 4)    # casterLevel / fail flag
    m.uc.mem_write(eff + 0xC8, b'\x77' * 4)    # GetFeatValue ret << 1
    m.uc.mem_write(eff + 0x188, b'\x88' * 4)   # <- wire field188
    m.uc.mem_write(pobj, struct.pack('<I', vtbl))
    m.uc.mem_write(vtbl + 4, struct.pack('<I', vstub))

    find = {'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
            'addr': addrs['find_' + side], 'conv': 'thiscall',
            'nargs': 3, 'pop': 12,
            'default': {'writes': [(2, [inp['localId']])], 'ret': 1}}
    if inp['find_rets']:
        find['rets'] = inp['find_rets']
    m.add_stub(find)
    m.add_stub({'name': 'CGameEffect::DecodeEffect', 'kind': 'code',
                'addr': addrs['decode_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'default': {'ret': eff}})
    m.add_stub({'name': 'CResRef::CResRef(BYTE*)', 'kind': 'code',
                'addr': addrs['ctorcopy_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': 0}})
    getdeny = {'name': 'CGameObjectArray::GetDeny', 'kind': 'code',
               'addr': addrs['getdeny_' + side], 'conv': 'thiscall',
               'nargs': 4, 'pop': 16,
               'default': {'writes': [(2, [pobj])], 'ret': 0},
               'arg_masks': [None, None, 0xFF, None, None]}  # BYTE thread
    if inp['getdeny_rets']:
        getdeny['rets'] = inp['getdeny_rets']
    m.add_stub(getdeny)
    m.add_stub({'name': 'CGameObject::GetObjectType', 'kind': 'code',
                'addr': vstub, 'conv': 'thiscall', 'nargs': 0, 'pop': 0,
                'default': {'ret': inp['objtypes'][-1]},
                'rets': inp['objtypes']})
    m.add_stub({'name': 'CGameSprite::GetCasterLevel', 'kind': 'code',
                'addr': addrs['caster_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12, 'default': {'ret': inp['caster']},
                'arg_masks': [None, None, 0xFF, None, None]})
    m.add_stub({'name': 'CGameSprite::GetFeatValue', 'kind': 'code',
                'addr': addrs['feat_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': inp['feat']}})
    m.add_stub({'name': 'CGameObjectArray::ReleaseDeny', 'kind': 'code',
                'addr': addrs['releasedeny_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12, 'default': {'ret': 0},
                'arg_masks': [None, None, 0xFF, None]})
    m.add_stub({'name': 'CResRef::operator=(BYTE*)', 'kind': 'code',
                'addr': addrs['finalop_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    if side == 'orig':
        # live on every full path; no ours counterpart at these sites
        for nm, a, n, p in (('CResRef::CResRef()', VAN_CTOR0, 0, 0),
                            ('CResRef::operator=(char*)', VAN_OPCHAR, 2, 8),
                            ('CSpell::SetResRef', VAN_SETRESREF, 3, 12)):
            m.add_stub({'name': nm, 'kind': 'code', 'addr': a,
                        'conv': 'thiscall', 'nargs': n, 'pop': p,
                        'default': {'ret': 0}, 'inline_ok': True})
    else:
        # accessors scripted so the REAL ours lea/call sites produce the
        # vanilla this values -- no ours member literals anywhere
        m.add_stub({'name': 'CBaldurChitin::GetObjectGame', 'kind': 'code',
                    'addr': addrs['gog_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0,
                    'default': {'ret': game + 0x52},   # 0x375A - 0x3708
                    'inline_ok': True})
        m.add_stub({'name': 'CInfGame::GetObjectArray', 'kind': 'code',
                    'addr': addrs['goa_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0,
                    'default': {'ret': game + 0x372C},
                    'inline_ok': True})
        m.add_stub({'name': 'CSpell::CSpell', 'kind': 'code',
                    'addr': addrs['spellctor_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0, 'default': {'ret': 0},
                    'inline_ok': True})
        m.add_stub({'name': 'CResRef::operator!=', 'kind': 'code',
                    'addr': addrs['opneq_ours'], 'conv': 'thiscall',
                    'nargs': 1, 'pop': 4, 'default': {'ret': 0},
                    'inline_ok': True})   # ret 0 -> fetch region skipped
    return m


W = dict(
    player=0x11223344, remote=-700, effectID=0x0C, targetType=0xFF,
    spellLevel=0x80, effectAmount=0xFFFFFFFF, dwFlags=0xDEADBEEF,
    durationType=0x8000, duration=0x80000000, probUpper=0xFF,
    probLower=0x00, res=b'\xff' * 8, sourceRes=b'ABCDEFGH',
    numDice=0xFFFFFFFF, diceSize=0x7FFFFFFF, savingThrow=-1,
    saveMod=-0x80000000, special=0xCAFEBABE, srcx=-0x80000000,
    srcy=0x7FFFFFFF, player2=0x11111111, remote2=0x22222222,
    tgtx=0, tgty=-1, field188=0xFF,
    localId=0x6C1F, sourceId=0x33333333,
    find_rets=[1, 1], getdeny_rets=[0], objtypes=[0x51, 0x51],
    caster=5, feat=5,
)

SETS = {
    # Find#1 misses (`cmp eax,1` shape): straight FALSE, nothing else fires
    'set1-find1-miss': dict(W, find_rets=[0]),
    # full path, sprite, damage: effectID 0xC writes eff+0x44; caster 0x8000
    # sign-extends into eff+0xC4, feat 0x40000000 shl-overflow into eff+0xC8
    'set2-happy-sprite-damage': dict(W, caster=0x8000, feat=0x40000000),
    # marker (player2 == 0 && remote2 == INVALID) skips Find#2; objType 0x01
    # passes the AIBASE mask but is not a sprite: eff+0xC4 = 1, no caster
    # /feat; negative localId into m_targetId
    'set3-marker-nosprite': dict(W, find_rets=[1], objtypes=[0x01, 0x01],
                                 player2=0, remote2=-1, effectID=0x10,
                                 localId=-0x1234),
    # Find#2 misses (`test eax,eax` shape): remote2 == INVALID is NOT a
    # marker when player2 != 0; FALSE before DecodeEffect
    'set4-find2-miss': dict(W, find_rets=[1, 0], player2=0x7FFFFFFF,
                            remote2=-1, effectID=0x0C),
    # GetDeny SHARED(4) once then terminal 2: loop re-runs GetDeny, no
    # vtable dispatch, no ReleaseDeny, eff+0xC4 = 1
    'set5-retry-terminal': dict(W, getdeny_rets=[4, 2], effectID=0x0F),
    # objType 0x02 fails the AIBASE byte mask: ONE virtual call, eff+0xC4
    # = 1, still ReleaseDeny
    'set6-maskfail-one-vcall': dict(W, objtypes=[0x02], effectID=0x0E),
}
