#include "CGameTrigger.h"

#include "CAIAction.h"
#include "CAIScript.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CProjectile.h"
#include "CScreenWorld.h"
#include "CTimerWorld.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CVidPoly.h"
#include "marshal_ptr.h"

// 0x8D71DC
const LONG CGameTrigger::RANGE_TRIGGER = 16
    * CPathSearch::GRID_SQUARE_SIZEX
    * CPathSearch::GRID_SQUARE_SIZEX;

// 0x8D71C4
const LONG CGameTrigger::RANGE_EDGE = (CSearchBitmap::TRAVEL_WIDTH + 10)
    * (CSearchBitmap::TRAVEL_WIDTH + 10)
    * CPathSearch::GRID_SQUARE_SIZEX
    * CPathSearch::GRID_SQUARE_SIZEX;

// 0x4CCAB0
CGameTrigger::CGameTrigger(CGameArea* pArea, CAreaFileTriggerObject* pTriggerObject, CAreaPoint* pPoints, WORD maxPts)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
    // __LINE__: 110
    UTIL_ASSERT(pArea != NULL && pTriggerObject != NULL && pPoints != NULL && pTriggerObject->m_pickPointStart + pTriggerObject->m_pickPointCount <= maxPts);

    m_objectType = TYPE_TRIGGER;
    m_triggerType = pTriggerObject->m_triggerType;
    m_rBounding.left = pTriggerObject->m_boundingRectLeft;
    m_rBounding.top = pTriggerObject->m_boundingRectTop;
    m_rBounding.right = pTriggerObject->m_boundingRectRight + 1;
    m_rBounding.bottom = pTriggerObject->m_boundingRectBottom + 1;
    m_cursorType = pTriggerObject->m_cursorType != 30 ? pTriggerObject->m_cursorType : 42;
    memcpy(m_newArea, pTriggerObject->m_newArea, RESREF_SIZE);
    memcpy(m_newEntryPoint, pTriggerObject->m_newEntryPoint, SCRIPTNAME_SIZE);
    m_dwFlags = pTriggerObject->m_dwFlags;
    m_description = pTriggerObject->m_description;
    m_nPolygon = pTriggerObject->m_pickPointCount;
    memcpy(m_scriptRes, pTriggerObject->m_script, RESREF_SIZE);
    strncpy(m_scriptName, pTriggerObject->m_scriptName, SCRIPTNAME_SIZE);

    CAIScript* pScript = new CAIScript(CResRef(pTriggerObject->m_script));
    SetScript(0, pScript);

    m_trapDetectionDifficulty = pTriggerObject->m_trapDetectionDifficulty;
    m_trapDisarmingDifficulty = pTriggerObject->m_trapDisarmingDifficulty;
    m_trapActivated = pTriggerObject->m_trapActivated;
    m_trapDetected = pTriggerObject->m_trapDetected;
    m_posTrapOrigin.x = pTriggerObject->m_posXTrapOrigin;
    m_posTrapOrigin.y = pTriggerObject->m_posYTrapOrigin;
    m_keyType = pTriggerObject->m_keyType;
    m_drawPoly = 0;

    if (m_nPolygon != 0) {
        m_pPolygon = new CPoint[m_nPolygon];

        if (m_pPolygon == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
            // __LINE__: 149
            UTIL_ASSERT(FALSE);
        }

        WORD adjust = 0;
        for (WORD cnt = 0; cnt < m_nPolygon; cnt++) {
            m_pPolygon[cnt - adjust].x = pPoints[cnt + pTriggerObject->m_pickPointStart].m_xPos;
            m_pPolygon[cnt - adjust].y = pPoints[cnt + pTriggerObject->m_pickPointStart].m_yPos;
            if (cnt >= 2) {
                int x2 = m_pPolygon[cnt - adjust - 2].x;
                int y2 = m_pPolygon[cnt - adjust - 2].y;

                int x1 = m_pPolygon[cnt - adjust - 1].x;
                int y1 = m_pPolygon[cnt - adjust - 1].y;

                int x0 = m_pPolygon[cnt - adjust].x;
                int y0 = m_pPolygon[cnt - adjust].y;

                if ((x2 == x1 && x1 == x0)
                    || (y2 == y1 && y1 == y0)
                    || (x2 != x1
                        && x1 != x0
                        && 1000 * (y2 - y1) / (x2 - x1) == 1000 * (y1 - y0) / (x1 - x0))) {
                    m_pPolygon[cnt - adjust - 1] = m_pPolygon[cnt - adjust];
                    adjust++;
                }
            }
        }

        m_nPolygon -= adjust;
    } else {
        m_pPolygon = NULL;
    }

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id,
        this,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CPoint pos;
        pos.x = (pTriggerObject->m_boundingRectLeft + pTriggerObject->m_boundingRectRight) / 2;
        pos.y = (pTriggerObject->m_boundingRectTop + pTriggerObject->m_boundingRectBottom) / 2;

        AddToArea(pArea,
            pos,
            0,
            m_triggerType == 0 ? LIST_FRONT : LIST_BACK);

        m_boundingRange = static_cast<WORD>(max((pTriggerObject->m_boundingRectRight - pTriggerObject->m_boundingRectLeft) / 2,
            2 * (pTriggerObject->m_boundingRectBottom - pTriggerObject->m_boundingRectTop) / 3));

        m_typeAI.SetName(CString(m_scriptName));

        CVariable name;
        name.SetName(CString(m_scriptName));
        name.m_intValue = m_id;
        pArea->GetNamedCreatures()->AddKey(name);

        if ((m_dwFlags & 0x200) != 0) {
            field_628.x = pTriggerObject->field_88;
            field_628.y = pTriggerObject->field_8C;
        }

        sub_481890(&m_rBounding, field_630);

        // FIXME: One assignment is usually enough.
        field_644 = NULL;
        field_644 = NULL;

        if (m_nPolygon != 0) {
            field_644 = new CAreaPoint[m_nPolygon];

            for (WORD cnt = 0; cnt < m_nPolygon; cnt++) {
                field_644[cnt].m_xPos = static_cast<WORD>(m_pPolygon[cnt].x);
                field_644[cnt].m_yPos = static_cast<WORD>(m_pPolygon[cnt].y);
            }
        }
    } else {
        delete this;
    }
}

// 0x4CD150
CGameTrigger::~CGameTrigger()
{
    if (m_pPolygon != NULL) {
        delete m_pPolygon;
    }

    if (field_644 != NULL) {
        delete field_644;
        field_644 = NULL;
    }

    // When there is only one element its an unowned pointer to `m_rBounding`.
    if (field_630.GetCount() > 1) {
        for (INT nIndex = 0; nIndex < field_630.GetCount(); nIndex++) {
            delete field_630[nIndex];
        }
    }

    field_630.SetSize(0);
}

// 0x47C830
BOOLEAN CGameTrigger::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}

// 0x4CD220
BOOLEAN CGameTrigger::DoAIUpdate(BOOLEAN active, LONG counter)
{
    // NOTE: Unlike CGameObject::DoAIUpdate (0x44CC00) the trigger's version
    // ignores `active` and `counter`: a trigger with a script gets an AI
    // update every time, paused or not.
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameObjectArray* pObjectArray = pGame->GetObjectArray();

    if ((m_dwFlags & 0x100) != 0) {
        return FALSE;
    }

    if (m_triggerType == 0) {
        if ((m_dwFlags & 0x20) != 0 && pGame->GetOptions()->m_nTutorialState == 0) {
            return FALSE;
        }

        if (m_trapActivated != 0) {
            CTypedPtrList<CPtrList, LONG*> lObjects(10);
            m_pArea->GetAllInRange(m_posVertList,
                m_pos,
                CAIObjectType::ANYONE,
                m_boundingRange,
                m_pArea->m_visibleTerrainTable,
                lObjects,
                FALSE,
                FALSE);

            POSITION pos = lObjects.GetHeadPosition();
            while (pos != NULL) {
                LONG iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(lObjects.GetNext(pos)));

                CGameSprite* pSprite;
                BYTE rc = pObjectArray->GetShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
                if (rc != CGameObjectArray::SUCCESS) {
                    continue;
                }

                if (pSprite->GetObjectType() != CGameObject::TYPE_SPRITE) {
                    pObjectArray->ReleaseShare(iObject, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                    continue;
                }

                if ((m_dwFlags & 0x40) != 0
                    || pGame->GetCharacterPortraitNum(pSprite->m_id) != -1
                    || pGame->m_familiars.Find(reinterpret_cast<int*>(pSprite->m_id)) != NULL) {
                    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
                        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
                        && pSprite->field_580 == 1) {
                        // NOTE: The original goes on to the next object
                        // without releasing this one's share (a leak, kept).
                        continue;
                    }

                    if (IsOverActivate(pSprite->GetPos())) {
                        CPoint ptLast(pSprite->field_536A, pSprite->field_536E);
                        if (!IsOverActivate(ptLast)) {
                            do {
                                rc = pObjectArray->GetDeny(iObject,
                                    CGameObjectArray::THREAD_ASYNCH,
                                    reinterpret_cast<CGameObject**>(&pSprite),
                                    INFINITE);
                            } while (rc == CGameObjectArray::DENIED || rc == CGameObjectArray::SHARED);

                            if (rc == CGameObjectArray::SUCCESS) {
                                // NOTE: CAITrigger::ENTERED (0x847E30).
                                CAITrigger entered(CAITRIGGER_ENTERED, pSprite->GetAIType(), 0);
                                CMessage* message = new CMessageSetTrigger(entered, m_id, m_id);
                                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                                pObjectArray->ReleaseDeny(iObject, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                            }

                            if ((m_dwFlags & 0x2) == 0) {
                                m_trapActivated = 0;
                            }
                        }
                    }

                    const CPoint& ptPos = pSprite->GetPos();
                    pSprite->field_536A = ptPos.x;
                    pSprite->field_536E = ptPos.y;
                }

                pObjectArray->ReleaseShare(iObject, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            }

            lObjects.RemoveAll();
            return TRUE;
        }
    }

    return CResRef(m_scriptRes) != "";
}

// 0x4CD630
void CGameTrigger::AIUpdate()
{
    if (m_drawPoly > 0) {
        m_drawPoly--;
    }

    ProcessAI();
}

// 0x4CE180
BOOLEAN CGameTrigger::CompressTime(DWORD deltaTime)
{
    if (m_triggerType == 0) {
        // NOTE: Unsigned compare.
        if (deltaTime > static_cast<DWORD>(m_drawPoly)) {
            m_drawPoly = 0;
        } else {
            m_drawPoly = static_cast<SHORT>(m_drawPoly - deltaTime);
        }
    }

    return TRUE;
}

// 0x4CE1C0
void CGameTrigger::DebugDump(const CString& message, BOOLEAN bEchoToScreen)
{
    STR_RES strRes;
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    CString sTemp;

    switch (m_triggerType) {
    case 0:
        if (bEchoToScreen) {
            pWorld->DisplayText(CString(""),
                CString("DEBUG DUMP: CGameTrigger - Proximity"),
                -1,
                FALSE);

            pWorld->DisplayText(CString(""),
                message,
                -1,
                FALSE);

            sTemp.Format("Current Area: %.*s", RESREF_SIZE, m_pArea->m_resRef.GetResRef());
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Current Position: x=%d y=%d", m_pos.x, m_pos.y);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Key Type: %.*s", RESREF_SIZE, m_keyType.GetResRef());
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Trapped: %s", m_trapActivated != 0 ? "TRUE" : "FALSE");
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Trap Detected: %s", m_trapDetected != 0 ? "TRUE" : "FALSE");
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Reset Trap: %s", (m_dwFlags & 0x2) != 0 ? "TRUE" : "FALSE");
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Trap Detection Difficulty: %d%%", m_trapDetectionDifficulty);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Trap Removal Difficulty: %d%%", m_trapDisarmingDifficulty);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Trap Script: %.*s", RESREF_SIZE, m_scriptRes);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Trap Launching Point: x=%d y=%d", m_posTrapOrigin.x, m_posTrapOrigin.y);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);
        }
        break;
    case 1:
        if (bEchoToScreen) {
            pWorld->DisplayText(CString(""),
                CString("DEBUG DUMP: CGameTrigger - Info"),
                -1,
                FALSE);

            pWorld->DisplayText(CString(""),
                message,
                -1,
                FALSE);

            sTemp.Format("Current Area: %.*s", RESREF_SIZE, m_pArea->m_resRef.GetResRef());
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Current Position: x=%d y=%d", m_pos.x, m_pos.y);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("STRREF: %d", m_description);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            // FIXME: `strRes` is never fetched (always empty string)
            sTemp.Format("Description: %s", (LPCSTR)strRes.szText);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);
        }
        break;
    case 2:
        if (bEchoToScreen) {
            pWorld->DisplayText(CString(""),
                CString("DEBUG DUMP: CGameTrigger - Travel"),
                -1,
                FALSE);

            pWorld->DisplayText(CString(""),
                message,
                -1,
                FALSE);

            sTemp.Format("Current Area: %.*s", RESREF_SIZE, m_pArea->m_resRef.GetResRef());
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Current Position: x=%d y=%d", m_pos.x, m_pos.y);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Target Area: %.*s", RESREF_SIZE, m_newArea);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Entry Point Name: %.*s", SCRIPTNAME_SIZE, m_newEntryPoint);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);

            sTemp.Format("Party Required: %s", (m_dwFlags & 0x4) != 0 ? "TRUE" : "FALSE");
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);
        }
        break;
    }
}

// 0x4CEBE0
BOOL CGameTrigger::IsOver(const CPoint& pt)
{
    if ((m_dwFlags & 0x100) != 0) {
        return FALSE;
    }

    if (m_triggerType == 0) {
        if (m_trapActivated == 0) {
            return FALSE;
        }

        if (m_trapDetected == 0) {
            return FALSE;
        }

        if (g_pBaldurChitin->GetObjectGame()->GetState() != 2) {
            return FALSE;
        }

        if (g_pBaldurChitin->GetObjectGame()->GetIconIndex() != 36) {
            return FALSE;
        }
    }

    if (!m_rBounding.PtInRect(pt)) {
        return FALSE;
    }

    if (!g_pBaldurChitin->GetObjectGame()->GetGroup()->IsPartyLeader()) {
        return FALSE;
    }

    if (m_pPolygon != NULL) {
        return CVidPoly::IsPtInPoly(m_pPolygon, m_nPolygon, pt);
    }

    return TRUE;
}

// 0x4CECB0
BOOL CGameTrigger::IsOverActivate(const CPoint& pt)
{
    if (!m_rBounding.PtInRect(pt)) {
        return FALSE;
    }

    if (m_pPolygon != NULL) {
        return CVidPoly::IsPtInPoly(m_pPolygon, m_nPolygon, pt);
    }

    return TRUE;
}

// 0x4CED10
void CGameTrigger::OnActionButton(const CPoint& pt)
{
    // A click on the region: its script learns it was clicked (by the
    // group's leader); then by the game's state: a travel region sends the
    // group there to leave the area, an info region shows its text, a trap
    // with the disarm cursor is disarmed, anything else is a click on the
    // ground.
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CAIGroup* pGroup = &pGame->m_group;
    BYTE rc;

    if (pGroup->m_memberList.GetCount() != 0) {
        CAIObjectType leaderType;

        LONG nLeader = pGroup->GetGroupLeader();
        if (nLeader != CGameObjectArray::INVALID_INDEX) {
            // NOTE: The original's retry loop tests the result for SHARED
            // and then for DENIED on the same path (0x4CEE6B-0x4CEE79), so
            // it never retries; one attempt here.
            CGameAIBase* pLeader;
            rc = pGame->GetObjectArray()->GetShare(nLeader,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pLeader),
                INFINITE);

            if (rc == CGameObjectArray::SUCCESS) {
                leaderType.Set(pLeader->GetAIType());
                pGame->GetObjectArray()->ReleaseShare(nLeader,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        // NOTE: CAITrigger::CLICKED (0x847E78).
        CAITrigger clicked(CAITRIGGER_CLICKED, leaderType, 0);
        CMessage* message = new CMessageSetTrigger(clicked, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    switch (pGame->m_nState) {
    case 0:
        pGame->m_lastClick.x = -1;
        pGame->m_lastClick.y = -1;
        pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;

        if (pGroup->GetCount() == 0) {
            return;
        }

        if (m_triggerType == 2) {
            // Travel: LeaveAreaName (93, the SHORT at 0x84782E) on this
            // region, then a move to it (its alternate point with the flag
            // 0x200); the group's AddAction puts the move first.
            CAIAction leave(93, CAIObjectType(), m_id, 0, 0);
            pGroup->GroupAction(leave, TRUE, NULL);

            CPoint ptDest = (m_dwFlags & 0x200) != 0 ? field_628 : m_pos;
            CAIAction move(CAIAction::MOVETOPOINT, ptDest, 0, -1);
            pGroup->GroupAction(move, FALSE, NULL);

            pGroup->SetGroupTriggerId(m_id);

            if (m_description != -1 && m_description != 0) {
                STR_RES strRes;
                g_pBaldurChitin->GetTlkTable().Fetch(m_description, strRes);
                g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                    strRes.szText,
                    RGB(0xD7, 0xD7, 0xBE),
                    RGB(0xB4, 0xB4, 0xFF),
                    -1,
                    FALSE);
            }
            return;
        }

        if (m_triggerType == 1) {
            // Info: the text floats over the region.
            if (m_description != -1) {
                FloatText(m_description, 10, 5);
            }
            return;
        }

        CGameObject::OnActionButton(pt);
        return;
    case 2:
        pGame->m_lastClick.x = -1;
        pGame->m_lastClick.y = -1;
        pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;

        switch (pGame->m_iconIndex) {
        case 0x0C:
        case 0x12:
        case 0x14:
        case 0x28:
        case 0xFF:
            CGameObject::OnActionButton(pt);
            return;
        case 0x24:
            // The disarm cursor on an active, detected trap: the selected
            // character with ranks in Disable Device (the base skill 5)
            // sends the group to remove it.
            if (m_triggerType == 0 && m_trapActivated != 0 && m_trapDetected != 0) {
                SHORT nSelected = static_cast<SHORT>(g_pBaldurChitin->m_pEngineWorld->GetSelectedCharacter());
                LONG nSelectedId = nSelected < pGame->m_nCharacters
                    ? pGame->m_characterPortraits[nSelected]
                    : CGameObjectArray::INVALID_INDEX;

                CGameSprite* pSprite;
                do {
                    rc = pGame->GetObjectArray()->GetShare(nSelectedId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    BYTE nDisableDevice = pSprite->m_baseStats.m_skills[5];
                    pGame->GetObjectArray()->ReleaseShare(nSelectedId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    if (nDisableDevice == 0) {
                        return;
                    }
                }

                // RemoveTraps (28, the SHORT at 0x8477BA) on this region.
                CAIAction removeTraps(28, m_typeAI, 0, 0, 0);
                pGroup->GroupAction(removeTraps, TRUE, NULL);

                pGame->m_nState = 0;
                pGame->m_cButtonArray.m_nSelectedButton = 100;
                pGame->m_cButtonArray.UpdateState();
                return;
            }

            CGameObject::OnActionButton(pt);
            return;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
            // __LINE__: 973
            UTIL_ASSERT(FALSE);
            return;
        }
    case 3:
        pGame->m_lastClick.x = -1;
        pGame->m_lastClick.y = -1;
        pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;
        CGameObject::OnActionButton(pt);
        return;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
        // __LINE__: 984
        UTIL_ASSERT(FALSE);
        return;
    }
}

// 0x4CFA60
void CGameTrigger::Render(CGameArea* pArea, CVidMode* pVidMode, INT nSurface)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
    // __LINE__: 1020
    UTIL_ASSERT(pVidMode != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // The highlight colours (COLORREFs at 0x8A85FC, 0x8A8608 and 0x8A8600,
    // only read): a found trap under the thieving cursor, a trap whose
    // outline is drawn for a while (m_drawPoly), and every region while the
    // trigger polygons are shown (CInfGame::field_4AB2, the cheat key).
    const COLORREF rgbAction = RGB(0, 250, 0);
    const COLORREF rgbDrawPoly = RGB(255, 0, 0);
    const COLORREF rgbShowAll = RGB(0, 0, 255);

    COLORREF rgbColor;
    if (m_triggerType == 0
        && m_trapActivated != 0
        && m_trapDetected != 0
        && m_id == m_pArea->m_iPicked
        && pGame->m_nState == 2
        && pGame->m_iconIndex == 36) {
        rgbColor = rgbAction;
    } else if (m_triggerType == 0 && m_drawPoly > 0) {
        rgbColor = rgbDrawPoly;
    } else {
        if (!pGame->field_4AB2) {
            return;
        }

        rgbColor = rgbShowAll;
    }

    if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated) {
        FillPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbColor));
    }

    m_pArea->GetInfinity()->OutlinePoly(m_pPolygon, m_nPolygon, m_rBounding, rgbColor);
}

// 0x4CFC20
void CGameTrigger::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
        // __LINE__: 111
        UTIL_ASSERT(FALSE);
    }

    delete this;
}

// 0x4CFC80
void CGameTrigger::SetCursor(LONG nToolTip)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if ((m_dwFlags & 0x100) == 0) {
        switch (pGame->GetState()) {
        case 0:
            if (pGame->GetGroup()->GetCount() != 0) {
                if (m_triggerType != 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(m_cursorType, FALSE);
                } else {
                    CGameObject::SetCursor(nToolTip);
                }
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
            }
            break;
        case 1:
        case 3:
            CGameObject::SetCursor(nToolTip);
            break;
        case 2:
            switch (pGame->GetIconIndex()) {
            case 12:
            case 18:
            case 40:
            case 255:
                CGameObject::SetCursor(nToolTip);
                break;
            case 36:
                if (m_triggerType == 0
                    && m_trapActivated != 0
                    && m_trapDetected != 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(38, FALSE);

                    SHORT nPortrait = g_pBaldurChitin->m_pEngineWorld->GetSelectedCharacter();

                    // NOTE: Uninline.
                    LONG nCharacterId = pGame->GetCharacterId(nPortrait);

                    CGameSprite* pSprite;

                    BYTE rc;
                    do {
                        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        if (pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_DISABLE_DEVICE] == 0) {
                            g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                        }

                        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                } else {
                    CGameObject::SetCursor(nToolTip);
                }
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
                // __LINE__: 1200
                UTIL_ASSERT(FALSE);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
            // __LINE__: 1210
            UTIL_ASSERT(FALSE);
        }
    } else {
        CGameObject::SetCursor(nToolTip);
    }
}

// 0x4CFF70
void CGameTrigger::Marshal(CAreaFileTriggerObject** pTriggerObject)
{
    CAreaPoint* pPoints;
    DWORD cnt;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
    // __LINE__: 1235
    UTIL_ASSERT(pTriggerObject != NULL);

    *pTriggerObject = new CAreaFileTriggerObject();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
    // __LINE__: 1241
    UTIL_ASSERT(*pTriggerObject != NULL);

    // FIXME: Redundant, memset is a part of constructor.
    memset(*pTriggerObject, 0, sizeof(CAreaFileTriggerObject));

    if (m_nPolygon != 0) {
        pPoints = new CAreaPoint[m_nPolygon];
        if (pPoints != NULL) {
            for (cnt = 0; cnt < m_nPolygon; cnt++) {
                pPoints[cnt].m_xPos = 0;
                pPoints[cnt].m_yPos = 0;
            }
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
            // __LINE__: 1246
            UTIL_ASSERT(pPoints != NULL);
        }

        for (cnt = 0; cnt < m_nPolygon; cnt++) {
            pPoints[cnt].m_xPos = static_cast<WORD>(m_pPolygon[cnt].x);
            pPoints[cnt].m_yPos = static_cast<WORD>(m_pPolygon[cnt].y);
        }

        // The game-file writers resolve the handle (compat/marshal_ptr.h).
        (*pTriggerObject)->m_pickPointStart = iwd2r::MarshalPtr(pPoints);
        (*pTriggerObject)->m_pickPointCount = m_nPolygon;
    }

    (*pTriggerObject)->m_boundingRectLeft = static_cast<WORD>(m_rBounding.left);
    (*pTriggerObject)->m_boundingRectTop = static_cast<WORD>(m_rBounding.top);
    (*pTriggerObject)->m_boundingRectRight = static_cast<WORD>(m_rBounding.right) - 1;
    (*pTriggerObject)->m_boundingRectBottom = static_cast<WORD>(m_rBounding.bottom) - 1;

    if ((m_dwFlags & 0x200) == 0) {
        (*pTriggerObject)->m_transitionWalkToX = static_cast<WORD>(m_pos.x);
        (*pTriggerObject)->m_transitionWalkToY = static_cast<WORD>(m_pos.y);
    } else {
        (*pTriggerObject)->m_transitionWalkToX = static_cast<WORD>(field_628.x);
        (*pTriggerObject)->m_transitionWalkToY = static_cast<WORD>(field_628.y);
    }

    (*pTriggerObject)->m_triggerType = m_triggerType;
    (*pTriggerObject)->m_cursorType = m_cursorType;
    memcpy((*pTriggerObject)->m_newArea, m_newArea, RESREF_SIZE);
    memcpy((*pTriggerObject)->m_newEntryPoint, m_newEntryPoint, SCRIPTNAME_SIZE);
    (*pTriggerObject)->m_dwFlags = m_dwFlags;
    (*pTriggerObject)->m_description = m_description;
    memcpy((*pTriggerObject)->m_script, m_scriptRes, RESREF_SIZE);
    strncpy((*pTriggerObject)->m_scriptName, m_scriptName, SCRIPTNAME_SIZE);
    (*pTriggerObject)->m_trapDetectionDifficulty = m_trapDetectionDifficulty;
    (*pTriggerObject)->m_trapDisarmingDifficulty = m_trapDisarmingDifficulty;
    (*pTriggerObject)->m_trapActivated = m_trapActivated;
    (*pTriggerObject)->m_trapDetected = m_trapDetected;
    (*pTriggerObject)->m_posXTrapOrigin = static_cast<WORD>(m_posTrapOrigin.x);
    (*pTriggerObject)->m_posYTrapOrigin = static_cast<WORD>(m_posTrapOrigin.y);
    m_keyType.GetResRef((*pTriggerObject)->m_keyType);
    (*pTriggerObject)->field_88 = field_628.x;
    (*pTriggerObject)->field_8C = field_628.y;
}

// 0x4D0230
void CGameTrigger::SetDrawPoly(SHORT time)
{
    if (m_drawPoly != time) {
        if (!g_pBaldurChitin->GetBaldurMessage()->m_bInMessageSetDrawPoly) {
            CMessageSetDrawPoly* pMessage = new CMessageSetDrawPoly(time, GetId(), GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }
    }
    m_drawPoly = time;
}

// 0x4D02A0
CPoint& CGameTrigger::GetPos()
{
    return (m_dwFlags & 0x200) != 0 ? field_628 : m_pos;
}

// The trigger's polygon filled with dwColor (in the surface's format)
// through the FX surface, piece by piece: each of the rectangles that cover
// it (field_630) that is on screen.
//
// 0x4D02C0
void CGameTrigger::FillPoly(CGameArea* pArea, CVidMode* pVidMode, INT nSurface, DWORD dwColor)
{
    CInfinity* pInfinity = pArea->GetInfinity();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameTrigger.cpp
    // __LINE__: 1432
    UTIL_ASSERT(pInfinity != NULL && pVidMode != NULL);

    CRect rViewPort;
    rViewPort.left = m_pArea->GetInfinity()->nCurrentX;
    rViewPort.top = m_pArea->GetInfinity()->nCurrentY;
    rViewPort.right = m_pArea->GetInfinity()->rViewPort.Width() + rViewPort.left;
    rViewPort.bottom = m_pArea->GetInfinity()->rViewPort.Height() + rViewPort.top;

    INT nRects = field_630.GetSize();
    CPoint ptReference(0, 0);

    CVidPoly cPoly;
    cPoly.SetPoly(reinterpret_cast<WORD*>(field_644), m_nPolygon);

    for (INT nRect = 0; nRect < nRects; nRect++) {
        CRect* pRect = field_630[nRect];
        if (pRect != NULL) {
            CRect rIntersect;
            IntersectRect(&rIntersect, &rViewPort, pRect);

            CRect rClip(rIntersect);
            if (rIntersect.left != 0 || rIntersect.right != 0 || rIntersect.top != 0 || rIntersect.bottom != 0) {
                CPoint ptPos(rIntersect.left, rIntersect.top);

                CRect rFx(rIntersect);
                OffsetRect(&rFx, -rIntersect.left, -rIntersect.top);

                if (pInfinity->FXPrep(rFx, CInfinity::FXPREP_COPYFROMBACK, nSurface, ptPos, ptReference)) {
                    if (pInfinity->FXLock(rFx, 0)) {
                        static_cast<CVidInf*>(pVidMode)->FXRender(&cPoly, rClip, dwColor, 0x10, ptPos, FALSE);

                        if (pInfinity->FXUnlock(0, NULL, CPoint(0, 0))) {
                            pInfinity->FXBltFrom(nSurface,
                                rFx,
                                ptPos.x,
                                ptPos.y,
                                ptReference.x,
                                ptReference.y,
                                0);
                        }
                    }
                }
            }
        }
    }
}

// 0x45B950
BOOL CGameTrigger::sub_45B950()
{
    return m_trapActivated && (m_dwFlags & 0x100) == 0;
}

// 0x4CD650
void CGameTrigger::AddEffect(CGameEffect* pEffect, BYTE list, BOOL noSave, BOOL immediateApply)
{
    switch (pEffect->m_effectID) {
    case CGAMEEFFECT_DETECTTRAPS:
        if (m_trapActivated != 0 && (m_dwFlags & 0x100) == 0) {
            if (m_trapDetected == 0 && m_trapDetectionDifficulty < 100 && (m_dwFlags & 0x8) != 0) {
                CAITrigger trigger(CAITRIGGER_DETECTED, m_typeAI, 0);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(
                    new CMessageSetTrigger(trigger, m_id, m_id), FALSE);
                m_trapDetected = 1;
                g_pBaldurChitin->GetMessageHandler()->AddMessage(
                    new CMessageTriggerStatus(m_dwFlags,
                        m_trapActivated != 0 && (m_dwFlags & 0x100) == 0, m_trapDetected, m_id, m_id),
                    FALSE);
                CGameObject* pObject = m_typeAI.sub_40CCA0(reinterpret_cast<CGameAIBase*>(m_id), FALSE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
                if (pObject != NULL) {
                    CAIObjectType spriteType(CGameObject::TYPE_SPRITE, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
                    if (pObject->GetAIType().Equal(spriteType)) {
                        static_cast<CGameAIBase*>(pObject)->AutoPause(0x80);
                    }
                }
            }
            if (m_trapDetected != 0) {
                if (m_drawPoly != 400 && !g_pBaldurChitin->GetBaldurMessage()->m_bInMessageSetDrawPoly) {
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(
                        new CMessageSetDrawPoly(400, m_id, m_id), FALSE);
                }
                m_drawPoly = 400;
            }
        }
        break;
    case CGAMEEFFECT_CASTSPELL: {
        CAIObjectType acteeType;
        acteeType.Set(GetAIType());
        CAIAction forceSpell;
        forceSpell.m_actionID = CAIAction::FORCESPELL;
        forceSpell.m_acteeID.Set(acteeType);
        CString spellRes;
        pEffect->m_res.CopyToString(spellRes);
        forceSpell.SetString1(spellRes);
        forceSpell.m_specificID = pEffect->m_effectAmount;
        g_pBaldurChitin->GetMessageHandler()->AddMessage(
            new CMessageAddAction(forceSpell, m_id, pEffect->m_sourceID), FALSE);
        break;
    }
    case CGAMEEFFECT_SUMMON: {
        ITEM_EFFECT removal;
        CGameEffect::ClearItemEffect(&removal, 68);
        removal.durationType = 4;
        removal.duration = (pEffect->m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime) / 15;
        removal.effectAmount = 1;
        CGameEffect* pRemoval = CGameEffect::DecodeEffect(&removal, CPoint(-1, -1), -1, CPoint(-1, -1));
        LONG x = pEffect->m_target.x / CPathSearch::GRID_SQUARE_SIZEX;
        LONG y = pEffect->m_target.y / CPathSearch::GRID_SQUARE_SIZEY;
        CCreatureFile creature;
        creature.SetResRef(pEffect->m_res, TRUE, TRUE);
        DWORD size = creature.GetDataSize();
        BYTE* data = creature.GetData();
        CGameSprite* pSprite = new CGameSprite(data, size, 0, 0, static_cast<DWORD>(-1),
            0, 0, 0x7FFFFFFF, CPoint(-1, -1), 0);
        pSprite->SetResRef(CResRef(creature.GetResRef().GetResRefStr()));
        creature.ReleaseData();
        if (pSprite != NULL) {
            LONG id = pSprite->GetId();
            CString res;
            pEffect->m_res.CopyToString(res);
            pSprite->SetResRef(CResRef(res));
            BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(id,
                CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                CPoint grid = m_pArea->m_search.GetNearestOpenSquare(CPoint(x, y), GetTerrainTable(),
                    pSprite->GetAnimation()->GetPersonalSpace(), -1);
                CPoint pos(grid.x * CPathSearch::GRID_SQUARE_SIZEX, grid.y * CPathSearch::GRID_SQUARE_SIZEY);
                pSprite->AddToArea(m_pArea, pos, 0, pSprite->GetAnimation()->GetListType());
                CProjectile* pProjectile = CProjectile::DecodeProjectile(144, pSprite, 0);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(
                    new CMessageFireProjectile(pProjectile->m_projectileType, id, CPoint(0, 0), 30, id, id, 0), FALSE);
                pProjectile->Fire(m_pArea, id, id, CPoint(0, 0), 0, 0);
                CAIObjectType type;
                type.Set(pSprite->GetAIType());
                CAIObjectType live;
                live.Set(pSprite->m_liveTypeAI);
                CAIObjectType start;
                start.Set(pSprite->m_liveTypeAI);
                if (pEffect->m_durationType != 1) {
                    pRemoval->m_casterLevel = pEffect->m_casterLevel;
                    pSprite->AddEffect(pRemoval, 1, TRUE, TRUE);
                }
                pSprite->SetSequence(1);
                pSprite->SetFacing(rand() % 16);
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(id,
                    CGameObjectArray::THREAD_ASYNCH, INFINITE);
            }
        }
        break;
    }
    default:
        break;
    }
    delete pEffect;
}
