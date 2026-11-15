# Cluster classification: unassigned rows above 0x5AF540 (Task 84r, STEP 1)

The remaining unassigned rows above 0x5AF540 (no `// 0x` marker in src/,
absent from WillScarlettOhara's fork, outside every contributor's assigned
area), one verdict each. Verdicts: (a) covered by the tree, (b)
compiler/STL/MFC template instance or thunk with no game logic, (c) real
game code to port.

Method: every row was read with `tools/parity/annotate.py 0xADDR` (full
listings kept in /tmp/t84r/annot_*.txt), its callers were searched across
the whole callgraph line (callgraph.tsv lists each caller once with all of
its callees in the remaining columns), and interesting bodies were
cross-checked against the tree (layout, existing markers — including
zero-padded ones — and in-tree caller bodies). Note two survey rows were
already marked with zero-padded markers (`// 0x0632530`,
`// 0x06475B0`), which the survey's marker scan missed.

Tally: 11 (a), 16 (b), 15 (c).

| address | size | ghidra name | class | verdict | evidence |
|---|---|---|---|---|---|
| 0x5CAE90 | 64 | FUN_005cae90 | STL copy helper | (b) | Range copy of 16-byte records (src,end,dst) until src==end, returns dst — memcpy loop semantics; callers 0x5A69B0 (CInfGame, tree) and 0x60BA30 (CScreenCreateChar, tree). |
| 0x5F6710 | 113 | FUN_005f6710 | FileFormat.h accessor | (b) | Element accessor for the FileFormat.h 16-byte vector: UtilAssert 0x780C00 on `.\Include\FileFormat.h` lines 0x985/0x986, returns begin+nIndex*0x10; sole caller 0x5EFFD0 (tree, CScreenCharacter/CRuleTables). |
| 0x5F9810 | 82 | FUN_005f9810 | STL vector helper | (b) | MSVC STL vector<16-byte POD> copy/append helper walking first/last cursors at this+4/this+8, 4-dword record copy, end pointer update; callers 0x5D63B0/0x60A5C0/0x60B610/0x6F2D80/0x70AE20. |
| 0x5F98B0 | 226 | FUN_005f98b0 | STL map teardown | (b) | STL tree remove-all: nil sentinel 0x8D13E8, node teardown 0x44A3F0/0x449ED0/0x44B230, operator_delete 0x7FC984, lock 0x7F9A11/0x7F9AAD, refcount 0x8D13E4. |
| 0x5F99A0 | 217 | FUN_005f99a0 | STL map teardown | (b) | Second COMDAT of the same map teardown; identical ten-caller set to 0x5F98B0. |
| 0x60BB80 | 90 | FUN_0060bb80 | STL vector clear | (b) | vector<16-byte POD> clear: (end-begin)>>4 count, in-place 4-dword record copy, end=begin, zeroes +0x14/+0x18. |
| 0x632530 | 484 | CUIControlButtonInventoryColorChoice ctor | (a) | Already ported — zero-padded marker `// 0x0632530` at src/CScreenInventory.cpp:6717. |
| 0x646940 | 95 | FUN_00646940 | MFC RemoveAll | (b) | MFC CPlex RemoveAll: vptr store 0x856598, chain-walk [eax] to NULL, zero +4..+0x14, CPlex_FreeDataChain 0x7FB7C8; caller 0x6469A0 plus vtable slot 0x82DBE4 (virtual dispatch). |
| 0x6475B0 | 327 | CScreenMovies::OnMovieItemSelect | (a) | Already ported — zero-padded marker `// 0x06475B0` at src/CScreenMovies.cpp:256. |
| 0x694510 | 267 | CScreenWorld_B3AUTO_00694510 | (c) | Real CScreenWorld game code; sole caller 0x6872D0 in tree (src/CScreenWorld.cpp:606). |
| 0x6A0350 | 151 | FUN_006a0350 | (a) | COMDAT duplicate of `CUIControlButton::~CUIControlButton` 0x4D4B80 (tree src/CUIControlButton.cpp:100). |
| 0x727C90 | 63 | CopySpellListEntries | (c) | Copies nCount 0x10-byte records src→dst, advancing dst even for NULL src entries; sole caller 0x725950 in tree (src/CGameSpriteSpells.cpp:22). |
| 0x767260 | 95 | FUN_00767260 | (b) | Same MFC CPlex RemoveAll shape as 0x646940 with vptr 0x85C210; caller 0x767240. |
| 0x77A4F0 | 151 | FUN_0077a4f0 | (a) | COMDAT duplicate of `CUIControlButton::~CUIControlButton` 0x4D4B80 (tree src/CUIControlButton.cpp:100). |
| 0x77BF30 | 1410 | FUN_0077bf30 | (c) | Game code, CLUAConsole-scale; no callers in the callgraph. |
| 0x781630 | 236 | FUN_00781630 | (c) | ChUtil bit-field extraction helper (value,nBits,nLowBits) with UTIL_ASSERT "nBits <= 32" (ChUtil.cpp:0x489); sole caller 0x4C8B10 in tree (src/CGameSound.cpp:8). |
| 0x784470 | 1036 | CResourceManager_CopyMPTempFiles | (c) | Real CResourceManager code (MP temp-file copy/delete); sole caller 0x64C600 (not in tree). |
| 0x787400 | 832 | CResourceManager_DefunctCtrlODebug | (c) | Real CResourceManager debug code; sole caller 0x6872D0 in tree (src/CScreenWorld.cpp:606). |
| 0x7891E1 | 109 | FUN_007891e1 | (a) | Tail fragment of `CDimm::WriteFile` (0x789090), in tree at src/CDimm.cpp:2788. |
| 0x78C90B | 312 | FUN_0078c90b | (a) | Continuation of `CResCache::CopyFile` (0x78C4F0), in tree at src/CResCache.cpp:185. |
| 0x78D730 | 192 | CGamePathsList_AddPath | (b) | MFC list AddTail: CPlex_Create 0x7FB7A8 growth, node link at +4/+8, CString assign 0x7FCD07 — container mechanics; callers 0x782240 (tree src/CDimm.cpp) and 0x78D860. |
| 0x78D7F0 | 108 | FUN_0078d7f0 | (b) | MFC CPlex RemoveAll: vptr store 0x85DBF8, per-node destruct via 0x7FAFC7, zero +4..+0x14, CPlex_FreeDataChain 0x7FB7C8; caller 0x78D940 plus vtable slot 0x84379F. |
| 0x78D860 | 212 | FUN_0078d860 | (b) | CGamePathsList copy-assign: src flags +0x14 bit 0 selects node-walk AddTail (0x801134) vs GetCount (0x801550) + CString copy 0x7FC98F + AddPath 0x78D730 loop — MFC list mechanics; no callers. |
| 0x794050 | 102 | FUN_00794050 | (a) | Implicit member dtor of CTlkFileOverride: CFile dtor at +0x36, ~CTlkFileOverrideHeader 0x7B5D50 at +8, CString_Destruct 0x7FCC1A at +4/+0 — emitted by the inline empty dtor at src/CTlkFileOverride.h:73 with that member layout. |
| 0x79465F | 230 | Catch@0079465f | (a) | Compiler-generated catch funclet of `CTlkTable::Fetch` (STR_RES/CResRef bookkeeping); Fetch in tree at src/CTlkTable.cpp:92. |
| 0x797E70 | 622 | CVidMode_VFTable_0AC | (c) | CVidMode darken-rect rendering code; no callers in the callgraph. |
| 0x7A6895 | 196 | FUN_007a6895 | (c) | Appends to the 64-entry player list in g_pChitin+0x952 region: count +0x126 < 0x40, CString from entry+0x30 stored at +0x12a, 16-byte record at +0x22a, flag byte +0x62a, returns 1; no callers in callgraph. |
| 0x7ACCB0 | 138 | FUN_007accb0 | (c) | Builds the standalone dither cube at global 0x9060F0 (0x20000 bytes, RGB 6:6:5 indexing); called by 0x7CE3D0 and 0x7CE930. |
| 0x7B1F70 | 865 | FUN_007b1f70 | (c) | Tile-blit dispatcher: bpp switch (0x20→0x7CE930, 0x18→0x7CE3D0, 0x10→0x7B25E0); sole caller 0x5CB260 in tree (src/CInfinity.cpp:258). |
| 0x7B25E0 | 1120 | FUN_007b25e0 | (c) | 16bpp palette-dither blit, dispatched from 0x7B1F70. |
| 0x7B3F66 | 146 | FUN_007b3f66 | (a) | Loop-rotation split (gamma/brightness scale tail) of `CVidBitmap::BltBmp24To16Tint`, in tree at src/CVidBitmap.cpp:495 (marker 0x7B3B70); also referenced by the tree comment at src/CVidBitmap.cpp:598. |
| 0x7B3FF8 | 562 | FUN_007b3ff8 | (a) | Second Ghidra split of the same `BltBmp24To16Tint`; tree src/CVidBitmap.cpp:495. |
| 0x7B5370 | 76 | C2DArray_Destruct | (a) | CResHelper&lt;T&gt; dtor COMDAT (CResRef destruct + delete pRes); template at src/CRes.h:73-84 — the Ghidra name is a mislabel. |
| 0x7BB8F0 | 78 | FUN_007bb8f0 | (b) | GLSetup 1.1.2 vendor thunk: guards globals 0x906914/0x9074B0, GLSetupEnumDrivers 0x7B7750 + GLSetupProbeDriver 0x7B7DA0, then forwards 2 args through IAT [0x8BBCD8]; tree models GLSetup as platform-unavailable (src/CChitin3d.cpp). |
| 0x7BB940 | 88 | FUN_007bb940 | (b) | Same GLSetup thunk forwarding 4 args through IAT [0x8BBCDC]. |
| 0x7BB9A0 | 83 | FUN_007bb9a0 | (b) | Same GLSetup thunk forwarding 3 args through IAT [0x8BBCE0]. |
| 0x7BD9D0 | 653 | CVidMode_RenderBlackFade3d | (c) | CVidMode::RenderBlackFade3d; sole caller 0x5CEDE0 in tree (src/CInfinity.cpp:2203, src/CVideo3d.cpp:2055). |
| 0x7BE5A0 | 1239 | FUN_007be5a0 | (c) | CVidMode blit; sole caller 0x79EC20 in tree (src/CVidInf.cpp:2022). |
| 0x7C8830 | 169 | FUN_007c8830 | (b) | GLSetup SDK wrapper (vendor library 0x7D7000-0x7D9000); caller 0x780AD0 in tree (src/CUtil.cpp). |
| 0x7C88E0 | 156 | FUN_007c88e0 | (b) | GLSetup SDK wrapper twin; caller 0x780B60 in tree (src/CUtil.cpp). |
| 0x7CE3D0 | 1116 | FUN_007ce3d0 | (c) | 24bpp palette-dither blit, dispatched from 0x7B1F70. |
| 0x7CE930 | 927 | FUN_007ce930 | (c) | 32bpp palette-dither blit: CVidPalette ctor 0x7BEEA0/dtor 0x7BEEE0, SetPalette 0x7BF390 on both palettes, Realize 0x7BF150 into LUTs 0x9064F8/0x9060F4 (flags 0x20\|0x400000), dither cube 0x9060F0 built via 0x7ACCB0; dispatched from 0x7B1F70. |
