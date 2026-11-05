"""0x511F40 CMessageProjectileTrailingVFX::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    CAIObjectType localObject(0,0,0,0,0,0,0,0,-1,0,0);   (11 immediates; the
                                                        object is never used,
                                                        vanilla never dtors it)
    UTIL_ASSERT(pData != NULL);                        (never fires here)
    cnt   = CNetwork::SPEC_MSG_HEADER_LENGTH;          (.data value 3, own
                                                        copy per side)
    playerId = *(int*)(pData + cnt + 0);               (wire +3)
    objectId = *(int*)(pData + cnt + 4);               (wire +7)
    if (game->remoteObjectArray.Find(playerId, objectId, &localId) == 1)
        m_targetId = localId;                          (this +4)
    else
        m_targetId = -1;                               (image default dword,
                                                        vanilla 0x851878 /
                                                        ours 0xbf386c -- both
                                                        verified -1; NO early
                                                        FALSE, continues)
    len     = pData[cnt + 8];                          (wire +0xB)
    copy(len bytes wire -> scratch);                   (vanilla: inline rep
    localStr1.ConstructWithLength(scratch, len);        movsd+movsb; ours:
                                                        imported memcpy IAT
    m_resRef.operator=(localStr1);                     (this +0xC, sret form)
    areaLen = pData[cnt + 9 + len];                    (wire +0xC+len)
    copy(areaLen bytes wire -> scratch);
    localStr2.ConstructWithLength(scratch, areaLen);
    m_resRef2.operator=(localStr2);                    (this +0x14)
    m_launchX = *(int*)(pData + ..);                   (this +0x18..+0x24, four
                                                        direct wire loads)
    m_launchY/m_targetX/m_targetY likewise;
    m_launchHeight = *(short*)(pData + ..);            (this +0x28)
    localStr2.~CString(); localStr1.~CString();        (vanilla destructs FOUR
    scratchHolder.~CString(); tempRef.~CString();       CStrings, ours three --
    return TRUE;                                       (mov eax,1 always)

The counterpart of c511d00.  dwSize is never read (mode val, = 0x1F + len +
areaLen per the wire convention, kept for the record).  The Assign/ctor
family is stubbed so the wire layout is pinned by the compared CALL args:
ConstructWithLength's (scratch STACK*, len immediate) pairs, the sret-form
CResRef::operator= (two stack words: hidden return buffer + source), and
CString::operator= into this+0x14 -- plus the four direct this+0x18..+0x28
stores in the state comparison.

Count asymmetries are codegen shape, all inline_ok: ours' imported memcpy
(wire->scratch copies vanilla inlines), ours' extra default CString ctor
and the CAIObjectType dtor vanilla omits (leak), and the 4-vs-3 CString
dtor sweep.  The CAIObjectType ctor itself is KEPT -- both sides call it
with the same eleven immediates on a stack local.  m_resRef (this+0xC) and
m_sourceId (this+8) keep their sentinels: both assigns are stubbed, so
only the body's own stores (m_targetId, the coords block) can land.

Find's out-param is a plain stack local on both sides ([esp+0x1C] vanilla,
[ebp-0x80] ours) -- no arg-slot reuse this time; the hit path reads the
stub-written localId back, the miss path stores the -1 default instead.

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h): TRUE on every path, both sides.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageProjectileTrailingVFX::UnmarshalMessage',
    'short': 'TrailingVFX::Unmarshal',
    'exe': 0x511F40,
    'ours': 'CMessageProjectileTrailingVFX::UnmarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageProjectileTrailingVFX',
    'this_size': 0x2C,                      # vanilla packs to 42 bytes
    'members': ('m_targetId', 'm_sourceId', 'm_resRef', 'm_resRef2',
                'm_launchX', 'm_launchY', 'm_targetX', 'm_targetY',
                'm_launchHeight'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'find_orig': 0x59B300,
              'find_ours': ('map',
                            '?Find@CGameRemoteObjectArray@@QAEHIHAAH@Z'),
              'objctor_orig': 0x40AE80,
              'objctor_ours': ('map',
                               '??0CAIObjectType@@QAE@EEEEEEEEHEH@Z'),
              'ctorptrlen_orig': 0x7FA7F5,
              'ctorptrlen_ours': ('map', '??0CString@@QAE@PBDH@Z'),
              'resrefassign_orig': 0x78ACC0,
              'resrefassign_ours': ('map',
                                    '??4CResRef@@QAE?AV0@ABVCString@@@Z'),
              'cstringassign_orig': 0x7FCD07,
              'cstringassign_ours': ('map', '??4CString@@QAEAAV0@ABV0@@Z'),
              'cstringdtor_orig': 0x7FCC1A,
              'cstringdtor_ours': ('map', '??1CString@@QAE@XZ'),
              'ctor0arg_ours': ('map', '??0CString@@QAE@XZ'),
              'objdtor_ours': ('map', '??1CAIObjectType@@QAE@XZ'),
              # our rebuild imports memcpy; vanilla inlines the wire copies.
              'memcpyiat_ours': ('import', 'memcpy'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    wire_len = 0x1F + len(inp['resref']) + len(inp['area'])
    this = blk_alloc(0x2C)
    pdata = blk_alloc(0x40)               # header at +0..2, wire at +3..
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    rarray = game + 0x3800                # fixed both sides
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x2C,
                [('pData', pdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_arg('dwSize', 0, struct.pack('<I', wire_len))   # never read
    off = ctx['off'][side]
    m.set_this(off['m_targetId'], struct.pack('<I', 0xDEADBEEF))
    m.set_this(off['m_sourceId'], struct.pack('<I', 0xFEEDFACE))  # never read
    m.uc.mem_write(this + off['m_resRef'], b'\x99' * 8)   # assign is stubbed
    m.set_this(off['m_resRef2'], struct.pack('<I', 0x99999999))
    m.set_this(off['m_launchX'], struct.pack('<i', 0x11111111))
    m.set_this(off['m_launchY'], struct.pack('<i', 0x22222222))
    m.set_this(off['m_targetX'], struct.pack('<i', 0x33333333))
    m.set_this(off['m_targetY'], struct.pack('<i', 0x44444444))
    m.set_this(off['m_launchHeight'], struct.pack('<H', 0x5555))
    rl, al = len(inp['resref']), len(inp['area'])
    m.uc.mem_write(pdata + 0, b'\xC7\xC6\xC5')            # spec header, skipped
    m.uc.mem_write(pdata + 3, struct.pack('<i', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(pdata + 0xB, struct.pack('<B', rl))
    m.uc.mem_write(pdata + 0xC, inp['resref'])
    m.uc.mem_write(pdata + 0xC + rl, struct.pack('<B', al))
    m.uc.mem_write(pdata + 0xD + rl, inp['area'])
    m.uc.mem_write(pdata + 0xD + rl + al,
                   struct.pack('<4ih', inp['launchX'], inp['launchY'],
                               inp['targetX'], inp['targetY'],
                               inp['launchHeight']))
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))

    m.add_stub({'name': 'CAIObjectType::CAIObjectType', 'kind': 'code',
                'addr': addrs['objctor_' + side], 'conv': 'thiscall',
                'nargs': 11, 'pop': 44, 'default': {'ret': 0}})
    m.add_stub({'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
                'addr': addrs['find_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12,
                'default': {'writes': [(2, [inp['localId']])],
                            'ret': inp['findRet']}})
    m.add_stub({'name': 'CString::CString(char*,int)', 'kind': 'code',
                'addr': addrs['ctorptrlen_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0}})
    m.add_stub({'name': 'CResRef::operator=', 'kind': 'code',
                'addr': addrs['resrefassign_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0}})
    m.add_stub({'name': 'CString::operator=', 'kind': 'code',
                'addr': addrs['cstringassign_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': 0}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    # count asymmetries vs vanilla -- codegen shape, not behavior:
    m.add_stub({'name': 'CString::~CString', 'kind': 'code',
                'addr': addrs['cstringdtor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0},
                'inline_ok': True})       # vanilla 4 dtors, ours 3
    if side == 'ours':
        # vanilla default-constructs one local inline (zero stores) and never
        # destructs the CAIObjectType; ours emits both calls.
        m.add_stub({'name': 'CString::CString', 'kind': 'code',
                    'addr': addrs['ctor0arg_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0, 'default': {'ret': 0},
                    'inline_ok': True})
        m.add_stub({'name': 'CAIObjectType::~CAIObjectType', 'kind': 'code',
                    'addr': addrs['objdtor_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0, 'default': {'ret': 0},
                    'inline_ok': True})
        # the two wire->scratch copies vanilla inlines (rep movsd+movsb)
        m.add_stub({'name': 'memcpy', 'kind': 'iat', 'addr': addrs['memcpyiat_ours'],
                    'conv': 'cdecl', 'nargs': 3, 'pop': 0,
                    'action': 'copy', 'inline_ok': True})
    return m


SETS = {
    # Find hit: m_targetId = localId; plain string lengths
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, localId=0x777,
                     findRet=1, resref=b'TRAILVFX', area=b'AR3000',
                     launchX=-100, launchY=200, targetX=-300, targetY=400,
                     launchHeight=17),
    # Find miss: m_targetId = -1 (image default), still TRUE; full-width ids
    'set2-miss': dict(playerId=0x7FFFFFFF, objectId=-0x80000000, localId=0x777,
                      findRet=0, resref=b'TRAILVFX', area=b'AR3000',
                      launchX=0, launchY=-1, targetX=1, targetY=0,
                      launchHeight=-32768),
    # empty strings: both length bytes zero, no copy loops, coords at +0xD
    'set3-empty-strings': dict(playerId=0, objectId=-1, localId=-1,
                               findRet=0, resref=b'', area=b'',
                               launchX=-0x80000000, launchY=0x7FFFFFFF,
                               targetX=-1, targetY=-100,
                               launchHeight=32767),
    # odd lengths exercise the movsb tails (vanilla) vs memcpy (ours);
    # negative localId stored on the hit path
    'set4-odd-lengths': dict(playerId=0x99, objectId=0x7F001, localId=-2,
                             findRet=1, resref=b'ODD', area=b'AREAX',
                             launchX=1234567, launchY=-7654321,
                             targetX=0x40000000, targetY=-0x40000000,
                             launchHeight=0x1234),
    # dword-aligned length (8) with full-width coords and min height
    'set5-aligned-fullwidth': dict(playerId=-5, objectId=0, localId=0,
                                   findRet=1, resref=b'12345678', area=b'AR99',
                                   launchX=0x7FFFFFFF, launchY=-0x80000000,
                                   targetX=-0x80000000, targetY=0x7FFFFFFF,
                                   launchHeight=-32768),
}
