"""0x4F8FA0 CMessageAddItem::UnmarshalMessage(BYTE* pData, DWORD dwSize).

    Resolve (player, remote) through CGameRemoteObjectArray::Find into
    m_targetId, then unmarshal the item payload: copy name[length] to a
    stack local (vanilla rep movs, ours memcpy import), CResRef(name) on
    the byval slot / a stack temp, CItem(resref, uses0..2, wear, flags) on
    a stack temp, SetResRef into m_item, copy the temp's five scalar
    members into m_item, ~CItem, assert cnt == dwSize (never fires --
    dwSize is seeded 24+length), return TRUE (FALSE on a Find miss).

    cursor seeds at SPEC_MSG_HEADER_LENGTH = 3 (.data vanilla 0x85E670 /
    ours 0xBFD9F0 -- the image holds the initialized value, as in
    c4f8880), so wire[0..2] are the message sub-header and the map is
    absolute (name offsets vary with length):

      +0x03 player DWORD      +0x0C+length uses0 WORD
      +0x07 remote DWORD      ..+2 uses1 WORD ..+4 uses2 WORD
      +0x0B length BYTE       ..+6 wear WORD  ..+8 flags DWORD
      +0x0C name[length]      total = 24 + length

    FINDING -- our build's CItem has NO vtable slot at 0, shifting every
    member from bLoading on by -4; the harness MEASURES this as the
    write-seq difference behind sets 2-5's EQUAL(state,layout) verdicts
    (same final state, differing write offsets -- machine.py:552-553).
    Vanilla's ctor 0x4E7840 stores vtable 0x84CED0 at +0 (src/CItem.h
    documents the base at 0x0004, cResRef at 0x000C), so its
    m_useCount1 @0x18, m_wear @0x1E, m_flags @0x20, sizeof 0xEE.  Ours
    (PDB) compiles bLoading @0, pRes @4, cResRef @8, m_useCount1 @0x14,
    m_wear @0x1A, m_flags @0x1C, sizeof 0xEA -- every member from
    bLoading on is -4, and our ctor 0x63ACD0 stores no vptr.  The five
    m_item copies therefore store at this+0x24/0x26/0x28/0x2A (word) +
    0x2C (dword) in vanilla (0x4F90CD/0x4F90D5/0x4F90DD/0x4F90E5/
    0x4F90EC, edi = this+0xC) but this+0x20/0x22/0x24/0x26 (word) +
    0x28 (dword) in ours (0x665C0B..0x665C43, esi = this).  layout.py's
    field map covers CMessageAddItem's own members only -- m_item is an
    opaque identity blob (no descent into embedded classes), so the
    field_map.get(o,o) remap (machine.py:547) leaves both sides' offsets
    literal and the -4 shift surfaces as the seq difference it is.  The
    final state still compares equal: the copy VALUES are zeros on both
    sides (see below), and final_state replays writes onto this_init --
    the all-zero snapshot captured at Machine construction (machine.py:
    201-204) -- so set_this's 0xAA sentinel never enters the state
    compare.  set1 (Find miss) returns before the ctor path: no m_item
    writes at all, the lone EQUAL(layout).

    Temp-item observability (documented gap): vanilla reads the temp's
    fields through the ctor's RETURN (esi = eax: esi+0xC for SetResRef's
    arg, esi+0x18..0x20 for the copies), ours through fixed stack slots
    the stubs never write.  Both ctor stubs return ESP0-0x800 -- a deep,
    zero-filled stack address below both frames -- so vanilla's copies
    read zeros and ours' stack slots read zeros: the copy VALUES are
    unobservable (zeros both sides) and the OFFSETS are the diff that
    matters.  Same for the byval resref: vanilla builds it at [esp] via
    the stubbed CResRef ctor (writes nothing -> zeros), ours pushes
    [eax]/[eax+4] through the ctor stub's zero return.

    CItem ctor WORD args (arg_masks [None,None,None,0xFFFF,None,0xFFFF,
    0xFFFF,None]; index 0 = this):
      arg3 uses0 -- ours passes the packed dword@P (uses1 in the upper
      half, 0x665BDD); vanilla's eax is clean (its length counter was
      zero-extended).
      arg4 uses1 -- both clean: vanilla's ecx is zeroed by its own rep
      movsb count before `mov cx`, ours shifts the packed dword down.
      arg5 uses2 -- vanilla's edx upper still holds `length` from the
      name-copy loop (length << 16, nonzero for every non-empty name;
      exercised by lengths 5/8/3), ours movzx via a stack slot.
      arg6 wear -- vanilla's esi upper holds the wire cursor pointer
      (BLK high bits, 0x4F9075 `mov si`), ours movzx.

    Game alignment, task-76b layout rule: no ours-side member offset is
    hard-coded.  Vanilla loads its chitin +0x1C54 member -> CInfGame G,
    Find this = G+0x375A (0x4F900B lea).  Ours reaches the game through
    GetObjectGame, stubbed inline_ok with ret game+0x52 so the REAL
    `lea ecx,[eax+0x3708]` lands at G+0x375A.  this-class seeds go
    through ctx['off'][side] (tools/emu/layout.py field map).

    KEPT stubs (identical multiset both sides): Find, CResRef(BYTE*)
    ctor, CItem ctor, CItem::SetResRef, ~CItem, UtilAssert (noreturn,
    never fires -- both sides assert pData and cnt == dwSize).
    inline_ok: ours-only GetObjectGame and the memcpy IAT import (action
    copy -- the variable-length name copy really goes through it);
    vanilla's name copy is a rep movs, no call.
"""

import struct

VAN_GAME_MEMBER = 0x1C54    # chitin member holding CInfGame* (read, not called)

CASE = {
    'name': 'CMessageAddItem::UnmarshalMessage',
    'short': 'AddItem::Unmarshal',
    'exe': 0x4F8FA0,
    'ours': 'CMessageAddItem::UnmarshalMessage',
    'convention': 'thiscall',
    'this_class': 'CMessageAddItem',
    'this_size': 0x100,     # covers m_item at either sizeof (0xEE/0xEA)
    'members': ('m_targetId', 'm_item'),
    'field_mapped': True,
    'extra': {'chitin_orig': 0x8CF6DC,
              'chitin_ours': ('map', '?g_pBaldurChitin@@3PAVCBaldurChitin@@A'),
              'find_orig': 0x59B300,
              'find_ours': ('map',
                            '?Find@CGameRemoteObjectArray@@QAEHIHAAH@Z'),
              'resrefctor_orig': 0x78A990,
              'resrefctor_ours': ('map', '??0CResRef@@QAE@PAE@Z'),
              'itemctor_orig': 0x4E7E90,
              'itemctor_ours': ('map',
                                '??0CItem@@QAE@VCResRef@@GGGHI@Z'),
              'setresref_orig': 0x4E8350,
              'setresref_ours': ('map',
                                 '?SetResRef@CItem@@QAEXABVCResRef@@H@Z'),
              'itemdtor_orig': 0x4E8180,
              'itemdtor_ours': ('map', '??1CItem@@QAE@XZ'),
              'assert_orig': 0x780C00,
              'assert_ours': ('map', '?UtilAssert@CUtil@@SAXHPBD00@Z'),
              # ours-only inline_ok: the accessor replaces vanilla's inline
              # chitin read; ret scripted in build()
              'gog_ours': ('map',
                           '?GetObjectGame@CBaldurChitin@@QAEPAVCInfGame@@XZ'),
              'memcpy_ours': ('import', 'memcpy')},
    'returns': 'u32',
}


def _wire(inp):
    ln = inp['length']
    b = bytearray(24 + ln)
    b[0:3] = b'\xAA\xBB\xCC'                    # message sub-header (skipped)
    d = lambda v: v & 0xFFFFFFFF
    struct.pack_into('<I', b, 0x03, d(inp['player']))
    struct.pack_into('<I', b, 0x07, d(inp['remote']))
    b[0x0B] = ln & 0xFF
    b[0x0C:0x0C + ln] = inp['name'][:ln]
    n = 0x0C + ln
    struct.pack_into('<H', b, n, inp['uses0'] & 0xFFFF)
    struct.pack_into('<H', b, n + 2, inp['uses1'] & 0xFFFF)
    struct.pack_into('<H', b, n + 4, inp['uses2'] & 0xFFFF)
    struct.pack_into('<H', b, n + 6, inp['wear'] & 0xFFFF)
    struct.pack_into('<I', b, n + 8, d(inp['flags']))
    return bytes(b)


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, ESP0, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    wsz = 24 + inp['length']
    this = blk_alloc(0x100)                 # m_item blob, either layout
    wire = blk_alloc(wsz)
    dwsize = blk_alloc(4)
    chitin = blk_alloc(VAN_GAME_MEMBER + 4)
    game = blk_alloc(0x3900)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 0x100,
                [('pData', wire, wsz, 'ref'),
                 ('dwSize', dwsize, 4, 'val')])
    off = ctx['off'][side]
    m.set_this(off['m_targetId'], struct.pack('<I', 0x11111111))  # <- localId
    m.set_this(off['m_item'], b'\xAA' * 0xF0)   # sentinel for the copies
    m.uc.mem_write(dwsize, struct.pack('<I', wsz))
    m.uc.mem_write(wire, _wire(inp))
    m.uc.mem_write(chitin + VAN_GAME_MEMBER, struct.pack('<I', game))
    m.uc.mem_write(addrs['chitin_' + side], struct.pack('<I', chitin))
    # zero scratch below both frames: ctor returns point here so vanilla's
    # ret-based temp reads and ours' resref loads see zeros (see docstring)
    zero = ESP0 - 0x800

    find = {'name': 'CGameRemoteObjectArray::Find', 'kind': 'code',
            'addr': addrs['find_' + side], 'conv': 'thiscall',
            'nargs': 3, 'pop': 12,
            'default': {'writes': [(2, [inp['localId']])], 'ret': 1}}
    if inp['find_rets']:
        find['rets'] = inp['find_rets']
    m.add_stub(find)
    m.add_stub({'name': 'CResRef::CResRef(BYTE*)', 'kind': 'code',
                'addr': addrs['resrefctor_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': zero}})
    m.add_stub({'name': 'CItem::CItem', 'kind': 'code',
                'addr': addrs['itemctor_' + side], 'conv': 'thiscall',
                'nargs': 7, 'pop': 28, 'default': {'ret': zero},
                # uses0 (ours packed dword), uses2 (vanilla length<<16),
                # wear (vanilla wire-ptr high bits) -- see docstring
                'arg_masks': [None, None, None, 0xFFFF, None, 0xFFFF,
                              0xFFFF, None]})
    m.add_stub({'name': 'CItem::SetResRef', 'kind': 'code',
                'addr': addrs['setresref_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': 0}})
    m.add_stub({'name': 'CItem::~CItem', 'kind': 'code',
                'addr': addrs['itemdtor_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'CUtil::UtilAssert', 'kind': 'code',
                'addr': addrs['assert_' + side], 'conv': 'cdecl',
                'nargs': 4, 'pop': 0, 'noreturn': True})
    if side != 'orig':
        m.add_stub({'name': 'CBaldurChitin::GetObjectGame', 'kind': 'code',
                    'addr': addrs['gog_ours'], 'conv': 'thiscall',
                    'nargs': 0, 'pop': 0,
                    'default': {'ret': game + 0x52},   # 0x375A - 0x3708
                    'inline_ok': True})
        m.add_stub({'name': 'memcpy', 'kind': 'iat',
                    'addr': addrs['memcpy_ours'], 'conv': 'cdecl',
                    'nargs': 3, 'pop': 0, 'action': 'copy',
                    'inline_ok': True})
    return m


W = dict(
    player=0x11223344, remote=-700, length=5, name=b'ITM01',
    uses0=0xFFFF, uses1=0x8000, uses2=0x7FFF, wear=0xFFFF,
    flags=0xDEADBEEF, localId=0x6C1F, find_rets=[1],
)

SETS = {
    # Find misses (`cmp eax,1`): straight FALSE before the ctor path --
    # no m_item writes, so no CItem divergence: the lone EQUAL(layout)
    'set1-find-miss': dict(W, find_rets=[0]),
    # full path, length 5: vanilla's uses2 arg carries length<<16 =
    # 0x50000 in its stale upper (masked); ours passes the packed dword
    # as uses0 (masked)
    'set2-happy-length5': dict(W),
    # length 0 edge: empty name, memcpy/rep-movs count 0
    'set3-length-zero': dict(W, length=0, name=b''),
    # length 8, player/remote full-width too; length % 4 == 0
    'set4-length8-fullwidth': dict(W, length=8, name=b'\xff' * 8,
                                   player=0xFFFFFFFF, remote=-1),
    # length 3 (stale upper 0x30000), negative localId into m_targetId
    'set5-length3-neglocal': dict(W, length=3, name=b'ABC',
                                  localId=-0x1234),
}
