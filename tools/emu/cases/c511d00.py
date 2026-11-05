"""0x511D00 CMessageProjectileTrailingVFX::MarshalMessage(BYTE** pData, DWORD* dwSize).

    UTIL_ASSERT(pData != NULL && dwSize != NULL);     (never fires here)
    do { rc = GetShare(m_targetId, THREAD_ASYNCH, &pObject, INFINITE); }
    while (rc == SHARED || rc == DENIED);
    if (rc == SUCCESS) {                               (hit)
        playerId = pObject->m_remotePlayerID;          (+0x62 vanilla / +0x6C ours)
        objectId = pObject->m_remoteObjectID;          (+0x66 vanilla / +0x70 ours)
        ReleaseShare(m_targetId, THREAD_ASYNCH, INFINITE);
    } else { playerId = 0; objectId = -1; }            (miss: image default dword,
                                                        NO ReleaseShare, continues)
    m_resRef.CopyToString(&localCString);             (callee stubbed)
    len = localCString.GetLength();                   ([data-8] after the stub write)
    areaLen = m_resRef2 data length;                  (this +0x14, [data-8])
    *dwSize = len + areaLen + 0x1C;                   (payload-only; vanilla stores
                                                        len+9 first, then adds
                                                        areaLen+0x13 -- same sum)
    UTIL_ASSERT(*dwSize <= STATICBUFFERSIZE);         (never fires here)
    (*pData)[0] = playerId;  [4] = objectId;  [8] = (BYTE)len;
    memcpy((*pData)+9, resrefChars, len);             (vanilla: GetBuffer + inline
    (*pData)[9+len] = (BYTE)areaLen;                   rep movsd/movsb; ours: GetBuffer
    memcpy((*pData)+10+len, areaChars, areaLen);       + imported memcpy IAT thunk)
    (*pData)[10+len+areaLen .. +16] = m_launchX/Y, m_targetX/Y;   (this +0x18..+0x24)
    (*pData)[...] = m_launchHeight;                   (WORD, this +0x28)
    UTIL_ASSERT(cnt == *dwSize);                      (never fires here)
    localCString.~CString();

The GetShare family of c515010/c516d50 with a string tail.  Every MFC consumer
is stubbed: CopyToString writes a fake CString data pointer (a block with the
length dword at ptr-8, chars at ptr) into its out-param, and GetBuffer's
per-call rets feed the same blocks to both copy paths -- so the body's reads
([ptr-8] lengths) and the wire bytes are fully driven by the case.

Slot shapes that had to be checked before writing this: vanilla keeps the
local CString at [esp+0x10]; ours constructs it ON the ppData argument stack
slot ([ebp+8], ppData cached in ebx first) and reuses that slot for
CopyToString's out-param, GetBuffer #1's this and the final dtor.  All of
those addresses are machine stack, region 'stack', filtered from the write
comparison -- and the calls' this/arg values normalize to 'STACK*' on both
sides, so the slot reuse cannot leak into the verdict.  Ours' extra
CString::CString() call (vanilla inlines the default construction) is
inline_ok; the imported memcpy is the iat+copy+inline_ok stub of c515010.

The miss path is the interesting one: no ReleaseShare, playerId forced to 0
and objectId to the image default dword (both images hold -1 at vanilla
0x851878 / ours 0xbf386c -- verified equal before casing) -- then the body
CONTINUES into the same wire build with the defaulted pair.

m_resRef (this+0xC, 8 bytes) and m_sourceId (this+8) are seeded with
sentinels and never touched: CopyToString is stubbed, so only the m_resRef
ADDRESS (this-relative, equal on both sides) is observed.

Returns 'void' -- `virtual void MarshalMessage(BYTE**, DWORD*)`
(src/CMessage.h): EAX is not part of the contract.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_ARRAY = {'orig': 0x372C, 'ours': 0x36C4}    # embedded array offset
POBJ_PLAYER = {'orig': 0x62, 'ours': 0x6C}
POBJ_OBJECT = {'orig': 0x66, 'ours': 0x70}
GAME_SHIFT = GAME_ARRAY['orig'] - GAME_ARRAY['ours']

CASE = {
    'name': 'CMessageProjectileTrailingVFX::MarshalMessage',
    'short': 'TrailingVFX::Marshal',
    'exe': 0x511D00,
    'ours': 'CMessageProjectileTrailingVFX::MarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageProjectileTrailingVFX',
    'this_size': 0x2C,                      # vanilla packs to 42 bytes
    'members': ('m_targetId', 'm_sourceId', 'm_resRef', 'm_resRef2',
                'm_launchX', 'm_launchY', 'm_targetX', 'm_targetY',
                'm_launchHeight'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'getshare_orig': 0x599A50,
              'getshare_ours': ('map',
                                '?GetShare@CGameObjectArray@@QAEEHEPAPAVCGameObject@@I@Z'),
              'releaseshare_orig': 0x599E70,
              'releaseshare_ours': ('map',
                                    '?ReleaseShare@CGameObjectArray@@QAEEHEI@Z'),
              'copytostring_orig': 0x78A9F0,
              'copytostring_ours': ('map',
                                    '?CopyToString@CResRef@@QBEXAAVCString@@@Z'),
              'getbuffer_orig': 0x7FCFFE,
              'getbuffer_ours': ('map', '?GetBuffer@CString@@QAEPADH@Z'),
              'ctor_ours': ('map', '??0CString@@QAE@XZ'),
              'cstringdtor_orig': 0x7FCC1A,
              'cstringdtor_ours': ('map', '??1CString@@QAE@XZ'),
              # our rebuild imports memcpy; vanilla inlines rep movsd+movsb.
              'memcpyiat_ours': ('import', 'memcpy'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    buf_need = 8 + 0x1C + len(inp['resref']) + len(inp['area'])
    this = blk_alloc(0x2C)
    ppdata = blk_alloc(max(0x40, buf_need))  # [0] -> wire buffer at +8
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)                 # one base, carved per side
    pobj = blk_alloc(0x78)                   # m_remote*ID on both layouts
    rblk = blk_alloc(8 + len(inp['resref']))  # fake CString data: len at -8
    ablk = blk_alloc(8 + len(inp['area']))
    rdata, adata = rblk + 8, ablk + 8
    game_side = game + (GAME_SHIFT if side == 'ours' else 0)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x2C,
                [('ppData', ppdata, max(0x40, buf_need), 'ref'),
                 ('dwSize', dwsize, 4, 'ref')])
    off = ctx['off'][side]
    m.set_this(off['m_targetId'], struct.pack('<i', inp['targetId']))
    m.set_this(off['m_sourceId'], struct.pack('<I', 0xFEEDFACE))  # never read
    m.uc.mem_write(this + off['m_resRef'], b'\x99' * 8)  # stub's this, unread
    m.set_this(off['m_resRef2'], struct.pack('<I', adata))
    m.set_this(off['m_launchX'], struct.pack('<i', inp['launchX']))
    m.set_this(off['m_launchY'], struct.pack('<i', inp['launchY']))
    m.set_this(off['m_targetX'], struct.pack('<i', inp['targetX']))
    m.set_this(off['m_targetY'], struct.pack('<i', inp['targetY']))
    m.set_this(off['m_launchHeight'], struct.pack('<h', inp['launchHeight']))
    m.uc.mem_write(ppdata, struct.pack('<I', ppdata + 8))
    m.uc.mem_write(dwsize, struct.pack('<I', 0xEEEEEEEE))
    m.uc.mem_write(rblk, struct.pack('<I', len(inp['resref'])))
    m.uc.mem_write(rdata, inp['resref'])
    m.uc.mem_write(ablk, struct.pack('<I', len(inp['area'])))
    m.uc.mem_write(adata, inp['area'])
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
        gs['rets'] = [5]                  # DENIED once, then SUCCESS
    m.add_stub(gs)
    m.add_stub({'name': 'CGameObjectArray::ReleaseShare', 'kind': 'code',
                'addr': addrs['releaseshare_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12, 'default': {'ret': 0},
                'arg_masks': [None, None, 0xFF, None]})   # BYTE flag (arg 1)
    m.add_stub({'name': 'CResRef::CopyToString', 'kind': 'code',
                'addr': addrs['copytostring_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4,
                'default': {'writes': [(0, [rdata])], 'ret': 0}})
    m.add_stub({'name': 'CString::GetBuffer', 'kind': 'code',
                'addr': addrs['getbuffer_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'rets': [rdata, adata]})
    m.add_stub({'name': 'CString::~CString', 'kind': 'code',
                'addr': addrs['cstringdtor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    if side == 'ours':
        # vanilla inlines the local CString's default construction; our
        # rebuild calls the imported ctor on the reused ppData stack slot.
        m.add_stub({'name': 'CString::CString', 'kind': 'code',
                    'addr': addrs['ctor_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0, 'default': {'ret': 0},
                    'inline_ok': True})
        # vanilla inlines both copies (rep movsd + movsb); ours imports memcpy
        # through the vcruntime140 IAT (call -> jmp dword ptr [slot]).
        m.add_stub({'name': 'memcpy', 'kind': 'iat', 'addr': addrs['memcpyiat_ours'],
                    'conv': 'cdecl', 'nargs': 3, 'pop': 0,
                    'action': 'copy', 'inline_ok': True})
    return m


SETS = {
    # hit: both ids from the object, plain string lengths
    'set1-hit': dict(targetId=0x1234, playerId=0xAAAA, objectId=0x3333,
                     hit=True, retry=False,
                     resref=b'TRAILVFX', area=b'AR3000',
                     launchX=-100, launchY=200, targetX=-300, targetY=400,
                     launchHeight=17),
    # miss: no ReleaseShare, playerId 0 / objectId -1 (image default) on the wire
    'set2-miss': dict(targetId=0x1234, playerId=0xBEEF, objectId=-700,
                      hit=False, retry=False,
                      resref=b'TRAILVFX', area=b'AR3000',
                      launchX=0, launchY=-1, targetX=1, targetY=0,
                      launchHeight=-32768),
    # DENIED once then SUCCESS (loop back to GetShare)
    'set3-retry-denied': dict(targetId=-2, playerId=0x7FFFFFFF,
                              objectId=-0x80000000, hit=True, retry=True,
                              resref=b'AB', area=b'CD',
                              launchX=0x7FFFFFFF, launchY=-0x80000000,
                              targetX=-0x80000000, targetY=0x7FFFFFFF,
                              launchHeight=32767),
    # empty strings: *dwSize = 0x1C, both length bytes zero, no copy loops
    'set4-empty-strings': dict(targetId=0, playerId=0, objectId=-1,
                               hit=True, retry=False,
                               resref=b'', area=b'',
                               launchX=-1, launchY=-1, targetX=-1, targetY=-1,
                               launchHeight=-1),
    # odd lengths exercise the movsb tail (vanilla) vs memcpy (ours)
    'set5-odd-lengths': dict(targetId=0x99, playerId=-5, objectId=0x7F001,
                             hit=True, retry=False,
                             resref=b'ODD', area=b'AREAX',
                             launchX=1234567, launchY=-7654321,
                             targetX=0x40000000, targetY=-0x40000000,
                             launchHeight=0x1234),
}
