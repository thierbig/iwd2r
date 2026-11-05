"""0x45EA70 TString::MaxSize(this) -- the zero-arg leaf of the debt list.

Both images compile this to `mov eax,0xfffffffd; ret`: no memory traffic, no
calls, no stack args.  It pins the harness contract at the trivial end --
same entry conditions, same EAX, no writes, no calls, on both sides -- so
any verdict but EQUAL means the runner itself drifted.
"""

CASE = {
    'name': 'TString::MaxSize', 'short': 'TString::MaxSize',
    'exe': 0x45EA70, 'ours': 'TString::MaxSize', 'expect_rva': 0x42C2B0,
    'convention': 'thiscall', 'this_class': None, 'this_size': 16,
    'members': (), 'field_mapped': False,
    'extra': {}, 'resolve': {},
}


def build(side, inp, pe, addrs, ctx):
    from machine import BLK_LO, BLK_PTR, Machine, blk_alloc
    BLK_PTR[0] = BLK_LO                     # identical block addresses per side
    this = blk_alloc(16)
    return Machine(side, pe, addrs[side], 'thiscall', this, 16, [])


SETS = {
    'set1-leaf': dict(),
}
