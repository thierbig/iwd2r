"""0x45E870 TString::AssignString(const TString& src, UINT idx, UINT count).

    thiscall, 3 dword stack args, ret 0xC, returns this ('u32' -- both
    sides move this into eax on every path: vanilla `mov eax,ebp;
    ret 0xC` at 0x45EA66, ours `mov eax,esi` at 0x82C58F/0x82C5EA).
    TString is layout-identical both sides (16 bytes: flag byte 0,
    m_pData 4, m_nLen 8, m_nRes 0xC -- precedent c44bc00/c44ba90/
    c44bc20), so offsets stay literal; no field map.  src is a 'ref'
    block (the pushed value is &srcTString), idx/count are 'val'
    dwords.

    Decision table (vanilla 0x45E870 / ours 0x82C450), in order:

    ENTRY: idx > src.m_nLen -> Xran noreturn (vanilla 0x45E8AE ->
    0x7F9C0E, ours 0x82C465 -> ILT 0x401BF4).  count' = cmov-min(count,
    len - idx); vanilla additionally clamps its memmove/PA arithmetics
    to the .rdata constant at 0x8485C0 (min at 0x45E8C4) -- whose IMAGE
    VALUE is 0xFFFFFFFF (objdump -s -j .rdata; the 0x0001FFFF lives at
    the NEXT dword 0x8485C4 and never loads here -- see FINDING).

    NON-SELF share gate: count' != 0, count' == src.len, rc byte at
    src.pData-1 < 0xFE.  SHARE: Tidy(this,1) then adopt src's
    pData/len/res and rc++ at src.pData-1 (vanilla calls real Tidy
    0x448D50 at 0x45E99C; ours real 0x82CA60 via ILT 0x41021C --
    Tidy is unstubbed so its only visible effect is the operator
    delete inside).  MISS -> deep.

    NON-SELF deep: MaxSize gate (real, pure `mov eax,0xFFFFFFFD; ret`
    -- vanilla 0x45EA70, ours 0x82C940) -> count' > 0xFFFFFFFD fires
    Xlen noreturn (set13: full-width src.len 0xFFFFFFFE); then
    PA(count',1) whose decision table is c44bc20's (shared-n0
    dec-rc+zero; shared-n>0 unconditional Grow; plain carve-out keep
    / release+Grow); on TRUE, memcpy src.pData+idx -> this.pData
    (vanilla inline rep movs 0x45EA50/0x45EA5A; ours IAT thunk
    0xB6E3A5 -> slot 0xFC69FC) and SetLength(count') (vanilla inline
    len+NUL 0x45E975; ours real 0x82CA40 via ILT 0x4079D2).

    SELF part1 (delete [idx, end)): end = idx + count'; vanilla
    shifts the tail with memmove(pData+end, pData+end+min(clamp,
    tail), tail - min) through the REAL CRT at 0x7E84F0 (call site
    0x45E8D8), then PA(len - min, 0) (vanilla inline table, real
    call 0x44BC20 at 0x45E8EA; ours 0x82C950 via ILT 0x41D0A2 with
    end as the argument) and, on TRUE, inline len+NUL (vanilla
    0x45E8F3) / real SetLength (ours 0x82C4A1).  tail == 0 skips
    part1 entirely.  Ours has NO part1 memmove at all -- provably
    equivalent, see FINDING.

    SELF part2 (delete [0, idx)): Detach (vanilla real 0x44BD40 at
    0x45E905, ours real 0x82C7A0 via ILT 0x41DC19); min(idx, len) ==
    0 -> ret; memmove(pData, pData+min, len - min) (vanilla REAL CRT
    at 0x45E926; ours IAT thunk 0xB6E3C9 -> slot 0xFC6A1C); PA(len -
    min, 0); TRUE -> SetLength(len - min).

    FINDING (corrected, equivalence): the clamp dword at 0x8485C0 is
    0xFFFFFFFF in the image, so min(clamp, tail) == tail for EVERY
    tail and the part1 memmove count is identically n = tail - tail
    = 0 -- the call at 0x45E8D8 is a provable no-op with no ours
    counterpart, invisible because vanilla's CRT stays real/unstubbed
    (unrecorded) and a zero-count rep movs writes nothing.  PA at
    0x45E8EA therefore always gets len - tail == end, byte-identical
    to ours' PA(end,0) at 0x82C49A: part1 has NO behavioral
    difference.  An earlier reading put 0x0001FFFF at 0x8485C0 and
    predicted a keep-the-far-tail DIFF for tail > 0x1FFFF (set20);
    that value actually lives at 0x8485C4, never loaded here, and
    set20 measured EQUAL -- the image dump settled it.  c44ba90's
    docstring carries the same 0x0001FFFF-at-0x8485C0 slip (its
    conclusion "equivalent for every len this case can seed" still
    holds, stronger: the clamp never bites at all).

    Stubs (identical names both sides): TString::Xran and TString::Xlen
    (cdecl 0, noreturn -- Xran fires in set12, Xlen in set13, both
    sides), operator delete (cdecl 1, pop 0, ret 0 -- vanilla 0x7FC984
    CRT-free wrapper, ours the vector deallocator ??_V at 0xB41DA3,
    caller-cleaned both), TString::Grow (thiscall 1, pop 4, ret 0 --
    the call sites rely on Grow's `ret 4`).  Grow is stubbed because
    the real body is an SEH-wrapped allocator whose VC6-vs-MSVC2022
    codegen would drown AssignString's own signal (c44bc20); the stub
    allocates nothing, so after a Tidy(1) both sides copy into the
    mapped zero page at raw 0..len (c44ba90's deep-set device -- the
    copy bytes/offsets compare, not buffer identity).

    Deliberately NOT stubbed (real, invisible calls -- their WRITES
    are the comparison; stubbing would manufacture calls-DIFFs since
    vanilla inlines the same logic): Detach, Tidy, PrepareAssign,
    SetLength, MaxSize (addresses in extra for documentation only).
    ours-only inline_ok: the memcpy and memmove IAT imports (action
    copy -- machine.py:552 chunks them into dword stores comparable
    with vanilla's rep movsd; counts not divisible by 4 downgrade to
    EQUAL(state,layout), set19).  Vanilla's memmove stays REAL for
    the same reason: its natural rep-movs writes ARE the comparison,
    and the provably-n=0 part1 call is unrecorded and harmless.
"""

import struct

CLAMP = 0xFFFFFFFF                     # vanilla .rdata 0x8485C0 image value
                                       # (0x0001FFFF is at 0x8485C4, unused here)

CASE = {
    'name': 'TString::AssignString',
    'short': 'TString::AssignString',
    'exe': 0x45E870, 'ours': 'TString::AssignString',
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {
        'xran_orig': 0x7F9C0E,
        'xran_ours': ('map', '?Xran@TString@@KAXXZ'),
        'xlen_orig': 0x7F9AC2,
        'xlen_ours': ('map', '?Xlen@TString@@KAXXZ'),
        'delete_orig': 0x7FC984,
        'delete_ours': ('map', '??_V@YAXPAX@Z'),
        'grow_orig': 0x44BE10,
        'grow_ours': ('map', '?Grow@TString@@IAEXI@Z'),
        # documentation only -- deliberately NOT stubbed (see docstring)
        'detach_orig': 0x44BD40,
        'detach_ours': ('map', '?Detach@TString@@IAEXXZ'),
        'tidy_orig': 0x448D50,
        'tidy_ours': ('map', '?Tidy@TString@@IAEXE@Z'),
        'pa_orig': 0x44BC20,
        'pa_ours': ('map', '?PrepareAssign@TString@@IAEEIE@Z'),
        'setlength_orig': 0x44BC00,
        'setlength_ours': ('map', '?SetLength@TString@@IAEXI@Z'),
        'maxsize_orig': 0x45EA70,
        'maxsize_ours': ('map', '?MaxSize@TString@@IBEIXZ'),
        'memmove_orig_real': 0x7E84F0,  # vanilla CRT, left real on purpose
        'memcpy_ours': ('import', 'memcpy'),
        'memmove_ours': ('import', 'memmove'),
    },
    'returns': 'u32',
}


def _tstr(addr, flags, pdata, slen, res, m):
    # flag byte lives in the 4-byte m_buf union head, dwords start +4
    # (a bare '<B' + '<I' concat would land m_pData at +1, not +4)
    m.uc.mem_write(addr, struct.pack('<B', flags) + b'\0' * 3 +
                   struct.pack('<III', pdata, slen, res))


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(16)
    src = this if inp['self'] else blk_alloc(16)
    iarg = blk_alloc(4)
    carg = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16,
                [('src', src, 16, 'ref'),
                 ('idx', iarg, 4, 'val'),
                 ('count', carg, 4, 'val')])

    if inp['tbuf']:
        tdata = m.preseed_heap(inp['tsize'], inp['tcontent'], inp['trc'])
    else:
        tdata = 0
    if inp['sbuf']:
        sdata = m.preseed_heap(inp['ssize'], inp['scontent'], inp['src_rc'])
    else:
        sdata = 0
    _tstr(this, inp['tflags'], tdata, inp['tlen'], inp['tres'], m)
    if not inp['self']:
        _tstr(src, inp['sflags'], sdata, inp['slen'], inp['sres'], m)
    m.uc.mem_write(iarg, struct.pack('<I', inp['idx'] & 0xFFFFFFFF))
    m.uc.mem_write(carg, struct.pack('<I', inp['count'] & 0xFFFFFFFF))
    if inp['null_page']:
        # Grow is stubbed to its return only (no allocation), so after a
        # Tidy(1) both sides copy into pData == 0 -- give raw 0.. a zero
        # page instead of faulting (c44ba90's deep-set device)
        m.uc.mem_map(0, 0x1000)

    m.add_stub({'name': 'TString::Xran', 'kind': 'code',
                'addr': addrs['xran_' + side], 'conv': 'cdecl',
                'nargs': 0, 'pop': 0, 'noreturn': True})
    m.add_stub({'name': 'TString::Xlen', 'kind': 'code',
                'addr': addrs['xlen_' + side], 'conv': 'cdecl',
                'nargs': 0, 'pop': 0, 'noreturn': True})
    m.add_stub({'name': 'operator delete', 'kind': 'code',
                'addr': addrs['delete_' + side], 'conv': 'cdecl',
                'nargs': 1, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'TString::Grow', 'kind': 'code',
                'addr': addrs['grow_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': 0}})
    if side != 'orig':
        m.add_stub({'name': 'memcpy', 'kind': 'iat',
                    'addr': addrs['memcpy_ours'], 'conv': 'cdecl',
                    'nargs': 3, 'pop': 0, 'action': 'copy',
                    'inline_ok': True})
        m.add_stub({'name': 'memmove', 'kind': 'iat',
                    'addr': addrs['memmove_ours'], 'conv': 'cdecl',
                    'nargs': 3, 'pop': 0, 'action': 'copy',
                    'inline_ok': True})
    return m


W = dict(tflags=0, tbuf=False, tcontent=b'', tsize=0, trc=0, tlen=0,
         tres=0x1F, sflags=0, sbuf=False, scontent=b'', ssize=0,
         src_rc=0, slen=0, sres=0x1F, self=False, idx=0, count=0,
         null_page=False)

SETS = {
    # ---- non-self, share gate taken ----
    # this holds a plain buffer: Tidy(1) frees it (delete VISIBLE),
    # then adopt src pData/len/res and rc 1 -> 2
    'set1-share-plain-this': dict(W, tbuf=True, tsize=7, tcontent=b'OLD!!',
                                  tlen=5, tres=5,
                                  sbuf=True, ssize=7, scontent=b'HELO!',
                                  src_rc=1, slen=5, sres=5, count=5),
    # this empty: Tidy(1) has nothing to free, no delete
    'set2-share-empty-this': dict(W, sbuf=True, ssize=7, scontent=b'HELO!',
                                  src_rc=1, slen=5, sres=5, count=5),
    # last shareable count: rc 0xFD -> 0xFE
    'set3-share-rc-edge': dict(W, sbuf=True, ssize=3, scontent=b'X',
                               src_rc=0xFD, slen=1, sres=1, count=1),
    # ---- non-self, gate miss -> deep ----
    # count' 4 != src.len 5 -> deep; plain small-buffer carve-out KEEP
    # (res < 0x20, res >= count'): no delete, no Grow, 4-byte copy
    # (dword-divisible) straight into the OLD buffer + SetLength(4)
    'set4-deep-keep-carveout': dict(W, tbuf=True, tsize=6,
                                    tcontent=b'OLD!', tlen=4,
                                    sbuf=True, ssize=7, scontent=b'HELO!',
                                    src_rc=1, slen=5, sres=5,
                                    idx=1, count=5),
    # frozen src (rc 0xFF) with count' == len -> deep; this empty and
    # res 0 < count' 8 -> Tidy(1) + Grow (VISIBLE), copy lands at raw
    # 0..8 (dword-divisible), SetLength(8)
    'set5-deep-empty-this-grow': dict(W, tres=0, sbuf=True, ssize=10,
                                      scontent=b'\xff' * 8, src_rc=0xFF,
                                      slen=8, sres=8, count=8,
                                      null_page=True),
    # res >= 0x20 always releases: delete + Grow + copy at raw 0..4
    'set6-deep-bigres-release': dict(W, tbuf=True, tsize=7,
                                     tcontent=b'OLD!!', tlen=5, tres=0x20,
                                     sbuf=True, ssize=10,
                                     scontent=b'ABCDEFGH', src_rc=1,
                                     slen=8, sres=8, idx=1, count=4,
                                     null_page=True),
    # small buffer that does NOT fit (res 0x1F < count' 0x20):
    # delete + Grow + 0x20-byte copy at raw 0
    'set7-deep-smallgrow': dict(W, tbuf=True, tsize=7,
                                tcontent=b'OLD!!', tlen=5,
                                sbuf=True, ssize=0x22,
                                scontent=b'\xAA' * 0x20, src_rc=0xFF,
                                slen=0x20, sres=0x20, count=0x20,
                                null_page=True),
    # shared this (rc 2), count' > 0: UNCONDITIONAL Grow -- no
    # release, no zeroing; the copy overwrites the old (still-live)
    # this buffer in place, rc untouched
    'set8-deep-shared-this-grow': dict(W, tbuf=True, tsize=7,
                                       tcontent=b'OLD!!', tlen=5, tres=5,
                                       trc=2,
                                       sbuf=True, ssize=6,
                                       scontent=b'WXYZ', src_rc=0xFF,
                                       slen=4, sres=4, count=4),
    # count' == 0 -> deep; PA shared-n0: dec rc 3 -> 2 + zero trio,
    # FALSE, bail (bDetach ignored)
    'set9-deep-c0-shared': dict(W, tbuf=True, tsize=7,
                                tcontent=b'HELO!', tlen=5, trc=3,
                                sbuf=True, ssize=7, scontent=b'HELO!',
                                src_rc=1, slen=5),
    # count' == 0, plain this: Tidy(1) -> delete VISIBLE + zeros,
    # FALSE, bail
    'set10-deep-c0-plain': dict(W, tbuf=True, tsize=7,
                                tcontent=b'HELO!', tlen=5,
                                sbuf=True, ssize=7, scontent=b'HELO!',
                                src_rc=1, slen=5),
    # count' == 0, this empty: zero trio only, no calls
    'set11-deep-c0-empty': dict(W, sbuf=True, ssize=7,
                                scontent=b'HELO!', src_rc=1, slen=5),
    # ---- asserts ----
    # idx 6 > src.len 5: Xran FIRES on both sides
    'set12-xran': dict(W, sbuf=True, ssize=7, scontent=b'HELO!',
                       src_rc=1, slen=5, idx=6, count=1),
    # full-width: count' = min(0xFFFFFFFF, 0xFFFFFFFE) == src.len so
    # the share gate passes the count tests but frozen rc 0xFF drops
    # it to deep; real MaxSize 0xFFFFFFFD < count' -> Xlen FIRES
    'set13-xlen-fullwidth': dict(W, sbuf=True, ssize=6,
                                 scontent=b'\xEE', src_rc=0xFF,
                                 slen=0xFFFFFFFE, count=0xFFFFFFFF),
    # ---- self ----
    # head replace, tail 5: part1 only -- PA(3,0) keep, SetLength(3)
    'set14-self-replace-head': dict(W, self=True, tbuf=True, tsize=10,
                                    tcontent=b'ABCDEFGH', tlen=8,
                                    tres=8, count=3),
    # middle erase: part1 SetLength(6)+NUL@6, part2 dword-sized
    # memmove (min 2, n 4) + SetLength(4) -> "CDEF"
    'set15-self-mid-erase': dict(W, self=True, tbuf=True, tsize=12,
                                 tcontent=b'ABCDEFGHIJ', tlen=10,
                                 tres=10, idx=2, count=4),
    # erase to end: tail 0 skips part1; part2 memmove(4) -> "DEFG"
    'set16-self-erase-to-end': dict(W, self=True, tbuf=True, tsize=9,
                                    tcontent=b'ABCDEFG', tlen=7,
                                    tres=7, idx=3, count=4),
    # idx 0, count 0: PA(0,0) keep-n0 writes len=0 + NUL itself and
    # returns FALSE -- SetLength skipped; part2 min 0 -> ret
    'set17-self-zero-count': dict(W, self=True, tbuf=True, tsize=7,
                                  tcontent=b'HELLO', tlen=5, tres=5),
    # self, len 0, no buffer: nothing on either side
    'set18-self-empty': dict(W, self=True, count=3),
    # self SHARED (rc 3): Detach privatizes via COW (rc 3 -> 2, zero
    # trio, PA(8,1) -> Tidy(1)+Grow, copy old -> raw 0..8,
    # SetLength(8)), then the normal two-part erase -- three VISIBLE
    # Grow calls, never a delete.  part2 memmove n=3 is NOT
    # dword-divisible: rep movsb x3 vs one rounded chunk ->
    # EQUAL(state,layout) by copy granularity
    'set19-self-shared-cow': dict(W, self=True, tbuf=True, tsize=10,
                                  tcontent=b'ABCDEFGH', tlen=8, tres=8,
                                  trc=3, idx=2, count=3,
                                  null_page=True),
    # clamp guard (FINDING): tail 0x20000 > the 0x0001FFFF next-door
    # dword.  Had 0x8485C0 held 0x0001FFFF, vanilla would keep the
    # far tail (memmove 1 byte, PA len 6, NUL@6) against ours' len 5
    # -- DIFF.  The image holds 0xFFFFFFFF there, so min(clamp,tail)
    # = tail, part1 memmove n=0, PA(end=5,0): both sides len 5,
    # NUL@5.  set20 measured EQUAL, which is what settled the value.
    'set20-self-tail-beyond-clamp': dict(
        W, self=True, tbuf=True, tsize=0x20008,
        tcontent=b'\xAA' * 0x20004 + b'\xEE', tlen=0x20005,
        tres=0x20005, count=5),
}
