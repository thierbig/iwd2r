---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83g arm)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; the arm this batch ported is observable without game data and is driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit dc0a169e): case 293 fully on a live CHARBASE sprite -- the hiding-state clear, the stealth grey-out, the invisible effect stripped off the timed list, and the queued CMessageAddEffect to self carrying the remove-all-of-type opcode; the non-hiding target answers the 0x44E1DB forced ACTION_DONE and a NOONE actee keeps the ACTION_ERROR entry default through the done tail. Not driven and why: the toolbar-refresh branch (SetSelectedButton/GetSelectedCharacter/ResetState) -- the test sprite has no portrait, so GetCharacterPortraitNum answers -1 and the exe's own gate skips the branch; SetModalState(0, TRUE) is called but a fresh sprite's modal state is already 0, so its early-out leaves nothing observable"
kind: replay
parity_waiver: "none needed beyond the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d), which already covers the columns; parity ran 0 RED after each range. The new RE004 lints at src/CGameAIBase.cpp:1467/:1475 (sub_453170, sub_453160) are the same category the 83d/83e waivers record: in-tree helpers called by their existing address-derived names (CGameSprite::sub_453170 / sub_453160, CGameSprite.cpp -- the hiding-state getter/setter pair); descriptive renames are a follow-up"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83g arm

The dispatcher itself is 44DC10.md; this file records the one inline arm
Task 83g ported (replacing the last TODO stub in the dispatcher's action
table), split into two ranges over two commits. Register roles as in
83d/83e: ebx = this, edi = &m_curAction (this+0x476), ebp = pTarget,
esi = returnValue, [esp+0x10] = bShare.

Exit tails (as in 83d/83e): 0x4526F7 done; 0x44E1DB forces ACTION_DONE
then falls into release; 0x4514DE the shared AddMessage tail (AddMessage,
forced ACTION_DONE, release).

## Callers

The arm is a jumptable target of the dispatcher itself (0x44DC10);
nothing else calls this range.

## Callees

- 293 UnHide, range 1 -- hiding state and toolbar refresh
  (0x45197F-0x451C7A, 64da7460) -- GetTargetShareType 0x45BDD0 with the
  byte at 0x84C507 (0x31 = TYPE_SPRITE); a null target jumps 0x4526F7
  (done) BEFORE bShare is set, keeping the ACTION_ERROR entry default --
  unlike 294/270 which force ACTION_DONE on the same miss. bShare, then
  CGameSprite::sub_453170 0x453170 (the m_bHiding getter); a target not
  hiding jumps 0x44E1DB (forced ACTION_DONE) before any effect work.
  Hiding target: sub_453160(0) 0x453160 (the setter),
  SetModalState(0, TRUE) 0x71FC00 (early-outs when the sprite's modal
  state is already 0), SetStealthGreyOut 0x4531E0 with the dword at
  0x85BD1C (90). Toolbar: GetId 0x4530B0, GetObjectGame 0x40CB10,
  GetCharacterPortraitNum 0x5AF4E0 gated against the dword at 0x851878
  (-1; not a party member skips the whole block, exe order), then
  GetButtonArray 0x452FD0, SetSelectedButton 0x452C50 with 100 (0x64),
  GetActiveEngine 0x4537C0 and its GetSelectedCharacter (vtable+0xD0)
  compared to the portrait number, and the m_nState read 0x452C40
  compared against 0x66, 0x65, 0x68, 0x67, 0x69, 0x70, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x6a (102, 101, 104, 103,
  105, 112, 115-123, 106 -- the same sixteen the SetModalState idiom in
  CGameSprite.cpp reads), each equal skipping ResetState 0x588FF0.
  0x452C40 is in tree as a direct m_nState member read (the
  CGameSprite.cpp:1729 idiom), as is the GetObjectGame/GetButtonArray
  chain it shares.
- 293 UnHide, range 2 -- effect-list removal and report
  (0x451C7B-0x451D30, f2cb5dd8) -- GetTimedEffectList 0x4531C0, then
  RemoveAllOfType 0x4C08E0 on the TIMED list with (pSprite,
  effectID 0x14 = CGAMEEFFECT_INVISIBLE, posLeave =
  GetEquipedEffectList 0x452F30 -> GetPosCurrent 0x4531D0 -- the
  equipped list's current position is left in place, never matched on
  the timed list --, effectAmount -1 = any). The 0x30-byte stack
  ITEM_EFFECT is filled by ClearItemEffect 0x4A2E00 with 0x114 (276 =
  ICEWIND_CGAMEEFFECT_REMOVEALLOFTYPE), dwFlags(+8) = 0x14,
  effectAmount(+4) = 0. DecodeEffect 0x48C800 with the source point
  built by the packed CPoint ctor 0x452B00 from the target's id (the
  engine passes object ids where a CPoint is expected), the target id
  from GetId 0x4530B0, and the CPoint(-1,-1) target from 0x51DE20.
  operator_new 0x7FC95B (failure branch dead in tree, the throwing
  new), CMessageAddEffect 0x4533D0 with this m_id at [ebx+0x5C] pushed
  twice -- caller and target are both the acting object -- then the
  0x4514DE tail (GetMessageHandler, AddMessage 0x4F7500, forced
  ACTION_DONE, release).

All callees are in tree; none stubbed, none newly ported (the six new
callees per function budget is untouched: RemoveAllOfType, the effect
list getters, the hiding pair, SetModalState, SetStealthGreyOut and the
UI members all predate this batch).

## Notes

- The port keeps the exe's operand order for the portrait gate: the
  party-member test runs BEFORE SetSelectedButton, and the whole UI
  block is skipped when GetCharacterPortraitNum answers -1, so a
  non-party target takes none of the UI calls.
- RemoveAllOfType deletes the effects it removes after OnRemove (empty
  for the invisible effect -- no subclass override), which the test
  relies on to observe the strip through IsTypeOnList.
- Tests (dc0a169e): the new [ai][data] case passes alone and in the
  full test_ai.cpp run (35 cases, 708 assertions). The [ai] tag run
  aborts early on the known order-dependent flake family
  (test_sprite_actions.cpp; victims vary per run, this session 4730
  SIGABRT / 4300 SIGSEGV) -- proven pre-existing by rebuilding the
  2cf82acb src/CGameAIBase.cpp (the commit before both ranges) and
  rerunning: same early abort. Not a regression of this batch.
