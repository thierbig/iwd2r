---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83e arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; the arms this batch ported that are observable without game data are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit 7f36d3eb): 290, 291, 292, 294, 295, 296 on their no-target/no-resolver paths (the return codes the exe's tails force), and 297 fully (the queued CMessageScreenShake). Not driven and why: the success cores of 290/291/294 are calls to 0x7624F0/0x482040/0x7625F0, which src does not recover (TODOs in the arms); 295's receiving-sprite resolver 0x45C290 is unported, so its branch never resolves; 296's markup call 0x5C9C90 is unported (the arm still answers the ACTION_DONE the exe forces after it). Case 293 UnHide (0x45197F-0x451D30) was excluded from this run by the task and keeps its stub"
kind: replay
parity_waiver: "none needed for 83e: the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d) already covers the columns, and parity ran 0 RED after each range. The new RE004 lints at src/CGameAIBase.cpp:1413/:1418/:1439 (sub_40CB20, sub_761650, sub_40CCA0) are the same category the 83d waiver records: in-tree helpers called by their existing address-derived names (CAIObjectType::sub_40CB20 CAIObjectType.cpp, CGameSprite::sub_761650 CGameSprite.cpp, CAIObjectType::sub_40CCA0 CAIObjectType.cpp); descriptive renames are a follow-up"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83e arms

The dispatcher itself is 44DC10.md; this file records the seven inline arms
Task 83e ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d: ebx = this, edi = &m_curAction (this+0x476),
ebp = pTarget, esi = returnValue, [esp+0x10] = bShare (esp = the frame point
after any outstanding argument pushes are undone by ret N), and 291's pActee
local sits at [esp+0x14]. Case 293 (0x45197F-0x451D30) is untouched.

Exit tails (as in 83d): 0x4526F7 done; 0x44E1DB forces ACTION_DONE then
release; 0x44E1DE the plain release test; 0x451451 ACTION_ERROR; 0x4514DE
the shared AddMessage tail (forces ACTION_DONE after the post, falls into
release); 0x4514DC the same tail entered with eax zeroed (allocation-failure
post of NULL); 0x451DC9 the shared actee ReleaseShare sequence (push -1,
push the THREAD_ASYNCH byte at 0x851873, GetId, GetObjectGame, GetObjectArray,
ReleaseShare 0x599E70), shared by 291 and 295.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 290 SpellHitEffectPoint (0x45176D-0x451805, 2939d2b9) --
  GetTargetShareType 0x45BDD0 with the byte at 0x84C507 (0x31 =
  TYPE_SPRITE); on miss, again with the byte at 0x84C502 (0x11 =
  TYPE_CONTAINER, both bytes read off the exe). Sprite path: bShare, then
  0x7624F0 (annotated CGameAIBase_SpellHitEffect) with (pTarget, GetDest
  0x452B70, 0, GetSpecifics 0x41E180, GetSpecifics2 0x41E190). Container
  path: bShare, GetId 0x4530B0, the GetPos virtual (call [vtable+0x1C]),
  then 0x482040 with (pTarget, GetPos(), GetId(), GetSpecifics,
  GetSpecifics2). Both calls are not ported (TODOs); both returns go to esi
  and jmp 0x44E1DE; a double miss jumps 0x451451 (ACTION_ERROR).
- 291 SpellHitEffectSprite (0x451805-0x45189C, 2939d2b9) --
  GetTargetShareType (TYPE_SPRITE), bShare, GetActee2 0x41E170,
  CAIObjectType::sub_40CB20 0x40CB20(this, TYPE_SPRITE, 0) -- in tree; it
  acquires a share. CGameObject::GetArea 0x40CB00 gates the body. Success:
  0x7624F0 with (pTarget, pActee GetPos virtual, pActee GetId,
  GetSpecifics, GetSpecifics2) -- not ported (TODO) -- then the shared
  0x451DC9 actee ReleaseShare and jmp 0x44E1DE. A null or area-less actee
  jumps 0x451451 without releasing the share sub_40CB20 acquired; the port
  reproduces that leak with a NOTE.
- 292 ChunkCreature (0x45189C-0x45197F, 2939d2b9) --
  GetTargetShareType (TYPE_SPRITE), bShare, CGameSprite::sub_761650 0x761650
  (in tree under its address-derived name), operator_new 0x7FC95B (0x30
  bytes = sizeof ITEM_EFFECT) with the "Memory Allocation problem"
  UtilAssert 0x780C00 (line 0x8EE) on failure -- dead in tree, the throwing
  new; the buffer is never freed (ClearItemEffect/DecodeEffect shallow-copy
  what they need and the message owns the decoded effect), CGameEffect::
  ClearItemEffect 0x4A2E00(effect, 13), the field writes dwFlags(+8)=8,
  numDice(+0x1C)=0, effectAmount(+4)=0x31, two CPoint(-1,-1) temps built by
  the ctor 0x51DE20, CGameEffect::DecodeEffect 0x48C800(effect, temp2, -1,
  temp1), CMessageAddEffect 0x4533D0(pEffect, this m_id at [ebx+0x5C],
  pTarget GetId), then the 0x4514DE tail (AddMessage 0x4F7500, forced
  ACTION_DONE, release). A null target jumps 0x451451 (ACTION_ERROR).
- 294 XEquipItem (0x451D30-0x451D7F, d1c4f984) -- GetTargetShareType
  (TYPE_SPRITE); a null target jumps 0x44E1DB -- forced ACTION_DONE, before
  bShare is set. The share path: bShare, GetSpecifics2, GetSpecifics, the
  GetString1 0x41E1B0 CString temp (the push-ecx sret slot, tracked at
  [esp+0x34]), then 0x7625F0 on the target with (CString, GetSpecifics,
  GetSpecifics2) -- not ported (TODO); its return goes to esi and the arm
  jumps 0x44E1DE.
- 295 TransferInventory (0x451D7F-0x451DED, d1c4f984) -- 0x45C290
  (annotated CGameSprite_B3AUTO_0045c290) on this, a __thiscall returning
  the receiving sprite -- not ported (TODO), so in tree pTarget stays null
  and the arm answers the 0x451451 ACTION_ERROR the exe takes on its null;
  the non-null branch is kept for structure: bShare, GetActee2, sub_40CCA0
  0x40CCA0(this, 0) -- in tree, acquires a share -- then 0x4C83F0
  (annotated CGameObject::FloatText) with (pTarget, pActee) -- not ported
  (TODO); its return is the action result; the shared 0x451DC9 tail
  releases the actee and the arm jumps 0x44E1DE. A null pActee jumps
  0x44E1DE directly (entry-default ACTION_ERROR).
- 296 ChangeStoreMarkup (0x451DED-0x451E5F, 4318ae9a) -- GetString1 into a
  CString temp, CResRef ctor 0x78A970 (in tree) on it, GetSpecifics3
  0x41E1A0 / GetSpecifics2 / GetSpecifics, GetObjectGame 0x40CB10, then
  0x5C9C90 (annotated CInfGame_B3AUTO_005c9c90) with (&resRef,
  GetSpecifics, GetSpecifics2, GetSpecifics3) -- not ported (TODO); the
  temps are not materialized in tree since the call is unported. The
  CString_Destruct 0x7FCC1A runs, then `or esi,-1` and jmp 0x4526F7:
  ACTION_DONE straight to done, no release (no target was acquired).
- 297 ScreenShake (0x451E5F-0x451EAA, 4318ae9a) -- operator_new 0x7FC95B
  (0x12 bytes = sizeof CMessageScreenShake), the failure branch je 0x45222C
  (post of NULL) dead in tree, this m_id at [ebx+0x5C] pushed twice,
  GetSpecifics3 / GetSpecifics2 / GetSpecifics, CMessageScreenShake ctor
  0x4534C0(duration, magnitudeX, magnitudeY, 1, m_id, m_id) -- push order
  maps GetSpecifics to duration, GetSpecifics2 to magnitudeX,
  GetSpecifics3 to magnitudeY -- then jmp 0x45222E: GetMessageHandler
  0x4537E0, AddMessage 0x4F7500(message, FALSE), `or esi,-1`, done. Fully
  ported; in tree `new CMessageScreenShake(GetSpecifics(), GetSpecifics2(),
  GetSpecifics3(), 1, GetId(), GetId())` + AddMessage, ACTION_DONE, done.

## Notes

- 294's null path is the same forced-ACTION_DONE tail as 270 (0x44E1DB
  before bShare), unlike 290/291/292 whose misses take 0x451451; the test
  pins the difference.
- 296 forces ACTION_DONE regardless of what 0x5C9C90 returns (esi is set
  after the call), so the TODO port keeps DONE, not the ACTION_ERROR entry
  default the other TODO arms keep.
- 291/295's pActee releases go through the shared 0x451DC9 sequence, in
  tree the inlined GetObjectGame()->GetObjectArray()->ReleaseShare(id,
  THREAD_ASYNCH, INFINITE) with the byte at 0x851873 as the thread flag and
  -1 as the wait.
- The trylevel stores (`mov byte ptr [esp+0x6fc/0x6f8/0x700], imm`) and the
  /GS cookie writes in these ranges are MSVC bookkeeping, not body.
- Tests (7f36d3eb): both new [ai][data] cases pass alone and after a full
  rebuild at HEAD (19 assertions). The [ai] tag run aborts early on a
  SIGSEGV at tests/test_sprite_actions.cpp:3877 ("sprite: Rest refills a
  memorized but spent spell", inside StartEngine) -- proven pre-existing:
  the same run against a build of the base 27e1e703 src/CGameAIBase.cpp
  fails the same way, and the test passes alone; it is the known
  order-dependent [ai] flake family recorded in the task-82 progress notes
  (earlier victim test_sprite_actions.cpp:2392), not a regression of this
  batch.
