# Survey 163 — area thirteen: effects, messages and game-object helpers (0x4C0000-0x5A0000)

Survey for thierbig/iwd2r#163: 349 functions / 59,088 bytes. Read-only analysis at `53063343`
(includes #160-#162); no source, evidence, MAP or recovery-status change was made for this survey.

## Headline

| disposition | rows |
|---|---:|
| **PORT** (single-player-reachable, missing in the tree) | **12** |
| COVERED (already ported, compiler-generated, or a template/container instance the tree provides) | 302 |
| DROP (multiplayer-only under ADR 0002, or dead) | 34 |
| UNSURE (0x5819B0, caller not in `functions.tsv`) | 1 |
| **total** | **349** |

The issue's 349 "unported functions" contains 175 rows under 64 bytes and several clusters that
collapse on inspection:

- **~40 rows are already ported** (case-insensitive marker search; each part found its share) — the
  enumeration is stale in the same way area twelve's was.
- **The five message-codec classes the issue flags (CMessageContinueDialog,
  CMessageInsertResponse, CMessageLoadDialog, CMessageGroup [Ghidra exports it as "CMessage105"],
  CMessage117) are MP-only** — every one is *created* in single-player code paths, but the codecs
  are reached only with a session: in single player `CMessageHandler::Broadcast/Send` queue the
  message and `AsynchronousUpdate` calls `Run()` directly, never marshalling (see part B's table).
- **The repeating 300/332-byte bodies are not collection templates** — they are the
  compiler-generated complete destructors of the `CProjectile` subclass family (identical member
  layout, hence identical sizes); their 30-byte twins are the scalar-deleting destructors. All
  those classes are in the tree.
- **The real collection-template block is part A's 0x4C45A0-0x4C5E80** (`std::set<void*>`,
  `std::set<std::string>`, `std::set<int>`, `std::bitset<256>/<32>`, `CResHelper`) — already covered
  by the host STL and `compat/`.
- **The 16 part-D "CInfButtonArray helpers" are MSVC `std::deque<int>` internals**, not game code:
  `src/CInfButtonArray.h:89-93` documents the original's `std::deque<INT>` replaced by `CList`, and
  the bodies contain only `operator_new`/`operator_delete` and deque iterator/block arithmetic.
- **The five part-D projectile/effect rows also collapse**: two are compiler-generated destructors of
  modelled classes (vptr store + base-dtor tail call), two are `std::set` NIL-bound lookup
  instantiations the tree's own NOTES already cover, and one is a `_Tree` erase/insert-under-lock
  internal. Part D therefore has **no port work** — the only open item there is 0x5819B0.

So the area's real work is **12 rows in three clusters**, listed under "What actually needs porting".

## SP-neutrality of the removed lobby sync points (issue's first item)

Delivered separately as a comment on #163: the 14 removed `m_pEngineMultiPlayer` sync points are
SP-neutral (verdict no SP-visible work lost; the only SP path through MP-class code is the
create-char case-3 cast artifact, already reproduced with `StartSinglePlayer`). One naming
correction from it, carried into `docs/survey-159.md`: `0x64DDB0` is
`CScreenMultiPlayer::StartMultiPlayer`, not `UpdateMainPanel`.

## What actually needs porting (12 rows, ~17 KB)

| cluster | rows | bytes | where |
|---|---|---:|---|
| **A** `CDerivedStats` arrays + `ProcessEffectList` helper | 0x4E7000, 0x4E7050, 0x4E7070, 0x4E7720 | 185 | `src/CDerivedStats.*`, `src/CGameSprite.cpp` |
| **B** `CProjectileExploding` family | 0x52D430 `AreaEffect`, 0x52E4D0 / 0x52EC80 / 0x52F9E0 `AIUpdate`s | 3,999 | `src/CProjectileBAM.*` |
| **C** effects/helpers | 0x55F710 Summon `ApplyEffect`, 0x564580 Dominate `OnRemove`, 0x5705D0 `CMessageScreenShake` send, 0x533600 spray-cell spawner | 1,374 | `src/IcewindCGameEffects.*`, `src/CMessage.cpp`, `src/CProjectileBAM.cpp` |

Part D's five candidate rows verified as compiler/library output and the only open item there is
**0x5819B0** (see judgement calls).

The highest-value row is **0x55F710**: it is SP-reachable (opcode `CGAMEEFFECT_SUMMON` = 67 decoded
at `src/CGameEffect.cpp:407`, applied through `ProcessEffectList`) and it is the missing vtable slot
2 of `IcewindCGameEffectSummon`. Landing it (with `UsesDice` 0x49FC40 and the class's `/* 00NN */`
annotations) lets `vtable_audit.py` anchor the class GREEN instead of skipping it, and retires the
stale import-parked entries. Its history — imported at `b2619ee5` with exe-verified body and
evidence, parked at `e9d3e4af`, sibling 0x560140 restored without it in #135 — is written up in
part C.

## Method

`functions.tsv` / `callgraph.tsv` for names, sizes and callers; capstone disassembly through
`tools/parity/annotate.py` with the harness `IWD2R_HOME`, plus a raw `E8/E9` rel32 call-site scan of
`.text` where the linear annotate path stops early; `.rdata` dword/vtable scans for class
attribution (the exe has no RTTI); the tree's `src/` markers and `compat/` templates for
COVERED/PORT; `docs/adr/0002-single-player-only.md` for the multiplayer rule; the existing
`docs/evidence/` analyses as a second witness. Will's commit (7820ef22) is not fetchable from this
checkout, so names come from the Ghidra exports, decompiles and tree history — not from Will.

# Survey #163 — PART A (0x4C1AE0–0x4F6EF0, 87 rows, 9,398 bytes)

Read-only survey of area thirteen. Sources: `functions.tsv`/`callgraph.tsv`, `parity/annotate.py` listings, a raw `E8` rel32 call-site scan over the full `.text` (linear capstone disassembly stops early, so the raw scan is the reliable caller source), `.rdata` vtable scans, `notes/survey-120.md` for the template rows it already dispositioned, and `src/`/`compat/` for what the tree already provides. Will's tree was not available locally; names come from the Ghidra exports and the tree.

## Counts

| disposition | rows | bytes |
|---|---|---|
| COVERED — template instance / compiler-generated / compat inline | 68 | 7,715 |
| DROP — already ported (inline copies), dead, dispatcher-only, or MP-only mirror | 15 | 1,498 |
| PORT — SP-reachable and not in tree | 4 | 185 |
| **total** | **87** | **9,398** |

83/87 rows need no code. The only real work in this slice is four helpers of the `CDerivedStats`/`ProcessEffectList` tail (two already named in the tree's own `TODO: Incomplete` comments).

## Table (every address dispositioned)

### COVERED — repeating collection-template instances (36 rows)

| family (template that covers it) | addresses (size) | disposition / reason |
|---|---|---|
| VC6 `std::set<void*>` = `std::_Tree<void*>` (`_Nil` 0x8D48B4; tree member `CGameEffectBase::field_58`, `src/CGameEffect.h:2106`) | 0x4C45A0 (230), 0x4C4690 (572), 0x4C48D0 (204), 0x4C49A0 (235), 0x4C4A90 (138), 0x4C4B20 (67), 0x4C5010 (1017), 0x4C5410 (135), 0x4C54A0 (59), 0x4C54E0 (611), 0x4C5750 (25), 0x4C5770 (27) | COVERED — `std::set<void*>` insert/erase/copy/rotate/node-alloc internals; host libstdc++ provides them (callers: the three `CPersistantEffect*::ApplyEffect/Copy` and `CProjectileArea`) |
| VC6 `std::set<std::string>` = `_Tree<string>` (nodes 0x20; comparator 0x4C5E80) | 0x4C4C90 (607), 0x4C4EF0 (176), 0x4C5870 (619), 0x4C5B30 (82), 0x4C5B90 (86), 0x4C5BF0 (81), 0x4C5C50 (82), 0x4C5CB0 (86), 0x4C5D30 (81), 0x4C5D10 (27), 0x4C5E80 (80) | COVERED — `std::set<std::string>` internals (`m_resourceImmunities`, `src/CDerivedStats.h:617`); 0x4C5E80 is `std::string::compare` |
| VC6 `std::set<int>` = `_Tree<int>` (`_Nil` 0x8D13F0) | 0x4C4FA0 (102), 0x4C5AE0 (60, export `UnknownStatsStruct_Get`) | COVERED — `std::set<int>` lower_bound/find (`m_naturalImmunities`); 0x4C5AE0 is a find, not a "stats getter" |
| VC6 `std::bitset<256>` | 0x4C4B90 (96), 0x4C4BF0 (52) | COVERED — `set(pos,val)` and `test(pos)` (`CDerivedStats::m_spellStates`, `src/CDerivedStats.h:562`) |
| VC6 `std::bitset<32>` | 0x4C4C30 (93) | COVERED — `set(pos,val)` (`m_visualEffects`, `src/CDerivedStats.h:563`) |
| VC6 `std::bitset<N>` throw helper | 0x4C5790 (217, export `invalid_bitset`) | COVERED — `_Throw_out_of_range`; STL runtime |
| VC6 STL pair/iterator helper | 0x4C4B70 (22) | COVERED — used 28× by the ported `CDerivedStats::Reload` around its `_Tree<int>::insert` calls; host `std::set` spells it directly |
| MFC `CList/CPtrList` iteration (`compat/mfc_list.h`, `CList::GetNext`) | 0x4C5B20 (15, export `CircularLinkedListNode_GetPreviousAndSelf`) | COVERED — `POSITION` advance/read helper; compat template provides it (callers: `CPersistantEffect*::ApplyEffect`, 0x516B70, `IcewindMisc::GetSpellResrefEffects`) |
| `CResHelper<T,nType>::SetResRef` (`src/CRes.h:79-110`) | 0x4C5D90 (228, T,1016), 0x4EBF40 (225, T,4 sound), 0x4EC030 (228, CResItem,1005) | COVERED — template duplicate; tree instantiates the same `CResHelper` |
| MFC geometry inline (`compat/mfc_geometry.h`) | 0x4D35D0 (21, `CPoint_Negate`), 0x4D35F0 (32, `CRect_Construct`) | COVERED — `CPoint::operator-()` and `CRect(l,t,r,b)` are already inline in compat; both rows are only called from the ported `CUIPanel::Render` (0x4D3173/0x4D319C) and the compiler emits them inline |

### COVERED — compiler-generated (32 rows)

All are `scalar deleting destructor` wrappers (30 bytes each): `call <real dtor>; test byte[esp+8],1; jz; push this; call operator_delete; ret 4`. The real dtor of every one is already ported and marker-carrying, so the tree's compiler emits the wrapper. Owning class from each dtor's tree marker:

| addresses (size 30) | owning class |
|---|---|
| 0x4C1AE0, 0x4C2140, 0x4C2B90, 0x4C3160 | `CPersistantEffect84C4B4/84C4C4/84C4D4/84C484` (dtors 0x4C1B00/0x4C2160/0x4C2BB0/0x4C3180, marked in `src/CGameEffect.cpp`) |
| 0x4C8FF0, 0x4C9970, 0x4CA7B0, 0x4CB490, 0x4CC7B0, 0x4CD130 | `CGameSound`, `CGameSpawning`, `CGameStatic`, `CGameText`, `CGameTiledObject`, `CGameTrigger` (dtors marked in their own files) |
| 0x4D2570, 0x4D4B60, 0x4D5A20, 0x4D5F80, 0x4D6BF0, 0x4D9720, 0x4E18B0, 0x4E1D80, 0x4E4280, 0x4E4BE0, 0x4E60A0, 0x4E6460 | `CUIControlBase`, `CUIControlButton`, `CUIControlButton3State`, `CUIControlSlider`, `CUIControlEdit`, `CUIControlEditMultiLine`, `CUIControlEditMultiLineScroller`, `CUIControlTextDisplay`, `CUIControlLabel`, `CUIControlScrollBar`, `CUIControlButtonScrollBar`, `CUIControlButtonPlusMinus` (all dtors marked) |
| 0x4CAEA0 | unresolved class; real dtor is the 1-byte empty 0x78E730; compiler wrapper only (see unclassified) |
| 0x4D4730 | unresolved class; real dtor is 0x653D70 = `jmp CObList_Destruct` (trivial teardown thunk); compiler wrapper only (see unclassified) |
| 0x4E78E0, 0x4F5610, 0x4F5B20, 0x4F6040, 0x4F6150, 0x4F6370, 0x4F6A80 | `CItem`, `CMessageEnterStoreMode`, `CMessageLoadDialog`, `CMessageSetLastAttacker`, `CMessageSetPath`, `CMessageSpriteUpdate`, `CMessageProjectileTrailingVFX` (all dtors marked, except the VFX member dtor below) |

Plus one compiler-generated member teardown:

| address | size | export | identity | disposition / reason |
|---|---|---|---|---|
| 0x4F6AA0 | 67 | FUN_004f6aa0 | `CMessageProjectileTrailingVFX::~CMessageProjectileTrailingVFX` — `CString` member teardown | COVERED — compiler-generated member dtor; class declared in tree (`src/CMessage.h:2406`), matching survey-121 row 64's drop/compiler |

### DROP — 15 rows (no port work)

| address | size | export | real identity | SP/MP/dead + evidence | reason |
|---|---|---|---|---|---|
| 0x4C26A0 | 334 | CGameEffectBase_Construct | `CGameEffectBase::CGameEffectBase()` | already in tree | DROP — already ported (`src/CGameEffect.cpp:214`, `// NOTE: Inlined.`), per survey-120 |
| 0x4D3D90 | 75 | FUN_004d3d90 | helper that deletes a panel list (`CUIPanel::~CUIPanel`+delete loop) then `CObList_RemoveAll` | dead: 0 direct call sites in `.text`, 0 pointer references in `.rdata/.data` | DROP — unreferenced in the exe; nothing to port |
| 0x4E7120 | 37 | arg_present_on_list | `CImmunitiesProjectile::OnList(DWORD)` — walks the `CTypedPtrList<CPtrList,DWORD*>` comparing the node value | SP-reachable (called by ported `CProjectile::DeliverEffects` and effect paths) | DROP — already ported as the inlined form (`src/CImmunities.cpp:470`, `// NOTE: Inlined.`) |
| 0x4F4F30 | 21 | FUN_004f4f30 | zero-init of a 5-dword local struct | MP-only: both call sites in `CMessageHandler::AsynchronousUpdate`'s removed network dispatch (0x4F23B2/0x4F2454) | DROP — dispatcher-only local init; no SP caller |
| 0x4F5130 | 157 | FUN_004f5130 | `CMessageAddItem::CMessageAddItem(const CItem&,...)` (msg 2) | receiver copy: both call sites in `CMessageHandler::AsynchronousUpdate`; comm type SEND | DROP — already implemented inline in tree (`src/CMessage.cpp:12198`); out-of-line copy is dispatcher-only |
| 0x4F53E0 | 32 | FUN_004f53e0 | `CMessageDoorOpen::CMessageDoorOpen(BOOLEAN,...)` (msg 18) | dispatcher-only; comm type SEND | DROP — already inline in tree (`src/CMessage.cpp:13611`). Tree note "no standalone constructor address exists" is half-right: it does exist, but only the network dispatcher calls it |
| 0x4F54F0 | 32 | FUN_004f54f0 | `CMessageEnterDialogMode` second ctor (msg 22) | dispatcher-only; BROADCAST_OTHERS vtable | DROP — class ctor already inline in tree (`src/CMessage.cpp:28429`) |
| 0x4F59D0 | 133 | FUN_004f59d0 | `CMessageInsertResponse::CMessageInsertResponse` (msg 28) | dispatcher-only | DROP — already inline in tree (`src/CMessage.cpp:28730`) |
| 0x4F5A90 | 116 | FUN_004f5a90 | `CMessageLoadDialog::CMessageLoadDialog` (msg 30) | dispatcher-only | DROP — already inline in tree (`src/CMessage.cpp:28654`) |
| 0x4F5C70 | 34 | FUN_004f5c70 | `CMessageRemoveItem::CMessageRemoveItem` (msg 34) | dispatcher-only | DROP — already inline in tree (`src/CMessage.cpp:29532`) |
| 0x4F69B0 | 182 | FUN_004f69b0 | `CMessageProjectileTrailingVFX::CMessageProjectileTrailingVFX` (msg 86) | MP-only mirror: vtable 0x84D328 comm type BROADCAST_OTHERS; sender copies inlined in `CProjectileBAM::Fire`/`CProjectileSingleTarget::Fire`; out-of-line copy called only by the dispatcher | DROP — MP-only receive construction; class body absent in tree and only needed for the omitted broadcaster |
| 0x4F6A70 | 6 | CMessageProjectileTrailingVFX::GetMsgSubType | subtype accessor returning 86 | MP-only (broadcast-to-others mirror) | DROP — accessor needed only by the removed dispatcher |
| 0x4F6B20 | 39 | FUN_004f6b20 | `CMessageAddSummonLink::CMessageAddSummonLink` (msg 88) | dispatcher-only; SP send site inlines the ctor (`IcewindCGameEffectSummon::SpawnFromResRef`) | DROP — already inline in tree (`src/CMessage.cpp:25687`; network overrides dropped per #121) |
| 0x4F6E50 | 135 | FUN_004f6e50 | `CMessageStoreMarkup` ctor (msg 108) | dispatcher-only | DROP — already ported in tree (`src/CMessage.cpp:26765`) but unmarked — known #121 G12c row-65 marker/evidence gap |
| 0x4F6EF0 | 165 | FUN_004f6ef0 | `CMessage117` ctor (msg 117) | MP-only mirror: vtable 0x84D45C comm type BROADCAST_FORCED_OTHERS; constructed inline only in `IcewindCGameEffectFireshield::ApplyEffect` (0x568E9D) and `WhenStruck::ApplyEffect` (0x56A6B6) for other players; no tree class, no SP consumer | DROP — MP-only mirror class; nothing for single player to carry |

### PORT — 4 rows (SP-reachable, missing in tree)

| address | size | export | real identity | SP/MP/dead + evidence | disposition / reason |
|---|---|---|---|---|---|
| 0x4E7000 | 35 | FUN_004e7000 | weighted sum of nine 4-byte values at `this+4` (weights 1,2,4,…) | SP: two call sites in `CGameSprite::ProcessEffectList` (0x72FA5D/0x72FAB2), inside the region the tree leaves as `TODO: Incomplete (0x72E759-0x72FB24)` | PORT — helper of the unported `ProcessEffectList` tail (AI actions/messages); no tree equivalent |
| 0x4E7050 | 19 | FUN_004e7050 | zeroes ten 8-byte entries (array clear) | SP: `CDerivedStats::CDerivedStats`, `Reload`, `BonusInit` (0x443FDB/0x443FE6/0x4446C9/0x446191) | PORT — the ten-entry arrays at +0x328/+0x378 are named as missing in the tree's TODO (`src/CDerivedStats.cpp:17-19`) |
| 0x4E7070 | 40 | FUN_004e7070 | copies ten 8-byte entries (array copy) | SP: `CDerivedStats::operator=` (0x4459FA/0x445A0C) | PORT — same TODO (`src/CDerivedStats.cpp:549-550`) |
| 0x4E7720 | 91 | FUN_004e7720 | copies a `CPtrList` of 8-byte entries (alloc + `CPtrList_AddTail` per node) | SP: `CDerivedStats::operator=` four times (0x445CE8–0x445D1E) | PORT — the tree TODO names this helper (`src/CDerivedStats.cpp:567`): four lists at +0x4E4..+0x538 are not copied |

## Template-family map

- VC6 `std::set<void*>` (`_Tree<void*>`, `_Nil` 0x8D48B4): 0x4C45A0, 0x4C4690, 0x4C48D0, 0x4C49A0, 0x4C4A90, 0x4C4B20, 0x4C5010, 0x4C5410, 0x4C54A0, 0x4C54E0, 0x4C5750, 0x4C5770 — tree member `CGameEffectBase::field_58` (`src/CGameEffect.h:2106`).
- VC6 `std::set<std::string>` (`_Tree<string>`) + `std::string`: 0x4C4C90, 0x4C4EF0, 0x4C5870, 0x4C5B30, 0x4C5B90, 0x4C5BF0, 0x4C5C50, 0x4C5CB0, 0x4C5D30, 0x4C5D10, 0x4C5E80 — `m_resourceImmunities`.
- VC6 `std::set<int>` (`_Tree<int>`, `_Nil` 0x8D13F0): 0x4C4FA0, 0x4C5AE0 — `m_naturalImmunities`.
- VC6 `std::bitset<256>`: 0x4C4B90, 0x4C4BF0; `std::bitset<32>`: 0x4C4C30; `std::bitset<N>` throw helper: 0x4C5790.
- VC6 STL pair/iterator helper: 0x4C4B70.
- MFC `CList/CPtrList` (`compat/mfc_list.h`): 0x4C5B20.
- `CResHelper<T,nType>` (`src/CRes.h`): 0x4C5D90, 0x4EBF40, 0x4EC030.
- MFC geometry inline (`compat/mfc_geometry.h`): 0x4D35D0, 0x4D35F0.
- Compiler-emitted scalar-deleting destructors: all 30-byte rows (31), plus 1 compiler member dtor (0x4F6AA0).

## Unclassified rows (dispositioned, flagged)

1. 0x4CAEA0 (30) — scalar-deleting-destructor shape; real dtor is the 1-byte empty 0x78E730 (Ghidra names it `CBaldurEngine::OnRestButtonClick`, a name/identity mismatch). Owning class not pinned from `.rdata` slot searches. No port work either way (compiler wrapper) → COVERED, class attribution open.
2. 0x4D4730 (30) — same shape; real dtor 0x653D70 = `jmp CObList_Destruct` (trivial CObList teardown thunk). Owning class unresolved → COVERED (compiler wrapper), caveat.
3. 0x4C4B70 (22) — exact VC6 STL helper name unresolved (a 5-byte `{iterator,bool}` / pair temporary built 28× around `_Tree<int>::insert` in `CDerivedStats::Reload`); template unambiguous → COVERED STL, name approximate.
4. 0x4E7000 (35) — role in the unported `ProcessEffectList` tail not fully identified (weighted/bitfield checksum over nine values at CGameSprite+0xB00); PORT stands, exact semantics need the tail body.

## Discrepancies and cross-part notes

1. Tree notes vs exe (several in `src/CMessage.cpp`): the "constructor is inlined in the exe / no standalone constructor address exists" notes for `CMessageAddItem` (12198), `CMessageDoorOpen` (13611), `CMessageInsertResponse` (28730), `CMessageLoadDialog` (28654), `CMessageRemoveItem` (29532), `CMessageEnterDialogMode` (28429), `CMessageAddSummonLink` (25687) and `CMessageProjectileTrailingVFX` are half right: the standalone out-of-line ctor copies do exist at 0x4F5130/0x4F53E0/0x4F59D0/0x4F5A90/0x4F5C70/0x4F54F0/0x4F6B20/0x4F69B0 — but their only callers are the removed `CMessageHandler::AsynchronousUpdate` dispatcher, so the disposition is unchanged (DROP). Comment-correction item only.
2. Already-ported rows in the issue's "unported" list: 0x4C26A0 (`CGameEffectBase` ctor) and 0x4E7120 (`CImmunitiesProjectile::OnList`) are already in tree as inlined forms; 0x4F6E50 (`CMessageStoreMarkup` ctor) is present but unmarked (known #121 G12c gap). All three are DROP-already-ported.
3. Message codecs in the issue's bullet list (`CMessageInsertResponse`, `CMessageLoadDialog`, …): the ctor rows in this slice are dispatcher-only; their Marshal/Unmarshal/Run rows live outside part A and their comm types are SEND/BROADCAST_OTHERS, i.e. receive/broadcast-side. Parts B–D will disposition the codec bodies; from part A there is no PORT justification for any message row.
4. No git mutation: main checkout left clean at ca6b24c2 (the box was reported at 53063343; the two commits in between — #161/#162 — touch only `CGameSpriteLastUpdate::Initialize` and `SendLeaveAreaLuaPanic`, nothing in this range; no fetch/checkout performed).

# #163 survey — part B: 0x4F6FA0–0x532880 (87 rows, 21,220 bytes)

Read-only survey of area thirteen, part B, worked against main `53063343` (files read
via `git archive` into `/tmp`; the checkout was not mutated, nothing committed).
Method: `functions.tsv`/`callgraph.tsv`, Ghidra (annotate listings + oracle decompiles
where the function existed), vtable slot/pointer scans of `.rdata` for class attribution
(slot offsets 0x00/0x04/0x08/0x0C/0x10/0x14/0x18 of the CMessage vtables; slot 0 of the
scalar-deleting destructors), the constructor vtable-store sites of the `imm32` stores in
`.text`, and the tree's `docs/wso/import-parked/*.json` for the parked names.

**Counts:** PORT 4 (3,999 B) · DROP 18 (8,780 B) · COVERED 65 (8,441 B).
COVERED = 61 compiler-generated destructor rows of classes already in the tree
(message classes' scalar-deleting/complete destructors, and the 25 `CProjectile`
subclass dtor pairs) + 4 template/compiler rows.

Part B contains **no** `CPersistantEffect*_AutoDestructor` rows (those are part A,
0x4C1AE0–0x4C3160) and **no** collection-template rows (see the template section).

The SP/MP line for message codecs: in a single-player game the provider is
`SERV_PROV_NULL`; `CMessageHandler::Send`/`Broadcast` queue the message and
`CMessageHandler::AsynchronousUpdate` calls `Run()` directly — no `MarshalMessage`/
`UnmarshalMessage` (tree `src/CMessage.cpp:10308-10315` and `10492-10530`; the network
send/receive blocks are the omitted single-player-only code). Consequently every codec is
MP-only even when the message itself is *created* in an SP code path.

## Dispositions (address order)

| address | size | export name | real class::method | reach | disposition | reason |
|---|---|---|---|---|---|---|
| 0x4F6FA0 | 6 | `CMessage117::GetMsgSubType` | CMessage117::GetMsgSubType | MP (class sent to other players only) | **DROP** | only needed by the MP codec/dispatch; senders omit creation in SP (tree NOTE in IcewindCGameEffects.cpp:4062) |
| 0x4F70C0 | 67 | `FUN_004f70c0` | CMessageProjectileImmumityUpdate::~CMessageProjectileImmumityUpdate | compiler-generated | **COVERED** | complete dtor of the in-tree class: destroys its CImmunitiesProjectile member; tree gets it from the member's dtor |
| 0x4FAA20 | 30 | `FUN_004faa20` | CMessageColorUpdate_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CMessageColorUpdate, which is in tree; the compiler emits it |
| 0x4FB490 | 30 | `FUN_004fb490` | CMessageContainerItems_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CMessageContainerItems, which is in tree; the compiler emits it |
| 0x4FD930 | 1022 | `CMessageContinueDialog::MarshalMessage` | CMessageContinueDialog::MarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from CMessageHandler's omitted network send block; SP queues the message and runs it locally (CMessage.cpp:10308-10315) |
| 0x4FDD30 | 588 | `CMessageContinueDialog::UnmarshalMessage` | CMessageContinueDialog::UnmarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from CMessageHandler::AsynchronousUpdate's omitted receive block; SP never unmarshals |
| 0x5017E0 | 2036 | `CMessageInsertResponse::MarshalMessage` | CMessageInsertResponse::MarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted network send block; its SP sender is CGameAIBase::StartCutScene 0x462F90 / CGameDialogReply::Pick 0x485750, which queue and Run locally |
| 0x501FE0 | 1411 | `CMessageInsertResponse::UnmarshalMessage` | CMessageInsertResponse::UnmarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted receive block |
| 0x502B40 | 541 | `CMessageLoadDialog::MarshalMessage` | CMessageLoadDialog::MarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted network send block; SP sender CScreenWorld::StartDialog 0x68EA00 queues and Runs locally |
| 0x502D60 | 325 | `CMessageLoadDialog::UnmarshalMessage` | CMessageLoadDialog::UnmarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted receive block |
| 0x50B600 | 30 | `CMessageSpriteEquipment_AutoDestructor` | CMessageSpriteEquipment_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CMessageSpriteEquipment, which is in tree; the compiler emits it |
| 0x5124D0 | 203 | `FUN_005124d0` | CMessage88::UnmarshalMessage (AddSummonLink network face) | MP (network receive) | **DROP** | the AddSummonLink network overrides are already omitted by the in-tree note (src/CMessage.h) under ADR 0002 |
| 0x5125A0 | 655 | `FUN_005125a0` | CMessage88::Run (AddSummonLink network face) | MP (broadcast to others) | **DROP** | same in-tree note: ctor only in single player |
| 0x514810 | 30 | `FUN_00514810` | CMessageWeaponImmumityUpdate_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CMessageWeaponImmumityUpdate, which is in tree; the compiler emits it |
| 0x514F60 | 129 | `FUN_00514f60` | CMessageProjectileImmumityUpdate::CMessageProjectileImmumityUpdate | MP (broadcast to others) | **DROP** | class GetCommType is BROADCAST_OTHERS; the SP creation site is omitted as behaviour-neutral (message is created and dropped in SP) |
| 0x515410 | 30 | `FUN_00515410` | CMessageGroup_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CMessageGroup, which is in tree; the compiler emits it |
| 0x5154E0 | 397 | `CMessage105::MarshalMessage` | CMessageGroup::MarshalMessage (export CMessage105) | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted network send block; the class (vtable 0x84D564) and its Run are in tree |
| 0x515670 | 383 | `CMessage105::UnmarshalMessage` | CMessageGroup::UnmarshalMessage (export CMessage105) | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted receive block |
| 0x516910 | 354 | `CMessage117::MarshalMessage` | CMessage117::MarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted network send block; its senders IcewindCGameEffectFireshield/WhenStruck::ApplyEffect are SP but the creation is deliberately omitted in tree (single player only, vtable 0x84D45C) |
| 0x516A80 | 227 | `CMessage117::UnmarshalMessage` | CMessage117::UnmarshalMessage | MP (only the network send/receive blocks) | **DROP** | codec reached only from the omitted receive block |
| 0x516B70 | 349 | `FUN_00516b70` | CMessage117::Run | MP (remote clients only) | **DROP** | comm type is BROADCAST_FORCED_OTHERS (folded GetCommType 0x4536E0), so Run only runs on remote clients |
| 0x516CD0 | 99 | `FUN_00516cd0` | CMessageSpellLevelImmumityUpdate::CMessageSpellLevelImmumityUpdate | MP (broadcast to others) | **DROP** | same: only remote players apply it; SP creation omitted as behaviour-neutral |
| 0x516D40 | 6 | `CMessageProjectileImmumityUpdate::GetMsgSubType` | CMessageProjectileImmumityUpdate/CMessageSpellLevelImmumityUpdate::GetMsgSubType | compiler-generated | **COVERED** | one folded 6-byte body for both classes; the tree port already carries it on CMessageProjectileImmumityUpdate::GetMsgSubType with a NOTE naming 0x516D40 |
| 0x516FC0 | 13 | `FUN_00516fc0` | CMessageHandler receive helper (CPtrList::AddTail copy) | MP (receive dispatch) | **DROP** | only caller is the omitted MP receive block of CMessageHandler::AsynchronousUpdate; body is an inlined MFC CPtrList wrapper |
| 0x516FD0 | 42 | `FUN_00516fd0` | CMessageHandler receive helper (list-node construction) | MP (receive dispatch) | **DROP** | only caller is the omitted MP receive block; body constructs a 0xC-byte list node |
| 0x517000 | 17 | `SetP1ToP2` | std::_Tree internal helper (SetP1ToP2; called only by MSVC6 tree internals) | template/compiler | **COVERED** | MSVC6 template instantiation; the tree's libstdc++/compat template emits the equivalent |
| 0x51C310 | 30 | `FUN_0051c310` | CGameObject84D624_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CGameObject84D624, which is in tree; the compiler emits it |
| 0x51D480 | 25 | `FUN_0051d480` | inlined MFC CPtrList construction inside CGameObject84D624::CGameObject84D624 | template/compiler | **COVERED** | MSVC6 template instantiation; the tree's libstdc++/compat template emits the equivalent |
| 0x51DE20 | 18 | `FUN_0051de20` | CPoint-style two-long constructor copy (compiler-generated, no game body) | template/compiler | **COVERED** | MSVC6 template instantiation; the tree's libstdc++/compat template emits the equivalent |
| 0x51EA70 | 30 | `FUN_0051ea70` | CProjectile_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile, which is in tree; the compiler emits it |
| 0x5297E0 | 30 | `FUN_005297e0` | CProjectileSparkleExplosion_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileSparkleExplosion, in tree; emitted by the compiler |
| 0x529800 | 332 | `FUN_00529800` | CProjectileSparkleExplosion::~CProjectileSparkleExplosion | compiler-generated | **COVERED** | complete dtor of CProjectileSparkleExplosion, in tree; member/base destruction is compiler-generated |
| 0x529990 | 30 | `FUN_00529990` | CProjectile84D838_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84D838, in tree; emitted by the compiler |
| 0x5299B0 | 218 | `FUN_005299b0` | CProjectile84D838::~CProjectile84D838 | compiler-generated | **COVERED** | complete dtor of CProjectile84D838, in tree; member/base destruction is compiler-generated |
| 0x529AB0 | 30 | `FUN_00529ab0` | CProjectile84DA4C_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84DA4C, in tree; emitted by the compiler |
| 0x529AD0 | 281 | `FUN_00529ad0` | CProjectile84DA4C::~CProjectile84DA4C | compiler-generated | **COVERED** | complete dtor of CProjectile84DA4C, in tree; member/base destruction is compiler-generated |
| 0x529BF0 | 229 | `FUN_00529bf0` | MSVC6 STL container destructor (local _Tree/_List; reached only from unwind tables) | template/compiler | **COVERED** | MSVC6 template instantiation; the tree's libstdc++/compat template emits the equivalent |
| 0x529E30 | 30 | `FUN_00529e30` | CProjectile84DAD4_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84DAD4, in tree; emitted by the compiler |
| 0x529E50 | 183 | `FUN_00529e50` | CProjectile84DAD4::~CProjectile84DAD4 | compiler-generated | **COVERED** | complete dtor of CProjectile84DAD4, in tree; member/base destruction is compiler-generated |
| 0x52B010 | 30 | `FUN_0052b010` | CProjectileBAM (CProjectileTravelling in the fork)_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileBAM (CProjectileTravelling in the fork), which is in tree; the compiler emits it |
| 0x52CBA0 | 30 | `FUN_0052cba0` | CProjectileSparkle_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileSparkle, in tree; emitted by the compiler |
| 0x52CBC0 | 281 | `FUN_0052cbc0` | CProjectileSparkle::~CProjectileSparkle | compiler-generated | **COVERED** | complete dtor of CProjectileSparkle, in tree; member/base destruction is compiler-generated |
| 0x52CE10 | 30 | `FUN_0052ce10` | CProjectileExploding_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileExploding, in tree; emitted by the compiler |
| 0x52CE30 | 332 | `FUN_0052ce30` | CProjectileExploding::~CProjectileExploding | compiler-generated | **COVERED** | complete dtor of CProjectileExploding, in tree; member/base destruction is compiler-generated |
| 0x52D430 | 953 | `FUN_0052d430` | CProjectileExploding::AreaEffect | SP (projectile explosion runs in SP) | **PORT** | parked behind the CProjectileTravelling naming only (docs/wso/import-parked/52D430.json); our classes exist (CProjectileExploding), so port the SP explosion path |
| 0x52E360 | 30 | `FUN_0052e360` | CProjectileExplodingWeapon_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileExplodingWeapon, in tree; emitted by the compiler |
| 0x52E380 | 332 | `FUN_0052e380` | CProjectileExplodingWeapon::~CProjectileExplodingWeapon | compiler-generated | **COVERED** | complete dtor of CProjectileExplodingWeapon, in tree; member/base destruction is compiler-generated |
| 0x52E4D0 | 1131 | `FUN_0052e4d0` | CProjectileExplodingWeapon::AIUpdate | SP (projectile AI update in SP) | **PORT** | parked only on the import's fork naming; our CProjectileExplodingWeapon exists (src/CProjectileBAM.h) |
| 0x52EB10 | 30 | `FUN_0052eb10` | CProjectileExplodingFlame_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileExplodingFlame, in tree; emitted by the compiler |
| 0x52EB30 | 332 | `FUN_0052eb30` | CProjectileExplodingFlame::~CProjectileExplodingFlame | compiler-generated | **COVERED** | complete dtor of CProjectileExplodingFlame, in tree; member/base destruction is compiler-generated |
| 0x52EC80 | 1339 | `FUN_0052ec80` | CProjectileExplodingFlame::AIUpdate | SP | **PORT** | same: our CProjectileExplodingFlame exists; AIUpdate runs every tick in SP |
| 0x52F5E0 | 30 | `FUN_0052f5e0` | CProjectileSkullTrap_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileSkullTrap, in tree; emitted by the compiler |
| 0x52F600 | 351 | `FUN_0052f600` | CProjectileSkullTrap::~CProjectileSkullTrap | compiler-generated | **COVERED** | complete dtor of CProjectileSkullTrap, in tree; member/base destruction is compiler-generated |
| 0x52F9E0 | 576 | `FUN_0052f9e0` | CProjectileSkullTrap::AIUpdate | SP | **PORT** | same: our CProjectileSkullTrap exists; AIUpdate runs in SP |
| 0x52FD50 | 30 | `FUN_0052fd50` | CProjectile84DE0C_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84DE0C, in tree; emitted by the compiler |
| 0x52FD70 | 281 | `FUN_0052fd70` | CProjectile84DE0C::~CProjectile84DE0C | compiler-generated | **COVERED** | complete dtor of CProjectile84DE0C, in tree; member/base destruction is compiler-generated |
| 0x52FED0 | 30 | `FUN_0052fed0` | CProjectile84DE94_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84DE94, in tree; emitted by the compiler |
| 0x52FEF0 | 281 | `FUN_0052fef0` | CProjectile84DE94::~CProjectile84DE94 | compiler-generated | **COVERED** | complete dtor of CProjectile84DE94, in tree; member/base destruction is compiler-generated |
| 0x5301C0 | 30 | `FUN_005301c0` | CProjectile84DF1C_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84DF1C, in tree; emitted by the compiler |
| 0x5301E0 | 281 | `FUN_005301e0` | CProjectile84DF1C::~CProjectile84DF1C | compiler-generated | **COVERED** | complete dtor of CProjectile84DF1C, in tree; member/base destruction is compiler-generated |
| 0x5303D0 | 30 | `FUN_005303d0` | CProjectile84DFA4_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84DFA4, in tree; emitted by the compiler |
| 0x5303F0 | 281 | `FUN_005303f0` | CProjectile84DFA4::~CProjectile84DFA4 | compiler-generated | **COVERED** | complete dtor of CProjectile84DFA4, in tree; member/base destruction is compiler-generated |
| 0x5308E0 | 30 | `FUN_005308e0` | CProjectile84D8B8_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84D8B8, in tree; emitted by the compiler |
| 0x530900 | 183 | `FUN_00530900` | CProjectile84D8B8::~CProjectile84D8B8 | compiler-generated | **COVERED** | complete dtor of CProjectile84D8B8, in tree; member/base destruction is compiler-generated |
| 0x530B40 | 30 | `FUN_00530b40` | CProjectile84E030_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84E030, which is in tree; the compiler emits it |
| 0x531210 | 30 | `FUN_00531210` | CProjectile84E0C4_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectile84E0C4, in tree; emitted by the compiler |
| 0x531230 | 300 | `FUN_00531230` | CProjectile84E0C4::~CProjectile84E0C4 | compiler-generated | **COVERED** | complete dtor of CProjectile84E0C4, in tree; member/base destruction is compiler-generated |
| 0x531430 | 30 | `FUN_00531430` | CProjectileBolt_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileBolt, in tree; emitted by the compiler |
| 0x531450 | 281 | `FUN_00531450` | CProjectileBolt::~CProjectileBolt | compiler-generated | **COVERED** | complete dtor of CProjectileBolt, in tree; member/base destruction is compiler-generated |
| 0x531650 | 30 | `FUN_00531650` | CProjectileBoulder_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileBoulder, in tree; emitted by the compiler |
| 0x531670 | 281 | `FUN_00531670` | CProjectileBoulder::~CProjectileBoulder | compiler-generated | **COVERED** | complete dtor of CProjectileBoulder, in tree; member/base destruction is compiler-generated |
| 0x531860 | 30 | `FUN_00531860` | CProjectileDart_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileDart, in tree; emitted by the compiler |
| 0x531880 | 281 | `FUN_00531880` | CProjectileDart::~CProjectileDart | compiler-generated | **COVERED** | complete dtor of CProjectileDart, in tree; member/base destruction is compiler-generated |
| 0x531AA0 | 30 | `FUN_00531aa0` | CProjectileSlingStone_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileSlingStone, in tree; emitted by the compiler |
| 0x531AC0 | 281 | `FUN_00531ac0` | CProjectileSlingStone::~CProjectileSlingStone | compiler-generated | **COVERED** | complete dtor of CProjectileSlingStone, in tree; member/base destruction is compiler-generated |
| 0x531CB0 | 30 | `FUN_00531cb0` | CProjectileAxe_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileAxe, in tree; emitted by the compiler |
| 0x531CD0 | 281 | `FUN_00531cd0` | CProjectileAxe::~CProjectileAxe | compiler-generated | **COVERED** | complete dtor of CProjectileAxe, in tree; member/base destruction is compiler-generated |
| 0x531EC0 | 30 | `FUN_00531ec0` | CProjectileSpear_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileSpear, in tree; emitted by the compiler |
| 0x531EE0 | 281 | `FUN_00531ee0` | CProjectileSpear::~CProjectileSpear | compiler-generated | **COVERED** | complete dtor of CProjectileSpear, in tree; member/base destruction is compiler-generated |
| 0x532120 | 30 | `FUN_00532120` | CProjectileDagger_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileDagger, in tree; emitted by the compiler |
| 0x532140 | 281 | `FUN_00532140` | CProjectileDagger::~CProjectileDagger | compiler-generated | **COVERED** | complete dtor of CProjectileDagger, in tree; member/base destruction is compiler-generated |
| 0x532360 | 30 | `FUN_00532360` | CProjectileAcidBlob_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileAcidBlob, in tree; emitted by the compiler |
| 0x532380 | 281 | `FUN_00532380` | CProjectileAcidBlob::~CProjectileAcidBlob | compiler-generated | **COVERED** | complete dtor of CProjectileAcidBlob, in tree; member/base destruction is compiler-generated |
| 0x5325E0 | 30 | `FUN_005325e0` | CProjectileArrow_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileArrow, in tree; emitted by the compiler |
| 0x532600 | 281 | `FUN_00532600` | CProjectileArrow::~CProjectileArrow | compiler-generated | **COVERED** | complete dtor of CProjectileArrow, in tree; member/base destruction is compiler-generated |
| 0x532860 | 30 | `FUN_00532860` | CProjectileSPFLMARR_AutoDestructor | compiler-generated | **COVERED** | scalar-deleting dtor of CProjectileSPFLMARR, in tree; emitted by the compiler |
| 0x532880 | 281 | `FUN_00532880` | CProjectileSPFLMARR::~CProjectileSPFLMARR | compiler-generated | **COVERED** | complete dtor of CProjectileSPFLMARR, in tree; member/base destruction is compiler-generated |
## Message-codec SP/MP table

| class (vtable) | Marshal | Unmarshal | SP sender(s) / receiver evidence | comm type | codec reach |
|---|---|---|---|---|---|
| CMessageContinueDialog (0x84905C) | 0x4FD930 (1022) | 0x4FDD30 (588) | creators CGameDialogSprite::AsynchronousUpdate 0x483F00, FUN_004F5400; subtype folded 0x4844C0 | folded GetCommType 0x40A0D0 | MP only |
| CMessageInsertResponse (0x848BB0) | 0x5017E0 (2036) | 0x501FE0 (1411) | creators CGameAIBase::StartCutScene 0x462F90, CGameDialogReply::Pick 0x485750, FUN_004F59D0; subtype 0x463280 | folded 0x40A0D0 | MP only |
| CMessageLoadDialog (0x84D1D8) | 0x502B40 (541) | 0x502D60 (325) | creators CScreenWorld::StartDialog 0x68EA00, CMessagePickedDialogResponse::Run 0x4FDF80, FUN_004F5A90; subtype 0x4F5B10 | folded 0x43E170 | MP only |
| **CMessageGroup** (0x84D564; Ghidra exports its codecs as "CMessage105") | 0x5154E0 (397) | 0x515670 (383) | creator CMessageGroup::CMessageGroup 0x5152C0; getter 0x515400, Run 0x5157F0 — class and Run in tree | folded 0x40A0D0 | MP only |
| CMessage117 (0x84D45C) | 0x516910 (354) | 0x516A80 (227) | creators IcewindCGameEffectFireshield::ApplyEffect 0x568D00 and …WhenStruck::ApplyEffect 0x56A5A0 (SP, creation omitted in tree with NOTE); Run 0x516B70, getter 0x4F6FA0 | 0x4536E0 = BROADCAST_FORCED_OTHERS | MP only (Run remote-clients only) |

All ten codec rows are **DROP (MP-only)**. Reverse call graphs show no direct callers of
any Marshal/Unmarshal (they are vtable-dispatched only), and the vtable store sites give
the creators above: each message is created by SP code, but its codec runs only inside the
omitted network blocks.

## Template / repeated-body corrections

- **The repeated 300/332/281-byte bodies at 0x52CE30–0x52EB30 (and the rest of the
  0x5297E0–0x532880 block) are NOT collection templates.** They are the
  compiler-generated complete destructors of the `CProjectile` subclass family
  (`CProjectileExploding`, `…ExplodingWeapon`, `…ExplodingFlame`, `…SkullTrap`,
  `…Sparkle`, `…Bolt`, `…Boulder`, `CProjectileDart`, `CProjectileSpear`, …), all of
  which already exist in the tree (`src/CProjectileBAM.h`, `src/CProjectile.h`, …). The
  repeating sizes come from the identical `CProjectile` member layout, not from a
  template; the 30-byte twins are their scalar-deleting destructors. Each pair is
  COVERED (class in tree → the compiler emits both).
- The real MSVC6 template block in area thirteen is **0x4C45A0–0x4C5E80 (part A, not
  B)**: `std::set<void*>` for CPersistantEffect84C4B4/84C4D4/84C484 `field_58`
  (insert 0x4C45A0/0x4C4690 — in-tree NOTEs at `src/CGameEffect.cpp:18828`/`19084` —
  `_Tree` dtor/copy/assign 0x4C48D0/0x4C49A0/0x4C4A90, clear/erase 0x4C5010, node
  copy/free 0x4C5410/0x4C54A0), `std::set<CString>` for the immunity resrefs
  (comparator 0x4C5E80; insert/find 0x4C4C90/0x4C4EF0/0x4C5870),
  `std::bitset<256>::set/test` (0x4C4B90/0x4C4BF0), `std::bitset<32>::set` (0x4C4C30),
  and `CResHelper<CGameEffect>::SetResRef` (0x4C5D90). The 0x55B5E0/0x570780 blocks are
  outside part B.
- In part B the only template/compiler rows are 0x517000 (MSVC6 `_Tree` helper),
  0x51D480 (MFC `CPtrList` construction), 0x51DE20 (CPoint-style two-long ctor copy) and
  0x529BF0 (STL local-container dtor, reached only from unwind tables).

## Unclassified rows / judgement calls (for the maintainer)

- **0x516FC0, 0x516FD0** — bodies are inlined MFC `CPtrList` wrappers (AddTail / node
  construction) whose only callers are the omitted MP receive dispatch; the owning class
  is not recoverable from the binary. Booked **DROP (MP-only)**; identity unclassified.
- **0x514F60, 0x516CD0** (CMessageProjectileImmumityUpdate / CMessageSpellLevelImmumityUpdate
  constructors) — in the original they execute in SP, but the message is
  BROADCAST_OTHERS and is created-then-dropped, so the tree omits the creation sites as
  behaviour-neutral. Booked **DROP as MP-only** under the usual rule; if "SP-reachable"
  is read as "executes in SP" they would be PORT, though no in-tree caller exists.
- **0x51DE20** (CPoint-style two-long ctor copy) and **0x529BF0** (STL container dtor) —
  attributed as compiler/template **COVERED** from body shape and unwind-only callers;
  identities are heuristic.
- 0x4C4B70 / 0x4C4FA0 / 0x4C5750 / 0x4C5B30 / 0x4C5B90 / 0x4C5BF0 / 0x4C5C50 /
  0x4C5CB0 / 0x4C5D30 (part A, listed for the template map only): exact `_Tree` member
  inferred from callers, not decompiled individually.

## Survey corrections carried forward

1. Ghidra's "CMessage105" export names are the **CMessageGroup** codecs (class vtable
   0x84D564, getter/Run named CMessageGroup) — one class, not two.
2. 0x516D40 is a single folded `GetMsgSubType` body shared by the two immunity-message
   vtables (0x84D548, 0x84D580); the tree already carries it on
   CMessageProjectileImmumityUpdate::GetMsgSubType with a NOTE naming 0x516D40.
3. **No CPersistantEffect\*_AutoDestructor rows are in part B** — the 30-byte rows
   0x4C1AE0/0x4C2140/0x4C2B90/0x4C3160 are in part A, as are the collection-template
   instantiations; the maintainer's instruction on them applies there, not here.

### PORT rows (readiness)

The four PORT rows are the parked `CProjectileExploding` family SP methods (`AreaEffect`
0x52D430 + the three `AIUpdate`s). The import parked them only because the fork names the
class `CProjectileTravelling`; our tree already has `CProjectileExploding`,
`CProjectileExplodingWeapon`, `CProjectileExplodingFlame`, `CProjectileSkullTrap`
(`src/CProjectileBAM.h`), so they can be ported one class per PR with the existing
vtable-slot/address comments as parity anchors.

# #163 survey — part C: `0x532DC0–0x575B20` (87 rows, 14,926 B)

Read-only against `origin/main` `53063343`. Method: all 87 rows annotated with `tools/parity/annotate.py`; callers from `callgraph.tsv` plus a raw `E8/E9` rel32 scan of `.text`; vtable attribution from the dword references in `.rdata` (no RTTI in the exe, so classes were pinned through vtable slot tables + src headers + existing evidence docs); tree coverage from `git grep 0xADDR` / src class models; the existing `docs/evidence/*` analysis for neighbouring ported functions was used as a second witness. No commits, no git mutation.

## Counts

| disposition | rows | bytes | breakdown |
|---|---:|---:|---|
| **PORT** | 4 | 1,374 | real SP code |
| **COVERED** | 82 | 13,547 | already ported/inlined 13 (868 B) · implicit/compiler dtor artifacts of modelled classes 14 (1,679 B) · STL/MFC/template/compiler instances 54 (10,893 B) · CRT/global dtor 1 (107 B) |
| **DROP** | 1 | 5 | dead unreferenced thunk |
| **still open / unclassified** | 0 | 0 | every row dispositioned; identity-only gaps listed below |
| **total** | **87** | **14,926** | |

No MP-only row exists in this range (the projectile/effect/container/screen-shake code is all reachable in single player); the only dead row is `0x557FB0`.

"inherited (SP)" in the table means: the row is a compiler/template/dtor artifact of a class or container that single-player code uses (the evidence is in the reason column — a ported caller, a modelled class, or a template the tree already instantiates); it carries no standalone reachability of its own.

## 0x55F710 `IcewindCGameEffectSummon::ApplyEffect` — the write-up

**Identity.** 511 B, `vtable 0x84F0A8` **slot 2 (+0x08)**. Ghidra/WSO export name `CGameEffect_opcode_411_2`; real name from the WSO i071 import (`b2619ee5`) and `docs/evidence/560140.md`. The class vtable is 12 slots: `+0x00` dtor `0x55F6D0`, `+0x04` `Copy` 0x55F620, **`+0x08` ApplyEffect 0x55F710**, `+0x0C` `ResolveEffect` 0x4A3030, `+0x10/+0x14` folded `OnPortraitLDblClick`, `+0x18` `CheckSave`, `+0x1C` `UsesDice` 0x49FC40, `+0x20` `DisplayString`, `+0x24` `OnRemove`, `+0x28` `ResolveSourceSprite` 0x560140, `+0x2C` `SpawnFromResRef` 0x55FAD0. Slot 2 is confirmed by the `.rdata` dword at `0x84F0B0`.

**SP/MP/dead — SP.** The effect class is created by `CGameEffect::DecodeEffect` case **`CGAMEEFFECT_SUMMON` = 67** (`src/CGameEffect.h:80`, `src/CGameEffect.cpp:407`), i.e. the normal spell/monster summon effect; its `ApplyEffect` runs wherever effects are applied, including single player through `CGameSprite::ProcessEffectList` (`src/CGameSprite.cpp:16887`) / the `CGameEffect` apply dispatch (`src/CGameEffect.cpp:1241`). Nothing in its body or its only caller chain is session-gated. It is not dead and not MP-only.

**Body (verified against the annotate listing, 0x55F710–0x55F90E).**
1. `ResolveSourceSprite()` first — `mov eax,[esi]; call [eax+0x28]` at 0x55F72A-0x55F72D — then, only if the resolution left it NULL, caches the passed caster into `m_pSprite` (+0x18C).
2. Summon count: `m_effectAmount` (+0x18) plus `m_numDice` rolls of `rand() % m_diceSize + 1` (zero dice size rolls plain 1).
3. Per summon: `Icewind586B70::Instance()->sub_586CA0(caster)` registry-limit check; on failure `FeedBack(FEEDBACK_TOOMANYSUMMONED, 0, 0, 0, 50, 0, 0, -1, 0, 0)` and break (the switch is fallen into, not returned).
4. `CString resRef = m_res.GetResRefStr()` (0x78AA30), then the virtual `SpawnFromResRef(resRef, &m_target)` — `call [vtable+0x2C]` at 0x55F7B5.
5. Rider: for a non-null spawn and `m_durationType` in {0, 3, 0x1000} (with a redundant `!= 1` re-test the exe also performs), builds `IcewindMisc::CreateEffectSkillUnsummon(spawn, (m_duration - gameTime)/15, 0)` via the inline `mul 0x88888889 / shr 3`, copies `m_casterLevel` into it, and `AddEffect(..., EFFECT_LIST_TIMED, TRUE, TRUE)` through `[spawn+0x78]`.
6. `AddSummonVisualHit(spawn, m_dwFlags != 0 ? 0x15 : 0)`.
7. Closing group VFX by a switch on `m_dwFlags`: 1→0x120, 2→0x121, 3→0x122, 4→0x123, 5→0x124, 6→0x168, 0/7→0, default `UTIL_ASSERT(FALSE)`.
8. `m_done = TRUE; return TRUE;`.

**What our class needs from it.** `src/IcewindCGameEffects.h` declares only `Copy`, `ResolveSourceSprite` (+0x28) and `SpawnFromResRef` (+0x2C); slot 2 therefore still inherits `CGameEffect::ApplyEffect`. Adding the `ApplyEffect(CGameSprite*)` override in the right declaration order, plus the `/* 00NN */` slot annotations for slot 0 (0x55F6D0), slot 2 (this body) and slot 7 (0x49FC40, the 6-byte `return TRUE`), lets `tools/parity/vtable_audit.py` anchor the class on the annotations and report **GREEN** instead of `skip`. That is exactly task-7; this row is the missing slot-2 body.

**Import-vs-port bookkeeping.**
- `b2619ee5` (2026-11-02) — **import**: `wso i071: import IcewindCGameEffectSummon::ApplyEffect (0x55F710)`. The verified 511-byte body (fork body checked against the exe, five readings corrected; the leading `ResolveSourceSprite` virtual restored) was committed to `src/IcewindCGameEffects.cpp` **with `docs/evidence/55F710.md`** (the blob still exists at that commit); the commit carries `Imported-From: WillScarlettOhara/iwd2-re@7820ef224ac4d9527e1844ce0984478f63e5a4cd (MIT per author, 2026-10-17)`.
- `ff512b02` (same day) — frida "unmeasured WSO import" marking for 9 i071 rows.
- `e9d3e4af` (2026-11-02) — **parked** the pair: removed the body and `55F710.md`, created `docs/wso/import-parked/55F710.json` and `560140.json`. Recorded reason: the frida scenario set (world_entry, first_fight, walk_and_talk, action_bar, equip_shots, inventory, level_up, world_map) does not exercise the summon apply path.
- `7a10d571` (#135, 2026-11-24) — restored **only** `0x560140 ResolveSourceSprite`, not `0x55F710`; `560140.json` remains in the tree (stale, and it still describes the dependency on the parked 55F710); `docs/evidence/55F710.md` is absent at main but recoverable from `b2619ee5`; `560140.md` itself lists 0x55F710 as missing.
- **Recommendation:** PORT (SP-reachable). Cheapest correct route: restore the already-exe-verified i071 body from `b2619ee5`, re-add its evidence (rewritten as read-only with the slot table and the SP reachability above), add the slot-2 pointer-to-member test, and annotate the class (task-7). A fresh port of the same body is the alternative if a measured/`trace` evidence trail is wanted first; the body itself is session-independent, only its behaviour needs a live sprite/area for an oracle-grade run. Either way this closes the Summon vtable's last logic slot and retires the stale `560140.json`.

## Template map (repeated bodies named)

| template / machinery | addresses in part C | evidence anchor |
|---|---|---|
| `std::set<LONG>` → VC6 `_Tree` (heap locks, `_Nil`) | 0x537010 (range ctor of a temp set), 0x5370C0 (`_Tree` dtor) | used by ported projectile/effect dstors; `src/CProjectile.cpp:565-566` |
| `std::map` / `_Tree` instantiation **A** (nil `0x8E28C0`) | 0x55B520, 0x55B590, 0x55B5E0 (insert), 0x55B830 (erase), 0x55BBC0 (copy/ctor), 0x55C040 (1017 B clear), 0x55C440, 0x55C4D0, 0x55C780, 0x55C7A0, 0x55CB30, 0x55CCD0; 0x55A7A0 (229 B clear/dtor) | `docs/evidence/55A890.md`, `55B1B0.md` (VC6 std::map/std::list lowering) |
| `std::map` / `_Tree` instantiation **B** (nil `0x8E28C8`) | 0x55B900, 0x55B970, 0x55BA60, 0x55BAF0, 0x55BE00, 0x55C800, 0x55CA70, 0x55CAD0, 0x55CB50, 0x55CBB0, 0x55CC70; **`std::set<DWORD>` `m_struckTargets`**: 0x55C510 (insert), 0x55CC10 (find) | `docs/evidence/57BE80.md`; `55B1B0.md` |
| `std::map<LONG,LONG>` `m_targetHits` family (nil `0x8E3E38`/`0x8D48B4`) | 0x56F090 (229 B clear), 0x570620, 0x570690 (copy/assign), 0x570780 (1017 B clear), 0x570B80, 0x570C50, 0x570CC0, 0x570D10, 0x570D50 (ctor), 0x570DE0, 0x570E00, 0x571070, 0x5710D0 | `docs/evidence/56FED0.md` (map erase helpers 0x5F9AA0/0x570B80); `56EDD0.md` |
| `std::list<T>` machinery | 0x5371C0 (`CircularLinkedList_InsertNewNodeAfter`), 0x55BB40 (delete range), 0x55BB90 (`NewNode`), 0x56BE50 (`28`-byte payload insert) | `52A1A0.md`, `5779A0.md`, `575090.md`; tree uses `std::list<LONG>` |
| MFC `CPtrList`/`CPlex`/`CString` | 0x5371A0 (CGameEffectList ctor, `CPtrList_Construct` + vtable 0x84EA40), 0x537220 (`StringContainer_Set`), 0x547780/0x5477A0 (CPlex collection dtor), 0x547800 (`CPlex_Create`), 0x547880 (memset) | `cluster-unassigned-a.md` (b) |
| VC6 `std::string` | 0x56BDD0 (assign/grow), 0x56F180 (refcount free / global-object dtor, unwind-only callers) | string-assign waivers in `55/56/57` evidence |
| compiler helpers/stubs | 0x5467E0 (`Average`), 0x54FAC0 (vtable-store ctor, vtable 0x84ECD4 = CPersistantEffect family), 0x554B60 (empty virtual `ret 0xc`, CVisualEffect slot 19/0x4C), 0x557FB0 (dead jmp thunk), 0x56DB30 (×10 container math), 0x56E1F0 (CGameAnimation dtor body), 0x575B20 (5-byte dtor thunk) | per-row evidence below |

## Already ported / inlined rows (13, 868 B)

`0x5402E0` (C2DArray ctor, `src/C2DArray.cpp:11`), `0x5467E0` (Average inlined, `src/CGameSprite.cpp:29356`), `0x55FF40` (inlined in `SpawnFromResRef`, `src/IcewindCGameEffects.cpp:966-992`), and the ten slot-0-dtor/thunk artifacts whose bodies are ported: `0x534B10`→`src/CProjectileShaft.cpp:27`, `0x5356A0`→`src/CProjectile84E940.cpp:31`, `0x536280`→`src/CProjectile84E9C0.cpp:38`, `0x5549B0`→`src/CVisualEffect.cpp:176`, `0x55D240`→`src/CGameAnimationType.cpp`, `0x55D2C0`→`src/CGameAnimationType84EEB8.cpp`, `0x56CEE0`→`src/CGameObject84F0DC.cpp`, `0x56E260`→`src/CGameObject84F14C.cpp`, `0x56F070`→`src/CProjectileArea.cpp`, `0x575B20`→thunk to ported `0x56F1F0`.

## Identity-only gaps (all dispositioned, names incomplete)

- **0x533600** — real code, PORT, but class/method name unresolved: its only external call site `0x5331DE` lies in an unexported chunk `0x532F00–0x5335FF` that is missing from `functions.tsv` (Ghidra export gap, not a survey row). Shape: CProjectileBAM-family spray-cell spawner.
- **0x5705D0** — PORT, CMessageScreenShake send factory confirmed by vtable `0x8488A8` and the field values matching the `CMessageScreenShake` ctor, but its caller `0x575C90` is another unexported chunk (Tremor/area path), so no method name.
- **0x547780 / 0x5477A0** — MFC CPlex-backed collection (vtable `0x84EBCC`/`0x84EBD0`); tree class absent, covered by MFC compat.
- **0x554B60** — empty virtual (`ret 0xc`) in CVisualEffect slot 19 (+0x4C); method name not distinguishable.
- **0x54FAC0** — 7-byte vtable-store ctor for vtable `0x84ECD4` (CPersistantEffect family); exact class not identifiable.
- **0x56E1F0** — class-level CGameAnimation destructor body (vtable `0x84F1B8`), no separate method name.
- **0x56F180** — global-object destructor cleanup (unwind-only callers), owning class unknown.

## Full table

| address | size | export name | class::method / identity | SP/MP | disposition | reason |
|---|---:|---|---|---|---|---|
| 0x532DC0 | 30 | `FUN_00532dc0` | CProjectileSPBRNHND deleting dtor | inherited (SP) | **COVERED** | CProjectileSPBRNHND deleting dtor (vtable 0x84E69C); class modelled src/CProjectileBAM.h:414, implicit dtor |
| 0x532DE0 | 281 | `FUN_00532de0` | CProjectileSPBRNHND destructor body | inherited (SP) | **COVERED** | CProjectileSPBRNHND destructor body; implicit dtor of the modelled class (members + CGameObject/CSound/CVidCell/CObList) |
| 0x533600 | 769 | `FUN_00533600` | real code: spray-cell spawn helper of the CProjectileBAM family | SP | **PORT** | real code: spray-cell spawn helper of the CProjectileBAM family (allocates cells, CTor+SetSequenceDirection+SetPalette, calls the slot-0x88 AddVisualToArea, recurses); SP projectile path. Class/method name unresolved: its only external call site 0x5331DE is in an unexported chunk (0x532F00-0x5335FF not in functions.tsv) |
| 0x533910 | 30 | `FUN_00533910` | CProjectileSprayCell deleting dtor | inherited (SP) | **COVERED** | CProjectileSprayCell deleting dtor (vtable 0x84E724); class modelled src/CProjectileBAM.h:427 |
| 0x533930 | 281 | `FUN_00533930` | CProjectileSprayCell destructor body | inherited (SP) | **COVERED** | CProjectileSprayCell destructor body; implicit dtor of the modelled class |
| 0x533DF0 | 30 | `FUN_00533df0` | CProjectileFollowUp deleting dtor | inherited (SP) | **COVERED** | CProjectileFollowUp deleting dtor (vtable 0x84E7B0); class modelled src/CProjectileBAM.h:439 |
| 0x533E10 | 300 | `FUN_00533e10` | CProjectileFollowUp destructor body | inherited (SP) | **COVERED** | CProjectileFollowUp destructor body; implicit dtor of the modelled class |
| 0x534770 | 30 | `FUN_00534770` | CProjectileMAGMISS deleting dtor | inherited (SP) | **COVERED** | CProjectileMAGMISS deleting dtor (vtable 0x84E838); class modelled src/CProjectileBAM.h:458 |
| 0x534790 | 300 | `FUN_00534790` | CProjectileMAGMISS destructor body | inherited (SP) | **COVERED** | CProjectileMAGMISS destructor body; implicit dtor of the modelled class |
| 0x534B10 | 30 | `FUN_00534b10` | CProjectileShaft deleting dtor | inherited (SP) | **COVERED** | CProjectileShaft deleting dtor (vtable 0x84E8C0); body 0x534B30 ported at src/CProjectileShaft.cpp:27 |
| 0x5356A0 | 30 | `FUN_005356a0` | CProjectile84E940 deleting dtor | inherited (SP) | **COVERED** | CProjectile84E940 deleting dtor (vtable 0x84E940); body 0x5356C0 ported at src/CProjectile84E940.cpp:31 |
| 0x536280 | 30 | `FUN_00536280` | CProjectile84E9C0 deleting dtor | inherited (SP) | **COVERED** | CProjectile84E9C0 deleting dtor (vtable 0x84E9C0); body 0x5362A0 ported at src/CProjectile84E9C0.cpp:38 |
| 0x537010 | 161 | `FUN_00537010` | std::set<LONG> range constructor | inherited (SP) | **COVERED** | std::set<LONG> range constructor (temp set default ctor; _Tree nil/heap lock family) |
| 0x5370C0 | 217 | `FUN_005370c0` | std::set<LONG> destructor | inherited (SP) | **COVERED** | std::set<LONG> destructor (std::_Tree teardown; used by ported projectile/effect dstors) |
| 0x5371A0 | 25 | `FUN_005371a0` | CGameEffectList constructor | inherited (SP) | **COVERED** | CGameEffectList constructor (CPtrList_Construct + vtable 0x84EA40; MFC CPtrList/MFC compat) |
| 0x5371C0 | 83 | `CircularLinkedList_InsertNewNodeAfter` | std::list node-insert machinery | inherited (SP) | **COVERED** | std::list node-insert machinery (CircularLinkedList_InsertNewNodeAfter) |
| 0x537220 | 156 | `StringContainer_Set` | CString/std::string assign internals | inherited (SP) | **COVERED** | CString/std::string assign internals (StringContainer_Set + SetStrLen/Grow) |
| 0x5402E0 | 77 | `FUN_005402e0` | already ported: C2DArray constructor at src/C2DArray.cpp:11 | inherited (SP) | **COVERED** | already ported: C2DArray constructor at src/C2DArray.cpp:11 |
| 0x5467E0 | 18 | `Average` | inlined in tree: Average helper | inherited (SP) | **COVERED** | inlined in tree: Average helper (src/CGameSprite.cpp:29356, comment at :29356; src/CScreenCharacter.cpp:11349) |
| 0x547780 | 30 | `FUN_00547780` | MFC CPlex-backed collection deleting dtor | inherited (SP) | **COVERED** | MFC CPlex-backed collection deleting dtor (class unresolved; MFC compat) |
| 0x5477A0 | 95 | `FUN_005477a0` | MFC CPlex-backed collection destructor | inherited (SP) | **COVERED** | MFC CPlex-backed collection destructor (CPlex_FreeDataChain; MFC compat) |
| 0x547800 | 93 | `FUN_00547800` | MFC CPlex_Create growth helper | inherited (SP) | **COVERED** | MFC CPlex_Create growth helper |
| 0x547880 | 17 | `FUN_00547880` | compiler memset helper | inherited (SP) | **COVERED** | compiler memset helper (rep stosd) |
| 0x54FAC0 | 7 | `FUN_0054fac0` | compiler vtable-store constructor | inherited (SP) | **COVERED** | compiler vtable-store constructor (vtable 0x84ECD4; CPersistantEffect family, src/CGameEffect.h:2019+) |
| 0x5549B0 | 30 | `FUN_005549b0` | CVisualEffect deleting dtor | inherited (SP) | **COVERED** | CVisualEffect deleting dtor (vtable 0x84EDFC); body 0x5549D0 ported at src/CVisualEffect.cpp:176 |
| 0x554B60 | 3 | `nullsub_26` | empty virtual stub | inherited (SP) | **COVERED** | empty virtual stub (ret 0xc) in the CVisualEffect vtable 0x84EDFC slot 19 (+0x4C); class modelled src/CVisualEffect.h |
| 0x557FB0 | 5 | `nullsub_4` | dead: five-byte jmp 0x799CA0 thunk with no call site and no .rdata reference | dead | **DROP** | dead: five-byte jmp 0x799CA0 thunk with no call site and no .rdata reference; unreachable compiler artifact |
| 0x55A7A0 | 229 | `FUN_0055a7a0` | std::map clear/destructor | inherited (SP) | **COVERED** | std::map clear/destructor (map nil 0x8e28c0 family) |
| 0x55B520 | 102 | `FUN_0055b520` | std::map _Tree find | inherited (SP) | **COVERED** | std::map _Tree find (nil 0x8e28c0) |
| 0x55B590 | 67 | `FUN_0055b590` | std::map _Tree lower_bound/find | inherited (SP) | **COVERED** | std::map _Tree lower_bound/find (nil 0x8e28c0) |
| 0x55B5E0 | 585 | `FUN_0055b5e0` | std::map insert | inherited (SP) | **COVERED** | std::map insert (nil 0x8e28c0) |
| 0x55B830 | 204 | `FUN_0055b830` | std::map erase | inherited (SP) | **COVERED** | std::map erase (0x8e28c0 family) |
| 0x55B900 | 102 | `FUN_0055b900` | std::map find | inherited (SP) | **COVERED** | std::map find (nil 0x8e28c8) |
| 0x55B970 | 235 | `FUN_0055b970` | std::map insert | inherited (SP) | **COVERED** | std::map insert (nil 0x8e28c8) |
| 0x55BA60 | 138 | `FUN_0055ba60` | std::map node alloc | inherited (SP) | **COVERED** | std::map node alloc (0x14-byte node) |
| 0x55BAF0 | 67 | `FUN_0055baf0` | std::map find/lower_bound | inherited (SP) | **COVERED** | std::map find/lower_bound (nil 0x8e28c8) |
| 0x55BB40 | 68 | `CircularLinkedList_DeleteAllNodesAtAndBefore` | std::list node-range delete | inherited (SP) | **COVERED** | std::list node-range delete (CircularLinkedList_DeleteAllNodesAtAndBefore) |
| 0x55BB90 | 42 | `CircularLinkedList_NewNode` | std::list node alloc | inherited (SP) | **COVERED** | std::list node alloc (CircularLinkedList_NewNode, 0xC bytes) |
| 0x55BBC0 | 572 | `FUN_0055bbc0` | std::map copy/ctor | inherited (SP) | **COVERED** | std::map copy/ctor (0x8e28c0 family) |
| 0x55BE00 | 572 | `FUN_0055be00` | std::map insert | inherited (SP) | **COVERED** | std::map insert (0x8e28c8 family) |
| 0x55C040 | 1017 | `FUN_0055c040` | std::map clear/erase | inherited (SP) | **COVERED** | std::map clear/erase (0x8e28c0 family) |
| 0x55C440 | 135 | `FUN_0055c440` | std::map _Tree successor/iterator | inherited (SP) | **COVERED** | std::map _Tree successor/iterator |
| 0x55C4D0 | 59 | `FUN_0055c4d0` | std::map iterator compare/find | inherited (SP) | **COVERED** | std::map iterator compare/find |
| 0x55C510 | 611 | `FUN_0055c510` | std::set<DWORD> insert | inherited (SP) | **COVERED** | std::set<DWORD> insert (m_struckTargets; 0x8e28c8 family) |
| 0x55C780 | 25 | `FUN_0055c780` | std::_Tree iterator increment | inherited (SP) | **COVERED** | std::_Tree iterator increment (nil 0x8e28c8) |
| 0x55C7A0 | 86 | `FUN_0055c7a0` | std::_Tree lower_bound | inherited (SP) | **COVERED** | std::_Tree lower_bound |
| 0x55C800 | 611 | `FUN_0055c800` | std::_Tree/map node ops | inherited (SP) | **COVERED** | std::_Tree/map node ops (set/map #2) |
| 0x55CA70 | 82 | `FUN_0055ca70` | std::_Tree rotate/rebalance | inherited (SP) | **COVERED** | std::_Tree rotate/rebalance |
| 0x55CAD0 | 86 | `FUN_0055cad0` | std::_Tree rotate/rebalance | inherited (SP) | **COVERED** | std::_Tree rotate/rebalance |
| 0x55CB30 | 27 | `FUN_0055cb30` | std::_Tree node alloc | inherited (SP) | **COVERED** | std::_Tree node alloc (0x1c bytes) |
| 0x55CB50 | 82 | `FUN_0055cb50` | std::_Tree rotate | inherited (SP) | **COVERED** | std::_Tree rotate |
| 0x55CBB0 | 86 | `FUN_0055cbb0` | std::_Tree rotate | inherited (SP) | **COVERED** | std::_Tree rotate |
| 0x55CC10 | 81 | `FUN_0055cc10` | std::set<DWORD> find/insert helper | inherited (SP) | **COVERED** | std::set<DWORD> find/insert helper (m_struckTargets) |
| 0x55CC70 | 81 | `FUN_0055cc70` | std::_Tree rotate/erase | inherited (SP) | **COVERED** | std::_Tree rotate/erase |
| 0x55CCD0 | 29 | `FUN_0055ccd0` | std::_Tree node pair copy | inherited (SP) | **COVERED** | std::_Tree node pair copy |
| 0x55D240 | 30 | `FUN_0055d240` | CGameAnimationTypeMonsterMulti/Quadrant deleting dtor | inherited (SP) | **COVERED** | CGameAnimationTypeMonsterMulti/Quadrant deleting dtor (vtable 0x84EF8C); body 0x55D060 ported at src/CGameAnimationType.cpp |
| 0x55D2C0 | 30 | `FUN_0055d2c0` | CGameAnimationType84EEB8 deleting dtor | inherited (SP) | **COVERED** | CGameAnimationType84EEB8 deleting dtor (vtable 0x84EEB8); body 0x55D2E0 ported at src/CGameAnimationType84EEB8.cpp |
| 0x55F6D0 | 30 | `CGameEffect_opcode_411_0` | IcewindCGameEffectSummon deleting dtor | inherited (SP) | **COVERED** | IcewindCGameEffectSummon deleting dtor (vtable 0x84F0A8 slot 0); implicit dtor |
| 0x55F6F0 | 17 | `CGameEffect_opcode_411_Destruct` | IcewindCGameEffectSummon destructor body: resets the vtable to CGameEffect base + CSound teardown | inherited (SP) | **COVERED** | IcewindCGameEffectSummon destructor body: resets the vtable to CGameEffect base + CSound teardown; our implicit dtor does the same |
| 0x55F710 | 511 | `CGameEffect_opcode_411_2` | IcewindCGameEffectSummon::ApplyEffect, vtable 0x84F0A8 slot 2 | SP | **PORT** | IcewindCGameEffectSummon::ApplyEffect, vtable 0x84F0A8 slot 2 (0x08); effect opcode CGAMEEFFECT_SUMMON (67) decoded at src/CGameEffect.cpp:407; runs in SP via the effect-apply path (CGameSprite::ProcessEffectList src/CGameSprite.cpp:16887) |
| 0x55FF40 | 498 | `FUN_0055ff40` | inlined in the ported IcewindCGameEffectSummon::SpawnFromResRef 0x55FAD0 | inherited (SP) | **COVERED** | inlined in the ported IcewindCGameEffectSummon::SpawnFromResRef 0x55FAD0 (src/IcewindCGameEffects.cpp:966-992) |
| 0x564580 | 20 | `CGameEffect_opcode_263_9` | IcewindCGameEffectDominate::OnRemove, vtable 0x84B7F0 slot 9 | SP | **PORT** | IcewindCGameEffectDominate::OnRemove, vtable 0x84B7F0 slot 9 (0x24); body = Icewind586B70::Instance()->Remove(pSprite); the tree class (src/IcewindCGameEffects.h) declares Copy/ApplyEffect/OnAddSpecific but no OnRemove |
| 0x56BDD0 | 118 | `FUN_0056bdd0` | std::string assign/grow internals | inherited (SP) | **COVERED** | std::string assign/grow internals (VC6 COW) |
| 0x56BE50 | 86 | `FUN_0056be50` | std::list<T>::_Insert | inherited (SP) | **COVERED** | std::list<T>::_Insert (28-byte payload, 0x24-byte node) |
| 0x56CEE0 | 30 | `FUN_0056cee0` | CGameObject84F0DC deleting dtor | inherited (SP) | **COVERED** | CGameObject84F0DC deleting dtor (vtable 0x84F0DC); body 0x56CF00 ported at src/CGameObject84F0DC.cpp |
| 0x56DB30 | 836 | `FUN_0056db30` | container size math | inherited (SP) | **COVERED** | container size math (x10 stride, 0x66666667 divide; no game logic) |
| 0x56E1F0 | 20 | `FUN_0056e1f0` | CGameAnimation destructor body | inherited (SP) | **COVERED** | CGameAnimation destructor body (vtable 0x84F1B8): vtable reset + deletes m_animation; class modelled src/CGameAnimation.h |
| 0x56E260 | 30 | `FUN_0056e260` | CGameObject84F14C deleting dtor | inherited (SP) | **COVERED** | CGameObject84F14C deleting dtor (vtable 0x84F14C); body 0x56E580 ported at src/CGameObject84F14C.cpp |
| 0x56F070 | 30 | `FUN_0056f070` | CProjectileArea deleting dtor | inherited (SP) | **COVERED** | CProjectileArea deleting dtor (vtable 0x84F1C4); body 0x56F1F0 ported at src/CProjectileArea.cpp |
| 0x56F090 | 229 | `FUN_0056f090` | std::map clear/destructor | inherited (SP) | **COVERED** | std::map clear/destructor (CProjectileArea target map; helpers 0x570780/0x570CC0/0x570D10) |
| 0x56F180 | 107 | `FUN_0056f180` | compiler/CRT: global object destructor cleanup | exit/unwind only | **COVERED** | compiler/CRT: global object destructor cleanup; callers are Unwind@0081fad0/81fade/81fc83/... only (exception-cleanup funclets), body is the VC6 string refcount free |
| 0x5705D0 | 74 | `FUN_005705d0` | real code: builds CMessageScreenShake | SP | **PORT** | real code: builds CMessageScreenShake(30,10,-20,0,id,id) and AddMessage; called from 0x575C99 (CProjectileTremor/area path; caller 0x575C90 is another Ghidra-export gap, not a survey row); SP screen-shake path |
| 0x570620 | 102 | `FUN_00570620` | std::map _Tree find | inherited (SP) | **COVERED** | std::map _Tree find (nil 0x8d48b4) |
| 0x570690 | 230 | `FUN_00570690` | std::map copy-ctor/assign | inherited (SP) | **COVERED** | std::map copy-ctor/assign (calls 0x570E00/0x571070) |
| 0x570780 | 1017 | `FUN_00570780` | std::map erase/clear | inherited (SP) | **COVERED** | std::map erase/clear (map helpers 0x5F9AA0/0x570B80) |
| 0x570B80 | 204 | `FUN_00570b80` | std::map erase helper | inherited (SP) | **COVERED** | std::map erase helper |
| 0x570C50 | 102 | `FUN_00570c50` | std::map find | inherited (SP) | **COVERED** | std::map find (nil 0x8e3e38) |
| 0x570CC0 | 67 | `FUN_00570cc0` | std::map find/lower_bound | inherited (SP) | **COVERED** | std::map find/lower_bound (nil 0x8e3e38) |
| 0x570D10 | 59 | `FUN_00570d10` | std::_Tree node unlink/erase helper | inherited (SP) | **COVERED** | std::_Tree node unlink/erase helper |
| 0x570D50 | 138 | `FUN_00570d50` | std::map constructor | inherited (SP) | **COVERED** | std::map constructor (nil-node alloc; called by CProjectileArea ctor 0x56EDD0) |
| 0x570DE0 | 25 | `FUN_00570de0` | std::_Tree iterator increment | inherited (SP) | **COVERED** | std::_Tree iterator increment (nil 0x8e3e38) |
| 0x570E00 | 611 | `FUN_00570e00` | std::map insert/node ops | inherited (SP) | **COVERED** | std::map insert/node ops (calls 0x5710D0) |
| 0x571070 | 81 | `FUN_00571070` | std::map erase/find helper | inherited (SP) | **COVERED** | std::map erase/find helper |
| 0x5710D0 | 23 | `FUN_005710d0` | std::_Tree node pair copy | inherited (SP) | **COVERED** | std::_Tree node pair copy |
| 0x575190 | 30 | `FUN_00575190` | CProjectileArea850384 deleting dtor | inherited (SP) | **COVERED** | CProjectileArea850384 deleting dtor (vtable 0x850384); class modelled src/CProjectileArea.h:360 |
| 0x5751B0 | 297 | `FUN_005751b0` | CProjectileArea850384 destructor body | inherited (SP) | **COVERED** | CProjectileArea850384 destructor body; implicit dtor of the modelled class (members + base CProjectileArea dtor) |
| 0x575B20 | 5 | `FUN_00575b20` | five-byte scalar deleting-dtor thunk, jmp 0x56F1F0 | inherited (SP) | **COVERED** | five-byte scalar deleting-dtor thunk, jmp 0x56F1F0 (CProjectileArea8504BC, modelled; base dtor ported) |

# #163 survey — PART D: 0x5776B0–0x59F4E0 (88 rows, 13,544 bytes)

Read-only survey of area thirteen (effects/messages/game-object helpers), part D.
Box: origin/main `53063343`. Method: `functions.tsv`/`callgraph.tsv`, address markers in `src/`
(case-insensitive), vtable pointers/stores, annotate; ADR 0002 for the MP rule. No commits.

**Revision 2 (final after task-44 verification).** Revision 1 booked 16 `CInfButtonArray` rows PORT (they are
`std::deque<int>` internals) and 6 projectile/effect rows PORT. Both are now corrected: the five "projectile
methods" are compiler-generated class destructors / MSVC STL container internals too. Final ledger: **0 PORT,
87 COVERED, 0 DROP, 1 UNSURE**. The range is entirely already-ported rows + compiler/library output; the one
real-game body left is 0x5819B0, unconfirmed (see caveat).

## Summary (revision 2)

| Disposition | Rows |
|---|---|
| **PORT** | **0** |
| **COVERED** | **87** (23 already ported; 15 more scalar dtors; 49 template/library/compiler) |
| **DROP** | **0** |
| **UNSURE** | **1** (0x5819B0 — real game body, caller unconfirmed; left out) |

**23 rows are already ported** (address markers present at the box) — the issue's unported-function
enumeration is stale here, exactly as in part A.

### Two headline corrections (both change the survey)

1. **0x5961A0–0x596B50 are not game code.** The 16 rows are **MSVC `std::deque<int>` implementation methods**:
   `src/CInfButtonArray.h:89-93` documents that the original member at 0x1986 is a `std::deque<INT>` and the
   tree replaced it with `CList<INT,INT> m_stateStack`; the bodies call only `operator_new`/`operator_delete` and
   each other and use the deque constants (0x1000-byte blocks, 0x3FF mask, 0x3FFFFF map arithmetic); the call
   sites are the ported `SetState`/`sub_589FF0`/`OnLButtonPressed` where the tree uses
   `m_stateStack.AddHead/RemoveHead`. **All 16 COVERED** (container replacement).
   Maintainer question (one line for the survey comment): keep the documented `CList` deviation, or restore
   `std::deque<INT>` so the compiler emits the original 16 functions?
2. **The five "projectile/effect methods" are also compiler/library output** (details below): two are
   `std::set` find/insert instantiations, one is the compiler-generated destructor of a ported
   projectile class, one is another `std::_Tree` internal, one is the compiler-generated destructor of
   `CProjectileBigBoulder` (class ported, adds no members). **COVERED, no port.**

## Already ported at the box (16 non-scalar rows; markers present)

0x57AB70 (570), 0x57B0A0 (297), 0x57B210 (1385), 0x57C7B0 (771, `CProjectileSingleTarget::DeliverEffects`
parked at slot 0x0078 per `src/CProjectileSingleTarget.h`), 0x57D5D0 (297), 0x57E570 (5, thunk), 0x57F380
(5, thunk), 0x57F780 (324, `CProjectileSingleTarget` scalar body — `CProjectileSingleTarget.h:303`), 0x5800E0
(330), 0x5803E0 (353, same class member teardown — `:331`), 0x580770 (297), 0x580CE0 (297), 0x581DD0 (353),
0x582210 (19), 0x582230 (56), 0x582670 (59).
Plus 0x57E1A0 (`CProjectileBAM::~CProjectileBAM`, 5 B; class dtor in `src/CProjectileBAM.cpp:59`).
And 7 of the scalar dtors below are also already ported (marked *).

## Scalar deleting destructors (22 rows; compiler-generated from the class vtables) — COVERED

| addr | size | class / real dtor |
|---|---|---|
| 0x5776B0 | 30 | `CProjectileBigBoulder` scalar dtor (real dtor 0x5776D0, vtable 0x850AD4) |
| 0x5780F0 | 30 | `CProjectileSingleTarget::~CProjectileSingleTarget` |
| 0x579B10* | 30 | real dtor `thunk_FUN_005783e0` |
| 0x579DC0* | 30 | real dtor 0x579DE0 |
| 0x57B080* | 30 | real dtor 0x57B0A0 (297, ported) |
| 0x57C660 | 30 | `IcewindCProjectileChainLightning` scalar dtor (real dtor 0x57C680) |
| 0x57CE60 | 30 | `CProjectile850F9C::~CProjectile850F9C` |
| 0x57E180 | 30 | `CProjectileBAM::~CProjectileBAM` |
| 0x57EAB0 | 30 | `CProjectileSunray::~CProjectileSunray` |
| 0x57F360* | 30 | real dtor `thunk_FUN_0051e9b0` |
| 0x5803C0* | 30 | real dtor 0x5803E0 (353, ported) |
| 0x580750 | 30 | real dtor 0x580770 (297, ported) |
| 0x580CC0* | 30 | real dtor 0x580CE0 (297, ported) |
| 0x581DB0* | 30 | real dtor 0x581DD0 (353, ported) |
| 0x5834C0 | 30 | real dtor 0x582780 (STL lock/container helper) |
| 0x5834E0 | 30 | real dtor 0x582980 (STL lock/container helper) |
| 0x583530 | 30 | real dtor 0x583550 (337, STL lock/container helper) |
| 0x58FF00 | 30 | `C2DArray` scalar dtor (`C2DArray_Destruct`; caller `CInfButtonArray::UpdateButtons`) |
| 0x5995F0 | 30 | `CDungeonMaster::~CDungeonMaster` |
| 0x59EB20 | 30 | `CMoveList::~CMoveList` |
| 0x59EC60 | 30 | real dtor 0x59EBD0 (string-list container family) |
| 0x59F4E0 | 30 | `CAIObjectType::~CAIObjectType` (caller `CInfGame::~CInfGame`) |

## The five previously-PORT rows — final identity and evidence (all COVERED)

| addr | size | final identity | evidence |
|---|---|---|---|
| 0x5776D0 | 11 | `CProjectileBigBoulder::~CProjectileBigBoulder` (compiler-generated: class has no members) | dtor sets vptr 0x850AD4 then tail-jumps `CProjectileArea::~` (0x56F1F0); ctor 0x577590 = tree `src/CProjectile.cpp:4680`, class `src/CProjectileArea.h:450-454` "Big Boulder (vtable 0x850AD4) … adds no members of its own"; the tree class declares no dtor, so its compiler emits the same vptr-store + base-dtor body. COVERED. |
| 0x577D60 | 45 | `std::set<LONG>`/map RB-tree lookup instantiation (static member set) | body: `[ecx+4]`→`[eax+4]`, sentinel global 0x8E3E38, walk `[ecx+0xC]` vs `*(arg)`, `[ecx+8]`/`[ecx]`, returns node — MSVC `_Tree::find/lower_bound`; only caller `CProjectileArea84F3AC::CreateSecondary` (tree NOTE at `src/CProjectileArea.cpp:622`: original looks the target up in the hit counts and drops the result). No game state. COVERED (tree's std::set members produce equivalents from the compiler). |
| 0x577D90 | 45 | same, second `std::set` instantiation | identical shape with sentinel 0x8E28C8 (`CProjectileSingleTarget.h:156-158` notes the exe std::set internals with NIL 0x8E28C8); callers `FUN_0057b210`/`FUN_0057be80` are compiler bodies. COVERED. |
| 0x57C680 | 297 | `IcewindCProjectileChainLightning::~IcewindCProjectileChainLightning` (compiler-generated) | destroys the `std::set<LONG>` at `this+0x2AE` (`[+0xC]` count, `[+4]` node chain), frees `_Nil` 0x8D48B4 under refcount 0x8D48B0 + `UnknownLock_Enter/Leave`, then tail-calls `CProjectileSingleTarget::~` (0x5783E0); `src/CProjectileSingleTarget.h:163-200` documents exactly those internals + class layout ("native size 0x2BE", set is the last member); the tree class declares no dtor → same compiler output. COVERED. |
| 0x582270 | 1017 | `std::_Tree`/container internal (erase/insert with lock) | calls `FUN_0055B590` (RB-tree helper, sentinel 0x8E28C0), `UnknownLock_Enter/Leave`, node rewiring, `operator_delete`; no game calls/strings; callers are compiler bodies of projectile classes (`0x57F780`/`0x5803E0` member teardown, `0x57D4E0` EH cleanup). COVERED (library). |

## UNSURE (left out of porting)

**0x5819B0** (725 B): real game body, no callgraph callers and no pointer-table reference, but five rel32 call
sites at 0x581927/0x581946/0x581965/0x581984/0x5819A3 lie in the preceding unlisted function (~0x581900, a
render routine: `CInfinity::FXBltFrom`/`FXUnlock`, `CGameArea::GetHeightOffset`, object animation fields).
Body: static "corcat" CResRef cache (guard 0x8E4EB0, global 0x8E52C0), `CCreatureFile` load, `new CGameSprite`,
`CGameAnimationType::GetEllipseRect`, `CGameObjectArray::GetDeny`, animation vtable calls + `SetFacing`, ret 0xC.
SP-reachable-looking, but the class/method and exact effect are unconfirmed — **do not book PORT**; needs a
targeted pass on the 0x581900 caller (it is absent from `functions.tsv`, so its own reachability is unproven).

## Template / library families — COVERED

| family | addresses |
|---|---|
| MSVC `std::deque<INT>` internals (corrected; container replaced by CList) | 0x5961A0, 0x596250, 0x5962E0, 0x596580, 0x596600, 0x596670, 0x5967A0, 0x596810, 0x596870, 0x5968C0, 0x596950, 0x596980, 0x5969F0, 0x596A80, 0x596B10, 0x596B50 |
| MSVC STL `_Tree`/list/lock internals (projectile/effect containers) | 0x582780, 0x582790, 0x5828E0, 0x582910, 0x582930, 0x582970, 0x582980, 0x5829E0, 0x582B50, 0x582B70, 0x582BA0, 0x583030, 0x583440, 0x583550, 0x5836B0, 0x5836F0, 0x5837D0, 0x583820 |
| circular-list template instances | 0x5867D0 (`CircularLinkedList_CopyChainToAfter`), 0x587660, 0x5876D0 |
| CString/CObList/CPtrArray/C2DArray container instantiations | 0x57AB40 (`CSummonLimitStruct_Destruct`), 0x58FE70 (4-byte stub), 0x59EB40, 0x59EBD0, 0x59EC80 (`CPtrArray_Destruct`) |
| other compiler output | 0x57D4E0 (230, EH-only cleanup/template dtor), 0x57E1A0 (`CProjectileBAM::~`, class ported) |

## Disposition ledger (88 rows)

23 already-ported rows (16 non-scalar + 7 scalar dtors*), 15 other scalar dtors, 16 deque internals,
18 STL lock/container instances, 3 circular-list instances, 5 string/list container instances,
2 other compiler/ported rows (0x57D4E0, 0x57E1A0), 5 previously-PORT rows now COVERED
(0x5776D0, 0x577D60, 0x577D90, 0x57C680, 0x582270), 1 UNSURE (0x5819B0) = 88.


## Judgement calls, for the maintainer

1. **`m_stateStack` container fidelity (part D).** The original uses `std::deque<INT>` for
   `CInfButtonArray::m_stateStack`; the tree replaced it with `CList<INT,INT>` and documents the
   deviation at `src/CInfButtonArray.h:89-93`, implementing the same front-stack semantics. This is
   why 16 part-D rows are COVERED-by-replacement. Do you want the exact container back so the
   compiler emits those 16 functions (removing the last documented deviation), or keep the
   documented `CList` version? Behaviour-equivalent either way.
2. **Immunity-message constructors 0x514F60 / 0x516CD0 (part B).** In the original they execute in
   SP (the message is created, then broadcast-to-others and dropped); the tree omits the creation
   site as behaviour-neutral, so they are booked DROP-as-MP-only. If you read "SP-reachable =
   executes in SP" literally they would be PORT with no in-tree caller. Say which convention you
   want and I'll rebook if needed.
3. **`0x5819B0` (part D, 725 B) — the area's only open row.** Part D's verification says it is a
   real game body (a static "corcat" `CResRef` cache with an init guard, `CCreatureFile` →
   `new CGameSprite` → `CGameAnimationType::GetEllipseRect` → `CGameObjectArray::GetDeny` →
   animation vtable calls + `SetFacing`), with no callgraph callers and no pointer-table reference.
   A rel32 scan found five call sites (0x581927/46/65/84/A3) inside the **preceding function at
   ~0x581900**, which is not in `functions.tsv` — an unlisted render routine
   (`CInfinity::FXBltFrom`/`FXUnlock`, `CGameArea::GetHeightOffset`, object animation fields). Its
   reachability looks SP but is unproven. Needs a targeted pass on that caller before any decision;
   it is not ported on speculation.
4. **Will's tree** (7820ef22) is not available in this checkout. If you want the part-C imported
   body treated as an import in the 0x55F710 PR, the `Imported-From:` trailer will cite
   `WillScarlettOhara/iwd2-re` per `docs/wso/README.md`; a Will-tree cross-check of the remaining
   names would need a fetchable ref.

## Corrections to the issue / earlier records

1. **Stale "unported" rows**: ~40 of the 349 already have tree markers (each part lists its own),
   plus rows present-but-unmarked (`0x4F6E50 CMessageStoreMarkup` ctor, the known #121 gap).
2. **`CMessage105` is `CMessageGroup`**: Ghidra exports the class's codecs under "CMessage105", but
   vtable 0x84D564, its ctor 0x5152C0, getter 0x515400 and Run 0x5157F0 are `CMessageGroup`, which
   is in the tree. One class, not two.
3. **The 300/332-byte "template" bodies** around 0x52CE30-0x52EB30 and 0x55B5E0-0x55C800 are
   complete destructors / map-and-set instances respectively (part B and part C name each family);
   none is a game-code template needing a port.
4. **Part B carries no `CPersistantEffect*_AutoDestructor` rows and no collection-template rows** —
   those are in part A (0x4C1AE0-0x4C3160, 0x4C45A0-0x4C5E80).
5. **Message constructors**: the tree's "the constructor is inlined in the exe / no standalone
   constructor address exists" notes for eight message classes are half-right — the out-of-line ctor
   copies do exist (0x4F5130, 0x4F53E0, 0x4F54F0, 0x4F59D0, 0x4F5A90, 0x4F5C70, 0x4F69B0,
   0x4F6B20) but their only caller is the removed network dispatcher, so the disposition is
   unchanged. Worth a comment cleanup when those files are next touched.

## Verification

- Every one of the 349 addresses was annotated; all are `functions.tsv` entries with matching sizes.
- Class attribution: `.rdata` vtable tables + ctor vtable stores + src headers; no RTTI in the exe,
  so a handful of unnamed-class identities are stated as inferred (part A: 0x4CAEA0, 0x4D4730;
  part C: 0x533600, 0x5705D0 caller gap, 0x547780/0x5477A0, 0x54FAC0, 0x56E1F0, 0x56F180;
  part D: unnamed scalar dtors marked *). None of those affects a disposition that requires porting.
- Tree coverage: case-insensitive `// 0xADDR` marker search and `src/` class models at `53063343`;
  `compat/` templates named for every COVERED template row.
- Read-only: the survey made no commits and no git mutation; each part worked from the main checkout
  or a `git archive` copy.
