---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83h arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; all three arms this batch ported are observable without game data and are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit daa08463): case 243 fully on the game's variable hash -- the guard variable set to 1, the target variable incremented on an existing (pre-set) value, both CMessageSetVariable messages queued in order, and the second run changing nothing through the guard; cases 244/245 fully on the game's embedded journal -- AddEntry seeds a quest entry (type 1) in chapter 0 and a plain entry (type 4) in chapter 4, JournalEntryDone flips the quest entry's wType to 2, DeleteJournalEntry removes the chapter-4 entry through the all-chapters scan and leaves the quest entry's type at 2. Not driven and why: nothing -- these three arms have no game-data branch (the journal session-open UI branch inside DeleteEntry is a callee detail recorded in CGameJournal's own row, and the test path takes the session-closed branch the engine itself takes outside the journal screen)"
kind: replay
parity_waiver: "none needed for 83h: the row's own waiver in 44DC10.md (asm lints 2026-11-10 83d) already covers the columns, and parity ran 0 RED after the batch (structural parity: 0 RED, 0 error, 1 yellow, 2 warning, 1 skipped, 3 clean). The RE004 lints parity reports (src/CGameAIBase.cpp:1332/:1350/:1480/:1485/:1506/:1530/:1538/:1618) all fall in the 83d/83e/83g arms (sub_761990, sub_40CB20, sub_761650, sub_453170/sub_453160, sub_40CCA0) -- none in this batch's ranges (case 243-245 live at :1107-:1183 and call no sub_ helpers); they are the same address-derived-name category the 83d/83e/83g waivers record, and descriptive renames remain a follow-up"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83h arms

The dispatcher itself is 44DC10.md; this file records the three inline arms
Task 83h ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d-83g: ebx = this, edi = &m_curAction
(this+0x476), ebp = pTarget, esi = returnValue.

Exit tails (as in 83d-83g): 0x4526F7 done; 0x44E1DB forces ACTION_DONE
then release. The case 243 tail (0x4505EB) is an `or esi,-1` before the
CString destructors, then jmp 0x4526F7 -- ACTION_DONE straight to done,
no release (no target is acquired anywhere in 243/244/245).

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 243 IncrementGlobalOnce, range 1 -- both string parses
  (0x450299-0x450487, 8a192131) -- GetString1 0x41E1B0 and GetString2
  0x452B90 into CString temps, MakeUpper 0x7FD0F0 on each, then Find(':')
  0x7FD095, Left 0x7FAA72 / TrimLeft 0x7FAF47 / TrimRight 0x7FAEFB /
  Right 0x7FA9F6 splitting each string into scope and name on the first
  colon. All CString helpers are compiler traffic; the split is in tree as
  the same Left/Right/Trim sequence on the MakeUpper'd strings.
- 243 IncrementGlobalOnce, range 2 -- the guard and the pair
  (0x450487-0x450648, 51fde6ac) -- the guard lookup: strcmp 0x7E7A54 of
  scope1 against the "GLOBAL" literal at 0x8A7FBC; equal takes
  GetObjectGame 0x40CB10 -> CInfGame::GetVariables 0x4286A0 (direct call)
  -> FindKey as the variables object's vtable slot +4, else the same
  GetObjectGame -> CInfGame::GetArea 0x5B7510 (direct call) ->
  CGameArea::GetVariables 0x453090 -> FindKey (vtable +4), with no null
  check on the area and no LOCALS/MYAREA handling -- the exe's own
  omission, kept. A guard found with m_intValue (+0x28) == 1 jumps
  0x4505EB (the tail), changing nothing. Otherwise: m_specificID
  ([ebx+0x52C]) saved to esi, CString::Concat 0x7FCDFD builds
  scope1+name1 (colonless -- the format SetGlobal and IncrementGlobal
  parse with their Left(6) scope read), SetString1 0x41CD60 replaces
  string1, m_specificID = 1 (ebp), SetGlobal 0x45EDE0 (in tree, called as
  the member helper); then Concat builds scope2+name2, SetString1 again,
  m_specificID restored, IncrementGlobal 0x460300 (in tree). The exe
  passes SetGlobal a by-value CString copy of the name that the callee
  never reads -- a dead argument not even set up on the area path -- so
  the in-tree SetGlobal() without it is the same function. Both return
  values are discarded; the tail forces ACTION_DONE.
- 244 DeleteJournalEntry (0x450648-0x45066E, 31dd8828) -- the journal is
  reached inline, no helper calls: g_pBaldurChitin at 0x8CF6DC, game at
  +0x1C54, journal embedded at game+0x44C8 (`add ecx, 0x44c8`), then
  CGameJournal::DeleteEntry 0x4C7560 (in tree, CGameJournal.cpp) with
  m_specificID ([ebx+0x52C]); the tail is `or esi,-1` then jmp 0x4526F7 --
  ACTION_DONE.
- 245 JournalEntryDone (0x45066E-0x450694, 31dd8828) -- the same inline
  chitin+0x1C54/0x44C8 chain, CGameJournal::SetQuestDone 0x4C7220 (in
  tree, CGameJournal.cpp) with m_specificID; same forced ACTION_DONE tail.
  In tree both arms read GetObjectGame()->GetJournal(), the inline
  equivalent of the raw offset chain.

All callees are in tree; none stubbed, none newly ported (DeleteEntry and
SetQuestDone predate this batch in src/CGameJournal.cpp; SetGlobal and
IncrementGlobal are the dispatcher's own in-tree members). The six new
callees per function budget is untouched: zero new.

## Notes

- The colonless rejoin (Concat of scope+name without ':') is deliberate
  and listing-verified: the exe's SetGlobal/IncrementGlobal parse string1
  as Left(6) = scope, Right(len-6) = name -- "GLOBAL", "LOCALS" and
  "MYAREA" are each exactly six characters, so the split never needs the
  colon and the arm's rejoined string is exactly what the pair expects.
- The guard test reads only m_intValue == 1 (the value SetGlobal just
  wrote); any other value (including a fresh LOCALS-style 0) re-fires the
  pair, matching the exe's single comparison.
- A variable IncrementGlobal creates itself starts from an uninitialized
  CVariable (the original's default constructor is empty), so the test
  pre-sets the target through the SetGlobal action (assignment, 10 + 7 =
  17 deterministic) -- the nondeterminism is the exe's own, kept in tree.
- Tests (daa08463): both new [ai][data] cases pass alone (17 and 12
  assertions) and under systemd-run MemoryMax=8G. The [ai] tag run's
  known order-dependent flake family (test_sprite_actions.cpp) is
  pre-existing per the 83e/83g records; the new tests were run by name
  filter, not the full tag.
