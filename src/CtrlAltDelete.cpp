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

// 0x4EC250 void CtrlAltDelete_MoveToArea(CString sAreaName) — 252 bytes
// (0x4EC250-0x4EC34B). ecx is saved in esi and forwarded to 0x4EC1C0 on the
// error path only, and that function never reads it either.
//
// Moves the party to the named area when the key table has an entry of res
// type 1010 (area, as in CAreaFile's CResHelper<CResArea, 1010>); otherwise
// prints the "Error File <name> does not exist" text.
void CtrlAltDelete_MoveToArea(CString sAreaName)
{
    CResRef cResRef(sAreaName);

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(cResRef, 1010, TRUE) == NULL) {
        CtrlAltDelete_DisplayText("Error File " + sAreaName + " does not exist\n");
    } else {
        g_pBaldurChitin->GetScreenWorld()->MoveToArea(sAreaName);
    }
}

// 0x4EC390 void CtrlAltDelete_CreateCreature(CString sName) — 1,243 bytes
// (0x4EC390-0x4EC86A). ecx is saved in esi and forwarded to 0x4EC1C0 on the
// error path only, which never reads it.
//
// Looks the name up as res type 1009 (creature, as in CCreatureFile's
// CResHelper<CResCRE, 1009>) and prints the same "Error File" text as
// MoveToArea when it is missing. Otherwise spawns the creature in the
// visible area at the search square nearest the viewport center, and, when
// the creature file gives a non-blank script name, records it in the area's
// named-creature list under its script name with the sprite id as value.
void CtrlAltDelete_CreateCreature(CString sName)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pGame->GetVisibleArea();
    CResRef cResRef(sName);

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(cResRef, 1009, TRUE) == NULL) {
        CtrlAltDelete_DisplayText("Error File " + sName + " does not exist\n");
        return;
    }

    // The search square is the viewport center, in grid coordinates.
    CInfinity* pInfinity = pArea->GetInfinity();
    INT nViewX;
    INT nViewY;
    pInfinity->GetViewPosition(nViewX, nViewY);
    CRect rcView(&pInfinity->rViewPort);
    INT nGridX = (nViewX + (rcView.right - rcView.left) / 2) / CPathSearch::GRID_SQUARE_SIZEX;
    INT nGridY = (nViewY + (rcView.bottom - rcView.top) / 2) / CPathSearch::GRID_SQUARE_SIZEY;

    CCreatureFile cCreatureFile;

    // NOTE: Uninline. The original expands CResHelper::SetResRef here
    // (resref compare, GetResObject, Request); ours calls it.
    cCreatureFile.SetResRef(CResRef(sName), TRUE, TRUE);

    // NOTE: The original evaluates the size first.
    DWORD nSize = cCreatureFile.GetDataSize();
    BYTE* pData = cCreatureFile.GetData();

    CGameSprite* pSprite = new CGameSprite(pData,
        nSize,
        0,
        0,
        -1,
        0,
        0,
        0x7FFFFFFF,
        CPoint(-1, -1),
        0);

    // NOTE: As in `CSpawn::ExecuteSpawn`, the original sets the resref before
    // the NULL check below.
    pSprite->SetResRef(CResRef(cCreatureFile.GetResRef().GetResRefStr()));

    cCreatureFile.ReleaseData();

    if (pSprite != NULL) {
        pSprite->SetResRef(CResRef(sName));

        CVariable cVariable;

        CString sScriptName(pSprite->m_scriptName);
        CUtil::TrimLeft(sScriptName);
        // NOTE: The original compares the trimmed name against the global
        // empty CString at 0x8C5CF8 with strcmp; the tree's spelling for
        // that compare is the empty-CString operator (CGameAIBase uses
        // `CString("") == ...` for the same global).
        if (sScriptName != CString("")) {
            cVariable.SetName(pSprite->GetScriptName());
            cVariable.m_intValue = pSprite->m_id;
            pArea->m_namedCreatures.AddKey(cVariable);
        }

        CPoint ptSquare = pArea->m_search.GetNearestOpenSquare(CPoint(nGridX, nGridY),
            CGameObject::DEFAULT_TERRAIN_TABLE,
            pSprite->m_animation.GetPersonalSpace(),
            -1);

        // NOTE: Unlike `CSpawn::ExecuteSpawn`'s spawn point, there is no
        // GRID_SQUARE_SIZEX/2 term here: the original multiplies the square
        // index straight through.
        pSprite->AddToArea(pArea,
            CPoint(ptSquare.x * CPathSearch::GRID_SQUARE_SIZEX,
                ptSquare.y * CPathSearch::GRID_SQUARE_SIZEY),
            0,
            pSprite->m_animation.GetListType());
    }
}

// 0x4EC350 void CtrlAltDelete_EnableCheatKeys(INT nEnable) — 24 bytes
// (0x4EC350-0x4EC367). ecx is not read; the store goes through the global
// screen world. Stores the argument into CScreenWorld+0x156, the flag the
// world screen's OnKeyDown consults for the Ctrl cheat keys.
void CtrlAltDelete_EnableCheatKeys(INT nEnable)
{
    g_pBaldurChitin->GetScreenWorld()->m_bCheatKeysEnabled = nEnable;
}

// 0x4EC370 void CtrlAltDelete_SetCurrentXP(DWORD dwCurrentXP) — 25 bytes
// (0x4EC370-0x4EC388). ecx is not read. Forwards the argument to the world
// screen.
void CtrlAltDelete_SetCurrentXP(DWORD dwCurrentXP)
{
    g_pBaldurChitin->GetScreenWorld()->SetCurrentXP(dwCurrentXP);
}

// 0x4ECA80 void CtrlAltDelete_SetAreaExplored() — 37 bytes
// (0x4ECA80-0x4ECAA4). ecx is not read. Tail-calls SetAreaExplored on the
// visible area's visibility map.
void CtrlAltDelete_SetAreaExplored()
{
    g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->m_visibility.SetAreaExplored();
}

// 0x4ECAB0 void CtrlAltDelete_AddGold(DWORD dwGold) — 25 bytes
// (0x4ECAB0-0x4ECAC8). ecx is not read. Forwards the argument to the world
// screen.
void CtrlAltDelete_AddGold(DWORD dwGold)
{
    g_pBaldurChitin->GetScreenWorld()->AddGold(dwGold);
}

// 0x4ED230 void CtrlAltDelete_AddGold500() — 22 bytes (0x4ED230-0x4ED245).
// ecx is not read. Adds a fixed 500 gold, the console command that takes no
// argument.
void CtrlAltDelete_AddGold500()
{
    g_pBaldurChitin->GetScreenWorld()->AddGold(500);
}

// 0x4ED250 void CtrlAltDelete_AddItemPack() — 97 bytes (0x4ED250-0x4ED2B0).
// ecx is saved in esi and forwarded as the this of 0x4ED500 on all three
// calls; that callee is not ported, so this body is a stub. The original
// hands the player the cheat item pack: five "potn08" (0x8ABD88), five
// "potn20" (0x8ABD80) and one "scrlpet" (0x8ABD78), each via
// 0x4ED500(CString by value, count, 0, 0).
void CtrlAltDelete_AddItemPack()
{
    // TODO: 0x4ED250 calls 0x4ED500 (not ported).
}

// 0x4EDF60 void CtrlAltDelete_JeffKAttacks() — 31 bytes (0x4EDF60-0x4EDF7E).
// ecx is saved in esi and forwarded to 0x4EC390, which never reads it.
// Spawns the "JeffK" creature.
void CtrlAltDelete_JeffKAttacks()
{
    CtrlAltDelete_CreateCreature(CString("JeffK"));
}

// 0x4EDF80 void CtrlAltDelete_JeffKDefends() — 31 bytes (0x4EDF80-0x4EDF9E).
// ecx is saved in esi and forwarded to 0x4EC390, which never reads it.
// Spawns the "JeffK2" creature.
void CtrlAltDelete_JeffKDefends()
{
    CtrlAltDelete_CreateCreature(CString("JeffK2"));
}
