# scenarios/chr

CHR fixtures exported from the original engine in the Windows Sandbox
(differential oracle). Raised through the cheat console; production and
findings under `.superpowers/sdd/2026-10-23-frida-imports/task-49-report.md`.

| file | bytes | sha256 (prefix) | content | run |
|------|-------|-----------------|---------|-----|
| RYSDRD7C.CHR | 3606 | `860b4fee` | druid 7 (scriptname `Rysdrd7`): xp 21000, HP 56/56, STR raised to 11 at level 4, skills 1/5/1/5/1/1/1 | `runs/20261027-012948` (p3/chr-party) |
| RYSSC12C.CHR | 3174 | `57b49bc0` | sorcerer 12 (scriptname `Ryssc12`): xp 66000, HP 48/48, knows SPWI119/SPWI226 (L1), SPWI217 (L2), SPWI603 (L6) | `runs/20261030-213848` (p3/chr-sorcerer) |

How it was made (scenarios/pending/chr_raise_druid.txt): chargen druid, one
`CTRLALTDELETE:SETCURRENTXP(21000)` on a fresh console open, then one
level-up walk through the character record; a single Done grants all six
levels. The walk must commit the level-up the way Task 48's level-up does
(the record's own Done, never ESC): an earlier run that left the record with
ESC exported a level-1 file, because the raise lived only in the uncommitted
level-up session and `CGameSprite::Marshal` writes `m_baseStats`. The run's
`world_end` checkpoint reads level 7 back in the world, and the export
landed in the oracle copy's `Characters` folder
(`%USERPROFILE%\iwd2r-oracle\game\Characters`).

Druid 7 casts 4th-level divine spells (MXSPLDRD row 7), so this character
reaches Flame Strike (SPPR503, missile 67) and Spike Stones (SPPR512,
missile 213). The sorcerer 12 of the party plan is RYSSC12C.CHR.

How RYSSC12C was made (scenarios/pending/chr_raise_sorcerer.txt): chargen
sorcerer with the two L1 picks, one `CTRLALTDELETE:SETCURRENTXP(66000)` on
a fresh console open, then one eleven-level walk whose popups are drained
and committed with their own Done buttons, then Export with the typed name
`Ryssc12C` — Done lands on the "Overwrite existing character?" popup (the
typed name upper-cases onto the stale attempt-1 file), and Overwrite
(329,291) writes the live level-12 marshal over `RYSSC12C.CHR`.

The parse test pinning this content is `ChrFixtureTest` in
`tests/test_tools.py`.
