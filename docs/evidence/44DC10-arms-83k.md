---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83k arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; all six arms this batch ported are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit e32ff362, 48 assertions): 316 on the granted feat bit plus sub_71E240's +5 cold resistance, the refused second grant that still reports ACTION_DONE, and the no-target error tail; 242 on max-hitpoint base and derived for the set and modify halves under both providers (CMessageChangeStat is BROADCAST_FORCED_OTHERS, deleted before the queue in every provider -- see Notes); 251 on the no-area early out with gold untouched and the AR1000 drop that takes the 25 gold to a ground pile; 318 on the base-stats flag set and clear; 304 on add, clamp at the derived maximum, the dead target's no-op, and the no-target done; 288 on a real AR1000 door's flag set and clear with exactly one queued CMessageDoorStatus per call"
kind: replay
parity_waiver: "none needed for 83k: parity ran 0 RED after each range (d249d9e1, 46a643ac, d8afbb35) and again at the tests commit (e32ff362). The RE004s the tool still lists at src/CGameAIBase.cpp:1955/:1956 are the 316 arm's calls to sub_71E240 and sub_4531B0, the tree's address-derived names for in-tree helpers (CGameSprite.cpp:13746 and :33529, both with their own ported bodies and address comments) -- the same rename-is-a-follow-up precedent as 83d's sub_761990 and 83i's sub_45C290; the other RE004/RE001 findings are earlier rounds', covered by the row's own waiver in 44DC10.md. Everything else the 83k arms call lives in tree under its own row or member"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83k arms

The dispatcher itself is 44DC10.md; this file records the six inline arms
Task 83k ported (replacing TODO stubs), the commits, and the verified exit
tails. Register roles as in 83d/83e/83i/83j: ebx = this, edi = &m_curAction
(this+0x476), ebp = pTarget, esi = returnValue, [esp+0x10] = bShare.
Address order is 304 (0x45074B), 288 (0x451638), then 316, 242, 251, 318
(0x452318 onward); the tree's switch cases appear in numeric order, as 83i
already noted for 305.

Exit tails (as in 83d/83e/83i/83j): 0x4526F7 done; 0x44E1DE plain release;
0x44E1DB forces ACTION_DONE then release; 0x4514DE the shared AddMessage
tail; 0x451451 ACTION_ERROR straight to release.

- 304: a NULL target forces ACTION_DONE and jumps release before bShare
  (0x45075E); the live path sets bShare and falls into release with
  ACTION_DONE. No release leak: bShare covers it.
- 288: a NULL target jumps release with ACTION_DONE and bShare clear
  (0x45164C); the live path sets bShare, constructs the message, and takes
  the AddMessage/release tail at 0x4514DC into 0x44E1DB (forced done).
- 316, 251, 318: a NULL target takes 0x451451 (entry-default ACTION_ERROR,
  release); the live paths set bShare and release with ACTION_DONE.
- 242 is the odd one: a NULL target jumps the done tail 0x4526F7, and the
  live path also ends at 0x452487 -> done. bShare is never set, so the
  share GetTargetShareType took is never released in the original either --
  the port keeps the listing's behavior (the object array's share count
  absorbs it; the engine's own object cleanup path balances the denial).

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10); nothing
else calls these ranges.

## Callees

- 316 AddFeat (0x452318-0x4523B5, d249d9e1) -- GetTargetShareType 0x45BDD0
  with TYPE_SPRITE; null jumps 0x451451. sub_763A40 0x763A40(feat, 0): the
  gate's fall-through when the bit is clear and a2==0 returns TRUE without
  the prerequisite table (sub_763200 skipped), so a plain feat is grantable
  on a fresh sprite; a refusal still ends ACTION_DONE. On grant:
  GetBaseStats 0x453190 copied, SetFeatValue 0x762890(bit + 1),
  sub_71E240 0x71E240(oldBase, oldDerived) diffs the copies to apply stat
  side-effects (AEGIS_OF_RIME: m_resistColdBase +5, clamped 0..100),
  sub_4531B0 0x4531B0 (field_562C = 1), ProcessEffectList 0x72DE60.
- 242 ChangeStat (0x4523BA-0x452487, d249d9e1) -- GetTargetShareType;
  null jumps done. SetAtOffset 0x75F800(stat, value, modify) with stat =
  specifics1, value = specifics2, modify = the low byte of specifics3
  (STAT_MAXHITPOINTS writes m_baseStats.m_maxHitPointsBase and
  m_derivedStats.m_nMaxHitPoints). Provider != NULL gates the message:
  operator_new 0x7FC95B, CMessageChangeStat ctor 0x453740(stat, value,
  modify, id, id) -- both ids the target's own -- then GetMessageHandler
  0x4537E0 / AddMessage 0x4F7500.
- 251 DropInventoryEX (0x45248C-0x452570, 46a643ac) -- GetTargetShareType;
  null jumps 0x451451. DropInventory 0x754EE0(FALSE, "") returns the arm's
  own returnValue: no area means ACTION_DONE with nothing touched; with an
  area the flag-0x4 items and gold go to a ground pile (MISC07) through
  CInfGame::GetGroundPile, creating a CGameContainer at the sprite's
  position if none exists. The equipment broadcast needs all three of
  GetSessionOpen 0x452B50, InControl 0x4530C0 == FALSE and the game save's
  m_cutScene == TRUE: operator_new, CMessageSpriteEquipment ctor 0x50B510,
  AddMessage(message, FALSE, BROADCAST_FORCED_OTHERS) named inline.
- 318 SetCreatureAreaFlag (0x452575-0x4525E5, 46a643ac) --
  GetTargetShareType; null jumps 0x451451. GetBaseStats 0x453190, then
  specifics2 != 0 sets specifics1's bits in m_flags, else clears them
  (~mask). ACTION_DONE into release.
- 304 AddHP (0x45074B-0x4507DC, d8afbb35) -- GetTargetShareType; null
  forces ACTION_DONE and releases. STATE_DEAD in the target's derived
  generalState skips the body (still ACTION_DONE). Otherwise
  m_specificID is added to m_baseStats.m_hitPoints, clamped at
  CDerivedStats::GetAtOffset 0x446DD0(STAT_MAXHITPOINTS).
- 288 SetDoorFlag (0x451638-0x4516BE plus the shared tail
  0x4514DC-0x44E1DB, d8afbb35) -- GetTargetShareType with TYPE_DOOR (the
  share resolution type-checks the target through its virtual object type,
  so the test uses a real ARE door); null jumps release with ACTION_DONE
  (0x45164C). CGameDoor::GetFlags 0x453140 read, specifics2 != 0 ORs
  specifics1's mask, else clears it (~mask), SetFlags 0x453150 writes.
  operator_new, CMessageDoorStatus ctor 0x4FD370(pDoor, this->m_id,
  door id) -- the caller is the actor, the target the door -- then the
  AddMessage tail into 0x44E1DB.

## Notes

- Queue semantics differ per arm, and the tests pin all three:
  CMessageChangeStat is BROADCAST_FORCED_OTHERS (CMessage.cpp:21601), so
  AddMessage 0x4F7500 forwards to Broadcast(message, to-self FALSE,
  ignore-control TRUE), whose to-self-FALSE side deletes the message before
  the queue in every provider -- the original hands it to the network
  marshal instead (omitted per docs/adr/0002-single-player-only.md), so
  the field change is the only observable and both provider halves assert
  an empty queue. CMessageDoorStatus is BROADCAST_FORCED (to-self TRUE),
  queued locally under the null provider -- one per call, as asserted.
  CMessageSpriteEquipment never fires in a test engine (session open +
  out-of-control target + running cutscene never co-occur), so 251 also
  asserts an empty queue.
- 316's sub_763A40(feat, 0) call is the arm's whole gate: with a2 == 0 the
  upgrade-feat switch cases are still checked but a plain feat falls
  through to TRUE without prereqs (sub_763200 is only reached for the
  upgrade cases). That is why the test grants AEGIS_OF_RIME (feat 0) on a
  fresh CHARBASE.CRE sprite: the second call finds the bit set and
  refuses, still ACTION_DONE, with m_resistColdBase unchanged at 5.
- The AR1000 load needs the m_bDisplayStale pump fixture the AR3000
  CreateCreature case uses: CInfGame::LoadArea waits for the flag to clear
  and the test engine has no display loop, so without the pump thread the
  test hangs in the loader (found as a do_wait hang; the pump clears the
  flag every millisecond). The door is then found by walking
  m_lVertSort and type-checking GetObjectType() == TYPE_DOOR -- AR1000
  has six (AR1000_Door1..6), and MakeDoor's zero vtable cannot stand in
  because GetTargetShareType type-checks virtually.
- Tests (e32ff362): the new [ai][data] case passes alone (48 assertions).
  The [ai] tag suite is not rerun for 83k; the order-dependent flake
  family 83e/83i/83j recorded (baseline SIGSEGV, victims
  test_sprite_actions.cpp:899/905, 2392, 2859, 3877, 5183, 5649) still
  stands, and the brief's rule is to run the new case alone.
