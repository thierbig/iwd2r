---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83d arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; the arms this batch ported that are observable without game data are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit e70f2112): 189, 259, 260, 266, 267, 268, 270. Three arms are not driven and why: 258 -- on the empty test party CanRestParty answers TRUE, and RestParty runs RestPartyPrivate, whose tail asserts on and Selects the shared world engine (process-global state); 267 -- its export loop calls CharacterExport, which writes .chr files; 268 -- sub_761990 on the unequipped CHARBASE test sprite is indistinguishable from the initial state"
kind: replay
parity_waiver: "asm lints 2026-11-10 83d (recorded here; the gate reads the waiver of the row's own file 44DC10.md): lints - RE004 at src/CGameAIBase.cpp:1268/:1286 is CGameSprite::sub_761990, the tree's address-derived name for the in-tree helper (CGameSprite.cpp:31681) that the 268/270 arms call by its existing ported name; a descriptive rename is a follow-up, as the tree does for sub_42E1B0/sub_723BF0. RE001/RE002 at :1164/:1199 are the arms' empty-literal CString copies (the exe builds them from the \"\" at 0x8C5CF8) and RE001+RE003 at :1235 is the deliberate (LPCSTR) cast feeding Format's %s, which mirrors the listing's push of the string buffer; compiler traffic per the task brief. The remaining row findings (L612/L968/L1064/L1483) predate this batch and reproduce at base first. asm - the row's missing-in-src callees are pre-existing helper misses plus this batch's calls of in-tree methods (CanRestParty 0x5C1E30, RestParty 0x5C1160, GetNumCharacters 0x453020, GetCharacterId 0x452FE0, GetBaseStats 0x453190, CharacterExport 0x5A1AB0, DisplayTextRef 0x43E0E0) under their own rows"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83d arms

The dispatcher itself is 44DC10.md; this file records the seven inline arms
and the one out-of-line helper Task 83d ported (replacing TODO stubs), the
commits, and the verified exit tails. Register roles in the dispatcher
frame: ebx = this, edi = &m_curAction (this+0x476), ebp = pTarget, esi =
returnValue, the dword at [esp+0x10] = bShare.

Exit tails (all read off the listing):

- 0x4526F7 `done:` -- returns esi unchanged after the pending-action bookkeeping.
- 0x44E1DB -- `or esi,0xffffffff` then falls into the release test: forces
  ACTION_DONE before releasing. In tree `returnValue = ACTION_DONE; goto
  release;`.
- 0x44E1DE `release:` -- the plain release test; does not touch esi.
- 0x451451 -- `mov esi,0xFFFFFFFE` then the release test: ACTION_ERROR.
- 0x45222E -- shared message-post tail: GetMessageHandler, AddMessage,
  `or esi,-1`, jmp done. Ends ACTION_DONE.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 189 arm (0x44FF15-0x44FF1E, fc5af5f7) -- `mov ecx,ebx; call 0x466910;
  mov esi,eax; jmp 0x4526F7`: the whole arm is one call to the newly ported
  CGameAIBase::LeaveAreaLUAPanic (466910.md), return to the dispatcher's
  return register, done. In tree `returnValue = LeaveAreaLUAPanic(); goto
  done;`. Its one stub (0x4381C0) is documented in 466910.md.
- 258 RestUntilHealed (0x450BD9-0x450CEB, c2a0af39) -- CAIAction::GetFlags
  0x452BC0 (a4 = (flags & 2) ? 1 : 0, via sub/neg/sbb/inc), CInfGame::
  CanRestParty 0x5C1E30(&strError, 0, 1, a4), CInfGame::RestParty 0x5C1160,
  the m_bHealPartyOnRest global 0x8E7524 bracketing it, CBaldurChitin::
  GetTlkTable 0x4537D0, CTlkTable::Fetch 0x7941D0, CBaldurMessage::
  DisplayText 0x43DF60 with the "" at 0x8C5CF8 and color 0x0000FF00
  (RGB(0,255,0)), CGameObject::GetId 0x4530B0 twice, STR_RES ctor
  0x4537F0 / dtor 0x425B80, CString_Construct 0x7FCC88 / Destruct
  0x7FCC1A. All in tree except the STR_RES/CString plumbing, waived as
  compiler traffic.
- 259 SetVisualRange (0x450CEB-0x450D24, c2a0af39) -- operator_new 0x7FC95B
  (0x10 bytes), CMessage92 ctor 0x453640(m_id, m_id, GetSpecifics()), then
  the 0x45222E tail: CMessageHandler via GetMessageHandler 0x4537E0,
  CMessageHandler::AddMessage 0x4F7500(message, FALSE). In tree `new
  CMessage92(m_id, m_id, m_curAction.GetSpecifics())` + AddMessage.
- 260 JumpToPointInstant (0x450D24-0x450D61, c2a0af39, success tail fixed
  in be07ee31) -- CGameAIBase::GetTargetShareType 0x45BDD0 with the byte at
  0x84C507 (CGameObject::TYPE_SPRITE), CAIAction::GetDest 0x452B70 (the
  CPoint pushed as two dwords), CGameSprite::JumpToPoint 0x745950(pt,
  FALSE).
- 266 DisplayMessage (0x450D61-0x450E19, bc7e8613) -- GetSpecifics via
  0x41E180, GetTlkTable 0x4537D0, Fetch 0x7941D0, DisplayText 0x43DF60
  with the "" at 0x8C5CF8, both colors 0x00D7C8A0 (RGB(160,200,215)), the
  marker from 0x84C960 (-1), GetId twice; STR_RES/CString plumbing as 258.
- 267 ExportParty (0x450E19-0x450FBC, bc7e8613) -- CString default ctor
  0x403310 (sFileName), CBaldurMessage::DisplayTextRef 0x43E0E0(-1, 0x68CB
  = 26827, RGB(0,0,0), RGB(255,255,0) = 0x0000FFFF, marker, 0, 0),
  CInfGame::GetNumCharacters 0x453020, CInfGame::GetCharacterId 0x452FE0,
  CGameObjectArray::GetShare 0x599A50 (retry while SHARED/DENIED, the
  status bytes at 0x851870-0x851873), CGameSprite::GetBaseStats 0x453190
  (flags at +8, `test ch,8` = m_flags bit 0x800), CAIAction::GetString1
  0x41E1B0, CString_Format 0x7FAEE8("%s%d" at 0x8A7F4C), CInfGame::
  CharacterExport 0x5A1AB0, CGameObjectArray::ReleaseShare 0x599E70 on
  SUCCESS only.
- 268 ClearPartyEffects (0x450FBC-0x451096, 527ae11e) -- the same party
  walk as 267 (GetNumCharacters, GetCharacterId, GetShare retry, GetBaseStats
  bit 0x800) calling CGameSprite::sub_761990 0x761990, ReleaseShare
  0x599E70 on SUCCESS.
- 270 ClearSpriteEffects (0x451096-0x4510C3, 527ae11e) -- GetTargetShareType
  0x45BDD0 (TYPE_SPRITE), then bShare = TRUE and CGameSprite::sub_761990
  0x761990 on the share.

sub_761990 is in tree (CGameSprite.cpp:31681) under its address-derived
name: RemoveAllEffectsIgnoreMoreThenPermanent(NULL, FALSE, FALSE, 0, 0),
UnequipAll(FALSE), EquipAll(FALSE). Its descriptive rename is a follow-up.

## Notes

- 189: the callee is the whole story; both its paths answer ACTION_DONE
  (466910.md), and the arm passes that to done.
- 258: `cmp eax,1; jne` -- CanRestParty must answer exactly 1 to rest. The
  rest path stores eax (which is 1 here) into the m_bHealPartyOnRest global
  0x8E7524, calls RestParty(1, 1) (both argument pushes reuse eax), and
  zeroes the global again. The refused path Fetches strError and, only when
  it resolves, DisplaysText it in green (0x0000FF00 twice). Both tails are
  `or esi,-1; jmp 0x4526F7` -- ACTION_DONE either way.
- 259: the whole arm is the CMessage92 post through the 0x45222E shared
  tail; a failed allocation posts NULL (0x45222C enters the same tail with
  eax zeroed). Ends ACTION_DONE.
- 260: NULL target goes to 0x451451 (esi = 0xFFFFFFFE = ACTION_ERROR, then
  the release test). The success path sets bShare, calls
  JumpToPoint(GetDest(), FALSE), and jumps to 0x44E1DB -- ACTION_DONE
  before release. The first port (c2a0af39) fell through to release with
  the initial ACTION_ERROR still in returnValue; be07ee31 added the
  explicit ACTION_DONE.
- 266: a pure display arm -- Fetch(GetSpecifics()) and, when it resolves,
  DisplayText with an empty name, the fetched text, RGB(160,200,215) for
  both colors, the -1 marker, and GetId() twice; tail `or esi,-1; jmp
  done`.
- 267: esi starts at 1 and is the 1-based running export index,
  incremented only when a character is actually exported (0x450F58), not
  per loop iteration; the loop counter is the local at [esp+0x14] against
  the signed GetNumCharacters result (movsx, jl). The 0x800-flagged
  characters get sFileName = Format("%s%d", GetString1(), index) and
  CharacterExport(id, sFileName); the flag is read as `test ch,8` on the
  dword at GetBaseStats()+8. ReleaseShare runs only on the SUCCESS path.
  Tail 0x450FA3 destroys sFileName, `or esi,-1`, done. (esi is reused:
  index during the loop, return code after.)
- 268: the same walk; every SUCCESS share with the 0x800 flag gets
  sub_761990, ReleaseShare on SUCCESS. The empty-party exit and every tail
  is `or esi,-1; jmp done` (0x45108E for the empty case).
- 270: NULL target jumps straight to 0x44E1DB (ACTION_DONE, no share to
  release); the share path sets bShare, calls sub_761990 on the target, and
  jumps to 0x44E1DB too.
- The trylevel stores (`mov byte ptr [esp+0x6f8/0x700/0x708/0x714], imm`)
  throughout these ranges are MSVC SEH bookkeeping, not body.
