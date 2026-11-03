---
address: 0x7BAAC0
name: GLSetupResetDriver
exemption: unmeasured WSO import: world_entry, first_fight, walk_and_talk, action_bar, equip_shots, inventory, level_up, world_map: not exercised
kind: read-only
parity_waiver: "asm: _putenv (0x7EA143, the FX_GLIDE_NO_SPLASH restore site) is spelled GLSetupPutEnv (local no-op stand-in; MSVC/glibc name collision), so the source never spells _putenv - the call is present, renamed; DebugBreak keeps its name. No game call is missing from the imported body."
ghidra_version: 12.1.4
---

# 0x7BAAC0 GLSetupResetDriver

Imported from the WSO fork body (see the commit trailer); read against the disassembly of IWD2.exe. Free function (static in src/CChitin3d.cpp).

Read against the disassembly FUN_007baac0: FreeLibrary + NULL for each loaded handle in order driver (0x906914), mini (0x906918), gdi32 (0x906910), each guarded non-NULL; the splash restore under `!(DAT_009074ac(g_dwGLSetupControl) & 0x10) && checked(0x9074B4)` sprintf's "FX_GLIDE_NO_SPLASH=%s" from the cached getenv pointer (or "" when NULL) and _putenv's it, then clears the checked flag; the OverrideDispatch restore under `!(control & 0x20) && checked(0x9074BC)` RegCreateKeyExA's HKLM "Software\Silicon Graphics\OpenGL" (KEY_ALL_ACCESS) and RegSetValueExA's "OverrideDispatch" (REG_DWORD, 4 bytes of the cached 0x9074C0 value) + RegCloseKey, then clears the flag; `control & 0x40` selects DebugBreak as the stub every entry-point slot gets, else NULL. The sweep assigns that stub to the whole resolved table (the 0x907514.. block: 336 gl into the CVideo3d members, 51 glu, the WGL and pixel-format statics, g_pfnGLSetupSwapBuffers last at 0x9074C8). Tail: g_szGLSetupPrimaryPath(0x9072A4)[0] = 0, g_szGLSetupMiniPath(0x9073A8)[0] = 0, g_nSelectedGLSetupDriver(0x8BB0A0) = -1. Port note: FreeLibrary returns TRUE and handles stay NULL on the port (platform/win32_stubs.cpp), the Reg* shims fail closed, g_dwGLSetupControl is never written so the 0x40 DebugBreak branch is unreachable, and the reset reduces to NULLing the table and paths.
