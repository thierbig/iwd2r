"""0x516E90 CMessageSpellLevelImmumityUpdate::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                    (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;        (.data value 3, own copy per side)
    playerId = *(LONG*)(pData + cnt);              (+3)
    objectId = *(LONG*)(pData + cnt + 4);          (+7)
    memcpy(&m_spellLevelImmunities, pData + cnt + 8, 40);   (10 dwords, unconditional)
    if (game->GetRemoteObjectArray()->Find(playerId, objectId, &local) == TRUE) {
        m_targetId = local;                        (this +0x4)
        return TRUE;
    }
    return FALSE;                                  (m_targetId keeps its old value)

The Find family's stack-slot quirk (c50a6c0, c512950): vanilla passes the
out-param as lea ecx,[esp+0x14] at entry-16, which is the CALLER'S pData
argument slot, so Find's localId write lands on top of the saved pData
pointer and the hit path reads it back with mov edx,[esp+0xC] after the
pops -- the stub's slot-2 write hits exactly that address, both sides.

The 40-byte blob is copied verbatim (rep movsd) into
m_spellLevelImmunities at +0xC, hit and miss alike; only the +0x4 write
is conditional.  dwSize is never read (no cnt==dwSize assert in this
one).  Stubs: Find thiscall nargs=3 pop 12 (hit writes localId to stack
slot 2 and returns 1; miss returns 0), UtilAssert cdecl noreturn (only a
null pData reaches it).  The chitin->game->remote-array walk is carved
per side so the embedded remote array sits at one fixed address
(rarray = gamebase+0x3800): vanilla inlines [chitin+0x1C54] and
lea game+0x375A, ours reads [chitin+0x17EC] and game+0x3708 (c50a6c0's
family setup; g_pBaldurChitin is a setup write).

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h): TRUE on hit, FALSE on miss.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageSpellLevelImmumityUpdate::UnmarshalMessage',
    'short': 'SpellLevelImmunity::Unmarshal',
    'exe': 0x516E90,
    'ours': 'CMessageSpellLevelImmumityUpdate::UnmarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageSpellLevelImmumityUpdate',
    'this_size': 0x34,
    'members': ('m_targetId', 'm_sourceId', 'm_spellLevelImmunities'),
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
    this = blk_alloc(0x34)
    pdata = blk_alloc(0x40)               # payload at +3..+50
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    rarray = game + 0x3800                # fixed both sides
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x34,
                [('pData', pdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'val')])
    blob = struct.pack('<10i', *inp['blob'])
    m.set_this(ctx['off'][side]['m_targetId'],
               struct.pack('<I', 0xDEADBEEF))     # hit must overwrite, miss keep
    m.set_this(ctx['off'][side]['m_sourceId'],
               struct.pack('<I', 0xFEEDFACE))     # never touched by the body
    m.set_this(ctx['off'][side]['m_spellLevelImmunities'],
               b'\x11' * 0x28)                    # overwrite must show
    m.uc.mem_write(pdata + 0, b'\xC7\xC6\xC5')    # header bytes, skipped
    m.uc.mem_write(pdata + 3, struct.pack('<I', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(pdata + 11, blob)
    m.uc.mem_write(dwsize, struct.pack('<I', 51))  # 3+4+4+40, unread
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
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, hit=True,
                     localId=0x777,
                     blob=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]),
    'set2-miss': dict(playerId=0xAAAA, objectId=0x3333, hit=False,
                      localId=0x777,
                      blob=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]),
    'set3-hit-negative': dict(playerId=1, objectId=-700, hit=True,
                              localId=-2,
                              blob=[-1, 0, -0x80000000, 0x7FFFFFFF,
                                    -12345, 6789, -255, 256, -65536, 0x10000]),
    'set4-hit-zero-blob': dict(playerId=0, objectId=0, hit=True, localId=0,
                               blob=[0] * 10),
}
