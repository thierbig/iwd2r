---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83m arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; three of the four arms this batch ported are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit 0ced8a86): case 185 fully on the three master-area 2DA outcomes (row found and area loaded stores m_pGameAreaMaster and finishes ACTION_DONE, area not loaded finishes ACTION_ERROR, resref not in the 2DA finishes ACTION_ERROR), case 287 fully (null target forced DONE through the release, STATE_DEAD early-out, both specifics2 percent branches including the store-if-higher skip), and case 284 on the null target, the short-name no-op dead path (see Notes), the full 32-char rename flow (one m_dwFlags 6 area-resref message, the old name dropped from both named-creature hashes, the new name registered in the game hash), the empty-name 'none' fresh m_dwFlags 4 GLOBAL message, the unknown-name no-op, and the case-match no-op. Not driven and why: case 199, whose first act is the non-returning UTIL_ASSERT(FALSE) (0x780C00 suspends threads, shuts down and aborts the process); the dead message post behind the assert is recorded in the arm's source comment and Callees entry"
kind: replay
parity_waiver: "none needed for 83m: the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d, extended by the 83i and 83l segments) already covers the columns, and parity ran 0 RED after the batch (structural parity: 0 RED, 0 error, 1 yellow, 2 warning, 1 skipped, 3 clean). The RE004 lints parity reports (src/CGameAIBase.cpp:1483-:2268) all fall outside this batch's ranges (case 185 at :911-:934, case 199 at :973-:983, case 284 at :1646-:1713, case 287 at :1727-:1778; the 83m arms call no sub_ helpers -- every callee is a named in-tree method); they are the same address-derived-name category the 83d/83i/83l waiver segments record"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83m arms

The dispatcher itself is 44DC10.md; this file records the four inline arms
Task 83m ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d-83l: ebx = this, edi = &m_curAction
(this+0x476), ebp = pTarget, esi = returnValue.

Exit tails (as in 83d-83l): 0x4526F7 done; 0x44E1DB forces ACTION_DONE
then release. Cases 185, 199 and 284 reach 0x4526F7 without the release
(284 acquires a target but never sets bShare -- the original's quirk,
kept); 284's null target at 0x45127A is the same forced ACTION_DONE with
no release. Case 287 passes through the release on every path (bShare set
TRUE at 0x451513): the null target (0x45150E) and the dead-target and
no-store tails (0x4515E0) force ACTION_DONE first, the store path
(0x451633) falls into the same block.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 185 SetMasterArea (0x44FDCF-0x44FED7, 58552db6) -- no target acquired.
  GetString1 0x41E1B0 (the area resref), then C2DArray::Find 0x4037A0 on
  m_ruleTables.m_tMasterArea (CInfGame+0xA60) with CPoint(-1,-1) and TRUE.
  Row found: GetString1 again -> GetObjectGame 0x40CB10 ->
  CInfGame::GetArea 0x5B7510 (direct call); a loaded area stores into
  m_pGameAreaMaster (CInfGame+0x3812) and ACTION_DONE at 0x4526F7, an
  unloaded one finishes ACTION_ERROR. Row not found: a third GetString1 +
  CString_Concat_With_CString_InPlace 0x7FCE63 with the literal at 0x8A7F90
  (": That's no master area, that's my wife!") builds an sError the arm
  never reads, then ACTION_ERROR. Find is the in-tree C2DArray::Find and
  GetArea the in-tree CInfGame::GetArea.
- 199 TextScreen (0x44FFB9-0x450093, 58552db6) -- the first act is
  UTIL_ASSERT(FALSE) 0x780C00 (expression literal "FALSE" at 0x8A636C,
  file literal at 0x8A7F60), which ends in abort(); nothing behind it
  runs. The dead body, for the record: operator_new 0x7FC95B allocates a
  CMessageStartTextScreen whose small ctor is inlined (the visible calls
  around it are the CResRef from-CString ctor 0x78A970 on GetString1's
  result, the CResRef copy ctor 0x78A960, and CResRef::operator= 0x78AC30
  into the member), then AddMessage 0x4F7500 and ACTION_DONE. The
  in-tree CMessageStartTextScreen and UtilAssert carry this as ported.
- 287 SetHPPercent (0x4514FB-0x451638, 25628184) -- GetTargetShareType
  0x45BDD0 (TYPE_SPRITE); null target -> 0x45150E forcing ACTION_DONE
  into the release. STATE_DEAD (bit 0x800) tested in the derived
  m_generalState (0x451523, GetDerivedStats 0x4531A0) or the base one
  (0x451535, GetBaseStats 0x453190) -> ACTION_DONE, target untouched.
  GetSpecifics 0x41E180 clamped to [0,100] and scaled to a fraction
  (0x451548-0x451580); GetSpecifics2 0x41E190 selects the value:
  0 -> the derived m_nMaxHitPoints, stored only when value*fraction is
  above the base m_hitPoints (0x4515D7-0x4515E0 otherwise jumps to the
  release tail without storing); nonzero -> the current base m_hitPoints.
  The store truncates through __ftol (0x45161C-0x45162F) into the base
  m_hitPoints SHORT. All in tree.
- 284 SetScriptName (0x44DCDA-0x44E042, 25628184) -- GetTargetShareType
  0x45BDD0 (TYPE_AIBASE); null -> 0x45127A forced ACTION_DONE, no
  release. sOldName is CString(m_scriptName char[32], 32) through
  CString_ConstructWithLength 0x7FA7F5 (call at 0x44DD01) -- the length
  ctor keeps embedded NULs, which is the dead-path finding in Notes.
  GetString1 0x41E1B0 -> sNewName; strcmp 0x7E7A54 against "" (literal at
  0x8C5CF8) with CString_operator_equ 0x7FCD57 substituting "none"
  (0x8A7FC4) when empty. CompareStrings 0x7E8182 (lstrcmpi,
  case-insensitive) then gates the body: equal names jump 0x44DD5A ->
  0x44DFFC, past both hash blocks, where the null-message check skips
  AddMessage into the destructors and the tail. Area path ([ebp+0x12] =
  m_pArea): CGameArea::GetNamedCreatures 0x40CAF0 -> RemoveKey at
  vtable+0x1C (implementation CVariableHash::RemoveKey 0x5511C0) with
  (sOldName, TRUE, GetId 0x4530B0); on TRUE, CResRef::CopyToString
  0x78A9F0 (the area resref), a new CMessageSetVariable (ctor inlined;
  CString_operator_equ 0x7FCD07 at 0x44DE13/0x44DE20 assigns scope and
  name) with m_dwFlags = 2, then CVariable::CVariable 0x452BD0 +
  CString_Construct 0x7FC98F + CVariable::SetName 0x452C60 -> AddKey at
  vtable+0 (0x44DE8B) registering the new name in the game hash. Game
  path: GetObjectGame 0x40CB10 -> CInfGame::GetNamedCreatures 0x40CAE0 ->
  RemoveKey at 0x44DEE1; on TRUE a fresh CMessageSetVariable("GLOBAL")
  with m_dwFlags = 4, or 0x44DFA8 joining with |= 4, and the same
  CVariable/SetName/AddKey at 0x44DFF8. pSetVariable non-null ->
  AddMessage 0x4F7500 (0x44E013). 0x44E03D jumps to 0x4526F7: ACTION_DONE,
  bShare never set, no release. All in tree.

All callees are in tree; none stubbed, none newly ported. The six new
callees per function budget is untouched: zero new.

## Notes

- The case 284 dead path (listing-verified): the padded sOldName ctor
  plus CVariableHashBase::Hash 0x54FAD0 -- which hashes every byte of
  GetLength() up to 32, a NUL contributing PRIME[i]*0 and so collapsing
  the multiplicative term to 0 -- buckets the padded 32-byte lookup
  differently from the short name the engine registered through the
  strlen ctor (CInfGame.cpp:2706). RemoveKey 0x5511C0 (its normalization
  appends toupper of every byte past GetLength() up to 0x20 -- the padded
  NULs qualify, since NUL != 0x20; its id check is
  `!a2 || entry.m_intValue(+0x28) == nValue`) therefore finds nothing and
  returns FALSE in both hashes. In the original exe, case 284 is a no-op
  for any script name shorter than 32 chars: the removal, both messages
  and the registration run only for names that fill the char[32] exactly.
  The port reproduces this byte-for-byte (same padded ctor, same Hash
  over the full length), and the test pins both behaviors -- the
  short-name no-op (no message, both hashes untouched, m_scriptName
  unchanged) and the live 32-char flow.
- Case 284 never writes m_scriptName: the rename exists only in the
  named-creature hashes and the CMessageSetVariable broadcast, so the
  test sets the field directly to reach the live path -- state setup, not
  engine behavior.
- The two compares in 284 are deliberately different: the empty-string
  default is a case-sensitive strcmp against "", the old-vs-new gate the
  case-insensitive lstrcmpi wrapper, ported as strcmp and _stricmp.
- The case 185 sError (name + ": That's no master area, that's my wife!")
  is never read before the arm finishes ACTION_ERROR -- kept dead, as the
  exe has it.
- Tests (0ced8a86): the new [ai][data] case ("ai: ExecuteAction 185 287
  and 284 master area hp percent and script name") passes alone (53
  assertions) under systemd-run MemoryMax=8G. Case 199 is not drivable
  for the abort above. The [ai] tag run's known order-dependent flake
  family (test_sprite_actions.cpp) is pre-existing per the 83e/83g/83h
  records; the new test was run by name filter, not the full tag.

- Reviewer note (284, open question): the two "no area-message" paths
  load the pending-message register from the frame, not from zero --
  0x44DEA0 (no area or area RemoveKey failed) and 0x44DFFC (names equal)
  both do `mov esi, [esp+0x30]`, and 0x44E000 posts whatever is there
  when non-null. [esp+0x30] is the CString the prologue seeds with
  afxEmptyString (0x44DC45) and the shared tail destructs (0x4526FF), so
  in the exe those paths would hand AddMessage 0x4F7500 a string data
  pointer. The port treats them as "no message" (NULL). Not reproduced
  because it is undefined behavior; revisit if a same-name SetScriptName
  is ever observed to crash or not crash in the original.
