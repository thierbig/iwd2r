# Follow-ups from the verification harness

These items were found while building the harness (project 2) and parked. The first group is engine behaviour for project 3. The rest is hardening of the tools.

## Engine fidelity (project 3)

- **`CTlkFileOverride` double free (about 1 Linux run in 60).** `CTlkFileOverride::CloseFiles`, called from `CScreenConnection::EngineActivated` (0x5FA9B0, src/CScreenConnection.cpp near line 348, before `OpenOverride("temp/default.toh", ...)`) on the AI thread during the start movie, aborts with a double free in about one Linux run in 60. Not diagnosed yet: find what else closes or reopens the override files at that moment, and pin it with a test.
- **`ProcessEffectList` (0x72DE60) remainders.** Still TODO in src/CGameSprite.cpp: 0x72E3A8-0x72E70E (the colour ranges and colour effects, the animation change with its search bitmap update and sequence reset, and their messages; the reset calls `SetSequence` 0x707230 twice, which can draw rand at 0x7077AE), 0x72E759-0x72FB24 (the AI actions and messages after a change of state, the general state against `field_72EC`), and `HandlePersistantEffects` (0x733200, with 1; 0x72FBA7-0x72FBC3) while the world timer runs, which needs 0x51E8B0 on `m_cRegeneratedPersistantEffectList`. docs/evidence/72DE60.md has the details.
- **`HandleEffects` (0x734550) remainder.** The visual effect object at `field_72E2` (GetDeny, `IcewindCVisualEffect` 0x555200, ReleaseDeny; 0x7345C2-0x734634) is a TODO; 0x555200 is not in src/.
- **Undeclared `CDerivedStats` members.** The two 10-entry arrays of 8-byte entries at +0x328 and +0x378 (zeroed by the constructor at 0x443CA3/0x443CB9, emptied by `ClearAll` 0x4E7050 in the constructor, `Reload` and `operator=`, copied by 0x4E7070) are not declared, so the TODOs in src/CDerivedStats.cpp skip them; the six `CPtrList`s at +0x4AC..+0x538 are declared without their element types (only the constructor, `ClearAll` and the copy touch them). Declare them with the code that reads them.
- **CD aliases outside the HD0 prefix.** On Windows `platform/fs.cpp` re-roots a drive path onto the data folder only when it starts with the `[Alias] HD0:` install prefix (other drive paths pass through natively). The CD2 alias of the vanilla copy sits under HD0, so `CDimm::CDScan` finds its folder; a CD alias rooted elsewhere would still make the stubbed `CDimm::CDSwitch` (0x7828D0) quit with exit 0 and its stderr line. Port `CDSwitch` or re-root every alias from the INI.
- **`preloadBuffers` (0x7D1EE0) `field_6C`:** the original adds the read count to it after the fill; the port sets it only with a second section, which shifts the running section offset on later refills (bookkeeping, not audio).
- **`CGameArea::SortLists` (0x4794E0) queue loop:** after the assert on a non-DELETED failed share the original still inserts the object into the queued list (`joined_r0x00479643`); ours decrements and asserts but does not insert (assert-only path). Also check whether the queued-list insertion is `InsertAfter` the first node with `y >= obj.y` (the decompile's 0x7FB367 thunk) or our `InsertBefore(pos)`; both tests queue one object, so neither distinguishes them.
- **`CAIObjectType::Decode` party specifiers (4-7, 28, 47, 48 → `CInfGame` 0x5BDFE0 nType 1-7):** the specifier → nType table is verified semantically, not against the original's table at 0x851898-0x8518A4; the `ai/decode` oracle case excludes these specifiers and the unit test has one party member. Add a two-member case (different level, damage, AC) and the specifiers to the oracle case.
- **`atol`.** The original's `atol` is VC6's. Our engine maps it only in `C2DArray::GetAtLong` and `CRuleTables::GetNextLevelXP`. Other call sites should use it too. Consider mapping `atol` in `compat/mfc.h` the way `rand` is, with oracle coverage.
- **Upper-casing.** The original upper-cases by code page (`_mbsupr`). Ours handles ASCII only.
- **`CGameSprite::CheckAranceFailure` (0x7243F0).** The original compares 16-bit values. Ours compares 32-bit values. The case needs engine state and is pending.
- **State dump coverage.** The dump lacks gender, alignment and class levels. Widen the schema, and add a screenshot comparison to `replay compare`.
- **Pending oracle cases that need engine state:** `CheckSave` (0x4A42F0), `ProcessEffectList` (0x72DE60), `Hit` (0x73B760), `Damage` (0x73D560), `C2DArray::Load` (0x402B70) and `CheckAranceFailure` (0x7243F0). See the table in [docs/evidence/README.md](evidence/README.md).

- **Sprites answer FALSE for the base's unported status triggers (M2).** `CGameSprite::EvaluateStatusTrigger` (0x731B30) falls through to `CGameAIBase::EvaluateStatusTrigger` (0x453840), which ports True, False, Global/GT/LT, GlobalTimerExpired, RandomNum/GT/LT, Exists, Dead, IsActive, Allegiance, Class, General, Gender, HP/GT/LT, Range, See, EntirePartyOnMap, AreaRestDisabled and InCutsceneMode; every other trigger (NearLocation, Race, Specifics, Alignment, the base TimerActive, ...) answers FALSE for sprites and other objects until the base port covers it (docs/evidence/453840.md lists the TODO range).

## Runner

These are in `oracle/runner/` and `oracle/loader/`.

- The original's `GetModuleHandle(NULL)` returns the runner's `hInstance`, not the exe's. Original code that loads resources by module handle would look in the wrong place.
- `crt_ranges` in `oracle/loader/vanilla.json` is not exhaustive. A C runtime variable outside the listed ranges is restored with `.data` before each case, which can desynchronise the CRT.
- Pages outside the image's sections stay read-write. Writes there are not caught.
- `WriteImage` restores the protection of one page only, so a write spanning pages with different protections leaves the later ones changed.
- `assert(lo <= hi)` in the generators is compiled out in `RelWithDebInfo`, so a reversed range is not caught in the shipped build.
- A heap-corruption break writes `complete: true` into the report, so a run cut short looks complete.
- The per-file timeout is a DWORD and can overflow for very large values.
- A `WAIT_FAILED` from the wait on the child is reported as a timeout, which hides the real error.
- The compare step labels fixed expected values as "original", which misreads in the output of cases that check against constants.
- The runner needs the x86 Visual C++ runtime on the host; `oracle.py build` copies only what the runner imports.
- A fault inside original code that holds a CRT lock leaves the lock held, and later cases in the same file could hang. The fresh process per file and the timeout contain it.

## Vanilla copy

These are in `build_vanilla` and `verify_vanilla` in `tools/oracle.py`.

- `dialog.tlk` and the 188 Override files restored from the install have no reference MD5, so a wrong file there would not be noticed.
- The `mpsave/` folder is kept in the copy.
- UTF-16 INI files and a `resolution=` key in another case are not handled by the INI preparation.
- The vanilla copy drifts across runs while its marker stays valid. Both engines write cache files and INI state into it, and only `replay compare` backs up and restores the INIs; `replay record` alone does not. `ensure_vanilla` checks only the marker (`verify_vanilla` runs when the copy is built), so a later run can start from a different state. Snapshot the writable files (the INIs and the cache) around every run, or add a drift check against the manifest the marker was written from.
  - The Linux test copy keeps the oracle's `[Alias]` paths (`HD0:=%USERPROFILE%\iwd2r-oracle\game\`), but they do not reach the Windows drive: `platform/fs.cpp` `ResolveLocked` maps every drive path into the data root, so Linux runs read and write only the Linux copy. Tests no longer write the copy: every engine-starting test runs on a private view (`tests/data_view.h`), and `tools/gates.sh` fails a preset whose tests changed the copy (a manifest of path, type, size and mtime taken before the first preset; a full `ctest --preset linux-gcc` left it unchanged on 2026-10-14). `Characters/PROBE*`, `Party.ini` and the INI files dated 2026-10-13 are from runs before the views and are left as they are. The 2026-10-14 smoke hash flip (`2096ed037cd709a7` / `609c79747c7a4a2c`) was not drift: `CScreenConnection::UpdateMainPanel` shows the day background `START` from 07:00 to 17:59 local time and `STARTN` (with a torch) otherwise. The smoke test now runs the engine under a `TZ` in which it is noon, on a view with empty save folders (`DataView::Saves::None`; a quicksave in `mpsave` enables Quick Load), so the pinned hash is the day menu with Quick Load disabled. Open: other frame-comparing tests that start the engine in real time read the wall clock too; pin it the same way (or use the replay clock) when one compares pixels.

## Recorder

These are in `oracle/replay/` and `tools/`.

- `play.py --vm linux` cannot produce replay dumps: it drives the game through xdotool and drops `IWD2R_SCRIPT`, so replay mode never leaves the menu, and `play.py --linux --headless` without the vanilla copy fails at the save step (CInfGame.cpp:4206). Our half of a comparison is only reachable through `oracle.py replay compare` (or its `run_ours`). Add a `play.py --replay <scenario>` path (or an `oracle.py replay ours`) that runs our replay half against a recorded scenario on Linux.
- The recorder drops a door whose name or flags fail to read, while our dump always includes it; emit the door with null values or log the drop so the two sides cannot disagree silently (M2 Task 1 review).
- The dump invariant (`recorder.cpp`) is enforced on checkpoint dumps only, not on periodic ones.
- **Check the search wait's premise in the original (M1/M2 review item 13).** Our replay mode waits after an object's update until the path search it queued is done (`ReplayWaitForSearch`, src/CGameArea.cpp; platform/replay.h), on the premise that in the original every such search is done by the requester's next update. Nothing checks that premise per recording. Make it an invariant: in `oracle/replay/recorder.cpp`, hook the original's `CGameArea::AIUpdate` (0x46E3D0) around its three per-object loops' `AIUpdate` calls (where ours calls `ReplayWaitForSearch`) so that, at the start of each object's update, a sprite whose `m_currentSearchRequest` is still `STATE_WAITING` or `STATE_PROCESSING` from its previous update counts in a new global `search_not_ready` (schema, `tools/gen_fields.py`, the recorder's dump); our engine writes 0 (the wait guarantees it). `invariant_problem` in tools/oracle.py then fails a dump whose count is not 0, on the original's side too, so a recording where the premise breaks says so instead of passing with our wait masking it. Optionally, `invariant_problem` can also fail our side when `globals.au_counter` differs from `frame` outside held frames. Needs the recorder (the vis-probe stream's file), an x86 rebuild (`oracle.py build`) and one sandbox recording of walk_and_talk and first_fight to see the count stay 0.
- `oracle.py` reads the injector's pid with a blocking `readline` that has no timeout, so a stuck injector hangs the run.
- A swallowed `WM_KILLFOCUS` skips the `SetCursor` call the original would have made.
- `g_lastAu` starts at 0, so the first stall count can be off by one.
- Loads are deterministic in the replay model (`platform/replay_clock.h` `FrameGate`): the calls a load draws from inside an AI update are held (no frame, no clock step, no timer), so `au_counter` now equals `frame` and a load costs the same AI updates on every machine (sandbox world_entry identical with every ReadFile off the main thread slowed by 1 ms). Open: a host recording of `scenarios/world_entry.txt` made after the change, to confirm the host matches the sandbox (the pre-change host recordings `runs/20261013-143300`/`-143324` differ in `ai_rng_state` from chapter on). `held_frames` still varies between machines; it is a timing field.
- `GetKeyState(VK_CAPITAL)` is not hooked, so Caps Lock state comes from the host.
- `tools/gen_fields.py` does not handle template pointers or declarations with several declarators; such fields are skipped or misread.
- `tools/minischema.py` does not handle array-form `items`, so a schema using them is not validated.
- A test that aborts inside the engine (an engine assertion, `_Exit`) leaves its `/tmp/iwd2r-view-*` folder behind: `DataView`'s destructor never runs. A normal Catch2 failure cleans up. Sweep stale views at the start of a run, or register the view for removal in the crash handler.
- The replay mode's timers and clock have no data-free test: `platform::replay::SetTimer`, `KillTimer`, `FireTimers` (including its re-arm of periodic timers) and the virtual wall clock (`WallClock`, `NowMs`) in `platform/replay.cpp` are exercised only by the Windows `replay compare`, so a regression there shows up only as a timing difference against the original. `tests/test_clock.cpp` covers the real-time clock, not these.

## Tooling

- `replay compare` reports a `known_differences.json` entry that matched nothing, but does not fail on it. An entry carries no scenario, so one valid for another scenario would look stale. It stays a warning until entries carry a `scenario` field.
- Consider a data-gated test through `CRuleTables::GetClassStringMixed` (the fighter-type string choice: Barbarian 34, otherwise 16799). The id choice is confirmed by the disassembly noted in `docs/evidence/544120.md` and exercised by the chargen replay.

## Project 4 follow-ups

- **`CVidBlitter::SoftBlt` (0x7B7280) and `SoftSrcKeyBlt` (0x7B74F0).** Still stubs. `CVidBlitter::Init` selects them only at 16 bpp (every other depth takes `DxBlt`), and iwd2r runs at 32 bpp, so nothing reaches them; porting them needs `IDirectDrawSurface::GetSurfaceDesc` in `platform/ddraw_shim.h` and two `DDSURFACEDESC` members in `CVidBlitter.h`. Ruling P4-9.
- **Real-GPU runs in a Hyper-V VM (GPU-P).** Windows Sandbox shares the host GPU through GPU partitioning but picks the adapter itself (the AMD iGPU on a hybrid machine, no Vulkan), so `Presenter=gpu` never runs on the RTX there. A Generation 2 Hyper-V VM under `%USERPROFILE%\iwd2r-vm` with `Add-VMGpuPartitionAdapter -InstancePath <RTX>` and the host's NVIDIA driver store copied into the guest runs our engine on the real GPU with real Vulkan, driven headlessly over PowerShell Direct (`Invoke-Command -VMName`) like the sandbox driver. Needs one admin setup by the user (Hyper-V is present on this host: Windows 11 Pro, vmms running). Planned as `play.py --vm hyperv` after project 4 Task 6.
- **A free Xvfb display across `play.py` processes.** `tools/realinput.py`'s `start_xvfb` picks the first free display number under a lock that only covers the threads of one process. Two `--vm linux` runs started at the same moment from different worktrees can choose the same number; the second Xvfb exits, but `x_display_ready` may already see the first one's server, and both games would run on one display. Pick and start under a `tools/filelock.py` lock (or check that our Xvfb is still alive when the display answers) so parallel streams can run `--vm linux` at once.
- **D3D12 and Metal shader blobs.** The GPU presenter asks SDL for `SDL_GPU_SHADERFORMAT_SPIRV` only, so it runs on Vulkan alone: a Windows machine without a Vulkan driver, and macOS, get `classic`. Compiling the shaders to DXIL and MSL as well (`tools/shaders.py`, with SDL_shadercross or the vendor compilers) and passing every format SDL accepts would bring the presenter to D3D12 and Metal.
- **`FillPoly`'s edge-table cache (0x7C1DE0) is keyed by the vertex list's address and count.** A WED freed and another loaded whose polygon list lands at the same address with the same vertex count takes the old polygon's edges, in the original as in ours (kept; docs/evidence/7C13A0.md). Sixteen entries are held. If wall occlusion ever shows a stale shape after an area change, flushing the cache when an area's WED is released would bound it.
- **The world screen at Scale 2, World Scale 1 has not been seen running.** Before UI scaling counts as done, `scenarios/world_entry.txt` must run at 1920x1200 (or 2560x1440) with `Scale=2` and `World Scale=1`, with shots showing the world at 1x and the UI at 2x. The panels themselves are checked: every GUIW08/GUIW10 panel lies inside `WorldPanelRects`' band for the viewport it is shown over (`world_panels_lie_in_the_band` in `tests/test_layout.cpp`, the CHU rectangles read with `tools/bif.py`); the world screen's UI manager holds only GUIW's ten panels and no control lies outside its panel, so the quick weapon, item and spell choices are drawn by the action bar's (panel 1's) own controls, not by a popup above it.
- **The default stays `Presenter=classic`** until the gpu presenter has run on a real GPU under Windows (ruling P4-13; the Hyper-V GPU-P run above is that run). `auto` behaves as `gpu` once the probe passes.
- **User check of the gpu presenter on real Windows Vulkan (P4-5).** Not yet done: the probe's `presenter: gpu probe ok` line, the swapchain claim on the game's window, Ctrl+Shift+R back to classic on the same HWND, alt-tab, minimise and a high-DPI display.
- **Stub verification (M2/M3).** `replay compare` compares dump fields, not pixels. Add a framebuffer hash or a screenshot comparison, and scenarios that reach wall occlusion (WED wall polygons with a sprite behind), translucent and 0x200 cells, palette range effects, lightning weather and the message screen, so that `FXRenderClippingPolys`, `RenderMessageScreen`, `PolyLine`'s 32-bpp body, `DrawLightingEffect32` and the cell blends get runtime coverage. Any shot comparison must mask (or compare with a tolerance) the six start-screen shots' animated box at x 78-132, y 360-414, which differs between any two runs, including two runs with identical settings.
- **The cell blends' bound** reads the current `CVidInf`'s `m_SurfaceDesc` instead of taking the extent from `BKRender`/`FXRender`, which know the surface they draw into.
- **High-DPI mouse mapping in the gpu presenter.** The whole-frame mapping (`GpuBackend::WindowToFrame`/`FrameToWindow`, `platform/present_gpu.cpp`) scales by `SDL_GetWindowPixelDensity`, while drawing uses the swapchain-to-window size ratio; the two agree only while the swapchain is the window's pixel size. Untested on a high-DPI display; use the drawing's ratio in both.
- **The classic presenter's NEAREST outlives its region list.** A composition region sets the texture's scale mode to nearest (`present_classic.cpp`, `DrawRegion`), and nothing restores linear when the list empties at Scale 1; it only shows when the window is not the frame's size (a resized window, high DPI). Restore the mode the texture was made with when the whole-frame path draws.
- **A rebuild budget per time window.** A run allows one device rebuild; a lost-again device falls back to classic for the rest of the run, and a new window or the hotkey does not bring gpu back (fixed in the project 4 review round: the fallback is sticky). Two genuine acquire failures hours apart in a long session therefore leave classic for good. Allow one rebuild per time window (say one per ten minutes) instead.
- **Untested region edges.** The region matrix test in `tests/test_layout.cpp` skips the vacuous pairs (equal scales); the release-build fallback in `ScreenLayout.cpp`'s `RegionsFor` when a world screen's regions do not tile is unreachable with the current panels and untested; `play.py`'s `window_point` ignores the letterbox offset in a resized window.
- **The mouse mapping off the main thread.** `SetRegions` and `Present` run on the main thread, but `GetCursorPos` maps the mouse from the AI thread too (`CChitin::AsynchronousUpdate`). The region list is a locked snapshot; the backend's whole-frame mapping (`SDL_RenderCoordinatesFromWindow` in the classic backend, the last frame's placement in the gpu one) is read without a lock while the main thread may be replacing the backend (the hotkey, a new window). Guard the backend pointer, or cache the mapping in the front under the same lock.
- **Negative letterbox offsets.** When the window is smaller than the frame times the world scale, `RegionsFor`'s centring offsets go negative: the regions crop at the window's edges while the whole-frame path scales the frame down. Untested.

### Render API

The guards in `src/render/api.h` leave these for the renderer that first draws the world through a backend.

- **Tiles.** `CVidTile::Render` only fills the VRAM tile cache on a miss, at (0, 0), without the stencil tile or fog. Each frame's tiles reach the screen through `CInfTileSet::Render` (0x5CB4A0) and `CInfTileSet::RenderBlack` (0x5CB630). A screen-level renderer must guard those two, with the tile, the destination and the stencil.
- **FX groups.** FX draws arrive in FX-surface coordinates. The placement comes from `FXPrep` (ptPos, ptReference, mirror flags 0x10/0x20) and the composite from `FXUnlock` and `CInfinity::FXBltFrom`. A backend needs begin-group and end-group calls at `FXPrep` and `FXUnlock`. `CInfinity::FXRenderClippingPolys` (0x5CE350) writes the wall-occlusion mask into the FX surface between them with `FillConvexPoly`/`FillPoly`, unguarded; the group calls must carry it too.
- **Pointer.** `RenderPointer` draws its cell to the front surface in screen coordinates, and it reaches the backend as an ordinary `DrawCell`. Add a `DrawPointer` primitive, or pass `nSurface`.
- **Frames and layers.** Nothing in the engine calls `BeginFrame`, `EndFrame` or `SetLayer` yet.

## Creature construction and spawning (from project 3, milestone 2)

- `CGameSpawning::Spawn` (0x4C9CE0) is checked against the disassembly and a Catch2 budget table, but not against the original at runtime: walk_and_talk's AR1000 has no ARE spawn points. Untested paths: the kept bug where a rejected creature's power stays in the sum (needs a forced rand pick of an over-budget creature), a visible chosen square under an invisible point, spawn-once before `field_136` is set, and the BYTE wrap of the party level sum. A partial-fixture oracle case could cover the gates and the `m_maxTypeNum` 0 path (oracle case `spawn/spawn_point` is pending).

## Load-time equipment and AI rand parity (from project 3, milestone 2, rng round 2)

- Faithful ports that change default gameplay widely, so new crashes or gameplay differences may trace back to them: the weapon-set switch `CGameSprite::sub_726810` (0x726810) now runs at every party load, on every weapon-set click in the inventory, and for every non-PC creature loaded from a CRE; the area now counts the party (`m_nCharacters` compared with 0xFF), so the visibility branches run; `CGameArea::OnActivation` stops the world timer when the world is not paused; creatures' `m_active` follows the time-of-day mask, so they appear and vanish by hour.
- `byte_8D86B0` is a file-static in both src/CGameSprite.cpp and src/CItem.cpp; the original has one never-written global. Both copies stay zero, so they cannot disagree, but one shared extern would be cleaner.
- Weaker tests: the `m_active` test restates the formula; the trigger test skips the proximity-trap path (type 0, ENTERED/GetDeny); the weapon-set test never covers the launcher path; `CGameArea::OnActivation`'s fix has no unit test (the headless sound start segfaults) and is covered only by the replay's rand trace.

## Creature animation and items (from project 3, milestone 3, items-anim)

- Until `CGameEffectColorTintSolid::ApplyEffect` (0x4A7000) and `CGameEffectColorLightSolid::ApplyEffect` (0x4A71B0) advance their range pointer as the original does (0x4A7100), loading 00SSCYC or 53DRACAH allocates without bound. The fix is routed to the effects-core stream. Run creature sweeps one creature per process under the memory cap (`tests/test_cre.cpp` `[.][cre-sweep]`).
- 15 sampled creatures assert in the base `CGameEffect::ApplyEffect` until effect opcodes 261, 98, 206 and 429 are ported (effects streams).
- `CGameAnimationTypeMonsterIcewindNew` was compared with the original only where its constructor reaches (`Render`, `SetSequence` and the rest of `ClearColorEffects` were not); other animation files pass `bWarningIfMissing` TRUE everywhere, where the original passes 0 for some cells.
- `CItem`: `m_nAbilities` is zeroed in the copy and CRE-record constructors (the original leaves it unset on a missing item) but not in the resref constructor 0x4E7E90; make it consistent. The destructor test reads fields after `~CItem`, which is undefined behaviour and would break under LTO. The 10GUTHE/12OGR test only checks that construction succeeds, not the palette type, weapon cell or mirror test.

## Effects (from project 3, milestone 3, effects-core and effects-iwd)

- Polymorph (135, druid wild shapes), ReplaceSelf (151) and FeatCleave (442) are not ported; each prints a one-shot `iwd2r: effect N (...) is not ported` warning. No test covers the warnings.
- `CGameEffectApplyEffect` (177) and the two equip-item effects (182, 183) are ported, but the vanilla data has no EFF files, so their decode path and most of 177's mode matching are verified only against the disassembly.
- Spell immunity (206/290) waits for the shared `CDerivedStats` immunity set from the stats-spell stream; opcodes 3, 12 and 440 wait for its shared `GetSpellResrefEffects`.
- Dominate (263) and Summon (411) are not ported; their blockers are in docs/evidence/5645A0.md and 55F710.md.
- Effect 233 (visual spell hit, the most common opcode) queues `CMessageFireProjectile`, whose `Run` (0x5007F0) is being ported by the projectiles stream; until then the queued message asserts.

## Derived stats, spells and projectiles (from project 3, milestone 3)

- Stoneskin (218) and other damage reductions: when the source effect leaves without its pool running out (dispel, `RemoveAllOfType`, expiry), the `m_damageReductions` entries keep pointers into the deleted effect until the next `ProcessEffectList` rebuild; a hit or a character-screen lookup in that window touches freed memory, as in the original. Ruling M3-10 only bounds the exhausted-pool path. A faithful-in-play fix would clear matching entries when the effect is removed.
- Not ported: `CGameEffect::CheckSave` (0x4A42F0), the only reader of `m_spellPenetrationBonus`; the persistent effects' TODOs (0x4C1C00, 0x4C21E0, 0x4C2CB0, 0x4C3280); the derived-stats arrays at +0x328 and +0x378.
- Projectiles: the decoder 0x51EAF0 sends about 380 unported types to the instant projectile (a temporary DEVIATION): effects land on the Fire frame and later object ids shift, so a combat replay diverges from the first unported projectile fired. `Render`, sparkle, the visual-effect and area-explosion objects are not ported (projectiles are invisible). Spell-hit types 45, 52, 57 and 72 return NULL (see the warning in `CProjectile.h`). The cone projectile 0x579B40 is missing, so effect 293 stays blocked.
- Effect TODOs now wirable with the merged projectile classes (src/IcewindCGameEffects.cpp): 233, 235, 255, 256, 264, 449, 265, 294, 295, 417 (types 0 and 2) and 430 (partly).

## Resting, stores and import (from project 3, milestone 4, rest-store)

- Rest encounters: `CGameArea::CheckRestEncounter` (0x46FEA0) is not ported; `RestPartyPrivate` has a TODO where the original calls it, so a rest is never interrupted and draws none of the encounter's `rand()` calls.
- The Rest action handler `CGameSprite` 0x7500F0 (spell memorization refresh after a rest) is not ported.
- `CanRestParty`'s last check, `CGameObjectArray` 0x5C9300 (no resting while any sprite has spell state 46 or 47), is a TODO that always passes.
- Store and character files now load (their byte-reversed signature checks were fixed), and so do EFF and cell-header resources. The three apply-EFF effects (177, 182, 183) now apply EFF files and draw `rand()`; an EFF holding an effect id `DecodeEffect` does not know dereferences NULL, as in the original.
- `CBaldurChitin::AllocResObject` has no special class for resource types 1021 and 1022 (the biography); `ImportCharacter` works around it.

## Chargen multiplayer branch (from the 2026-10-30 import fixes)

- **OnDoneButtonClick (0x615F50) lacks its multiplayer status calls.** The exe calls `CMultiplayerSettings::GetCharacterStatus` (0x518B60) and `SignalCharacterStatus` (0x518BA0) on the Done path; our body has neither (parity asm, waived as pre-existing in docs/evidence/615F50.md). Single-player is unaffected.

## Save and load screens (from project 3, milestone 4, save-screens)

- `CMoveList::Marshal` (0x5170F0), now called by `CInfGame::SaveGame`, still has a TODO to add the sprite id to the area's `field_AAA` (src/CMoveList.cpp); until it is ported a creature with a pending move is not saved with the master area.
- `scenarios/pending/save_load_screens.txt` saves and loads through Options headless but has no checkpoints yet, so a replay compares only the exit code.
- `CVidInf::PrintSurfaceToBmp` reads the DirectDraw surfaces; if the render API's GPU backend ever presents normal runs, screenshots would come out empty. Its 3d variant (0x7BE5A0) is not ported because iwd2r refuses the 3d mode.

## Effect wiring (from project 3, milestone 3, effect-wiring)

- The cone projectile's Fire (0x579EF0) reproduces the original's x87 arithmetic in doubles in the same order. Nothing sets `-ffp-contract=off`, so on platforms whose compilers fuse multiply-add (aarch64 with GCC or Clang) a corner or puff point can move by one unit. Set it project-wide before any non-x86 build is compared with the original.
- `DecodeProjectile`'s 14 cone cases can now call the cone projectile (0x579B40) instead of the instant fallback; 430 still takes the fallback for most types.
- Shroud of Flame's EffSoF1/EffSoF2 path (index >= 1) has no test yet; it is reachable now that `StartSpriteEffect` asserts only on a failed allocation.
- `Ftol` is duplicated in src/CProjectileSingleTarget.cpp and src/CAIGroup.cpp.

## Scalable text (after `Text Renderer=scalable`)

The crisp-text requirement is met by `Program Options/Text Renderer=scalable` (spec docs/superpowers/specs/2026-10-14-iwd2r-scalable-text-design.md, README's display section). What it leaves:

- **Tooltips keep the bitmap font.** `CInfToolTip::RenderText` draws through `CVidInf::FXTextOut` into the FX surface, which `FXBltToBack` composites onto the back buffer at a place the call does not carry. The capture declines it (stderr says so once). Capturing it needs the FX begin-group and end-group calls (the FX groups item under the render API) or a notification at `FXBltToBack` with the FX rectangle and its destination, with evidence; the tags would then be written into the FX surface (only where the pixel is not the colour key) and the glyphs moved by the blit's offset.
- **Proportional fit.** Each TrueType glyph is centred in its bitmap cell (narrowed when wider), so spacing follows the bitmap font's advances: words look more widely spaced than the face's own metrics. A later option could fit each run's glyphs proportionally inside the same total width.
- **The pointer and the glyph's reach.** Inside its cell a glyph shows only where its tag survives, so the pointer and anything drawn over it hide it exactly; the part of a TrueType glyph above or below its bitmap cell (an i's dot, a descender) is drawn wherever the draw's clip allows and can cover the pointer there.
- **Golden frames at Scale 3.** `text_golden_presented_frames` and `text_golden_presented_frames_gpu` pin the presented main menu, character generation and gender page at Scale 2 per machine in `<IWD2R_HOME>/text-golden` (`name hash` lines; with the file present a missing or different hash fails). Headless runs present at most the dummy and offscreen displays' 1024x768 (the engine fits its window to the desktop), so Scale 2 is shown as a 1.28x stretch and Scale 3 is out of reach; it needs a larger virtual display. The overlay's own scale handling is covered at 1-4 by `tests/test_text_overlay.cpp`. A dialog golden needs `scenarios/pending/world_entry.txt` to move out of pending.
- **The overlay is recomposed whole when any glyph changes.** Static screens cost nothing (the composed image is reused), but a screen with a lot of text and something changing every frame (hit points over moving creatures) recomposes every visible glyph each frame on the CPU. Tile the overlay and recompose only the tiles whose glyphs changed if a profile shows it.
- **Translucent and brightest blends keep the tag.** `CVidCell::sub_7D0950` (0x7D0950, translucent) and `sub_7D1190` (0x7D1190, brightest) write RGB and keep the destination's X byte, so a translucent sprite or effect drawn over text leaves the tag and the scalable text shows crisp on top of it, where the bitmap text would show through dimmed.
- **The layer's bookkeeping.** A glyph completely hidden under the pointer for longer than the forget delay (1800 presents, only while the layer holds more than 4096 glyphs) is dropped; its tags come back with the pointer's save-under but nothing draws them until the control redraws. Keeping glyphs whose tag is still in the back buffer does not help (the tags are in the save-under surface then). `Layer::Add` scans every live glyph for overlaps (O(n) per character) and every present scans every glyph's box; a grid of glyphs by cell would make both local.
- **Out-of-cell parts ignore tags.** The part of a TrueType glyph outside its bitmap cell (an i's dot, a descender) is drawn wherever the draw's clip allows, without a tag test, so it can paint over whatever covers that spot (the pointer, a panel drawn later).
- **TOOLFONT draws with Crimson Pro**, the body face, not the sans the spec suggested for small labels: a sans looked out of place next to the bitmap serif in the side-by-side screenshots.
- **Glyphs are centred in their cell** (the advance's slack split on both sides), not left-aligned as the spec says: the same spacing between neighbours, half the slack less at a run's start.

## Character, spellbook and options screens (from project 3, milestone 5, screens-char)

- Blocked stubs in src/CScreenCharacter.cpp: `UpdateEquipmentInformation` 0x5DE2C0 and the character text export `sub_5E4560` need CGameSprite 0x723EA0, 0x73CF10 and 0x73D080, CRuleTables 0x547020 and CItem 0x4E9840 (all but 0x723EA0 are ported on p3/combat, none on first); their comments name them. (`CheckDropSlot` 0x5E6A30 is ported.)
- `CGameSprite.h` lacks `BYTE field_7282[11]`, so level-up always rolls hit points; the original skips that roll (and its `rand()`) in one rare drained-level case. No test pins level-up's rand count.

## Inventory, containers and stores (from project 3, milestone 5, screens-items)

- TODOs left in the item-use paths: `CGameSprite::CheckQuickLists`, `CheckIfUsedUp` and 0x723830 are not called yet.
- The container slot handler 0x6957E0 needs `SwapItemGround` before dropping onto the ground works as in the original.
- Container, bag and temple buy paths have no tests yet.
- The area-move test uses AR1001 because loading AR3000 aborted (see the persistent-effect resref fix).

## Persistent effects and spawn lists (from project 3, milestone 3, crashfix-regen)

- `CGameEffectDisease::ApplyEffect` uses an uninitialised `pEffect` after `UTIL_ASSERT(FALSE)` for an unknown mode. This is the original's bug, kept as is.
- Running several engine test cases in one `iwd2r_tests` process can crash at random, because each test file starts its own engine (seen with `[effects]` across test_effects and test_effects_iwd, and with the movement rounding test). ctest and the gate run one process per case, so they are unaffected.
- The AR1001 stand-in in the area-move test (screens-items) can go back to AR3000 now that AR3000 loads.

## UI controls and visual systems (from project 3, milestone 5, ui-misc)

- `CGameSprite::CheckQuickLists` (0x71ABA0) is ported and called by spontaneous casting (0x716770), the inventory, spellbook and level-up screens, Swing (0x739B60), the three refreshes of the Spell action (0x740270) and `sub_75E330` (M1/M2 review fix round).
- `CScreenWorld::OnKeyDown` (0x6872D0) is ported (ui-misc round 2) but for two TODOs: the cheat keys (0x68956A-0x68BC5D, Ctrl with `field_156`, which nothing sets) and the keymap's spell keys 53-371 (0x6893FD-0x689502: CGameSprite 0x719180 and the shared table at 0x8B4EB0). Keymap 21 now calls the ported `CGameSprite::EquipRanged` (0x75E150) as at 0x6893E8.
- The text character sheet export (`CScreenCharacter::sub_5E4560`, 0x5EFFD0) ends in the original's assert at the spell lists (it passes a list index where `CGameSprite::GetSpells` wants a class); ours keeps it (ruling P4-10). No vanilla button starts it (GUIREC panel 2 has no control 51).
- Movie subtitles (`CBaldurProjector` 0x43E300, 0x43EA70, 0x43EC20, 0x43EED0) are not ported: Bink playback needs binkw32 (absent on Linux) and the subtitles blit DirectDraw surfaces.
- The Lua console's Enter (0x77BF30, `CLUAConsole` 0x5838A0) is not ported; iwd2r has no Lua runtime.
- Character generation reports no Ctrl (its vtable's GetCtrlKey is CWarp's) and ignores Enter in the biography popup, in the original too: the biography edit has no Ctrl shortcuts or line breaks there. The character screen's and the journal's edits have them.
- `CDerivedStats`' arrays at +0x328 and +0x378 and its six lists at +0x4AC to +0x538 are still undeclared; only the constructor, ClearAll and the copy touch them.
- `CBaldurMessage::SendMapWorldAnnounceStatus` (0x4354E0) is still a stub that returns FALSE. `CGameSprite::LeaveArea` (0x74C0D0) calls it in a single-player game too, because a single-player game is a host session. It sends the world map's state to the other players, so with no other player nothing is lost; port it with the rest of CBaldurMessage's announcements.
- Area travel (ui-misc round 4) leaves these parts of `CBaldurMessage` and `CInfGame` 0x5C4AD0 as NOTEs because they need a remote player: the client's request in 0x438710, the message 0x438F90 to the other players, the host's wait for slow clients (0x5C5FEE-0x5C686A), the clients' sprite updates, and a client's own wait.

## Combat and spell casting (from project 3, milestone 3, combat)

These block TODOs in the combat stream's ports; each is in a file or function the stream was not granted.

- `CGameSprite` 0x71F860, the ammunition's and launcher's effects on a missile's projectile, is not ported: Swing's missile branch fires without them.
- **spell_showcase friendly-fire residual.** Party hp at cast_done reads Aria 1/4, Bram 3/4 vs the original's 0/0, with enemy hp and positions identical value for value; most plausibly the last Magic Missile still in flight at cast_done (fight_done follows 150 frames later). Details in the Task 44 report (first/.superpowers/sdd/2026-10-23-frida-imports/task-44-report.md).
- `CGameArea` 0x46E130, which gives an effect to the area's creatures (target types 4-8), is not ported: FireSpell, FireSpellPoint, ForceSpell, ForceSpellPoint, Spell, Swing's melee effects and 0x755A70 drop those effects (TODOs).
- The projectile class DecodeProjectile builds for type 304 is not ported, so it has no member at +0x356: the five spell and missile paths draw the type-304 `rand() % 1000000` for the fire message but do not store it in the projectile (TODOs).
- The first_fight scenario ends at its `fight_done` checkpoint, so nothing after the fight is covered: loot pickup, corpse fade and the area's next spawn. A later scenario that runs past the fight (M4/M5) should pin them.
