# The WSO fork: imports, attribution, and the renderer artifacts

iwd2r absorbs the parallel recovery effort at
[WillScarlettOhara/iwd2-re](https://github.com/WillScarlettOhara/iwd2-re) —
the "WSO fork" — and will later port his GPU renderer into iwd2r's own GL
path. This file is the public summary: what the fork is, what the two trees
measured against each other, the rules every import follows, and where the
renderer artifacts come from. The design spec is
[docs/superpowers/specs/2026-10-17-wso-integration-design.md](../superpowers/specs/2026-10-17-wso-integration-design.md).

## The fork

Both trees descend from the same upstream commit (Batalov `4fa36bcd`,
2023-12-29). The fork's `main` (`wso/main`, fetched 2026-10-17) is 1,572
commits past it; ours was 1,295 when the spec measured it — about five
months of recovered engine code
on his side, and on ours the Linux build, the oracle, the replays and the
evidence gate. His verification is a differential Frida trace of the real
`IWD2.exe` against his own build, a recovery note per function under
`docs/*-recovery.md` in his tree, and a stated truth hierarchy of binary
over Ghidra decompile. His build is Windows-only (VS2019, MFC, x86);
imports are therefore per function and must compile under the platform
layer on Linux.

`wso` is a read-only remote and his tree is untrusted input: read and
compare with git, never execute anything from it. History is not merged — a
wholesale merge conflicts in 118 files (115 in `src/`) — so attribution
travels by the import trailer and docs/THIRD_PARTY.md, not by history.

## What we measured

Before anything was imported, the trees were compared function by function
over the 201 shared `src/*.cpp` files. The first measurement, made with the
pre-tool script and recorded as the design spec's table:

| Category | Functions |
|---|---|
| Identical real bodies in both trees | 4,337 |
| Stubs in both | 1,971 |
| His recovery where ours is still at upstream | 528 |
| His real body where ours is a stub | 26 |
| Functions only he has in shared files | 746 |
| Real functions in his 14 files we do not have | 159 |
| Our recovery where his is still at upstream | 279 |
| Our real body where his is a stub | 85 |
| Both recovered independently, bodies differ | 406 |

That table is the first historical measurement, not the current numbers.
The comparison is now [tools/xrec.py](../../tools/xrec.py), which matches
functions by their `// 0xADDR` comment before falling back to the name: the
trees drifted apart on names, and address-first matching pairs 356 renamed
functions the name-only script had filed as two one-sided rows. The current
comparison classifies 8,661 functions into ten categories;
`both_changed_differ` — the disagreement count wave 1 resolves — is **529**,
not 406. The current table and the account of every moved number are in
[xrec-report.md](xrec-report.md); the per-function rows are in
[disagreements.json](disagreements.json).

## Attribution

Every commit that brings in the fork's code — a wave-1 adopted body
included — carries exactly:

```
Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)
```

His additions are MIT, as granted to thierbig on 2026-10-17;
[docs/THIRD_PARTY.md](../THIRD_PARTY.md) records the grant and links his
LICENSE when it is published. IEex, the extender his renderer is built
into, is Bubb's work and that grant cannot cover it: the renderer port may
carry his code, never IEex's.

## Evidence: the `trace` kind

[tools/check_evidence.py](../../tools/check_evidence.py) knows four evidence
kinds: `oracle`, `replay`, `read-only` and `trace`. `trace` means the
function is verified by a differential trace of the original against a
reconstruction — the fork's own style of verification. A `trace` evidence
file must cite its source in `trace_source`, in one of exactly two forms:

```
WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md
frida <probe name> orig:<sha256> ours:<sha256> compare=0
```

The first cites his recovery note (never a source file): the gate reads
the note at that sha with `git show` and requires the function's address in
it, or its name as a whole word together with its class's name (a bare
"Render" names a method of every class), and an unreadable note (the fork
not fetched) fails. The second is the line the wave driver's Frida phase
writes for a differential capture of our own binary against the original
(`tools/frida/`, [docs/frida-tracing.md](../frida-tracing.md)): its
`compare=0` means the two captures agree call for call in a run whose state
dumps matched, and only the driver writes it. A missing, empty or
unrecognizable citation fails the gate; the format contract is in
[docs/evidence/README.md](../evidence/README.md). Rules math — attack
rolls, saving throws, damage, effect application — never accepts a trace:
those functions require `kind: oracle`, imported or not.

## Briefing agents

Five tools under `tools/`, adopted from the fork's `scripts/` at
`e8dd16a`, change how work is handed to an agent: implementer briefs name
functions by `assemble_context` bundle, reviewers start from the digest,
and nobody answers "what does this call" by reading a raw decompile.

- [tools/fn_digest.py](../../tools/fn_digest.py) — `digest(decompile_text,
  callgraph)` → a ≤30-line brief: signature, callees with resolved names,
  string literals, globals, unresolved calls, loop and switch counts.
- [tools/src_find.py](../../tools/src_find.py) — `find(query)` →
  `(file, line, signature)` for `0xADDR`, `Class::`, `Class::Method` or a
  bare name, resolved from the `// 0xADDR` markers. No grep chains.
- [tools/assemble_context.py](../../tools/assemble_context.py) —
  `bundle(address)` → digest, decompile, the first 40 lines of each
  caller's decompile, callees' signatures, resolved IDS constants and
  resrefs, in that order, with a token estimate on the first line. At most
  12 callers are shown (`CALLER_CAP`); callers without a decompile are
  skipped. The
  decompiles are `$IWD2R_HOME/ghidra-exports/decompile/<ADDR>.c`, written
  once per function by `tools/ghidra/export_decompiles.py` (chunked,
  resumable, read-only Ghidra; a missing file reads `decompile: not
  exported`); when the function belongs to a class and the BG2 index exists, the
  class's BG2 layout is appended as a hypothesis to check against IWD2
  (the offsets are BG2's, never quoted as ours).
- [tools/token_audit.py](../../tools/token_audit.py) —
  `summarize(transcript.jsonl)` → where a session's context went: usage
  totals, per-tool result weight, largest single results.
- [tools/bg2_find.py](../../tools/bg2_find.py) — `lookup(query)` →
  class/member/method layout from a SQLite index built once from the
  BG2:EE PDB symbol dumps (131 MB of dumps → 12 MB index, ~1 s to build,
  ~1 ms per lookup). The dumps are pinned like everything else — see the
  next section.

Measured on this stream's own coordinator transcript (12.4 MB, 901
messages carrying usage): tool results dominated the context — Read
~76K tokens over 40 calls, Bash ~51K over 183, Agent ~5K over 22 — with
the largest single Read ~13.6K tokens, and ~49.2M cumulative cache-read
tokens (~54,655 per message). That measurement is why the bundle caps
callees at 20 and states its own token cost on line one.

## How the coordinator drives a wave

Wave 1's loop is five commands, all in [tools/wave.py](../../tools/wave.py);
batch state lives in [wave-state.json](wave-state.json), created by the
first `next`, and no batch bookkeeping is written by hand:

    python3 tools/wave.py next            # rank rows, record the batch, cut its worktree, triage it
    python3 tools/wave.py brief b001      # the dispatch text, from brief-template.md
    python3 tools/wave.py verdict b001 0x643CC0 ours probe-17   # per reported row
    python3 tools/wave.py close b001      # evidence + clean tree + gates, then ff `first`
    python3 tools/wave.py scorecard       # the tally into xrec-report.md

`next` ranks `both_changed_differ` rows by size (his length + ours),
takes twenty, and never lets two live batches own the same file. It
also cuts the batch branch `p3/wso-<id>` from `first`, with its worktree
at `<main worktree>-worktrees/p3-wso-<id>` (a branch that already has a
worktree keeps it). Then it applies [triage.json](triage.json):
- a row whose class calibration enables, and whose evidence file is in
  the worktree, gets its `ours` verdict and the evidence line, committed
  on the batch branch;
- an `ours-stub` row leaves the batch for `docs/wso/import-list.json`
  (created when missing);
- every other row stays for the worker.

It reports `triage: n auto, n import, n ambiguous`. Do not
`git worktree add` after `next`: the worktree already exists.

### Will's tip: `recompare`

Will pushes daily, and the rows were compared at one sha. `wave.py recompare
[--tip SHA] [--no-fetch]` fetches `wso`, resolves the tip (default `wso/main`),
runs `tools/xrec.py 4fa36bcd <tip> first` and appends to `disagreements.json`
the `both_changed_differ` rows that are in no batch and have no verdict (an
older undecided row for the same address is replaced by the tip's). The state's
`fork_base` becomes the tip. It prints `recompare: N new rows at <tip7> (M
already queued, K decided)` and exits 0, or 2 when the tip does not resolve
(nothing is written). The fork is only read, through `git show` and `git
ls-tree`. `--state`, `--disagreements` and `--repo` point it at copies, to look
without touching the live files.

`decided` is built from the batches of both queue states — the one given and
its import twin beside it (`import-state.json`) — so a recompare never re-files
a row an import batch closed: a closed row holds unless the fork's body for its
address changed between that batch's pin and the tip (unreadable bodies hold).
The same append/replace pass then runs for the import categories
(`theirs_real_ours_stub`, `theirs_changed_ours_base`, `only_theirs`), so the
rows `next --queue import` cuts are refreshed too, and the import state's
`fork_base` moves to the tip with the wave state's (its in-flight batches keep
their pin). The message adds `recompare import: N new rows (M already queued,
K decided)` for that pass, and — only when it is not empty — one line counting
the file rows whose tip category left the import categories (e.g. now `same`)
but that no batch holds: they are reported and left alone.

Each batch carries the `fork_base` its rows were compared at, recorded at cut
(old batches fall back to the state's). `brief`, the triage evidence line and
`close`'s check read the batch's pin: a batch cut before a recompare is briefed
and closed at its own sha, and its evidence must say `disagreed with WSO at
<sha>` or `corroborated by WSO at <sha>` with that sha (an abbreviation of at
least seven digits is enough). `wave.py run` calls `recompare` once when the
wave-1 queue is exhausted, logs `recompare: N new rows at <tip7>`, and cuts
again when N is not zero.
`close` refuses — listing every reason — unless each row has a verdict,
the batch's worktree has no uncommitted paths, every touched function's
`docs/evidence/<ADDRESS>.md` says `disagreed with WSO` or
`corroborated by WSO`, the branch has not modified `tools/gates.sh` or
`tools/parity` (a batch must not change the gate that certifies it), and
`tools/gates.sh --base first` exits 0 in that
worktree; only then does it fast-forward `first` to the batch branch and
print the tally. After a close, sync the worktree that has `first`
checked out: `git -C <path> reset --hard first`. `status` is one line
per batch, and `scorecard` rewrites the "## Scorecard" section of
[xrec-report.md](xrec-report.md) from state (idempotent — the same
state writes the same bytes); when `docs/wso/audit.json` (rule, seed,
state, pool, drawn, and rows with `probe` replay|oracle|park and `result`
match|flip|parked) exists, the section ends with one rule-3 audit sample
paragraph rendered from it, and a malformed file fails the scorecard with
exit 2. Waves 2 to 5 are imports and reuse
`close --kind import`: no verdicts are required, but every row's
evidence must be `kind: trace` or better instead.

## Running the batches

`wave.py run` ([tools/driver.py](../../tools/driver.py)) runs that loop
without a coordinator turn per batch: `next` (cut and triage), the brief,
one headless worker from a pool (by default `claude-glm`,
`glm-5.3-flash[1m]`, the batch worktree as its cwd), the mechanical checks with one fix round, a review
when it can matter (any fork or neither verdict, or the seeded
twenty-percent sample) with one fix round, then the verdicts, `close`,
the push of `first:main first:first` to `iwd2r`, and a templated
`## driver <id>` entry in
`.superpowers/sdd/2026-10-17-wso-integration/progress.md`. The driver
itself uses no model.

A row the worker's reading calls fork or neither, with no probe, replay or
trace case it can run, goes under `## Undecided` (no verdict line); the
driver moves it out of the batch to `docs/wso/probe-list.json`, which
`next` treats as decided, for a later probe.

`python3 tools/wave.py resolve <address> fork|ours|neither <case>` settles
such a row: it checks `docs/evidence/<ADDR>.md` for a `disagreed with WSO` /
`corroborated by WSO` line ending `case <case> decides`, refuses a
fork/neither verdict decided by a dump reading (as `close` does), appends
the row (rebuilt from `disagreements.json`) to the batch it left unless
it is already there, records the verdict as `verdict` does (`measured:
none`), and removes the entry from the probe list; the batch's status is
not touched.  A fork/neither resolve reads the losing kind from the batch
branch, like `verdict`, so the branch must still exist and carry the
evidence file.

`python3 tools/wave.py undecide <batch> <address> fork|neither <reason...>` is
the reverse, for the coordinator: it moves a row of an `assigned` batch to the
probe list (`source: coordinator`) and drops its verdict, as the driver does
for `## Undecided` rows.  It refuses a closed batch, a foreign address, or an
address already listed, and writes nothing then.  The batch branch must then
hold our body for that function (the merge base's) and an evidence file with
no deciding sentence; `resolve` brings the row back.  Run it while the batch is
not in flight (its exception cleared but not yet retried, or the driver paused):
the command edits the state without the driver's lock, like `resolve` and
`unpark`, and a worker running on that batch could still record a verdict for
the moved row.

A rule that is not a check, a flag or a test is not a rule.

`close` refuses
a fork or neither verdict decided by a dump reading unless
`--allow-dump RULING --ledger PATH` names a `Ruling <id>` line of that file;
the driver rejects a worker whose transcript shows a git command naming a
`wso/` ref, a whole-file read of a covered source file, a history rewrite
(refused: `reset`, `rebase`, `revert`, `restore`, `clean`, any `stash`,
`commit --amend`, `filter-repo`, `push`, `branch -d/-D/-f/-M/-C`,
`checkout -B`, `switch -C`, `checkout -- <path>`, `checkout <sha> <path>`
and a bare `git checkout <path>` of a file; allowed: `add`, `commit`,
`status`, `diff`, `log`, `show`, `ls-tree`, `merge-base`, `rev-parse`,
`blame`, `checkout -b`, `checkout <branch>`, `switch -c`) or a launch outside
`tools/play.py` and `tools/gltrace.py`; those two launchers allow two
launches per attempt until `--new-attempt "<why>"`; and every merge into
`first` pulls first (`git pull --ff-only`) and fast-forwards.

    python3 -I tools/wave.py run --workers 2 --budget 50000
    python3 -I tools/wave.py run --queue wave1 --pool claude=claude:sonnet:1
    python3 -I tools/wave.py run --pool glm:2 --pool claude --budget-glm 30000
    python3 -I tools/wave.py run --queue import --pool claude
    python3 -I tools/wave.py run --clear b011
    python3 -I tools/wave.py run --dry-run --state /outside/the/repo/state-copy.json

- `--pool NAME=WRAPPER:MODEL[:WORKERS]`: a worker pool; repeatable.
  `WORKERS` defaults to 1. The two known names also take the short form
  `NAME[:WORKERS]`, which fills in the wrapper and model: `glm` is
  `claude-glm:glm-5.3-flash[1m]` on either queue, and `claude` is
  `claude:sonnet` on `--queue wave1` and `claude:haiku` on
  `--queue import`. With no `--pool` the driver has one pool,
  `glm=claude-glm:glm-5.3-flash[1m]:2`. A pool whose wrapper is not
  `claude-glm` gets its model as `--model <model>` (not `CLAUDE_GLM_MODEL`),
  and its transcripts are read from `~/.claude/projects` (or
  `CLAUDE_CONFIG_DIR`).
- `--reviewer-model NAME=MODEL`: the model that pool's reviewer uses
  (same wrapper as the pool); repeatable. Defaults: `sonnet` for the
  `claude` pool, `glm-5.3-flash[1m]` for the `glm` pool, and the pool's
  own worker model for any other pool.
- `--workers N`: the default pool's worker count. It is an error
  together with `--pool`: give each pool its own `:WORKERS`.
- `--budget C`: credits for this run over all pools (no limit without
  it). Before each new batch (a cut, or the retry of a cleared
  exception) the run stops when the credits spent plus the next batch's
  estimate exceed `C`; a batch is never stopped midway. The estimate is
  the mean of this run's batches with known credits, 1200 before there
  is one, and a batch whose credits are unknown counts as the estimate.
- `--budget-NAME C`: the same cap for one pool; that pool stops
  (`stopped: "budget"`) and the others go on. Z.ai credits price only
  GLM turns, so `--budget-NAME` on a pool whose model is not GLM is
  refused with a message (ruling R40), and so is a name that is no pool.
- `--max-batches N`: stop starting batches once N have started in this
  run, counting adopted, cut and resumed ones. The batches in flight
  finish; the run then exits 0 (3 if a pool ended parked) with
  `stopped: {reason: "max-batches", n: N}` in the status file.
- `--size N`: rows per batch (default 20; refused below 1). A smaller
  batch keeps an import worker inside its timeout (i047's twenty
  whole-function `only_theirs` rows timed the worker out).
- Adoption (`--queue wave1`): before any new cut, the driver adopts every
  batch in state `assigned` with no recorded verdict that nothing in the
  status file claims (running, parked or exception), oldest `assigned_at`
  first. It cuts the branch and worktree when they are missing, as `next`
  does, applies triage (auto verdicts, ours-stub rows to the import list,
  a `class` on every row), then runs it like a fresh cut. A batch with
  any verdict is left for the coordinator.
- After every close the driver commits `docs/wso/wave-state.json` and the
  regenerated `docs/wso/xrec-report.md` on `first` (staged by name, message
  `wave-state: close <id> (ours N, fork N, neither N)`), before the push.
  Also when it pushes a closed batch again. The same commit carries the
  probe list (wave 1), `docs/wso/import-list.json` (where ours-stub rows
  leave wave 1), and, on `--queue import`, `docs/wso/import-state.json`
  (message `import-state: close <id> ...`; no scorecard). If the commit
  fails, those paths are unstaged, one aoe line goes out, and the push still
  happens.
- Several workers per pool (`--pool glm:2`): `docs/MAP.md` is not the
  batch's. The driver regenerates it once at each close, on the branch after
  the base is merged in, and commits it by name (`map: regenerate ...`), so
  `gen_map.py --check` passes after the fast-forward and two parallel closes
  never conflict on it (a worker's own edit of it loses to the base's copy at
  the sync). A stop (SIGTERM, SIGINT) starts no new worker (a worker forked
  just as it lands is killed on joining), and takes only re-entrant locks in
  the handler. The signal reaches only the main thread: its own writes (the
  cut, an adoption, the recompare) finish before the stop is raised, and the
  stop waits up to 30 s for a git write in progress on another thread. The
  run still joins a close in flight without a limit (the gate included, about
  ten minutes), and a Ctrl-C in the terminal also reaches the git and gate
  children directly (same process group); the bounded wait is for a SIGTERM
  sent to the driver's pid.
- Two `claude-glm` launches at once race on the wrapper's re-link of
  `~/.claude` into `~/.claude-glm` (and on its `settings.json.tmp`). The
  wrapper lives outside the repo, so the fix is a patch the coordinator
  applies once: `patch ~/.local/bin/claude-glm < tools/claude-glm.lock.patch`
  (an `flock` on `~/.claude-glm/.relink.lock` around that section; needs
  util-linux `flock`). Apply it before running `--pool glm:2`.
- Workers: every `AOE_*` variable is removed from a worker's environment
  (for every wrapper), and the driver's own `aoe send` runs without
  `CLAUDECODE`, `CLAUDE_CODE_*`, `CLAUDE_PID` and `CLAUDE_EFFORT`. A `claude`
  worker gets `--setting-sources project,local`, `--strict-mcp-config` and
  `ENABLE_CLAUDEAI_MCP_SERVERS=0`. Every run has no stdin and a time bound,
  `--worker-timeout SECONDS` (default 10800): on a timeout the whole process
  group is killed and the batch is parked with the reason `timeout`, like a
  429 park (the run exits 3 once the other pools are done). A worker's
  subagent transcripts (`<session>/subagents/*.jsonl`) are scanned and
  counted too. Stopping the driver (SIGTERM or SIGINT) stops its workers: the
  handler kills every live worker and reviewer process group, the run exits 1
  through its normal cleanup (locks released), and the batches in flight stay
  in `running` for the next run to resume. Workers get
  `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`, and a Bash command that sets
  `IWD2R_DESKTOP_OK` is a terminal `transcript` exception.
- The mechanical checks (the worker's fix round) also fail on: uncommitted
  paths in the batch worktree ("commit or delete: ..."; the driver no longer
  commits them for you), a path changed against `first` outside `src/`,
  `compat/`, `docs/evidence/`, `docs/MAP.md`, `oracle/cases/` and
  `oracle/CMakeLists.txt`, a `fork` verdict whose evidence has no `kind:`
  line. (A stale `docs/MAP.md` is no longer checked here: the close
  regenerates it.) Every verdict row's evidence carries the deciding
  sentence (`disagreed with WSO at <sha>: ...; case <id> decides` or
  `corroborated by WSO at <sha>: ...; case <id> decides`, wrapped lines
  allowed) and the checks enforce it; a driver probe pass needs the
  `corroborated by WSO` form.
- Retrying a cleared exception (`--clear`): at the `close`, `record` or
  `push` stage the batch goes straight to that stage on its worktree and
  recorded verdicts, with no worker run; at any other stage the worker
  restarts with a `Resume:` preamble, as after a park; a failed adoption
  cut is adopted again. The one aoe line per exception is
  `driver: <id> exception at <stage>; see <status file>`, never problem text.

- `--queue wave1|import|probes`: which list to work (default `wave1`).
  `probes` (Task 28) is `docs/wso/probe-state.json`: batches `p<NNN>`
  (branch `p3/wso-p<NNN>`, status `driver/status-probes.json`, brief
  `probe-brief-template.md`) of up to 20 rows of `docs/wso/probe-list.json`.
  The worker adopts the fork body for each `fork` row (report section
  `## Candidates`: `candidate <0xADDR> fork|neither <reason>`; a `neither` row
  is not built); the Frida phase decides it. Equal: the evidence becomes
  `kind: trace`, the driver adds `...; case frida-<batch>-<scenario> decides`
  under `## WSO delta` and records the row through `resolve_probe` (fork,
  measured frida) so it returns to its original batch. Different: the finding
  is appended to the entry's `reason`, an entry goes to
  `docs/wso/engine-gaps.json`, and the fix round puts our body back. Not
  exercised: the entry stays, its reason refreshed. `close --kind probes`
  needs every row resolved or back on the list with our body; the scorecard
  gains a `probes: resolved N, on list M` line. The phase only records the
  decision (`outcome: fork-equal`, case, measured body, losing kind on the
  probe-state row); the record stage, after the checks and the (mandatory) review,
  calls `resolve_probe`. A fork candidate's evidence must be `kind: read-only`;
  a `neither`-reading row can only be listed as `neither`. A row an open batch
  holds is not cut again; a closed batch releases the rows it left on the list,
  stamped `probed: {batch, scenarios, hits, pin, hand}`, and `next` offers one
  again only when the coverage map now gives it a scenario in neither `scenarios`
  (what ran) nor `hits` (what the map gave it then), or its pin moved; a `hand`
  row (a `neither` candidate, or a fork body that measured different) only on a
  pin move. To undo a `fork-equal` row a review rejected, remove `outcome`,
  `case`, `measured_body` and `losing_kind` from its probe-state row (and put
  the evidence file back as the merge base has it); the phase measures it again
  at the next run. `wave1`
  is `docs/wso/wave-state.json`: the `both_changed_differ` rows, triaged,
  batch ids `b<NNN>`, brief `brief-template.md`, closed with verdicts.
  `import` is `docs/wso/import-state.json`: the `theirs_real_ours_stub`
  rows, the `theirs_changed_ours_base` rows and the `only_theirs` rows,
  in that order, plus the entries of `docs/wso/import-list.json`, no
  triage, batch ids `i<NNN>` (branches and worktrees `p3/wso-i<NNN>`,
  so they never
  collide with wave 1's), brief `import-brief-template.md`, closed with
  `close --kind import`: no verdict lines or evidence phrases are
  checked, the evidence must be `kind: trace` or better, and the tool
  tests, `check_evidence` and parity still run. The import queue skips
  the oracle phase and never rewrites wave 1's scorecard. Its coverage
  requirement is per category (2026-10-26 ruling): where ours lacks the
  body — the stub and `only_theirs` categories, rule 1 — every
  candidate is offered, covered first, and a row no scenario exercises
  imports unmeasured (below); where both trees have the body —
  `theirs_changed_ours_base`, rule 2 — only the rows the coverage map
  exercises are cut, and the rest are kept by default (ours stays, no
  worker runs). It runs the
  Frida phase (with `--frida`): the scenarios the coverage map picks
  for the batch (the nine world scenarios, plus `biography` for screen
  rows; the driver log names them), each through `probe.py run` once per
  side. Before it the driver builds our x86 exe from the batch worktree in
  that worktree's own Windows checkout (`%USERPROFILE%\iwd2r-build-<worktree
  name>`, `driver.frida_build`, compiler output in the batch's
  `frida-build.log`), hooks at the RVAs of that build's map and runs that
  exe (`probe.py run --build-checkout`), so the bodies measured are the
  batch's; the tools are staged from the driver's checkout. A phase-wide
  failure (the build fails, the run raises, or every scenario failed with no
  status on either side) is the batch's exception at stage `frida` (status
  file, ledger, one aoe line): no row is measured, parked or rewritten, and
  clearing it reruns the worker and the phase; a row-level failure (one
  scenario failed for one row, or one row not exercised) still parks that
  row — except the rule-1 row no scenario exercised, which the phase
  marks unmeasured instead (below). On
  a wave-1 batch the build is the branch's too, so an equal row confirms
  whichever side the verdict records (`measured: frida`, ours, neither or
  an adopted fork body) and a different one is a finding for that side.
  `--frida-map MAP`
  overrides the build with an existing one (the exe beside MAP), for hand
  runs. A row is measured equal when `trace_diff compare` exits 0 over the
  captured events and both sidecars sent their exit-time (final) hit totals
  (equal events without final totals on both sides are `not measured`: the
  hooks cap what they log and the timer's totals lose a run-dependent
  tail). Frequency is the callers' business: when the final totals differ
  but every captured event is equal, or one side's events are a strict
  prefix of the other's with that side under the cap (with final totals on
  both sides; without them it is `not measured`), the row is still equal and
  the difference goes to the ledger as a caller finding (`- frida frequency
  only (a caller finding): ...`, for both queues), never a finding or a
  verdict change; only an event-level difference (an argument or a return
  at a call both sides made, or a crash) is `different`. The evidence cites
  only a scenario whose compare returned 0, so an import row that is equal
  only as a prefix IS parked (no exact compare to cite); wave-1 and the
  calibration treat it as equal. All of it holds only in
  a scenario whose two runs kept the same state:
  both sides write checkpoint and periodic state dumps, `truth.frida_run`
  compares them with the replay compare's own functions
  (`truth.compare_dumps`: `oracle.compare_checkpoints` and
  `compare_frames`, its screens and known differences), and a scenario whose
  dumps differ measures nothing (its rows read `not measured`, "dumps
  differ"); the driver writes trace evidence only from a run whose dumps
  match (a void function logs no return, so its trace mostly measures its
  callers; the dumps are what speak for its body). The driver then rewrites that row's evidence front matter to
  `kind: trace` with `trace_source: frida <batch>-<scenario> orig:<sha256>
  ours:<sha256> compare=0` (dropping `exemption:`) and commits it as
  `frida: trace evidence for <N> rows of <batch>`. The phase measures
  every row that is not parked and every function the batch touched
  against the merge base that carries a `// 0xADDR` marker (a stub
  dependency imported with a row). Workers write `kind: read-only` with an
  `exemption:`; only three kinds of evidence stand as written and are not
  measured (`wave.kept_evidence`): `kind: oracle` with an `oracle_case`
  that a case in `oracle/cases` registers, `kind: trace` citing a fork
  recovery note that names the function, and an evidence file unchanged
  since the merge base at `trace` or better. A declared `replay`, an oracle
  without its case or a worker's `frida` line is measured like a read-only
  row, and a frida line the driver did not write (it keeps the lines it
  wrote in the status file, `frida_written`) is a mechanical-check problem.
  A measured row that is not equal (different, no hook, `scenario
  failed`, `not measured`, or — outside rule 1 — exercised by no
  scenario) cannot close: the phase runs
  once per batch, so the finding tells the worker to park it in its fix
  round: edit that function back to the merge base's body (one function,
  never a whole file that holds other rows' changes), delete its evidence
  file and write `{address, name, file, batch, needs}` as the row's own
  `docs/wso/import-parked/<ADDR>.json` (one file per row, so two batches
  parking different rows merge cleanly; `needs` is what the finding names:
  the scenarios tried, `frida different: <detail>`, or the status text;
  a batch may add files there). Rule 1 (2026-10-26 ruling) takes one case
  out of parking: a row where ours never had the body
  (`theirs_real_ours_stub`, `only_theirs`) that no scenario exercised.
  The import needs no coverage there, so the phase rewrites that row's
  evidence `exemption:` to start `unmeasured WSO import` (the driver
  only, never the worker, like the frida `trace_source` line; `kind:`
  stays as it is), commits it (`frida: unmeasured WSO import marking
  for <N> row(s) of <batch>`), and the row closes unmeasured: the close
  takes the marking as its evidence, like the three forms above. Only a
  marking that fails falls back to parking the row.
  `python3 -I tools/wave.py unmeasured-imports` lists every evidence
  file that carries the marking, so a later verification pass knows
  exactly what never measured.

  Rule 3: rows parked before that ruling only for lack of coverage come
  back through `python3 -I tools/wave.py requeue [--dry-run] [--reason
  TEXT]`: every parked rule-1 row whose `needs` ends in
  `not exercised` re-enters the queue (unpark's move). A row parked for
  any other reason (a dependency, an exe contradiction, a failed check)
  stays parked, as do a base-body row (rule 2 keeps those ours) and a row
  of a batch that is not closed. `--dry-run` lists the rows and moves
  nothing.

  `close --kind import` accepts a batch when
  every row that is not parked is imported (its body changed against the
  merge base: "row <addr> not imported: body unchanged against <base>;
  import it or park it") and at `trace` or better as the driver's frida
  trace or as evidence that stands as written (or carrying the
  unmeasured WSO import marking, rule 1), every parked row has its
  body back at the merge base's and its evidence gone, and every
  dependency the batch touched meets the same bar.
  The two queues' assigned batches all hold files, so they stay
  disjoint (`tools/claims.py` counts both state files). Each queue has
  its own status file and run lock, so one driver per queue can run at
  the same time; the ledger is one file.
- `--dry-run`: one batch with a fake worker that answers `ours` for every
  row; no review, no record, no close, no push, no `aoe send`, no budget.
  It needs `--state` pointing at a copy of the wave state OUTSIDE the
  repo, and refuses the live `docs/wso/wave-state.json` (even through a
  symlink). It still cuts a real batch (branch, worktree, triage
  commits), so run it in a disposable `git clone --shared`. Its ledger
  and status go to `driver/dry-run/`.
- `--state PATH`: the queue's state file (default `docs/wso/wave-state.json`
  or `docs/wso/import-state.json`).
- `--seed S`: the review sample seed (default `2026-10-22`), recorded in
  each ledger entry's review line.
- `--aoe-session ID`: the AoE session that gets one `aoe send` line per
  exception.
- `--clear BATCH_ID`: removes that batch's entry from `exceptions`,
  prints what it removed, and exits 0, or 2 when there is no such entry.
  The next run retries that batch before any new cut.

  Scenario rules of the phase. A UI-only scenario (`truth.FRIDA_UI_SCENARIOS`,
  `biography`) never reaches the world screen, so both sides run with
  `--no-loaded` (CLEAN when the game ends without a crash). A scenario that
  fails the same non-crash way on both sides (both NOT-LOADED, both no
  status, the same OBSERVER-ERROR) measured nothing: it is left out of every
  row (`info['uninformative']`, one driver log line each) and, if every
  scenario is such, the phase's infrastructure exception fires; a one-sided
  failure still reads `scenario failed`. A scenario must not depend on state
  it wrote in an earlier run: every probe run (both sides, the coverage pass
  too) starts with the empty save folder a replay record gives
  (`probe.saves_of_its_own`, `oracle.prepared_saves` mode empty: the copy's
  own saves wait aside and are put back, the run's saves go to
  `runs/<stamp>/replay/saves`), so a saving scenario sees the same state on
  both sides every time; a scenario that needs a save goes through
  `--load`. `save_load_screens` stays in `truth.PHASE_HELD_OUT` until one
  real original/ours pair confirms matching dumps. The driver's stop
  (SIGTERM, SIGINT) also kills the running `probe.py` launches and taskkills
  the sandbox processes.

**Pools and threads.** Pools share the batches: a batch goes to the first
pool that has a free worker, is not parked and is under its budget, and
ties rotate round-robin. Each busy worker is one thread, up to the pool's
`WORKERS`; a run with one worker in all runs everything on the main
thread. Only the worker and reviewer runs, the oracle and Frida phases and
the mechanical checks run in parallel. Every wave-state write and every
repo-level git operation (the cut, record, close with its fast-forward, the
push) runs under one driver-wide lock, and the gate is serialised across
drivers by the `tools/gates.sh` lock. A batch cut before another closed has
the base merged into its branch just before its close, because `close`
only fast-forwards. The source files of two batches are disjoint, but
every batch with passing oracle probes adds a case line after the same
anchor in `oracle/CMakeLists.txt`, so the driver writes
`oracle/CMakeLists.txt merge=union` once into the common git dir's
`info/attributes` (only when that exact line is missing) and those lines
merge as a union. If the base moves again while a close waits on the gate
lock (the other queue's driver closed first), the close syncs, gates and
tries once more before it raises a close exception. Workers run with a
scrubbed environment: the variables that tie a process to the launching
Claude Code session are dropped for every wrapper, and a wrapper other than
`claude-glm` also loses the GLM endpoint, token and model variables. The
deciding pool is recorded as `pool` in the batch's state entry, printed as
a `- pool <name>` line in its ledger entry, and shown beside the batch in
the scorecard.

**Per-batch figures.** Each batch's ledger entry and its `batches` entry in
the status file carry `wall` (seconds from the cut, or the resume, to the
finish), `tokens` (`{input, cached, output}`) and `tool_calls`, summed over
the batch's worker and reviewer runs from their transcripts (once per
message id). Credits stay GLM-only; tokens and tool calls count every
model.

The status file is
`.superpowers/sdd/2026-10-17-wso-integration/driver/status.json` for
`wave1` and `driver/status-import.json` for `import`
(untracked; each batch's briefs, reports and review diff are beside it in
`driver/<id>/`). Its keys:

- `current`: `{batch, stage, at}`, the latest batch in flight; `running`
  is `{<id>: {batch, stage, at, pool}}` for all of them.
- `pools`: `{<name>: {credits, budget, parked, stopped}}`. `credits` is
  this run's spend on the pool (a batch with unknown credits counts as the
  estimate), `budget` its `--budget-NAME` (null for none), `stopped` is
  `"budget"` once the pool stopped, and `parked` is `{<batch id>: {at,
  reason}}`: the batches a 429 parked on that pool (a pool with several
  workers can park several; a resume takes a batch off every pool's map). A 429 parks only its own
  pool; the other pools keep taking batches. This replaces the single
  top-level `parked` of the one-worker driver; a run still reads an old
  one once and moves it to its batch's pool.
- `stopped`: `{at, reason: "budget", spent, budget}`, the last stop of the
  `--budget` cap over all pools; a later run clears it.
- `stalled`: `{at, message}`, set when the rows left wait on files that
  assigned batches own; the next cut clears it.
- `halted`: `{at, reason}`, set when the run halted (a launch failure,
  an auth or billing API error, or the breaker); a later run that starts
  a batch clears it.
- `exceptions`: `{<id>: {stage, problems, at}}`, the batches waiting on
  the coordinator.
- `batches`: `{<id>: {...}}`, each batch's last outcome: `outcome`
  (closed, exception, parked, or checked for a dry run), `credits` (null
  when unknown), `at`, `wall`, `tokens` and `tool_calls`, plus `stage` for
  an exception, and `wip` (the WIP commit's sha) or `wip_error` (why none
  was made) for a park.
- `updated_at`: the time of the last write.
- `dry_run`: `true`, in the dry run's own status file only.

Every write is a read-modify-write of that file under an `fcntl.flock`
on `status.json.lock`. The coordinator clears an exception with
`--clear`, never by editing the file by hand: a hand edit races the
driver's writes, and either loses a new exception or brings back the one
it removed.

One driver runs per status file (so one per queue). `run` holds a non-blocking flock on
`status.json.run.lock`, with its pid written in it, for its whole
lifetime; a second `run` on the same status file exits 1 with `another
driver is running (pid N)`. `--clear` does not take that lock.

Exit codes:

- 0: nothing left (every row is assigned or decided), or the budget
  stop; a dry run that cuts a batch exits 0 whatever the batch's outcome
  (its outcome is in its status file). A dry run that cuts nothing exits
  like a run: 0 when nothing is left, 4 when stalled, 1 on a cut failure.
  Any dry run exits 1 on a held run lock or a corrupt status file.
- 1: a cut or push failure (an exception that is not the batch's own),
  a halt (`halted`: a launch failure, an auth or billing API error, or
  the breaker), a status file that
  cannot be read (the message names it), or another driver already
  running on the same status file.
- 3: a pool ended parked on a Z.ai rate limit (the other pools ran on
  until they had nothing left).
- 4: stalled: the rows left wait on files owned by assigned batches.

(2 is a usage error, a refused dry run, or `--clear` with no such entry.)

**Park and resume.** A worker or reviewer hit by Z.ai's rate limit
parks the batch, and its pool with it. Either its final JSON result is an error (`is_error`,
or text that starts `API Error`) saying `Request rejected (429)`, or the
last API-error entry in its transcript is a rate limit
(`apiErrorStatus` 429 or `error` `rate_limit`, that text as fallback)
and no successful turn follows it. Tool output, file text and the
worker's own prose never count, and neither does a 429 the worker
retried past. The driver commits whatever the batch worktree holds
uncommitted (`git add -A && git commit -m "WIP: parked by driver"`, in
the batch worktree only, and skipped when it is clean), writes the pool's
`parked`, and the run exits 3 once the other pools are done. The next `run`
settles what the last one left before any new cut. First, a batch it
closed without writing the ledger entry gets the entry and the push. Then
each pool's parked batch (on that pool), and after them a batch left
in `current` or `running` (the process died mid-batch), run again, each with its
brief re-rendered under a `Resume:` preamble that lists the verdicts
already recorded. The preamble forbids rewriting the branch. When its tip is
the driver's own unpushed WIP commit, the driver itself soft-resets it
before resuming, so the previous work is staged in the worktree and the
worker commits it again with proper messages and `Imported-From:` trailers
without any git command. `parked` clears when that batch
ends any other way. A batch whose worktree is gone (a directory removed
by hand stays listed until `git worktree prune`) becomes its `worktree`
exception, which also ends its park, and the run goes on.

**Halts.** Three failures are not a batch's own, so the run halts (exit
1, `halted` written, one `aoe send` line) instead of cutting every row
left into a batch and an exception:

- A launch failure: a worker, reviewer or check step that cannot be
  started in an existing worktree (for example `claude-glm` not on the
  timer's `PATH`). The batch is parked with reason `launch failed:
  <error>`, so the next run resumes it first.
- An API error no retry cures: the final JSON result, or the last
  API-error entry in the transcript with no successful turn after it,
  carries `error` `authentication_failed` or `billing_error`, or
  `apiErrorStatus` 401, 402 or 403. The batch is parked with reason
  `api error: <kind>` and resumed first once the login or the credit is
  fixed. One the worker got past does not halt.
- The breaker: 3 batch exceptions in a row whose worker never really
  ran: no transcript, or a transcript with no successful turn (only
  `<synthetic>` or API-error entries). A batch whose worker did run
  resets the count. It counts real turns, not credits, because a Claude
  pool's credits read 0.

**The Friday timer** (user-side; the timer is not in the repo) starts
the driver again after the weekly Z.ai reset, instead of sending
`continue` to a model:

    systemd-run --user --unit=zai-resume-driver --on-calendar=... python3 -I tools/wave.py run --workers 2 --budget 50000

`...` is the reset's calendar time. A user unit starts in the home
directory, so add `--working-directory=<this worktree>` and make sure
`claude-glm` is on the unit's `PATH`.

## Nothing from his IWD2EE branch enters the repository

His renderer is built into IEex, and his IWD2EE work is game-data overrides
and compiled binaries. None of it enters this repository — no game-data
overrides, no compiled binaries from his IWD2EE work, ever — excluded by
the bring-your-own-copy rule and by the gate. The success criterion is zero
bytes of his IWD2EE branch in the repository. What iwd2r reads of that work
is fetched by hash into a directory outside the tree, and the pins file
carries only URLs and sha256 hashes: pointer metadata, not bytes.

The artifacts are not in the `iwd2-re` fork. They live in
[WillScarlettOhara/IWD2EE](https://github.com/WillScarlettOhara/IWD2EE) at
commit `138495fde69b6092b7318fad462d571864e9d9b8`;
upstream IEex is [RedChimera/IWD2EE](https://github.com/RedChimera/IWD2EE)
at `60de54611b249d3bcca3f6522ef9db5e7ec27339`. Both sides are pinned — DLLs
and Lua from each — so the renderer streams can diff the fork's work
against upstream's. [tools/wso_pins.json](../../tools/wso_pins.json) is the
authoritative list: 27 files — the 3 DLLs, 20 fork Lua files (14 patch
scripts and 6 `_State` scripts; about 53,000 lines — the spec's early count
of 16 patch files was an undercount), the 2 upstream Lua files, and the 2
BG2:EE symbol-dump archives (`bg2_pdb_extracted.zip`, `BG2EE_rep.zip`)
pinned from the fork's own repo at `e8dd16a`, which
[tools/bg2_find.py](../../tools/bg2_find.py) indexes.

    python3 -I tools/wso_fetch.py --check    # verify the staged copies
    python3 -I tools/wso_fetch.py            # fetch by pinned hash

Both land under `${IWD2R_HOME:-~/.local/share/iwd2r}/wso/` and nowhere
else: a download is hashed in memory and written only when it matches its
pin. Two pins are zip archives (`unzip: true`): `wso_fetch` verifies the
archive's hash first, then extracts it beside the archive under the same
`wso/` tree — member names are sanitized (no absolute paths, no `..`, no
symlinks, no drive letters) and capped at 4,096 files and 2 GiB — and
nothing extracted is ever executed.

## Working beside the driver

A Claude stream and the batch driver share this repo, so each file has one
owner at a time. `wave-state.json` has one writer, the driver; nobody else
edits it. Claim before you edit and release at merge:
`python3 -I tools/claims.py claim <stream> <holder> <file>...`, then
`... release <stream>` ([tools/claims.py](../../tools/claims.py), recorded in
`docs/wso/claims.json`). Claims always go to the checkout that has `first`
checked out, whichever worktree you run the tool from, and the tool refuses
when none has it. Paths are stored repo-relative. `wave.py next` skips claimed files exactly as it
skips files of assigned batches. Run `git pull --ff-only` before every merge
into `first`. Keep one ledger per plan. `tools/gates.sh` takes an exclusive
lock on `.gates.lock` at the main repo root, so two gates never run at once;
`--no-wait` fails fast instead of waiting.
