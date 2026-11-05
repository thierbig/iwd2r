"""0x513100 CMessageRemoveSummonLink::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                    (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;        (a .data constant: 3)
    playerId = *(PLAYER_ID*)(pData + cnt);         (+3)
    objectId = *(LONG*)(pData + cnt + 4);          (+7)
    if (game->GetRemoteObjectArray()->Find(playerId, objectId, &localId))
        m_summonedId = localId;                    (this +0xC)
    else
        m_summonedId = CGameObjectArray::INVALID_INDEX;   (-1)
    return TRUE;

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h:532); both paths load EAX=1.  The body is the same shape
as CMessageUnsummon::UnmarshalMessage (0x512950, case c512950) -- the
unmarshal target is m_summonedId (+0xC on both sides) and the wire read
is the same (playerId, objectId) pair at pData+3/+7.

SPEC_MSG_HEADER_LENGTH and INVALID_INDEX are file-initialized .data on
BOTH sides (vanilla 0x85E670=3 / 0x851878=-1, ours 0xBFDA90=3 /
0xBF38E0=-1, each probed from its image), so neither needs a preseed:
each side reads its own copy and stores the same value.  dwSize is
pushed but read by neither side (vanilla never touches [esp+0xC] before
ret 8; ours never touches [ebp+0xC]); it rides as a 'val' arg anyway
because the signature has it.

The chitin->game and game->remote-array steps differ per side: vanilla
inlines [chitin+0x1C54] then lea game+0x375A, while ours calls
CBaldurChitin::GetObjectGame ([ecx+0x17EC]) and embeds the array at
game+0x3708 -- the same built-vs-documented layout drift c512830 and
c512fe0 record.  One game block is carved per side so the embedded
CGameRemoteObjectArray sits at ONE fixed address (gamebase+0x3800 both
sides); the Find stub's this-pointer then normalizes equal.  g_pBaldur
Chitin is a setup write (vanilla 0x8CF6DC is in the zero tail, ours is
the map symbol ?g_pBaldurChitin).

Stubs (same spec both sides):
  Find       thiscall nargs=3 (playerId, objectId, LONG* pOut); the out
             param is stack slot 2, so a hit set writes the scripted
             localId there and returns TRUE(1); a miss set returns 0
             with no write and the caller stores INVALID_INDEX.
  UtilAssert cdecl nargs=4, noreturn: only the pData==NULL assert trip
             reaches it; a valid set never does.

No filters beyond the runner's default: the stub's write to the caller's
localId stack slot is dropped with all stack writes, and the compared
state is the this-region m_summonedId store (preset to 0xDEADBEEF so
both overwrites -- localId and INVALID_INDEX -- are visible).  Nothing
is inline_ok; pData is only read (+3/+7), never written.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageRemoveSummonLink::UnmarshalMessage',
    'short': 'RemoveSummonLink::Unmarshal',
    'exe': 0x513100, 'ours': 'CMessageRemoveSummonLink::UnmarshalMessage',
    'convention': 'thiscall', 'this_class': 'CMessageRemoveSummonLink',
    'this_size': 0x10,
    'members': ('m_targetId', 'm_sourceId', 'm_summonedId'),
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
    this = blk_alloc(0x10)
    pdata = blk_alloc(0x20)               # payload at +3/+7 (SPEC = 3)
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side (docstring)
    rarray = game + 0x3800                # fixed both sides, above both offsets
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x10,
                [('pData', pdata, 0x20, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_this(ctx['off'][side]['m_summonedId'],
               struct.pack('<I', 0xDEADBEEF))     # overwrite must show
    m.uc.mem_write(pdata + 3, struct.pack('<I', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(dwsize, struct.pack('<I', 8))  # never read by either side
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
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, hit=True, localId=0x777),
    'set2-miss': dict(playerId=0xAAAA, objectId=0x3333, hit=False, localId=0x777),
    'set3-hit-negative': dict(playerId=1, objectId=-700, hit=True, localId=-2),
}
