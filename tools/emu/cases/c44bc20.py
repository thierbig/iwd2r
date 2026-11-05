"""0x44BC20 TString::PrepareAssign(unsigned n, BOOL bDetach).

    thiscall, 2 dword args, ret 8, returns BOOL in AL ('u8' -- the
    upper bytes of eax are garbage on both sides).  TString is
    layout-identical both sides (16 bytes: flag byte 0, m_pData 4,
    m_nLen 8, m_nRes 0xC -- src/TString.h, precedent c44bc00/c44ba90),
    so offsets stay literal; no field map.  n/bDetach are passed by
    value through 4-byte 'val' blocks -- vanilla reads the bDetach LOW
    byte at [esp+0x14], ours at [ebp+0xC], the same pushed dword.

    Decision table (vanilla / ours 0x82C950), in execution order:

    Xlen gate: n > 0xFFFFFFFD -> noreturn assert (vanilla `cmp edi,
    0xFFFFFFFD; jbe` at 0x44BC27, bare call 0x7F9AC2; ours `ja
    0x82CA00` -> ILT -> Xlen).  Both sides terminate inside the stub
    (set1); the boundary value 0xFFFFFFFD itself passes the gate
    (set2).  Xlen is cdecl 0-arg, so the terminated args compare as
    empty on both sides.

    SHARED (pData != 0, rc byte at pData-1 not in {0, 0xFF}):
      n == 0 -- dec rc, then zero the m_pData/m_nLen/m_nRes trio.
          Vanilla inlines both halves (0x44BC50-0x44BC5C); ours inlines
          the dec (0x82C97A/0x82C97C) then delegates the trio to a
          REAL Tidy(this,0) call at 0x82C982 -- Tidy's bDetach==0 path
          has zero calls of its own, so the delegation is invisible in
          the call list and its three stores (0x82CA8E/95/9C) replay
          vanilla's inline stores exactly.  FALSE.  bDetach is ignored
          on this path on both sides.
      n > 0 -- UNCONDITIONAL Grow: no fit check, no release, no
          zeroing (vanilla 0x44BC4A `jne 0x44BD28` lands past the res
          compare at 0x44BD23, straight onto `push edi; call Grow`;
          ours 0x82C976 `jne 0x82C9F0` is the same shape).  The share
          release is deferred into Grow's own body, which calls
          Tidy(1) after copying -- so neither side writes rc here.
          TRUE.

    NOT shared:
      n == 0, bDetach -- release + zero trio, FALSE.  pData == 0
          skips the release (no call); rc in {0, 0xFF} frees with
          operator delete.  Vanilla inlines the Tidy body
          (0x44BC72-0x44BCAF); ours delegates to REAL Tidy(this,1) at
          0x82C99D, whose ONLY call is the stubbed delete.  Symmetric
          call multisets, identical writes.
      n == 0, keep -- m_nLen = 0 and NUL at pData when a buffer
          exists (vanilla 0x44BCB6/0x44BCB9, ours 0x82C9AE/0x82C9B5);
          no calls.  FALSE.
      n > 0, keep -- fit fast path: res >= n returns TRUE writing
          nothing (vanilla 0x44BD23 -> 0x44BD30, ours 0x82C9EB ->
          0x82C9F8); res < n calls Grow (TRUE).
      n > 0, bDetach -- small-buffer carve-out: when res < 0x20 AND
          res >= n the buffer is KEPT (TRUE, nothing) -- vanilla
          `cmp ecx,0x1f; ja` / `cmp ecx,edi; jae` at 0x44BCC7-0x44BCD1,
          ours `cmp eax,0x20; jae` / `cmp edi,eax; jbe` at
          0x82C9C6-0x82C9CC (same predicate, different encodings).
          Otherwise (res >= 0x20 regardless of fit, or res < n):
          release + zero trio + Grow (vanilla 0x44BCD3-0x44BD2B --
          delete at 0x44BD01, Grow at 0x44BD2B/0x44BCF4; ours Tidy(1)
          at 0x82C9D2 + Grow at 0x82C9DA).  pData == 0 takes the same
          tail with no delete (vanilla 0x44BD0A, ours Tidy(1) skips
          the release).  The shared-release inline at 0x44BCE4 is dead
          code -- the not-shared gate guarantees rc in {0, 0xFF}.

    Stubs (identical names both sides): TString::Xlen (cdecl 0,
    noreturn -- it FIRES in set1 on both sides, unlike the tripwire
    stubs in c44ba90/c4f8fa0), operator delete (cdecl 1, pop 0, ret
    0 -- vanilla 0x7FC984 is a cdecl CRT-free wrapper `push [esp+4];
    call 0x7E9689; pop ecx; ret` and ours frees through the VECTOR
    deallocator ??_V at 0xB41DA3 via ILT 0x413E9E, both
    caller-cleaned with `add esp,4` at the call sites, so one cdecl
    stub covers both), TString::Grow (thiscall 1, pop 4 -- the call
    sites rely on Grow's `ret 4` before their own `pop edi`).

    Grow is stubbed because the real body is an SEH-wrapped allocator
    (vanilla 0x44BE10 SEH prologue, ours 0x82C810 stack cookie +
    cmovs) whose VC6-vs-MSVC2022 codegen would drown PrepareAssign's
    own signal; the stub reduces it to its return-from-call.

    Tidy is deliberately NOT stubbed (the inverse of c44ba90, which
    stubbed it): PrepareAssign is the function that WOULD call Tidy,
    and ours really does (0x82C982/0x82C99D/0x82C9D2) while vanilla
    inlines the same logic -- stubbing would manufacture a calls-DIFF
    ([Tidy] vs []) that no behavioral difference causes.  Unstubbed,
    ours' delegation costs nothing in the call list and its writes
    are the comparison.  Tidy's address stays in extra for
    documentation only.
"""

import struct

CASE = {
    'name': 'TString::PrepareAssign', 'short': 'TString::PrepareAssign',
    'exe': 0x44BC20, 'ours': 'TString::PrepareAssign',
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {
        'xlen_orig': 0x7F9AC2,
        'xlen_ours': ('map', '?Xlen@TString@@KAXXZ'),
        'delete_orig': 0x7FC984,
        'delete_ours': ('map', '??_V@YAXPAX@Z'),
        'grow_orig': 0x44BE10,
        'grow_ours': ('map', '?Grow@TString@@IAEXI@Z'),
        # documentation only -- deliberately NOT stubbed (see docstring)
        'tidy_ours': ('map', '?Tidy@TString@@IAEXE@Z'),
    },
    'returns': 'u8',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(16)
    narg = blk_alloc(4)
    barg = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16,
                [('n', narg, 4, 'val'), ('bDetach', barg, 4, 'val')])

    if inp['buf']:
        # heap-backed string: content at ptr..ptr+len, rc byte at ptr-1
        pdata = m.preseed_heap(inp['size'], inp['content'], inp['rc'])
    else:
        pdata = 0
    # flag byte lives in the 4-byte m_buf union head, dwords start +4
    # (a bare '<B' + '<I' concat would land m_pData at +1, not +4)
    m.uc.mem_write(this, struct.pack('<B', inp['flags']) + b'\0' * 3 +
                   struct.pack('<III', pdata, inp['len'], inp['res']))
    m.uc.mem_write(narg, struct.pack('<I', inp['n'] & 0xFFFFFFFF))
    m.uc.mem_write(barg, struct.pack('<I', inp['b']))

    m.add_stub({'name': 'TString::Xlen', 'kind': 'code',
                'addr': addrs['xlen_' + side], 'conv': 'cdecl',
                'nargs': 0, 'pop': 0, 'noreturn': True})
    m.add_stub({'name': 'operator delete', 'kind': 'code',
                'addr': addrs['delete_' + side], 'conv': 'cdecl',
                'nargs': 1, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'TString::Grow', 'kind': 'code',
                'addr': addrs['grow_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': 0}})
    return m


W = dict(flags=0, buf=False, content=b'', size=0, rc=0, len=0,
         res=0x1F, n=0, b=0)

SETS = {
    # full-width n over the Xlen gate: both sides terminate in the
    # assert (0x44BC2E / 0x82CA00) -- the noreturn stub FIRES here
    'set1-xlen-n-fullwidth': dict(W, buf=True, size=13,
                                  content=b'HELO!', len=5,
                                  n=0xFFFFFFFF),
    # gate boundary: 0xFFFFFFFD passes `cmp edi,0xFFFFFFFD; jbe/ja`,
    # keep path has res < n -> Grow
    'set2-boundary-n-ffffffd': dict(W, buf=True, size=13,
                                    content=b'HELO!', len=5,
                                    n=0xFFFFFFFD),
    # plain (rc 0), n 0, detach: delete + zero trio, FALSE
    'set3-plain-n0-detach': dict(W, buf=True, size=13,
                                 content=b'HELO!', len=5, b=1),
    # frozen (rc 0xFF) edge: same not-shared release path
    'set4-frozen-n0-detach': dict(W, buf=True, size=13,
                                  content=b'HELO!', rc=0xFF, len=5,
                                  b=1),
    # no buffer, n 0, detach: zero trio, no calls, FALSE
    'set5-nodata-n0-detach': dict(W, b=1),
    # plain, n 0, keep: m_nLen = 0 + NUL at pData, no calls, FALSE
    'set6-plain-n0-keep': dict(W, buf=True, size=13,
                               content=b'HELO!', len=5),
    # shared (rc 3), n 0: rc 3 -> 2 + zero trio, FALSE -- bDetach
    # ignored on the shared-n0 path
    'set7-share-n0': dict(W, buf=True, size=13, content=b'HELO!',
                          rc=3, len=5),
    # last shareable count: rc 0xFD -> 0xFC (rc edge)
    'set8-share-n0-rc-edge': dict(W, buf=True, size=9, content=b'X',
                                  rc=0xFD, len=1, b=1),
    # shared (rc 2), n > 0: UNCONDITIONAL Grow -- no fit check, no
    # rc write (release deferred into Grow), TRUE
    'set9-share-n-fit': dict(W, buf=True, size=13, content=b'HELO!',
                             rc=2, len=5, n=4),
    # plain, n fits in res, keep: TRUE, nothing written
    'set10-plain-n-fit': dict(W, buf=True, size=13,
                              content=b'HELO!', len=5, n=4),
    # plain, n over res, keep: Grow only, TRUE
    'set11-plain-n-keep-grow': dict(W, buf=True, size=13,
                                    content=b'HELO!', len=5, n=0x20),
    # plain, detach, small buffer that fits (res < 0x20, res >= n):
    # buffer KEPT -- TRUE, nothing
    'set12-plain-trim-fit': dict(W, buf=True, size=13,
                                 content=b'HELO!', len=5, n=4, b=1),
    # plain, detach, res crosses the 0x1F/0x20 carve-out boundary:
    # big buffers are always released -- delete + zero trio + Grow
    'set13-plain-trim-bigres': dict(W, buf=True, size=13,
                                    content=b'HELO!', len=5, res=0x20,
                                    n=4, b=1),
    # full-width res takes the same trim-release path
    'set14-plain-trim-res-fullwidth': dict(W, buf=True, size=13,
                                           content=b'HELO!', len=5,
                                           res=0xFFFFFFFF, n=4, b=1),
    # plain, detach, small buffer that does NOT fit: delete + zero
    # trio + Grow
    'set15-plain-trim-smallgrow': dict(W, buf=True, size=13,
                                       content=b'HELO!', len=5,
                                       n=0x20, b=1),
    # no buffer, detach, n > res: zero trio + Grow, no delete
    # (n must EXCEED res -- res >= n keeps even with bDetach)
    'set16-nodata-trim-grow': dict(W, n=0x20, b=1),
}
