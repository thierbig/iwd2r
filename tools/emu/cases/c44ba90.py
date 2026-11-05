"""0x44BA90 TString::ConstructCopy(TString* dest, const TString* src).

    static cdecl, two pointer args, returns void.  Both sides open with
    the same prologue block: NULL dest -> bare ret; [dest] = [src] flag
    byte; zero dest's m_pData/m_nLen/m_nRes (vanilla 0x44BAC9-0x44BACF,
    ours 0x82C6CD-0x82C6D5); then read src's m_nLen -- AFTER the
    zeroing, so a dest == src call always observes len == 0 and the
    self-assign Xran branch is dead code on both sides (vanilla reads
    [esi+8] at 0x44BAD2, ours [ebx+8] at 0x82C6CC; neither can see the
    pre-zero length).  No set can fire the Xran stub; it stays as a
    tripwire.  The zeroing also makes dest seeding inert -- final_state
    replays writes onto the all-zero this_init snapshot (machine.py),
    so only src's fields are observable input.

    Paths after the prologue (vanilla / ours 0x82C6A0):
      self, len==0 -- Detach; then the copy-count test (eax = [ebp+8]
          minus len, min with the clamp) is <= 0 and 0x44BB03
          `cmp esi,edi; jbe 0x44BB36` SKIPS memmove, PA and SetLength --
          the PA call at 0x44BB25 sits inside the count>0 branch -- so
          vanilla runs Detach a second time: calls are Detach, Detach.
          Ours matches exactly: Detach then a tail-jmp to Detach
          (0x82C6DE, 0x82C6E9).  (An earlier reading put vanilla's PA
          call on the self path unconditionally; the jbe guard says
          otherwise and set7's EQUAL agrees.)
      share (len>0, rc < 0xFE) -- Tidy(dest,1); adopt src's
          pData/m_nLen/m_nRes; rc++ at pData-1.  Store order identical:
          vanilla 0x44BB7F/0x44BB85/0x44BB8C/0x44BB96, ours
          0x82C711/0x82C717/0x82C71E/0x82C721.  Vanilla clamps len to
          the .data constant at 0x8485C0 (image value 0x0001FFFF) and
          only shares when the clamp did not bite; ours has no clamp --
          equivalent for every len < 0x1FFFF this case can seed.
      deep (len==0, or rc >= 0xFE) -- PA(dest,len,1); on FALSE bail
          (set6); copy len bytes from src's buffer to dest's (vanilla
          inline rep movsd/movsb 0x44BBC3-0x44BBD4; ours memcpy@0xB6E3A5
          IAT thunk 0x82C748, stubbed iat+copy+inline_ok as in c4f8fa0);
          then m_nLen=len + NUL at pData[len] -- INLINE in vanilla
          (0x44BBD6-0x44BBDC), via a REAL SetLength call in ours
          (0x82C753 -> 0x82CA40).  SetLength is left unstubbed: ours'
          call runs the real three instructions, which write exactly
          vanilla's inline pair, and an unstubbed call adds nothing to
          the call list -- so the deep sets compare writes only.  PA is
          stubbed to its return value alone, so unlike the real one it
          allocates nothing: dest.m_pData stays 0 after the prologue
          zeroing and the copy destination is NULL on BOTH sides.  The
          deep sets map the zero-filled NULL page (uc.mem_map(0,
          0x1000)) so vanilla's rep movs and ours' memcpy stub both
          land at raw 0..len -- the copy's bytes, offsets and the call
          sequence are what gets measured, not buffer identity.

    Verdict notes: set5 (len 3) is EQUAL(state,layout) -- vanilla's rep
    movsb records three 1-byte writes at raw 0..2, ours' copy action
    records one dword chunk (machine.py _rec_write_bytes rounds up to a
    4-byte store), and raw writes replay into no region snapshot, so
    the final-state compare is unaffected.  len 8 divides evenly, so
    set4's chunks match rep movsd exactly (EQUAL(layout)).  Corrupt-
    state divergence NOT exercised: src m_pData==0 with len>0 makes
    vanilla adopt the image global 0x8485BC and bump rc at 0x8485BB
    (image byte 0xFF -> frozen), while ours adopts NULL (0x82C70E
    re-reads [ebx+4], not the 0xBE44CB fallback held in ecx) and bumps
    rc at 0xBE44CA (image byte 0x00) -- unnormalizable image-region
    writes on both sides, and unreachable through the public API (len>0
    implies a real buffer).

    TString is layout-identical both sides (16 bytes: flag byte 0,
    m_pData 4, m_nLen 8, m_nRes 0xC -- src/TString.h, precedent
    c44bc00), so offsets stay literal; no field map.  Shared/frozen
    source buffers come from preseed_heap so the rc++ write lands in
    the comparable heap region; an empty self string keeps pData == 0
    (the realistic state -- the share gate needs len>0, Tidy frees
    empties) and nothing on the self path dereferences it.

    Stubs (identical names both sides): Detach (thiscall 0, pop 0),
    Tidy (thiscall 1, pop 4), PrepareAssign (thiscall 2, pop 8, ret
    scripted per set), Xran (cdecl 0, noreturn -- dead branch, kept as
    a tripwire).  ours-only inline_ok: the memcpy IAT thunk.  SetLength
    deliberately NOT stubbed (see deep path).  The two cdecl pointer
    args are passed by value through 4-byte 'val' blocks so set1 can
    push a literal NULL dest.
"""

import struct

CASE = {
    'name': 'TString::ConstructCopy', 'short': 'TString::ConstructCopy',
    'exe': 0x44BA90, 'ours': 'TString::ConstructCopy',
    'convention': 'cdecl', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {
        'detach_orig': 0x44BD40,
        'detach_ours': ('map', '?Detach@TString@@IAEXXZ'),
        'tidy_orig': 0x448D50,
        'tidy_ours': ('map', '?Tidy@TString@@IAEXE@Z'),
        'pa_orig': 0x44BC20,
        'pa_ours': ('map', '?PrepareAssign@TString@@IAEEIE@Z'),
        'xran_orig': 0x7F9C0E,
        'xran_ours': ('map', '?Xran@TString@@KAXXZ'),
        'memcpy_ours': ('import', 'memcpy'),
    },
    'returns': 'void',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    dest = blk_alloc(16)                       # this-anchored TString
    src = dest if inp['self'] else blk_alloc(16)
    darg = blk_alloc(4)                        # cdecl args by value:
    sarg = blk_alloc(4)                        # raw pointers (0 = NULL)
    m = Machine(side, pe, addrs[side], 'cdecl', dest, 16,
                [('dest', darg, 4, 'val'), ('src', sarg, 4, 'val')])

    if inp['sbuf']:                            # shared/frozen source
        sbuf = m.preseed_heap(inp['sbuf'], inp['content'], inp['rc'])
    else:
        sbuf = 0                               # empty string, no buffer
    # src struct: flag byte in the 4-byte m_buf union head, then the dwords
    # (a bare '<B' + '<I' concat would land m_pData at +1, not +4)
    srcbytes = (struct.pack('<B', inp['sflags']) + b'\0' * 3 +
                struct.pack('<III', sbuf, inp['len'], inp['sres']))
    m.uc.mem_write(src, srcbytes)   # self: overwrites dest == src, one state
    m.uc.mem_write(darg, struct.pack('<I', 0 if inp['null_dest'] else dest))
    m.uc.mem_write(sarg, struct.pack('<I', src))
    if inp['deep']:
        # PA is stubbed to its return value only (no allocation), so
        # dest.m_pData is 0 after the prologue zeroing and both sides
        # copy to NULL -- give that address a zero page instead of
        # faulting; the copy bytes/offsets compare at raw 0..len
        m.uc.mem_map(0, 0x1000)

    m.add_stub({'name': 'TString::Detach', 'kind': 'code',
                'addr': addrs['detach_' + side], 'conv': 'thiscall',
                'nargs': 0, 'pop': 0, 'default': {'ret': 0}})
    m.add_stub({'name': 'TString::Tidy', 'kind': 'code',
                'addr': addrs['tidy_' + side], 'conv': 'thiscall',
                'nargs': 1, 'pop': 4, 'default': {'ret': 0}})
    m.add_stub({'name': 'TString::PrepareAssign', 'kind': 'code',
                'addr': addrs['pa_' + side], 'conv': 'thiscall',
                'nargs': 2, 'pop': 8, 'default': {'ret': inp['pa']}})
    m.add_stub({'name': 'TString::Xran', 'kind': 'code',
                'addr': addrs['xran_' + side], 'conv': 'cdecl',
                'nargs': 0, 'pop': 0, 'noreturn': True})
    if side != 'orig':
        m.add_stub({'name': 'memcpy', 'kind': 'iat',
                    'addr': addrs['memcpy_ours'], 'conv': 'cdecl',
                    'nargs': 3, 'pop': 0, 'action': 'copy',
                    'inline_ok': True})
    return m


W = dict(sflags=0, sbuf=0, content=b'', rc=0, len=0, sres=0x1F,
         pa=1, self=False, null_dest=False, deep=False)

SETS = {
    # NULL dest: bare ret before any write/call
    'set1-null-dest': dict(W, null_dest=True),
    # share: rc 1 -> 2, buffer/len/reservation adopted
    'set2-share': dict(W, sflags=0x5A, sbuf=7, content=b'HELO!',
                       rc=1, len=5),
    # share at the last shareable count: rc 0xFD -> 0xFE, full-width flag
    'set3-share-rc-edge': dict(W, sflags=0xFF, sbuf=3, content=b'X',
                               rc=0xFD, len=1),
    # deep, frozen source (rc >= 0xFE), len 8 divides the dword copy --
    # write chunks match vanilla's rep movsd exactly (EQUAL(layout))
    'set4-deep-frozen-len8': dict(W, sbuf=10, content=b'\xff' * 8,
                                  rc=0xFE, len=8, deep=True),
    # deep, len 3: rep movsb vs one rounded-up dword chunk ->
    # EQUAL(state,layout) by copy-action granularity (docstring)
    'set5-deep-len3': dict(W, sbuf=5, content=b'abc', rc=0xFF, len=3,
                           deep=True),
    # deep, PA refuses: bail after the flag/zero prologue
    'set6-deep-pa-false': dict(W, sbuf=7, content=b'HELO!', rc=0xFE,
                               len=5, pa=0, deep=True),
    # dest == src: len reads 0 after the zeroing, the count<=0 jbe skips
    # PA and SetLength -- Detach, Detach on both sides (EQUAL)
    'set7-self-empty': dict(W, self=True, sflags=0x77),
}
