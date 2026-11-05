"""0x4C5ED0 TString::Compare(this, int nPos, int nCount, const char* pStr2, int nLen2) const.

    if ((UINT)nPos > m_nLen) Xran();                       (range check)
    cnt = min(nCount, m_nLen - nPos);                 (clamp to remainder)
    n   = min(cnt, nLen2);
    r   = memcmp(m_pData + nPos, pStr2, n);           (byte-order semantics)
    if (r)  return r;                                 (-1 / +1)
    if (cnt < nLen2) return -1;                       (proper prefix)
    return cnt != nLen2 ? +1 : 0;                     (proper suffix / equal)

A pure const comparison: no writes anywhere, no allocator, no state --
the entire contract is EAX in {-1, 0, +1}.  The sbb/sbb pair after
`repe cmpsb` reproduces memcmp's unsigned-byte ordering, and the
epilogue turns a zero memcmp into the prefix/suffix verdicts.  nPos is
checked unsigned against m_nLen (jae), so every set keeps
0 <= nPos <= m_nLen; Xran (0x7F9C0E vanilla) is stubbed noreturn but
never fires, same treatment as UtilAssert in the message cases.

TString is layout-identical both sides (16 bytes, src/TString.h),
offsets literal.  pStr2 is a plain char* -- a raw block, no refcount
byte shape needed.  Sets cover equal / proper-prefix (-1) /
proper-suffix (+1) / mismatch in both byte orders / nPos>0 with nCount
over clamping / empty-vs-empty with m_pData null (memcmp of zero bytes;
esi is zeroed but never dereferenced) / empty-vs-nonempty (-1).

Returns 'u32' -- `int Compare(int, int, const char*, int) const`
(src/TString.h:171); the value is sign-full in EAX.
"""

import struct

CASE = {
    'name': 'TString::Compare', 'short': 'TString::Compare',
    'exe': 0x4C5ED0, 'ours': 'TString::Compare',
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {'xran_orig': 0x7F9C0E, 'xran_ours': ('map', 'TString::Xran')},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO
    this = blk_alloc(16)
    npos = blk_alloc(4)
    ncount = blk_alloc(4)
    pstr2 = blk_alloc(0x20)
    nlen2 = blk_alloc(4)
    m = Machine(side, pe, addrs[side], 'thiscall', this, 16,
                [('nPos', npos, 4, 'val'), ('nCount', ncount, 4, 'val'),
                 ('pStr2', pstr2, 0x20, 'ref'), ('nLen2', nlen2, 4, 'val')])
    m.set_arg('nPos', 0, struct.pack('<i', inp['nPos']))
    m.set_arg('nCount', 0, struct.pack('<i', inp['nCount']))
    m.set_arg('nLen2', 0, struct.pack('<i', inp['nLen2']))
    m.uc.mem_write(pstr2, inp['pstr2'])
    if inp.get('preseed'):
        ps = inp['preseed']
        data = m.preseed_heap(ps['size'], ps['content'], ps['refcount'])
        m.set_this(4, struct.pack('<I', data))       # m_pData
        m.set_this(8, struct.pack('<i', ps['len']))  # m_nLen
        m.set_this(0xC, struct.pack('<i', ps['res']))
    m.add_stub({'name': 'TString::Xran', 'kind': 'code',
                'addr': addrs['xran_' + side], 'conv': 'cdecl',
                'nargs': 0, 'pop': 0, 'noreturn': True})
    return m


def _ps(content, ln):
    return dict(size=len(content) + 1, content=content,
                refcount=0, len=ln, res=len(content))


SETS = {
    'set1-equal': dict(nPos=0, nCount=5, nLen2=5, pstr2=b'hello\0',
                       preseed=_ps(b'hello\0', 5)),
    'set2-proper-prefix': dict(nPos=0, nCount=3, nLen2=5, pstr2=b'hello\0',
                               preseed=_ps(b'hel\0', 3)),
    'set3-proper-suffix': dict(nPos=0, nCount=5, nLen2=4, pstr2=b'hell\0',
                               preseed=_ps(b'hello\0', 5)),
    'set4-mismatch-lt': dict(nPos=0, nCount=3, nLen2=3, pstr2=b'abd\0',
                             preseed=_ps(b'abc\0', 3)),
    'set5-mismatch-gt': dict(nPos=0, nCount=3, nLen2=3, pstr2=b'abc\0',
                             preseed=_ps(b'abd\0', 3)),
    'set6-substr-clamp': dict(nPos=2, nCount=10, nLen2=3, pstr2=b'llo\0',
                              preseed=_ps(b'hello\0', 5)),
    'set7-empty-null': dict(nPos=0, nCount=0, nLen2=0, pstr2=b'\0',
                            preseed=None),
    'set8-empty-vs-one': dict(nPos=0, nCount=0, nLen2=1, pstr2=b'a\0',
                              preseed=_ps(b'\0', 0)),
}
