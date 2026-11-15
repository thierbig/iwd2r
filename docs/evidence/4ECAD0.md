---
address: 0x4ECAD0
name: CtrlAltDelete_SetGlobal
exemption: "no oracle case: the only caller is the unported console dispatcher 0x5838A0's SETGLOBAL arm, and driving it needs a live game with game and area variable hashes. The scope dispatch, both hash paths and the hit/miss stores are pinned in-tree against the listing below."
parity_waiver: "asm lints 2026-11-15 T85b: CString copy-ctor 0x7FC98F is the five staged by-value argument temps this listing builds (the FindKey name on each path at 0x4ECB30/0x4ECBD6, the GetArea scope at 0x4ECBA9, the SetName name on each path at 0x4ECB73/0x4ECC0B), CString dtor 0x7FCC1A destructs the two by-value parameters, MakeUpper 0x7FD0F0 upper-cases both parameters, and strcmp 0x7E7A54 is the scope == 'GLOBAL' compare; compiler/library traffic, no game call waived."
kind: read-only
ghidra_version: 12.1.4
---

# 0x4ECAD0 CtrlAltDelete_SetGlobal

The SETGLOBAL handler of the debug console's "CtrlAltDelete:" cheat-command
layer, free function `void CtrlAltDelete_SetGlobal(CString sName, CString
sScope, DWORD dwValue)` — the two CStrings by value on the stack, the callee
destructing them (`ret 0xC` at 0x4ECC5F, the layer's callee-pops spelling as
in 0x4EC870's `ret 0x10`), 402 bytes (0x4ECAD0-0x4ECC62). Ported in commit
(see report).

## Naming

No string names this function directly; the name comes from the dispatch
chain, the same scheme as CtrlAltDelete_CreateItem (0x4EC870). The console
stores its command words obfuscated — each byte as `2*ch + 0x10` — and the
literal at 0x8AF6C8 decodes to "SETGLOBAL". The dispatcher's resolver
(0x583E60) matches it (strcmp at 0x58403D) and stores id 0x1D = 29 at
0x584049; the dispatcher computes id-12 = 17 into the index table at
0x583E34 (byte 0x0B at 0x583E45), and the state jump table at 0x583DF4
(`jmp [ecx*4+0x583df4]` at 0x583A6A) slot 11 lands on the arm at 0x583D22,
which calls here from 0x583D79. The CREATEITEM link of the same two tables
was verified numerically this run (slot 6 → 0x583B81, matching the 0x4EC870
evidence). Usage: `SETGLOBAL <name> <scope> <value>`.

## Body

SEH prologue, then MakeUpper 0x7FD0F0 on the scope argument (0x4ECAEF)
followed by the name argument (0x4ECAFC), and a local CVariable constructed
at the frame's 0x0C slot (ctor 0x452BD0 at 0x4ECB09; the listing constructs
it eagerly, the port constructs it in the miss branch — same default
construction, and CVariable has no destructor, so the epilogue never touches
it).

Scope pick: strcmp 0x7E7A54 of the scope against "GLOBAL" (literal
0x8A7FBC, compare at 0x4ECB12-0x4ECB22).

- **GLOBAL** (equal): a staged by-value copy of the name (0x7FC98F at
  0x4ECB30), `GetObjectGame()` (0x40CB10 at 0x4ECB40, chitin global
  0x8CF6DC +0x1C54), `CInfGame::GetVariables()` (0x4286A0 at 0x4ECB47),
  then `FindKey` virtual slot 4 (0x4ECB54) with the name temp. Hit: dwValue
  (arg3, re-read at 0x4ECB5B) stored into found+0x28 (`m_intValue`, NOT the
  DWORD field at +0x24) and straight to the epilogue (0x4ECB62). Miss: a
  second staged name copy (0x4ECB73), `SetName` 0x452C60 on the local
  CVariable (0x4ECB7C), dwValue into local+0x28 (0x4ECB81-0x4ECB85, the
  `[esp+0x34]` store), then AddKey: the listing inlines the receiver as
  game+0x47FC (`lea ecx,[eax+0x47fc]` at 0x4ECB95) — the m_variables member
  GetVariables returned — and calls virtual slot 0 at 0x4ECC2E with `&local`
  (`AddKey(CVariable&)`, the reference pushed at 0x4ECC2D).

- **any other scope** (0x4ECBA0): a staged by-value copy of the scope
  (0x4ECBA9), `GetObjectGame()`, then `CInfGame::GetArea()` (0x5B7510 at
  0x4ECBBF, the CString-by-value overload). NULL area jumps straight to the
  epilogue (0x4ECBC8) — a silent no-op, unlike the handlers above, which
  print the "Error File" text. Otherwise `GetVariables()` on the area
  (0x453090 at 0x4ECBE2) and the same hit/miss shape: hit stores dwValue
  into found+0x28 (0x4ECBF6-0x4ECBFA); miss stages the name (0x4ECC0B),
  SetName 0x452C60 (0x4ECC14), dwValue into local+0x28 (0x4ECC19-0x4ECC1D),
  AddKey inlined as `lea ecx,[esi+0xace]` (0x4ECC21) — area+0xACE, the
  area's m_variables member — virtual slot 0 at 0x4ECC2E.

Epilogue: CString dtor 0x7FCC1A on the scope (0x4ECC39) then the name
(0x4ECC4A), `ret 0xC` (0x4ECC5F). The port reuses one GetVariables()
pointer per path where the listing re-derives game+0x47FC / area+0xACE for
the AddKey receiver — the same object in both spellings.

## ecx / calling convention

ecx (the dispatcher's this) is never read — unlike 0x4EC870 the function
does not even save it. Free function.

## Callers

Not in tree: the console dispatcher 0x5838A0's SETGLOBAL arm 0x583D22. The
arm requires exactly three parsed arguments (`call 0x584420` argc, `cmp
eax,0x3 / jne` to the usage-error state), fetches tokens 1 and 2 via
0x65F8F0 (this = dispatcher+0x24) into staged by-value CString temps
(0x7FC98F), leaves the DWORD value (from the dispatcher's [esi+0x14]
table) deepest on the stack, and calls here at 0x583D79 with the arm's EH
state byte set just before. The dispatcher is deliberately unported
(docs/followups.md).

## Test

None: setting a variable needs a live CInfGame (GLOBAL scope) or a loaded
area (any other scope) with variable hashes; the console layer has no test
harness (no dispatcher). The pinned facts above — the MakeUpper order, the
"GLOBAL" literal and compare, both GetVariables paths with their inlined
AddKey receivers (+0x47FC / +0xACE), the silent-miss area semantics, and
the m_intValue (+0x28) stores on both hit and miss — are verified against
the listing.
