#include "CtrlAltDelete.h"

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameObject.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CPathSearch.h"
#include "CResRef.h"
#include "CScreenInventory.h"
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

// 0x4EC870 void CtrlAltDelete_CreateItem(CString sItem, WORD wUseCount1,
// WORD wUseCount2, WORD wUseCount3) — 517 bytes (0x4EC870-0x4ECA75). The
// CREATEITEM console command. The dispatcher's command-word resolver
// (0x583E60) matches the obfuscated literal at 0x8AF680 — the console
// stores its command words as 2*ch + 0x10, and this one decodes to
// "CREATEITEM" — maps it to id 0x15, whose dispatch arm (0x583B81) calls
// here from four sites (jump table 0x583E50, one per argument count 1-4):
// the parsed item name plus up to three use counts, the missing counts
// zero. ecx (the dispatcher's this) is saved in esi and forwarded to
// 0x4EC1C0 on the error path only, which never reads it.
//
// Same body as CtrlAltDelete_AddItem (0x4ED500), the internal helper the
// FIRSTAID pack handler uses, with one spelling difference: the original
// guards the allocation (0x4EC9F2 `test ebp,ebp / je` stores NULL instead
// of constructing). The listing's INVALID_INDEX arm at 0x4EC954 is
// unreachable (the loop enters only with nIndex < m_nCharacters, and
// 0x4EC93D returns when the count is not positive) and is omitted.
void CtrlAltDelete_CreateItem(CString sItem, WORD wUseCount1, WORD wUseCount2, WORD wUseCount3)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CResRef cResRef(sItem);

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(cResRef, 1005, TRUE) == NULL) {
        CtrlAltDelete_DisplayText("Error File " + sItem + " does not exist\n");
        return;
    }

    for (SHORT nIndex = 0; nIndex < pGame->m_nCharacters; nIndex++) {
        LONG nCharacterId = pGame->m_characterPortraits[nIndex];
        CGameObject* pObject = NULL;
        BYTE nStatus;

        // The original spins while another thread shares the sprite; GetDeny
        // itself sleeps a millisecond before returning SHARED. Unlike the
        // CInfButtonArray callers it does not retry on DENIED.
        do {
            nStatus = pGame->GetObjectArray()->GetDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (nStatus == CGameObjectArray::SHARED);

        if (nStatus != CGameObjectArray::SUCCESS) {
            continue;
        }

        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);

        SHORT nSlot = 0;
        while (nSlot < CScreenInventory::PERSONAL_INVENTORY_SIZE
            && pSprite->m_equipment.m_items[18 + nSlot] != NULL) {
            nSlot++;
        }

        // No free slot: release the sprite and try the next party member.
        if (nSlot == CScreenInventory::PERSONAL_INVENTORY_SIZE) {
            pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        // NOTE: On a failed allocation the original skips the construction
        // and stores NULL in the slot (0x4ECA2A `xor eax,eax`); the handout
        // still stops here.
        pSprite->m_equipment.m_items[18 + nSlot] = new CItem(CResRef(sItem),
            wUseCount1,
            wUseCount2,
            wUseCount3,
            0,
            0);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        // One item is placed per call (0x4ECA37 falls straight into the
        // epilogue).
        return;
    }
}

// 0x4ECAD0 void CtrlAltDelete_SetGlobal(CString sName, CString sScope,
// DWORD dwValue) — 402 bytes (0x4ECAD0-0x4ECC62). The SETGLOBAL console
// command: the dispatcher's resolver (0x583E60) matches the obfuscated
// literal at 0x8AF6C8, which decodes to "SETGLOBAL", maps it to id 29, and
// the dispatch arm at 0x583D22 calls here from 0x583D79 once exactly three
// arguments are parsed. ecx (the dispatcher's this) is not read.
//
// Upper-cases both text arguments on entry (MakeUpper 0x7FD0F0, scope first
// then name in the listing), then picks the variable hash: scope "GLOBAL"
// (strcmp 0x7E7A54 against 0x8A7FBC) takes the game's hash
// (CInfGame::GetVariables 0x4286A0); any other scope names an area
// (CInfGame::GetArea 0x5B7510, by-value CString) and a miss is a silent
// no-op, unlike the handlers above, which print the "Error File" text.
// Either hash: FindKey(sName) (virtual slot 4) hit stores dwValue into
// m_intValue (+0x28, not the DWORD field at +0x24); miss constructs a local
// CVariable (0x452BD0), SetName(sName) (0x452C60), stores dwValue into
// m_intValue, and AddKey (virtual slot 0).
void CtrlAltDelete_SetGlobal(CString sName, CString sScope, DWORD dwValue)
{
    sScope.MakeUpper();
    sName.MakeUpper();

    CVariableHash* pVariables;

    if (sScope == CString("GLOBAL")) {
        pVariables = g_pBaldurChitin->GetObjectGame()->GetVariables();
    } else {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);

        if (pArea == NULL) {
            // Missing area: the original falls straight into the epilogue.
            return;
        }

        pVariables = pArea->GetVariables();
    }

    CVariable* pVariable = pVariables->FindKey(sName);

    if (pVariable != NULL) {
        pVariable->m_intValue = dwValue;
    } else {
        CVariable cVariable;

        cVariable.SetName(sName);
        cVariable.m_intValue = dwValue;
        pVariables->AddKey(cVariable);
    }
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
// ecx is saved in esi and forwarded as the this of the three 0x4ED500 calls;
// that callee never reads it. Hands the player the cheat item pack: five
// "potn08" (0x8ABD88), five "potn20" (0x8ABD80) and one "scrlpet" (0x8ABD78),
// each via CtrlAltDelete_AddItem(CString by value, count, 0, 0).
void CtrlAltDelete_AddItemPack()
{
    CtrlAltDelete_AddItem(CString("potn08"), 5, 0, 0);
    CtrlAltDelete_AddItem(CString("potn20"), 5, 0, 0);
    CtrlAltDelete_AddItem(CString("scrlpet"), 1, 0, 0);
}

// 0x4ED500 void CtrlAltDelete_AddItem(CString sItem, WORD wUseCount1,
// WORD wUseCount2, WORD wUseCount3) — 557 bytes (0x4ED500-0x4ED72D). ecx (the
// dispatcher's this, which the item pack handler forwards) is not read.
//
// Looks the item name up as res type 1005 (item, as in CItem's
// CResHelper<CResItem, 1005>) and prints the same "Error File" text as
// MoveToArea when it is missing. Otherwise walks the party slots and gives the
// item to the first member with a free personal-inventory slot: denies the
// sprite, takes the first NULL of m_items[18..18 + PERSONAL_INVENTORY_SIZE)
// (the pointer array at 0x4B20), news a CItem there with the use counts,
// releases the sprite and stops — one item is placed per call. The listing's
// INVALID_INDEX arm at 0x4ED60C is unreachable (the loop enters only with
// nIndex < m_nCharacters, and 0x4ED5FB returns when the count is not positive)
// and is omitted.
void CtrlAltDelete_AddItem(CString sItem, WORD wUseCount1, WORD wUseCount2, WORD wUseCount3)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CResRef cResRef(sItem);

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(cResRef, 1005, TRUE) == NULL) {
        CtrlAltDelete_DisplayText("Error File " + sItem + " does not exist\n");
        return;
    }

    for (SHORT nIndex = 0; nIndex < pGame->m_nCharacters; nIndex++) {
        LONG nCharacterId = pGame->m_characterPortraits[nIndex];
        CGameObject* pObject = NULL;
        BYTE nStatus;

        // The original spins while another thread shares the sprite; GetDeny
        // itself sleeps a millisecond before returning SHARED. Unlike the
        // CInfButtonArray callers it does not retry on DENIED.
        do {
            nStatus = pGame->GetObjectArray()->GetDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (nStatus == CGameObjectArray::SHARED);

        if (nStatus != CGameObjectArray::SUCCESS) {
            continue;
        }

        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);

        SHORT nSlot = 0;
        while (nSlot < CScreenInventory::PERSONAL_INVENTORY_SIZE
            && pSprite->m_equipment.m_items[18 + nSlot] != NULL) {
            nSlot++;
        }

        // No free slot: release the sprite and try the next party member.
        if (nSlot == CScreenInventory::PERSONAL_INVENTORY_SIZE) {
            pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        // NOTE: The original stores operator new's result (0xEE bytes) with no
        // NULL check: a failed allocation stores NULL in the slot and the
        // handout still stops here.
        pSprite->m_equipment.m_items[18 + nSlot] = new CItem(CResRef(sItem),
            wUseCount1,
            wUseCount2,
            wUseCount3,
            0,
            0);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        // The original stops after the first placement (0x4ED702 falls
        // straight into the epilogue); the pack handler's three calls place
        // one item each.
        return;
    }
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
