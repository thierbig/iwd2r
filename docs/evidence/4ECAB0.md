---
address: 0x4ECAB0
name: CtrlAltDelete_AddGold
exemption: "no oracle case: the only caller is the unported console dispatcher 0x5838A0; adding gold needs a live party. The forward is pinned in-tree against the listing below."
kind: read-only
ghidra_version: 12.1.4
---

# 0x4ECAB0 CtrlAltDelete_AddGold

A debug console "CtrlAltDelete:" cheat-command handler, free function
consuming one DWORD by value (`ret 4`), 25 bytes (0x4ECAB0-0x4ECAC8).
Ported in commit (see report).

## Body

Loads the argument from [esp+4], takes the world screen from
g_pBaldurChitin+0x1C88 (spelled `GetScreenWorld()` in the port), pushes
the argument and calls `CScreenWorld::AddGold` (0x6907E0); `ret 4`
then pops the handler's own argument. The port:

```cpp
g_pBaldurChitin->GetScreenWorld()->AddGold(dwGold);
```

Same shape as CtrlAltDelete_SetCurrentXP (0x4EC370) with the gold
callee.

## ecx / calling convention

ecx is never read. `ret 4` pops the DWORD argument; the callee
(0x6907E0) pops its own. Free function.

## Callers

Not in tree: the console dispatcher 0x5838A0 (deliberately unported;
docs/followups.md).

## Test

None: the callee reaches the party's gold, which needs a loaded game.
Verified against the listing above.
