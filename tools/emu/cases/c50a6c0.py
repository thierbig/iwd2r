"""0x50A6C0 CMessageSetVariable::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    UTIL_ASSERT(pData != NULL);                    (never fires here)
    cnt = CNetwork::SPEC_MSG_HEADER_LENGTH;        (a .data constant: 3)
    playerId = *(PLAYER_ID*)(pData + cnt);         (+3)
    objectId = *(LONG*)(pData + cnt + 4);          (+7)
    if (game->GetRemoteObjectArray()->Find(playerId, objectId, &local) == TRUE)
        m_targetId = local;                        (this +0x4)
    else
        m_targetId = CGameObjectArray::INVALID_INDEX;   (and CONTINUE)
    len1 = *(SHORT*)(pData + cnt + 8);  cnt += 2;  (wire +11, signed)
    <first wire string>                            (wire +13, len1 bytes)
    len2 = *(SHORT*)(pData + cnt);     cnt += 2;
    <second wire string>
    m_nValue      = *(LONG*)(pData + cnt);         (this +0x14)
    m_bIncrement  = *(BYTE*)(pData + cnt);         (this +0x18)
    m_dwFlags     = *(DWORD*)(pData + cnt);        (vanilla +0x1A, ours +0x1C)
    UTIL_ASSERT(cnt + 4 == dwSize);                (dwSize == 24 + len1 + len2)
    return TRUE;                                   (EAX=1, hit AND miss)

FINDING (expected DIFF, not a measurement): the two wire strings land in
SWAPPED members.  Vanilla assigns the first string to m_sName (lea ecx,
[ebx+0x10] at 0x50A75F) and the second to m_sScope ([ebx+0xC] at
0x50A79B); our rebuild assigns the first to m_sScope (lea ecx,[ecx+0xC]
at 0x668AB8) and the second to m_sName ([ecx+0x10] at 0x668AF3).  Our
own source confirms it: UnmarshalMessage (src/CMessage.cpp:21297) assigns
m_sScope first, while our MarshalMessage (src/CMessage.cpp:21254) writes
m_sName first -- matching vanilla's order -- so our Marshal->Unmarshal
round trip swaps the two strings and our Unmarshal cannot correctly
receive a vanilla-marshaled SetVariable message.  Reported, not fixed
(worker rule); the case documents the divergence mechanically: the
operator= stubs' this-args record this+0x10/this+0xC in one order on
vanilla and the other on ours, and calls_eq carries the DIFF (call args
are never field-mapped, deliberately: both offsets exist on both sides,
so a field map would hide a real bug rather than reconcile drift).

Secondary drift, field-mapped away: m_dwFlags sits at +0x1A packed in
vanilla but +0x1C (natural alignment after the BOOLEAN at +0x18) in our
build, and the object is 0x1E bytes there vs 0x20 here -- the same
built-vs-documented family the other Task 74 cases record
(the header's /* 001A */ comment is vanilla's layout).

SPEC_MSG_HEADER_LENGTH (vanilla 0x85E670, ours 0xBFDA90) and
CGameObjectArray::INVALID_INDEX (vanilla 0x851878, ours 0xBF38E0) are
file-initialized .data on BOTH sides (probed: 3 and 0xFFFFFFFF), so each
side reads its own copy.  The chitin->game and game->remote-array walk
is the family's: vanilla inlines [chitin+0x1C54] then lea game+0x375A,
ours calls CBaldurChitin::GetObjectGame ([ecx+0x17EC]) and embeds the
array at game+0x3708; one game block carved per side pins the embedded
array at gamebase+0x3800 so the Find stub's this-pointer normalizes
equal.  g_pBaldurChitin is a setup write (vanilla 0x8CF6DC zero tail,
ours the map symbol ?g_pBaldurChitin).

The CString trio never runs: stubbed on both sides, so no allocator or
CRT is reached (the real ctor allocates, which Unicorn cannot serve).
Ours reaches the bodies through ILT thunks; the stub sits on the real
body in both images (the Find precedent):
  CString::CString(const char*, int)   thiscall, 2 stack args, pop 8;
      returns a preseeded heap buffer (same address both sides, so
      operator='s argument normalizes equal)
  CString::operator=(const CString&)   thiscall, 1 stack arg, pop 4;
      its `this` is the member being assigned -- the swap's witness
  CString::~CString()                  thiscall, 0 args, pop 0
The temps live on the stack (vanilla [esp+0x18]/[esp+0x1c], ours
[ebp+8]/[ebp-0x18] -- ours reuses the pData argument slot), so the ctor
and dtor this-args normalize to STACK* and stay equal.  Find is the
family's (hit writes localId to stack slot 2 and returns 1; miss returns
0 and the caller CONTINUES -- this family stores INVALID_INDEX and still
returns TRUE, which the miss set proves).  UtilAssert is cdecl noreturn;
valid sets never reach it.

Returns 'u32' -- `virtual BOOL UnmarshalMessage(BYTE*, DWORD)`
(src/CMessage.h:1163): TRUE on every non-assert path, both hit and miss.
"""

import struct

CHITIN_GAME = {'orig': 0x1C54, 'ours': 0x17EC}   # -> CInfGame*
GAME_RARRAY = {'orig': 0x375A, 'ours': 0x3708}   # embedded remote array

CASE = {
    'name': 'CMessageSetVariable::UnmarshalMessage',
    'short': 'CMessageSetVariable::Unmarshal',
    'exe': 0x50A6C0, 'ours': 'CMessageSetVariable::UnmarshalMessage',
    'convention': 'thiscall', 'this_class': 'CMessageSetVariable',
    'this_size': 0x20,
    'members': ('m_targetId', 'm_sourceId', 'm_sScope', 'm_sName',
                'm_nValue', 'm_bIncrement', 'm_dwFlags'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'find_orig': 0x59B300,
              'find_ours': ('map',
                            '?Find@CGameRemoteObjectArray@@QAEHIHAAH@Z'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z'),
              'ctor_orig': 0x7FA7F5,
              'ctor_ours': ('map', '??0CString@@QAE@PBDH@Z'),
              'assign_orig': 0x7FCD07,
              'assign_ours': ('map', '??4CString@@QAEAAV0@ABV0@@Z'),
              'dtor_orig': 0x7FCC1A,
              'dtor_ours': ('map', '??1CString@@QAE@XZ')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(0x20)                # covers 0x1E (vanilla) too
    pdata = blk_alloc(0x40)               # payload at +3..+24+L1+L2
    dwsize = blk_alloc(4)
    chitin = blk_alloc(0x1C58)
    game = blk_alloc(0x3900)              # one base, carved per side
    rarray = game + 0x3800                # fixed both sides
    game_side = rarray - GAME_RARRAY[side]
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x20,
                [('pData', pdata, 0x40, 'ref'), ('dwSize', dwsize, 4, 'val')])
    m.set_this(ctx['off'][side]['m_targetId'],
               struct.pack('<I', 0xDEADBEEF))     # overwrite must show
    cstr = m.preseed_heap(0x20, b'\xAA' * 15, 1)  # both ctor calls hand
    m.add_stub({'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
                'addr': addrs['find_' + side], 'conv': 'thiscall',
                'nargs': 3, 'pop': 12,
                'default': ({'writes': [(2, [inp['localId']])], 'ret': 1}
                            if inp['hit'] else {'ret': 0})})
    m.add_stub({'name': 'CString::CString', 'kind': 'code',
                'addr': addrs['ctor_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': cstr}})
    m.add_stub({'name': 'CString::operator=', 'kind': 'code',
                'addr': addrs['assign_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': cstr}})
    m.add_stub({'name': 'CString::~CString', 'kind': 'code',
                'addr': addrs['dtor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    s1, s2 = inp['s1'], inp['s2']
    m.uc.mem_write(pdata + 3, struct.pack('<I', inp['playerId']))
    m.uc.mem_write(pdata + 7, struct.pack('<i', inp['objectId']))
    m.uc.mem_write(pdata + 11, struct.pack('<h', len(s1)))
    m.uc.mem_write(pdata + 13, s1)
    p = 13 + len(s1)
    m.uc.mem_write(pdata + p, struct.pack('<h', len(s2)))
    m.uc.mem_write(pdata + p + 2, s2)
    p += 2 + len(s2)
    m.uc.mem_write(pdata + p, struct.pack('<i', inp['nValue']))
    m.uc.mem_write(pdata + p + 4, bytes([inp['bIncrement']]))
    m.uc.mem_write(pdata + p + 5, struct.pack('<I', inp['dwFlags']))
    m.uc.mem_write(dwsize, struct.pack('<I', 24 + len(s1) + len(s2)))
    m.uc.mem_write(chitin + CHITIN_GAME[side], struct.pack('<I', game_side))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))
    return m


SETS = {
    'set1-hit': dict(playerId=0xAAAA, objectId=0x3333, s1=b'NAME',
                     s2=b'SCOPES', nValue=0x12345678, bIncrement=1,
                     dwFlags=0xCAFEF00D, hit=True, localId=0x777),
    'set2-miss': dict(playerId=0xAAAA, objectId=0x3333, s1=b'NAME',
                      s2=b'SCOPES', nValue=0x12345678, bIncrement=1,
                      dwFlags=0xCAFEF00D, hit=False, localId=0x777),
    'set3-hit-negative': dict(playerId=1, objectId=-700, s1=b'X', s2=b'Y9',
                              nValue=-2, bIncrement=0,
                              dwFlags=0x80000001, hit=True, localId=-2),
}
