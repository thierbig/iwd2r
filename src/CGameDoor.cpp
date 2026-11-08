#include "CGameDoor.h"

#include "CAIScript.h"
#include "CBaldurChitin.h"
#include "CAIGroup.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CTiledObject.h"
#include "CTimerWorld.h"
#include "CPathSearch.h"
#include "CScreenWorld.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CVidPoly.h"
#include "marshal_ptr.h"

// 0x8D41D8
const LONG CGameDoor::RANGE_DOOR = 16
    * CPathSearch::GRID_SQUARE_SIZEX
    * CPathSearch::GRID_SQUARE_SIZEX;

// 0x485AC0
CGameDoor::CGameDoor(CGameArea* pArea, CAreaFileDoorObject* pDoorObject, CAreaPoint* pPoints, WORD maxPts)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 107
    UTIL_ASSERT(pArea != NULL && pDoorObject != NULL && pPoints != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 108
    UTIL_ASSERT(pDoorObject->m_openSelectionPointStart + pDoorObject->m_openSelectionPointCount <= maxPts);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 109
    UTIL_ASSERT(pDoorObject->m_closedSelectionPointStart + pDoorObject->m_closedSelectionPointCount <= maxPts);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 110
    UTIL_ASSERT(pDoorObject->m_openSearchSquaresStart + pDoorObject->m_openSearchSquaresCount <= maxPts);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 111
    UTIL_ASSERT(pDoorObject->m_closedSearchSquaresStart + pDoorObject->m_closedSearchSquaresCount <= maxPts);

    m_objectType = TYPE_DOOR;
    m_resID = pDoorObject->m_doorID;

    m_rOpenBounding.left = pDoorObject->m_openBoundingRectLeft;
    m_rOpenBounding.top = pDoorObject->m_openBoundingRectTop;
    m_rOpenBounding.right = pDoorObject->m_openBoundingRectRight + 1;
    m_rOpenBounding.bottom = pDoorObject->m_openBoundingRectBottom + 1;

    m_rClosedBounding.left = pDoorObject->m_closedBoundingRectLeft;
    m_rClosedBounding.top = pDoorObject->m_closedBoundingRectTop;
    m_rClosedBounding.right = pDoorObject->m_closedBoundingRectRight + 1;
    m_rClosedBounding.bottom = pDoorObject->m_closedBoundingRectBottom + 1;

    m_cursorType = pDoorObject->m_cursorType;
    m_dwFlags = pDoorObject->m_dwFlags;
    m_nOpenPolygon = pDoorObject->m_openSelectionPointCount;
    m_nClosedPolygon = pDoorObject->m_closedSelectionPointCount;

    m_ptDest1.x = pDoorObject->m_posXWalkTo1;
    m_ptDest1.y = pDoorObject->m_posYWalkTo1;

    m_ptDest2.x = pDoorObject->m_posXWalkTo2;
    m_ptDest2.y = pDoorObject->m_posYWalkTo2;

    m_strNotPickable = pDoorObject->m_strNotPickable;

    POSITION pos = pArea->m_lTiledObjects.AddTail(&m_tiledObject);
    WORD wInitialState = (m_dwFlags & 0x1) != 0
        ? CTiledObject::STATE_PRIMARY_TILE
        : CTiledObject::STATE_SECONDARY_TILE;
    m_tiledObject.Initialize(pArea->m_pResWED,
        m_resID,
        pos,
        wInitialState);

    memcpy(m_scriptRes, pDoorObject->m_script, RESREF_SIZE);

    strncpy(m_scriptName, pDoorObject->m_scriptName, SCRIPTNAME_SIZE);

    CAIScript* pScript = new CAIScript(CResRef(pDoorObject->m_script));
    SetScript(0, pScript);

    m_hitPoints = pDoorObject->m_hitPoints != 0 ? pDoorObject->m_hitPoints : 20;
    m_armourClass = pDoorObject->m_armourClass;
    m_openSound = pDoorObject->m_openSound;
    m_closeSound = pDoorObject->m_closeSound;
    m_trapDetectionDifficulty = pDoorObject->m_trapDetectionDifficulty;
    m_trapDisarmingDifficulty = pDoorObject->m_trapDisarmingDifficulty;
    m_trapActivated = pDoorObject->m_trapActivated;
    m_trapDetected = pDoorObject->m_trapDetected;
    m_posXTrapOrigin = pDoorObject->m_posXTrapOrigin;
    m_posYTrapOrigin = pDoorObject->m_posYTrapOrigin;
    m_keyType = pDoorObject->m_keyType;
    m_detectionDifficulty = pDoorObject->m_detectionDifficulty;
    m_lockDifficulty = pDoorObject->m_lockDifficulty;
    m_drawPoly = 0;

    if (m_nOpenPolygon != 0) {
        m_pOpenPolygon = new CPoint[m_nOpenPolygon];

        if (m_pOpenPolygon == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
            // __LINE__: 170
            UTIL_ASSERT(FALSE);
        }

        WORD adjust = 0;
        for (WORD cnt = 0; cnt < m_nOpenPolygon; cnt++) {
            m_pOpenPolygon[cnt - adjust].x = pPoints[cnt + pDoorObject->m_openSelectionPointStart].m_xPos;
            m_pOpenPolygon[cnt - adjust].y = pPoints[cnt + pDoorObject->m_openSelectionPointStart].m_yPos;
            if (cnt >= 2) {
                int x2 = m_pOpenPolygon[cnt - adjust - 2].x;
                int y2 = m_pOpenPolygon[cnt - adjust - 2].y;

                int x1 = m_pOpenPolygon[cnt - adjust - 1].x;
                int y1 = m_pOpenPolygon[cnt - adjust - 1].y;

                int x0 = m_pOpenPolygon[cnt - adjust].x;
                int y0 = m_pOpenPolygon[cnt - adjust].y;

                if ((x2 == x1 && x1 == x0)
                    || (y2 == y1 && y1 == y0)
                    || (x2 != x1
                        && x1 != x0
                        && 1000 * (y2 - y1) / (x2 - x1) == 1000 * (y1 - y0) / (x1 - x0))) {
                    m_pOpenPolygon[cnt - adjust - 1] = m_pOpenPolygon[cnt - adjust];
                    adjust++;
                }
            }
        }

        m_nOpenPolygon -= adjust;
    } else {
        m_pOpenPolygon = NULL;
    }

    if (m_nClosedPolygon != 0) {
        m_pClosedPolygon = new CPoint[m_nClosedPolygon];

        if (m_pClosedPolygon == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
            // __LINE__: 206
            UTIL_ASSERT(FALSE);
        }

        WORD adjust = 0;
        for (WORD cnt = 0; cnt < m_nClosedPolygon; cnt++) {
            m_pClosedPolygon[cnt - adjust].x = pPoints[cnt + pDoorObject->m_closedSelectionPointStart].m_xPos;
            m_pClosedPolygon[cnt - adjust].y = pPoints[cnt + pDoorObject->m_closedSelectionPointStart].m_yPos;
            if (cnt >= 2) {
                int x2 = m_pClosedPolygon[cnt - adjust - 2].x;
                int y2 = m_pClosedPolygon[cnt - adjust - 2].y;

                int x1 = m_pClosedPolygon[cnt - adjust - 1].x;
                int y1 = m_pClosedPolygon[cnt - adjust - 1].y;

                int x0 = m_pClosedPolygon[cnt - adjust].x;
                int y0 = m_pClosedPolygon[cnt - adjust].y;

                if ((x2 == x1 && x1 == x0)
                    || (y2 == y1 && y1 == y0)
                    || (x2 != x1
                        && x1 != x0
                        && 1000 * (y2 - y1) / (x2 - x1) == 1000 * (y1 - y0) / (x1 - x0))) {
                    m_pClosedPolygon[cnt - adjust - 1] = m_pClosedPolygon[cnt - adjust];
                    adjust++;
                }
            }
        }

        m_nClosedPolygon -= adjust;
    } else {
        m_pClosedPolygon = NULL;
    }

    m_nOpenSearch = pDoorObject->m_openSearchSquaresCount;
    if (m_nOpenSearch != 0) {
        m_pOpenSearch = new CPoint[m_nOpenSearch];

        if (m_pOpenSearch == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
            // __LINE__: 243
            UTIL_ASSERT(FALSE);
        }

        for (WORD cnt = 0; cnt < m_nOpenSearch; cnt++) {
            m_pOpenSearch[cnt].x = pPoints[pDoorObject->m_openSearchSquaresStart + cnt].m_xPos;
            m_pOpenSearch[cnt].y = pPoints[pDoorObject->m_openSearchSquaresStart + cnt].m_yPos;
        }

        if ((m_dwFlags & 0x1) != 0) {
            pArea->m_search.AddDoor(m_pOpenSearch,
                m_nOpenSearch,
                (m_dwFlags & 0x400) != 0);
        }
    } else {
        m_pOpenSearch = NULL;
    }

    m_ptOpenDest.x = (pDoorObject->m_openBoundingRectLeft + pDoorObject->m_openBoundingRectRight) / 2;
    m_ptOpenDest.y = pDoorObject->m_openBoundingRectBottom;

    m_nClosedSearch = pDoorObject->m_closedSearchSquaresCount;
    if (m_nClosedSearch != 0) {
        m_pClosedSearch = new CPoint[m_nClosedSearch];

        if (m_pClosedSearch == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
            // __LINE__: 270
            UTIL_ASSERT(FALSE);
        }

        for (WORD cnt = 0; cnt < m_nClosedSearch; cnt++) {
            m_pClosedSearch[cnt].x = pPoints[pDoorObject->m_closedSearchSquaresStart + cnt].m_xPos;
            m_pClosedSearch[cnt].y = pPoints[pDoorObject->m_closedSearchSquaresStart + cnt].m_yPos;
        }

        if ((m_dwFlags & 0x1) == 0) {
            pArea->m_search.AddDoor(m_pClosedSearch,
                m_nClosedSearch,
                (m_dwFlags & 0x400) != 0);
        }
    } else {
        m_pClosedSearch = NULL;
    }

    m_ptClosedDest.x = (pDoorObject->m_closedBoundingRectLeft + pDoorObject->m_closedBoundingRectRight) / 2;
    m_ptClosedDest.y = pDoorObject->m_closedBoundingRectBottom;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id,
        this,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        if ((m_dwFlags & 0x1) != 0) {
            AddToArea(pArea, m_ptOpenDest, 0, LIST_FRONT);
        } else {
            AddToArea(pArea, m_ptClosedDest, 0, LIST_FRONT);
        }

        m_typeAI.SetName(CString(m_scriptName));

        CVariable name;
        name.SetName(CString(m_scriptName));
        name.m_intValue = m_id;
        pArea->GetNamedCreatures()->AddKey(name);

        sub_481890(&m_rClosedBounding, field_6DA);
        sub_481890(&m_rOpenBounding, field_6F2);

        field_6EE = NULL;
        field_706 = NULL;

        if (m_nClosedPolygon != 0) {
            field_6EE = new CAreaPoint[m_nClosedPolygon];

            for (WORD cnt = 0; cnt < m_nClosedPolygon; cnt++) {
                field_6EE[cnt].m_xPos = static_cast<WORD>(m_pClosedPolygon[cnt].x);
                field_6EE[cnt].m_yPos = static_cast<WORD>(m_pClosedPolygon[cnt].y);
            }
        } else {
            field_6EE = NULL;
        }

        if (m_nOpenPolygon != 0) {
            field_706 = new CAreaPoint[m_nOpenPolygon];

            for (WORD cnt = 0; cnt < m_nOpenPolygon; cnt++) {
                field_706[cnt].m_xPos = static_cast<WORD>(m_pOpenPolygon[cnt].x);
                field_706[cnt].m_yPos = static_cast<WORD>(m_pOpenPolygon[cnt].y);
            }
        } else {
            field_706 = NULL;
        }

        m_nAICounter = 0;
    } else {
        delete this;
    }
}

// 0x4866E0
CGameDoor::~CGameDoor()
{
    if (m_pOpenPolygon != NULL) {
        delete m_pOpenPolygon;
    }

    if (m_pClosedPolygon != NULL) {
        delete m_pClosedPolygon;
    }

    if (m_pOpenSearch != NULL) {
        delete m_pOpenSearch;
    }

    if (m_pClosedSearch != NULL) {
        delete m_pClosedSearch;
    }

    if (field_6EE != NULL) {
        delete field_6EE;
        field_6EE = NULL;
    }

    // When there is only one element its an unowned pointer to
    // `m_rClosedBounding`.
    if (field_6DA.GetCount() > 1) {
        for (INT nIndex = 0; nIndex < field_6DA.GetCount(); nIndex++) {
            delete field_6DA[nIndex];
        }
    }
    field_6DA.SetSize(0);

    if (field_706 != NULL) {
        delete field_706;
        field_706 = NULL;
    }

    // When there is only one element its an unowned pointer to
    // `m_rOpenBounding`.
    if (field_6F2.GetCount() > 1) {
        for (INT nIndex = 0; nIndex < field_6F2.GetCount(); nIndex++) {
            delete field_6F2[nIndex];
        }
    }
    field_6F2.SetSize(0);
}

// 0x486860
void CGameDoor::AddEffect(CGameEffect* pEffect, BYTE list, BOOL noSave, BOOL immediateApply)
{
    switch (pEffect->m_effectID) {
    case CGAMEEFFECT_KNOCK:
        if (m_lockDifficulty == 100) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(
                (STRREF)-1, 0x5A81, 0, 0xD7C8A0, (LONG)-1, m_id, m_id);
            break;
        }
        m_dwFlags &= ~0x2U;
        g_pBaldurChitin->GetMessageHandler()->AddMessage(
            new CMessageDoorOpen(TRUE, m_id, m_id), FALSE);
        break;

    default:
        // TODO: 0x486929-0x48746A (ranges 2-4) not ported
        break;
    }

    delete pEffect;
}

// 0x47C830
BOOLEAN CGameDoor::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}

// 0x487460
void CGameDoor::AIUpdate()
{
    if (m_drawPoly > 0) {
        m_drawPoly--;
    }

    ProcessAI();

    if (g_pBaldurChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE) {
        if (m_nAICounter++ == 225) {
            m_nAICounter = 0;

            CMessageDoorStatus* pMessage = new CMessageDoorStatus(this, m_id, m_id);

            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }
    }
}

// 0x47E040
void CGameDoor::DebugDump(const CString& message, BOOLEAN bEchoToScreen)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    CString sTemp;

    if (bEchoToScreen) {
        pWorld->DisplayText(CString(""),
            CString("DEBUG DUMP: CGameDoor"),
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

        sTemp.Format("Locked: %s\n", (m_dwFlags & 0x2) != 0 ? "TRUE" : "FALSE");
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Lock Difficulty: %d%%\n", m_lockDifficulty);
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

        sTemp.Format("Trap Launching Point: x=%d y=%d", m_posXTrapOrigin, m_posYTrapOrigin);
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);
    }
}

// 0x487C10
BOOL CGameDoor::IsOver(const CPoint& pt)
{
    if ((m_dwFlags & 0x2000) != 0) {
        return FALSE;
    }

    if ((m_dwFlags & 0x1) != 0) {
        if (!m_rOpenBounding.PtInRect(pt)) {
            return FALSE;
        }

        if (!g_pBaldurChitin->GetObjectGame()->GetGroup()->IsPartyLeader()) {
            return FALSE;
        }

        if (m_pOpenPolygon != NULL) {
            return CVidPoly::IsPtInPoly(m_pOpenPolygon, m_nOpenPolygon, pt);
        }
    } else {
        if (!m_rClosedBounding.PtInRect(pt)) {
            return FALSE;
        }

        if (!g_pBaldurChitin->GetObjectGame()->GetGroup()->IsPartyLeader()) {
            return FALSE;
        }

        if (m_pClosedPolygon != NULL) {
            return CVidPoly::IsPtInPoly(m_pClosedPolygon, m_nClosedPolygon, pt);
        }
    }

    return TRUE;
}

// 0x488B30
void CGameDoor::Render(CGameArea* pArea, CVidMode* pVidMode, INT nSurface)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    SHORT nState = pGame->m_nState;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 1028
    UTIL_ASSERT(pVidMode != NULL);

    // The highlight colours (COLORREFs at 0x8A85F8, 0x8A85FC, 0x8A8604 and
    // 0x8A8608, only read): the picked door, a trapped or locked one under
    // the thieving cursors, a door with flags 0x80 and 0x100 whose outline
    // is drawn for a while, and the others drawn for a while or while the
    // menu key is held. An open door (flag 0x1) is drawn with its open
    // polygon, a closed one with its closed polygon.
    const COLORREF rgbPicked = RGB(32, 64, 160);
    const COLORREF rgbAction = RGB(0, 250, 0);
    const COLORREF rgbDrawPolyHidden = RGB(255, 0, 255);
    const COLORREF rgbDrawPoly = RGB(255, 0, 0);

    BOOL bFilled = FALSE;

    if ((m_dwFlags & 0x2000) != 0) {
        return;
    }

    if (m_id == m_pArea->m_iPicked
        && m_pArea->m_visibility.IsTileExplored(m_pArea->m_visibility.PointToTile(m_pos))
        && (m_dwFlags & 0x80) == 0) {
        if (pGame->GetGroup()->GetCount() > 0 && nState == 0) {
            if ((m_dwFlags & 0x1) != 0) {
                if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated) {
                    FillOpenPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbPicked));
                    bFilled = TRUE;
                }

                m_pArea->GetInfinity()->OutlinePoly(m_pOpenPolygon, m_nOpenPolygon, m_rOpenBounding, rgbPicked);
            } else {
                if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated) {
                    FillClosedPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbPicked));
                    bFilled = TRUE;
                }

                m_pArea->GetInfinity()->OutlinePoly(m_pClosedPolygon, m_nClosedPolygon, m_rClosedBounding, rgbPicked);
            }
        }

        if (pGame->m_nState == 2) {
            if (pGame->m_iconIndex == 36
                && ((m_trapActivated != 0 && m_trapDetected != 0) || (m_dwFlags & 0x2) != 0)) {
                if ((m_dwFlags & 0x1) != 0) {
                    if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated) {
                        FillOpenPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbAction));
                        bFilled = TRUE;
                    }

                    m_pArea->GetInfinity()->OutlinePoly(m_pOpenPolygon, m_nOpenPolygon, m_rOpenBounding, rgbAction);
                } else {
                    if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated) {
                        FillClosedPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbAction));
                        bFilled = TRUE;
                    }

                    m_pArea->GetInfinity()->OutlinePoly(m_pClosedPolygon, m_nClosedPolygon, m_rClosedBounding, rgbAction);
                }
            }

            if (pGame->m_iconIndex == 12
                && (m_dwFlags & 0x1) == 0
                && (m_dwFlags & 0x2) != 0) {
                if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated) {
                    FillClosedPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbAction));
                    bFilled = TRUE;
                }

                m_pArea->GetInfinity()->OutlinePoly(m_pClosedPolygon, m_nClosedPolygon, m_rClosedBounding, rgbAction);
            }
        }
    }

    COLORREF rgbColor;
    if (m_drawPoly > 0) {
        if ((m_dwFlags & 0x80) != 0 && (m_dwFlags & 0x100) != 0) {
            rgbColor = rgbDrawPolyHidden;
        } else {
            rgbColor = rgbDrawPoly;
        }
    } else {
        if ((m_dwFlags & 0x80) != 0) {
            return;
        }

        if (!m_pArea->m_visibility.IsTileExplored(m_pArea->m_visibility.PointToTile(m_pos))) {
            return;
        }

        if (g_pBaldurChitin->pActiveEngine != g_pBaldurChitin->m_pEngineWorld) {
            return;
        }

        if (g_pBaldurChitin->m_pEngineWorld->GetMenuKey() != TRUE) {
            return;
        }

        rgbColor = rgbDrawPoly;
    }

    if (CInfinity::TRANSLUCENT_BLTS_ON && !g_pChitin->cVideo.m_bIs3dAccelerated && !bFilled) {
        if ((m_dwFlags & 0x1) != 0) {
            if (m_nOpenPolygon > 0) {
                FillOpenPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbColor));
            }
        } else {
            if (m_nClosedPolygon > 0) {
                FillClosedPoly(pArea, pVidMode, nSurface, g_pChitin->GetCurrentVideoMode()->PackColor(rgbColor));
            }
        }
    }

    if ((m_dwFlags & 0x1) != 0) {
        if (m_nOpenPolygon == 0) {
            return;
        }

        m_pArea->GetInfinity()->OutlinePoly(m_pOpenPolygon, m_nOpenPolygon, m_rOpenBounding, rgbColor);
    } else {
        if (m_nClosedPolygon == 0) {
            return;
        }

        m_pArea->GetInfinity()->OutlinePoly(m_pClosedPolygon, m_nClosedPolygon, m_rClosedBounding, rgbColor);
    }
}

// 0x4892B0
void CGameDoor::RemoveFromArea()
{
    m_pArea->m_lTiledObjects.RemoveAt(m_tiledObject.m_posAreaList);

    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 1269
        UTIL_ASSERT(FALSE);
    }

    delete this;
}

// 0x489330
void CGameDoor::SetCursor(LONG nToolTip)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if ((m_dwFlags & 0x80) == 0 || (m_dwFlags & 0x100) != 0) {
        switch (pGame->GetState()) {
        case 0:
            if (pGame->GetGroup()->GetCount() != 0) {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(m_cursorType, FALSE);
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
                if ((m_dwFlags & 0x1) == 0 && (m_dwFlags & 0x2) != 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(12, FALSE);
                } else {
                    CGameObject::SetCursor(nToolTip);
                }
                break;
            case 18:
            case 40:
            case 255:
                CGameObject::SetCursor(nToolTip);
                break;
            case 36:
                if (1) {
                    INT nNewCursor;
                    if (m_trapActivated != 0 && m_trapDetected != 0) {
                        nNewCursor = 38;
                    } else {
                        if ((m_dwFlags & 0x2) == 0) {
                            CGameObject::SetCursor(nToolTip);
                            break;
                        }

                        nNewCursor = 26;
                    }

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
                        if (nNewCursor == 26) {
                            if (pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] == 0) {
                                g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                            }
                        } else {
                            if (pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_DISABLE_DEVICE] == 0) {
                                g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                            }
                        }

                        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                }
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
                // __LINE__: 1370
                UTIL_ASSERT(FALSE);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
            // __LINE__: 1380
            UTIL_ASSERT(FALSE);
        }
    } else {
        CGameObject::SetCursor(nToolTip);
    }
}

// 0x48AC80
void CGameDoor::OnDoorStatusUpdate(BOOLEAN bDoorOpened, DWORD dwFlags, WORD nTrapActivated, WORD nTrapDetected)
{
    if ((m_dwFlags & 0x1) == 0 || bDoorOpened) {
        if ((m_dwFlags & 0x1) == 0 && bDoorOpened == TRUE) {
            m_dwFlags |= 0x1;
            m_pos = m_ptClosedDest;
            m_tiledObject.m_wAIState = CTiledObject::STATE_PRIMARY_TILE;

            if (m_pClosedSearch != NULL) {
                m_pArea->m_search.RemoveDoor(m_pClosedSearch, m_nClosedSearch);
            }

            if (m_pOpenSearch != NULL) {
                m_pArea->m_search.AddDoor(m_pOpenSearch,
                    m_nOpenSearch,
                    (m_dwFlags & 0x400) != 0);
            }
        }
    } else {
        m_dwFlags &= ~0x1;
        m_pos = m_ptOpenDest;
        m_tiledObject.m_wAIState = CTiledObject::STATE_SECONDARY_TILE;

        if (m_pOpenSearch != NULL) {
            m_pArea->m_search.RemoveDoor(m_pOpenSearch, m_nOpenSearch);
        }

        if (m_pClosedSearch != NULL) {
            m_pArea->m_search.AddDoor(m_pClosedSearch,
                m_nClosedSearch,
                (m_dwFlags & 0x400) != 0);
        }
    }

    m_dwFlags = dwFlags;
    m_trapDetected = nTrapDetected;
    m_trapActivated = nTrapActivated;
}

// 0x48ADC0
void CGameDoor::Marshal(CAreaFileDoorObject** pDoorObject)
{
    CAreaPoint* pPoints;
    DWORD cnt;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 1904
    UTIL_ASSERT(pDoorObject != NULL);

    *pDoorObject = new CAreaFileDoorObject();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 1910
    UTIL_ASSERT(*pDoorObject != NULL);

    // FIXME: Redundant, memset is a part of constructor.
    memset(*pDoorObject, 0, sizeof(CAreaFileDoorObject));

    if (m_nClosedSearch > 0) {
        pPoints = new CAreaPoint[m_nClosedSearch];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 1915
        UTIL_ASSERT(pPoints != NULL);

        for (cnt = 0; cnt < m_nClosedSearch; cnt++) {
            pPoints[cnt].m_xPos = static_cast<WORD>(m_pClosedSearch[cnt].x);
            pPoints[cnt].m_yPos = static_cast<WORD>(m_pClosedSearch[cnt].y);
        }

        // The game-file writers resolve the handle (compat/marshal_ptr.h).
        (*pDoorObject)->m_closedSearchSquaresStart = iwd2r::MarshalPtr(pPoints);
        (*pDoorObject)->m_closedSearchSquaresCount = m_nClosedSearch;
    }

    if (m_nOpenSearch > 0) {
        pPoints = new CAreaPoint[m_nOpenSearch];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 1927
        UTIL_ASSERT(pPoints != NULL);

        for (cnt = 0; cnt < m_nOpenSearch; cnt++) {
            pPoints[cnt].m_xPos = static_cast<WORD>(m_pOpenSearch[cnt].x);
            pPoints[cnt].m_yPos = static_cast<WORD>(m_pOpenSearch[cnt].y);
        }

        // The game-file writers resolve the handle (compat/marshal_ptr.h).
        (*pDoorObject)->m_openSearchSquaresStart = iwd2r::MarshalPtr(pPoints);
        (*pDoorObject)->m_openSearchSquaresCount = m_nOpenSearch;
    }

    if (m_nClosedPolygon > 0) {
        pPoints = new CAreaPoint[m_nClosedPolygon];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 1939
        UTIL_ASSERT(pPoints != NULL);

        for (cnt = 0; cnt < m_nClosedPolygon; cnt++) {
            pPoints[cnt].m_xPos = static_cast<WORD>(m_pClosedPolygon[cnt].x);
            pPoints[cnt].m_yPos = static_cast<WORD>(m_pClosedPolygon[cnt].y);
        }

        // The game-file writers resolve the handle (compat/marshal_ptr.h).
        (*pDoorObject)->m_closedSelectionPointStart = iwd2r::MarshalPtr(pPoints);
        (*pDoorObject)->m_closedSelectionPointCount = m_nClosedPolygon;
    }

    if (m_nOpenPolygon > 0) {
        pPoints = new CAreaPoint[m_nOpenPolygon];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 1951
        UTIL_ASSERT(pPoints != NULL);

        for (cnt = 0; cnt < m_nOpenPolygon; cnt++) {
            pPoints[cnt].m_xPos = static_cast<WORD>(m_pOpenPolygon[cnt].x);
            pPoints[cnt].m_yPos = static_cast<WORD>(m_pOpenPolygon[cnt].y);
        }

        // The game-file writers resolve the handle (compat/marshal_ptr.h).
        (*pDoorObject)->m_openSelectionPointStart = iwd2r::MarshalPtr(pPoints);
        (*pDoorObject)->m_openSelectionPointCount = m_nOpenPolygon;
    }

    m_resID.GetResRef((*pDoorObject)->m_doorID);
    (*pDoorObject)->m_closedBoundingRectLeft = static_cast<WORD>(m_rClosedBounding.left);
    (*pDoorObject)->m_closedBoundingRectTop = static_cast<WORD>(m_rClosedBounding.top);
    (*pDoorObject)->m_closedBoundingRectRight = static_cast<WORD>(m_rClosedBounding.right) - 1;
    (*pDoorObject)->m_closedBoundingRectBottom = static_cast<WORD>(m_rClosedBounding.bottom) - 1;
    (*pDoorObject)->m_openBoundingRectLeft = static_cast<WORD>(m_rOpenBounding.left);
    (*pDoorObject)->m_openBoundingRectTop = static_cast<WORD>(m_rOpenBounding.top);
    (*pDoorObject)->m_openBoundingRectRight = static_cast<WORD>(m_rOpenBounding.right) - 1;
    (*pDoorObject)->m_openBoundingRectBottom = static_cast<WORD>(m_rOpenBounding.bottom) - 1;
    (*pDoorObject)->m_cursorType = m_cursorType;
    (*pDoorObject)->m_dwFlags = m_dwFlags;
    (*pDoorObject)->m_posXWalkTo1 = static_cast<WORD>(m_ptDest1.x);
    (*pDoorObject)->m_posYWalkTo1 = static_cast<WORD>(m_ptDest1.y);
    (*pDoorObject)->m_posXWalkTo2 = static_cast<WORD>(m_ptDest2.x);
    (*pDoorObject)->m_posYWalkTo2 = static_cast<WORD>(m_ptDest2.y);
    memcpy((*pDoorObject)->m_script, m_scriptRes, RESREF_SIZE);
    strncpy((*pDoorObject)->m_scriptName, m_scriptName, SCRIPTNAME_SIZE);
    (*pDoorObject)->m_hitPoints = m_hitPoints;
    (*pDoorObject)->m_armourClass = m_armourClass;
    m_openSound.GetResRef((*pDoorObject)->m_openSound);
    m_closeSound.GetResRef((*pDoorObject)->m_closeSound);
    (*pDoorObject)->m_trapDetectionDifficulty = m_trapDetectionDifficulty;
    (*pDoorObject)->m_trapDisarmingDifficulty = m_trapDisarmingDifficulty;
    (*pDoorObject)->m_trapActivated = m_trapActivated;
    (*pDoorObject)->m_trapDetected = m_trapDetected;
    (*pDoorObject)->m_posXTrapOrigin = m_posXTrapOrigin;
    (*pDoorObject)->m_posYTrapOrigin = m_posYTrapOrigin;
    m_keyType.GetResRef((*pDoorObject)->m_keyType);
    (*pDoorObject)->m_detectionDifficulty = m_detectionDifficulty;
    (*pDoorObject)->m_lockDifficulty = m_lockDifficulty;
    (*pDoorObject)->m_strNotPickable = m_strNotPickable;
}

// 0x487530
BOOLEAN CGameDoor::CompressTime(DWORD deltaTime)
{
    if (static_cast<DWORD>(m_drawPoly) < deltaTime) {
        m_drawPoly = 0;
    } else {
        m_drawPoly = static_cast<SHORT>(m_drawPoly - deltaTime);
    }
    if (deltaTime > static_cast<DWORD>(CTimerWorld::TIMESCALE_MSEC_PER_SEC
            * CTimerWorld::TIMESCALE_SEC_PER_MIN * CTimerWorld::TIMESCALE_MIN_PER_HOUR)) {
        if ((m_dwFlags & 1) != 0 && (m_dwFlags & 0x20) == 0
            && m_pArea->m_search.CanToggleDoor(m_pClosedSearch, m_nClosedSearch)
            && rand() % 5 != 0) {
            m_dwFlags &= ~1;
            m_pos = m_ptOpenDest;
            m_tiledObject.m_wAIState = CTiledObject::STATE_SECONDARY_TILE;
            if (m_pOpenSearch != NULL) {
                m_pArea->m_search.RemoveDoor(m_pOpenSearch, m_nOpenSearch);
            }
            if (m_pClosedSearch != NULL) {
                m_pArea->m_search.AddDoor(m_pClosedSearch, m_nClosedSearch, (m_dwFlags & 0x400) != 0);
            }
            if (g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE) {
                g_pBaldurChitin->GetMessageHandler()->AddMessage(new CMessageDoorStatus(this, m_id, m_id), FALSE);
            }
        }
        m_dwFlags &= ~0x1000;
    }
    return TRUE;
}

// 0x48B350
void CGameDoor::SetDrawPoly(SHORT time)
{
    if (m_drawPoly != time) {
        if (!g_pBaldurChitin->GetBaldurMessage()->m_bInMessageSetDrawPoly) {
            CMessageSetDrawPoly* pMessage = new CMessageSetDrawPoly(time, GetId(), GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }
    }
    m_drawPoly = time;
}

// The door's closed polygon filled with dwColor (in the surface's
// format) through the FX surface, piece by piece: each of the rectangles
// that cover it (field_6DA) that is on screen.
//
// 0x48B3C0
void CGameDoor::FillClosedPoly(CGameArea* pArea, CVidMode* pVidMode, INT nSurface, DWORD dwColor)
{
    CInfinity* pInfinity = pArea->GetInfinity();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 2155
    UTIL_ASSERT(pInfinity != NULL && pVidMode != NULL);

    CRect rViewPort;
    rViewPort.left = m_pArea->GetInfinity()->nCurrentX;
    rViewPort.top = m_pArea->GetInfinity()->nCurrentY;
    rViewPort.right = m_pArea->GetInfinity()->rViewPort.Width() + rViewPort.left;
    rViewPort.bottom = m_pArea->GetInfinity()->rViewPort.Height() + rViewPort.top;

    INT nRects = field_6DA.GetSize();
    CPoint ptReference(0, 0);

    CVidPoly cPoly;
    cPoly.SetPoly(reinterpret_cast<WORD*>(field_6EE), m_nClosedPolygon);

    for (INT nRect = 0; nRect < nRects; nRect++) {
        CRect* pRect = field_6DA[nRect];
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

// The door's open polygon filled with dwColor (in the surface's
// format) through the FX surface, piece by piece: each of the rectangles
// that cover it (field_6F2) that is on screen.
//
// 0x48B600
void CGameDoor::FillOpenPoly(CGameArea* pArea, CVidMode* pVidMode, INT nSurface, DWORD dwColor)
{
    CInfinity* pInfinity = pArea->GetInfinity();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
    // __LINE__: 2204
    UTIL_ASSERT(pInfinity != NULL && pVidMode != NULL);

    CRect rViewPort;
    rViewPort.left = m_pArea->GetInfinity()->nCurrentX;
    rViewPort.top = m_pArea->GetInfinity()->nCurrentY;
    rViewPort.right = m_pArea->GetInfinity()->rViewPort.Width() + rViewPort.left;
    rViewPort.bottom = m_pArea->GetInfinity()->rViewPort.Height() + rViewPort.top;

    INT nRects = field_6F2.GetSize();
    CPoint ptReference(0, 0);

    CVidPoly cPoly;
    cPoly.SetPoly(reinterpret_cast<WORD*>(field_706), m_nOpenPolygon);

    for (INT nRect = 0; nRect < nRects; nRect++) {
        CRect* pRect = field_6F2[nRect];
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

// 0x453140
DWORD CGameDoor::GetFlags()
{
    return m_dwFlags;
}

// 0x453150
void CGameDoor::SetFlags(DWORD dwFlags)
{
    m_dwFlags = dwFlags;
}

// 0x45B700
BOOL CGameDoor::sub_45B700()
{
    return m_dwFlags & 0x1;
}

// NOTE: Inlined in `ToggleDoor`: the strref a locked door reports, its own
// text when it has one (flag 0x200), else 15874 (0x3E02).
static STRREF ToggleDoorLockedText(CGameDoor* pDoor)
{
    if ((pDoor->m_dwFlags & 0x200) != 0 && static_cast<LONG>(pDoor->m_strNotPickable) > -1) {
        return pDoor->m_strNotPickable;
    }

    return 15874;
}

// NOTE: Inlined in `ToggleDoor`: the priority of the door's sound from the
// listener's distance.
static BYTE ToggleDoorSoundPriority(const CPoint& ptListener, const CPoint& pos)
{
    LONG dx = ptListener.x - pos.x;
    LONG dy = ptListener.y - pos.y;
    LONG nPriority = 99 - ((dy * dy / 144 + dx * dx / 256) * 99) / 6400;
    if (nPriority < 0) {
        nPriority = 0;
    }
    return static_cast<BYTE>(nPriority);
}

// 0x489680
void CGameDoor::ToggleDoor(const CAIObjectType& cause, BOOL bForce)
{
    BOOLEAN bCanToggle = TRUE;

    // NOTE: Unused.
    CString sUnused;
    CSound soundUnused;

    if (!bForce && (m_dwFlags & 0x2) != 0) {
        // Locked.
        if (m_keyType == "") {
            CMessage* message = new CMessageDisplayTextRefSend(-1,
                ToggleDoorLockedText(this),
                0,
                RGB(215, 215, 190),
                -1,
                m_id,
                cause.m_nInstance);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            return;
        }

        if (!PartyHasItem(m_keyType)) {
            CGameObject* pObject;
            BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(cause.m_nInstance,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
            if (rc != CGameObjectArray::SUCCESS) {
                CMessage* message = new CMessageDisplayTextRefSend(-1,
                    ToggleDoorLockedText(this),
                    0,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    cause.m_nInstance);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                return;
            }

            if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
                CMessage* message = new CMessageDisplayTextRefSend(-1,
                    ToggleDoorLockedText(this),
                    0,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    cause.m_nInstance);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(cause.m_nInstance,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return;
            }

            CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
            CString sKey;
            m_keyType.CopyToString(sKey);
            if (pSprite->FindItemPersonal(sKey, 0, FALSE) == -1
                && pSprite->FindItemBags(sKey, 0, FALSE) == -1) {
                CMessage* message = new CMessageDisplayTextRefSend(-1,
                    ToggleDoorLockedText(this),
                    0,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    cause.m_nInstance);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(cause.m_nInstance,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return;
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(cause.m_nInstance,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if ((m_dwFlags & 0x1) != 0) {
        // Open: close it.
        if (_strnicmp(reinterpret_cast<const char*>(m_pArea->m_header.m_areaName), "ar5004", 7) == 0) {
            if (!m_pArea->m_search.CanToggleDoor(m_pClosedSearch, m_nClosedSearch)) {
                // The creature in the door's way: the door gets the action
                // 292 (the SHORT at 0x8479A4) on it, and the one closing the
                // door gets CloseDoor (144, 0x847894) on this door again
                // (0x4899A0-0x489DE1).
                CAIAction action;
                action.m_actionID = CAIAction::NO_ACTION;
                action.m_specificID = 0;
                action.m_specificID2 = 0;
                action.m_specificID3 = 0;
                action.m_string1 = "";
                action.m_string2 = "";
                action.m_internalFlags = 0;

                CGameSprite* pBlocker = m_pArea->sub_47A1C0(m_id, m_pClosedSearch, m_nClosedSearch);
                if (pBlocker != NULL) {
                    action.m_actionID = 292;
                    action.m_acteeID.Set(pBlocker->GetAIType());
                    CMessage* message = new CMessageAddAction(action, m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pBlocker->m_id,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    action.m_actionID = 144;
                    action.m_acteeID.Set(m_typeAI);
                    message = new CMessageAddAction(action, m_id, cause.m_nInstance);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                return;
            }
        } else {
            bCanToggle = m_pArea->m_search.CanToggleDoor(m_pClosedSearch, m_nClosedSearch);
            if (!bCanToggle) {
                return;
            }
        }

        if (!m_sound.IsSoundPlaying()) {
            if (m_closeSound != "") {
                m_sound.SetResRef(m_closeSound, TRUE, TRUE);
            } else if ((m_dwFlags & 0x80) != 0) {
                m_sound.SetResRef(CResRef("AMB_D04B"), TRUE, TRUE);
            } else {
                m_sound.SetResRef(CResRef("AMB_D03B"), TRUE, TRUE);
            }

            m_sound.SetChannel(2, reinterpret_cast<DWORD_PTR>(m_pArea));

            CPoint ptListener;
            LONG nListenerZ;
            g_pBaldurChitin->cSoundMixer.GetListenPosition(ptListener, nListenerZ);
            m_sound.SetPriority(ToggleDoorSoundPriority(ptListener, m_pos));
            m_sound.Play(m_pos.x, m_pos.y, m_posZ, FALSE);

            // NOTE: The sound is sent to the other players here (vtable
            // 0x84918C, BROADCAST_FORCED_OTHERS). Omitted: single player only
            // (docs/adr/0002-single-player-only.md).
        }

        m_dwFlags &= ~0x1;
        m_pos = m_ptOpenDest;
        m_tiledObject.m_wAIState = CTiledObject::STATE_SECONDARY_TILE;

        if (m_pOpenSearch != NULL) {
            m_pArea->m_search.RemoveDoor(m_pOpenSearch, m_nOpenSearch);
        }

        if (m_pClosedSearch != NULL) {
            m_pArea->m_search.AddDoor(m_pClosedSearch, m_nClosedSearch, (m_dwFlags >> 10) & 1);
        }

        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == m_remotePlayerID) {
            CMessage* message = new CMessageDoorStatus(this, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        if (m_trapActivated != 0) {
            CAITrigger trigger(CAITRIGGER_CLOSED, cause, 0);
            m_pendingTriggers.AddTail(new CAITrigger(trigger));

            if ((m_dwFlags & 0x4) == 0) {
                m_trapActivated = 0;
                CMessage* message = new CMessageDoorStatus(this, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
    } else {
        // Closed: open it.
        if ((m_dwFlags & 0x1800) == 0x800) {
            STRREF strNotPickable = m_strNotPickable;
            if (static_cast<LONG>(strNotPickable) > -1) {
                if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
                    FloatText(strNotPickable, 10, 5);
                } else {
                    // NOTE: A connected game sends the floating text
                    // (CMessageFloatText). Omitted: single player only
                    // (docs/adr/0002-single-player-only.md).
                }

                CMessage* message = new CMessageDisplayTextRefSend(-1,
                    strNotPickable,
                    0,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    cause.m_nInstance);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            m_dwFlags |= 0x1000;
            return;
        }

        if (!m_sound.IsSoundPlaying()) {
            if (m_openSound != "") {
                m_sound.SetResRef(m_openSound, TRUE, TRUE);
            } else if ((m_dwFlags & 0x80) != 0) {
                m_sound.SetResRef(CResRef("AMB_D04A"), TRUE, TRUE);
            } else {
                m_sound.SetResRef(CResRef("AMB_D03A"), TRUE, TRUE);
            }

            m_sound.SetChannel(2, reinterpret_cast<DWORD_PTR>(m_pArea));

            CPoint ptListener;
            LONG nListenerZ;
            g_pBaldurChitin->cSoundMixer.GetListenPosition(ptListener, nListenerZ);
            m_sound.SetPriority(ToggleDoorSoundPriority(ptListener, m_pos));
            m_sound.Play(m_pos.x, m_pos.y, m_posZ, FALSE);

            // NOTE: As above, the message to the other players is omitted.
        }

        m_dwFlags |= 0x1;
        m_pos = m_ptClosedDest;
        m_tiledObject.m_wAIState = CTiledObject::STATE_PRIMARY_TILE;

        if (m_pClosedSearch != NULL) {
            m_pArea->m_search.RemoveDoor(m_pClosedSearch, m_nClosedSearch);
        }

        if (m_pOpenSearch != NULL) {
            m_pArea->m_search.AddDoor(m_pOpenSearch, m_nOpenSearch, (m_dwFlags >> 10) & 1);
        }

        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == m_remotePlayerID) {
            CMessage* message = new CMessageDoorStatus(this, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        if (m_trapActivated != 0) {
            CAITrigger trigger(CAITrigger::OPENED, cause, 0);
            m_pendingTriggers.AddTail(new CAITrigger(trigger));

            if ((m_dwFlags & 0x4) == 0) {
                m_trapActivated = 0;
                CMessage* message = new CMessageDoorStatus(this, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
    }

    if (bCanToggle && (m_dwFlags & 0x2) != 0) {
        // Toggled while locked: unlocked, and the key taken when the door
        // says so (flag 0x4000).
        m_dwFlags &= ~0x2;
        if ((m_dwFlags & 0x4000) != 0
            && m_keyType != ""
            && PartyHasItem(m_keyType)) {
            // NOTE: The SHORT at 0x84785C (116, TakePartyItem).
            m_curAction.m_actionID = 116;
            m_curAction.SetString1(m_keyType.GetResRefStr());
            // TODO: 0x48A48B calls TakePartyItem (0x463B30, not ported).
        }
    }
}

// 0x48B2C0
const CPoint& CGameDoor::GetCloserDestPt(const CPoint& pt)
{
    LONG dy2 = pt.y - m_ptDest2.y;
    LONG dx2 = pt.x - m_ptDest2.x;
    LONG dy1 = pt.y - m_ptDest1.y;
    LONG dx1 = pt.x - m_ptDest1.x;

    if (dy2 * dy2 * 16 / 9 + dx2 * dx2 <= dy1 * dy1 * 16 / 9 + dx1 * dx1) {
        return m_ptDest2;
    }

    return m_ptDest1;
}

// 0x487D10
void CGameDoor::OnActionButton(const CPoint& pt)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CAIGroup* pGroup = &pGame->m_group;

    if ((m_dwFlags & 0x80) != 0 && (m_dwFlags & 0x100) == 0) {
        pGame->m_lastClick.x = -1;
        pGame->m_lastClick.y = -1;
        pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;
        CGameObject::OnActionButton(pt);
        return;
    }

    switch (pGame->m_nState) {
    case 0:
        break;
    case 2:
        pGame->m_lastClick.x = -1;
        pGame->m_lastClick.y = -1;
        pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;
        // TODO: 0x487E10-0x488493, the special cursors on a door (spells,
        // thieving, bashing; the switch on CInfGame::m_iconIndex at
        // 0x488A1C/0x488A30) are not ported; the door does nothing.
        return;
    case 3:
        pGame->m_lastClick.x = -1;
        pGame->m_lastClick.y = -1;
        pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;
        CGameObject::OnActionButton(pt);
        return;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 989
        UTIL_ASSERT(FALSE);
        return;
    }

    pGame->m_lastClick.x = -1;
    pGame->m_lastClick.y = -1;
    pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;

    if (pGroup->GetCount() == 0) {
        return;
    }

    // UseDoor (142) on this door, for the whole group.
    CAIAction useDoor;
    useDoor.m_actionID = 142;
    useDoor.m_specificID = m_id;
    useDoor.m_specificID2 = 0;
    useDoor.m_specificID3 = 0;
    useDoor.m_internalFlags = 0;
    pGroup->GroupAction(useDoor, TRUE, NULL);

    LONG* pGroupList = pGroup->GetGroupList();
    LONG nRange = RANGE_DOOR;

    if (pGroupList == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
        // __LINE__: 857
        UTIL_ASSERT(FALSE);
        return;
    }

    CPoint ptDest;
    SHORT nIndex;
    for (nIndex = 0; nIndex < pGroup->GetCount(); nIndex++) {
        CGameObject* pObject;
        if (pGame->GetObjectArray()->GetShare(pGroupList[nIndex],
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDoor.cpp
            // __LINE__: 865
            UTIL_ASSERT(FALSE);
            continue;
        }

        const CPoint& pos = pObject->GetPos();

        // NOTE: Inlined `GetCloserDestPt`.
        LONG dy1 = pos.y - m_ptDest1.y;
        LONG dx1 = pos.x - m_ptDest1.x;
        LONG dy2 = pos.y - m_ptDest2.y;
        LONG dx2 = pos.x - m_ptDest2.x;
        const CPoint* pDest = dy1 * dy1 * 16 / 9 + dx1 * dx1 < dy2 * dy2 * 16 / 9 + dx2 * dx2
            ? &m_ptDest1
            : &m_ptDest2;
        ptDest = *pDest;

        if (_strnicmp(reinterpret_cast<const char*>(m_pArea->m_header.m_areaName), "ar5004", 7) == 0) {
            nRange = RANGE_DOOR / 10;
        } else if (_strnicmp(reinterpret_cast<const char*>(m_pArea->m_header.m_areaName), "ar5202", 7) == 0) {
            nRange = RANGE_DOOR / 5;
        }

        LONG dy = pos.y - ptDest.y;
        LONG dx = pos.x - ptDest.x;
        if (dy * dy * 16 / 9 + dx * dx <= nRange) {
            pGame->GetObjectArray()->ReleaseShare(pGroupList[nIndex],
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            break;
        }

        pGame->GetObjectArray()->ReleaseShare(pGroupList[nIndex],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (nIndex == pGroup->GetCount()) {
        // Nobody is next to the door: the group walks to it first.
        CAIAction moveToPoint;
        moveToPoint.m_actionID = CAIAction::MOVETOPOINT;
        moveToPoint.m_dest = ptDest;
        moveToPoint.m_specificID = 0;
        moveToPoint.m_specificID2 = -1;
        moveToPoint.m_specificID3 = 0;
        moveToPoint.m_internalFlags = 0;
        pGroup->GroupAction(moveToPoint, FALSE, NULL);
    }

    delete pGroupList;
}
