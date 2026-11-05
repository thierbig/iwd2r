# Decompiler evidence

Every engine function a change touches needs an evidence file here: the
original's decompilation next to our port, and either an oracle case that
compares the two at run time or a reason why none is needed.
`tools/check_evidence.py` enforces it in CI.

## Workflow

1. Change a function under `src/` that carries a `// 0xADDR` comment.
2. Draft its evidence from the labelled Ghidra project
   (`tools/ghidra/make_project.py` creates it):

       python3 tools/oracle.py evidence 0x5845D0
       python3 tools/oracle.py evidence IcewindMisc::Roll

   A name shared by several functions (overloads) lists their addresses;
   run the command again with one of them. An existing file is kept unless
   `--force` is given.
3. Fill in `oracle_case` (or replace it with `exemption`), the `kind` and the
   Notes. Leave the generated decompilation as Ghidra wrote it.
4. Check before pushing:

       python3 -I tools/check_evidence.py --base origin/main

## Format

The file name is the address in upper-case hex without `0x`:
`docs/evidence/5845D0.md`. It starts with a front-matter block of simple
`key: value` lines (quotes around a value are optional):

    ---
    address: 0x5845D0
    name: IcewindMisc::Roll
    oracle_case: rand/roll
    kind: oracle
    ghidra_version: 12.1.4
    ---

- `address`: as in the `// 0xADDR` comment of the source, upper-case hex.
- `name`: the function's full name in the Ghidra project.
- `oracle_case`: the oracle case that compares this function with the
  original, or instead
- `exemption`: a short reason no case is needed, such as `"pure data"`.
- `kind`: how the function is evidenced — `oracle` (its `oracle_case` runs in
  the gate), `replay` (a replay scenario or compare run covers it),
  `read-only` (nothing runs it; the exemption stands on its own) or `trace`
  (a run trace covers it; the kind the WSO fork's imports carry). A
  `trace` document's `trace_source` cites where the trace came from, in one
  of exactly three forms, and the gate rejects a `trace` without one:

      WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md
      frida <probe> orig:<sha256> ours:<sha256> compare=0
      emu c<addr> orig:<sha256> ours:<sha256> at:<commit> sets=<n> equal=<n>

  The first is the fork's recovery note, never one of his source files: the
  gate reads it at that sha with `git show` (the fork must be fetched; an
  unreadable note fails) and requires the function's address in it (hex,
  any case, with or without `0x`) or its name as a whole word together with
  its class's name. The second is a Frida differential capture of our own
  binary against the original, with the sha256 of the two trace files; only
  the wave driver's Frida phase writes it (in an import batch, a frida line
  the driver did not write is a check problem). The third is a
  differential-emulator case (`tools/emu/run.py`): the case module's
  address in lower-case hex without `0x` (`c5845d0` names
  `tools/emu/cases/c5845d0.py`), the sha256 of the original and of our
  rebuilt exe, the commit our exe was built at, and the set counts — an
  all-EQUAL run over every input set is a measurement, so the gate requires
  `n >= 1` and `sets == equal`; `oracle.py emu --write-evidence` writes it.
  A `trace` whose source
  checks out needs neither `oracle_case` nor `exemption`.
  The checker reports counts per kind but does not require one — except for
  the rules-math set (`RULES_MATH` in `tools/check_evidence.py`: attack rolls,
  saving throws, damage resolution, effect application), which requires
  `kind: oracle`; triaged in `rules-math-triage.md`.
- `ghidra_version`: the Ghidra that produced the decompilation
  (`tools/ghidra/versions.json`).
- `parity_waiver`: waives a structural-parity RED (`tools/parity/run.py`) a
  human confirmed against the binary. It may open with a list of the
  columns it waives — `vtable asm 2026-10-25 i023: <reason>` waives the
  vtable and asm columns, `vtable: <reason>` just vtable — and with no
  column token it waives asm only, the column it was introduced for. The
  column names are run.py's: `asm`, `vtable`, `layout`, `offsets`,
  `markers`, `lints`. The first token that is not a column name starts the
  reason, so the value stays one readable sentence and existing waivers
  keep their meaning.

Then the sections `Decompiled (Ghidra)`, `Callers` (functions referring to
the entry, with the referring address), `Callees` and `Notes`.

## What counts as touched

`tools/check_evidence.py [--base REV]` reads `git diff <base>...HEAD -- src/`.
A labelled function spans from its `// 0xADDR` comment (directly above the
definition, the rule `tools/ghidra/labels.py` uses for the Ghidra labels) to
the first following line that starts with `}`. A committed change to any line
of a span touches the function. Changes between functions, in headers, and
deleted functions are ignored; `src/` files new in the branch count in full.

Layout changes in headers are not detected: a struct whose fields move,
change size or are renamed (such as a file format in `src/FileFormat.h`)
touches no function span. File their evidence by hand, under the function
that reads the layout, and say in its Notes which offsets changed and why
(docs/evidence/4440F0.md, CDerivedStats::Reload, for the creature header's
ability order).

## Pending cases

A function ours does not implement yet, or one whose original needs engine
state no fixture can honestly provide, gets a pending case instead of a
comparison:

    ORACLE_CASE_PENDING("pending/roll_hit_points", RollHitPoints,
        "ours not implemented (...); original-only determinism check");
    ORACLE_CASE_PENDING("pending/hit", nullptr,
        "ours not implemented (...); needs engine state (...)");

Its evidence file sets `oracle_case` to the pending case, and the Notes say
why it is pending.

- **With a body.** Only when the original runs on a fixture the case builds:
  a zeroed block of the original object's size with just the members the
  function reads filled in (no faked sprites, items or effect lists, and no
  faked engine globals but for one exception).
  The exception (ruling M1-3): an engine global the function reads, such
  as the original's `g_pBaldurChitin` (0x8CF6DC), may point at a zeroed
  block with only the members the function reads filled in, ours at the
  same, both put back after each case. The original runs against the same
  fixture, so a read past those members crashes rather than passes.
  `oracle/cases/rules.cpp` (starting_hp), `combat.cpp`, `ui_misc.cpp` and
  others do this; each says in its header which members it fills.
  The body seeds the original's `srand`, calls the function, seeds
  again with the same value and calls it with the same inputs, and requires
  the same result and the same following `rand` value. It must run at least
  one input: a body that runs none fails as `PENDING (ran 0 inputs)`.
- **Without a body** (`nullptr`): the case is listed, not run, and its
  reason names the state the function needs.

`python3 tools/oracle.py run` lists both under `pending` and does not fail
for them. Functions found for a pending case that `src/` does not define yet
are named in `tools/ghidra/extra_labels.txt` (one `0xADDR NAME` line, with a
comment saying how the function was identified), which
`tools/ghidra/make_project.py` applies below our `src/` names and above
IEex's.

When ours is written, the pending case becomes a comparison case in the case
file of its class, and `oracle_case` follows it. Once `src/` defines the
function, its `extra_labels.txt` line can go.

`oracle/cases/pending.cpp` holds project 3's combat functions and FetchRumor:

| Function | Case | Body |
|---|---|---|
| 0x4A42F0 CGameEffect::CheckSave (ours ported, tests/test_effects_save.cpp) | pending/check_save | none: g_pBaldurChitin's game, sprite vtables, feats, the object array |
| 0x72DE60 CGameSprite::ProcessEffectList (rolls the save d20s) | pending/save_rolls | none: the whole effect list |
| 0x73B760 CGameSprite::Hit (attack d20 against AC) | pending/hit | none: equipped CItems, sprites from the constructor, the game's options (ours ported, tests/test_combat.cpp) |
| 0x547020 CRuleTables::GetIterativeAttackBonus | rules/iterative_attack_bonus | compared with ours |
| 0x73D560 CGameSprite::Damage (weapon damage dice) | pending/damage | none: equipped CItems, the target's animation, the message handler (ours ported, tests/test_combat.cpp) |
| 0x483B70 CGameDialogSprite::FetchRumor | dialog/fetch_rumor | none: the resource manager (the DLG and the IDS lists), the object array, a sprite answering status triggers |

## Replays and known differences

`python3 tools/oracle.py replay compare <scenario>` compares our engine with the original at every `checkpoint` line of a scenario. A difference that is understood and accepted goes in `oracle/replay/known_differences.json` as `field`, `checkpoint` (a name, or `"*"`), `reason` and `evidence`. The `evidence` value is an address such as `0x72DE60` or a path, and the file must exist under `docs/evidence/`, or the command stops. The compare prints a `STALE` line for any entry that matched no difference, so remove it. The file is empty now.
