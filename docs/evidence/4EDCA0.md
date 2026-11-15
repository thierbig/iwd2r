---
address: 0x4EDCA0
name: CtrlAltDelete_AddSpellClass
exemption: "no oracle case: the only caller is the ADDSPELL console handler 0x4ECC70 (ported this task), reached from the debug console's CtrlAltDelete:ADDSPELL line, which needs a live party, the game's m_spellsByClass master lists and object-array deny round trips — no capture reaches it. The Catch2 suite pins the wiring one level up (tests/test_cluaconsole.cpp [clua] drives Execute on the dispatcher)."
kind: read-only
ghidra_version: 12.1.4
---

# 0x4EDCA0 CtrlAltDelete_AddSpellClass

Free function in the original's CtrlAltDelete.cpp, `__stdcall`-shaped
callee-cleaned `void AddSpellClass(const BYTE& nClass, DWORD dwCount)`
(`ret 8`), 692 bytes (0x4EDCA0-0x4EDF53). ecx (the console's this,
forwarded by the dispatcher chain) is not read: it is overwritten at
entry and the game comes from the 0x8CF6DC global. Ported in commit
90c8374f1.

## Body

`nClass` arrives as a byte reference (the callers stage a local byte and
push its address); `dwCount` is by value. Resolves the class's grouped
spell list — `&game->m_spellsByClass[game->GetSpellcasterIndex(nClass)]`
(0x5CA5D0, index scaled by the 0x4C CGroupedSpellList stride) — then
walks the party through `m_group.m_memberList` (head node at
game+0x388E; the guard's `add 0x3884` test at 0x4EDCE5 is a dead
null-game artifact) and, per member, GetDeny/ReleaseDeny round trip as
in CAIGroup::GroupAction (0x404D00), retrying SHARED and DENIED;
rc != SUCCESS advances without release (0x4EDF3E).

Per member: `for (nLevel = 0; nLevel < m_nHighestLevel; nLevel++)`
(0x4EDF08 `inc/cmp/jb`, exclusive) and `for (nIndex = 0; nIndex <
m_lists[nLevel].m_nCount; nIndex++)` (0x4EDD9D `jae` exit, exclusive),
with the exe's inlined checked accesses (UTIL_ASSERT 0x780C00 at
0x8A7DCC/0x8AA868/0x8AA844 — CSpellList.cpp lines 772/724/725 and the
m_spells lines 652/653) read as direct member access. Each entry's id
(`m_pList[nIndex].m_nID`, stride 8) indexes the game's master
`m_spells.m_pList` (CResRef, stride 8) for
`AddKnownSpell(nClass, nLevel, m_spells.m_pList[nID], dwCount, dwCount, 0)`
(0x724930 at 0x4EDE8E; a4/a5 both dwCount, a6 0 — all by reference).

Inside the inner loop, immediately after AddKnownSpell (0x4EDE93):
`if (nClass == 2 || nClass == 10)` — BARD and SORCERER, whose casters
draw from a per-level count rather than a book per spell — tops up the
sprite's count with three separate GetSpellsAtLevel calls (0x724790 at
0x4EDEA8/0x4EDEBD/0x4EDEE2), each with the current (nClass, nLevel):
read `field_14`, store `field_14 + dwCount`, then
`sub_725D30(dwCount, FALSE)` (0x725D30; a1/a2 by reference, FALSE a
staged byte). The port spells the three calls out.

## Callers

In tree: CtrlAltDelete_AddSpell 0x4ECC70's "ALL" arm (all 7 spellcaster
indices) and each named-class arm (BARD 2, CLERIC 3, DRUID 4, PALADIN 7,
RANGER 8, SORCERER 10, WIZARD 11) — 8 call sites in the exe, all in the
function ported alongside this one.

## Test

Not driven directly: the effect (known-spell books and per-level cast
counts on party sprites) needs a live world with the game's
m_spellsByClass lists filled and the object array populated, which the
Catch2 suite does not build. The dispatcher arm that reaches it is
covered by tests/test_cluaconsole.cpp [clua].
