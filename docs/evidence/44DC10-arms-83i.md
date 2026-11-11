---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83i arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; all six arms this batch ported are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit f19c5c6f, 48 assertions): 273/274 on both provider branches -- the other-provider branch via the queued CMessageToggleInterface (m_bHide, source, target), the null-provider branch via CScreenWorld::m_nAutoUnhideInterface, the counter sub_453780/sub_453790 maintain, because HideInterface/UnhideInterface stop at their field_4A24 guard without a visible area so the UI manager's hidden flag cannot flip in a test; 279 via the world timer moving by exactly the specifics delta; 280 via the queued CMessageAddEffect (fatigue effect, amount GetSpecifics2, flags and duration type 1, caller to sprite addressing) and both ACTION_ERROR paths (no sprite, non-zero specifics); 281/305 via the CAIObjectType stored into field_342/field_37E (the resolved sprite's GetAIType, or NOONE when nothing resolves)"
kind: replay
parity_waiver: "none needed for 83i: parity ran 0 RED after each range (8259742c, 910c5129, 63ba2ee1) and again at the tests commit. The lints the new arms raise are the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d, extended 2026-11-10 83i): the RE004s are the arms' calls by tree address-derived names (sub_453780/sub_453790 in 273/274, sub_45C290/sub_4530F0 in 281, sub_45C290/sub_453110 in 305), all in tree, and 0x45C290 is ported with its own row (45C290.md)"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83i arms

The dispatcher itself is 44DC10.md; this file records the six inline arms
Task 83i ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d/83e: ebx = this, edi = &m_curAction
(this+0x476), ebp = pTarget, esi = returnValue, [esp+0x10] = bShare. The
action table names both 279 and 280 "AdvanceTime"; the addresses are the
authority and the two arms are unrelated: 279 moves the world clock, 280 is
the rest refusal that applies a fatigue effect.

Exit tails (as in 83d/83e): 0x4526F7 done; 0x44E1DB forces ACTION_DONE then
release; 0x44E1DE the plain release test; 0x451451 ACTION_ERROR; 0x4514DE
the shared AddMessage tail (the AddMessage call site 0x44E1D6, forced
ACTION_DONE, release), entered from 280 through 0x4514DA. 273, 274, and 279
post nothing through the shared site -- 273/274 AddMessage inline in the
arm -- and all three jump 0x4526F7 (done, no release; no target was
acquired); 281/305 force ACTION_DONE at 0x44E1DB into the release block for
their target share.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 273 HideGUI (0x45114E-0x4511D5, 8259742c) -- CNetwork::GetServiceProvider
  0x452B40. Null provider (single player): GetScreenWorld 0x438550, the
  GetManager virtual (call [vtable+0xCC]), the accessor 0x45B240 on the
  manager (see Notes), test eax,eax / jne skips the body when it already
  reports hidden; the body is HideInterface 0x6926A0 then sub_453790(TRUE)
  0x453790. Any other provider: operator_new 0x7FC95B (0xE bytes =
  sizeof CMessageToggleInterface), the ctor 0x453670(TRUE, this m_id at
  [ebx+0x5C], m_id), GetMessageHandler 0x4537E0, AddMessage 0x4F7500
  (message, FALSE) inline in the arm. `or esi,-1`, jmp 0x4526F7: ACTION_DONE
  straight to done, no release, on either path.
- 274 UnhideGUI (0x4511D5-0x451282, 8259742c) -- GetServiceProvider. Null
  provider: GetScreenWorld, sub_453780 0x453780 (reads the auto-unhide
  counter), test eax,eax, zero skips the body; the body is UnhideInterface
  0x692850 then sub_453790(FALSE). Other provider: the CMessageToggleInterface
  ctor 0x453670(FALSE, m_id, m_id) and the same inline AddMessage as 273.
  ACTION_DONE, jmp 0x4526F7, no release.
- 281 MarkObject (0x451362-0x45139D, 910c5129) -- sub_45C290 0x45C290
  (thiscall on this; ported with the arm, 45C290.md) resolves the actee into
  ebp; non-null: bShare, the GetAIType virtual (call [vtable+0x10]) on the
  target, sub_4530F0 0x4530F0(result), which stores field_342; null:
  sub_4530F0(CAIObjectType::NOONE). Both paths jmp 0x44E1DB: forced
  ACTION_DONE into the release block (the share).
- 305 SetMyTarget (0x45139D-0x4513D8, 910c5129) -- the same two branches as
  281 with sub_453110 0x453110 (stores field_37E): sub_45C290, then bShare +
  GetAIType virtual + sub_453110, or sub_453110(NOONE); jmp 0x44E1DB into
  release. In tree the arm sits after case 295's stub, so switch order
  differs from address order.
- 279 AdvanceTime (0x4513D8-0x451427, 63ba2ee1) -- GetSpecifics 0x41E180,
  GetObjectGame 0x40CB10, GetWorldTimer 0x453010,
  CTimerWorld::AdvanceCurrentTime 0x54EDE0(specifics); then GetSpecifics
  again, GetObjectGame, CInfGame::FeedBack 0x5C2630(FEEDBACK_TIMEPASSEDREST,
  specifics, TRUE). `or esi,-1`, jmp 0x4526F7: ACTION_DONE, no release.
- 280 AdvanceTime (0x451427-0x4514FB, 63ba2ee1) -- GetTargetShareType
  0x45BDD0 with the byte at 0x84C507 (0x31 = TYPE_SPRITE); a null target
  jumps 0x451451 (ACTION_ERROR). bShare, GetSpecifics 0x41E180; non-zero
  jumps 0x451451 too (the rest is refused). The effect: ClearItemEffect
  0x4A2E00(&effect, 0x5D = CGAMEEFFECT_FATIGUE), GetSpecifics 0x41E190 into
  effectAmount(+4), dwFlags(+8)=1, durationType(+0xC)=1, a CPoint(-1,-1)
  temp built by the ctor helper 0x51DE20 (as in 83e's 292), DecodeEffect
  0x48C800(&effect, &m_pos at ebx+6, m_id at [ebx+0x5C], temp) -- the port
  passes GetId(), the virtual returning that member, as 83e's 297 does.
  operator_new 0x7FC95B, GetId 0x4530B0 twice (caller, then pTarget),
  CMessageAddEffect ctor 0x4533D0(pEffect, caller, target), jmp 0x4514DA into
  the shared 0x4514DE tail: GetMessageHandler 0x4537E0, AddMessage 0x4F7500,
  forced ACTION_DONE, release.

## Notes

- 273's guard is a call, not a member read in the listing: GetScreenWorld,
  the GetManager virtual, then 0x45B240 (Ghidra FUN_0045b240,
  0x45B240-0x45B243, `mov eax,[ecx]; ret`) whose return the arm tests. That
  is a compiler-generated one-instruction getter for the first dword of
  CUIManager, which the tree declares `/* 0000 */ BOOL m_bHidden`; the port
  reads the member (`GetManager()->m_bHidden`), the getter being compiler
  traffic per the brief rather than a body worth a row. 274's guard is a
  plain call to sub_453780 (0x45124B), no accessor involved.
- The auto-unhide counter the 273/274 single-player path runs on is
  CScreenWorld::m_nAutoUnhideInterface (`/* 11CC */`): sub_453780 returns
  `m_nAutoUnhideInterface > 0` (CScreenWorld.cpp:5259) and sub_453790
  increments or decrements it (5265). The port calls the two helpers 1:1.
- The exe's action table carries the same "AdvanceTime" name on 279 and 280;
  the task brief flagged the duplicate and trusted the addresses. 279 hands
  the specifics (a time of day) to the world timer and the feedback window;
  280 is the fatigue half of resting -- sprite resolved, specifics zero,
  specifics2 the fatigue amount.
- Tests (f19c5c6f): the new [ai][data] case passes alone (48 assertions) and
  after a full rebuild at HEAD. The [ai] tag run fails around the new test
  with SIGSEGVs at varying lines (first run test_ai.cpp:2772 in the 296
  ChangeStoreMarkup area plus test_sprite_actions.cpp:899/905; a rerun
  elsewhere) -- proven pre-existing: with tests/test_ai.cpp reverted to base
  (the ported src arms still in) and rebuilt, the baseline [ai] run fails
  every time (three runs: SIGSEGV at test_sprite_actions.cpp:2859 and
  truncated case counts), while the forward tree's failing case is "ai:
  ExecuteAction 227 CreateCreatureObject..." at test_ai.cpp:1857, which runs
  before the new test and passes alone twice. Same order-dependent [ai]
  flake family 83e recorded (victim test_sprite_actions.cpp:3877; earlier
  2392), not a regression of this batch.
