"""Differential emulation cases, one module per function, named by its
address in the original iwd2.exe (c578970.py -> 0x578970).

Contract (see run.py):
  CASE    declarative table: exe address, our function's map name, calling
          convention, `this` class + size for layout.py, required member
          names, and per-site address specs.  Nothing on our side may be a
          build-specific constant (fix 2): a spec is int (vanilla literal /
          shared constant), str or ('map', name) (resolved through
          truth._map_matches; ILT-thunked calls are caught by hooking the
          map-named body), ('import', 'dll!symbol') (IAT slot from our PE
          import table), or [spec, ...] for multi-site hooks.  Absence or
          ambiguity is a clean per-case ERROR 'not in this build: <name>'.
  build(side, inp, pe, addrs, ctx) -> machine.Machine
          addrs: {'orig': VA, 'ours': VA, **CASE['extra'] resolved}
          ctx:   {'off': {side: {member: offset}}, 'sent': machine.SENT,
                  'field_map': {exe_off: ours_off} | None}
  SETS    {name: input dict} in run order; include edge values.

Modules must stay importable without Unicorn (import machine inside build)
so tests/test_emu.py can validate the declarative tables on any interpreter.
"""

# Sentinel object addresses (inputs; never dereferenced by stubs).  They
# mirror machine.SENT (BLK_LO = 0x12000000 + 0x8000/0x9000/0xA000); restated
# here so case modules and their SETS stay importable without Unicorn.
# tests/test_emu.py cross-checks the two under the emulation venv.
BLK_LO = 0x12000000
SENT = {'anim': BLK_LO + 0x8000, 'shadow': BLK_LO + 0x9000, 'vidcell': BLK_LO + 0xA000}
