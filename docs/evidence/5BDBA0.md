---
address: 0x5BDBA0
name: CInfGame::sub_5BDBA0
exemption: unmeasured WSO import: world_entry, first_fight, walk_and_talk, action_bar, equip_shots, inventory, level_up, world_map: not exercised
kind: read-only
ghidra_version: 12.1.4
---

# 0x5BDBA0 CInfGame::sub_5BDBA0

## Decompiled (Ghidra)

```c
void CInfGame::sub_5BDBA0(short param_1,int param_2)

{
  int in_ECX;

  param_1 = param_1 * 10;
  *(short *)(in_ECX + 0x387a) = param_1;
  if (param_2 != 0) {
    if (200 < param_1) {
      param_1 = 200;
    }
    *(short *)(in_ECX + 0x387a) = param_1;
    if (param_1 < 10) {
      *(undefined2 *)(in_ECX + 0x387a) = 10;
      return;
    }
    *(short *)(in_ECX + 0x387a) = param_1;
  }
  return;
}
```

## Callers

- 0x651E30 FUN_00651e30
- 0x6669E0 CUIControlButtonSinglePlayerModifyCharacterDelete::OnLButtonClick

## Callees

- none

## Notes

2026-11-02: verified against the exe (objdump, IWD2.exe md5
200f038bec3ed50a964d68b3f0f1f0e7) before importing: the argument arrives in
full 32 bits and is multiplied by 10 before every use (Ghidra's `short`
parameter loses that; our `int a1` keeps it), each store is a WORD store to
+0x387a which our header names `m_nReputation` (CInfGame.h, /* 387A */), the
200 comparison is signed, and the 10 floor returns immediately. The fork's
body (SetReputation) used min/max with the same final state; ours follows the
exe's store order in the style of the sibling ReputationAdjustment (0x5BDC00,
"1..20 in game terms, stored x10"). Semantics: set reputation to a1 whole
points; a2 nonzero clamps to the 1..20 range.
