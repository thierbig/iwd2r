#!/usr/bin/env bash
# Creates the differential-emulation venv under $IWD2R_HOME/emu-venv (user-local,
# no sudo, shared by every worktree) with the exact pins from requirements.txt
# (tools/emu/run.py needs Unicorn; tools/oracle.py emu relays into this venv).
# Idempotent: an existing venv that already satisfies the pins is left alone.
#
# As in tools/parity/setup.sh: this system's python3 has no pip/ensurepip
# (python3-venv ships without it), so the venv is created --without-pip and
# pip itself is bootstrapped from the official get-pip installer, pinned by
# version.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="${IWD2R_HOME:-$HOME/.local/share/iwd2r}"
venv="$root/emu-venv"
pip_pin=24.0

satisfied() {
  [ -x "$venv/bin/python" ] || return 1
  "$venv/bin/python" -I - "$here/requirements.txt" <<'PY' 2>/dev/null
import importlib.metadata as md
import sys
try:
    for line in open(sys.argv[1]):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, ver = line.partition("==")
        if md.version(name.replace("-", "_")) != ver:
            sys.exit(1)
except Exception:
    sys.exit(1)
sys.exit(0)
PY
}

pdbutil() {
  # tools/emu/layout.py reads our build's class layouts from its .pdb.
  if command -v llvm-pdbutil-18 >/dev/null 2>&1; then
    echo "llvm-pdbutil : $(command -v llvm-pdbutil-18)"
  else
    echo "llvm-pdbutil : missing; cases with a this-class need it (apt install llvm-18)"
  fi
}

if satisfied; then
  echo "emu venv already satisfies $(grep -v '^#' "$here/requirements.txt" | tr -s ' \n' ' ')"
  pdbutil
  exit 0
fi

mkdir -p "$root"

if [ ! -x "$venv/bin/python" ]; then
  echo "creating venv $venv ($(python3 -I -V 2>&1))"
  python3 -I -m venv --without-pip "$venv"
fi

if ! "$venv/bin/python" -I -m pip --version >/dev/null 2>&1; then
  tmp=$(mktemp -d "$root/.getpip.XXXXXX")
  trap 'rm -rf "$tmp"' EXIT
  # The rolling official installer (per-version /pip/<v>/ paths exist only for
  # EOL pythons); the pip it installs is pinned below.
  curl -fsSL --retry 3 -o "$tmp/get-pip.py" "https://bootstrap.pypa.io/get-pip.py"
  "$venv/bin/python" -I "$tmp/get-pip.py" "pip==$pip_pin"
fi

"$venv/bin/pip" install -q -r "$here/requirements.txt"

echo
echo "Install root : $root"
echo "emu venv     : $venv"
"$venv/bin/pip" freeze 2>/dev/null | grep -Ei '^(unicorn|pip)==' | sed 's/^/  /'
pdbutil
echo "Entry        : python3 -I tools/oracle.py emu --list"
