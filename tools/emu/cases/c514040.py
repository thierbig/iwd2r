"""0x514040 CMessage97::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                    (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;        (a .data constant: 3)
    playerId = *(PLAYER_ID*)(pData + cnt);         (+3)
    objectId = *(LONG*)(pData + cnt + 4);          (+7)
    if (game->GetRemoteObjectArray()->Find(playerId, objectId, &localId) != TRUE)
        return FALSE;                              (EAX=0, this untouched)
    m_targetId  = localId;                         (this +0x4)
    m_nScript   = *(INT*)(pData + cnt + 8);        (this +0xC, wire +11)
    memcpy(&m_resScript, pData + cnt + 12, 8);     (this +0x10, wire +15)
    return TRUE;                                   (EAX=1)

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h:532) -- and unlike the Unsummon family the miss path
returns FALSE without touching `this`, so the compared return value
(1 vs 0) carries the branch and the preseeded members prove the miss
stored nothing.

SPEC_MSG_HEADER_LENGTH is file-initialized .data on BOTH sides (vanilla
0x85E670=3, ours 0xBFDA90=3, each probed from its image), so it needs no
preseed: each side reads its own copy and computes the same +3.  dwSize
is pushed but read by neither side (vanilla never touches [esp+0x14]
before ret 8; ours never touches [ebp+0xC]); it rides as a 'val' arg
anyway because the signature has it.

The chitin->game and game->remote-array steps differ per side: vanilla
inlines [chitin+0x1C54] then lea game+0x375A, while ours calls
CBaldurChitin::GetObjectGame ([ecx+0x17EC]) and embeds the array at
game+0x3708 -- the same built-vs-documented layout drift c512830 and
c512fe0 record.  One game block is carved per side so the embedded
CGameRemoteObjectArray sits at ONE fixed address (gamebase+0x3800 both
sides); the Find stub's this-pointer then normalizes equal.  g_pBaldur
Chitin is a setup write (vanilla 0x8CF6DC is in the zero tail, ours is
the map symbol ?g_pBaldurChitin).

The resref copy is inline on both sides (vanilla 0x5140A4..0x5140B6,
ours 0x6654F4..0x665508) -- the marshal side's GetResRef call does not
appear here because the destination is &m_resScript, not a pointer
chase.

Stubs (same spec both sides):
  Find       thiscall nargs=3 (playerId, objectId, LONG* pOut); the out
             param is stack slot 2, so a hit set writes the scripted
             localId there and returns TRUE(1); a miss set returns 0
             with no write and the caller returns FALSE untouched.
  UtilAssert cdecl nargs=4, noreturn: only the pData==NULL assert trip
             reaches it; a valid set never does.

No filters beyond the runner's default: the stub's write to the caller's
localId stack slot is dropped with all stack writes, and the compared
state is the this-region stores (m_targetId, m_nScript and m_resScript
all preset to 0xDEADBEEF so the hit's overwrites are visible and the
miss's silence is proven).  Nothing is inline_ok; pData is only read
(+3/+7/+11/+15..+22), never written.

Members m_targetId/m_sourceId (+0x4/+0x8, CMessage base) and
m_nScript/m_resScript (+0xC/+0x10, the CMessage97 tail) at identical
offsets both sides (identity field map, like c4f6bc0); this_size 0x18.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessage97::UnmarshalMessage',
    'short': 'CMessage97::Unmarshal',
    'exe': 0x514040, 'ours': 'CMessage97::UnmarshalMessage',
    'convention': 'thiscall', 'this_class': 'CMessage97',
    'this_size': 0x18,
    'members': ('m_targetId', 'm_sourceId', 'm_nScript', 'm_resScript'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'find_orig': 0x59B300,
              'find_ours': ('map',
                            '?Find@CGameRemoteObjectArray@@QAEHIHAAH@Z'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x18)
    pdata = blk_alloc(0x20)               # payload at +3..+23 (SPEC = 3)
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side (docstring)
    rarray = game + 0x3800                # fixed both sides, above both offsets
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x18,
                [('pData', pdata, 0x20, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_this(ctx['off'][side]['m_targetId'],
               struct.pack('<I', 0xDEADBEEF))     # overwrite must show
    m.set_this(ctx['off'][side]['m_nScript'],
               struct.pack('<I', 0xDEADBEEF))
    m.set_this(ctx['off'][side]['m_resScript'],
               b'\xEF\xBE\xAD\xDE\xEF\xBE\xAD\xDE')
    m.uc.mem_write(pdata + 3, struct.pack('<I', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(pdata + 11, struct.pack('<i', inp['nScript']))
    m.uc.mem_write(pdata + 15, inp['resref'])
    m.uc.mem_write(dwsize, struct.pack('<I', 23))  # never read by either side
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))

    m.add_stub({'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
                'addr': addrs['find_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12,
                'default': ({'writes': [(2, [inp['localId']])], 'ret': 1}
                            if inp['hit'] else {'ret': 0})})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    return m


SETS = {
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, nScript=0x2A5A5,
                     resref=b'GUIClient', hit=True, localId=0x777),
    'set2-miss': dict(playerId=0xAAAA, objectId=0x3333, nScript=0x2A5A5,
                      resref=b'GUIClient', hit=False, localId=0x777),
    'set3-hit-negative': dict(playerId=1, objectId=-700, nScript=-3,
                              resref=b'SPWIN9\xff', hit=True, localId=-2),
}
