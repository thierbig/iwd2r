"""0x516D50 CMessageSpellLevelImmumityUpdate::MarshalMessage(BYTE** pData, DWORD* dwSize).

    UTIL_ASSERT(pData != NULL && dwSize != NULL);      (never fires here)
    do { rc = game->GetObjectArray()->GetShare(m_targetId, THREAD_ASYNCH,
                                               &pObject, INFINITE); }
    while (rc == SHARED || rc == DENIED);
    if (rc != SUCCESS) { *dwSize = 0; return; }
    playerId  = pObject->m_remotePlayerID;             (+0x62 vanilla / +0x6C ours)
    objectId  = pObject->m_remoteObjectID;             (+0x66 vanilla / +0x70 ours)
    ReleaseShare(m_targetId, THREAD_ASYNCH, INFINITE);
    *dwSize = 0x30;                                    (STATICBUFFERSIZE-asserted)
    (*pData)[0] = playerId;  (*pData)[4] = objectId;
    memcpy(*pData + 8, &m_spellLevelImmunities, 40);   (10 dwords)

Byte-for-byte the CMessageUnsummon::MarshalMessage shape (c512830) with
a wider payload: same do-while over GetShare rc in {SHARED, DENIED},
same miss path that stores *dwSize = 0 and returns, same object-array
walk.  The GetShare out-param is the caller's pData argument slot (lea
ecx,[esp+0x10] at entry-12), so the stub's slot-2 write IS the pObject
local the success path then reads with mov eax,[esp+0xC] -- ebp keeps
the original ppData read at entry, which is how the body still finds the
buffer after the slot was overwritten.  ppData points into its own arg
block (+8), so the 0x30-byte payload lands in the compared
'arg:ppData' region.

Setup identical to c512830: one game block carved per side so the
embedded CGameObjectArray sits at gamebase+0x372C both sides (ours'
game pointer is gamebase+0x68 so +0x36C4 lands there too); pObject's
remote ids preseeded at the per-side offsets; vanilla's helpers
(GetObjectGame/GetObjectArray) run for real on the fixture blocks.

Stubs: GetShare thiscall nargs=4 pop 16 (hit writes pobj to slot 2,
ret SUCCESS; miss ret 2; retry scripts rets [5] = DENIED once then the
hit default); ReleaseShare nargs=3 pop 12 plain ret 0, both BYTE-flag
arg_masks per c512830; UtilAssert cdecl noreturn.  The blob copied at
+0xC is whatever the case preseeded, so the copy itself is compared.

Returns 'void' -- `virtual void MarshalMessage(BYTE**, DWORD*)`
(src/CMessage.h); EAX is not part of the contract.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_ARRAY = {'orig': 0x372C, 'ours': 0x36C4}   # embedded array offset
POBJ_PLAYER = {'orig': 0x62, 'ours': 0x6C}
POBJ_OBJECT = {'orig': 0x66, 'ours': 0x70}
GAME_SHIFT = GAME_ARRAY['orig'] - GAME_ARRAY['ours']

CASE = {
    'name': 'CMessageSpellLevelImmumityUpdate::MarshalMessage',
    'short': 'SpellLevelImmunity::Marshal',
    'exe': 0x516D50,
    'ours': 'CMessageSpellLevelImmumityUpdate::MarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageSpellLevelImmumityUpdate',
    'this_size': 0x34,
    'members': ('m_targetId', 'm_sourceId', 'm_spellLevelImmunities'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'getshare_orig': 0x599A50,
              'getshare_ours': ('map',
                                '?GetShare@CGameObjectArray@@QAEEHEPAPAVCGameObject@@I@Z'),
              'releaseshare_orig': 0x599E70,
              'releaseshare_ours': ('map',
                                    '?ReleaseShare@CGameObjectArray@@QAEEHEI@Z'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x34)
    ppdata = blk_alloc(0x40)              # [0] -> buffer at +8; 0x30 payload
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    pobj = blk_alloc(0x78)                # covers m_remote*ID on both layouts
    game_side = game + (GAME_SHIFT if side == 'ours' else 0)
    blob = struct.pack('<10i', *inp['blob'])
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x34,
                [('ppData', ppdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'ref')])
    m.set_this(ctx['off'][side]['m_targetId'], struct.pack('<i', inp['targetId']))
    m.set_this(ctx['off'][side]['m_spellLevelImmunities'], blob)
    m.uc.mem_write(ppdata, struct.pack('<I', ppdata + 8))
    m.uc.mem_write(dwsize, struct.pack('<I', 0xEEEEEEEE))  # 0x30 / 0 must show
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(pobj + POBJ_PLAYER[side], struct.pack('<I', inp['playerId']))
    m.uc.mem_write(pobj + POBJ_OBJECT[side], struct.pack('<i', inp['objectId']))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))

    hit = {'writes': [(2, [pobj])], 'ret': 0}
    gs = {'name': 'CGameObjectArray::GetShare', 'kind': 'code',
          'addr': addrs['getshare_' + side], 'conv': 'thiscall',
          'nargs': 4, 'pop': 16, 'default': hit if inp['hit'] else {'ret': 2},
          'arg_masks': [None, None, 0xFF, None, None]}   # BYTE flag (arg 1)
    if inp['retry']:
        gs['rets'] = [5]                 # DENIED once, then SUCCESS
    m.add_stub(gs)
    m.add_stub({'name': 'CGameObjectArray::ReleaseShare', 'kind': 'code',
                'addr': addrs['releaseshare_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12, 'default': {'ret': 0},
                'arg_masks': [None, None, 0xFF, None]})   # BYTE flag (arg 1)
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    return m


SETS = {
    'set1-hit': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                     hit=True, retry=False,
                     blob=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]),
    'set2-miss': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                      hit=False, retry=False,
                      blob=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]),
    'set3-retry-denied': dict(targetId=0x5678, playerId=0xBEEF, objectId=-700,
                              hit=True, retry=True,
                              blob=[-1, 0, -0x80000000, 0x7FFFFFFF, -12345,
                                    6789, -255, 256, -65536, 0x10000]),
    'set4-hit-zero-blob': dict(targetId=-2, playerId=0, objectId=0,
                               hit=True, retry=False, blob=[0] * 10),
}
