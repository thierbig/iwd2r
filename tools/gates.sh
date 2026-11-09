#!/usr/bin/env bash
# The CI gate, run locally: the same steps .github/workflows/ci.yml runs,
# for when GitHub Actions is disabled (gh workflow disable CI) to save
# minutes. Linux presets always; the Windows builds with --windows.
#
#   tools/gates.sh [--base REV] [--windows] [--skip-clang] [--no-wait]
#                  [--full] [--summary-fixture] [--worktree]
#                  [--require-game-data] [--require-complete-parity]
#
# --base is the evidence check's base (default: iwd2r/main, what CI's push
# event would have used). Engine tests run serially and muted. Exits 1 on the
# first failing step and prints a summary.
#
# Pre-push preflight (all opt-in; the defaults above are unchanged), the
# switches a maintainer-bound local run uses to fail closed where the
# default gate skips or ignores:
#   --worktree                 the decompiler-evidence check and structural
#                              parity audit the working tree (staged,
#                              unstaged and untracked files) through the
#                              tools' own --worktree mode, so uncommitted
#                              edits cannot pass uncovered -- the default
#                              checks are committed-only and read a dirty
#                              tree as "0 touched functions". No local
#                              commit is needed for coverage.
#   --require-game-data        fail before any step unless actual game data
#                              is present: CHITIN.KEY after the default
#                              discovery below (IWD2_DATA, else the
#                              Linux-local copy).
#   --require-complete-parity  structural parity runs strict
#                              (--require-complete): a missing vanilla exe,
#                              a base it cannot diff, or missing
#                              functions.tsv/callgraph.tsv is a failure,
#                              not an exit-0 SKIP.
# Each has an env default (1 enables it): IWD2R_GATE_WORKTREE,
# IWD2R_GATE_REQUIRE_GAME_DATA, IWD2R_GATE_REQUIRE_COMPLETE_PARITY.
# Practical pre-push command, from the worktree under test:
#   tools/gates.sh --base first --worktree --require-game-data --require-complete-parity
# On a docs-only change the build, test and parity steps keep taking the
# fast path below even with the preflight flags (nothing under src/
# changed); the parity step still skips there rather than re-checking the
# base's engine state.
#
# One gate runs at a time: gates.sh holds an exclusive flock on .gates.lock at
# the MAIN repo root (shared by every worktree) and a second gate waits for it.
# --no-wait fails fast, naming the holder, instead. --dry-lock SECONDS takes
# the lock, holds it that long and exits 0 without running any step (tests).
# The summary prints `step <name>: <seconds>s` per step, `gates: total <s>s` and
# whether ccache was used (both Linux presets, when `command -v ccache`
# succeeds). --summary-fixture runs two trivial steps and the summary (tests).
# Exit status: 3 for --no-wait with the lock held, 4 when flock itself fails.
# Docs-only fast path: when --base is given and the branch changed no engine
# input against it (may_skip below is an allow-list of the paths that none of
# the engine build, ctest, the replay/scenario tests or the gate itself read;
# any other path runs the full gate), the linux-gcc, linux-clang and
# structural parity steps print `skipped (docs-only change against BASE)`
# and the summary says `gates: docs-only fast path`; every other step runs.
# --full forces the full gate; without --base (CI's default) it is always full;
# a BASE that is not an ancestor of HEAD, or that cannot be diffed, is full.
# A docs-only close does not re-check the engine state of the base; the next
# engine batch's full gate does.
# The lock file is git-ignored: `git clean -X` deletes it mid-hold, and a new
# gate would then lock a fresh inode and run beside the old one.
set -u
cd "$(dirname "$0")/.."
BASE=iwd2r/main; BASE_GIVEN=0; FULL=0; WINDOWS=0; CLANG=1; NOWAIT=0; DRYLOCK=; FIXTURE=0
# The pre-push preflight switches (header above): the env vars are the
# defaults, the flags enable on top of them.
WORKTREE="${IWD2R_GATE_WORKTREE:-0}"
REQUIRE_GAME_DATA="${IWD2R_GATE_REQUIRE_GAME_DATA:-0}"
REQUIRE_COMPLETE_PARITY="${IWD2R_GATE_REQUIRE_COMPLETE_PARITY:-0}"
while [ $# -gt 0 ]; do
    case "$1" in
        --base) BASE="$2"; BASE_GIVEN=1; shift 2 ;;
        --full) FULL=1; shift ;;
        --windows) WINDOWS=1; shift ;;
        --skip-clang) CLANG=0; shift ;;
        --no-wait) NOWAIT=1; shift ;;
        --dry-lock) DRYLOCK="$2"; shift 2 ;;
        --summary-fixture) FIXTURE=1; shift ;;
        --worktree) WORKTREE=1; shift ;;
        --require-game-data) REQUIRE_GAME_DATA=1; shift ;;
        --require-complete-parity) REQUIRE_COMPLETE_PARITY=1; shift ;;
        *) echo "gates: unknown option $1" >&2; exit 2 ;;
    esac
done
# The shared --worktree argument for the evidence check and parity below.
WORKTREE_ARG=""
[ "$WORKTREE" = 1 ] && WORKTREE_ARG="--worktree"
# Whether a changed path MAY skip the engine steps: an allow-list, so anything
# unknown (a root-level file, a new directory, CMakeUserPresets.json) runs the
# full gate. tools/ is allowed except what the gate or the C++ tests read:
# the gate itself (gates.sh, parity/), tools/dataview_rules.json (the C++
# tests), tools/check_evidence.py and tools/oracle.py.
may_skip() {
    case "$1" in
        tools/gates.sh|tools/parity/*|tools/dataview_rules.json|tools/check_evidence.py|tools/oracle.py) return 1 ;;
        docs/*|.superpowers/*|.github/*|tools/*) return 0 ;;
        README.md|LICENSE.md|tests/test_tools.py) return 0 ;;
    esac
    return 1
}
# Sets DOCS_ONLY=1 when the fast path applies. Every git failure fails closed
# (full gate). Renames are listed as two paths (--no-renames), so moving an
# engine file into docs/ still names the engine path.
DOCS_ONLY=0
detect_docs_only() {
    local list f rc=0
    [ "$FULL" = 0 ] && [ "$BASE_GIVEN" = 1 ] || return 0
    git rev-parse --verify -q "$BASE^{commit}" >/dev/null 2>&1 || return 0
    git merge-base --is-ancestor "$BASE" HEAD 2>/dev/null || return 0
    list=$(mktemp) || return 0
    # Uncommitted and untracked paths count too: the gate certifies the tree.
    { git -c core.quotePath=false diff --no-renames -z --name-only "$BASE"...HEAD \
        && git -c core.quotePath=false diff --no-renames -z --name-only HEAD \
        && git -c core.quotePath=false ls-files -z --others --exclude-standard
    } > "$list" 2>/dev/null || rc=1
    if [ "$rc" = 0 ]; then
        while IFS= read -r -d '' f; do
            may_skip "$f" || { rc=1; break; }
        done < "$list"
    fi
    rm -f "$list"
    [ "$rc" = 0 ] && DOCS_ONLY=1
    return 0
}
detect_docs_only
# Pre-push preflight (opt-in, header above): require actual game data
# before any step runs or the lock is taken. The same default discovery
# the engine steps use below (IWD2_DATA wins, else the Linux-local copy).
# ${HOME:-} keeps a HOME-less environment (the summary fixture's minimal
# env) from tripping set -u here, where the original assignment sat after
# the fixture exit and was never reached by one.
LINUX_DATA="${IWD2R_HOME:-${HOME:-}/.local/share/iwd2r}/game"
discover_game_data() {
    [ -z "${IWD2_DATA:-}" ] && [ -f "$LINUX_DATA/CHITIN.KEY" ] && export IWD2_DATA="$LINUX_DATA"
    return 0
}
if [ "$REQUIRE_GAME_DATA" = 1 ]; then
    discover_game_data
    if [ -z "${IWD2_DATA:-}" ] || [ ! -f "$IWD2_DATA/CHITIN.KEY" ]; then
        echo "gates: --require-game-data: no CHITIN.KEY after default discovery" >&2
        echo "gates:   set IWD2_DATA to a game-data root, or make the Linux copy at $LINUX_DATA" >&2
        echo "gates:   (the rsync line in the comment above the engine steps in this script); or drop --require-game-data" >&2
        exit 1
    fi
    echo "gates: --require-game-data: game data at $IWD2_DATA/CHITIN.KEY"
fi
failed=()
GATE_START=$SECONDS
USE_CCACHE=0
command -v ccache >/dev/null 2>&1 && USE_CCACHE=1
step() {
    local name="$1" t0=$SECONDS; shift
    echo "=== $name"
    # fd 9 (the gate lock) is closed in every child: a surviving orphan must
    # not keep the lock after this script is killed.
    if "$@" 9>&-; then echo "--- $name: ok"; else echo "--- $name: FAILED"; failed+=("$name"); fi
    echo "step $name: $((SECONDS - t0))s"
}
# A step the docs-only fast path skips.
gated_step() {
    if [ "$DOCS_ONLY" = 1 ]; then echo "=== $1: skipped (docs-only change against $BASE)"; return 0; fi
    step "$@"
}
summary() {
    echo
    if [ "$DOCS_ONLY" = 1 ]; then echo "gates: docs-only fast path"; else echo "gates: full gate"; fi
    echo "gates: total $((SECONDS - GATE_START))s"
    if [ "$USE_CCACHE" = 1 ]; then echo "gates: ccache: installed, launcher set for ${PRESETS_BUILT:-no preset (none built)}"
    else echo "gates: ccache not used (not installed)"; fi
    if [ ${#failed[@]} -eq 0 ]; then echo "gates: all passed"; return 0; fi
    echo "gates: FAILED: ${failed[*]}"; return 1
}
if [ "$FIXTURE" = 1 ]; then
    step "fixture one" true
    gated_step "linux-gcc build and tests" true
    [ "$CLANG" = 1 ] && gated_step "linux-clang build and tests" true
    gated_step "structural parity (base $BASE)" true
    step "fixture two" true
    summary; exit $?
fi
# The lock lives at the main repo root, not at this worktree's root: batch
# worktrees run this script too, and a per-worktree lock would serialise nothing.
GATE_LOCK="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")/.gates.lock"
command -v flock >/dev/null 2>&1 || { echo "gates: flock (util-linux) is missing; refusing to run without the gate lock" >&2; exit 4; }
exec 9>>"$GATE_LOCK" || { echo "gates: cannot open $GATE_LOCK" >&2; exit 4; }
# flock -n exits 77 (-E) only when the lock is held elsewhere; any other
# failure is a real error and never lets the gate run unlocked.
flock -n -E 77 9; lock_rc=$?
if [ "$lock_rc" = 77 ]; then
    holder=$(head -n 1 "$GATE_LOCK" 2>/dev/null)
    if [ "$NOWAIT" = 1 ]; then
        echo "gates: another gate holds $GATE_LOCK ($holder); not waiting (--no-wait)" >&2
        exit 3
    fi
    echo "gates: waiting for the gate lock $GATE_LOCK ($holder)" >&2
    flock 9; lock_rc=$?
fi
[ "$lock_rc" = 0 ] || { echo "gates: flock failed on $GATE_LOCK (status $lock_rc)" >&2; exit 4; }
# Record the holder for the messages above (the lock is on the inode, so
# rewriting the file through its path keeps it).
echo "held by pid $$ in $PWD since $(date -u +%FT%TZ)" > "$GATE_LOCK"
GATE_START=$SECONDS   # the total excludes the wait for the lock
if [ -n "$DRYLOCK" ]; then sleep "$DRYLOCK" 9>&-; exit 0; fi
export IWD2R_AUDIO=null SDL_AUDIO_DRIVER=dummy
# Data tests read a Linux-local copy of the vanilla game when one exists:
# the oracle's copy on the Windows drive is locked while a sandbox replay
# maps it, which fails every engine-starting test with "bad key file".
# Its icewind2.ini [Alias] paths still name the oracle copy, but the engine maps
# every drive path into the data root (platform/fs.cpp), so a Linux run never
# reads the Windows drive. Make it with: rsync -a --exclude temp --exclude cache --exclude tempsave
#   --exclude mpsave --exclude ScrnShot /mnt/c/Users/<you>/iwd2r-oracle/game/ ~/.local/share/iwd2r/game/
# Tests never write the copy: every engine-starting test runs on a private
# view of it (tests/data_view.h), and copy_unchanged below fails a preset whose
# tests changed it anyway.
discover_game_data
# The start-screen smoke test's frame hash depends on the game copy, so each
# machine pins its own: tests/smoke_start_screen.cpp requires the hash in
# IWD2R_SMOKE_HASH when it is set. The test fixes what else the menu reads
# (noon local time: the day background; no saves: Quick Load disabled).
SMOKE_HASH_FILE="${IWD2R_HOME:-$HOME/.local/share/iwd2r}/smoke-hash"
if [ -z "${IWD2R_SMOKE_HASH:-}" ] && [ -f "$SMOKE_HASH_FILE" ]; then
    IWD2R_SMOKE_HASH=$(tr -d ' \r\n' < "$SMOKE_HASH_FILE"); export IWD2R_SMOKE_HASH
elif [ -z "${IWD2R_SMOKE_HASH:-}" ]; then
    echo "gates: the smoke frame hash is not pinned; run build/linux-gcc/tests/iwd2r_tests \"start screen smoke\" and save the framebuffer-hash it prints in $SMOKE_HASH_FILE"
fi
# Lavapipe (Mesa's software Vulkan) runs the gpu presenter's tests: on a
# machine that has it, a skipped [present] case means they did not run.
LAVAPIPE=0
if ls /usr/lib/*/libvulkan_lvp.so /usr/lib/libvulkan_lvp.so /usr/lib64/libvulkan_lvp.so >/dev/null 2>&1 \
    || { command -v vulkaninfo >/dev/null 2>&1 && vulkaninfo --summary 2>/dev/null | grep -qi llvmpipe; }; then
    LAVAPIPE=1
fi
# Engine tests run under a memory cap. A test that allocates without bound
# (on 2026-10-14 a creature-construction sweep grew iwd2r_tests to ~29 GB)
# otherwise triggers the kernel's OOM kill, which takes down the whole WSL
# VM and every agent in it. Under the cap only the test process dies.
# IWD2R_TEST_MEMORY_MAX overrides the limit; systemd-run must be available.
TEST_MEMORY_MAX="${IWD2R_TEST_MEMORY_MAX:-8G}"
capped() {
    if ! command -v systemd-run >/dev/null 2>&1; then
        echo "gates: systemd-run is missing; refusing to run engine tests without a memory cap" >&2
        return 1
    fi
    systemd-run --user --scope -q -p MemoryMax="$TEST_MEMORY_MAX" -p MemorySwapMax=0 -- "$@"
}
# The copy's state (every entry's path, type, size and modification time),
# taken before the first preset's tests. A test that writes the copy makes
# later runs depend on it (2026-10-13: Characters/PROBE*, Party.ini and the
# INI files), so a change fails the preset and lists what changed.
copy_manifest() {
    find "$IWD2_DATA/" -printf '%P\t%y\t%s\t%T@\n' 2>/dev/null | LC_ALL=C sort
}
COPY_MANIFEST=""
if [ -n "${IWD2_DATA:-}" ] && [ -d "$IWD2_DATA" ]; then
    COPY_MANIFEST=$(mktemp); copy_manifest > "$COPY_MANIFEST"
fi
copy_unchanged() {
    [ -n "$COPY_MANIFEST" ] || return 0
    local now
    now=$(mktemp); copy_manifest > "$now"
    if ! cmp -s "$COPY_MANIFEST" "$now"; then
        echo "gates: the tests changed the game copy $IWD2_DATA (< before, > after; path, type, size, mtime):"
        diff "$COPY_MANIFEST" "$now" | grep '^[<>]' | head -50
        # The next preset reports only its own changes.
        mv "$now" "$COPY_MANIFEST"
        return 1
    fi
    rm -f "$now"
}
linux_preset() {
    local preset="$1" log rc skipped
    PRESETS_BUILT="${PRESETS_BUILT:+$PRESETS_BUILT, }$preset"
    # ccache: the launcher is a cache variable, so an existing build directory
    # is reconfigured once when it does not match (and cleared when ccache is
    # gone, or every compile would fail on the missing launcher).
    local want="" cache="build/$preset/CMakeCache.txt"
    [ "$USE_CCACHE" = 1 ] && want=ccache
    if [ ! -f "$cache" ] || [ "$(sed -n 's/^CMAKE_CXX_COMPILER_LAUNCHER:[A-Z]*=//p' "$cache")" != "$want" ]; then
        cmake --preset "$preset" -DCMAKE_C_COMPILER_LAUNCHER="$want" \
            -DCMAKE_CXX_COMPILER_LAUNCHER="$want" || return 1
    fi
    cmake --build --preset "$preset" || return 1
    log=$(mktemp)
    # The cases are independent processes (data views, no shared state), so
    # they run in parallel under the one memory cap; IWD2R_CTEST_JOBS=1 runs
    # them one at a time.
    capped ctest --preset "$preset" --output-on-failure -j "${IWD2R_CTEST_JOBS:-8}" | tee "$log"
    rc=${PIPESTATUS[0]}
    if [ "$LAVAPIPE" = 1 ]; then
        # The [present] cases ctest reports as skipped (each test is its own
        # ctest entry, named as the TEST_CASE).
        skipped=$("build/$preset/tests/iwd2r_tests" "[present]" --list-tests --verbosity quiet 2>/dev/null \
            | while IFS= read -r name; do
                [ -n "$name" ] && grep -F -- " $name " "$log" | grep -q 'Skipped' && echo "$name"
            done)
        if [ -n "$skipped" ]; then
            echo "gates: lavapipe is installed but these gpu presenter tests were skipped (no Vulkan device in the test):"
            echo "$skipped" | sed 's/^/  /'
            rc=1
        fi
    fi
    rm -f "$log"
    copy_unchanged || rc=1
    return "$rc"
}
gated_step "linux-gcc build and tests" linux_preset linux-gcc
[ "$CLANG" = 1 ] && gated_step "linux-clang build and tests" linux_preset linux-clang
step "tool tests" python3 -I tests/test_tools.py
step "subsystem map" python3 -I tools/gen_map.py --check
step "recovery status" python3 -I tools/re_status.py --check
step "replay fields" python3 tools/gen_fields.py --check
[ -f tools/shaders.py ] && step "shader blobs" python3 -I tools/shaders.py --check
step "decompiler evidence (base $BASE)" python3 -I tools/check_evidence.py --base "$BASE" $WORKTREE_ARG
# Structural parity (tools/parity/run.py): the changed functions' call graph,
# vtables, struct layout, member offsets and address markers against the
# vanilla exe. The exe-based columns need pefile/capstone from the parity venv
# (tools/parity/setup.sh); on bare python3 they SKIP and the lints still run.
# Without the exe there is nothing to diff against. --require-complete-parity
# (the pre-push preflight) instead runs it strict: run.py's own SKIPs (no
# vanilla exe, an undiffable base, missing functions.tsv/callgraph.tsv) are
# failures, so there is no exe pre-check on that path.
parity_py=python3
parity_venv="${IWD2R_HOME:-$HOME/.local/share/iwd2r}/parity-venv/bin/python"
[ -x "$parity_venv" ] && parity_py="$parity_venv"
parity_step() {
    local exe
    if [ "$REQUIRE_COMPLETE_PARITY" = 1 ]; then
        "$parity_py" tools/parity/run.py --base "$BASE" $WORKTREE_ARG --require-complete
        return
    fi
    exe=$(python3 -I -c "import sys; sys.path.insert(0, 'tools'); import oracle; print(oracle.VANILLA_EXE)")
    if [ -f "$exe" ]; then
        "$parity_py" tools/parity/run.py --base "$BASE" $WORKTREE_ARG
    else
        echo "structural parity: skipped (no vanilla exe)"
    fi
}
gated_step "structural parity (base $BASE)" parity_step
format_check() {
    local files
    files=$(git diff --name-only --diff-filter=d 4fa36bc -- '*.cpp' '*.h' | grep -v '^src/' || true)
    [ -z "$files" ] || clang-format --dry-run --Werror $files
}
step "format" format_check
if [ "$WINDOWS" = 1 ]; then
    # The x86 preset with the oracle runner, injector and recorder; the x64
    # engine and the x86 engine (what tools/frida/probe.py runs) through
    # play.py's build helpers. Windows ctest runs are covered
    # by the sandbox suite (play.py --vm windows --suite) for now.
    step "windows x86 oracle build" python3 tools/oracle.py build
    step "windows x64 build" python3 -c "import sys; sys.path.insert(0, 'tools'); import play; play.sync_windows(); play.build_windows()"
    step "windows x86 engine build (frida probe)" python3 -c "import sys; sys.path.insert(0, 'tools'); import play; play.sync_windows(); play.build_windows_x86()"
fi
[ -n "$COPY_MANIFEST" ] && rm -f "$COPY_MANIFEST"
summary; exit $?
