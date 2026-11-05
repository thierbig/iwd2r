"""0x45EA70 TString::MaxSize(this) -- the zero-arg leaf of the debt list.

Both images compile this to `mov eax,0xfffffffd; ret`: no memory traffic, no
calls, no stack args.  It pins the harness contract at the trivial end --
same entry conditions, same EAX, no writes, no calls, on both sides -- so
any verdict but EQUAL means the runner itself drifted.

Returns 'u32' — `UINT MaxSize() const` (src/TString.h:81); EAX compared
masked to 32 bits.  This is the case fix 1's reproduction used: patching
ours to `mov eax,0xfffffffc` must now read DIFF.
"""

CASE = {
    'name': 'TString::MaxSize', 'short': 'TString::MaxSize',
    'exe': 0x45EA70, 'ours': 'TString::MaxSize',
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {},
    'returns': 'u32',
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO                     # identical block addresses per side
    this = blk_alloc(16)
    return Machine(side, pe, addrs[side], 'thiscall', this, 16, [])


SETS = {
    'set1-leaf': dict(),
}
