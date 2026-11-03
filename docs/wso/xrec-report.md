# xrec report — first cross-recovery comparison (2026-10-17)

Run from the repo root:

    python3 -I tools/xrec.py 4fa36bcd wso/main HEAD --json docs/wso/disagreements.json

201 shared src/*.cpp files (14 theirs-only, 20 ours-only); 8,661 functions
classified.

| category | functions |
|---|---|
| same | 4,424 |
| same_stub | 2,005 |
| only_theirs | 413 |
| only_ours | 277 |
| theirs_real_ours_stub | 33 |
| ours_real_theirs_stub | 88 |
| theirs_changed_ours_base | 592 |
| ours_changed_theirs_base | 287 |
| both_changed_differ | 529 |
| both_stub_diff | 13 |

Theirs-only files (whole-file imports, wave 5; 159 real functions across
them): AutoLoad.cpp, AutoUI.cpp, CGameFireball3d.cpp, CGameTemporal.cpp,
CMessageDoorOpen.cpp, CProjectileSPBOOM.cpp, CScreenMultiPlayer.cpp,
CSparkleCluster.cpp, CVisualEffect.cpp, DebugLog.cpp,
IcewindCGameAnimationTypeEffect.cpp, IcewindCProjectileTargetMap.cpp,
MfcCString.cpp, TString.cpp.

Against the pre-task script (the spec's table): matching by `// 0xADDR`
before name collapses 682 one-sided rows (746/609 into 413/277) into 356
pairs — the trees' name drift hid real disagreements. both_changed_differ
grows 406 to 529, theirs_changed_ours_base 528 to 592, identical real bodies
4,337 to 4,424.

The rider 22960d3b (base looked up by address in the name loop too) changes
nothing in the recorded run: against base 4fa36bcd, rerun with the pre-rider
tool at ten ours refs (HEAD, the base itself, and eight branch and side
refs), the JSON is byte-identical. Base
stamps 0x4B0050 on two functions (`CGameEffectResistElectricity` and
`CGameEffectResistFire::ApplyEffect`) and 0x5ED130 on two
(`...CharacterScriptScripts` and `...CharacterExportCharacters::OnItemSelected`),
so neither address is a unique key and the lookup falls back to the by-name
one the rider replaced; base's single 0x44DC10 stamp is the `ExecuteAction`
stub, which puts `CGameAIBase::StartCutScene` and `IncrementChapter` in
`both_changed_differ` under either lookup (base has neither function by
name). The same four rows do move `theirs_changed_ours_base` →
`both_changed_differ` if the comparison is run against base 8ce297d7
(iwd2r/main): there 0x4B0050 and 0x5ED130 carry a single stamp each, the
address lookup engages and returns bodies matching neither tree
(`CGameEffectResistFire::ApplyEffect`, `CGameAIBase::StartCutScene`,
`CGameAIBase::IncrementChapter`,
`CUIControlTextDisplayCharacterScriptScripts::OnItemSelected`). That base is
this branch's own src state — against it no row is
`ours_changed_theirs_base` or `both_changed_differ` at all — so it is not
the comparison recorded here; the table keeps base 4fa36bcd, the base this
report documents.

## Spot check (20 rows, mandated by the spec)

Method: an independent script (kept in /tmp, not the repo) extracted each
sampled function from `git show` of both trees by its `// 0xADDR` comment
line — the tool itself finds functions by signature regex — brace-matched
with string literals skipped, stripped comments with a separate state
machine, reclassified with the ten rules, and compared against the row.
Result: 20/20 agree, every body length reproduced exactly. The check also
caught the address match doing its job on a real rename:
`GetSpellAbilityValue` (theirs) is `GetSaveAdjustmentForSourceSprite` (ours)
at 0x547040, paired as both_changed_differ instead of two one-sided rows.
The largest row, `CUIControlBase::CreateControl` (95,790 / 90,825 chars), is
a real ~3,300-line function with ~2,000 case labels in both trees, not a
brace-matching runaway.

The check caught one misclassification. In src/CScreenMap.cpp their tree
stamps `// 0x643CC0` on two functions — `Render` (real) and `RenderFogOfWar`
(TODO stub) — ours only on `Render`; the tool paired their stub with our
real body under `Render`'s name. Such duplication is common: 120
(file, address) pairs are stamped on more than one function of a file in at
least one tree (placeholder stamps: 0x49FC40 on small getters across the
screen files, 0x40A0E0 on 104 CMessage functions). Fix: an address two
functions of one tree carry is not used as a key; those functions match by
name. Regression test `test_xrec_ambiguous_address_pairs_by_name`; the
affected row now reads `Render` same 1452/1452 and `RenderFogOfWar`
ours_real_theirs_stub 2/2710. Counts above and the 20/20 result are from the
fixed tool.

## JSON

docs/wso/disagreements.json: one row per function,
`{"file", "name", "address", "category", "their_len", "our_len"}`, 8,661
rows, sorted by category then total body length descending (file, name
tiebreak); 7,847 rows carry an address. Category counts match the table
above; the sort order was verified programmatically.

## Scorecard

Per-side tally across every batch in docs/wso/wave-state.json, split by deciding evidence kind (regenerate with `python3 tools/wave.py scorecard`):

Decided for the fork: 15 (losing kind: oracle 2, replay 1, read-only 12) -- deciding evidence: dump reading 10, trace 5; measured: probe 0, frida 5, replay 0, none 10
Decided for ours: 604 (losing kind: trace 604) -- deciding evidence: replay checkpoint 47, dump reading 551, trace 2, triage 4; measured: probe 0, frida 114, replay 0, none 490
Decided for neither: 12 -- deciding evidence: replay checkpoint 2, dump reading 8, trace 2; measured: probe 0, frida 1, replay 0, none 11
Deciding evidence across all 631 verdicts: oracle probe 0, replay checkpoint 49, dump reading 569, trace 9, triage 4

Batches: b001 closed (20 rows), b002 closed (20 rows), b003 closed (20 rows), b004 closed (20 rows), b005 closed (20 rows), b006 closed (20 rows), b007 closed (20 rows), b008 closed (20 rows), b009 closed (20 rows), b010 closed (20 rows), b011 closed (20 rows, pool claude), b012 closed (20 rows, pool claude), b013 closed (19 rows, pool glm), b014 closed (18 rows, pool glm), b015 closed (20 rows, pool glm), b016 closed (20 rows, pool glm), b017 closed (20 rows, pool glm), b018 closed (19 rows, pool glm), b019 closed (17 rows, pool glm), b020 closed (19 rows, pool glm), b021 closed (20 rows, pool glm), b022 closed (20 rows, pool glm), b023 closed (20 rows, pool glm), b024 closed (3 rows, pool glm), b025 closed (8 rows, pool glm), b026 closed (8 rows, pool glm), b027 closed (19 rows, pool glm), b028 closed (20 rows, pool glm), b029 closed (4 rows, pool glm), b030 closed (17 rows, pool glm), b031 closed (12 rows, pool glm), b032 closed (4 rows, pool glm), b033 closed (11 rows, pool glm), b034 closed (16 rows, pool glm), b035 closed (10 rows, pool glm), b036 closed (10 rows, pool glm), b037 closed (9 rows, pool glm), b038 closed (9 rows, pool glm), b039 closed (9 rows, pool glm), b040 closed (10 rows, pool glm)

imports: stubs 22 imported / 0 kept / 17 parked; base-body 12 imported / 171 kept / 394 parked; only-theirs 114 imported; 1 kept / 119 parked; 41 kept by default

probes: resolved 4, on list 20

Coverage: 613 of 1598 candidate functions exercised by 12 scenarios (2026-10-26)

Wave 1 rows without an address (not batchable): 114

Calibration, computed from triage.json classes over closed verdicts:

class           agree total  status
rename-only         1     1  enabled
signature-only      3     3  enabled
fork-stub           2     2  enabled
ours-stub           0     0  never enabled
ambiguous         594   621  never enabled
    mismatches: 0x5FDB20, 0x600B50, 0x63C940, 0x65BCF0, 0x704D40, 0x737EF0, 0x738E70, 0x73B760, 0x5FD0A0, 0x614EF0, 0x63BE80, 0x67B8A0, 0x708280, 0x72FD20, 0x67E560, 0x5D3A80, 0x7941D0, 0x7A3FD0, 0x478490, 0x5D79B0, 0x671FC0, 0x477EE0, 0x734550, 0x73CD00, 0x5E4E80, 0x615F50, 0x742840

Truth calibration (seed 2026-10-22, sample 0x58FF20, 0x5DE2C0, 0x62F360, 0x7338E0, 0x46B160, 0x54C770, 0x4BBCB0, 0x5ABA20, 0x71FC00, 0x6175A0, tools/truth.py over the 18 ruling rows with side fork or neither plus a sample of 10 dump-decided ours rows; a path measures a row when it gives pass/fail (oracle) or equal/different (Frida) and agrees on pass or equal): oracle probes: measured 0 of 28 rows, agreed 0, disagreed 0; Frida trace diff: measured 13 of 28 rows, agreed 12, disagreed 1; Frida on by default: yes.

Audit sample (rule 3): seed rule3-2026-10-23 over 199 dump-decided ours rows at 9fad8484; 10 drawn: 3 probed (replay 2, oracle 1), 7 parked, 0 flips.

## Triage calibration

Every auto class is checked against the verdicts recorded in the closed batches (b001-b010). A class is enabled only when every recorded verdict agrees (side ours) and there is at least one. Triage ran over our tree at 82d75232, the commit that recorded the disagreement list, because HEAD has since adopted the fork bodies for the resolved rows. Generated by `python3 -I tools/triage.py --calibrate`.

```
class           agree total  status
rename-only         0     0  disabled (no verdicts)
signature-only      0     0  disabled (no verdicts)
fork-stub           2     2  enabled
ours-stub           0     0  never enabled
ambiguous         180   198  never enabled
    mismatches: 0x5FDB20, 0x600B50, 0x63C940, 0x65BCF0, 0x704D40, 0x737EF0, 0x738E70, 0x73B760, 0x5FD0A0, 0x614EF0, 0x63BE80, 0x67B8A0, 0x708280, 0x72FD20, 0x67E560, 0x5D3A80, 0x7941D0, 0x7A3FD0
```
