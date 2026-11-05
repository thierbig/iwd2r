"""0x4F8470 CMessageAddEffect::MarshalMessage(BYTE** pData, DWORD* dwSize).

    UTIL_ASSERT(pData != NULL && dwSize != NULL);   (never fires here)
    CResRef localRef;                               (default ctor, stubbed)
    do { rc = GetShare(m_targetId, THREAD_ASYNCH, &pObject, INFINITE); }
    while (rc == SHARED || rc == DENIED);
    if (rc != SUCCESS) { *dwSize = 0; goto tail; }  (early tail: both dtors,
                                                     NO wire stores at all)
    playerId = pObject->m_remotePlayerID;           (+0x62 vanilla / +0x6C ours)
    objectId = pObject->m_remoteObjectID;           (+0x66 vanilla / +0x70 ours)
    ReleaseShare(m_targetId, THREAD_ASYNCH, INFINITE);
    localRef  = m_effect->field_2C;                 (CResRef copy-assign)
    localRef.CopyToString(&localStr1);              (stubbed callee)
    localRef2 = m_effect->field_90;                 (second CResRef)
    localRef2.CopyToString(&localStr2);
    *dwSize = 0x59;                                 (FIXED -- zero-padded format,
                                                     unlike c511d00's sized wire)
    UTIL_ASSERT(*dwSize <= STATICBUFFERSIZE);       (never fires here)
    wire[0] = playerId;  wire[4] = objectId;
    wire[8] = (WORD)field_0C;   wire[0xA] = (BYTE)field_10;
    wire[0xB] = (BYTE)field_14; wire[0xC] = field_18;   wire[0x10] = field_1C;
    wire[0x14] = (WORD)field_20; wire[0x16] = field_24; (unaligned dword)
    wire[0x1A] = (BYTE)field_28; wire[0x1B] = (BYTE)field_2A;
    zero 8 bytes at wire+0x1C; memcpy(wire+0x1C, GetBuffer(localStr1), len1);
    zero 8 bytes at wire+0x24; memcpy(wire+0x24, GetBuffer(localStr2), len2);
    if (m_effect->UsesDice()) {                     (VIRTUAL -- vtable slot 7)
        wire[0x2C] = field_34;  wire[0x30] = field_38;
    } else {
        wire[0x2C] = field_54;  wire[0x30] = field_50;  (swapped source order)
    }
    wire[0x34] = field_3C; wire[0x38] = field_40; wire[0x3C] = field_44;
    wire[0x40] = field_7C; wire[0x44] = field_80;
    do { rc = GetShare(field_10C, THREAD_ASYNCH, &pObject2, INFINITE); }
    while (SHARED || DENIED);
    if (SUCCESS) { p2 = pObject2 ids; ReleaseShare(field_10C, ...); }
    else { p2 = (0, image default); }               (vanilla 0x851878 / ours
                                                     0xbf386c, both -1)
    wire[0x48] = p2.playerId; wire[0x4C] = p2.objectId;
    wire[0x50] = field_84; wire[0x54] = field_88; wire[0x58] = (BYTE)field_188;
    UTIL_ASSERT(cnt == *dwSize);                    (never fires -- const 0x59)
    tail: ~localStr2(); ~localStr1();

The GetShare family of c511d00 with a CGameEffect payload and the first
VIRTUAL dispatch: m_effect is a blk_alloc'd fake whose +0 holds a 0x20-byte
fake vtable, and slot 7 (+0x1C, CGameEffect::UsesDice per src/CGameEffect.h)
points at a scratch block carrying a kind:'code' stub -- the `call dword
ptr [eax+0x1C]` at vanilla 0x4F86E8 / ours 0x64D4F9 lands there, and the
stub's per-set return drives the dice branch (field_34/38 vs the swapped
field_54/50 pair).  The fake vtable is identical on both sides, so the
stub's this (m_effect) normalizes equal and stays KEPT-compared.

Single-default stub limitations, accepted: the two CopyToString calls share
one out-write, so both local CStrings hold the SAME data pointer (rdata) --
both copies are len(resref) bytes, the second sourced from GetBuffer ret #2
(adata), which must therefore carry >= len(resref) bytes; and both GetShare
hits return the SAME pobj block, so wire+0x48/+0x4C mirror wire[0]/[4] on a
double hit.  Neither weakens the field-mapping the copies pin.

CResRef::operator= is inline_ok on BOTH sides: VC6 materializes the first
call's source (field_2C) into a stack temp (0x4F8574-0x4F857F) where our
rebuild passes the embedded member directly -- compiler shape, not
behavior.  Ours' two extra CString default ctors and the imported memcpy
are the usual inline_ok asymmetries (c511d00).  Everything else -- the
CResRef ctor, both GetShare retry loops, both ReleaseShare, both
CopyToString, both GetBuffer, both CString dtors, UsesDice -- is KEPT with
equal call counts on both sides; ours' game accessors (0x40C680/0x4090AC)
run as real code over the c511d00 chitin/game seeding.

Returns 'void' -- `virtual void MarshalMessage(BYTE**, DWORD*)`
(src/CMessage.h); EAX is not part of the contract.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_ARRAY = {'orig': 0x372C, 'ours': 0x36C4}    # embedded array offset
POBJ_PLAYER = {'orig': 0x62, 'ours': 0x6C}
POBJ_OBJECT = {'orig': 0x66, 'ours': 0x70}
GAME_SHIFT = GAME_ARRAY['orig'] - GAME_ARRAY['ours']
WIRE_SIZE = 0x59                                 # the fixed *dwSize constant

CASE = {
    'name': 'CMessageAddEffect::MarshalMessage',
    'short': 'AddEffect::Marshal',
    'exe': 0x4F8470,
    'ours': 'CMessageAddEffect::MarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageAddEffect',
    'this_size': 0x10,
    'members': ('m_targetId', 'm_effect'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'resrefctor_orig': 0x78A960,
              'resrefctor_ours': ('map', '??0CResRef@@QAE@XZ'),
              'resrefassign_orig': 0x78AC30,
              'resrefassign_ours': ('map',
                                    '??4CResRef@@QAE?AV0@ABV0@@Z'),
              'getshare_orig': 0x599A50,
              'getshare_ours': ('map',
                                '?GetShare@CGameObjectArray@@QAEEHEPAPAVCGameObject@@I@Z'),
              'releaseshare_orig': 0x599E70,
              'releaseshare_ours': ('map',
                                    '?ReleaseShare@CGameObjectArray@@QAEEHEI@Z'),
              'copytostring_orig': 0x78A9F0,
              'copytostring_ours': ('map',
                                    '?CopyToString@CResRef@@QBEXAAVCString@@@Z'),
              'getbuffer_orig': 0x7FCFFE,
              'getbuffer_ours': ('map', '?GetBuffer@CString@@QAEPADH@Z'),
              'cstringdtor_orig': 0x7FCC1A,
              'cstringdtor_ours': ('map', '??1CString@@QAE@XZ'),
              'ctor0arg_ours': ('map', '??0CString@@QAE@XZ'),
              # our rebuild imports memcpy; vanilla inlines rep movsd+movsb.
              'memcpyiat_ours': ('import', 'memcpy'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x10)
    eff = blk_alloc(0x18C)                    # fake CGameEffect
    vtbl = blk_alloc(0x20)                    # fake vtable, slot 7 at +0x1C
    vstub = blk_alloc(4)                      # scratch UsesDice target
    ppdata = blk_alloc(8 + WIRE_SIZE)         # [0] -> wire buffer at +8
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)                  # one base, carved per side
    pobj = blk_alloc(0x78)                    # shared by both GetShare hits
    rblk = blk_alloc(8 + max(8, len(inp['resref'])))
    ablk = blk_alloc(8 + max(8, len(inp['area']), len(inp['resref'])))
    rdata, adata = rblk + 8, ablk + 8
    game_side = game + (GAME_SHIFT if side == 'ours' else 0)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x10,
                [('ppData', ppdata, 8 + WIRE_SIZE, 'ref'),
                 ('dwSize', dwsize, 4, 'ref')])
    off = ctx['off'][side]
    m.set_this(off['m_targetId'], struct.pack('<i', inp['targetId']))
    m.set_this(off['m_effect'], struct.pack('<I', eff))
    m.uc.mem_write(eff, struct.pack('<I', vtbl))
    m.uc.mem_write(vtbl + 0x1C, struct.pack('<I', vstub))
    m.uc.mem_write(ppdata, struct.pack('<I', ppdata + 8))
    m.uc.mem_write(dwsize, struct.pack('<I', 0xEEEEEEEE))  # 0x59 / 0 must show
    m.uc.mem_write(rblk, struct.pack('<I', len(inp['resref'])))
    m.uc.mem_write(rdata, inp['resref'])
    m.uc.mem_write(ablk, struct.pack('<I', len(inp['area'])))
    m.uc.mem_write(adata, inp['area'])        # padded: copy 2 takes len1 bytes
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(pobj + POBJ_PLAYER[side], struct.pack('<i', inp['playerId']))
    m.uc.mem_write(pobj + POBJ_OBJECT[side], struct.pack('<i', inp['objectId']))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))

    e = inp['eff']                            # seed the CGameEffect payload
    m.uc.mem_write(eff + 0x0C, struct.pack('<H', e['w0C']))
    m.uc.mem_write(eff + 0x10, struct.pack('<B', e['b10']))
    m.uc.mem_write(eff + 0x14, struct.pack('<B', e['b14']))
    m.uc.mem_write(eff + 0x18, struct.pack('<I', e['d18']))
    m.uc.mem_write(eff + 0x1C, struct.pack('<I', e['d1C']))
    m.uc.mem_write(eff + 0x20, struct.pack('<H', e['w20']))
    m.uc.mem_write(eff + 0x24, struct.pack('<I', e['d24']))
    m.uc.mem_write(eff + 0x28, struct.pack('<B', e['b28']))
    m.uc.mem_write(eff + 0x2A, struct.pack('<B', e['b2A']))
    m.uc.mem_write(eff + 0x2C, b'\x11' * 8)   # CResRef #1 (assignee stubbed)
    m.uc.mem_write(eff + 0x34, struct.pack('<i', e['nz1']))
    m.uc.mem_write(eff + 0x38, struct.pack('<i', e['nz2']))
    m.uc.mem_write(eff + 0x3C, struct.pack('<I', e['d3C']))
    m.uc.mem_write(eff + 0x40, struct.pack('<I', e['d40']))
    m.uc.mem_write(eff + 0x44, struct.pack('<I', e['d44']))
    m.uc.mem_write(eff + 0x50, struct.pack('<i', e['z2']))
    m.uc.mem_write(eff + 0x54, struct.pack('<i', e['z1']))
    m.uc.mem_write(eff + 0x7C, struct.pack('<I', e['d7C']))
    m.uc.mem_write(eff + 0x80, struct.pack('<I', e['d80']))
    m.uc.mem_write(eff + 0x84, struct.pack('<I', e['d84']))
    m.uc.mem_write(eff + 0x88, struct.pack('<I', e['d88']))
    m.uc.mem_write(eff + 0x90, b'\x22' * 8)   # CResRef #2
    m.uc.mem_write(eff + 0x10C, struct.pack('<i', inp['sourceId2']))
    m.uc.mem_write(eff + 0x188, struct.pack('<B', e['b188']))

    m.add_stub({'name': 'CResRef::CResRef', 'kind': 'code',
                'addr': addrs['resrefctor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    # one stub serves both GetShare sites; rets script the retry / miss mixes
    rets = []
    if inp['retry1']:
        rets.append(5)                        # DENIED once at site #1
    if inp['hit1'] and not inp['hit2']:
        rets += [0, 2]                        # site #1 hit, site #2 miss
    gs = {'name': 'CGameObjectArray::GetShare', 'kind': 'code',
          'addr': addrs['getshare_' + side], 'conv': 'thiscall',
          'nargs': 4, 'pop': 16,
          'default': ({'writes': [(2, [pobj])], 'ret': 0}
                      if inp['hit1'] else {'ret': 2}),
          'arg_masks': [None, None, 0xFF, None, None]}   # BYTE flag (arg 1)
    if rets:
        gs['rets'] = rets
    m.add_stub(gs)
    m.add_stub({'name': 'CGameObjectArray::ReleaseShare', 'kind': 'code',
                'addr': addrs['releaseshare_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12, 'default': {'ret': 0},
                'arg_masks': [None, None, 0xFF, None]})   # BYTE flag (arg 1)
    m.add_stub({'name': 'CResRef::CopyToString', 'kind': 'code',
                'addr': addrs['copytostring_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4,
                'default': {'writes': [(0, [rdata])], 'ret': 0}})
    m.add_stub({'name': 'CString::GetBuffer', 'kind': 'code',
                'addr': addrs['getbuffer_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'rets': [rdata, adata]})
    m.add_stub({'name': 'CGameEffect::UsesDice', 'kind': 'code',
                'addr': vstub, 'conv': 'thiscall', 'nargs': 0, 'pop': 0,
                'default': {'ret': inp['virt']}})
    m.add_stub({'name': 'CString::~CString', 'kind': 'code',
                'addr': addrs['cstringdtor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    # inline_ok on BOTH sides: VC6 materializes call #1's source into a stack
    # temp (0x4F8574) where ours passes m_effect+0x2C directly -- codegen.
    m.add_stub({'name': 'CResRef::operator=', 'kind': 'code',
                'addr': addrs['resrefassign_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0},
                'inline_ok': True})
    if side == 'ours':
        # vanilla default-constructs both local CStrings inline (zero calls);
        # ours calls the imported ctor twice on stack locals.
        m.add_stub({'name': 'CString::CString', 'kind': 'code',
                    'addr': addrs['ctor0arg_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0, 'default': {'ret': 0},
                    'inline_ok': True})
        # both wire copies: vanilla rep movsd+movsb inline, ours memcpy IAT
        m.add_stub({'name': 'memcpy', 'kind': 'iat', 'addr': addrs['memcpyiat_ours'],
                    'conv': 'cdecl', 'nargs': 3, 'pop': 0,
                    'action': 'copy', 'inline_ok': True})
    return m


EFF = dict(w0C=0x1234, b10=0x55, b14=0x00, d18=1000, d1C=2000, w20=300,
           d24=4000, b28=0x7F, b2A=0xFF,
           nz1=111, nz2=-222, z1=-333, z2=444,
           d3C=11, d40=22, d44=33, d7C=44, d80=55, d84=66, d88=77, b188=0x42)

SETS = {
    # both hits, UsesDice true: duration pair from field_34/38; 8-char resref
    'set1-hit-hit-dice': dict(targetId=0x1234, sourceId2=0x5678,
                              playerId=0xAAAA, objectId=0x3333,
                              hit1=True, retry1=False, hit2=True, virt=1,
                              resref=b'SPFIRE33', area=b'AR5000',
                              eff=dict(EFF)),
    # GetShare #1 misses: *dwSize = 0, no wire stores, straight to the dtor
    # tail -- UsesDice / second GetShare / strings never reached
    'set2-miss-first': dict(targetId=0x1234, sourceId2=0x5678,
                            playerId=0xBEEF, objectId=-700,
                            hit1=False, retry1=False, hit2=True, virt=1,
                            resref=b'', area=b'', eff=dict(EFF)),
    # DENIED once at site #1 then through; UsesDice false: the SWAPPED pair
    # (wire+0x2C <- field_54, wire+0x30 <- field_50); odd string lengths
    'set3-retry-dice0': dict(targetId=-2, sourceId2=0x99,
                             playerId=0x7FFFFFFF, objectId=-0x80000000,
                             hit1=True, retry1=True, hit2=True, virt=0,
                             resref=b'ODD', area=b'AREAX',
                             eff=dict(EFF, w0C=0x8000, b10=0xFF, z1=-100,
                                      z2=-200, b188=0x00)),
    # site #1 hits, site #2 misses: wire+0x48 = 0, wire+0x4C = -1 (image
    # default); empty strings make both copies zero-length
    'set4-hit-miss-second': dict(targetId=0, sourceId2=-1,
                                 playerId=-0x80000000, objectId=0x7FFFFFFF,
                                 hit1=True, retry1=False, hit2=False,
                                 virt=0x7FFFFFFF,
                                 resref=b'', area=b'',
                                 eff=dict(EFF, d18=0xFFFFFFFF, d1C=0,
                                          b28=0x00, b2A=0x00)),
    # full-width payload: extreme words/bytes/dwords, dice ret with the sign
    # bit set, dword-aligned resref
    'set5-fullwidth': dict(targetId=0x7FFFFFFF, sourceId2=-0x80000000,
                           playerId=0, objectId=-1,
                           hit1=True, retry1=False, hit2=True,
                           virt=0x80000000,
                           resref=b'12345678', area=b'AR99',
                           eff=dict(EFF, w0C=0xFFFF, b10=0x80, b14=0xFF,
                                    d18=0x80000000, d1C=0x7FFFFFFF,
                                    w20=0x8000, d24=0xFFFFFFFF, b28=0x01,
                                    b2A=0x80, nz1=-0x80000000,
                                    nz2=0x7FFFFFFF, z1=-1, z2=1,
                                    d3C=0xDEADBEEF, d40=0, d44=1,
                                    d7C=0xCAFEBABE, d80=0x80000001,
                                    d84=0xFFFFFFFF, d88=0x7FFFFFFF,
                                    b188=0xFF)),
}
