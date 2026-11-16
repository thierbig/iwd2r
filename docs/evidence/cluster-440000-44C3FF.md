# Cluster classification: 0x440000-0x44C3FF (Task 84n, STEP 1)

The 40 unmarked rows of this cluster, one verdict each. Verdicts: (a) covered
by the tree, (b) compiler/STL/MFC template instance or thunk with no game
logic, (c) real game code to port.

Method: every row was read with `tools/parity/annotate.py 0xADDR` (full
listings kept in /tmp/t84n/annot1.txt and /tmp/t84n/annot2.txt) plus its
Ghidra decompile, and its callers were searched in the exported callgraph.
The cluster turns out to be one contiguous block of MSVC STL template
instantiation code for three `_Tree` (red-black tree) flavors plus two
`std::vector` element sizes, with three dead `CResHelper<T,n>::SetResRef`
COMDAT instances at its start.

The three tree flavors are told apart by their shared nil-node singleton:
nil 0x8D13E8 (0x20-byte nodes, `TString` value copied via
`TString::ConstructCopy` 0x44BA90), nil 0x8D13F0 (0x14-byte nodes, value
copied via `SetP1ToP2`/0x44B4B0), nil 0x8D13F4 (0x14-byte nodes, value
copied via `SetP1ToP2` with the nested-struct clone 0x44C320). The game code
that uses these containers is the CDerivedStats family (e.g. the in-tree
caller 0x447E50 `CDamageReduction::AddDamageType`, src/CDerivedStats.cpp)
and 0x725B50; our tree spells those members as `std::set`/`std::map`, so
these bodies are compiler output, not hand-written game logic.

| address | size | ghidra name | class | verdict | evidence |
|---|---|---|---|---|---|
| 0x4428A0 | 228 | CResHelper_CResCell_SetResRef | CResHelper | (a) | `CResHelper<CResCell,1000>::SetResRef` (push 0x3e8 at 0x442924 before `CDimm::GetResObject` 0x786DF0); generic template spelled at src/CRes.h:85; no caller in the callgraph — dead COMDAT. |
| 0x442990 | 228 | CResHelper_CResBAH_SetResRef | CResHelper | (a) | Same template with type id 0x44c (push at 0x442A14); covered by src/CRes.h:85; no callers. |
| 0x442A80 | 228 | CResHelper_CResMosaic_SetResRef | CResHelper | (a) | Same template with type id 0x3ec (push at 0x442B04); covered by src/CRes.h:85; no callers. |
| 0x4436C0 | 79 | FUN_004436c0 | CObList-derived dtor | (b) | MSVC D1 complete destructor of the CObList-derived list class with vtable 0x8484CC (the CDerivedStats +0x4AC/+0x4C8 members, spelled `CPtrList` at src/CDerivedStats.h:621): vtable store + `call 0x4E71B0` ClearAll (in tree, src/CColorEffects.cpp:37 / src/CMoveList.cpp:32) + CObList dtor 0x7FB298; its D0 deleting flavor is 0x443710. No game logic beyond the covered ClearAll call. |
| 0x4464A0 | 61 | FUN_004464a0 | copy glue | (b) | MSVC 16-byte UDT copy helper: copies param_2 to the hidden ECX return buffer and to param_1; called only by vector<0x10>::insert 0x449110 and 0x725B50. |
| 0x448DD0 | 572 | FUN_00448dd0 | STL _Tree | (b) | `_Tree` insert for the nil 0x8D13F0 flavor (4 refs): calls 0x4C5770, node insert 0x44AB10, rotates 0x44B0A0/0x44B120/0x44B180. |
| 0x449010 | 204 | FUN_00449010 | STL _Tree | (b) | `_Tree::erase(first,last)` range erase, nil 0x8D13F0 (3 refs); calls 0x44AA40, 0x44B100, 0x44A550. |
| 0x449110 | 752 | FUN_00449110 | STL vector | (b) | `std::vector<0x10-POD>::insert` out-of-line instantiation: grow/move loop with 0x4464A0 element copy. |
| 0x449400 | 1017 | FUN_00449400 | STL _Tree | (b) | `_Tree::erase(iterator)` with rebalance for nil 0x8D13F4 (9 refs): prev walker 0x44B1E0, UnknownLock_Enter/Leave, operator_delete. Callers 0x445240/0x448470/0x448980/0x44B2D0/0x44B500/0x44B6C0/0x44BF20/0x44C0C0 — all container glue. |
| 0x449800 | 60 | FUN_00449800 | STL _Tree | (b) | `_Tree` lower_bound walk (first node whose key >= param_2), nil 0x8D13F4; result written through sret param_1. |
| 0x449880 | 138 | FUN_00449880 | STL _Tree | (b) | `_Tree::_Init`: operator_new(0x14) shared nil node under UnknownLock for nil 0x8D13F4; caller 0x448700. |
| 0x449910 | 611 | RedBlackTree_AddValueInternal | STL _Tree | (b) | `_Tree` insert-node + SetP1ToP2 value copy + inline rebalance, nil 0x8D13F4 (7 refs); callers 0x447E50 (in-tree `CDamageReduction::AddDamageType`, src/CDerivedStats.cpp) and 0x448740. |
| 0x449B80 | 835 | FUN_00449b80 | STL vector | (b) | `std::vector<0x24-struct>::insert` out-of-line instantiation: element copy ctor 0x44B8A0, move glue 0x445240, assignments 0x44BF20/0x44C0C0. |
| 0x449ED0 | 1061 | UnknownContainerDerived_Remove | STL _Tree | (b) | `_Tree::erase(iterator)` with rebalance for nil 0x8D13E8 (9 refs), the 0x20-node/TString-value flavor; twin of 0x449400. |
| 0x44A300 | 235 | FUN_0044a300 | STL _Tree | (b) | `_Tree::_Copy` (copy-assign body): left-to-right node clone, 0x20-byte nodes, nil 0x8D13E8, value via `TString::ConstructCopy` 0x44BA90 (in tree, src/TString.h). |
| 0x44A3F0 | 107 | UnknownContainer4_FreeAll | STL _Tree | (b) | `_Tree` recursive free-all (operator_delete per node, nil 0x8D13E8, 2 refs). |
| 0x44A460 | 138 | UnknownContainerDerived_Construct | STL _Tree | (b) | `_Tree::_Init` for nil 0x8D13E8 (4 refs): operator_new(0x20) nil node under UnknownLock. |
| 0x44A4F0 | 86 | FUN_0044a4f0 | STL _Tree | (b) | Tree node next-walker (iterator increment), nil 0x8D13E8, sret (ret 8). |
| 0x44A550 | 1017 | FUN_0044a550 | STL _Tree | (b) | `_Tree::erase(iterator)` with rebalance for nil 0x8D13F0 (9 refs) — third twin of 0x449400/0x449ED0. |
| 0x44A950 | 235 | FUN_0044a950 | STL _Tree | (b) | `_Tree::_Copy` twin of 0x44A300 for the nil 0x8D13F0 flavor (0x14-byte nodes, SetP1ToP2 value, child clone 0x44B010). |
| 0x44AB10 | 611 | FUN_0044ab10 | STL _Tree | (b) | `_Tree` insert-node + value copy + rebalance tail for nil 0x8D13F0 (7 refs) — twin of 0x449910. |
| 0x44ADF0 | 82 | FUN_0044adf0 | STL _Tree | (b) | `_Tree` _Lrotate (jump-only body, no calls), nil 0x8D13F4 flavor. |
| 0x44AE70 | 86 | FUN_0044ae70 | STL _Tree | (b) | `_Tree` _Rrotate (jump-only body), nil 0x8D13F4 flavor. |
| 0x44AED0 | 81 | RedBlackTree_Rotate | STL _Tree | (b) | Rotate helper for nil 0x8D13F4 — jump-only twin of 0x44B180; no direct calls (inlined at use sites). |
| 0x44AF60 | 139 | FUN_0044af60 | STL _Tree | (b) | Recursive `_Tree` node clone, nil 0x8D13E8: operator_new(0x20), color/key copy, `TString::ConstructCopy` 0x44BA90 value. |
| 0x44B010 | 135 | FUN_0044b010 | STL _Tree | (b) | Recursive `_Tree` node clone twin for nil 0x8D13F0: value copy 0x44B4B0. |
| 0x44B0A0 | 82 | FUN_0044b0a0 | STL _Tree | (b) | _Lrotate twin of 0x44ADF0 for the nil 0x8D13F0 flavor. |
| 0x44B120 | 86 | FUN_0044b120 | STL _Tree | (b) | _Rrotate twin of 0x44AE70 for the nil 0x8D13F0 flavor. |
| 0x44B180 | 81 | FUN_0044b180 | STL _Tree | (b) | Rotate twin of 0x44AED0 for the nil 0x8D13F0 flavor. |
| 0x44B1E0 | 67 | FUN_0044b1e0 | STL _Tree | (b) | Tree node prev/next walker (iterator movement), nil 0x8D13F4. |
| 0x44B230 | 67 | UnknownContainer4_GetPrevious | STL _Tree | (b) | Tree node walker twin, nil 0x8D13E8. |
| 0x44B280 | 67 | FUN_0044b280 | STL _Tree | (b) | Tree node walker twin, nil 0x8D13F0. |
| 0x44B2D0 | 471 | FUN_0044b2d0 | STL map | (b) | `std::map` operator[]-shaped find-or-insert for the 0x24-byte value struct with a nested nil-0x8D13F4 tree: lower_bound + construct 0x4C5770 + insert 0x449840 + nested-tree clone 0x44C320 + value copies SetP1ToP2/0x44B4B0 + erase 0x449400 + rotate 0x44AE50. |
| 0x44B500 | 438 | FUN_0044b500 | STL map | (b) | Identical call set to 0x44B2D0 (same nine callees) — COMDAT twin. |
| 0x44B6C0 | 465 | FUN_0044b6c0 | STL map | (b) | Identical call set to 0x44B2D0 (same nine callees) — COMDAT twin. |
| 0x44B8A0 | 453 | FUN_0044b8a0 | STL struct copy | (b) | Copy ctor of the 0x24-byte struct embedding a nil-0x8D13F4 tree (8 refs) at +0xC: scalar field copies + node-by-node tree clone (operator_new(0x14)), SEH-wrapped. |
| 0x44BF20 | 411 | FUN_0044bf20 | STL struct copy | (b) | Copy assignment of that 0x24 struct: clear own tree (0x44B1E0 + 0x449400 loop) then clone source's (nil 0x8D13F4). |
| 0x44C0C0 | 368 | FUN_0044c0c0 | STL struct copy | (b) | Copy-assignment twin of 0x44BF20 with the `this != &other` self-check. |
| 0x44C230 | 235 | FUN_0044c230 | STL _Tree | (b) | `_Tree::_Copy` twin for the nil 0x8D13F4 flavor: 0x14-byte nodes, SetP1ToP2 value, child clone 0x44C320 — same shape as 0x44A950/0x44A300. |
| 0x44C320 | 135 | FUN_0044c320 | STL _Tree | (b) | Recursive `_Tree` node clone for nil 0x8D13F4: operator_new(0x14), plain value copy. |

## Counts

- (a) covered: 3 (0x4428A0, 0x442990, 0x442A80)
- (b) compiler/STL: 37
- (c) real game code to port: 0

STEP 2 is therefore empty: no row of this cluster is hand-written game
logic. The nearest to game code is 0x4436C0, whose only body call — the
shared ClearAll at 0x4E71B0 — is already in the tree twice; porting the
destructor thunk would require naming a list class the tree deliberately
spells as plain `CPtrList` (documented in docs/evidence/443B30.md), with no
observable behavior to gain.
