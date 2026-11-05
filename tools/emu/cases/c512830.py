"""0x512830 CMessageUnsummon::MarshalMessage(BYTE** pData, DWORD* dwSize).

    do { rc = game->GetObjectArray()->GetShare(m_summonedId, THREAD_ASYNCH,
                                               &pObject, INFINITE); }
    while (rc == SHARED || rc == DENIED);
    if (rc == SUCCESS) {
        id  = pObject->m_remotePlayerID  (+0x62)
        oid = pObject->m_remoteObjectID  (+0x66)
        ReleaseShare(m_summonedId, THREAD_ASYNCH, INFINITE);
        *dwSize = 8;  *pData[0] = id;  *pData[4] = oid;   (asserts inline)
    } else { *dwSize = 0; }

Returns 'void' -- `virtual void MarshalMessage(BYTE**, DWORD*)`
(src/CMessage.h:531), so EAX is not part of the contract.

Setup (block addresses identical both sides, so every compared pointer
normalizes equal).  g_pBaldurChitin is a setup write: vanilla's 0x8CF6DC
is in the zero tail of .data (probed), ours is the map symbol
?g_pBaldurChitin.  The chitin->game and game->array steps differ per
side, because our build's layout drifts from the offsets our headers
document (vanilla's): vanilla inlines [chitin+0x1C54] and game+0x372C,
while ours calls CBaldurChitin::GetObjectGame ([ecx+0x17EC]) and
CInfGame::GetObjectArray (lea ecx+0x36C4) -- the helpers run for real,
unstubbed, on the fixture blocks.  So one game block is carved per side
at the offsets that put the embedded CGameObjectArray at ONE fixed
address (gamebase+0x372C both sides: ours' game pointer is gamebase+0x68
so that +0x36C4 lands there too), and pObject's m_remotePlayerID /
m_remoteObjectID are preseeded at +0x62/+0x66 on vanilla but +0x6C/+0x70
on ours (same drift; each side reads its own member).  Wire payload and
every compared value are unaffected by the drift; it is a parity-gate
matter, recorded in the Task 74 report.

Stubs (same spec both sides; vanilla literal bodies vs ours' map names):
  GetShare    thiscall nargs=4 (index, flag, CGameObject**, timeout);
              the pObject out-param is stack slot 2, so a hit set writes
              the preseeded pObject block's address there and returns
              SUCCESS; the miss set returns 2 (an rc outside
              {0,4,5}) with no write; the retry set scripts rets [5]
              (DENIED once, then SUCCESS) -- both images run the same
              do-while, so both make the same two calls.  The flag is a
              BYTE parameter and arg_masks compares it at that width:
              ours' codegen loads it with `mov cl, [flag]` into an ECX
              that still holds the game pointer and pushes full ECX, so
              the pushed dword's upper bits are garbage the x86 ABI
              leaves unspecified (vanilla pushes a plain 0).
  ReleaseShare thiscall nargs=3, plain ret 0 (BYTE); flag masked to BYTE
              for the same reason (ours zeroes it there -- movzx -- so the
              mask changes nothing on either side).
  UtilAssert  cdecl nargs=4, noreturn: only an assert trip reaches it;
              a valid set never does, and if one side tripped while the
              other returned, the terminated-vs-returned comparison
              flags the DIFF.

No filters beyond the runner's default (stack/TIB writes dropped, so the
stub's write to the caller's pObject local is setup, not state); nothing
is inline_ok.  pData points into its own arg block (+8), so the payload
writes land in the compared 'arg:ppData' region.

Members m_targetId/m_sourceId/m_summonedId at identical offsets both
sides (identity field map, like c4f6bc0); the marshal key is
m_summonedId (+0xC).
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_ARRAY = {'orig': 0x372C, 'ours': 0x36C4}   # embedded array offset
POBJ_PLAYER = {'orig': 0x62, 'ours': 0x6C}
POBJ_OBJECT = {'orig': 0x66, 'ours': 0x70}
GAME_SHIFT = GAME_ARRAY['orig'] - GAME_ARRAY['ours']   # ours' game ptr shift

CASE = {
    'name': 'CMessageUnsummon::MarshalMessage',
    'short': 'Unsummon::Marshal',
    'exe': 0x512830, 'ours': 'CMessageUnsummon::MarshalMessage',
    'convention': 'thiscall', 'this_class': 'CMessageUnsummon',
    'this_size': 0x10,
    'members': ('m_targetId', 'm_sourceId', 'm_summonedId'),
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
    this = blk_alloc(0x10)
    ppdata = blk_alloc(0x20)             # [0] -> buffer at +8; payload lands here
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)             # one base, carved per side (docstring)
    pobj = blk_alloc(0x78)               # covers m_remote*ID on both layouts
    game_side = game + (GAME_SHIFT if side == 'ours' else 0)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x10,
                [('ppData', ppdata, 0x20, 'ref'), ('dwSize', dwsize, 4, 'ref')])
    m.set_this(ctx['off'][side]['m_summonedId'],
               struct.pack('<i', inp['summonedId']))
    m.uc.mem_write(ppdata, struct.pack('<I', ppdata + 8))
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
    'set1-hit': dict(summonedId=0x1234, playerId=0xAAAA, objectId=0x3333,
                     hit=True, retry=False),
    'set2-miss': dict(summonedId=0x1234, playerId=0xAAAA, objectId=0x3333,
                      hit=False, retry=False),
    'set3-retry-denied': dict(summonedId=0x5678, playerId=0xBEEF,
                              objectId=-700, hit=True, retry=True),
}
