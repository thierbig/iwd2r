---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83j arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; all six arms this batch ported are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit f012fb8d, 44 assertions): 312/313 on the stored apparent/regular name, the unchanged-name skip, and the no-target tail (their CMessageSpriteUpdate is a to-others broadcast the single-player handler deletes, so the queue stays empty -- see Notes); 301/303 on the area header's 0x40 bit with its CInfinity mirror, the rest-chance clamp at 100 (the min inline 0x45EA80), and the not-in-control branch that still moves the header without posting; 314 on ACTION_DONE with the termination body guarded off (m_gameSave.field_1AC) because it wants the object cursor and the video stack; 315 on the one queued CGAMEEFFECT_XP CMessageAddEffect for a party of one, its amount compared with a direct AddExperienceParty CR call because MoncRate is table-driven"
kind: replay
parity_waiver: "none needed for 83j: parity ran 0 RED after each range (041bddbf, 8abba962, 1275b009) and again at the tests commit (f012fb8d). The RE004/RE001 findings the tool still lists are earlier rounds' -- the 281/305 arms' sub_45C290/sub_4530F0/sub_453110 calls and the dispatcher's default-arm Format -- covered by the row's own waiver in 44DC10.md; the 83j arms call only methods that live in tree under their own rows or members"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83j arms

The dispatcher itself is 44DC10.md; this file records the six inline arms
Task 83j ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d/83e/83i: ebx = this, edi = &m_curAction
(this+0x476), ebp = pTarget, esi = returnValue, [esp+0x10] = bShare -- with
301/303 reusing esi as the area-type / rest-encounter temp before the
return value is set. Address order is 312, 313 (0x451EAA), then 301, 303
(0x4520ED), then 314, 315 (0x452253); the tree's switch cases appear in
numeric order, as 83i already noted for 305.

Exit tails (as in 83d/83e/83i): 0x4526F7 done; 0x44E1DB forces ACTION_DONE
then release; 0x4514DE the shared AddMessage tail (AddMessage call site,
forced ACTION_DONE, release). 301/303/314/315 never acquire a target: all
four end `or esi,-1` into 0x4526F7 (done, no release) -- 301/303 through
the shared convergence at 0x45224B, 314 at 0x4522ED, 315 at 0x452310.
312/313 set bShare and release through 0x44E1DB on the miss and
unchanged-name paths, and through 0x4514DE after the update message.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 312 SetApparentNameSTRREF (0x451EAA-0x451F37, 041bddbf) -- GetTargetShareType
  0x45BDD0 with TYPE_SPRITE; a null target jumps 0x44E1DB (forced
  ACTION_DONE before bShare). GetBaseStats 0x453190 twice around
  GetSpecifics 0x41E180 -- the unchanged-name check skips the message the
  same way. Store, then operator_new 0x7FC95B, GetId 0x4530B0 read twice
  (caller and target both the sprite), the CMessageSpriteUpdate ctor
  0x50C260 at 0x451F2D, jmp 0x4514DE: the shared AddMessage tail, forced
  ACTION_DONE, release.
- 313 SetRegularNameSTRREF (0x451F37-0x451FA3, 041bddbf) -- the same arm
  against m_name (base stats offset 0): GetTargetShareType (null jumps
  0x44E1DB at 0x451F4B), GetBaseStats/GetSpecifics check and store,
  operator_new at 0x451F88, then jmp 0x451F12 back into 312's
  GetId/ctor/tail block, so both arms share the message construction at
  0x451F12-0x451F37.
- 301 SetExtendedNight (0x4520ED-0x45216A, 8abba962) -- GetHeader 0x68BFF0,
  GetSpecifics 0x41E180: specifics non-zero sets bit 0x40 of the header's
  area type, zero clears it (0xFFBF mask). GetHeader again, GetInfinity
  0x453080, CInfinity::SetAreaType 0x452C30 mirrors the word. InControl
  0x4530C0 at 0x45212D gates the message: operator_new 0x7FC95B, the
  CMessageSetAreaType ctor 0x4536B0 with esi (the new type) and this->m_id
  at [ebx+0x5C] read directly and pushed twice (the getter inlined, as
  83e's 297), then jmp 0x45222E into the GetMessageHandler 0x4537E0 /
  AddMessage 0x4F7500 sequence shared with 303. 0x45224B: `or esi,-1`,
  jmp 0x4526F7.
- 303 SetRestEncounterChance (0x45216A-0x452253, 8abba962) --
  GetHeaderRestEncounter 0x4530A0; the null check at 0x452174 jumps
  0x4521E5, before the message half, exactly as ported (a real header is
  never null). GetSpecifics 0x41E180 and GetSpecifics2 0x41E190, each
  clamped through the min instantiation 0x45EA80 (verified
  `min(const int&, const int&)`: cmp/jl on two dereferenced refs) against
  the 0x64 on the stack, storing [esi+0xA8]/[esi+0xAA] = m_probDay/
  m_probNight. InControl 0x4530C0 at 0x4521E7 gates: operator_new, the
  CMessageSetAreaRestEncounter ctor 0x453700 at 0x452225 re-reading the
  stored words (0x452211/0x45221A), GetMessageHandler 0x4537E0, AddMessage
  0x4F7500 at 0x452246. 0x45224B converges with 301 into done.
- 314 EndGame (0x452253-0x4522F2, 1275b009) -- GetSpecifics 0x41E180,
  GetObjectGame 0x40CB10, CInfGame::ReadyCharacterTerminationSequence
  0x5C2570(3, specifics). GetObjectGame again, GetVisibleArea 0x453040,
  CGameArea::GetId 0x453070 -- the message's caller and target are both
  the visible area's id, not this object's. operator_new 0x7FC95B, the
  CMessageEndGame ctor 0x4531F0(3, specifics, nAreaId, nAreaId); the comm
  type comes from the table word at 0x84820C (3 = BROADCAST_OTHERS) loaded
  inline at 0x4522C5 rather than through GetCommType, so the arm calls the
  3-arg AddMessage 0x4F7530(message, FALSE, BROADCAST_OTHERS) at
  0x4522E5. 0x4522ED: `or esi,-1`, jmp 0x4526F7.
- 315 AddExperiencePartyCR (0x4522F2-0x452310, 1275b009) -- GetSpecifics
  0x41E180, GetObjectGame 0x40CB10, CInfGame::AddExperienceParty
  0x5C0BB0(specifics, FALSE, TRUE) -- the CR flag TRUE is what separates
  this arm from case 164's plain call. 0x452310: `or esi,-1`,
  jmp 0x4526F7.

## Notes

- To-others broadcasts are not locally queued, and that is the original's
  behavior, not a gap. AddMessage 0x4F7500 (2-arg) forwards through the
  virtual GetCommType to the 3-arg 0x4F7530: CMessageSpriteUpdate reports
  BROADCAST_OTHERS, CMessageSetAreaType and CMessageSetAreaRestEncounter
  BROADCAST_FORCED_OTHERS, and 314 passes BROADCAST_OTHERS explicitly.
  With bForcePassThrough FALSE (every one of these arms), each lands in
  Broadcast 0x4F7620 with bSendMessageToSelf FALSE, and the original's
  local-delivery sites are all behind bSendMessageToSelf == TRUE: in the
  single-player branch the marshal is skipped (the provider check that
  jumps 0x4F77BF) and the to-self-FALSE counterpart at 0x4F77D0 deletes
  the message (0x4F76F5 on the guard path). The in-tree Broadcast
  (src/CMessage.cpp, the marshal omitted per docs/adr/0002-single-player-only.md)
  matches: `if (bSendMessageToSelf == TRUE) AddTail else delete` on both
  branches. So the tests pin these arms on their field effects and assert
  the queue stays empty; only 315 queues (CMessageAddEffect is SEND, and
  Send AddTails for the local player or a null provider).
- 0x45EA80, which the disassembly helper labels CGameAIBase::GetSpecifics,
  is not that getter: the bytes at 0x45EA80 are `mov eax,[esp+4]; mov
  ecx,[esp+8]; mov eax,[eax]; mov ecx,[ecx]; cmp eax,ecx; jl +2; mov
  eax,ecx; ret` -- the std::min instantiation over two const int refs.
  The port spells the clamp `nDay < 100 ? nDay : 100`; the listing pushes
  the address of the 0x64 constant and of the specifics result and calls
  0x45EA80 twice (0x4521A2/0x4521D6).
- The 314 test guards ReadyCharacterTerminationSequence off
  (m_gameSave.field_1AC = 1). The body's whole gate is that field; inside,
  SetCursor reaches SetGreyScale, which dereferences GetCurrentVideoMode()
  unconditionally (CInfCursor.cpp:230/236) -- no video stack exists in an
  engine test (the object cursor is created only at CBaldurChitin.cpp:893).
  The visible area must also be a registered slot, or GetVisibleArea()
  dereferences a null m_gameAreas[id]; the test parks a CGameArea(9) in
  the slot and pins m_mode and m_nCharacterTerminationSequenceDelay
  untouched.
- The 315 test seeds the sprite a class level matching m_nLevel
  (m_nClassLevels[CAIOBJECTTYPE_C_FIGHTER - 1]). The share calls
  GetMulticlassingPenalty 0x7646E0, whose second-chance loop re-picks
  iBestClass only from classes with a non-zero level and then asserts in
  CDerivedStats::GetClassLevel (0x447990, UTIL_ASSERT(iClassType > 0))
  when iBestClass is still 0 -- a levelled character without class levels
  cannot exist in the original, so the test's degenerate CHARBASE.CRE
  sprite needs the seed. Found by backtrace through UtilAssert:
  GetClassLevel <- GetMulticlassingPenalty <- AddExperienceParty
  (CInfGame.cpp:8767) <- ExecuteAction.
- Tests (f012fb8d): the new [ai][data] case passes alone (44 assertions)
  and after a full rebuild at HEAD. The [ai] tag run still dies in the
  order-dependent flake family 83e/83i recorded: with the new test in, a
  SIGSEGV at test_sprite_actions.cpp:5183 after 4 cases; with the test
  reverted to base (the ported src arms still in) and rebuilt, the same
  run dies at test_sprite_actions.cpp:5649 after 3 cases -- the baseline
  fails without the new test, so it is not a regression of this batch
  (victims before: 899/905, 2392, 2859, 3877).
