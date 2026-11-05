"""0x515180 CMessageProjectileImmumityUpdate::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                     (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;         (.data value 3, own copy per side)
    playerId = *(int*)(pData + cnt + 0);            (wire +3)
    objectId = *(int*)(pData + cnt + 4);            (wire +7)
    immSize  = *(int*)(pData + cnt + 8);            (wire +0xB)
    payload  = pData + cnt + 0xC;                   (wire +0xF)
    m_projectileImmunities.Unmarshal(payload, immSize);   (this +0xC, stubbed)
    rc = game->remoteObjectArray.Find(playerId, objectId, &localId);
    if (rc == 1) { m_targetId = localId; return TRUE; }   (this +4)
    return FALSE;                                   (nothing stored)

The counterpart of c515010.  CImmunitiesProjectile::Unmarshal (0x4E6DF0
vanilla, void(BYTE*, DWORD)) is stubbed void: the nested list build is the
callee's own contract, not this body's -- the stub keeps the CALL visible
(payload pointer + immSize are compared args), which pins the wire layout
without modeling MFC CPtrList internals.  The this+0xC list region stays
poisoned, proving the body itself touches nothing past m_targetId.

The Find family's stack-slot quirk again (c5006c0, c516e90): vanilla
passes the out-param as lea [esp+0x14], ours as lea [ebp+8] -- both are
the CALLER'S pData argument slot, so the stub's slot-2 write lands there
and the hit path reads localId back from the same slot (vanilla
[esp+0xC] after the pops, ours [ebp+8]); the miss path never reads it,
so a stale write is harmless.  The remote array address is equalized
across sides (rarray fixed, game base carved per side).

Vanilla returns Find's eax raw (checked == 1 first); ours forces eax to
1/0 -- indistinguishable, the check gates the only path where they'd
differ.  dwSize (mode val, = 0xF + immSize, header included per the wire
convention) is never read: the body has no trailing cnt assert.

Layout: m_targetId +4 / m_sourceId +8 / m_projectileImmunities +0xC both
sides (c515010); m_sourceId is never read -- sentinel survives both
sides; this_size 0x28 covers vanilla's 0x1C-byte MFC list at +0xC.

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h): TRUE on the Find hit, FALSE on the miss.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageProjectileImmumityUpdate::UnmarshalMessage',
    'short': 'ProjImmunity::Unmarshal',
    'exe': 0x515180,
    'ours': 'CMessageProjectileImmumityUpdate::UnmarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageProjectileImmumityUpdate',
    'this_size': 0x28,                      # vanilla object; ours is 0x1C
    'members': ('m_targetId', 'm_sourceId', 'm_projectileImmunities'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'find_orig': 0x59B300,
              'find_ours': ('map',
                            '?Find@CGameRemoteObjectArray@@QAEHIHAAH@Z'),
              'unmarshal_orig': 0x4E6DF0,
              'unmarshal_ours': ('map',
                                 '?Unmarshal@CImmunitiesProjectile@@QAEXPAEI@Z'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x28)
    pdata = blk_alloc(0x40)               # header at +0..2, wire at +3..
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    rarray = game + 0x3800                # fixed both sides
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x28,
                [('pData', pdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_arg('dwSize', 0, struct.pack('<I', 0xF + inp['immSize']))
    m.set_this(ctx['off'][side]['m_targetId'], struct.pack('<I', 0xDEADBEEF))
    m.set_this(ctx['off'][side]['m_sourceId'], struct.pack('<I', 0xFEEDFACE))
    m.uc.mem_write(pdata + 0, b'\xC7\xC6\xC5')            # spec header, skipped
    m.uc.mem_write(pdata + 3, struct.pack('<i', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(pdata + 0xB, struct.pack('<I', inp['immSize']))
    m.uc.mem_write(pdata + 0xF, inp['payload'])
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))
    m.add_stub({'name': 'CImmunitiesProjectile::Unmarshal', 'kind': 'code',
                'addr': addrs['unmarshal_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0}})
    m.add_stub({'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
                'addr': addrs['find_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12,
                'default': {'writes': [(2, [inp['localId']])],
                            'ret': inp['findRet']}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    return m


SETS = {
    # Find hit: m_targetId = localId, TRUE
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, immSize=8,
                     payload=struct.pack('<2i', 1, 2),
                     localId=0x777, findRet=1),
    # Find miss: m_targetId keeps its sentinel, FALSE; full-width ids
    'set2-miss': dict(playerId=-5, objectId=-0x80000000, immSize=4,
                      payload=struct.pack('<i', 0x7FFFFFFF),
                      localId=0x777, findRet=0),
    # the none-marker pair with an empty immunity payload
    'set3-none-empty': dict(playerId=0, objectId=-1, immSize=0, payload=b'',
                            localId=-1, findRet=0),
    # full-width ids on the hit path; negative local id stored
    'set4-fullwidth-hit': dict(playerId=0x7FFFFFFF, objectId=-0x80000000,
                               immSize=16,
                               payload=struct.pack('<4I', 0xDEADBEEF, 0, 1,
                                                   0xCAFEBABE),
                               localId=-2, findRet=1),
    # odd payload size (pointer arg still exact), zero ids, hit with local 0
    'set5-odd-size': dict(playerId=0, objectId=0, immSize=5,
                          payload=b'\xDE\xAD\xBE\xEF\x80',
                          localId=0, findRet=1),
}
