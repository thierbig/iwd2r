# Cluster classification: unassigned rows 0x402A50-0x5AF540 (Task 84o, STEP 1)

The unassigned rows across 0x402A50-0x5AF540 (no `// 0x` marker in src/,
absent from WillScarlettOhara's fork, outside every contributor's assigned
area), one verdict each. Verdicts: (a) covered by the tree, (b)
compiler/STL/MFC template instance or thunk with no game logic, (c) real
game code to port. The task prose said "58 rows" but enumerated 53; the 53
enumerated rows are surveyed here.

Method: every row was read with `tools/parity/annotate.py 0xADDR` (full
listings kept in /tmp/t84o/annot*.txt), its callers were searched in the
exported callgraph, and interesting bodies were cross-checked against the
tree (layout, existing markers, caller bodies). Two rows needed the import
table: 0x4D0B20's three WS2_32 thunks (IAT 0x847568/0x84756C/0x847570 =
ordinals 3/10/23 = closesocket/ioctlsocket/socket) and 0x40AC60's CRT
initializer sequence.

Tally: 7 (a), 31 (b), 15 (c).

| address | size | ghidra name | class | verdict | evidence |
|---|---|---|---|---|---|
| 0x402A50 | 84 | FUN_00402a50 | vector deleting dtor | (b) | MSVC scalar deleting destructor COMDAT: cookie test `[p-4]`, `operator_delete` 0x7FC984; no game logic. |
| 0x40AC60 | 118 | FUN_0040ac60 | CRT initializer | (b) | CRT `$E` dynamic initializer: builds CString from 0x8C5CF8, calls `CAIObjectType::CAIObjectType` 0x40AD00 with ecx=0x8C7788 (global static), destructs temp — compiler-generated static init, no callable logic. |
| 0x40F120 | 63 | FUN_0040f120 | MFC dtor | (b) | MFC CFile-family dtor: vptr store 0x85FAA4, `CString_Destruct` 0x7FCC1A on member at +0x10 — compiler implicit dtor. |
| 0x4218F0 | 776 | WinMain | WinMain | (a) | `WinMain` already reconstructed at src/main.cpp:142-172 (marker at src/main.cpp:19). |
| 0x425B80 | 76 | FUN_00425b80 | implicit dtor | (b) | Implicit dtor of `{CString@0, CSound@4}` (CString_Destruct 0x7FCC1A + CSound dtor); its ctor is inlined at the four construction sites. |
| 0x426500 | 246 | ~C2DArray | C2DArray | (a) | `C2DArray::~C2DArray` at src/C2DArray.cpp:21. |
| 0x4278E0 | 76 | FUN_004278e0 | CBaldurEngine dtor | (a) | CBaldurEngine base dtor, implicit in tree (src/CBaldurEngine.h:12, layout src/CBaldurEngine.h:196); empty base, no out-of-line body needed. |
| 0x428AD0 | 1173 | FUN_00428ad0 | CBaldurProjector | (c) | CBaldurProjector::EngineActivated-scale body (subtitle block 0x43EC20 is in tree at src/CBaldurProjector.cpp:233 as a TODO note; this entry is the unported bulk). |
| 0x428F90 | 162 | FUN_00428f90 | MFC logging | (b) | MFC logging family operating on global 0xA18CE0; no game logic. |
| 0x429061 | 156 | FUN_00429061 | MFC logging | (b) | Same family on 0xA18CE0. |
| 0x429100 | 97 | FUN_00429100 | MFC logging | (b) | Same family on 0xA18CE0. |
| 0x429170 | 119 | FUN_00429170 | MFC logging | (b) | Same family on 0xA18CE0. |
| 0x4291F0 | 431 | FUN_004291f0 | MFC logging | (b) | Same family on 0xA18CE0. |
| 0x4293CB | 163 | FUN_004293cb | MFC logging | (b) | Same family on 0xA18CE0. |
| 0x435150 | 322 | FUN_00435150 | game code | (c) | Real game code; sole caller `CGameSprite::LeaveArea` 0x74C0D0 (tree src/CGameSprite.cpp:26078). |
| 0x4352A0 | 188 | FUN_004352a0 | CBaldurMessage | (c) | Method on the `GetBaldurMessage()` object (0x425CA0 at call site 0x4F4433): requires Chitin+0x1032 and +0x1033 flags, then `CScreenWorld::SetPendingMapWorld` 0x6933F0 or `StartMapWorldMultiplayerHost` 0x693410 with `CNetwork::GetPlayerID` 0x7A7220; dispatched from `AsynchronousUpdate` 0x4EE020. |
| 0x438560 | 432 | FUN_00438560 | CBaldurMessage | (c) | Method on the `GetBaldurMessage()` object (call site 0x4F466B), another message-dispatch arm of `AsynchronousUpdate` 0x4EE020. |
| 0x43F6D0 | 547 | FUN_0043f6d0 | CBaldurProjector | (c) | CBaldurProjector body (file family of in-tree 0x43E300, src/CBaldurProjector.cpp:18). |
| 0x43F9B0 | 837 | FUN_0043f9b0 | CBaldurProjector | (c) | CBaldurProjector body around sub_43E300. |
| 0x47A5C0 | 225 | CResHelper_CResBitmap_SetResRef | CResHelper | (a) | `CResHelper<CResBitmap>::SetResRef` template spelled at src/CRes.h:85; dead COMDAT, no callers (same family as the 84n cluster). |
| 0x4C5AE0 | 60 | FUN_004c5ae0 | STL _Tree | (b) | MSVC STL `_Tree` map lookup: nil sentinel 0x8D13F0 compare, key at `[n+0xC]`, result `[n+8]` — compiler template. |
| 0x4D0B20 | 92 | FUN_004d0b20 | CGameSpy | (c) | Non-blocking socket creator on Chitin+0x18D4 (`CGameSpy cGameSpy`, src/CChitin.h:237): `this+0` must be TRUE, arg2 picks SOCK_STREAM/SOCK_DGRAM, `socket` via IAT 0x847570, `ioctlsocket(FIONBIO 0x8004667E, 1)` via 0x84756C, `closesocket` on failure via 0x847568, stores handle through arg1; sole caller `CScreenConnection::HandleEMEvent` 0x600B50 (tree src/CScreenConnection.cpp:3274). |
| 0x4D3D90 | 75 | FUN_004d3d90 | CUIManager | (a) | `CUIManager::fUninit` at src/CUIManager.cpp:112 — tree body matches instruction-for-instruction (walk m_lPanels at +0xAE, delete each CUIPanel, RemoveAll, m_bInitialized=FALSE); exe kept an out-of-line COMDAT for the three screen callers (0x5D3350, 0x5D72C0, 0x671FA0). |
| 0x4E7680 | 158 | FUN_004e7680 | game code | (c) | Deep copy of a CPtrList of `CResRef*`: walks source nodes (`[n+0]` next, `[n+8]` data), `new` 8 bytes, `CResRef::CResRef` 0x78A960, `CResRef::operator=` 0x78AC30, `CPtrList::AddTail` 0x7FBE4E; called twice by `CDerivedStats::operator=` 0x445330 for the +0x4AC/+0x4C8 lists (tree TODO at src/CDerivedStats.cpp:545); sibling 0x4E7720 covers the four 8-byte-POD lists. |
| 0x4EDA50 | 90 | FUN_004eda50 | implicit dtor | (b) | Implicit member-destruction dtor COMDAT (no vptr store): `CStringArray_Destruct` 0x7FB9C1 at +0x24, MFC int-array dtor 0x7FB93A at +0x10, `CString_Destruct` 0x7FCC1A at +4; zero real callers, one unwind-table reference; owning class absent from tree. |
| 0x4EDAB0 | 492 | FUN_004edab0 | game code | (c) | Real game code; caller CLUAConsole_Input 0x5838A0 is not in the tree. |
| 0x4EDCA0 | 692 | FUN_004edca0 | game code | (c) | Real game code; caller FUN_004ECC70 0x4ECC70 is not in the tree. |
| 0x514360 | 107 | FUN_00514360 | CMessage subclass | (c) | Remote-id resolver on a 0xC-byte CMessage subclass (vtable 0x84D3B4, ctor 0x4F6C90): `UTIL_ASSERT(pData != NULL)` (CMessage.cpp 0x74FB), reads the id pair at `[0x85E670]+pData`, `CGameRemoteObjectArray::Find` 0x59B300 on `[[0x8CF6DC]+0x1C54]+0x375A`, stores the hit at `this+4`, returns TRUE/FALSE; called from an `AsynchronousUpdate` 0x4EE020 arm. |
| 0x529800 | 332 | FUN_00529800 | CProjectileExploding | (a) | Most-derived destructor of CProjectileExploding, implicit in tree (layout at src/CProjectileBAM.h:88-112 matches exactly). |
| 0x5367C0 | 475 | CProjectileShaft_AddToArea | CProjectileShaft | (c) | `CProjectileShaft::AddToArea` — vtable-dispatched, zero direct callers. |
| 0x5402E0 | 77 | C2DArray::C2DArray | C2DArray | (a) | `C2DArray` ctor at src/C2DArray.cpp:11. |
| 0x5477A0 | 95 | FUN_005477a0 | MFC collection dtor | (b) | Dtor of an MFC CPlex-backed collection: vptr store 0x84EBCC, walks `[esi+4]` next-chain to NULL, zeroes +4..+0x10, `CPlex_FreeDataChain` 0x7FB7C8 on m_pBlocks at +0x14. |
| 0x547800 | 93 | FUN_00547800 | MFC collection | (b) | `CPlex_Create` 0x7FB7A8 caller with 0xC-element blocks — MFC collection growth helper. |
| 0x56DA10 | 277 | FUN_0056da10 | TString | (c) | `TString::Delete`; callers 0x56BF30 plus four MFC-side sites. |
| 0x56DB30 | 836 | FUN_0056db30 | container math | (b) | Span arithmetic on `[ebx+8]..[ebx+0xC]` with magic 0x66666667 divide-by-10 — container size math, no game logic. |
| 0x587660 | 107 | FUN_00587660 | container math | (b) | ×10-stride element indexer (`lea edx,[eax+eax*4]; shl edx,1`). |
| 0x5876D0 | 516 | FUN_005876d0 | container math | (b) | Span `[edi+8..0xC]>>2` INT count plus index math. |
| 0x59E980 | 335 | FUN_0059e980 | CInfButtonArray | (c) | `~CInfButtonArray`: calls 0x587BD0 first (deletes the static CGameButtonList dword_8E6820 and all entries, in tree at src/CInfButtonArray.cpp:110-120), then implicit member teardown (deque<INT> +0x1986, CVidCells, 12 CInfButtonSettings); tree declares no dtor (src/CInfButtonArray.h:32). |
| 0x59EBD0 | 129 | FUN_0059ebd0 | CObList dtor | (b) | CString*-owning CObList-derived dtor (vtable 0x8518F8), a CInfGame member — MFC collection teardown. |
| 0x5961A0 | 162 | FUN_005961a0 | STL deque | (b) | MSVC STL deque<INT> mechanics (fields [esi+0xC] size, [esi+0x10]/[esi+0x20] cursors, ×4 strides) owned by CInfButtonArray; wrappers 0x596040/0x5960E0 already marked at src/CInfButtonArray.cpp:1253/1284. |
| 0x596250 | 139 | FUN_00596250 | STL deque | (b) | Same deque<INT> family. |
| 0x5962E0 | 662 | FUN_005962e0 | STL deque | (b) | Same deque<INT> family. |
| 0x596580 | 114 | FUN_00596580 | STL deque | (b) | Same deque<INT> family. |
| 0x596600 | 98 | FUN_00596600 | STL deque | (b) | Same deque<INT> family. |
| 0x596670 | 303 | FUN_00596670 | STL deque | (b) | Same deque<INT> family. |
| 0x5967A0 | 102 | FUN_005967a0 | STL deque | (b) | Same deque<INT> family. |
| 0x596810 | 96 | FUN_00596810 | STL deque | (b) | Same deque<INT> family. |
| 0x596870 | 78 | FUN_00596870 | STL deque | (b) | Same deque<INT> family. |
| 0x5968C0 | 139 | FUN_005968c0 | STL deque | (b) | Same deque<INT> family. |
| 0x596980 | 101 | FUN_00596980 | STL deque | (b) | Same deque<INT> family. |
| 0x5969F0 | 132 | FUN_005969f0 | STL deque | (b) | Same deque<INT> family. |
| 0x596A80 | 132 | FUN_00596a80 | STL deque | (b) | Same deque<INT> family. |
| 0x5AF540 | 441 | CInfGame_B3AUTO_005af540 | CInfGame | (c) | CInfGame-scope body; caller `CGameEffectFindFamiliar::ApplyEffect` 0x4B9EB0 (tree src/CGameEffect.cpp:16578). |
