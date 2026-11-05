"""0x5041A0 CMessageRemoveItem::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                    (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;        (a .data constant: 3)
    playerId = *(PLAYER_ID*)(pData + cnt);         (+3)
    objectId = *(LONG*)(pData + cnt + 4);          (+7)
    if (game->GetRemoteObjectArray()->Find(playerId, objectId, &localId) != TRUE)
        return FALSE;                              (EAX=0, this untouched)
    m_targetId = localId;                          (this +0x4)
    m_slot = *(SHORT*)(pData + cnt + 8);           (this +0xC, wire +11, 16-bit)
    UTIL_ASSERT(cnt + 10 == dwSize);               (i.e. dwSize == 13)
    return TRUE;                                   (EAX=1)

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h:2159).  Unlike every other unmarshal case so far this
one DOES read dwSize, but only on the hit path (the wire-size assert);
the miss returns FALSE before the compare on both sides (vanilla jnes
past the cmp at 0x5041FB, ours branches at 0x667A9F), so the miss set
carries a deliberately wrong dwSize (99) to prove neither side reads it
there.  The m_slot store is a 16-bit store on both sides (vanilla
0x504204/0x504211, ours 0x667AB8/0x667ABD).

SPEC_MSG_HEADER_LENGTH is file-initialized .data on BOTH sides (vanilla
0x85E670=3, ours 0xBFDA90=3, each probed from its image), so it needs no
preseed: each side reads its own copy and computes the same +3/+11/+13.
The assert line numbers differ (vanilla 0x3C07/0x3C21, ours
0x5591/0x55A7) but only a trip reaches them.

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
             with no write and the caller returns FALSE untouched.
  UtilAssert cdecl nargs=4, noreturn: only an assert trip reaches it;
             a valid set never does, and if one side tripped while the
             other returned, the terminated-vs-returned comparison
             flags the DIFF.

No filters beyond the runner's default: the stub's write to the caller's
localId stack slot is dropped with all stack writes, and the compared
state is the this-region stores (m_targetId preset to 0xDEADBEEF and
m_slot to 0xBEEF so the hit's overwrites are visible and the miss's
silence is proven).  Nothing is inline_ok; pData is only read
(+3/+7/+11), never written.

Members m_targetId/m_sourceId (+0x4/+0x8, CMessage base) and m_slot
(+0xC, the CMessageRemoveItem tail, src/CMessage.h:2162) at identical
offsets both sides (identity field map, like c4f6bc0); this_size 0x10.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageRemoveItem::UnmarshalMessage',
    'short': 'CMessageRemoveItem::Unmarshal',
    'exe': 0x5041A0, 'ours': 'CMessageRemoveItem::UnmarshalMessage',
    'convention': 'thiscall', 'this_class': 'CMessageRemoveItem',
    'this_size': 0x10,
    'members': ('m_targetId', 'm_sourceId', 'm_slot'),
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
    pdata = blk_alloc(0x20)               # payload at +3..+13 (SPEC = 3)
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side (docstring)
    rarray = game + 0x3800                # fixed both sides, above both offsets
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x10,
                [('pData', pdata, 0x20, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_this(ctx['off'][side]['m_targetId'],
               struct.pack('<I', 0xDEADBEEF))     # overwrite must show
    m.set_this(ctx['off'][side]['m_slot'], b'\xEF\xBE')
    m.uc.mem_write(pdata + 3, struct.pack('<I', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(pdata + 11, struct.pack('<h', inp['slot']))
    m.uc.mem_write(dwsize, struct.pack('<I', inp['dwSize']))
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
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, slot=0x777,
                     hit=True, localId=0x777, dwSize=13),
    'set2-miss': dict(playerId=0xAAAA, objectId=0x3333, slot=0x777,
                      hit=False, localId=0x777, dwSize=99),  # unread on miss
    'set3-hit-negative': dict(playerId=1, objectId=-700, slot=-2,
                              hit=True, localId=-2, dwSize=13),
}
