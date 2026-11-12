---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 83l arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; all six arms this batch ported are driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit 236c9dbf, 79 assertions): 233 on the hidden bit through sub_75F3D0 with and without an area and the no-target error tail; 235 on the queued CMessageSetSequence in range and the forced-done tail out of range (bounds 0 and 17 read from the table bytes 0x85BBB0/0x85BBBB); 278 on the sprite drop, the container type dispatch and the other-type error exit; 282 on the case-insensitive area-level lookup, the new entry, the clamp to -1..1 and the 0x2 flag; 283 on the 0x2000 critical-path bit both ways with its sprite update; 325 on the and/or/xor of the area header flags and the ACTION_ERROR every path keeps"
kind: replay
parity_waiver: "covered by the 83l segment of the parent row's waiver in 44DC10.md (RE004 address-derived names sub_75F3D0 and sub_45C290, the compat CPtrList_AddTail named-callee miss); the arms' other calls are in tree under their own rows"
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 83l arms

The dispatcher itself is 44DC10.md; this file records the six inline arms
Task 83l ported (replacing TODO stubs), the commits, and the exit tails.
Register roles as in 83d-83k: ebx = this, edi = &m_curAction (this+0x476),
ebp = pTarget, esi = returnValue (entry default ACTION_ERROR),
[esp+0x10] = bShare. Exit tails: 0x4526F7 done; 0x44E1DB forces
ACTION_DONE then release; 0x44E1DE release; 0x451451 ACTION_ERROR then
release; 0x4514DE the shared AddMessage tail into 0x44E1DB.

The worker (GLM) ported and committed the three ranges and wrote the test;
it hit its 250-turn cap before committing the test, so the driver
formatted, rebuilt and reran it, and wrote this file. Each arm was read
against the annotate.py listing.

## Callers

The arms are jumptable targets of the dispatcher itself (0x44DC10).

## Callees

- 282 OverrideAreaDifficulty (0x44E042-0x44E12E, 33a91fb5) -- GetString1
  0x41E1B0 compared with strcmp 0x7E7A54 against the "" at 0x8C5CF8 (the
  port's IsEmpty), m_specificID [ebx+0x52C] non-zero, clamped to -1..1.
  0x452F80 (inlined in the port) walks CInfGame+0x4BDC (field_4BDC) and
  matches m_sArea through CompareStrings 0x7E8182, the CRT _stricmp (the
  locale-handle check at [0xA1AC5C] then the fast path) -- hence
  CompareNoCase. A miss allocates 0x18 bytes, runs the ctor 0x452F50
  (452F50.md) and AddTails it. Then flags = (flags & ~1) | 2 and the level
  byte at +8. CString_Destruct, `or esi,-1`, jmp 0x4526F7.
- 283 SetCriticalPathObject (0x44E12E-0x44E223, 33a91fb5) --
  GetTargetShareType TYPE_SPRITE (miss: 0x44E1DB). m_specificID picks set
  or clear of 0x2000 in base stats m_flags ([ebp+0x5AC]). The message is
  sent when cNetwork.m_nServiceProvider (+0x1C, 0 = SERV_PROV_NULL) is 0
  or m_idLocalPlayer (+0x6FA) equals the target's m_remotePlayerID
  ([ebp+0x62]); CMessageSpriteUpdate 0x50C260 with the target id twice,
  AddMessage 0x4F7500 at 0x44E1D6, into 0x44E1DB.
- 233 HideCreature (0x452035-0x4520ED, df348f57) -- low byte of
  GetSpecifics, GetTargetShareType TYPE_SPRITE (miss: 0x4526F7 with the
  entry ACTION_ERROR), bShare, sub_75F3D0 0x75F3D0 whose return is esi;
  outside SERV_PROV_NULL (0x85E65C) CMessage90 0x453600(id, id, bHide)
  and AddMessage 0x4F7500; release at 0x44E1DE.
- 325 SetAreaFlags (0x45262C-0x4526AA, df348f57) -- GetSpecifics2 = 1/2/3
  selects and/or/xor of GetSpecifics into the area header m_flags
  (GetArea 0x40CB00, GetHeader 0x68BFF0, +0xC); each write jumps to
  0x4526F7 without touching esi (ACTION_ERROR), and any other selector
  sets esi = -2 at 0x4526C7 -- ACTION_ERROR either way.
- 278 DropInventoryEXExclude (0x4512E9-0x451362, fe8e8e20) -- sub_45C290
  0x45C290 (miss: 0x451451), bShare, virtual GetObjectType: TYPE_SPRITE
  (0x84C507) calls DropInventory 0x754EE0(0, GetString1()) into esi;
  TYPE_CONTAINER (0x84C502) calls 0x481D30 (381 bytes, not ported -- TODO
  in the arm, which keeps ACTION_ERROR where the exe stores its result)
  -- ported since as CGameContainer::DropInventoryEXExclude and wired in
  35eb6fc3, its return now the action return;
  any other type takes 0x451451. Release at 0x44E1DE.
- 235 PlaySequence (0x450B52-0x450BD9, fe8e8e20) -- sub_45C290 (miss:
  0x44E1DB), GetSpecifics bounds-checked against the zero-extended bytes
  at 0x85BBB0 (0) and 0x85BBBB (17 = CGAMESPRITE_SEQ_SLEEP), read from
  iwd2.exe; out of range takes 0x44E1DB before bShare. In range: bShare,
  CMessageSetSequence 0x453530(sequence, this->m_id, target id), jmp
  0x4514DE.

## Notes

- The test pumps the display loop (the area load waits for
  m_bDisplayStale), as the 83k AR3000 case does.
- Open follow-up: port 0x481D30 (the container's excluding drop) and
  return its result from 278.
