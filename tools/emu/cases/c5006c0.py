"""0x5006C0 CMessageFireProjectile::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                     (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;         (.data value 3, own copy per side)
    Find(pData[cnt+0], pData[cnt+4]) -> m_targetId; (hit: local id; miss: INVALID_INDEX -1)
    m_projectileType = *(WORD*)(pData + cnt + 8);   (this +0xC)
    Find(pData[cnt+10], pData[cnt+14]):             (source pair, +13/+17)
        hit  -> m_projectileTargetId = local id;    (this +0xE)
        miss -> if (sourceObjId == -1 && sourcePlayerId == 0)
                    m_projectileTargetId = -1;      (the none-marker)
                else return FALSE;                  (early, 0x5007E0)
    m_projectileTarget.x/y = pData[+21/+25];        (this +0x12/+0x16)
    m_height               = pData[+29];            (this +0x1A)
    field_20               = pData[+33];            (this +0x20)
    field_1E               = pData[+37];            (byte, this +0x1E)
    UTIL_ASSERT(cnt == dwSize);                     (line 0x309F; dwSize = 38 here)
    return TRUE;

The Find family's stack-slot quirk (c516e90, c512950): both calls pass the
out-param as lea [esp+0x18] at the five-push frame, which is the CALLER'S
pData argument slot, so the stub's slot-2 write lands on the saved pData
pointer and the hit paths read local ids back from [esp+0x18] -- edi
keeps the original pData for the payload reads that follow.  One stub
serves both calls: 'rets' scripts the per-call hit/miss sequence, the
slot-2 write applies every call (a miss return makes the caller ignore
the slot, so a stale write is harmless).

The early-FALSE path is the interesting branch: only m_targetId and
m_projectileType are stored before it, so every payload field keeps its
sentinel.  dwSize IS read this time (the trailing cnt==dwSize assert) --
38 = 3 header + 4+4 target ids + 2 type + 4+4 source ids + 4+4+4+4
dwords + 1 byte.  Layout: ours matches the header (m_projectileType
0xC .. field_20 0x20), reconciled through class_layout; m_sourceId (+8)
is never touched by this body, kept at a sentinel to prove it.

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h): TRUE on both paths that reach the end, FALSE early.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageFireProjectile::UnmarshalMessage',
    'short': 'FireProjectile::Unmarshal',
    'exe': 0x5006C0,
    'ours': 'CMessageFireProjectile::UnmarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageFireProjectile',
    'this_size': 0x24,
    'members': ('m_targetId', 'm_sourceId', 'm_projectileType',
                'm_projectileTargetId', 'm_projectileTarget', 'm_height',
                'field_1E', 'field_20'),
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

SENTINELS = {                     # offset-name -> bytes (this region)
    'm_targetId': 0xDEADBEEF,     # overwritten on hit, -1 on miss
    'm_sourceId': 0xFEEDFACE,     # never touched by the body
    'm_projectileType': 0xBEEF,   # overwritten before Find #2
    'm_projectileTargetId': 0xCAFEBABE,
    'm_height': 0x33333333,
    'field_1E': 0x44,
    'field_20': 0x55557777,
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x24)
    pdata = blk_alloc(0x40)               # payload at +3..+37 (38 bytes)
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    rarray = game + 0x3800                # fixed both sides
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x24,
                [('pData', pdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_arg('dwSize', 0, struct.pack('<I', inp['dwSize']))
    off = ctx['off'][side]
    m.set_this(off['m_sourceId'], struct.pack('<I', SENTINELS['m_sourceId']))
    m.set_this(off['m_targetId'], struct.pack('<I', SENTINELS['m_targetId']))
    m.set_this(off['m_projectileType'], struct.pack('<H', SENTINELS['m_projectileType']))
    m.set_this(off['m_projectileTargetId'],
               struct.pack('<I', SENTINELS['m_projectileTargetId']))
    m.set_this(off['m_projectileTarget'], b'\x22' * 8)
    m.set_this(off['m_height'], struct.pack('<i', SENTINELS['m_height']))
    m.set_this(off['field_1E'], struct.pack('<B', SENTINELS['field_1E']))
    m.set_this(off['field_20'], struct.pack('<i', SENTINELS['field_20']))
    m.uc.mem_write(pdata + 0, b'\xC7\xC6\xC5')            # header, skipped
    m.uc.mem_write(pdata + 3, struct.pack('<i', inp['playerId1']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId1']))
    m.uc.mem_write(pdata + 11, struct.pack('<H', inp['ptype']))
    m.uc.mem_write(pdata + 13, struct.pack('<i', inp['playerId2']))
    m.uc.mem_write(pdata + 17, struct.pack('<i', inp['objectId2']))
    m.uc.mem_write(pdata + 21, struct.pack('<i', inp['x']))
    m.uc.mem_write(pdata + 25, struct.pack('<i', inp['y']))
    m.uc.mem_write(pdata + 29, struct.pack('<i', inp['height']))
    m.uc.mem_write(pdata + 33, struct.pack('<i', inp['f20']))
    m.uc.mem_write(pdata + 37, struct.pack('<B', inp['f1e']))
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))
    m.add_stub({'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
                'addr': addrs['find_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12,
                'default': {'writes': [(2, [inp['localId']])], 'ret': 1},
                'rets': list(inp['find_rets'])})    # call1 = target, call2 = source
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    return m


SETS = {
    # both Find calls hit: ids = localId, full payload copied, TRUE
    'set1-both-hit': dict(playerId1=0xAAAA, objectId1=0x3333,
                          playerId2=0x1111, objectId2=0x2222,
                          ptype=0x0BEE, x=-1234, y=5678, height=-98765,
                          f20=0x7FFFFFFF, f1e=0xFF, localId=0x777,
                          find_rets=[1, 1], dwSize=38),
    # target misses (m_targetId = INVALID_INDEX -1), source hits; WORD edge
    'set2-target-miss': dict(playerId1=-5, objectId1=-0x80000000,
                             playerId2=0x1111, objectId2=0x2222,
                             ptype=0xFFFF, x=0, y=-1, height=1,
                             f20=-0x80000000, f1e=0, localId=0x777,
                             find_rets=[0, 1], dwSize=38),
    # source misses as the none-marker (playerId 0, objectId -1): -1 stored,
    # payload still copied, TRUE
    'set3-source-none': dict(playerId1=0xAAAA, objectId1=0x3333,
                             playerId2=0, objectId2=-1,
                             ptype=0x0001, x=-1, y=0, height=-0x80000000,
                             f20=-1, f1e=0x80, localId=0x777,
                             find_rets=[1, 0], dwSize=38),
    # source misses NOT the marker: early FALSE, payload fields keep sentinels
    'set4-source-miss-early-false': dict(playerId1=0xAAAA, objectId1=0x3333,
                                         playerId2=5, objectId2=-700,
                                         ptype=0x0BEE, x=-1234, y=5678,
                                         height=-98765, f20=0x7FFFFFFF,
                                         f1e=0xFF, localId=0x777,
                                         find_rets=[1, 0], dwSize=38),
    # both hit, zero/negative full-width payload
    'set5-negatives': dict(playerId1=-1, objectId1=0,
                           playerId2=0, objectId2=0,
                           ptype=0, x=-0x80000000, y=0x7FFFFFFF, height=0,
                           f20=0, f1e=0, localId=-2,
                           find_rets=[1, 1], dwSize=38),
}
