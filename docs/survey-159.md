# Survey 159 — area twelve: the multiplayer screen and its neighbours (0x640000-0x652E50)

Survey for thierbig/iwd2r#159: 134 functions / 39,902 bytes. Read-only analysis at `ca6b24c2`;
no source, evidence, MAP or recovery-status file was touched for this survey.

## Headline: the area has no unported work

| disposition | rows |
|---|---:|
| **PORT** | **0** |
| COVERED (compiler-generated, or already ported in the tree) | 20 |
| DROP (multiplayer-only under ADR 0002, dead code, or removed lobby port) | 114 |
| **total** | **134** |

Two independent findings make the area collapse:

1. **Part B is the *removed* lobby port, not unported work.** Commit `32213cd5` deleted
   `src/CScreenMultiPlayer.cpp` (3,637 lines) and its header under ADR 0002. That file carries a
   marker for **every one of the 53 non-compiler part-B addresses** (same addresses, same
   function sizes), and the current `src/CUIControlFactory.cpp` still keeps the two classes
   another screen shares (`CUIControlButtonMultiPlayerPortrait` at :5232,
   `CUIControlEditMultiPlayerChat` at :5315) with the note "moved here from the removed
   CScreenMultiPlayer.cpp". So part B is a policy drop, not a port backlog — the code exists in
   history if the lobby is ever restored.
2. **Part A's porting work is all covered.** Its 16 scalar-deleting destructors belong to
   already-ported `CScreenMap`/`CScreenMovies` control classes (the compiler emits them), one row
   (`0x6475B0 CScreenMovies::OnMovieItemSelect`) is already ported in the tree, the two initially
   unidentified destructors turned out to be the `CList<DWORD>` template instance for a ported
   `CUIControlButtonMapAreaMap` member, and the other 38 are `CScreenMultiPlayer` members or two
   unreferenced functions.

The issue's "134 unported functions" is stale in both directions: it counts compiler-generated
destructors and a previously ported function as unported, and part B's rows were ported once and
then deliberately deleted.

## Method

`functions.tsv` / `callgraph.tsv` for names, sizes and callers; capstone disassembly (`annotate.py`
with the harness `IWD2R_HOME`) for bodies, vtable stores and cross-references; the `.rdata` vtable
and pointer tables for class attribution; the `InfScreenMultiPlayer.cpp` assertion string
(`0x8B44F0`) to bind bodies to the lobby source unit; the tree's `src/` to decide COVERED vs PORT;
`docs/adr/0002-single-player-only.md` for the multiplayer rule; and the repo's pre-removal port
(`32213cd5^:src/CScreenMultiPlayer.cpp`) for names in part B (Will's tree at 7820ef22 is not in
this checkout). Names from history were re-checked against the exe structurally.

## Part A — rows 0x6405F0-0x64F170 (56 rows): 0 PORT, 18 COVERED, 38 DROP

| address | size | export name | real class::method | reach | disposition | reason |
|---|---|---|---|---|---|---|
| 0x6405F0 | 30 | `CScreenMap_006405f0` | `CScreenMap` scalar deleting dtor (sd=0x640610 `~CScreenMap`; vt=0x856390+0) | SP: class ported in `src/CScreenMap.cpp` | COVERED | compiler emits it for the ported class |
| 0x642720 | 30 | `FUN_00642720` | `CUIControlButtonMapWorld` scalar dtor (sd=0x642740; vt=0x8564A8+0) | SP: class ported in `src/CScreenMap.h` | COVERED | compiler-generated |
| 0x642B60 | 30 | `FUN_00642b60` | `CUIControlButtonMapAreaMap` scalar dtor (sd=0x642B80) | SP: ported | COVERED | compiler-generated |
| 0x6458E0 | 30 | `FUN_006458e0` | `CUIControlButtonMapError` scalar dtor (sd=0x645900) | SP: ported | COVERED | compiler-generated |
| 0x645A10 | 30 | `FUN_00645a10` | `CUIControlButtonMapNoteDone` scalar dtor (sd=0x645A30) | SP: ported | COVERED | compiler-generated |
| 0x645B40 | 30 | `FUN_00645b40` | `CUIControlButtonMapNoteCancel` scalar dtor (sd=0x645B60) | SP: ported | COVERED | compiler-generated |
| 0x645C70 | 30 | `FUN_00645c70` | `CUIControlButtonMapNoteDelete` scalar dtor (sd=0x645C90) | SP: ported | COVERED | compiler-generated |
| 0x645DE0 | 30 | `FUN_00645de0` | `CUIControlEditMultiLineMapNote` scalar dtor (sd=0x645E00) | SP: ported | COVERED | compiler-generated |
| 0x645FB0 | 30 | `FUN_00645fb0` | `CUIControlButtonMapShowNotes` scalar dtor (sd=0x645FD0) | SP: ported | COVERED | compiler-generated |
| 0x646170 | 30 | `FUN_00646170` | `CUIControlButtonMapNote` scalar dtor (sd=0x646190) | SP: ported | COVERED | compiler-generated |
| 0x646770 | 30 | `FUN_00646770` | `CUIControlButtonMapNoteFlagChoice` scalar dtor (sd=0x646790) | SP: ported | COVERED | compiler-generated |
| 0x646940 | 95 | `FUN_00646940` | `CList<DWORD>::~CList` (MFC template instance for `CUIControlButtonMapAreaMap::field_7BE`, `src/CScreenMap.h:153`; vptr 0x856598) | SP: compiler-generated from `compat/mfc_list.h` | COVERED | MFC template instantiation, no source to port |
| 0x6469A0 | 30 | `FUN_006469a0` | scalar deleting dtor of the same `CList<DWORD>` instance | SP: compiler-generated | COVERED | same template instance |
| 0x6470B0 | 30 | `CScreenMovies_006470b0` | `CScreenMovies` scalar dtor (sd=0x6470D0; vt=0x85696C+0) | SP: class ported | COVERED | compiler-generated |
| 0x6475B0 | 327 | `CScreenMovies::OnMovieItemSelect` | `CScreenMovies::OnMovieItemSelect` | SP: **already ported** at `src/CScreenMovies.cpp:257` (`// 0x06475B0`) | COVERED | issue list is stale for this row |
| 0x647BC0 | 30 | `FUN_00647bc0` | `CUIControlButtonMoviesPlay` scalar dtor (sd=0x647BE0) | SP: ported | COVERED | compiler-generated |
| 0x647D90 | 30 | `FUN_00647d90` | `CUIControlButtonMoviesCredits` scalar dtor (sd=0x647DB0) | SP: ported | COVERED | compiler-generated |
| 0x647FA0 | 30 | `FUN_00647fa0` | `CUIControlButtonMoviesDone` scalar dtor (sd=0x647FC0) | SP: ported | COVERED | compiler-generated |
| 0x6481C0 | 3152 | `CScreenMultiPlayer_Construct` | `CScreenMultiPlayer` constructor (stored at chitin+0x1C90 by `CBaldurChitin::Init` 0x423C3D) | MP: class deleted by ADR 0002 | DROP | the lobby screen class itself |
| 0x648E10 | 30 | `CScreenMultiPlayer_00648e10` | `CScreenMultiPlayer` scalar dtor (sd=0x648E30; vt=0x856C6C+0) | MP | DROP | lobby-screen member |
| 0x648E30 | 149 | `FUN_00648e30` | `~CScreenMultiPlayer` (calls `CUIManager`/`CVidFont`/`CWarp` dtors) | MP | DROP | lobby-screen member |
| 0x648ED0 | 420 | `CScreenMultiPlayer_00648ed0` | `CScreenMultiPlayer::EngineActivated` (vt+0x10) | MP | DROP | lobby-screen virtual |
| 0x649080 | 103 | `CScreenMultiPlayer_00649080` | `CScreenMultiPlayer::EngineDeactivated` (vt+0x14) | MP | DROP | lobby-screen virtual |
| 0x6490F0 | 1537 | `CScreenMultiPlayer_EngineInitialized` | `CScreenMultiPlayer::EngineInitialized` (vt+0x1C) | MP | DROP | lobby-screen virtual |
| 0x649700 | 591 | `CScreenMultiPlayer_OnKeyDown` | `CScreenMultiPlayer::OnKeyDown` (vt+0xA8) | MP | DROP | lobby-screen virtual |
| 0x649990 | 1887 | `CScreenMultiPlayer_00649990` | `CScreenMultiPlayer::TimerAsynchronousUpdate` (vt+0xBC) | MP | DROP | lobby-screen virtual |
| 0x64A130 | 278 | `FUN_0064a130` | `CScreenMultiPlayer` helper: text display / scroll refresh | MP | DROP | lobby-only helper |
| 0x64A270 | 529 | `FUN_0064a270` | `CScreenMultiPlayer` helper: panel activate/inactive refresh | MP | DROP | lobby-only helper |
| 0x64A4B0 | 625 | `FUN_0064a4b0` | `CScreenMultiPlayer` helper: panel/permission refresh | MP | DROP | lobby-only helper |
| 0x64A750 | 341 | `FUN_0064a750` | `CScreenMultiPlayer` helper: player-ready/character-status update | MP | DROP | lobby-only helper |
| 0x64A8B0 | 312 | `FUN_0064a8b0` | `CScreenMultiPlayer` helper: save multiplayer permissions / arbitration | MP | DROP | lobby-only helper |
| 0x64AA10 | 262 | `FUN_0064aa10` | `CScreenMultiPlayer` helper: `SaveMultiPlayerPermissions` + status refresh | MP | DROP | lobby-only helper |
| 0x64AB40 | 276 | `CScreenSpell_B3AUTO_0064ab40` | `CScreenMultiPlayer` helper: text-display scroll (Ghidra alias name is bogus) | MP | DROP | lobby-only helper |
| 0x64AC60 | 326 | `FUN_0064ac60` | `CScreenMultiPlayer` helper: permission/button state | MP | DROP | lobby-only helper |
| 0x64ADB0 | 398 | `FUN_0064adb0` | `CScreenMultiPlayer` helper: button enabling, char list | MP | DROP | lobby-only helper |
| 0x64AF40 | 333 | `FUN_0064af40` | `CScreenMultiPlayer` helper: UI update | MP | DROP | lobby-only helper |
| 0x64B090 | 681 | `FUN_0064b090` | `CScreenMultiPlayer` helper: permissions/character status | MP | DROP | lobby-only helper |
| 0x64B340 | 2472 | `FUN_0064b340` | `CScreenMultiPlayer` helper: main panel refresh | MP | DROP | lobby-only helper |
| 0x64BCF0 | 309 | `FUN_0064bcf0` | `CScreenMultiPlayer` helper: 3-state button selection | MP | DROP | lobby-only helper |
| 0x64BE30 | 989 | `FUN_0064be30` | `CScreenMultiPlayer` helper: permissions / 3-state buttons | MP | DROP | lobby-only helper |
| 0x64C210 | 416 | `FUN_0064c210` | `CScreenMultiPlayer` helper: permissions refresh | MP | DROP | lobby-only helper |
| 0x64C3B0 | 580 | `FUN_0064c3b0` | `CScreenMultiPlayer` helper: character status / control | MP | DROP | lobby-only helper |
| 0x64C600 | 4177 | `CScreenMultiPlayer_OnMainDoneButtonClick` | `CScreenMultiPlayer::OnMainDoneButtonClick` | MP | DROP | lobby start-game flow |
| 0x64D690 | 150 | `FUN_0064d690` | `CScreenMultiPlayer::ClearChatMessages` | reached in SP only through the always-constructed MP object (`CBaldurChitin::ClearChatMessages` 0x425BD0 via chitin+0x1C90 from `CUIControlButtonStartMenu::OnLButtonClick`) | DROP | lobby-screen member; ADR trimming needed at the call site |
| 0x64D730 | 182 | `FUN_0064d730` | `CScreenMultiPlayer` helper: can-save/permission check | MP | DROP | lobby-only helper |
| 0x64D7F0 | 344 | `FUN_0064d7f0` | `CScreenMultiPlayer` helper (can-save / permissions) | DEAD: no callers, no pointer references in the image | DROP | unreferenced in the original |
| 0x64D950 | 31 | `CScreenMultiPlayer_0064d950` | `CScreenMultiPlayer` virtual at vt+0x100 (slot 64): loops this+0x444 calling 0x64AA10 | MP | DROP | lobby-screen virtual |
| 0x64D970 | 516 | `FUN_0064d970` | `CScreenMultiPlayer` helper: arbitration lock + close session | MP | DROP | lobby-only helper |
| 0x64DB90 | 533 | `FUN_0064db90` | `CScreenMultiPlayer` helper: help/text update | MP | DROP | lobby-only helper |
| 0x64DDB0 | 536 | `FUN_0064ddb0` | `CScreenMultiPlayer::StartMultiPlayer` (the binary twin of `StartSinglePlayer`; reads `CMultiplayerSettings`, writes only this+0x48A) | reached in SP only through the always-constructed MP object (12 call sites load chitin+0x1C90 and null-check, e.g. `CScreenConnection::OnNewGameButtonClick` 0x5FD5A5, `CScreenLoad::LoadGame`) | DROP | lobby-screen member; ADR trimming needed at the call sites |
| 0x64DFD0 | 220 | `FUN_0064dfd0` | `CScreenMultiPlayer` helper: mouse capture / panel close | MP | DROP | lobby-only helper |
| 0x64E0B0 | 172 | `FUN_0064e0b0` | `CScreenMultiPlayer` helper | DEAD: no callers, no pointer references in the image | DROP | unreferenced in the original |
| 0x64E160 | 352 | `CGameSprite_B3AUTO_0064e160` | `CScreenMultiPlayer` helper: character class/level text (Ghidra alias name is bogus) | MP | DROP | lobby-only helper |
| 0x64E2C0 | 3502 | `FUN_0064e2c0` | `CScreenMultiPlayer` helper: large character/spell/item panel builder | MP | DROP | lobby-only helper |
| 0x64F090 | 216 | `FUN_0064f090` | `CScreenMultiPlayer` helper: permission/character status | MP | DROP | lobby-only helper |
| 0x64F170 | 136 | `FUN_0064f170` | `CScreenMultiPlayer` helper: portrait click path | MP | DROP | lobby-only helper |

## Part B — rows 0x64F200-0x652E50 (78 rows): 0 PORT, 2 COVERED, 76 DROP

All 23 control classes below are **lobby-only** (`CUIControl` subclasses created exclusively by the
GUIMP arm of `CUIControlBase::CreateControl` 0x76D370, in 0x76EB90-0x76F351; the next arm GUISP
starts at 0x76F351, and no function outside that arm calls any of their ctors). One-line reason for
every class: **"lobby-only control; creation sites are all in the removed GUIMP arm, no
single-player screen constructs it."** The pre-removal port in `32213cd5^:src/CScreenMultiPlayer.cpp`
carries the same addresses.

| class | ctor | dtor | scalar dtor | extra rows |
|---|---|---|---|---|
| CUIControlButtonMultiPlayerReady (3State) | 0x64F690 | 0x64F700 | 0x64F6E0 | |
| CUIControlButtonMultiPlayerPlayer | 0x64FC30 | 0x64FCF0 | 0x64FCD0 | |
| CUIControlButtonMultiPlayerCharacter | 0x64FE80 | 0x64FF40 | 0x64FF20 | |
| CUIControlButtonMultiPlayerDone | 0x6500A0 | 0x650190 | 0x650170 | |
| CUIControlButtonMultiPlayerOptions | 0x650270 | 0x650360 | 0x650340 | |
| CUIControlButtonMultiPlayerLogout | 0x6504B0 | 0x6505A0 | 0x650580 | |
| CUIControlButtonMultiPlayerPermissionsPermission (3State) | 0x650830 | 0x6508B0 | 0x650890 | |
| CUIControlButtonMultiPlayerPermissionsKick | 0x650A90 | 0x650AE0 | 0x650AC0 | |
| CUIControlButtonMultiPlayerPermissionsOptions | 0x650BC0 | 0x650CB0 | 0x650C90 | |
| CUIControlButtonMultiPlayerPermissionsListen (3State) | 0x650E00 | 0x650EF0 | 0x650ED0 | |
| CUIControlButtonMultiPlayerPermissionsDone | 0x650FE0 | 0x6510D0 | 0x6510B0 | |
| CUIControlButtonMultiPlayerOptionsImport (3State) | 0x6511B0 | 0x651230 | 0x651210 | |
| CUIControlButtonMultiPlayerOptionsCancel | 0x651420 | 0x651510 | 0x6514F0 | |
| CUIControlButtonMultiPlayerOptionsDone | 0x6515F0 | 0x6516E0 | 0x6516C0 | |
| CUIControlButtonMultiPlayerModifyCharacterCreate | 0x6517C0 | 0x6518B0 | 0x651890 | 0x651950 `OnLButtonClick` (837 B) |
| CUIControlButtonMultiPlayerModifyCharacterDelete | 0x651CA0 | 0x651D90 | 0x651D70 | 0x651E30 `OnLButtonClick` (1,122 B) |
| CUIControlButtonMultiPlayerModifyCharacterCancel | 0x6522A0 | 0x652390 | 0x652370 | |
| CUIControlButtonMultiPlayerModifyPlayerPlayer | 0x652470 | 0x652530 | 0x652510 | 0x6525D0 `OnLButtonClick` (180 B) |
| CUIControlButtonMultiPlayerModifyPlayerCancel | 0x652690 | 0x652780 | 0x652760 | |
| CUIControlButtonMultiPlayerModify | 0x652860 | 0x652950 | 0x652930 | |
| CUIControlButtonMultiPlayerError | 0x652A30 | 0x652A80 | 0x652A60 | |
| CUIControlButtonMultiPlayerViewCharacterDone | 0x652B60 | 0x652C30 | 0x652C10 | |
| CUIControlButtonMultiPlayerSelection (3State) | 0x652DD0 | 0x652E50 (outside the range) | 0x652E30 | |

`CScreenMultiPlayer` methods in part B (all DROP: class deleted by ADR 0002):

| address | size | export name | real class::method | disposition | reason |
|---|---|---|---|---|---|
| 0x64F200 | 160 | `FUN_0064f200` | `CScreenMultiPlayer::OnLogoutButtonClick` | DROP | lobby screen removed |
| 0x64F2A0 | 263 | `FUN_0064f2a0` | `CScreenMultiPlayer::UpdateHelp` | DROP | lobby screen removed |
| 0x64F3B0 | 281 | `FUN_0064f3b0` | `CScreenMultiPlayer::CopyChatEditBox` | DROP | lobby screen removed |
| 0x64F4D0 | 266 | `CScreenMultiPlayer_0064f4d0` | `CScreenMultiPlayer::GetChatEditBoxStatus` | DROP | lobby screen removed |
| 0x64F5E0 | 175 | `CScreenMultiPlayer_0064f5e0` | `CScreenMultiPlayer::SetChatEditBoxStatus` | DROP | lobby screen removed |

COVERED (compiler-generated; the class is one of the two the factory kept):

| address | size | export name | class | disposition | reason |
|---|---|---|---|---|---|
| 0x64F870 | 30 | `FUN_0064f870` | scalar deleting dtor of `CUIControlButtonMultiPlayerPortrait` (dtor 0x64F890 ported, `src/CUIControlFactory.cpp`) | COVERED | compiler emits it with the ported class |
| 0x6506B0 | 30 | `FUN_0064f6b0` | scalar deleting dtor of `CUIControlEditMultiPlayerChat` (dtor 0x6506D0 ported) | COVERED | same |

## Contradictions with the issue's assumptions

1. **"134 unported functions" is stale.** `0x6475B0 CScreenMovies::OnMovieItemSelect` is already
   ported (`src/CScreenMovies.cpp:257`, marker `// 0x06475B0`), and all 15 part-A 30-byte rows are
   compiler-generated scalar deleting destructors of already-ported classes.
2. **Part B is not "unported" at all**: it was ported before and deleted by `32213cd5` under
   ADR 0002 (3,637-line `src/CScreenMultiPlayer.cpp`). It is a policy drop with the code in
   history, not a backlog.
3. **"The repeating 30/151/193-byte rows look like per-control ctor / dtor / scalar-deleting-dtor
   triples"** — in this build the 30-byte rows are the *scalar deleting* dtors; the 151/193-byte
   rows are the real dtors/ctars. Structurally consistent, but the order in the issue is
   misleading.
4. **The range crosses into `CScreenMovies`, not just `CScreenMap`/`CScreenSinglePlayer`**:
   0x646A40-0x64A110 is `CScreenMovies` (0x64A110 is `CScreenMovies::TimerSynchronousUpdate`,
   already ported), and the 0x64Axxx/0x64Exxx rows are `CScreenMultiPlayer`, not
   `CScreenSinglePlayer`.
5. **`CScreenMultiPlayer` is constructed unconditionally by `CBaldurChitin::Init`** (0x423C3D,
   stored at chitin+0x1C90), so its object exists in the original engine in single player too.
   Two rows are therefore reached from SP engine code: `0x64D690 ClearChatMessages` (via
   `CBaldurChitin::ClearChatMessages` 0x425BD0 from `CUIControlButtonStartMenu::OnLButtonClick`) and
   `0x64DDB0 StartMultiPlayer` (12 call sites load chitin+0x1C90 and null-check it). Both stay
   DROP as lobby members, but **ADR 0002's deletion also has to trim or guard those call sites**,
   which is separate work from this area.

## Open questions for the maintainer

1. **ADR 0002 trimming: none needed in code.** The two lobby methods reached from SP code
   (`0x64D690 ClearChatMessages`, `0x64DDB0 StartMultiPlayer`) are called from 14 exe sites through
   `CBaldurChitin+0x1C90` (`m_pEngineMultiPlayer`, written unconditionally by `CBaldurChitin::Init`
   0x423C3D): `CBaldurChitin::ClearChatMessages`/`OnMultiplayerSessionOpen`
   (`src/CBaldurChitin.cpp:2066`/`:1983`), `CBaldurEngine::OnLeftPanelButtonClick`
   (`src/CBaldurEngine.cpp:767`), `CBaldurMessage::OnSettingsArbitrationLockStatus` (2 sites,
   `src/CMessage.cpp:2252`), `CMessageHandler::PostAsynchronousUpdate` (`src/CMessage.cpp:10296`),
   `CScreenConnection`'s `AutoStartDirectPlayLobby`/`HandleEMEvent`/`OnDoneButtonClick`/
   `OnLobbyNewGameButtonClick`/`OnNewGameButtonClick` (`src/CScreenConnection.cpp:3688/3275/1587/3877/1310`),
   `CScreenCreateChar::OnMainCancelButtonClick` (`src/CScreenCreateChar.cpp:6038`),
   `CScreenLoad::LoadGame` (`src/CScreenLoad.cpp:482`) and
   `CUIControlButtonCharGenAccept::OnLButtonClick` (`src/CScreenCreateChar.cpp:6771`).
   **The tree already omits all of them** — `m_pEngineMultiPlayer` and `+0x1C90` have zero
   references in `src/`; only the explanatory comment at `src/CMessage.cpp:1131` records the
   removal. So the ADR review item is that the 14 removed sync points are genuinely SP-neutral;
   the two to re-check are `CScreenCreateChar::OnMainCancelButtonClick` (keep its SP branch of the
   mode test) and the `CScreenConnection` Done/NewGame handlers whose MP branch drove the lobby.
2. **Booking convention for scalar deleting dtors of dropped lobby classes.** This survey books the
   23 scalar dtors of lobby classes as **DROP, part of their class line**, and only the two whose
   classes survive as COVERED (per the "compiler-generated rows count as covered if our tree gets
   them from the compiler" rule). Booking all 25 as COVERED would make it DROP 53 / COVERED 25;
   either is defensible.
3. **Range boundary:** `CUIControlButtonMultiPlayerSelection::~CUIControlButtonMultiPlayerSelection`
   is `0x652E50`, exactly the stated range end, and it starts at the boundary and extends beyond
   it — it is not in the 134 rows. Its scalar dtor 0x652E30 is in range (DROP with the class).
   Include 0x652E50 in area twelve, or leave it to area thirteen?
4. **Will's tree cross-check:** commit `7820ef22` is not in this checkout and no local bundle has
   it, so part-B names come from the repo's own pre-removal port verified structurally against the
   exe, not from Will. If you want a Will-tree cross-check of the 53 names, that needs a fetch of
   his commit.

One note for a possible restoration: the old port has `// TODO: Incomplete.` at `0x64E2C0`
(`CScreenMultiPlayer::ResetViewCharacterPanel`), so a restoration there would not be drop-in.

## Verification

- All 134 addresses are `functions.tsv` entries and their sizes match the issue.
- Structure: capstone disassembly of every row; 23 part-B ctors store a class vtable whose slot 0 is
  the 30-byte scalar dtor calling the 151-byte dtor ending in the base `CUIControlButton` /
  `CUIControlButton3State` destruction; base-class matches the pre-removal source inheritance for
  all 23.
- Creation sites: linear capstone scan of the 38,978-byte `CUIControlBase::CreateControl` — each of
  the 23 ctors has exactly one call, from the GUIMP arm; reverse callgraph shows no other caller.
- Removal evidence: `32213cd5` (message, diffstat `CScreenMultiPlayer.cpp -3,637` / `.h -316`,
  factory GUIMP arm reduced to the NOTE at `src/CUIControlFactory.cpp:544-547`), and the tree still
  contains exactly the two shared classes the factory kept.
- Part A: vtable pointer tables, ctor vtable stores, real-dtor to scalar-dtor call edges, the
  `InfScreenMultiPlayer.cpp` assertion string, and the tree's `src/` for the COVERED/PORT call.
- The two initially-unidentified destructors (0x646940/0x6469A0) were resolved by searching `.text`
  for every immediate occurrence of the vptr (0x856598 appears exactly three times: the
  `CUIControlButtonMapAreaMap` ctor's `mov dword ptr [esi+0x7BE], 0x856598`, its dtor, and
  0x646940) plus the `C7 /0` store-pattern scan that found the disp32 ctor store; the class member
  is at the same offset in `src/CScreenMap.h:153`.
- Tree checks run during the survey: `git status` clean before and after; no commits, no branches,
  no `git` mutation. Survey only: no `src/`, `docs/evidence/`, `MAP.md` or recovery-status change.
