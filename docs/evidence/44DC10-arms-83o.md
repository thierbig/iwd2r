---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83o arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; both arms are fully observable without game data and are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit 993d727b): 68 on its live path (the CallLightning writes on the caller's area infinity and the queued CMessageAddEffect's fields) and its no-target path, 131 on its live path (the in-tree sub_761650 stub runs; the death effect's flags 4, field_18C 1 and target-id source), its already-dead path and its no-target path. The caller's area is zeroed storage as MakeDoor gives its doors: CallLightning writes only CInfinity members, so no area file is needed"
kind: replay
parity_waiver: "none needed for 83o: parity ran 0 RED after each range and again at the rebuilt HEAD; the batch's edit regions added no lint findings (the row's remaining RE004/RE001 lines, L1648-L2336, are 83e/83i/83l arms' calls by address-derived names plus the waived Format cast -- none inside 83o's regions, and re_lint surfaces a varying subset of them per run, as the 83l waiver segment records), and the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d) covers the columns"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83o arms

The dispatcher itself is 44DC10.md; this file records the two inline arms
Task 83o ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d/83e: ebx = this, ebp = pTarget, esi =
returnValue, [esp+0x10] = bShare; edi enters as &m_curAction (this+0x476)
but both arms repurpose it (0 in 68, 1 in 131) once the action fields have
been read.

Exit tails these arms use: 0x44E1C7 loads g_pBaldurChitin from 0x8CF6DC
and lea ecx,[edx+0x4980] (the GetMessageHandler inline); 0x44E1D6 is the
AddMessage 0x4F7500 call; 0x44E1DB forces ACTION_DONE (`or esi,-1`) and
falls into the release test 0x44E1DE. Both arms reach 0x44E1DB on every
path -- miss, guard, and success -- so unlike the 290/291/292 misses
(0x451451 ACTION_ERROR), no path of 68 or 131 answers ACTION_ERROR.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 68 CallLightning (0x44E7A1-0x44E885, 75fbba87) -- GetTargetShareType
  0x45BDD0 with the byte at 0x84C507 (0x31 = TYPE_SPRITE); bShare is
  stored (0x44E7B6) before the null test, so the miss also leaves bShare
  set, and the null jumps 0x44E1DB (forced ACTION_DONE). Live path:
  esi = m_pArea [ebx+0x12] + 0x4CC (the GetInfinity inline, m_cInfinity),
  the GetPos virtual ([vtable+0x1C]) called twice on the target -- y at
  [eax+4] pushed first, then x at [eax] -- so the call is
  CInfinity::CallLightning 0x5D1340(pos.x, pos.y); in tree at
  src/CInfinity.cpp. Then operator_new 0x7FC95B (0x190 = sizeof
  CGameEffectDeath); its failure jump (0x44E808) only zeroes the pointer
  and the next store (0x44E832, m_effectAmount) dereferences it -- dead in
  tree with the throwing new; CGameEffect ctor 0x452C80, vtable 0x8487E8,
  m_effectID 13 at +0xC and field_18C 0 at +0x18C are covered by in-tree
  `new CGameEffectDeath()`; m_effectAmount 0 at +0x18, m_dwFlags 0x100 at
  +0x1C. CMessageAddEffect is inlined: operator_new 0x10, m_targetId +4 =
  the target's m_id [ebp+0x5C], m_sourceId +8 = this m_id [ebx+0x5C],
  vtable 0x8487CC at +0x0, m_effect at +0xC; a failed message new posts
  NULL (0x44E870) -- also dead. The arm loads g_pBaldurChitin itself and
  jumps 0x44E1D6: AddMessage(message, FALSE), then 0x44E1DB.
- 131 Kill (0x44EA6E-0x44EB32, 61a37f2c) -- GetTargetShareType (TYPE_
  SPRITE, same 0x84C507 byte), bShare stored before the test, null jumps
  0x44E1DB. The dead guard: eax = [ebp+0x920] (CGameSprite's
  m_derivedStats first dword, the general state), `test ah,8` = STATE_DEAD
  0x800; jne 0x44E1DB -- an already-dead target is left alone but still
  forces ACTION_DONE. Live path: CGameSprite::sub_761650 0x761650 on the
  target -- in tree under its address-derived name (src/CGameSprite.cpp),
  stub body, called not TODO'd; the same category the 83e waiver records
  for 292's call. Then the same 0x190 CGameEffectDeath construction
  (operator_new 0x7FC95B, dead failure jump 0x44EAC4 with the 0x44EAE4
  NULL deref, ctor 0x452C80, vtable 0x8487E8, m_effectID 13) with
  field_18C = edi = 1 at +0x18C, m_effectAmount 0 at +0x18, m_dwFlags 4
  at +0x1C, and m_sourceID +0x10C = the target's m_id [ebp+0x5C].
  CMessageAddEffect inlined as in 68 (failed message new posts NULL at
  0x44EB2B -- dead); the arm jumps 0x44E1C7 -> 0x44E1D6 (AddMessage) ->
  0x44E1DB.

## Notes

- Every path of both arms ends in the forced-ACTION_DONE tail 0x44E1DB,
  including the null-target and dead-target misses; the test pins all
  three miss paths to ACTION_DONE (the old stubs leaked the entry default
  ACTION_ERROR through `goto done`).
- Neither arm carries the "Memory Allocation problem" UtilAssert
  (0x780C00) that 292's news have: these failure branches only zero the
  effect pointer (the next store would dereference NULL) or post a NULL
  message. Both are dead in tree with the throwing new, and the code
  NOTEs say so against the listing addresses.
- Effect-member offsets: the CGameEffectBase header comments are
  base-relative (CGameEffectBase has no vtable); CGameEffect's vtable
  shifts them +4 in the real object, so the exe's +0xC/+0x18/+0x1C are
  m_effectID/m_effectAmount/m_dwFlags, while members declared in
  CGameEffect and CGameEffectDeath (m_sourceID 0x10C, field_188 0x188,
  field_18C 0x18C) match the exe directly. sizeof(CGameEffectDeath)
  0x190 matches the operator_new push.
- The trylevel stores (`mov byte ptr [esp+0x6f8/0x6fc], imm`, values
  0x13/0x14/0) in these ranges are MSVC bookkeeping, not body.
- Tests (993d727b): the new [ai][data] case passes alone (36 assertions)
  after a full rebuild at the rebuilt HEAD. Per the task budget the full
  [ai] tag was not rerun this batch; its order-dependent flake family is
  already recorded in 83e's notes and the task-82 progress notes.
