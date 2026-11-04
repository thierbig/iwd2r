"""Differential emulation cases, one module per function, named by its
address in the original iwd2.exe (c578970.py -> 0x578970).

Contract (see run.py):
  CASE    declarative table: exe address, our map-resolved name + expected
          rva (cross-check only, resolution goes through truth._map_matches),
          calling convention, `this` class + size for layout.py, required
          member names, per-side extra addresses the map cannot name
          (ILT thunks, import bodies), extra map-resolved names.
  build(side, inp, pe, addrs, ctx) -> machine.Machine
          addrs: {'orig': VA, 'ours': VA, **CASE['extra'], **resolved extras}
          ctx:   {'off': {side: {member: offset}}, 'sent': machine.SENT,
                  'field_map': {exe_off: ours_off} | None}
  SETS    {name: input dict} in run order; include edge values.

Modules must stay importable without Unicorn (import machine inside build)
so tests/test_emu.py can validate the declarative tables on any interpreter.
"""

# Sentinel object addresses (inputs; never dereferenced by stubs).  They
# mirror machine.SENT (BLK_LO = 0x12000000 + 0x8000/0x9000/0xA000); restated
# here so case modules and their SETS stay importable without Unicorn.
# tests/test_emu.py cross-checks the two when .venv-emu is present.
BLK_LO = 0x12000000
SENT = {'anim': BLK_LO + 0x8000, 'shadow': BLK_LO + 0x9000, 'vidcell': BLK_LO + 0xA000}
