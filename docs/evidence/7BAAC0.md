---
address: 0x7BAAC0
name: GLSetupResetDriver
exemption: "tears down GLSetup driver state that nothing in this tree sets up (our Init3d keeps loading opengl32.dll directly, so the probe never commits a driver); checked against the disassembly"
kind: read-only
ghidra_version: 12.1.4
---

# 0x7BAAC0 GLSetupResetDriver

Imported from the WSO fork body (see the commit trailer); read against the disassembly of IWD2.exe. New code block in src/CChitin3d.cpp (a dependency of the GLSetupSelectDriver import; the probe calls it on entry and again when a driver is rejected).

Read against 0x7BAAC0-0x7BB0xx: the three module handles are released via the FreeLibrary import at 0x8472C0 in the order 0x906914 (g_hGLSetupDriver), 0x906918 (g_hGLSetupMini), 0x906910 (g_hGLSetupGdi32), each re-zeroed only when nonzero. The FX_GLIDE_NO_SPLASH restore runs when control bit 0x10 (0x9074AC) is clear and the one-shot cache at 0x9074B4 is set: strlen("FX_GLIDE_NO_SPLASH" @ 0x8BD544) + strlen(cached value @ 0x9074B8, empty when NULL) < 0x100 guards a sprintf "%s=%s" into a 0x100 stack buffer and _putenv, then 0x9074B4 is cleared. The OverrideDispatch restore runs when control bit 0x20 is clear and 0x9074BC is set: RegCreateKeyExA (import 0x847010) with HKEY_LOCAL_MACHINE (0x80000002), "Software\Silicon Graphics\OpenGL" (0x8BD518), KEY_ALL_ACCESS (0xF003F); on success RegSetValueExA (0x847014) writes "OverrideDispatch" (0x8BD504) as REG_DWORD, 4 bytes from 0x9074C0, then RegCloseKey (0x847008); 0x9074BC is cleared either way. The entry-point table is cleared to ESI = 0, or to the safe-stub thunk 0x7BB5C0 when control bit 0x40 is set (the fork keeps this as an explicit address cast with a HACK comment); stores run over the g_pfnGLSetup* slots (0x9074C8-0x907504 area) and the CVideo3d:: table from 0x907514 in the fork body's declaration order, then the canonical paths at 0x9072A4 / 0x9073A8 are emptied and the selected index at 0x8BB0A0 is set to -1. The fork body spells every one of these steps in the same order; only its static linkage and its winver.h/mbstring.h build assumptions were adapted to this tree (non-static, file-local registry/_putenv shims that fail gracefully).
