#ifndef CTRLALTDELETE_H_
#define CTRLALTDELETE_H_

#include "FileFormat.h"

// The debug console's "CtrlAltDelete:" cheat-command layer. The console's
// dispatcher (CLUAConsole::Execute 0x5838A0) hands every handler its own this
// in ecx (this+0x38, this+0x40 for the FIRSTAID/HANS/MIDAS arms) and the
// parsed command argument as a CString by value on the stack. None of the
// ported handlers reads ecx, so they are free functions here; each body
// states what it does with ecx.

// 0x4EC1C0
void CtrlAltDelete_DisplayText(CString sText);

// 0x4EC250
void CtrlAltDelete_MoveToArea(CString sAreaName);

// 0x4EC390
void CtrlAltDelete_CreateCreature(CString sName);

// 0x4EC350
void CtrlAltDelete_EnableCheatKeys(INT nEnable);

// 0x4EC370
void CtrlAltDelete_SetCurrentXP(DWORD dwCurrentXP);

// 0x4EC870
void CtrlAltDelete_CreateItem(CString sItem, WORD wUseCount1, WORD wUseCount2, WORD wUseCount3);

// 0x4ECAD0
void CtrlAltDelete_SetGlobal(CString sName, CString sScope, DWORD dwValue);

// 0x4ECA80
void CtrlAltDelete_SetAreaExplored();

// 0x4ECAB0
void CtrlAltDelete_AddGold(DWORD dwGold);

// 0x4ED230
void CtrlAltDelete_AddGold500();

// 0x4ED250
void CtrlAltDelete_AddItemPack();

// 0x4ED2C0
void CtrlAltDelete_Hans();

// 0x4ED500
void CtrlAltDelete_AddItem(CString sItem, WORD wUseCount1, WORD wUseCount2, WORD wUseCount3);

// 0x4ED730
void CtrlAltDelete_CheckVar(CString sName, CString sScope);

// 0x4EDAB0
void CtrlAltDelete_SetChapter(INT nChapter);

// 0x4EDCA0
void CtrlAltDelete_AddSpellClass(const BYTE& nClass, DWORD dwCount);

// 0x4EDF60
void CtrlAltDelete_JeffKAttacks();

// 0x4EDF80
void CtrlAltDelete_JeffKDefends();

#endif /* CTRLALTDELETE_H_ */
