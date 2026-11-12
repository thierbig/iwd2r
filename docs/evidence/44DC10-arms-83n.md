---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83n arm)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; the arm this batch ported is driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit c42ed212): case 133 on all three of its branches -- a missing actee instance (GetShare fails, entry-default ACTION_ERROR, nothing queued), a sprite immune to the caller's AI type (the shared preamble releases the share, drops the target, ACTION_ERROR, nothing queued), and a live sprite (ACTION_DONE with the queued CMessageClearActions and the replacement NO_ACTION CMessageAddAction, both checked source/target ids, actionID, specifics1/2 and dest)"
kind: replay
parity_waiver: "none needed for 83n: the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d, extended by the 83i, 83l and 83m segments) already covers the columns, and parity ran 0 RED after the batch (structural parity: 0 RED, 0 error, 1 yellow, 2 warning, 1 skipped, 3 clean). The arm (src/CGameAIBase.cpp:626-:643) calls only named in-tree methods (ExecuteActionGetActee's inlined preamble, the non-virtual ClearActions overload, the CMessageAddAction ctor) -- no new RE004 rows; the one re_lint finding near the range (L613, the Kill stub's sub_761650) pre-exists at base first and is the same finding the 83d segment records at its then-line L612"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83n arm

The dispatcher itself is 44DC10.md; this file records the single largest
remaining inline arm Task 83n ported (replacing the TODO stub), the
commits, and the verified exit tails. Register roles as in 83d-83m:
ebx = this, ebp = pTarget, esi = returnValue, [esp+0x10] = bShare.

Exit tails (as in 83d-83m): 0x4526F7 done; 0x451451 ACTION_ERROR straight
to release; 0x44E1DE the plain release test.

## Callers

The arm is a jumptable target of the dispatcher itself (0x44DC10); nothing
else calls this range.

## Callees

- 133 ClearActions (0x44EC0A-0x44EE8B, 641 bytes, 35084f50) -- the
  shared actee preamble, inlined: CAIObjectType::sub_40CCA0 0x40CCA0 on
  &m_curAction.m_acteeID (lea ecx,[ebx+0x4B4], m_curAction at this+0x476
  + 0x3E) with checkBackList FALSE; the GetObjectType virtual (call
  [edx+4]) compared against the byte at 0x84C507 (0x31 = TYPE_SPRITE,
  read off the exe); CImmunitiesAIType::OnList 0x4E6FB0 on
  [pTarget+0xAE0] (= CGameSprite::m_derivedStats at 0x920 +
  CDerivedStats::m_cImmunitiesAIType at 0x1C0) with &this->m_typeAI
  (lea eax,[ebx+0x20]); an immune sprite is released through
  CGameObjectArray::ReleaseShare 0x599E70(pTarget->m_id at [ebp+0x5C],
  the thread byte at 0x851873 = 0, -1) and dropped (xor ebp,ebp). In tree
  this preamble is the shared ExecuteActionGetActee helper (the same one
  cases 140+ use), which ends in the SetTargetObject virtual at
  [vtable+0x70] (0x44EC69: mov eax,[ebx]; call [eax+0x70]).
- 133 continued: 0x44EC71 sets bShare ([esp+0x10] = 1) before testing
  pTarget; a NULL target takes 0x451451 (`mov esi,-2` then the release),
  the entry-default ACTION_ERROR. A live target calls the non-virtual
  CGameAIBase::ClearActions(CGameObject*) 0x463740 directly (0x44EC81)
  -- the CGameObject* overload at CGameAIBase.h:109, not the
  vtable+0x6C BOOL one -- whose return goes to esi (returnValue); it
  posts CMessageClearActions and answers ACTION_DONE.
- 133's message: operator new 0x7FC95B of 0xE2 bytes (= sizeof
  CMessageAddAction; vtables 0x847B24 derived / 0x847B40 base read off
  the exe, slots annotated to CMessageAddAction's in-tree methods); the
  failure branch (je 0x44EDFB, edi NULL) is dead in tree under the
  throwing new. The body stores pTarget->m_id at [edi+4] (m_targetId)
  and this->m_id at [edi+8] (m_sourceId) -- the CMessage(caller, target)
  order -- default-constructs the message's CAIAction at [edi+0xC]
  (0x403A40), swaps in the derived vptr, then CAIAction::operator=
  0x405090 assigns from a stack temporary built in place at
  [esp+0x1C4]: the inlined CAIAction(SHORT, const CPoint&, LONG, LONG)
  ctor 0x593D70 -- its m_actionID comes from the word at 0x847784 (0 =
  CAIAction::NO_ACTION, read off the exe), its three CAIObjectType
  members from the 11-arg ctor 0x40AE80 with ten zero arguments and
  nInstance -1 (the all-defaults CAIObjectType), its strings from the
  MFC empty-string singleton at 0x8C1758, m_specificID 0 /
  m_specificID2 -1 / m_specificID3 0, m_dest (-1,-1), m_internalFlags
  0, plus the ctor's documented create-and-destroy CAIAction quirk
  (0x403A40 ctor then 0x403B40 dtor at [esp+0x29C], CAIAction.cpp's
  "For unknown reason" FIXME). The temporary's CString dtors
  (0x44EE0F-0x44EE6E) run under the guard flag set at 0x44EDB0. The arm
  ends CMessageHandler::AddMessage 0x4F7500(message, FALSE) and jmp
  0x44E1DE (release per bShare, then done). Fully ported; in tree
  `new CMessageAddAction(CAIAction(CAIAction::NO_ACTION, CPoint(-1,-1),
  0, -1), m_id, pTarget->m_id)` -- the same construction site shape as
  CGameSprite.cpp's evil-cutoff ClearActions tail (0x72B9A0's case 27).

## Notes

- bShare is set before the NULL test (0x44EC71 writes [esp+0x10] then
  tests ebp), so even the ACTION_ERROR exit releases through the share
  path -- but on that path pTarget is already NULL (or was dropped by
  the immunity release), so the release test at 0x44E1DE skips both
  Release calls; the port keeps the order with the house
  `returnValue = ACTION_ERROR; goto release;` shape the other 0x451451
  arms use.
- ClearActions' own return (ACTION_DONE from 0x463740, or ACTION_ERROR
  only for a NULL target, which this arm has already excluded) is the
  arm's returnValue; the NO_ACTION message is posted unconditionally
  after it.
- The CMessageAddAction construction order in the listing (base fields,
  derived vptr swap, then operator= into the message's action) is what
  MSVC emits for the in-tree ctor's `: CMessage(caller, target)` +
  `m_action = action;` body ("NOTE: Uninline."); no separate semantics.
- The trylevel store (`mov dword ptr [esp+0x1C], 2` at 0x44EDB0) is the
  destructor-guard flag for the temporary's CString teardown, not body
  logic; /GS cookie writes in the range are MSVC bookkeeping.
- Tests (c42ed212): the new [ai][data] case passes alone under
  IWD2_DATA (19 assertions): missing actee, immune sprite (immunity
  list seeded with a copy of the caller's m_typeAI so OnList's
  OfType(pair, FALSE, FALSE) matches self), and the live-sprite success
  path with both queued messages checked. Not asserted: the
  SetTargetObject hook (the base CGameAIBase::SetTargetObject 0x799E60
  is an empty body in tree -- the virtual matters only for derived
  classes) and the message handler's processing of the pair
  (Run is outside the arm).
