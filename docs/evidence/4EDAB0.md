---
address: 0x4EDAB0
name: CtrlAltDelete_SetChapter
kind: read-only
ghidra_version: 12.1.4
---

# 0x4EDAB0 CtrlAltDelete_SetChapter

A debug console "CtrlAltDelete:" cheat-command handler, free function
taking one DWORD (`ret 0x4`), 492 bytes (0x4EDAB0-0x4EDC9B). Named for the
SETCHAPTER console word: the dispatcher's command-word resolver (0x583E60)
maps the word to id 0x25 and dispatch arm 12 calls here with -1 when no
argument was parsed and the parsed value otherwise, handing the handler
this+0x38. Ported in commit (see report).

Premise correction (listing-verified): the task brief said 0x4EDAB0 calls
0x4EDCA0 (692 B) — it does not. The callees of this body are 0x40AE80 ×3,
0x7FCD57 ×2, 0x5BFC40 ×2, 0x599A50, 0x599E70, 0x7FCC1A ×5 and the virtual
slot [eax+0x74]; 0x4EDCA0 is simply the next function after the padding
(0x4EDC9C-0x4EDC9F) that follows this one, and is not ported.

## Body

Sets the chapter by delivering an action to the protagonist.

1. A CAIAction is default-constructed at [esp+8] by a fully inlined
   0x403A40: three CAIObjectType constructions (0x40AE80, the same
   all-zero-with-nInstance--1 shape as in HANS 0x4ED2C0) at
   action+0x02/+0x3E/+0x7A, the empty-string pointer from 0x8C1758 into
   m_string1/m_string2 (0x4EDB24, 0x4EDB2B), NO_ACTION from 0x847784 into
   m_actionID (0x4EDB4C), m_specificID/m_specificID2/m_specificID3 zeroed
   (0x4EDB51-0x4EDB5F), 0x7FCD57 string assignments from the global empty
   CString 0x8C5CF8 (0x4EDB66, 0x4EDB77), m_internalFlags zeroed
   (0x4EDB7C). m_dest is left uninitialized, exactly as in the out-of-line
   default constructor; the port spells the constructor and inherits that.
2. The action is re-aimed: m_specificID = the incoming argument read from
   [esp+0xF0] (0x4EDB83, 0x4EDB96), m_actionID = the WORD at 0x8478B6 = 161
   (0x4EDB8A, 0x4EDBA5), and m_specificID2 = 1 (0x4EDBB0). 161 is
   INCREMENTCHAPTER — CGameAIBase's action switch routes it to
   IncrementChapter(), which resets m_specificID to -1 when
   m_specificID2 is zero, so the forced 1 preserves the chapter argument.
   The constant is new in CAIAction as INCREMENTCHAPTER (0x8478B6).
3. Inlined GetObjectGame (global 0x8CF6DC + 0x1C54, 0x4EDBBB) gives the
   game. GetProtagonist (0x5BFC40, 0x4EDBD0) is called with ecx = game and
   no arguments; its result, THREAD_ASYNCH (byte at 0x851873), &pObject
   ([esp+4], staged at 0x4EDBAC) and INFINITE (pushed 0x4EDBAA) go to
   CGameObjectArray::GetShare (0x599A50) on game+0x372C (0x4EDBDC).
4. Only on SUCCESS (cmp against the byte at 0x85186C, 0x4EDBE1):
   pObject's vtable slot 0x74 — CGameAIBase::AddAction — is called with
   &cAction (0x4EDBE9-0x4EDBF4). The port spells the static_cast to
   CGameSprite and the named AddAction, as in AddItem (0x4ED500).
5. The release re-reads the game global and re-runs GetProtagonist for the
   id instead of keeping either (0x4EDBF7-0x4EDC1B, ReleaseShare
   0x599E70); the port re-chains both calls.
6. Epilogue: the action's five unwind dtors run in reverse member order
   (m_string2, m_string1, m_acteeID2, m_acteeID, m_actorID; 0x7FCC1A at
   0x4EDC20-0x4EDC7F, trylevels 9, 8, 7, 6, -1), then `ret 0x4`.

## ecx / calling convention

ecx (the dispatcher's this, handed as this+0x38 by arm 12) is never read.
One DWORD argument of its own; free function, `ret 0x4`.

## Callers

In tree: CLUAConsole::Execute (src/CLUAConsole.cpp) case 12, the
SETCHAPTER arm, twice — with -1 when CountArgs() is zero and with
field_10.GetAt(0) when it is one.

## Test

tests/test_cluaconsole.cpp drives the arm end to end on zeroed
CGameObjectArray/CInfGame/CScreenWorld/CBaldurChitin buffers (added with
this port's test commit): Execute of "ctrlaltdelete:setchapter(3)" and
"ctrlaltdelete:setchapter()" both reach the handler and return with
field_0 still 0x25 — on the zeroed object array GetShare's THREAD_ASYNCH
bounds check returns BAD_INDEX, so the share branch and ReleaseShare are
skipped without a crash.
