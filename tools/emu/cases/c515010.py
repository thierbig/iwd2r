"""0x515010 CMessageProjectileImmumityUpdate::MarshalMessage(BYTE** pData, DWORD* dwSize).

    UTIL_ASSERT(pData != NULL && dwSize != NULL);   (never fires here)
    do { rc = GetShare(m_targetId, THREAD_ASYNCH, &pObject, INFINITE); }
    while (rc == SHARED || rc == DENIED);
    if (rc != SUCCESS) { *dwSize = 0; return; }
    playerId  = pObject->m_remotePlayerID;          (+0x62 vanilla / +0x6C ours)
    objectId  = pObject->m_remoteObjectID;          (+0x66 vanilla / +0x70 ours)
    ReleaseShare(m_targetId, THREAD_ASYNCH, INFINITE);
    immSize   = m_projectileImmunities.Marshal(&buf);   (callee stubbed)
    *dwSize   = immSize + 0xC;
    UTIL_ASSERT(*dwSize <= STATICBUFFERSIZE);       (never fires here)
    (*pData)[0] = playerId;  [4] = objectId;  [8] = immSize;
    memcpy(*pData + 0xC, buf, immSize);             (vanilla: rep movsd + movsb
                                                     tail, inlined; ours: a call
                                                     through the vcruntime140
                                                     memcpy IAT thunk)
    operator delete(buf);

Same GetShare family as c516d50 with a wider tail.  CImmunitiesProjectile::
Marshal (0x4E6DA0 vanilla) is stubbed: it writes a heap buffer pointer to
its out-param (stack slot 0) and returns the marshalled size -- so the
case controls the payload bytes and both copy paths (dword loop + byte
tail for sizes not multiple of 4).  The buffer is a per-side blk_alloc'd
block (same address both sides); the copy lands in the compared
'arg:ppData' region and the trailing delete is a symmetric free stub.

Vanilla passes Marshal's out-param as lea [esp+0x1C] = the CALLER'S ppData
argument slot -- compiler slot reuse again; the stub write hits wherever
the caller passed it, and both sides read the slot back.  Our compat
CPtrList is 16 bytes vs MFC's 0x1C, but m_projectileImmunities internals
are never touched (the callee is stubbed): only m_targetId is read, at +4
both sides; this_size 0x28 covers the larger vanilla object.

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
    'name': 'CMessageProjectileImmumityUpdate::MarshalMessage',
    'short': 'ProjImmunity::Marshal',
    'exe': 0x515010,
    'ours': 'CMessageProjectileImmumityUpdate::MarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageProjectileImmumityUpdate',
    'this_size': 0x28,                      # vanilla object; ours is 0x1C
    'members': ('m_targetId', 'm_sourceId', 'm_projectileImmunities'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'getshare_orig': 0x599A50,
              'getshare_ours': ('map',
                                '?GetShare@CGameObjectArray@@QAEEHEPAPAVCGameObject@@I@Z'),
              'releaseshare_orig': 0x599E70,
              'releaseshare_ours': ('map',
                                    '?ReleaseShare@CGameObjectArray@@QAEEHEI@Z'),
              'marshal_orig': 0x4E6DA0,
              'marshal_ours': ('map',
                               '?Marshal@CImmunitiesProjectile@@QAEIPAPAE@Z'),
              'delete_orig': 0x7FC984,
              'del_ours': ('map', '??3@YAXPAX@Z'),
              'delv_ours': ('map', '??_V@YAXPAX@Z'),
              # our rebuild imports memcpy (call -> jmp [slot]); vanilla
              # inlines rep movsd+movsb.  inline_ok: the imported call is
              # our codegen's shape of the copy vanilla inlined.
              'memcpyiat_ours': ('import', 'memcpy'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x28)
    ppdata = blk_alloc(0x40)              # [0] -> buffer at +8; 0xC + payload
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    pobj = blk_alloc(0x78)                # covers m_remote*ID on both layouts
    buf = blk_alloc(0x40)                 # the marshalled immunity payload
    game_side = game + (GAME_SHIFT if side == 'ours' else 0)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x28,
                [('ppData', ppdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'ref')])
    m.set_this(ctx['off'][side]['m_targetId'], struct.pack('<i', inp['targetId']))
    m.set_this(ctx['off'][side]['m_sourceId'],
               struct.pack('<I', 0xFEEDFACE))     # never read by the body
    m.uc.mem_write(ppdata, struct.pack('<I', ppdata + 8))
    m.uc.mem_write(dwsize, struct.pack('<I', 0xEEEEEEEE))  # 0xC+size / 0 must show
    m.uc.mem_write(buf, inp['payload'])
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(pobj + POBJ_PLAYER[side], struct.pack('<i', inp['playerId']))
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
    m.add_stub({'name': 'CImmunitiesProjectile::Marshal', 'kind': 'code',
                'addr': addrs['marshal_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4,
                'default': {'writes': [(0, [buf])], 'ret': inp['size']}})
    for key, nm in (('delete_orig', 'operator_delete'),
                    ('del_ours', 'operator_delete'),
                    ('delv_ours', 'operator_delete[]')):
        if key in addrs:
            m.add_stub({'name': nm, 'kind': 'code', 'addr': addrs[key],
                        'conv': 'cdecl', 'nargs': 1, 'pop': 0, 'action': 'free'})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    if side == 'ours':
        # vanilla inlines the payload copy (rep movsd + movsb); our rebuild
        # imports memcpy from vcruntime140.dll through an IAT thunk
        # (call -> jmp dword ptr [slot]).  kind 'iat' fills the slot with the
        # stub magic; action 'copy' performs the memmove vanilla's inline
        # loop performs; inline_ok keeps the call out of the comparison (it
        # is a codegen difference, not a behavior difference).
        m.add_stub({'name': 'memcpy', 'kind': 'iat', 'addr': addrs['memcpyiat_ours'],
                    'conv': 'cdecl', 'nargs': 3, 'pop': 0,
                    'action': 'copy', 'inline_ok': True})
    return m


SETS = {
    'set1-hit': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                     hit=True, retry=False, size=12,
                     payload=struct.pack('<3i', 1, 2, 3)),
    'set2-miss': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                      hit=False, retry=False, size=12,
                      payload=struct.pack('<3i', 1, 2, 3)),
    'set3-retry-denied': dict(targetId=-2, playerId=0xBEEF, objectId=-700,
                              hit=True, retry=True, size=4,
                              payload=struct.pack('<i', 0x7FFFFFFF)),
    'set4-hit-odd-size': dict(targetId=0x99, playerId=-0x80000000, objectId=-1,
                              hit=True, retry=False, size=5,
                              payload=b'\xDE\xAD\xBE\xEF\x80'),  # movsb tail
    'set5-hit-size-zero': dict(targetId=0, playerId=0, objectId=0,
                               hit=True, retry=False, size=0, payload=b''),
}
