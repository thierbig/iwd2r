"""0x513F00 CMessage97::MarshalMessage(BYTE** pData, DWORD* dwSize).

    do { rc = game->GetObjectArray()->GetShare(m_targetId, THREAD_ASYNCH,
                                               &pObject, INFINITE); }
    while (rc == SHARED || rc == DENIED);
    if (rc == SUCCESS) {
        id  = pObject->m_remotePlayerID  (+0x62)
        oid = pObject->m_remoteObjectID  (+0x66)
        ReleaseShare(m_targetId, THREAD_ASYNCH, INFINITE);
        *dwSize = 20;
        *pData[0] = id; *pData[4] = oid;          (asserts inline)
        *pData[8] = m_nScript;                    (this +0xC)
        memcpy(*pData + 12, m_resScript.GetResRef(), 8);   (this +0x10)
    } else { *dwSize = 0; }

Returns 'void' -- `virtual void MarshalMessage(BYTE**, DWORD*)`
(src/CMessage.h:531), so EAX is not part of the contract.  The house
shape is CMessageUnsummon::MarshalMessage's (0x512830, case c512830)
with a wider payload: the marshal key is m_targetId (+0x4) and the wire
is 20 bytes -- (playerId, objectId, nScript, resref[8]).

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

The resref copy differs in CODE but not in state: vanilla inlines the
8 bytes as two dword loads (0x513FFF..0x51400A), ours calls
CResRef::GetResRef (0x416BDA -> 0x69BC90: `mov eax,ecx; ret` -- returns
&m_resScript, runs for real, unstubbed, no writes) and then does the
same two dword stores.  The STATICBUFFERSIZE assert also differs
harmlessly: vanilla compares 20 against the .data dword at 0x8ABE0C
(file-initialized 0x4000), ours folds the constant comparison away --
it could never fire on either side.

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
              that still holds the game pointer (0x64D182) and pushes
              full ECX, so the pushed dword's upper bits are garbage the
              x86 ABI leaves unspecified; vanilla zeroes them
              (`xor edx,edx; mov dl,byte[0x851873]`, 0x513F38).
  ReleaseShare thiscall nargs=3, plain ret 0 (BYTE); flag masked to BYTE
              for the same reason (ours zeroes it there -- movzx -- so the
              mask changes nothing on either side).
  UtilAssert  cdecl nargs=4, noreturn: only an assert trip reaches it;
              a valid set never does, and if one side tripped while the
              other returned, the terminated-vs-returned comparison
              flags the DIFF.

No filters beyond the runner's default (stack/TIB writes dropped, so the
stub's write to the caller's pObject local is setup, not state); nothing
is inline_ok.  pData points into its own arg block (+8), so all 20
payload bytes land in the compared 'arg:ppData' region.

Members m_targetId/m_sourceId (+0x4/+0x8, CMessage base) and
m_nScript/m_resScript (+0xC/+0x10, the CMessage97 tail) at identical
offsets both sides (identity field map, like c4f6bc0); this_size 0x18.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_ARRAY = {'orig': 0x372C, 'ours': 0x36C4}   # embedded array offset
POBJ_PLAYER = {'orig': 0x62, 'ours': 0x6C}
POBJ_OBJECT = {'orig': 0x66, 'ours': 0x70}
GAME_SHIFT = GAME_ARRAY['orig'] - GAME_ARRAY['ours']   # ours' game ptr shift

CASE = {
    'name': 'CMessage97::MarshalMessage',
    'short': 'CMessage97::Marshal',
    'exe': 0x513F00, 'ours': 'CMessage97::MarshalMessage',
    'convention': 'thiscall', 'this_class': 'CMessage97',
    'this_size': 0x18,
    'members': ('m_targetId', 'm_sourceId', 'm_nScript', 'm_resScript'),
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
    this = blk_alloc(0x18)
    ppdata = blk_alloc(0x20)             # [0] -> buffer at +8; 20-byte payload
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)             # one base, carved per side (docstring)
    pobj = blk_alloc(0x78)               # covers m_remote*ID on both layouts
    game_side = game + (GAME_SHIFT if side == 'ours' else 0)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x18,
                [('ppData', ppdata, 0x20, 'ref'), ('dwSize', dwsize, 4, 'ref')])
    m.set_this(ctx['off'][side]['m_targetId'],
               struct.pack('<i', inp['targetId']))
    m.set_this(ctx['off'][side]['m_nScript'],
               struct.pack('<i', inp['nScript']))
    m.set_this(ctx['off'][side]['m_resScript'], inp['resref'])
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
    'set1-hit': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                     nScript=0x2A5A5, resref=b'GUIClient',
                     hit=True, retry=False),
    'set2-miss': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                      nScript=0x2A5A5, resref=b'GUIClient',
                      hit=False, retry=False),
    'set3-retry-denied': dict(targetId=0x5678, playerId=0xBEEF,
                              objectId=-700, nScript=-3, resref=b'SPWIN9\xff',
                              hit=True, retry=True),
}
