#include "CtrlAltDelete.h"

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameObject.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CPathSearch.h"
#include "CResRef.h"
#include "CScreenWorld.h"
#include "CUtil.h"
#include "CVariableHash.h"

// 0x4EC1C0 void CtrlAltDelete_DisplayText(CString sText) — 132 bytes
// (0x4EC1C0-0x4EC243). ecx (the dispatcher's this) is not read.
//
// Prints the command argument as text on the world screen. The original
// builds the name argument from the global empty CString at 0x8C5CF8 and
// loads DisplayText's lMarker from the DWORD at 0x84C960 (-1 at rest); the
// tree's other DisplayText callers spell that marker as the -1 literal
// (CScreenWorld 0x6893E8), so this port does too.
void CtrlAltDelete_DisplayText(CString sText)
{
    g_pBaldurChitin->GetScreenWorld()->DisplayText(CString(""), sText, -1, FALSE);
}
