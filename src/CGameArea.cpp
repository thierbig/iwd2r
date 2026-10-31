#include "CGameArea.h"

#include "CAIScript.h"
#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CCreatureFile.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameEffect.h"
#include "CGameObject.h"
#include "CGameSound.h"
#include "CGameSpawning.h"
#include "CGameSprite.h"
#include "CGameStatic.h"
#include "CGameTiledObject.h"
#include "CGameTrigger.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CObjectMarker.h"
#include "CPathSearch.h"
#include "CProjectile.h"
#include "CResWED.h"
#include "CScreenLoad.h"
#include "CScreenStore.h"
#include "CScreenWorld.h"
#include "CSpawn.h"
#include "CTiledObject.h"
#include "CUtil.h"
#include "CVidPoly.h"
#include "compat/marshal_ptr.h"
#include "platform/replay.h"

// 0x8D212C
INT CGameArea::dword_8D212C;

// 0x8D2138
BOOLEAN CGameArea::byte_8D2138;

// 0x469B60
CGameArea::CGameArea(BYTE id)
{
    field_B0E = 0;
    m_bPicked = FALSE;
    m_pObjectMarker = NULL;
    m_firstRender = 0;
    m_dwLastProgressRenderTickCount = 0;
    m_dwLastProgressMsgTickCount = 0;
    m_sndAmbientDayVolume = 0;
    m_sndAmbientNightVolume = 0;
    field_3EC = 0;
    m_nAIIndex = 0;
    m_nCurrentSong = 0;
    m_id = id;
    m_nCharacters = 0;
    m_nInfravision = 0;
    m_pResWED = NULL;
    m_bAreaLoaded = 0;
    InitializeCriticalSection(&field_1FC);
    InitializeCriticalSection(&field_214);
    m_nListManipulationThreadCounter = 0;
    m_ListManipulationThreadId = 0;

    // NOTE: Inlining.
    if (m_nListManipulationThreadCounter > 0) {
        m_nListManipulationThreadCounter = m_nListManipulationThreadCounter - 1;
    }

    if (m_nListManipulationThreadCounter == 0) {
        m_ListManipulationThreadId = 0;
    }

    m_pGame = NULL;
    m_nScrollState = 0;
    m_nKeyScrollState = 0;
    m_bTravelSquare = FALSE;
    m_iPicked = CGameObjectArray::INVALID_INDEX;
    m_iPickedTarget = CGameObjectArray::INVALID_INDEX;
    m_nToolTip = 0;
    m_visibility.m_pDynamicHeight = NULL;
    memset(&m_header, 0, sizeof(m_header));
    m_groupMove = FALSE;
    m_nRandomMonster = 0;
    m_nVisibleMonster = 0;
    m_pbmLumNight = NULL;
    m_bInPathSearch = FALSE;
    m_nInitialAreaID = 0;
    m_nBattleSongCounter = 0;
    m_selectSquare.left = -1;
    m_selectSquare.top = -1;
    m_selectSquare.right = -1;
    m_selectSquare.bottom = -1;
    m_ptOldViewPos.x = -1;
    m_ptOldViewPos.y = -1;
    m_moveDest.x = -1;
    m_moveDest.y = -1;
    m_nFirstObject = -1;
    field_241 = 0;
    m_iPickedOnDown = CGameObjectArray::INVALID_INDEX;
    m_sndAmbientVolume = 100;
    m_cInfinity.m_pArea = this;
    m_nCurrentSong = -1;
    field_438 = 0;
    field_436 = 0;
    m_startedMusic = FALSE;
    field_AE6 = 1;
    m_startedMusicCounter = 10;

    // NOTE: This assignment is slightly incorrect. Original code refers to
    // other offsets (0x8A8168 and 0x8A8154 respectively). It can mean two
    // things:
    // - There was second set of terrain table defaults, which is exactly the
    // same as in `CGameObject`.
    // - Such split was a result of some compiler optimization.
    //
    // Since both sets have the same values, there is no harm to reuse what's
    // already present.
    memcpy(&m_terrainTable, CGameObject::DEFAULT_TERRAIN_TABLE, 16);
    memcpy(&m_visibleTerrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, 16);

    mpSpawner = NULL;

    dword_8D212C = 0;
    byte_8D2138 = 0;

    field_B16 = 0;
}

// 0x46A070
CGameArea::~CGameArea()
{
    EnterCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));

    if (mpSpawner != NULL) {
        delete mpSpawner;
        mpSpawner = NULL;
    }

    if (m_bAreaLoaded) {
        ClearMarshal();
    }

    DeleteCriticalSection(&field_1FC);

    if (m_nListManipulationThreadCounter > 0) {
        m_nListManipulationThreadCounter--;
    }

    if (m_nListManipulationThreadCounter == 0) {
        m_ListManipulationThreadId = 0;
    }

    DeleteCriticalSection(&field_214);

    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
}

// 0x46A2E0
INT CGameArea::SetListManipulationThreadId(DWORD ThreadID)
{
    if (ThreadID != 0) {
        if (m_nListManipulationThreadCounter > 0) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 295
            UTIL_ASSERT(m_ListManipulationThreadId == ThreadID);
        } else {
            m_ListManipulationThreadId = ThreadID;
        }

        m_nListManipulationThreadCounter++;
    } else {
        if (m_nListManipulationThreadCounter > 0) {
            m_nListManipulationThreadCounter--;
        }

        if (m_nListManipulationThreadCounter == 0) {
            m_ListManipulationThreadId = 0;
        }
    }

    return m_nListManipulationThreadCounter;
}

// 0x46A360
void CGameArea::AddObject(LONG id, BYTE listType)
{
    CTypedPtrList<CPtrList, int*>* pList;
    switch (listType) {
    case CGAMEOBJECT_LIST_FRONT:
        pList = &m_lVertSortAdd;
        break;
    case CGAMEOBJECT_LIST_BACK:
        pList = &m_lVertSortBackAdd;
        break;
    case CGAMEOBJECT_LIST_FLIGHT:
        pList = &m_lVertSortFlightAdd;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 358
        UTIL_ASSERT(FALSE);
    }

    pList->AddTail(reinterpret_cast<int*>(id));
}

// 0x46A3D0
BOOL CGameArea::AdjustTarget(CPoint start, POINT* goal, BYTE personalSpace, SHORT tolerance)
{
    if (start.x == goal->x && start.y == goal->y) {
        return FALSE;
    }

    // Walks from the goal toward the start, one search square along the
    // longer axis per step, to the first square that is explored and
    // passable.
    CPoint pt(goal->x, goal->y);
    LONG dy = start.y - pt.y;
    LONG dx = start.x - pt.x;
    LONG stepX;
    LONG stepY;

    if (abs(dy) < abs(dx)) {
        stepX = (pt.x < start.x ? 1 : -1) * (1 << CGameSprite::EXACT_SCALE);
        stepY = dy * (1 << CGameSprite::EXACT_SCALE) / abs(dx);
    } else {
        stepX = dx * (1 << CGameSprite::EXACT_SCALE) / abs(dy);
        stepY = (pt.y < start.y ? 1 : -1) * (1 << CGameSprite::EXACT_SCALE);
    }

    LONG exactX = pt.x * (1 << CGameSprite::EXACT_SCALE) + (1 << (CGameSprite::EXACT_SCALE - 1));
    LONG exactY = pt.y * (1 << CGameSprite::EXACT_SCALE) + (1 << (CGameSprite::EXACT_SCALE - 1));

    while (1) {
        if (pt.x == start.x && pt.y == start.y) {
            return FALSE;
        }

        if (tolerance-- <= 0) {
            return FALSE;
        }

        SHORT nTile = static_cast<SHORT>(static_cast<SHORT>((pt.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2) / CVisibilityMap::SQUARE_SIZEY) * m_visibility.m_nWidth
            + (pt.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2) / CVisibilityMap::SQUARE_SIZEX);
        if (m_visibility.IsTileExplored(nTile)) {
            SHORT nTableIndex;
            if (m_search.GetCost(pt, CGameObject::DEFAULT_TERRAIN_TABLE, personalSpace, nTableIndex, TRUE) != CPathSearch::COST_IMPASSABLE) {
                break;
            }
        }

        exactX += stepX;
        pt.x = exactX >> CGameSprite::EXACT_SCALE;
        exactY += stepY;
        pt.y = exactY >> CGameSprite::EXACT_SCALE;
    }

    if (pt.x == start.x && pt.y == start.y) {
        return FALSE;
    }

    goal->x = pt.x;
    goal->y = pt.y;

    return TRUE;
}

// 0x46A630
BOOL CGameArea::SnapshotAdjustTarget(CPoint start, POINT* goal, BOOL bBump, SHORT tolerance)
{
    if (start.x == goal->x && start.y == goal->y) {
        return FALSE;
    }

    // AdjustTarget on the search thread's snapshot: no explored test, and
    // the cost from the snapshot of the dynamic costs.
    CPoint pt(goal->x, goal->y);
    LONG dy = start.y - pt.y;
    LONG dx = start.x - pt.x;
    LONG stepX;
    LONG stepY;

    if (abs(dy) < abs(dx)) {
        stepX = (pt.x < start.x ? 1 : -1) * (1 << CGameSprite::EXACT_SCALE);
        stepY = dy * (1 << CGameSprite::EXACT_SCALE) / abs(dx);
    } else {
        stepX = dx * (1 << CGameSprite::EXACT_SCALE) / abs(dy);
        stepY = (pt.y < start.y ? 1 : -1) * (1 << CGameSprite::EXACT_SCALE);
    }

    LONG exactX = pt.x * (1 << CGameSprite::EXACT_SCALE) + (1 << (CGameSprite::EXACT_SCALE - 1));
    LONG exactY = pt.y * (1 << CGameSprite::EXACT_SCALE) + (1 << (CGameSprite::EXACT_SCALE - 1));

    while (1) {
        if (pt.x == start.x && pt.y == start.y) {
            return FALSE;
        }

        if (tolerance-- <= 0) {
            return FALSE;
        }

        if (m_search.SnapshotGetCost(pt, bBump) != CPathSearch::COST_IMPASSABLE) {
            break;
        }

        exactY += stepY;
        exactX += stepX;
        pt.x = exactX >> CGameSprite::EXACT_SCALE;
        pt.y = exactY >> CGameSprite::EXACT_SCALE;
    }

    if (pt.x == start.x && pt.y == start.y) {
        return FALSE;
    }

    goal->x = pt.x;
    goal->y = pt.y;

    return TRUE;
}

// 0x46A7B0
void CGameArea::ApplyWindToAmbients(BYTE nPercentVolume)
{
    m_sndAmbientVolume = nPercentVolume;
    m_sndAmbientDay.SetVolume(m_sndAmbientDayVolume * m_sndAmbientVolume / 100);
    m_sndAmbientNight.SetVolume(m_sndAmbientNightVolume * m_sndAmbientVolume / 100);
}

// 0x46A820
BOOL CGameArea::CheckLOS(const CPoint& start, const CPoint& goal, const BYTE* terrainTable, BOOLEAN bCheckIfExplored)
{
    SHORT nTableIndex;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 539
    UTIL_ASSERT(terrainTable != NULL);

    if (start == goal) {
        return TRUE;
    }

    if ((start.x - goal.x) * (start.x - goal.x) + 16 * (start.y - goal.y) * (start.y - goal.y) / 9 > m_visibility.field_E * m_visibility.field_E * CVisibilityMap::SQUARE_SIZEX * CVisibilityMap::SQUARE_SIZEX) {
        return FALSE;
    }

    CPoint startSearch(start.x / CPathSearch::GRID_SQUARE_SIZEX,
        start.y / CPathSearch::GRID_SQUARE_SIZEY);

    CPoint goalSearch(goal.x / CPathSearch::GRID_SQUARE_SIZEX,
        goal.y / CPathSearch::GRID_SQUARE_SIZEY);

    if (startSearch.x < 0
        || startSearch.y < 0
        || startSearch.x >= m_search.m_GridSquareDimensions.cx
        || startSearch.y >= m_search.m_GridSquareDimensions.cy) {
        return FALSE;
    }

    if (goalSearch.x < 0
        || goalSearch.y < 0
        || goalSearch.x >= m_search.m_GridSquareDimensions.cx
        || goalSearch.y >= m_search.m_GridSquareDimensions.cy) {
        return FALSE;
    }

    if (startSearch == goalSearch) {
        return TRUE;
    }

    if (bCheckIfExplored) {
        if (!m_visibility.IsTileExplored(m_visibility.PointToTile(start))) {
            return FALSE;
        }

        if (!m_visibility.IsTileVisible(m_visibility.PointToTile(goal))) {
            return FALSE;
        }
    }

    CPoint deltaExact;
    if (abs(startSearch.y - goalSearch.y) < abs(startSearch.x - goalSearch.x)) {
        if (goalSearch.x > startSearch.x) {
            deltaExact.x = 1 << CGameSprite::EXACT_SCALE;
        } else {
            deltaExact.x = -(1 << CGameSprite::EXACT_SCALE);
        }
        deltaExact.y = ((goalSearch.y - startSearch.y) << CGameSprite::EXACT_SCALE) / abs(goalSearch.x - startSearch.x);
    } else {
        deltaExact.x = ((goalSearch.x - startSearch.x) << CGameSprite::EXACT_SCALE) / abs(goalSearch.y - startSearch.y);
        if (goalSearch.y > startSearch.y) {
            deltaExact.y = 1 << CGameSprite::EXACT_SCALE;
        } else {
            deltaExact.y = -(1 << CGameSprite::EXACT_SCALE);
        }
    }

    CPoint tempPoint;
    tempPoint.x = (1 << (CGameSprite::EXACT_SCALE - 1)) + (startSearch.x << CGameSprite::EXACT_SCALE);
    tempPoint.y = (1 << (CGameSprite::EXACT_SCALE - 1)) + (startSearch.y << CGameSprite::EXACT_SCALE);

    while (startSearch != goalSearch) {
        if (m_search.GetLOSCost(startSearch, terrainTable, nTableIndex, FALSE) == CPathSearch::COST_IMPASSABLE) {
            return FALSE;
        }

        if (bCheckIfExplored) {
            CPoint pt(startSearch.x * CPathSearch::GRID_SQUARE_SIZEX,
                startSearch.y * CPathSearch::GRID_SQUARE_SIZEY);
            if (!m_visibility.IsTileExplored(m_visibility.PointToTile(pt))) {
                return FALSE;
            }
        }

        tempPoint.x += deltaExact.x;
        tempPoint.y += deltaExact.y;

        startSearch.x = tempPoint.x >> CGameSprite::EXACT_SCALE;
        startSearch.y = tempPoint.y >> CGameSprite::EXACT_SCALE;
    }

    return TRUE;
}

// 0x46ABB0
BOOL CGameArea::CheckWalkable(const CPoint& start, const CPoint& goal, const BYTE* terrainTable, BYTE personalSpace, BOOLEAN bCheckIfExplored)
{
    SHORT nTableIndex;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 608
    UTIL_ASSERT(terrainTable != NULL);

    if (start == goal) {
        return TRUE;
    }

    if ((start.x - goal.x) * (start.x - goal.x) + 16 * (start.y - goal.y) * (start.y - goal.y) / 9 > m_visibility.field_E * m_visibility.field_E * CVisibilityMap::SQUARE_SIZEX * CVisibilityMap::SQUARE_SIZEX) {
        return FALSE;
    }

    CPoint startSearch(start.x / CPathSearch::GRID_SQUARE_SIZEX,
        start.y / CPathSearch::GRID_SQUARE_SIZEY);

    CPoint goalSearch(goal.x / CPathSearch::GRID_SQUARE_SIZEX,
        goal.y / CPathSearch::GRID_SQUARE_SIZEY);

    if (startSearch.x < 0
        || startSearch.y < 0
        || startSearch.x >= m_search.m_GridSquareDimensions.cx
        || startSearch.y >= m_search.m_GridSquareDimensions.cy) {
        return FALSE;
    }

    if (goalSearch.x < 0
        || goalSearch.y < 0
        || goalSearch.x >= m_search.m_GridSquareDimensions.cx
        || goalSearch.y >= m_search.m_GridSquareDimensions.cy) {
        return FALSE;
    }

    if (startSearch == goalSearch) {
        return TRUE;
    }

    if (bCheckIfExplored) {
        if (!m_visibility.IsTileExplored(m_visibility.PointToTile(start))) {
            return FALSE;
        }

        if (!m_visibility.IsTileVisible(m_visibility.PointToTile(goal))) {
            return FALSE;
        }
    }

    CPoint deltaExact;
    if (abs(startSearch.y - goalSearch.y) < abs(startSearch.x - goalSearch.x)) {
        if (goalSearch.x > startSearch.x) {
            deltaExact.x = 1 << CGameSprite::EXACT_SCALE;
        } else {
            deltaExact.x = -(1 << CGameSprite::EXACT_SCALE);
        }
        deltaExact.y = ((goalSearch.y - startSearch.y) << CGameSprite::EXACT_SCALE) / abs(goalSearch.x - startSearch.x);
    } else {
        deltaExact.x = ((goalSearch.x - startSearch.x) << CGameSprite::EXACT_SCALE) / abs(goalSearch.y - startSearch.y);
        if (goalSearch.y > startSearch.y) {
            deltaExact.y = 1 << CGameSprite::EXACT_SCALE;
        } else {
            deltaExact.y = -(1 << CGameSprite::EXACT_SCALE);
        }
    }

    CPoint tempPoint;
    tempPoint.x = (1 << (CGameSprite::EXACT_SCALE - 1)) + (startSearch.x << CGameSprite::EXACT_SCALE);
    tempPoint.y = (1 << (CGameSprite::EXACT_SCALE - 1)) + (startSearch.y << CGameSprite::EXACT_SCALE);

    while (startSearch != goalSearch) {
        if (m_search.GetCost(startSearch, terrainTable, personalSpace, nTableIndex, TRUE) == CPathSearch::COST_IMPASSABLE) {
            return FALSE;
        }

        if (bCheckIfExplored) {
            CPoint pt(startSearch.x * CPathSearch::GRID_SQUARE_SIZEX,
                startSearch.y * CPathSearch::GRID_SQUARE_SIZEY);
            if (!m_visibility.IsTileExplored(m_visibility.PointToTile(pt))) {
                return FALSE;
            }
        }

        tempPoint.x += deltaExact.x;
        tempPoint.y += deltaExact.y;

        startSearch.x = tempPoint.x >> CGameSprite::EXACT_SCALE;
        startSearch.y = tempPoint.y >> CGameSprite::EXACT_SCALE;
    }

    return TRUE;
}

// 0x46AF40
LONG CGameArea::GetGroundPile(const CPoint& ptPos)
{
    POSITION pos;
    LONG iObject;
    CGameObject* pObject;
    BYTE rc;

    LONG iGroundPile = CGameObjectArray::INVALID_INDEX;

    CPoint ptGridPos;
    ptGridPos.x = ptPos.x / CPathSearch::GRID_SQUARE_SIZEX;
    ptGridPos.y = ptPos.y / CPathSearch::GRID_SQUARE_SIZEY;

    pos = m_lVertSortBack.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetPrev(pos)));

        rc = m_pGame->GetObjectArray()->GetShare(iObject,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() == CGameObject::TYPE_CONTAINER
                && static_cast<CGameContainer*>(pObject)->m_containerType == 4) {
                if (ptGridPos.x == pObject->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX
                    && ptGridPos.y == pObject->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY) {
                    iGroundPile = iObject;

                    m_pGame->GetObjectArray()->ReleaseShare(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    break;
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (iGroundPile == CGameObjectArray::INVALID_INDEX) {
        pos = m_lVertSortBackAdd.GetTailPosition();
        while (pos != NULL) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBackAdd.GetPrev(pos)));

            rc = m_pGame->GetObjectArray()->GetShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pObject->GetObjectType() == CGameObject::TYPE_CONTAINER
                    && static_cast<CGameContainer*>(pObject)->m_containerType == 4) {
                    if (ptGridPos.x == pObject->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX
                        && ptGridPos.y == pObject->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY) {
                        iGroundPile = iObject;

                        m_pGame->GetObjectArray()->ReleaseShare(iObject,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);

                        break;
                    }
                }

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }

    return iGroundPile;
}

// 0x46B160
LONG CGameArea::GetNearest(LONG startObject, const CAIObjectType& type, SHORT range, const BYTE* terrainTable, BOOL checkLOS, BOOL seeInvisible, BOOL ignoreSleeping, BYTE nNearest, BOOL ignoreDead)
{
    // NOTE: `nNearest` is the search's mode: 0 finds the nearest, 1 the
    // farthest (and since the vertical scan stops once a side is as far as
    // the best so far, which starts at 0, mode 1 finds nothing), any other
    // value nothing. `ignoreDead` (the name is a guess) keeps only summoned
    // creatures when it is 1; dead creatures are always left out.
    LONG nFound = CGameObjectArray::INVALID_INDEX;
    CAIObjectType callerType;
    DWORD dwSkipStates = STATE_DEAD;
    BOOL bRaceBE = type.m_nRace == 0xBE;

    if (ignoreSleeping) {
        dwSkipStates = STATE_DEAD | STATE_SLEEPING;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 852
    UTIL_ASSERT(terrainTable != NULL);

    CGameObject* pObject;
    BYTE rc = m_pGame->GetObjectArray()->GetShare(startObject,
        CGameObjectArray::THREAD_ASYNCH,
        &pObject,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return CGameObjectArray::INVALID_INDEX;
    }

    callerType.Set(pObject->GetAIType());

    if (pObject->GetPos().x < 0 || pObject->GetPos().y < 0) {
        m_pGame->GetObjectArray()->ReleaseShare(startObject,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return CGameObjectArray::INVALID_INDEX;
    }

    if (type.m_nLocationType != 0 && (pObject->GetObjectType() & 0x1) != 0) {
        CPoint pt;
        INT nRadius;
        if (type.sub_40CD80(static_cast<CGameAIBase*>(pObject), pt, nRadius) == TRUE) {
            m_pGame->GetObjectArray()->ReleaseShare(startObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return sub_46DAE0(pt.x, pt.y, type, nRadius, terrainTable, checkLOS, seeInvisible, nNearest, ignoreDead);
        }
    }

    if (pObject->GetVertListType() != CGameObject::LIST_FRONT) {
        CPoint pt = pObject->GetPos();
        m_pGame->GetObjectArray()->ReleaseShare(startObject,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return sub_46DAE0(pt.x, pt.y, type, range, terrainTable, checkLOS, seeInvisible, nNearest, ignoreDead);
    }

    CPoint ptStart = pObject->GetPos();
    INT xStart = pObject->GetPos().x;
    INT yStart = 4 * pObject->GetPos().y / 3;

    SHORT nTile = static_cast<SHORT>(ptStart.y / CVisibilityMap::SQUARE_SIZEY * m_visibility.m_nWidth
        + ptStart.x / CVisibilityMap::SQUARE_SIZEX);
    if (nTile >= m_visibility.m_nWidth * m_visibility.m_nHeight) {
        // NOTE: The original formats "Illegal position (%d,%d)" into a new
        // buffer and frees it unused.
        m_pGame->GetObjectArray()->ReleaseShare(startObject,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return CGameObjectArray::INVALID_INDEX;
    }

    POSITION posVertList = pObject->GetVertListPos();

    m_pGame->GetObjectArray()->ReleaseShare(startObject,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    if (posVertList == NULL) {
        return CGameObjectArray::INVALID_INDEX;
    }

    if (checkLOS) {
        if (m_pGame->GetCharacterPortraitNum(startObject) == -1) {
            if (!m_visibility.IsTileExplored(nTile)) {
                return CGameObjectArray::INVALID_INDEX;
            }
        } else {
            if (!m_visibility.IsTileVisible(nTile)) {
                return CGameObjectArray::INVALID_INDEX;
            }
        }
    }

    LONG nBest;
    if (nNearest == 0) {
        nBest = range * range + 1;
    } else {
        nBest = 0;
    }

    // NOTE: The original reads the node's links directly; any vertical list
    // walks a node the same way.
    POSITION posPrev = posVertList;
    m_lVertSort.GetPrev(posPrev);
    POSITION posNext = posVertList;
    m_lVertSort.GetNext(posNext);

    // One object: a sprite in range, alive, visible to the caller, above
    // ground, not ignoring the caller's type, of the type, in sight when
    // asked, and nearer (or farther) than the best so far. Returns FALSE when
    // it is out of range vertically, which ends its side.
    auto Check = [&](LONG nId, CGameObject* pTarget) -> BOOL {
        if (pTarget->GetVertListPos() == NULL) {
            return TRUE;
        }

        INT xTarget = pTarget->GetPos().x;
        LONG dy = (yStart - 4 * pTarget->GetPos().y / 3) * (yStart - 4 * pTarget->GetPos().y / 3);
        if (dy >= nBest) {
            return FALSE;
        }

        if (pTarget->GetObjectType() != CGameObject::TYPE_SPRITE) {
            return TRUE;
        }

        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        BYTE nRace = pSprite->GetAIType().m_nRace;
        if ((pSprite->m_derivedStats.m_generalState & dwSkipStates) != 0
            || !pSprite->IsVisibleToCaller(seeInvisible)
            || !pSprite->GetAnimation()->GetAboveGround()
            || pSprite->m_derivedStats.m_cImmunitiesAIType.OnList(callerType)
            || pSprite->GetBaseStats()->field_294 == 1
            || (ignoreDead == 1 && (pSprite->m_derivedStats.m_generalState & STATE_SUMMONED_CREATURE) == 0)
            || (nRace == 0xBE && !bRaceBE)
            || (pSprite->GetObjectType() == CGameObject::TYPE_SPRITE
                && (!pSprite->m_active || !pSprite->m_activeAI || !pSprite->m_activeImprisonment))) {
            return TRUE;
        }

        if (!pSprite->GetAIType().OfType(type, FALSE, FALSE)) {
            return TRUE;
        }

        if (checkLOS && !CheckLOS(ptStart, pSprite->GetPos(), terrainTable, TRUE)) {
            return TRUE;
        }

        LONG nDistance = (xStart - xTarget) * (xStart - xTarget) + dy;
        if (nNearest == 0) {
            if (nDistance < nBest) {
                nBest = nDistance;
                nFound = nId;
            }
        } else if (nNearest == 1) {
            if (nDistance > nBest) {
                nBest = nDistance;
                nFound = nId;
            }
        }

        return TRUE;
    };

    while (1) {
        if (posPrev == NULL && posNext == NULL) {
            return nFound;
        }

        if (posPrev != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(posPrev)));

            CGameObject* pTarget;
            rc = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pTarget,
                INFINITE);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (!Check(nId, pTarget)) {
                posPrev = NULL;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (posNext == NULL) {
                continue;
            }
        }

        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(posNext)));

        CGameObject* pTarget;
        rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pTarget,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (!Check(nId, pTarget)) {
            posNext = NULL;
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x46BAD0
LONG CGameArea::sub_46BAD0(LONG startObject, const CAIObjectType& type, SHORT range, const BYTE* terrainTable, BOOL checkLOS, BOOL seeInvisible, BOOL ignoreSleeping, BYTE nNearest, BOOL summonedOnly)
{
    // GetNearest's search for the (nNearest + 1)-th nearest creature of the
    // type to startObject, or the farthest one found when there are fewer.
    // Unlike GetNearest it has no location type and no race 0xBE test, and
    // a caller off the front list searches from its point (sub_46DDC0).
    CAIObjectType callerType;
    DWORD dwSkipStates = STATE_DEAD;
    if (ignoreSleeping) {
        dwSkipStates = STATE_DEAD | STATE_SLEEPING;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 1188
    UTIL_ASSERT(terrainTable != NULL);

    CGameObject* pObject;
    BYTE rc = m_pGame->GetObjectArray()->GetShare(startObject,
        CGameObjectArray::THREAD_ASYNCH,
        &pObject,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return CGameObjectArray::INVALID_INDEX;
    }

    callerType.Set(pObject->GetAIType());

    if (pObject->GetPos().x < 0 || pObject->GetPos().y < 0) {
        m_pGame->GetObjectArray()->ReleaseShare(startObject,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return CGameObjectArray::INVALID_INDEX;
    }

    if (pObject->GetVertListType() != CGameObject::LIST_FRONT) {
        CPoint pt = pObject->GetPos();
        m_pGame->GetObjectArray()->ReleaseShare(startObject,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return sub_46DDC0(pt.x, pt.y, type, range, terrainTable, checkLOS, seeInvisible, nNearest, summonedOnly);
    }

    CPoint ptStart = pObject->GetPos();
    INT xStart = pObject->GetPos().x;
    INT yStart = 4 * pObject->GetPos().y / 3;

    SHORT nTile = static_cast<SHORT>(ptStart.y / CVisibilityMap::SQUARE_SIZEY * m_visibility.m_nWidth
        + ptStart.x / CVisibilityMap::SQUARE_SIZEX);
    if (nTile >= m_visibility.m_nWidth * m_visibility.m_nHeight) {
        // NOTE: The original formats "Illegal position (%d,%d)" into a new
        // buffer and frees it unused.
        m_pGame->GetObjectArray()->ReleaseShare(startObject,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return CGameObjectArray::INVALID_INDEX;
    }

    POSITION posVertList = pObject->GetVertListPos();

    m_pGame->GetObjectArray()->ReleaseShare(startObject,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    if (posVertList == NULL) {
        return CGameObjectArray::INVALID_INDEX;
    }

    if (checkLOS) {
        if (m_pGame->GetCharacterPortraitNum(startObject) == -1) {
            if (!m_visibility.IsTileExplored(nTile)) {
                return CGameObjectArray::INVALID_INDEX;
            }
        } else {
            if (!m_visibility.IsTileVisible(nTile)) {
                return CGameObjectArray::INVALID_INDEX;
            }
        }
    }

    // NOTE: The original keeps the two tables on its stack, 12 entries each;
    // its callers ask for at most the tenth nearest.
    LONG nDistances[256];
    LONG nIds[256];
    INT nCount = nNearest + 1;
    for (SHORT nIndex = 0; nIndex < nCount; nIndex++) {
        nDistances[nIndex] = range * range + 1;
        nIds[nIndex] = CGameObjectArray::INVALID_INDEX;
    }

    // NOTE: The original reads the node's links directly; any vertical list
    // walks a node the same way.
    POSITION posPrev = posVertList;
    m_lVertSort.GetPrev(posPrev);
    POSITION posNext = posVertList;
    m_lVertSort.GetNext(posNext);

    // One object (the original has the same code for each side): a sprite
    // in range, not skipped for its state, visible to the caller, above
    // ground, not ignoring the caller's type, not hidden, summoned when
    // asked, active, of the type and in sight when asked goes into the
    // first place of the table it is nearer than. Returns FALSE when it is
    // as far vertically as the table's last place, which ends its side.
    auto Check = [&](LONG nId, CGameObject* pTarget) -> BOOL {
        if (pTarget->GetVertListPos() == NULL) {
            return TRUE;
        }

        INT xTarget = pTarget->GetPos().x;
        LONG dy = (yStart - 4 * pTarget->GetPos().y / 3) * (yStart - 4 * pTarget->GetPos().y / 3);
        if (dy >= nDistances[nNearest]) {
            return FALSE;
        }

        if (pTarget->GetObjectType() != CGameObject::TYPE_SPRITE) {
            return TRUE;
        }

        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        if ((pSprite->m_derivedStats.m_generalState & dwSkipStates) != 0
            || !pSprite->IsVisibleToCaller(seeInvisible)
            || !pSprite->GetAnimation()->GetAboveGround()
            || pSprite->m_derivedStats.m_cImmunitiesAIType.OnList(callerType)
            || pSprite->GetBaseStats()->field_294 == 1
            || (summonedOnly == 1 && (pSprite->m_derivedStats.m_generalState & STATE_SUMMONED_CREATURE) == 0)
            || !pSprite->m_active
            || !pSprite->m_activeAI
            || !pSprite->m_activeImprisonment) {
            return TRUE;
        }

        if (!pSprite->GetAIType().OfType(type, FALSE, FALSE)) {
            return TRUE;
        }

        if (checkLOS && !CheckLOS(ptStart, pSprite->GetPos(), terrainTable, TRUE)) {
            return TRUE;
        }

        LONG nDistance = (xStart - xTarget) * (xStart - xTarget) + dy;
        for (SHORT nIndex = 0; nIndex < nCount; nIndex++) {
            if (nDistance < nDistances[nIndex]) {
                memmove(&nDistances[nIndex + 1], &nDistances[nIndex], (nNearest - nIndex) * sizeof(LONG));
                memmove(&nIds[nIndex + 1], &nIds[nIndex], (nNearest - nIndex) * sizeof(LONG));
                nDistances[nIndex] = nDistance;
                nIds[nIndex] = nId;
                break;
            }
        }

        return TRUE;
    };

    while (1) {
        if (posPrev == NULL && posNext == NULL) {
            break;
        }

        if (posPrev != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(posPrev)));

            CGameObject* pTarget;
            rc = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pTarget,
                INFINITE);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (!Check(nId, pTarget)) {
                posPrev = NULL;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (posNext == NULL) {
                continue;
            }
        }

        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(posNext)));

        CGameObject* pTarget;
        rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pTarget,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (!Check(nId, pTarget)) {
            posNext = NULL;
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    while (nNearest > 0 && nIds[nNearest] == CGameObjectArray::INVALID_INDEX) {
        nNearest--;
    }

    return nIds[nNearest];
}

// NOTE: The name is the Ghidra project's: the nearest active creature whose
// dialog (m_dialog or field_56E4) is `dialog`, from `startObject` within
// `range`: first the front list outward from the caller's node (or the whole
// front list from its head when the caller is not on it), then the back
// list from its head. CGameDialogSprite::Reload passes the larger of the
// area's width and height as the range.
//
// 0x46C460
LONG CGameArea::GetNearestDialogMatch(LONG startObject, CResRef dialog, LONG range)
{
    LONG nBest = range * range + 1;
    LONG nFound = CGameObjectArray::INVALID_INDEX;
    CAIObjectType callerType;

    CGameObject* pObject;
    BYTE rc = m_pGame->GetObjectArray()->GetShare(startObject,
        CGameObjectArray::THREAD_ASYNCH,
        &pObject,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return CGameObjectArray::INVALID_INDEX;
    }

    // NOTE: The caller's type is copied and never read.
    callerType.Set(pObject->GetAIType());
    BYTE listType = pObject->GetVertListType();
    INT x = pObject->GetPos().x;
    INT y = 4 * pObject->GetPos().y / 3;
    POSITION posVertList = pObject->GetVertListPos();

    m_pGame->GetObjectArray()->ReleaseShare(startObject,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    POSITION posPrev;
    POSITION posNext;
    if (listType == CGameObject::LIST_FRONT) {
        // NOTE: The original reads the node's links unchecked; a caller on
        // the front list type with no node would fault. Bounded: no
        // neighbours.
        posPrev = posVertList;
        posNext = posVertList;
        if (posVertList != NULL) {
            m_lVertSort.GetPrev(posPrev);
            m_lVertSort.GetNext(posNext);
        }
    } else {
        posNext = m_lVertSort.GetHeadPosition();
        posPrev = NULL;
    }

    // One creature on the front list. Returns FALSE when it is out of range
    // vertically, which ends its side.
    auto Check = [&](LONG nId, CGameSprite* pSprite) -> BOOL {
        if (!pSprite->m_active
            || !pSprite->m_activeAI
            || !pSprite->m_activeImprisonment
            || pSprite->GetVertListPos() == NULL) {
            return TRUE;
        }

        const CPoint& pt = pSprite->GetPos();
        LONG dy = (y - 4 * pt.y / 3) * (y - 4 * pt.y / 3);
        if (dy >= nBest) {
            return FALSE;
        }

        if (pSprite->m_dialog == dialog || pSprite->field_56E4 == dialog) {
            LONG nDistance = (x - pt.x) * (x - pt.x) + dy;
            if (nDistance < nBest) {
                nBest = nDistance;
                nFound = nId;
            }
        }

        return TRUE;
    };

    while (posPrev != NULL || posNext != NULL) {
        if (posPrev != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(posPrev)));
            rc = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
                // NOTE: This leaves the round without the side below.
                m_pGame->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                continue;
            }

            if (!Check(nId, static_cast<CGameSprite*>(pObject))) {
                posPrev = NULL;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (posNext == NULL) {
                continue;
            }
        }

        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(posNext)));
        rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
            && !Check(nId, static_cast<CGameSprite*>(pObject))) {
            posNext = NULL;
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    // The back list (the dead and the sleeping): no imprisonment test.
    POSITION pos = m_lVertSortBack.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetNext(pos)));
        rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE && pObject->GetVertListPos() != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
            const CPoint& pt = pSprite->GetPos();
            LONG dy = (y - 4 * pt.y / 3) * (y - 4 * pt.y / 3);
            if (dy < nBest) {
                if (pSprite->m_active
                    && pSprite->m_activeAI
                    && (pSprite->m_dialog == dialog || pSprite->field_56E4 == dialog)) {
                    LONG nDistance = (x - pt.x) * (x - pt.x) + dy;
                    if (nDistance < nBest) {
                        nBest = nDistance;
                        nFound = nId;
                    }
                }
            } else {
                pos = NULL;
            }
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    return nFound;
}

// NOTE: Walks the vertical list outward from `posVertList` (the caller's
// own node, which it skips), one object above and one below per round, and
// stops a side once an object on it is out of range vertically: the list is
// sorted by y.
//
// 0x46CD20
void CGameArea::GetAllInRange(POSITION posVertList, const CPoint& center, const CAIObjectType& type, SHORT range, const BYTE* terrainTable, CTypedPtrList<CPtrList, LONG*>& targets, BOOL lineOfSight, BOOL checkForNonSprites)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 1849
    UTIL_ASSERT(terrainTable != NULL);

    CPoint pt;
    pt.x = center.x;
    pt.y = 4 * center.y / 3;

    // NOTE: The original reads the node's links directly; any vertical list
    // walks a node the same way.
    POSITION posNext = posVertList;
    m_lVertSort.GetNext(posNext);
    POSITION posPrev = posVertList;
    m_lVertSort.GetPrev(posPrev);

    // One object: in range, of the type, in sight, active and above ground.
    // Returns FALSE when it is out of range vertically, which ends its side.
    auto Check = [&](CGameObject* pObject) -> BOOL {
        if (pObject->GetVertListPos() == NULL) {
            return TRUE;
        }

        const CPoint& ptObject = pObject->GetPos();
        LONG dy = (pt.y - 4 * ptObject.y / 3) * (pt.y - 4 * ptObject.y / 3);
        if (dy >= range * range + 1) {
            return FALSE;
        }

        if ((pt.x - ptObject.x) * (pt.x - ptObject.x) + dy < range * range + 1
            && pObject->GetAIType().OfType(type, checkForNonSprites, !checkForNonSprites)
            && (!lineOfSight
                || CheckLOS(center, pObject->GetPos(), terrainTable, FALSE))
            && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                || (static_cast<CGameSprite*>(pObject)->m_active
                    && static_cast<CGameSprite*>(pObject)->m_activeAI))
            && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                || static_cast<CGameSprite*>(pObject)->GetAnimation()->GetAboveGround())) {
            targets.AddTail(reinterpret_cast<LONG*>(static_cast<INT_PTR>(pObject->GetId())));
        }

        return TRUE;
    };

    while (1) {
        if (posPrev == NULL && posNext == NULL) {
            return;
        }

        if (posPrev != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(posPrev)));

            CGameObject* pObject;
            BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                && static_cast<CGameSprite*>(pObject)->GetBaseStats()->field_294 == 1) {
                // NOTE: This skip leaves the round without the side below
                // (0x46CDF4 jumps to the release at 0x46D119).
                m_pGame->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                continue;
            }

            if (!Check(pObject)) {
                posPrev = NULL;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (posNext == NULL) {
                continue;
            }
        }

        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(posNext)));

        CGameObject* pObject;
        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
            || static_cast<CGameSprite*>(pObject)->GetBaseStats()->field_294 != 1) {
            if (!Check(pObject)) {
                posNext = NULL;
            }
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x46D140
void CGameArea::GetAllInRange(const CPoint& center, const CAIObjectType& type, SHORT range, const BYTE* terrainTable, CTypedPtrList<CPtrList, LONG*>& targets, BOOL lineOfSight, BOOL checkForNonSprites)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 1969
    UTIL_ASSERT(terrainTable != NULL);

    CPoint pt;
    pt.x = center.x;
    pt.y = 4 * center.y / 3;

    POSITION pos = m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;

        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                || static_cast<CGameSprite*>(pObject)->GetBaseStats()->field_294 != 1) {
                const CPoint& ptObject = pObject->GetPos();
                if (abs(pt.y - 4 * ptObject.y / 3) <= range
                    && (4 * ptObject.y / 3 - pt.y) * (4 * ptObject.y / 3 - pt.y) + (ptObject.x - pt.x) * (ptObject.x - pt.x) <= range * range
                    && pObject->GetAIType().OfType(type, checkForNonSprites, !checkForNonSprites)
                    && (!lineOfSight
                        || CheckLOS(center, pObject->GetPos(), terrainTable, FALSE))
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || (static_cast<CGameSprite*>(pObject)->m_active
                            && static_cast<CGameSprite*>(pObject)->m_activeAI))
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || static_cast<CGameSprite*>(pObject)->GetAnimation()->GetAboveGround())) {
                    targets.AddTail(reinterpret_cast<LONG*>(nId));
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x46D390
int CGameArea::GetCountInRange(const CPoint& center, SHORT range, const CAIObjectType& type)
{
    CPoint pt;
    pt.x = center.x;
    pt.y = 4 * center.y / 3;

    POSITION pos = m_lVertSort.GetHeadPosition();
    int count = 0;
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;

        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            LONG x = pObject->GetPos().x;
            LONG y = 4 * pObject->GetPos().y / 3;
            if (abs(pt.y - y) <= range
                && (y - pt.y) * (y - pt.y) + (x - pt.x) * (x - pt.x) <= range * range
                && pObject->GetAIType().OfType(type, FALSE, FALSE)) {
                count++;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
    return count;
}

// 0x46D4D0
int CGameArea::GetCountInPoly(const CRect& rBounding, const CAIObjectType& type)
{
    POSITION pos = m_lVertSort.GetHeadPosition();
    int count = 0;
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;

        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (rBounding.PtInRect(pObject->GetPos())
                && pObject->GetAIType().OfType(type, FALSE, FALSE)) {
                count++;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
    return count;
}

// 0x46D590
void CGameArea::GetAllInRangeBack(const CPoint& center, const CAIObjectType& type, SHORT range, const BYTE* terrainTable, CTypedPtrList<CPtrList, LONG*>& targets, BOOL lineOfSight, BOOL ignoreDead, BOOL checkForNonSprites)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 2119
    UTIL_ASSERT(terrainTable != NULL);

    CPoint pt;
    pt.x = center.x;
    pt.y = 4 * center.y / 3;

    POSITION pos = m_lVertSortBack.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetNext(pos)));

        CGameObject* pObject;

        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                || static_cast<CGameSprite*>(pObject)->GetBaseStats()->field_294 != 1) {
                const CPoint& ptObject = pObject->GetPos();
                if (abs(pt.y - 4 * ptObject.y / 3) <= range
                    && (4 * ptObject.y / 3 - pt.y) * (4 * ptObject.y / 3 - pt.y) + (ptObject.x - pt.x) * (ptObject.x - pt.x) <= range * range
                    && pObject->GetAIType().OfType(type, checkForNonSprites, !checkForNonSprites)
                    && (!lineOfSight
                        || CheckLOS(center, pObject->GetPos(), terrainTable, FALSE))
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || (static_cast<CGameSprite*>(pObject)->m_active
                            && static_cast<CGameSprite*>(pObject)->m_activeAI))
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || static_cast<CGameSprite*>(pObject)->GetAnimation()->GetAboveGround())
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || ignoreDead
                        || (static_cast<CGameSprite*>(pObject)->GetDerivedStats()->m_generalState & STATE_DEAD) == 0)) {
                    targets.AddTail(reinterpret_cast<LONG*>(nId));
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x46D7E0
void CGameArea::GetAllInPoly(const CRect& rBounding, CPoint* pPoly, SHORT nPoly, const CAIObjectType& type, const BYTE* terrainTable, CTypedPtrList<CPtrList, LONG*>& targets, BOOLEAN checkBackList)
{
    POSITION pos;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 2194
    UTIL_ASSERT(terrainTable != NULL);

    pos = m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;

        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if ((pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                    || static_cast<CGameSprite*>(pObject)->GetBaseStats()->field_294 != 1)
                && rBounding.PtInRect(pObject->GetPos())
                && CVidPoly::IsPtInPoly(pPoly, nPoly, pObject->GetPos())
                && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                    || (static_cast<CGameSprite*>(pObject)->m_active
                        && static_cast<CGameSprite*>(pObject)->m_activeAI))
                && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                    || static_cast<CGameSprite*>(pObject)->GetAnimation()->GetAboveGround())) {
                targets.AddTail(reinterpret_cast<LONG*>(nId));
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (checkBackList == TRUE) {
        pos = m_lVertSortBack.GetHeadPosition();
        while (pos != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetNext(pos)));

            CGameObject* pObject;

            BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                if ((pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || static_cast<CGameSprite*>(pObject)->GetBaseStats()->field_294 != 1)
                    && rBounding.PtInRect(pObject->GetPos())
                    && CVidPoly::IsPtInPoly(pPoly, nPoly, pObject->GetPos())
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || (static_cast<CGameSprite*>(pObject)->m_active
                            && static_cast<CGameSprite*>(pObject)->m_activeAI))
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || static_cast<CGameSprite*>(pObject)->GetAnimation()->GetAboveGround())
                    && (pObject->GetObjectType() != CGameObject::TYPE_SPRITE
                        || ((static_cast<CGameSprite*>(pObject)->GetDerivedStats()->m_generalState & STATE_DEAD) != 0
                            && (static_cast<CGameSprite*>(pObject)->GetBaseStats()->m_generalState & STATE_DEAD) != 0))) {
                    targets.AddTail(reinterpret_cast<LONG*>(nId));
                }

                m_pGame->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }
}

// 0x46DAE0
LONG CGameArea::sub_46DAE0(INT x, INT y, const CAIObjectType& type, int a4, const BYTE* terrainTable, int a6, int a7, int a8, int a9)
{
    // NOTE: The arguments as GetNearest's: a4 the range, a6 checkLOS, a7
    // seeInvisible, a8 the mode (0 nearest, 1 farthest), a9 summoned
    // creatures only.
    SHORT range = static_cast<SHORT>(a4);
    BOOL bRaceBE = type.m_nRace == 0xBE;

    if (x < 0 || y < 0) {
        return CGameObjectArray::INVALID_INDEX;
    }

    LONG nBest;
    if (a8 == 0) {
        nBest = range * range + 1;
    } else {
        nBest = 0;
    }

    POSITION pos = m_lVertSort.GetHeadPosition();
    LONG nFound = CGameObjectArray::INVALID_INDEX;
    INT yScaled = 4 * y / 3;

    if (pos == NULL) {
        return CGameObjectArray::INVALID_INDEX;
    }

    do {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;
        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            BYTE nObjectType = pObject->GetObjectType();
            if (pObject->GetAIType().m_nRace != 0xBE || bRaceBE == TRUE) {
                CPoint ptObject(pObject->GetPos().x, 4 * pObject->GetPos().y / 3);
                if (yScaled - ptObject.y <= range) {
                    if (ptObject.y - yScaled > range) {
                        // The list is sorted by y: the rest are farther.
                        m_pGame->GetObjectArray()->ReleaseShare(nId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                        return nFound;
                    }

                    if (type.sub_40D920(ptObject)) {
                        BOOL bCandidate;
                        if (nObjectType == CGameObject::TYPE_SPRITE) {
                            CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
                            bCandidate = pSprite->m_active
                                && pSprite->m_activeAI
                                && pSprite->m_activeImprisonment
                                && pSprite->GetAnimation()->GetAboveGround()
                                && pSprite->IsVisibleToCaller(a7);
                        } else {
                            bCandidate = TRUE;
                        }

                        // NOTE: With a9 the original reads the general
                        // state at 0x920 of any object, a sprite's derived
                        // state; another object's bytes there are not a
                        // state. Ours reads it for sprites and takes 0 for
                        // the others.
                        DWORD dwState = nObjectType == CGameObject::TYPE_SPRITE
                            ? static_cast<CGameSprite*>(pObject)->m_derivedStats.m_generalState
                            : 0;

                        if (bCandidate
                            && pObject->GetAIType().OfType(type, FALSE, FALSE)
                            && (a9 == 0 || (dwState & STATE_SUMMONED_CREATURE) != 0)
                            && (!a6 || CheckLOS(CPoint(x, y), pObject->GetPos(), terrainTable, FALSE))) {
                            LONG nDistance = (ptObject.x - x) * (ptObject.x - x) + (ptObject.y - yScaled) * (ptObject.y - yScaled);
                            if (a8 == 0) {
                                if (nDistance < nBest) {
                                    nBest = nDistance;
                                    nFound = nId;
                                }
                            } else if (a8 == 1) {
                                if (nDistance > nBest) {
                                    nBest = nDistance;
                                    nFound = nId;
                                }
                            }
                        }
                    }
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    } while (pos != NULL);

    return nFound;
}

// 0x46DDC0
LONG CGameArea::sub_46DDC0(INT x, INT y, const CAIObjectType& type, SHORT range, const BYTE* terrainTable, BOOL checkLOS, BOOL seeInvisible, BYTE nNearest, BOOL summonedOnly)
{
    // The (nNearest + 1)-th nearest object of the type to the point within
    // range, or the farthest one found when there are fewer.
    if (x < 0 || y < 0) {
        return CGameObjectArray::INVALID_INDEX;
    }

    // NOTE: The original keeps the two tables on its stack, 12 entries each;
    // its callers ask for at most the tenth nearest.
    LONG nDistances[256];
    LONG nIds[256];
    INT nCount = nNearest + 1;
    for (SHORT nIndex = 0; nIndex < nCount; nIndex++) {
        nDistances[nIndex] = range * range + 1;
        nIds[nIndex] = CGameObjectArray::INVALID_INDEX;
    }

    INT yScaled = 4 * y / 3;

    POSITION pos = m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;
        BYTE rc = m_pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        LONG xObject = pObject->GetPos().x;
        LONG yObject = 4 * pObject->GetPos().y / 3;

        if (yScaled - yObject <= range) {
            if (yObject - yScaled > range) {
                // The list is sorted by y: the rest are farther.
                m_pGame->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                break;
            }

            BOOL bCandidate = TRUE;
            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
                bCandidate = pSprite->m_active
                    && pSprite->m_activeAI
                    && pSprite->m_activeImprisonment
                    && pSprite->GetAnimation()->GetAboveGround()
                    && pSprite->IsVisibleToCaller(seeInvisible);
            }

            // NOTE: With summonedOnly the original reads the general state
            // at 0x920 of any object, a sprite's derived state; ours reads it
            // for sprites and takes 0 for the others (as sub_46DAE0).
            DWORD dwState = pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                ? static_cast<CGameSprite*>(pObject)->m_derivedStats.m_generalState
                : 0;

            if (bCandidate
                && pObject->GetAIType().OfType(type, FALSE, FALSE)
                && (!summonedOnly || (dwState & STATE_SUMMONED_CREATURE) != 0)) {
                LONG nDistance = (xObject - x) * (xObject - x) + (yObject - yScaled) * (yObject - yScaled);
                for (SHORT nIndex = 0; nIndex < nCount; nIndex++) {
                    if (nDistance > nDistances[nIndex]) {
                        continue;
                    }

                    // NOTE: A failed line of sight moves on to the next
                    // place in the table and checks again.
                    if (checkLOS && !CheckLOS(CPoint(x, y), pObject->GetPos(), terrainTable, FALSE)) {
                        continue;
                    }

                    memmove(&nDistances[nIndex + 1], &nDistances[nIndex], (nNearest - nIndex) * sizeof(LONG));
                    memmove(&nIds[nIndex + 1], &nIds[nIndex], (nNearest - nIndex) * sizeof(LONG));
                    nDistances[nIndex] = nDistance;
                    nIds[nIndex] = nId;
                    break;
                }
            }
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    while (nNearest > 0 && nIds[nNearest] == CGameObjectArray::INVALID_INDEX) {
        nNearest--;
    }

    return nIds[nNearest];
}

// 0x46E130
void CGameArea::ApplyEffectToSprites(CGameEffect* pEffect, BOOL bNotParty, BOOL bMatchSpecific, BYTE nSpecific, CGameObject* pExclude)
{
    // Hands a copy of pEffect to every sprite of the area through a
    // CMessageAddEffect, skipping it per the filters. The effect's target
    // types 4-8 of the spell dispatchers all land here:
    //   4 all creatures, 5 all but the party, 6 the caster's AI specific,
    //   7 the target's AI specific, 8 all but the caster.
    //
    // NOTE: The original releases the share after the type and hidden checks
    // and takes it again for each later filter (party and network, specific,
    // exclude), releasing it before the message. A THREAD_ASYNCH share keeps
    // no count (ReleaseShare returns at once), so holding one share across the
    // filters is the same.
    POSITION pos = m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

        CGameObject* pObject;
        BYTE rc;
        do {
            rc = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (pSprite->GetBaseStats()->field_294 == 1) {
            // The hidden (fork: stealth) flag set: not a candidate.
            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        if (bNotParty && m_pGame->GetCharacterPortraitNum(nId) != -1) {
            // A party member, which the callers of this path want skipped
            // unless m_gameSave.field_1AC (CInfGame +0x43D8, the fork's arena
            // flag) is set and another player owns it.
            if (m_pGame->m_gameSave.field_1AC == 0) {
                m_pGame->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                continue;
            }

            BYTE rcParty = m_pGame->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
            if (rcParty != CGameObjectArray::SUCCESS
                || g_pChitin->cNetwork.m_nServiceProvider == 0
                || g_pChitin->cNetwork.m_idLocalPlayer == pObject->m_remotePlayerID) {
                m_pGame->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                continue;
            }
        }

        if (bMatchSpecific && pObject->GetAIType().m_nSpecific != nSpecific) {
            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        if (pExclude != NULL && pObject == pExclude) {
            m_pGame->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        m_pGame->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        CMessageAddEffect* pMessage = new CMessageAddEffect(pEffect->Copy(), nId, nId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }
}

// 0x46F5A0
BOOLEAN CGameArea::CanSaveGame(STRREF& strError)
{
    CGameObjectArray* pObjectArray = m_pGame->GetObjectArray();
    POSITION pos;
    LONG iObject;
    CGameObject* pObject;
    BYTE rc;

    if ((m_header.m_flags & 0x1) != 0) {
        strError = 16502;
        return FALSE;
    }

    pos = m_lVertSortFlight.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlight.GetPrev(pos)));

        rc = pObjectArray->GetShare(iObject,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (!pObject->CanSaveGame(strError)) {
                pObjectArray->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return FALSE;
            }

            pObjectArray->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_lVertSort.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(pos)));

        rc = pObjectArray->GetShare(iObject,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (!pObject->CanSaveGame(strError)) {
                pObjectArray->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return FALSE;
            }

            pObjectArray->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_lVertSortBack.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetPrev(pos)));

        rc = pObjectArray->GetShare(iObject,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (!pObject->CanSaveGame(strError)) {
                pObjectArray->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return FALSE;
            }

            pObjectArray->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    strError = -1;
    return TRUE;
}

namespace {

// iwd2r: the replay mode (platform/replay.h) makes the search thread's
// timing deterministic. A path search an object's update queued
// (CGameSprite::SetTarget 0x707D40, AIUpdateWalk 0x6F9040) waits for the
// object's deny, so the search thread takes the object's share and its
// snapshot of the search map when the update releases it; in the original
// it is spinning on that share by then, so the snapshot comes before the
// next object moves, and the search is done at the object's next update in
// every recording. Here the AI thread waits after the update until the
// search is done, so neither the snapshot nor the result depend on how the
// threads are scheduled. What the wait rules out, and what it does not
// cover, is in platform/replay.h.
//
// The wait has a deadline of real time (platform::replay::WaitForSearch):
// a search the search thread never takes up (its lost semaphore post, no
// search thread) would otherwise stop the AI thread for good. With no
// search thread, or once a wait has given up (the run already fails on
// search_wait_timeouts), one poll decides, so later objects do not wait
// for it again.
void ReplayWaitForSearch(CInfGame* pGame, LONG iObject)
{
    if (!platform::replay::Active()) {
        return;
    }

    const uint32_t nDeadlineMs = pGame->m_hSearchThread != NULL && platform::replay::SearchWaitTimeouts() == 0
        ? platform::replay::kSearchWaitMs
        : 0;
    platform::replay::WaitForSearch(nDeadlineMs, [pGame, iObject](std::string* what) -> bool {
        BOOL bPending = FALSE;
        const CSearchRequest* pRequest = NULL;
        CGameObject* pObject;
        CSingleLock searchLock(&(pGame->field_1B58), TRUE);
        const INT nFront = pGame->m_searchRequests.GetCount();
        const INT nBack = pGame->m_searchRequestsBack.GetCount();
        if (pGame->GetObjectArray()->GetShare(iObject, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS
            && pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            pRequest = static_cast<CGameSprite*>(pObject)->m_currentSearchRequest;
        }
        if (nFront != 0 || nBack != 0) {
            bPending = TRUE;
        } else if (pRequest != NULL
            && (pRequest->m_serviceState == CSearchRequest::STATE_PROCESSING
                || (pRequest->m_serviceState == CSearchRequest::STATE_WAITING
                    && pRequest->m_collisionDelay == 0))) {
            // A request is off the lists while the search thread runs it; one
            // still waiting with a collision delay is not queued yet.
            bPending = TRUE;
        }
        if (bPending) {
            *what = "object " + std::to_string(iObject) + ", "
                + (pRequest != NULL
                        ? "its request in state " + std::to_string(pRequest->m_serviceState) + " (collision delay "
                            + std::to_string(pRequest->m_collisionDelay) + ")"
                        : std::string("no request of its own"))
                + ", " + std::to_string(nFront) + " requests queued in front and " + std::to_string(nBack) + " in back, "
                + (pGame->m_hSearchThread != NULL ? "search thread running" : "no search thread");
        }
        searchLock.Unlock();
        return bPending != FALSE;
    });
}

}

// 0x46E3D0
void CGameArea::AIUpdate()
{
    POSITION pos;
    LONG iObject;
    CGameObject* pObject;
    BYTE rc;

    if (!m_bAreaLoaded) {
        return;
    }

    SortLists();

    if (m_firstRender > 0) {
        if (m_firstRender <= 1 && m_pGame->GetVisibleArea() == this) {
            return;
        }

        m_firstRender--;
    }

    if (field_241 > 0) {
        field_241--;
    } else {
        m_iPickedOnDown = CGameObjectArray::INVALID_INDEX;
    }

    INT x;
    INT y;
    m_cInfinity.GetViewPosition(x, y);

    BOOL bMouseInPlayArea = g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineWorld
        && m_ptMousePos.x >= m_cInfinity.rViewPort.left
        && m_ptMousePos.x < m_cInfinity.rViewPort.right
        && m_ptMousePos.y >= m_cInfinity.rViewPort.top
        && m_ptMousePos.y < m_cInfinity.rViewPort.bottom;

    CPoint ptWorldMouse = m_cInfinity.GetWorldCoordinates(m_ptMousePos);

    if ((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & 0x4000) != 0) {
        if (m_pGame->GetVisibleArea() == this
            && g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineWorld) {
            m_bTravelSquare = FALSE;
            if (m_ptOldViewPos.x != x || m_ptOldViewPos.y != y) {
                if (m_firstRender == 0) {
                    if (m_ptMousePos.x == 0 && x <= 0) {
                        if (m_nScrollState == 7
                            || (m_nScrollState == 8 && y <= 0)
                            || (m_nScrollState == 6 && y >= m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1)) {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 6;
                            m_nScrollState = 9;
                        }
                    } else if (m_ptMousePos.x == CVideo::SCREENWIDTH - 1
                        && x >= m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1) {
                        if (m_nScrollState == 3
                            || (m_nScrollState == 2 && y <= 0)
                            || (m_nScrollState == 4 && y >= m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1)) {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 2;
                            m_nScrollState = 9;
                        }
                    } else if (m_ptMousePos.y == 0 && y <= 0) {
                        if (m_nScrollState == 1
                            || (m_nScrollState == 8 && x <= 0)
                            || (m_nScrollState == 2 && x >= m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1)) {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 0;
                            m_nScrollState = 9;
                        }
                    } else if (m_ptMousePos.y == CVideo::SCREENHEIGHT - 1
                        && y >= m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1) {
                        if (m_nScrollState == 5
                            || (m_nScrollState == 6 && x <= 0)
                            || (m_nScrollState == 4 && x >= m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1)) {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 0;
                            m_nScrollState = 4;
                        }
                    }
                } else {
                    m_nScrollState = 0;
                    m_cInfinity.m_nScrollDelay = CInfinity::SCROLL_DELAY;
                }
            }

            if (m_groupMove) {
                if (abs(ptWorldMouse.x - m_moveDest.x) > 8
                    || abs(ptWorldMouse.y - m_moveDest.y) > 8) {
                    CPoint ptCursor;
                    if (bMouseInPlayArea) {
                        ptCursor.x = 2 * m_moveDest.x - ptWorldMouse.x;
                        ptCursor.y = 2 * m_moveDest.y - ptWorldMouse.y;
                    } else {
                        CPoint ptScreen;
                        ptScreen.x = min(max(m_cInfinity.rViewPort.left, m_ptMousePos.x), m_cInfinity.rViewPort.right - 1);
                        ptScreen.y = min(max(m_cInfinity.rViewPort.top, m_ptMousePos.y), m_cInfinity.rViewPort.bottom - 1);

                        CPoint ptWorld = m_cInfinity.GetWorldCoordinates(ptScreen);
                        ptCursor.x = 2 * m_moveDest.x - ptWorld.x;
                        ptCursor.y = 2 * m_moveDest.y - ptWorld.y;
                    }

                    m_pGame->GetGroup()->GroupDrawMove(m_moveDest,
                        m_pGame->GetGameSave()->m_curFormation,
                        ptCursor);

                    if (m_pGame->GetState() == 0) {
                        m_pGame->SetTempCursor(8);
                    }

                    if (m_pGame->GetState() == 3) {
                        if (m_selectSquare.left != -1) {
                            m_selectSquare.right += x - m_ptOldViewPos.x;
                            m_selectSquare.bottom += y - m_ptOldViewPos.y;
                        }
                    }
                } else {
                    m_pGame->GetGroup()->GroupCancelMove();
                    if (m_pGame->GetState() == 0) {
                        m_pGame->SetTempCursor(4);
                    }
                }
            } else {
                if (m_selectSquare.left != -1) {
                    m_selectSquare.right += x - m_ptOldViewPos.x;
                    m_selectSquare.bottom += y - m_ptOldViewPos.y;

                    if (m_pGame->GetState() == 0) {
                        m_pGame->SetTempCursor(0);
                    }
                }
            }

            m_ptOldViewPos.x = x;
            m_ptOldViewPos.y = y;

            if (m_nScrollState == 0) {
                if (bMouseInPlayArea) {
                    if (m_pGame->m_tempCursor == 4) {
                        BOOL bSquareVisible = m_visibility.IsTileExplored(m_visibility.PointToTile(ptWorldMouse));
                        if (m_iPicked != CGameObjectArray::INVALID_INDEX
                            && bSquareVisible) {
                            rc = m_pGame->GetObjectArray()->GetShare(m_iPicked,
                                CGameObjectArray::THREAD_ASYNCH,
                                &pObject,
                                INFINITE);
                            if (rc == CGameObjectArray::SUCCESS) {
                                pObject->SetCursor(m_nToolTip);
                                m_pGame->GetObjectArray()->ReleaseShare(m_iPicked,
                                    CGameObjectArray::THREAD_ASYNCH,
                                    INFINITE);
                            } else {
                                g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
                            }
                        } else {
                            if (m_pGame->GetGroup()->GetCount() != 0) {
                                if (bSquareVisible) {
                                    SHORT searchSquareCode;
                                    CPoint mouseSearchSquare;
                                    mouseSearchSquare.x = ptWorldMouse.x / CPathSearch::GRID_SQUARE_SIZEX;
                                    mouseSearchSquare.y = ptWorldMouse.y / CPathSearch::GRID_SQUARE_SIZEY;
                                    if (m_search.GetLOSCost(mouseSearchSquare, m_terrainTable, searchSquareCode, 0) == CPathSearch::COST_IMPASSABLE) {
                                        switch (m_pGame->GetState()) {
                                        case 0:
                                            if (searchSquareCode == 14) {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(34, FALSE);
                                                m_bTravelSquare = TRUE;
                                            } else {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                                            }
                                            break;
                                        case 1:
                                        case 2:
                                            if (g_pBaldurChitin->GetObjectGame()->GetIconIndex() != -1) {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                                            } else {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(102, FALSE);
                                                g_pBaldurChitin->GetObjectCursor()->SetCustomCursor(g_pBaldurChitin->GetObjectGame()->m_iconResRef, FALSE, -1);
                                            }
                                            break;
                                        case 3:
                                            g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                                            break;
                                        default:
                                            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                                            // __LINE__: 2809
                                            UTIL_ASSERT(!"Invalid Game State");
                                        }
                                    } else {
                                        switch (m_pGame->GetState()) {
                                        case 0:
                                            g_pBaldurChitin->GetObjectCursor()->SetCursor(4, FALSE);
                                            break;
                                        case 1:
                                            if (g_pBaldurChitin->GetObjectGame()->GetIconIndex() != -1) {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(g_pBaldurChitin->GetObjectGame()->GetIconIndex(), FALSE);
                                            } else {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(20, FALSE);
                                            }
                                            break;
                                        case 2:
                                            if (g_pBaldurChitin->GetObjectGame()->GetIconIndex() != -1) {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(g_pBaldurChitin->GetObjectGame()->GetIconIndex(), FALSE);
                                                g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                                            } else {
                                                g_pBaldurChitin->GetObjectCursor()->SetCursor(102, FALSE);
                                                g_pBaldurChitin->GetObjectCursor()->SetCustomCursor(g_pBaldurChitin->GetObjectGame()->m_iconResRef, FALSE, -1);
                                            }
                                            break;
                                        case 3:
                                            g_pBaldurChitin->GetObjectCursor()->SetCursor(16, FALSE);
                                            break;
                                        default:
                                            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                                            // __LINE__: 2845
                                            UTIL_ASSERT(!"Invalid Game State");
                                        }
                                    }
                                } else {
                                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                                }
                            } else {
                                g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
                            }
                        }
                    } else {
                        g_pBaldurChitin->GetObjectCursor()->SetCursor(m_pGame->m_tempCursor, FALSE);
                    }
                } else {
                    if (m_pGame->m_tempCursor == 4) {
                        rc = m_pGame->GetObjectArray()->GetShare(m_iPicked,
                            CGameObjectArray::THREAD_ASYNCH,
                            &pObject,
                            INFINITE);
                        if (rc == CGameObjectArray::SUCCESS) {
                            pObject->SetCursor(m_nToolTip);
                            m_pGame->GetObjectArray()->ReleaseShare(m_iPicked,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        } else {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
                        }
                    } else {
                        g_pBaldurChitin->GetObjectCursor()->SetCursor(m_pGame->m_tempCursor, FALSE);
                    }
                }
            }
        }
    } else {
        m_nScrollState = 0;
        m_nKeyScrollState = 0;
        if (m_pGame->m_tempCursor != 4) {
            g_pBaldurChitin->GetObjectCursor()->SetCursor(m_pGame->m_tempCursor, FALSE);
        } else {
            g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
        }
    }

    BOOL bActive = m_pGame->GetWorldTimer()->m_active;

    m_nToolTip++;

    if (mpSpawner != NULL) {
        mpSpawner->Execute(this);
    }

    pos = m_lVertSortFlight.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlight.GetPrev(pos)));

        do {
            rc = m_pGame->GetObjectArray()->GetShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->DoAIUpdate(bActive, g_pChitin->nAUCounter)) {
                do {
                    rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED);

                if (rc == CGameObjectArray::SUCCESS) {
                    pObject->AIUpdate();

                    m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    // iwd2r: a no-op unless IWD2R_STATE_DUMP is set.
                    ReplayWaitForSearch(m_pGame, iObject);
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_lVertSort.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(pos)));

        do {
            rc = m_pGame->GetObjectArray()->GetShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (!m_bPicked
                && bMouseInPlayArea
                && m_pGame->GetVisibleArea() == this
                && pObject->IsOver(ptWorldMouse)) {
                if (m_iPicked != iObject) {
                    if (m_pGame->m_tempCursor == 101) {
                        m_pGame->m_tempCursor = 4;
                    }
                    m_nToolTip = 0;
                }

                m_iPicked = iObject;

                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                    CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
                    if (pSprite->Orderable(FALSE)) {
                        m_iPickedTarget = pSprite->GetTargetId();
                    }
                }

                m_bPicked = TRUE;
            }

            if (pObject->DoAIUpdate(bActive, g_pChitin->nAUCounter)) {
                do {
                    rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED);

                if (rc == CGameObjectArray::SUCCESS) {
                    pObject->AIUpdate();

                    m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    // iwd2r: a no-op unless IWD2R_STATE_DUMP is set.
                    ReplayWaitForSearch(m_pGame, iObject);
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_lVertSortBack.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetPrev(pos)));

        do {
            rc = m_pGame->GetObjectArray()->GetShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (!m_bPicked
                && bMouseInPlayArea
                && m_pGame->GetVisibleArea() == this
                && pObject->IsOver(ptWorldMouse)) {
                if (m_iPicked != iObject) {
                    if (m_pGame->m_tempCursor == 101) {
                        m_pGame->m_tempCursor = 4;
                    }
                    m_nToolTip = 0;
                }

                m_iPicked = iObject;

                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                    CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
                    if (pSprite->Orderable(FALSE)) {
                        m_iPickedTarget = pSprite->GetTargetId();
                    }
                }

                m_bPicked = TRUE;
            }

            if (pObject->DoAIUpdate(bActive, g_pChitin->nAUCounter)) {
                do {
                    rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED);

                if (rc == CGameObjectArray::SUCCESS) {
                    pObject->AIUpdate();

                    m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    // iwd2r: a no-op unless IWD2R_STATE_DUMP is set.
                    ReplayWaitForSearch(m_pGame, iObject);
                }
            }

            m_pGame->GetObjectArray()->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (m_pGame->GetVisibleArea() == this) {
        if (!m_bPicked) {
            if (bMouseInPlayArea) {
                if (m_pGame->m_tempCursor == 101) {
                    m_pGame->m_tempCursor = 4;
                }
            }

            m_nToolTip = 0;
            m_iPicked = CGameObjectArray::INVALID_INDEX;
            m_iPickedTarget = CGameObjectArray::INVALID_INDEX;
        }

        m_cInfinity.AIUpdate();
    }

    if (g_pBaldurChitin->nAUCounter % 2 == 0) {
        g_pBaldurChitin->cSoundMixer.UpdateSoundList();

        CGameAIArea* pAIArea;

        do {
            rc = m_pGame->GetObjectArray()->GetDeny(m_nAIIndex,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pAIArea),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED);

        if (rc == CGameObjectArray::SUCCESS) {
            pAIArea->ProcessAI();

            m_pGame->GetObjectArray()->ReleaseDeny(m_nAIIndex,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    INT nDeadCharacters = 0;
    for (SHORT nPortrait = 0; nPortrait < g_pBaldurChitin->GetObjectGame()->GetNumCharacters(); nPortrait++) {
        CGameSprite* pSprite;

        rc = m_pGame->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait),
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if ((pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) != 0) {
                nDeadCharacters++;
            }

            m_pGame->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait),
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (nDeadCharacters == g_pBaldurChitin->GetObjectGame()->GetNumCharacters()) {
        if (m_nCurrentSong != 1) {
            g_pBaldurChitin->cSoundMixer.StartSong(1, 0x4 | 0x1);
            m_nCurrentSong = 1;
        }
    } else if (m_pGame->GetVisibleArea() == this
        && g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineStore) {
        if (m_startedMusicCounter > 0) {
            m_startedMusicCounter--;
        }

        if (!m_startedMusic) {
            if (m_startedMusicCounter == 0) {
                m_startedMusic = TRUE;
                if (m_nCurrentSong != -1) {
                    INT nBattleSongCounter = m_nBattleSongCounter;
                    g_pBaldurChitin->cSoundMixer.StartSong(-1, 0x4 | 0x1);
                    SleepEx(500, FALSE);
                    PlaySong(m_nCurrentSong, 0x1);
                    m_nBattleSongCounter = nBattleSongCounter;
                } else {
                    g_pBaldurChitin->cSoundMixer.StartSong(-1, 0x4 | 0x1);
                }
            }
        } else {
            if (m_startedMusicCounter == 0) {
                if (m_nBattleSongCounter > 0) {
                    if (bActive) {
                        m_nBattleSongCounter--;
                    }

                    if (m_nBattleSongCounter > 0) {
                        if (m_nCurrentSong != 3) {
                            PlaySong(3, 0x2 | 0x1);
                        }
                    } else {
                        if ((m_header.m_flags & 0x4) == 0) {
                            if (m_nCurrentSong == 3) {
                                PlaySong(256, 0x2);
                            }
                        }
                    }
                }

                if ((m_header.m_flags & 0x4) == 0) {
                    if (m_pGame->GetVisibleArea() == this
                        && g_pChitin->cSoundMixer.m_nCurrentSong == -1) {
                        if (!g_pBaldurChitin->GetActiveEngine()->StopMusic()) {
                            m_nCurrentSong = -1;
                        }
                    }

                    if (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay()) {
                        PlaySong(-1, 0x4 | 0x2);
                        m_nCurrentSong = 0;
                        m_startedMusic = FALSE;
                        m_startedMusicCounter = 600;
                    } else if (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsNight()) {
                        PlaySong(-1, 0x4 | 0x2);
                        m_nCurrentSong = 1;
                        m_startedMusic = FALSE;
                        m_startedMusicCounter = 600;
                    }
                }
            }
        }
    }
}

// 0x46F750
void CGameArea::CompressTime(DWORD deltaTime)
{
    LONG iObject;
    CGameObject* pObject;
    POSITION pos;
    BYTE rc;

    pos = m_lVertSortFlight.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlight.GetPrev(pos)));

        do {
            rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Uninline.
            if (pObject->InControl()) {
                pObject->CompressTime(deltaTime);
            }

            m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_lVertSort.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(pos)));

        do {
            rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Uninline.
            if (pObject->InControl()) {
                pObject->CompressTime(deltaTime);
            }

            m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_lVertSortBack.GetTailPosition();
    while (pos != NULL) {
        iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetPrev(pos)));

        do {
            rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Uninline.
            if (pObject->InControl()) {
                pObject->CompressTime(deltaTime);
            }

            m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x46F930
void CGameArea::DebugDump(const CString& message, BOOLEAN bEchoToScreen)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    CString sTemp;

    if (bEchoToScreen) {
        pWorld->DisplayText(CString(""),
            CString("DEBUG DUMP: CGameArea"),
            -1,
            FALSE);

        pWorld->DisplayText(CString(""),
            message,
            -1,
            FALSE);

        sTemp.Format("Name: %.*s", RESREF_SIZE, m_resRef.GetResRef());
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Outdoor: %s", (m_header.m_areaType & 0x1) != 0 ? "TRUE" : "FALSE");
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Day/Night: %s", (m_header.m_areaType & 0x2) != 0 ? "TRUE" : "FALSE");
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Extended Night: %s", (m_header.m_areaType & 0x40) != 0 ? "TRUE" : "FALSE");
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Weather: %s", (m_header.m_areaType & 0x4) != 0 ? "TRUE" : "FALSE");
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        const char* areaType;
        if ((m_header.m_areaType & 0x8) != 0) {
            areaType = "CITY";
        } else if ((m_header.m_areaType & 0x10) != 0) {
            areaType = "FOREST";
        } else if ((m_header.m_areaType & 0x20) != 0) {
            areaType = "DUNGEON";
        } else {
            areaType = "NORMAL";
        }

        sTemp.Format("Area Type: %s", areaType);
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Rain Probability: %d%%", m_header.m_rainProbability);
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Snow Probability: %d%%", m_header.m_snowProbability);
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Fog Probability: %d%%", m_header.m_fogProbability);
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        sTemp.Format("Lightning Probability: %d%%", m_header.m_lightningProbability);
        pWorld->DisplayText(CString(""),
            sTemp,
            -1,
            FALSE);

        POSITION pos = m_entryPoints.GetHeadPosition();
        while (pos != NULL) {
            CAreaFileCharacterEntryPoint* pEntryPoint = m_entryPoints.GetNext(pos);
            sTemp.Format("    Entry Point: x=%d, y=%d, Scriptname=%.*s",
                pEntryPoint->m_startX,
                pEntryPoint->m_startY,
                SCRIPTNAME_SIZE,
                pEntryPoint->m_entryName);
            pWorld->DisplayText(CString(""),
                sTemp,
                -1,
                FALSE);
        }
    }
}

// 0x46FE30
void CGameArea::ClearInput()
{
    m_nToolTip = 0;
    m_iPicked = CGameObjectArray::INVALID_INDEX;
    m_pGame->m_tempCursor = 0;
    m_pGame->GetGroup()->GroupCancelMove();
    m_pGame->m_tempCursor = 4;
    m_groupMove = FALSE;
    m_selectSquare.left = -1;
    m_selectSquare.top = -1;
    m_selectSquare.right = -1;
    m_selectSquare.bottom = -1;
}

// 0x46FEA0
BOOL CGameArea::CheckRestEncounter()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (m_headerRestEncounter.m_activated == 0) {
        return FALSE;
    }

    // One chance, at the hour the rest starts: the day chance in the
    // dawn-to-night band of the 108000 ms day (27000 <= t < 99000: the dawn
    // tick is day, the `jae` against TIME_DAWN at 0x46FEF5-0x46FEFB), the night
    // chance otherwise. A miss returns FALSE - the retry counter starts at 0
    // and the back edge (`inc bl; cmp bl,1; jb` at 0x46FF39-0x46FF3E) is never
    // taken, so the +TIMESCALE_MSEC_PER_HOUR second roll below never happens
    // and only the binary's shape remains of it.
    INT nDelay = 0;
    BYTE nTries = 0;

    do {
        UINT nTime = (pGame->m_worldTime.m_gameTime + nDelay) % CTimerWorld::TIMESCALE_MSEC_PER_DAY;
        if (nTime >= CTimerWorld::TIME_DAWN && nTime < CTimerWorld::TIME_NIGHT) {
            if (rand() % 100 <= m_headerRestEncounter.m_probDay) {
                break;
            }
        } else {
            if (rand() % 100 <= m_headerRestEncounter.m_probNight) {
                break;
            }
        }

        nDelay += CTimerWorld::TIMESCALE_MSEC_PER_HOUR;
        nTries++;
    } while (nTries == 0);

    if (nTries == 1) {
        return FALSE;
    }

    if (m_headerRestEncounter.m_randomCreatureNum == 0) {
        return FALSE;
    }

    if (m_headerRestEncounter.m_maxTypeNum == 0) {
        return FALSE;
    }

    // NOTE: The original draws only when the hour's length is not zero.
    if (CTimerWorld::TIMESCALE_MSEC_PER_HOUR != 0) {
        nDelay += rand() % CTimerWorld::TIMESCALE_MSEC_PER_HOUR;
    }

    pGame->m_worldTime.AddCurrentTime(nDelay);
    pGame->FeedBack(CInfGame::FEEDBACK_TIMEPASSEDREST, nDelay, TRUE);

    CCreatureFile creatureFile;

    LONG nProtagonist = pGame->GetProtagonist();

    // The protagonist's position. NOTE: The original stops at nothing, so the
    // last party member's id is what the message below names (the scan never
    // breaks when it finds the protagonist), and a party with no protagonist
    // reads an unset stack point; ours starts at (0, 0).
    CPoint ptProtagonist(0, 0);
    LONG nSpeaker = CGameObjectArray::INVALID_INDEX;

    for (SHORT nSlot = 0; nSlot < pGame->m_nCharacters; nSlot++) {
        LONG nId = pGame->m_characterPortraits[nSlot];

        CGameSprite* pSprite;

        BYTE rc = pGame->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->m_id == nProtagonist) {
                ptProtagonist = pSprite->GetPos();
            }

            pGame->GetObjectArray()->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }

        nSpeaker = nId;
    }

    // Nine rays of up to m_visibility.field_E search squares out of the
    // protagonist's square (ray 4 is the square itself: skipped). A cell is a
    // candidate when it has line of sight and is visible; the farthest one
    // wins, and a tie needs a one-in-seven draw. The best ray is the index
    // into the group's formation offsets later.
    INT nRadius = m_visibility.field_E;
    INT nBaseX = ptProtagonist.x / CPathSearch::GRID_SQUARE_SIZEX;
    INT nBaseY = ptProtagonist.y / CPathSearch::GRID_SQUARE_SIZEY;

    INT nBestSteps = 0;
    INT nBestX = nBaseX;
    INT nBestY = nBaseY;
    BYTE nBestRay = 0;

    for (INT nRay = 0; nRay < 9; nRay++) {
        if (nRay == 4) {
            continue;
        }

        INT nDirX = nRay % 3 - 1;
        INT nDirY = nRay / 3 - 1;
        INT nX = nBaseX;
        INT nY = nBaseY;
        INT nSteps = 0;

        if (nRadius > 0) {
            do {
                nX += nDirX;
                nY += nDirY;

                if (nX < 0 || nX >= m_search.m_GridSquareDimensions.cx
                    || nY < 0 || nY >= m_search.m_GridSquareDimensions.cy) {
                    // Out of the search grid: the cell before this one is the
                    // candidate, and it counts one step less.
                    nX += 1 - nDirX;
                    nY += 1 - nDirY;
                    nSteps--;
                } else {
                    SHORT nTableIndex;
                    if (m_search.GetLOSCost(CPoint(nX, nY),
                            CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE,
                            nTableIndex,
                            FALSE)
                        == CPathSearch::COST_IMPASSABLE) {
                        // NOTE: The original tests the byte as -1.
                    } else if (!m_visibility.IsTileVisible(m_visibility.PointToTile(
                                   CPoint(nX * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                                       nY * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2)))) {
                        // Not visible: the cell before this one is the candidate.
                    } else {
                        nSteps++;
                        continue;
                    }
                }

                // A candidate: farther than the best so far wins, an equal one
                // needs a one-in-seven draw. Either way the ray ends here.
                BOOL bTake = FALSE;
                if (nSteps > nBestSteps) {
                    bTake = TRUE;
                } else if (nSteps == nBestSteps) {
                    bTake = rand() % 7 == 0;
                }

                if (bTake) {
                    nBestSteps = nSteps;
                    nBestX = nX;
                    nBestY = nY;
                    nBestRay = static_cast<BYTE>(nRay);
                }

                break;
            } while (nSteps < nRadius);
        }

        if (nSteps == nRadius) {
            // The whole ray was walked: its last cell is a candidate too.
            BOOL bTake = FALSE;
            if (nSteps > nBestSteps) {
                bTake = TRUE;
            } else if (nSteps == nBestSteps) {
                bTake = rand() % 7 == 0;
            }

            if (bTake) {
                nBestSteps = nSteps;
                nBestX = nX;
                nBestY = nY;
                nBestRay = static_cast<BYTE>(nRay);
            }
        }
    }

    INT nMode = rand() % 10;

    LONG nCreatureIndex = 0;
    if (m_headerRestEncounter.m_randomCreatureNum != 0) {
        nCreatureIndex = rand() % m_headerRestEncounter.m_randomCreatureNum;
    }

    INT nCount = m_headerRestEncounter.m_maxTypeNum + rand() % 5 - 2;
    if (nCount < 1) {
        nCount = 1;
    }

    if (pGame->m_cOptions.m_nNightmareMode == 1) {
        nCount++;
    }

    CPoint* pOffsets = NULL;

    for (INT nCreature = 0; nCreature < nCount; nCreature++) {
        CResRef resRef(reinterpret_cast<const char*>(m_headerRestEncounter.m_randomCreature[nCreatureIndex]));

        // NOTE: Uninline.
        creatureFile.SetResRef(resRef, TRUE, TRUE);

        DWORD nSize = creatureFile.GetDataSize();
        BYTE* pData = creatureFile.GetData();

        CGameSprite* pSprite = new CGameSprite(pData,
            nSize,
            0,
            1,
            m_headerRestEncounter.m_lifeSpan * CTimerWorld::TIMESCALE_MSEC_PER_SEC + pGame->m_worldTime.m_gameTime,
            m_headerRestEncounter.m_huntingRange,
            m_headerRestEncounter.m_followRange,
            0x7FFFFFFF,
            ptProtagonist,
            0);

        // NOTE: Uninline.
        pSprite->SetResRef(CResRef(creatureFile.GetResRef().GetResRefStr()));

        creatureFile.ReleaseData();

        if (pSprite == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 3611
            UTIL_ASSERT(FALSE);
        } else {
            pSprite->SetResRef(resRef);

            // Up to five tries at a free, visible square. NOTE: The original
            // formats "Spawned creatures dropped due to invalid positions: %d
            // of %d" into a 200-byte buffer and frees it without printing
            // (0x4708D7-0x4708F3); not ported.
            BOOL bPlaced = FALSE;
            CPoint ptSpawn;

            for (INT nTry = 0; nTry < 5 && !bPlaced; nTry++) {
                INT nSquareX;
                INT nSquareY;

                if (nMode != 0) {
                    if (nCreature == 0) {
                        // NOTE: The original calls rand before GetDirection.
                        INT nFormation = rand() % 9 + 1;
                        SHORT nDirection = static_cast<SHORT>(pSprite->GetDirection(ptProtagonist) * 360 / 16 % 360);
                        pOffsets = pGame->m_group.GetOffsets(static_cast<SHORT>(nFormation), nDirection, TRUE);
                    }

                    // NOTE: The original indexes the offsets with
                    // `nBestRay % FORMATION_MAX_SIZE` but GetOffsets gave it
                    // max(members, 6) of them, so a party of six or fewer reads
                    // past the heap block; bounded as CGameSpawning::Spawn.
                    INT nMembers = pGame->m_group.m_memberList.GetCount();
                    INT nOffsets = nMembers < 7 ? 6 : static_cast<BYTE>(nMembers);
                    INT nOffset = nBestRay % CAIGROUP_FORMATION_MAX_SIZE;
                    CPoint ptOffset(0, 0);
                    if (nOffset < nOffsets) {
                        ptOffset = pOffsets[nOffset];
                    }

                    CPoint ptStart(ptOffset.x / CAIGroup::OFFSET_MULTIPLIER + nBestX,
                        ptOffset.y / CAIGroup::OFFSET_MULTIPLIER + nBestY);

                    CPoint ptSquare = m_search.GetNearestOpenSquare(ptStart,
                        pSprite->GetTerrainTable(),
                        pSprite->m_animation.GetPersonalSpace(),
                        -1);

                    nSquareX = ptSquare.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2;
                    nSquareY = ptSquare.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2;
                } else {
                    // The rare path: the best square itself, nudged back
                    // towards the protagonist's square half the time.
                    CPoint ptSquare = m_search.GetNearestOpenSquare(CPoint(nBestX, nBestY),
                        pSprite->GetTerrainTable(),
                        pSprite->m_animation.GetPersonalSpace(),
                        -1);

                    nSquareX = ptSquare.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2;
                    nSquareY = ptSquare.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2;

                    if (rand() % 2 == 0) {
                        nBestX += 1 - nBestRay % 3;
                        nBestY += 1 - nBestRay / 3;
                    }
                }

                ptSpawn = CPoint(nSquareX, nSquareY);

                if (m_visibility.PointToTile(ptSpawn) >= m_visibility.m_nWidth * m_visibility.m_nHeight) {
                    // Off the visibility map: try again.
                    continue;
                }

                // NOTE: The original asserts the animation here (0x826).
                pSprite->AddToArea(this, ptSpawn, 0, pSprite->m_animation.GetListType());
                pSprite->SetFacing(pSprite->GetDirection(ptProtagonist));

                bPlaced = TRUE;
            }
        }
    }

    if (pOffsets != NULL) {
        // NOTE: The original frees it with scalar operator delete; ours
        // matches GetOffsets' `new[]`.
        delete[] pOffsets;
    }

    g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
        m_headerRestEncounter.m_randomCreatureString[nCreatureIndex],
        0,
        0x590A5,
        -1,
        nSpeaker,
        nSpeaker);

    return TRUE;
}

// 0x470960
void CGameArea::ClearMarshal()
{
    LONG iObject;
    CGameObject* pObject;
    BYTE rc;

    EnterCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
    g_pBaldurChitin->GetObjectGame()->BeginListManipulation(this);
    EnterCriticalSection(&field_1FC);

    if (m_pResWED != NULL) {
        m_nCharacters = -1;

        while (!m_lVertSortFlight.IsEmpty()) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlight.RemoveHead()));

            do {
                rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pObject->IsProjectile()) {
                    static_cast<CProjectile*>(pObject)->RemoveSelf();
                } else {
                    pObject->RemoveFromArea();
                }

                m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        while (!m_lVertSort.IsEmpty()) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.RemoveHead()));

            do {
                rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pObject->IsProjectile()) {
                    static_cast<CProjectile*>(pObject)->RemoveSelf();
                } else {
                    pObject->RemoveFromArea();
                }

                m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        while (!m_lVertSortBack.IsEmpty()) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.RemoveHead()));

            do {
                rc = m_pGame->GetObjectArray()->GetDeny(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pObject->IsProjectile()) {
                    static_cast<CProjectile*>(pObject)->RemoveSelf();
                } else {
                    pObject->RemoveFromArea();
                }

                m_pGame->GetObjectArray()->ReleaseDeny(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        m_lVertSortFlightRemove.RemoveAll();
        m_lVertSortRemove.RemoveAll();
        m_lVertSortBackRemove.RemoveAll();

        m_nCharacters = 0;

        m_cInfinity.FreeWED();
        g_pChitin->cDimm.ReleaseResObject(m_pResWED);
        m_pResWED = NULL;
    }

    while (m_bInPathSearch) {
        Sleep(1);
    }

    m_sndAmbientDay.Stop();
    m_sndAmbientNight.Stop();
    m_visibility.Uninit();
    m_search.Uninit();
    m_bmHeight.GetRes()->Release();
    m_bmLum.GetRes()->Release();

    if (m_pbmLumNight != NULL) {
        m_pbmLumNight->GetRes()->Release();
        delete m_pbmLumNight;
        m_pbmLumNight = NULL;
    }

    if (m_visibility.m_pDynamicHeight != NULL) {
        delete m_visibility.m_pDynamicHeight;
        m_visibility.m_pDynamicHeight = NULL;
    }

    POSITION pos = m_entryPoints.GetHeadPosition();
    while (pos != NULL) {
        CAreaFileCharacterEntryPoint* pEntryPoint = m_entryPoints.GetNext(pos);
        delete pEntryPoint;
    }
    m_entryPoints.RemoveAll();

    m_bAreaLoaded = FALSE;

    LeaveCriticalSection(&field_1FC);
    g_pBaldurChitin->GetObjectGame()->EndListManipulation(this);
    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
}

// 0x470D20
BOOLEAN CGameArea::GetEntryPoint(const CString& sName, CPoint& ptEnter, SHORT& facing)
{
    POSITION pos = m_entryPoints.GetHeadPosition();
    while (pos != NULL) {
        CAreaFileCharacterEntryPoint* pEntryPoint = m_entryPoints.GetNext(pos);
        if (_strnicmp(pEntryPoint->m_entryName, sName, SCRIPTNAME_SIZE) == 0) {
            ptEnter.x = pEntryPoint->m_startX;
            ptEnter.y = pEntryPoint->m_startY;
            facing = static_cast<SHORT>(pEntryPoint->m_facing);
            return TRUE;
        }
    }
    return FALSE;
}

// 0x470D80
int CGameArea::GetHeightOffset(const CPoint& pt, BYTE listType)
{
    BYTE value;
    int nOffset = 0;

    if (listType != CGameObject::LIST_FLIGHT
        && m_bmHeight.GetRes() != NULL) {
        m_bmHeight.GetPixelValue(value, pt.x / 16, pt.y / 12, TRUE);
        nOffset = 8 - value;
        if (nOffset >= 7) {
            nOffset = 7;
        } else if (nOffset <= -7) {
            nOffset = -7;
        }

        if (nOffset == CGameObject::LIST_FRONT) {
            int index = (m_search.m_GridSquareDimensions.cx * (pt.y / 12) + pt.x / 16) / 2;
            if ((m_visibility.m_pDynamicHeight[index] & 0xF0) != 0) {
                nOffset -= 7;
            }
            if (nOffset <= -7) {
                nOffset = -7;
            }
        }
    }

    return nOffset;
}

// 0x470E60
void CGameArea::IncrHeightDynamic(const CPoint& point)
{
    int index = (m_search.m_GridSquareDimensions.cx * (point.y / 12) + point.x / 16) / 2;
    if ((m_visibility.m_pDynamicHeight[index] & 0xF0) < 0xF) {
        m_visibility.m_pDynamicHeight[index] += 16;
    }
}

// 0x470EC0
void CGameArea::DecrHeightDynamic(const CPoint& point)
{
    int index = (m_search.m_GridSquareDimensions.cx * (point.y / 12) + point.x / 16) / 2;
    if ((m_visibility.m_pDynamicHeight[index] & 0xF0) != 0) {
        m_visibility.m_pDynamicHeight[index] -= 16;
    }
}

// The light of a point: the area's light map (and its night one) sampled at
// the four corners of the 16 by 12 cell the point is in and interpolated.
//
// 0x470F10
COLORREF CGameArea::GetTintColor(const CPoint& cPoint, BYTE listType)
{
    // NOTE: The day bits of the BYTEs at 0x851930 (1) and 0x851931 (2).
    const BYTE DAY = 1;
    const BYTE NIGHT = 2;

    INT nFracX = cPoint.x % 16;
    INT nFracY = cPoint.y % 12;
    BYTE nCode = DAY;

    if (listType == CGameObject::LIST_FLIGHT) {
        return RGB(255, 255, 255);
    }

    if (m_bmLum.pRes == NULL) {
        return RGB(255, 255, 255);
    }

    if ((m_cInfinity.m_areaType & 0x40) != 0) {
        nCode = m_cInfinity.m_renderDayNightCode;
    }

    INT nX = cPoint.x / 16;
    INT nY = cPoint.y / 12;

    // NOTE: The original leaves the corners on its stack as they are when it
    // samples neither map (a code with neither bit); ours starts them at 0.
    // GetPixelColor answers TRUE for a loaded map, so the fallbacks to the
    // first corner below never run.
    RGBQUAD corners[4] = {};
    if ((nCode & DAY) != 0) {
        m_bmLum.GetPixelColor(corners[0], nX, nY, TRUE);
        if (!m_bmLum.GetPixelColor(corners[1], nX + 1, nY, TRUE)) {
            corners[1] = corners[0];
        }
        if (!m_bmLum.GetPixelColor(corners[2], nX, nY + 1, TRUE)) {
            corners[2] = corners[0];
        }
        if (!m_bmLum.GetPixelColor(corners[3], nX + 1, nY + 1, TRUE)) {
            corners[3] = corners[0];
        }
    }

    if ((nCode & NIGHT) != 0) {
        if (m_pbmLumNight == NULL) {
            // NOTE: The original calls through a NULL video mode when no
            // engine is active.
            return g_pBaldurChitin->GetActiveEngine()->pVidMode->GetGlobalTintColor();
        }

        RGBQUAD night[4] = {};
        m_pbmLumNight->GetPixelColor(night[0], nX, nY, TRUE);
        if (!m_pbmLumNight->GetPixelColor(night[1], nX + 1, nY, TRUE)) {
            night[1] = night[0];
        }
        if (!m_pbmLumNight->GetPixelColor(night[2], nX, nY + 1, TRUE)) {
            night[2] = night[0];
        }
        if (!m_pbmLumNight->GetPixelColor(night[3], nX + 1, nY + 1, TRUE)) {
            night[3] = night[0];
        }

        if ((nCode & DAY) != 0) {
            // Both: each corner's day and night light mixed by the daylight
            // intensity.
            INT nDay = m_cInfinity.m_dayLightIntensity;
            INT nNight = static_cast<BYTE>(0xFF - m_cInfinity.m_dayLightIntensity);
            for (INT n = 0; n < 4; n++) {
                corners[n].rgbRed = static_cast<BYTE>((corners[n].rgbRed * nDay + night[n].rgbRed * nNight) >> 8);
                corners[n].rgbGreen = static_cast<BYTE>((corners[n].rgbGreen * nDay + night[n].rgbGreen * nNight) >> 8);
                corners[n].rgbBlue = static_cast<BYTE>((corners[n].rgbBlue * nDay + night[n].rgbBlue * nNight) >> 8);
            }
        } else {
            for (INT n = 0; n < 4; n++) {
                corners[n] = night[n];
            }
        }
    }

    // Across the cell: the top and bottom edges in sixteenths, then down in
    // twelfths (each division truncated).
    auto lerp = [&](BYTE c0, BYTE c1, BYTE c2, BYTE c3) -> BYTE {
        INT nTop = (c0 * (16 - nFracX) + c1 * nFracX) / 16;
        INT nBottom = (c2 * (16 - nFracX) + c3 * nFracX) / 16;
        return static_cast<BYTE>((nTop * (12 - nFracY) + nBottom * nFracY) / 12);
    };

    BYTE nRed = lerp(corners[0].rgbRed, corners[1].rgbRed, corners[2].rgbRed, corners[3].rgbRed);
    BYTE nGreen = lerp(corners[0].rgbGreen, corners[1].rgbGreen, corners[2].rgbGreen, corners[3].rgbGreen);
    BYTE nBlue = lerp(corners[0].rgbBlue, corners[1].rgbBlue, corners[2].rgbBlue, corners[3].rgbBlue);
    return RGB(nRed, nGreen, nBlue);
}

// 0x471630
void CGameArea::Marshal(BYTE** pArea, DWORD* nArea, BOOLEAN bProgressBarInPlace)
{
    CTypedPtrList<CPtrList, CAreaFileStaticObject*> lStatics;
    CTypedPtrList<CPtrList, CAreaFileSoundObject*> lSounds;
    CTypedPtrList<CPtrList, CAreaFileRandomMonsterSpawningPoint*> lSpawnings;
    CTypedPtrList<CPtrList, CAreaFileContainer*> lContainers;
    CTypedPtrList<CPtrList, CAreaFileTriggerObject*> lTriggers;
    CTypedPtrList<CPtrList, CAreaFileDoorObject*> lDoors;
    CTypedPtrList<CPtrList, CAreaFileCreature*> lCreatures;
    CAreaFileStaticObject* pStaticObject;
    CAreaFileSoundObject* pSoundObject;
    CAreaFileRandomMonsterSpawningPoint* pSpawningObject;
    CAreaFileContainer* pContainerObject;
    CAreaFileTriggerObject* pTriggerObject;
    CAreaFileDoorObject* pDoorObject;
    CAreaFileCreature* pCreature;
    CGameObject* pObject;
    POSITION pos;
    LONG nObjectId;
    DWORD nObjects;
    DWORD dwProgressUnit = 0;
    DWORD nCreatureData;
    WORD nPoints;
    DWORD nItems;
    BYTE* pVisibility;
    DWORD nVisibility;
    CVariable* pVariables;
    WORD nVariables;
    DWORD cnt;
    DWORD nIndex;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 4239
    UTIL_ASSERT(pArea != NULL && nArea != NULL);

    nPoints = 0;
    nItems = 0;
    nCreatureData = 0;

    field_AAA.RemoveAll();

    if (g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster == this) {
        // The original fills this area's list again right after emptying it,
        // so the sprites of pending moves go into this area's creature list.
        g_pBaldurChitin->GetObjectGame()->m_cMoveList.Marshal(this);
        g_pBaldurChitin->GetObjectGame()->m_cLimboList.Marshal(this);
    }

    if (bProgressBarInPlace == TRUE) {
        dwProgressUnit = CInfGame::PROGRESSBAR_AREA_ADDITIONAL / 25;
        ProgressBarCallback(0, TRUE);
    }

    *pArea = NULL;
    *nArea = 0;

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !g_pChitin->cNetwork.GetSessionHosting()) {
        ProgressBarCallback(CInfGame::PROGRESSBAR_AREA_ADDITIONAL, FALSE);
        return;
    }

    // FLIGHT LIST (sounds, spawning points, creatures)
    nObjects = m_lVertSortFlight.GetCount();
    pos = m_lVertSortFlight.GetTailPosition();
    while (pos != NULL) {
        nObjectId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlight.GetPrev(pos)));
        if (m_pGame->GetObjectArray()->GetShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            switch (pObject->GetObjectType()) {
            case 0x00: // CGameObject::TYPE_NONE
            case 0x50: // CGameObject::TYPE_TEXT
                break;
            case 0x01: // CGameObject::TYPE_AIBASE
            case 0x11: // CGameObject::TYPE_CONTAINER
            case 0x21: // CGameObject::TYPE_DOOR
            case 0x30: // CGameObject::TYPE_STATIC
            case 0x40: // CGameObject::TYPE_OBJECT_MARKER
            case 0x41: // CGameObject::TYPE_TRIGGER
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4317
                UTIL_ASSERT(FALSE);
                break;
            case 0x10: // CGameObject::TYPE_SOUND
                static_cast<CGameSound*>(pObject)->Marshal(&pSoundObject);
                lSounds.AddTail(pSoundObject);
                break;
            case 0x20: // CGameObject::TYPE_SPAWNING
                static_cast<CGameSpawning*>(pObject)->Marshal(&pSpawningObject);
                lSpawnings.AddTail(pSpawningObject);
                break;
            case 0x31: // CGameObject::TYPE_SPRITE
                static_cast<CGameSprite*>(pObject)->Marshal(&pCreature);
                lCreatures.AddTail(pCreature);
                nCreatureData += pCreature->m_creatureSize;
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4337
                UTIL_ASSERT(FALSE);
                break;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (bProgressBarInPlace == TRUE && nObjects != 0) {
                ProgressBarCallback(4 * dwProgressUnit / nObjects, FALSE);
            }
        }
    }

    if (bProgressBarInPlace == TRUE && nObjects == 0) {
        ProgressBarCallback(4 * dwProgressUnit, FALSE);
    }

    // FRONT LIST (doors, statics, creatures, triggers)
    nObjects = m_lVertSort.GetCount();
    pos = m_lVertSort.GetTailPosition();
    while (pos != NULL) {
        nObjectId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(pos)));
        if (m_pGame->GetObjectArray()->GetShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            switch (pObject->GetObjectType()) {
            case 0x00: // CGameObject::TYPE_NONE
            case 0x01: // CGameObject::TYPE_AIBASE
            case 0x50: // CGameObject::TYPE_TEXT
                break;
            case 0x10: // CGameObject::TYPE_SOUND
            case 0x11: // CGameObject::TYPE_CONTAINER
            case 0x20: // CGameObject::TYPE_SPAWNING
            case 0x40: // CGameObject::TYPE_OBJECT_MARKER
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4372
                UTIL_ASSERT(FALSE);
                break;
            case 0x21: // CGameObject::TYPE_DOOR
                static_cast<CGameDoor*>(pObject)->Marshal(&pDoorObject);
                lDoors.AddTail(pDoorObject);
                nPoints += pDoorObject->m_closedSearchSquaresCount
                    + pDoorObject->m_openSearchSquaresCount
                    + pDoorObject->m_closedSelectionPointCount
                    + pDoorObject->m_openSelectionPointCount;
                break;
            case 0x30: // CGameObject::TYPE_STATIC
                static_cast<CGameStatic*>(pObject)->Marshal(&pStaticObject);
                lStatics.AddTail(pStaticObject);
                break;
            case 0x31: // CGameObject::TYPE_SPRITE
                if (!static_cast<CGameSprite*>(pObject)->m_bGlobal) {
                    static_cast<CGameSprite*>(pObject)->Marshal(&pCreature);
                    lCreatures.AddTail(pCreature);
                    nCreatureData += pCreature->m_creatureSize;
                }
                break;
            case 0x41: // CGameObject::TYPE_TRIGGER
                static_cast<CGameTrigger*>(pObject)->Marshal(&pTriggerObject);
                lTriggers.AddTail(pTriggerObject);
                nPoints += pTriggerObject->m_pickPointCount;
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4405
                UTIL_ASSERT(FALSE);
                break;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (bProgressBarInPlace == TRUE && nObjects != 0) {
                ProgressBarCallback(4 * dwProgressUnit / nObjects, FALSE);
            }
        }
    }

    if (bProgressBarInPlace == TRUE && nObjects == 0) {
        ProgressBarCallback(4 * dwProgressUnit, FALSE);
    }

    nObjects = field_AAA.GetCount();
    pos = field_AAA.GetTailPosition();
    while (pos != NULL) {
        nObjectId = static_cast<LONG>(reinterpret_cast<INT_PTR>(field_AAA.GetPrev(pos)));
        if (m_pGame->GetObjectArray()->GetDeny(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            switch (pObject->GetObjectType()) {
            case 0x00: // CGameObject::TYPE_NONE
            case 0x01: // CGameObject::TYPE_AIBASE
            case 0x50: // CGameObject::TYPE_TEXT
                break;
            case 0x10: // CGameObject::TYPE_SOUND
            case 0x11: // CGameObject::TYPE_CONTAINER
            case 0x20: // CGameObject::TYPE_SPAWNING
            case 0x21: // CGameObject::TYPE_DOOR
            case 0x30: // CGameObject::TYPE_STATIC
            case 0x40: // CGameObject::TYPE_OBJECT_MARKER
            case 0x41: // CGameObject::TYPE_TRIGGER
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4444
                UTIL_ASSERT(FALSE);
                break;
            case 0x31: // CGameObject::TYPE_SPRITE
                if (!static_cast<CGameSprite*>(pObject)->m_bGlobal) {
                    static_cast<CGameSprite*>(pObject)->Marshal(&pCreature);
                    lCreatures.AddTail(pCreature);
                    nCreatureData += pCreature->m_creatureSize;
                }
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4457
                UTIL_ASSERT(FALSE);
                break;
            }

            m_pGame->GetObjectArray()->ReleaseDeny(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (bProgressBarInPlace == TRUE && nObjects != 0) {
                ProgressBarCallback(4 * dwProgressUnit / nObjects, FALSE);
            }
        }
    }

    field_AAA.RemoveAll();

    // BACK LIST (containers, sounds, spawning points, creatures, triggers)
    nObjects = m_lVertSortBack.GetCount();
    pos = m_lVertSortBack.GetTailPosition();
    while (pos != NULL) {
        nObjectId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetPrev(pos)));
        if (m_pGame->GetObjectArray()->GetShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            switch (pObject->GetObjectType()) {
            case 0x00: // CGameObject::TYPE_NONE
            case 0x01: // CGameObject::TYPE_AIBASE
            case 0x40: // CGameObject::TYPE_OBJECT_MARKER
            case 0x50: // CGameObject::TYPE_TEXT
                break;
            case 0x21: // CGameObject::TYPE_DOOR
            case 0x30: // CGameObject::TYPE_STATIC
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4488
                UTIL_ASSERT(FALSE);
                break;
            case 0x10: // CGameObject::TYPE_SOUND
                static_cast<CGameSound*>(pObject)->Marshal(&pSoundObject);
                lSounds.AddTail(pSoundObject);
                break;
            case 0x11: // CGameObject::TYPE_CONTAINER
                static_cast<CGameContainer*>(pObject)->Marshal(&pContainerObject);
                lContainers.AddTail(pContainerObject);
                nPoints += pContainerObject->m_pickPointCount;
                nItems += pContainerObject->m_itemCount;
                break;
            case 0x20: // CGameObject::TYPE_SPAWNING
                static_cast<CGameSpawning*>(pObject)->Marshal(&pSpawningObject);
                lSpawnings.AddTail(pSpawningObject);
                break;
            case 0x31: // CGameObject::TYPE_SPRITE
                if (!static_cast<CGameSprite*>(pObject)->m_bGlobal) {
                    static_cast<CGameSprite*>(pObject)->Marshal(&pCreature);
                    lCreatures.AddTail(pCreature);
                    nCreatureData += pCreature->m_creatureSize;
                }
                break;
            case 0x41: // CGameObject::TYPE_TRIGGER
                static_cast<CGameTrigger*>(pObject)->Marshal(&pTriggerObject);
                lTriggers.AddTail(pTriggerObject);
                nPoints += pTriggerObject->m_pickPointCount;
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 4524
                UTIL_ASSERT(FALSE);
                break;
            }

            m_pGame->GetObjectArray()->ReleaseShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (bProgressBarInPlace == TRUE && nObjects != 0) {
                ProgressBarCallback(4 * dwProgressUnit / nObjects, FALSE);
            }
        }
    }

    if (bProgressBarInPlace == TRUE && nObjects == 0) {
        ProgressBarCallback(4 * dwProgressUnit, FALSE);
    }

    pVisibility = NULL;
    nVisibility = 0;
    m_visibility.Marshal(&pVisibility, &nVisibility);

    pVariables = NULL;
    nVariables = 0;
    m_variables.Marshal(&pVariables, &nVariables);

    *nArea += nCreatureData
        + nVisibility
        + sizeof(CCreatureFileItem) * nItems
        + sizeof(CAreaFileDoorObject) * lDoors.GetCount()
        + sizeof(CAreaFileRandomMonsterSpawningPoint) * lSpawnings.GetCount()
        + sizeof(CVariable) * nVariables
        + sizeof(CAreaFileCreature) * lCreatures.GetCount()
        + sizeof(CAreaFileTriggerObject) * lTriggers.GetCount()
        + sizeof(CAreaFileContainer) * lContainers.GetCount()
        + sizeof(CAreaFileSoundObject) * lSounds.GetCount()
        + sizeof(CAreaFileStaticObject) * lStatics.GetCount()
        + sizeof(CAreaPoint) * nPoints;
    *nArea += 8 + sizeof(CAreaFileHeader) + sizeof(CAreaFileOffsets) + sizeof(CAreaSoundsAndMusic) + sizeof(CAreaFileRestEncounter)
        + sizeof(CAreaFileCharacterEntryPoint) * m_entryPoints.GetCount();
    *nArea += sizeof(CAreaUserNote) * m_cGameAreaNotes.m_areaNoteList.GetCount();

    *pArea = new BYTE[*nArea];

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 4562
    UTIL_ASSERT(*pArea != NULL);

    memset(*pArea, 0, *nArea);

    cnt = 0;
    memcpy(*pArea, "AREAV9.1", 8);
    cnt += 8;

    m_header.m_lastSaved = m_pGame->GetWorldTimer()->m_gameTime / CTimerWorld::TIMESCALE_MSEC_PER_SEC;
    memcpy(*pArea + cnt, &m_header, sizeof(CAreaFileHeader));
    cnt += sizeof(CAreaFileHeader);

    CAreaFileOffsets* pOffsets = reinterpret_cast<CAreaFileOffsets*>(*pArea + cnt);
    cnt += sizeof(CAreaFileOffsets);

    memcpy(*pArea + cnt, &m_headerSound, sizeof(CAreaSoundsAndMusic));
    pOffsets->m_areaSoundsAndMusicOffset = cnt;
    cnt += sizeof(CAreaSoundsAndMusic);

    memcpy(*pArea + cnt, &m_headerRestEncounter, sizeof(CAreaFileRestEncounter));
    pOffsets->m_restingEncounterOffset = cnt;
    cnt += sizeof(CAreaFileRestEncounter);

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // VISIBILITY
    if (nVisibility > 0) {
        memcpy(*pArea + cnt, pVisibility, nVisibility);
        pOffsets->m_visibilityMapOffset = cnt;
        pOffsets->m_visibilityMapCount = nVisibility;
        cnt += nVisibility;
        delete[] pVisibility;
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // STATICS
    if (lStatics.GetCount() > 0) {
        pOffsets->m_staticObjectListOffset = cnt;
        pOffsets->m_staticObjectListCount = lStatics.GetCount();

        pos = lStatics.GetHeadPosition();
        while (pos != NULL) {
            pStaticObject = lStatics.GetNext(pos);
            memcpy(*pArea + cnt, pStaticObject, sizeof(CAreaFileStaticObject));
            cnt += sizeof(CAreaFileStaticObject);
            delete pStaticObject;
        }

        lStatics.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // SOUNDS
    if (lSounds.GetCount() > 0) {
        pOffsets->m_soundObjectOffset = cnt;
        pOffsets->m_soundObjectCount = static_cast<WORD>(lSounds.GetCount());

        pos = lSounds.GetHeadPosition();
        while (pos != NULL) {
            pSoundObject = lSounds.GetNext(pos);
            memcpy(*pArea + cnt, pSoundObject, sizeof(CAreaFileSoundObject));
            cnt += sizeof(CAreaFileSoundObject);
            delete pSoundObject;
        }

        lSounds.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // SPAWNING POINTS
    if (lSpawnings.GetCount() > 0) {
        pOffsets->m_randomMonsterSpawningPointTableOffset = cnt;
        pOffsets->m_randomMonsterSpawningPointTableCount = lSpawnings.GetCount();

        pos = lSpawnings.GetHeadPosition();
        while (pos != NULL) {
            pSpawningObject = lSpawnings.GetNext(pos);
            memcpy(*pArea + cnt, pSpawningObject, sizeof(CAreaFileRandomMonsterSpawningPoint));
            cnt += sizeof(CAreaFileRandomMonsterSpawningPoint);
            delete pSpawningObject;
        }

        lSpawnings.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // CREATURES
    if (lCreatures.GetCount() > 0) {
        pos = lCreatures.GetHeadPosition();
        while (pos != NULL) {
            pCreature = lCreatures.GetNext(pos);

            // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
            // where the original stores the pointer. A handle the table does
            // not know gives NULL, and the bytes then stay zero.
            BYTE* pCreatureData = static_cast<BYTE*>(iwd2r::UnmarshalPtr(pCreature->m_creatureOffset));
            if (pCreatureData != NULL) {
                memcpy(*pArea + cnt, pCreatureData, pCreature->m_creatureSize);
            }
            delete[] pCreatureData;

            pCreature->m_creatureOffset = cnt;
            cnt += pCreature->m_creatureSize;
        }

        pOffsets->m_creatureTableOffset = cnt;
        pOffsets->m_creatureTableCount = static_cast<WORD>(lCreatures.GetCount());

        pos = lCreatures.GetHeadPosition();
        while (pos != NULL) {
            pCreature = lCreatures.GetNext(pos);
            memcpy(*pArea + cnt, pCreature, sizeof(CAreaFileCreature));
            cnt += sizeof(CAreaFileCreature);
            delete pCreature;
        }

        lCreatures.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // POINTS
    if (nPoints > 0) {
        pOffsets->m_pointsOffset = cnt;
        pOffsets->m_pointsCount = nPoints;

        nIndex = 0;

        pos = lContainers.GetHeadPosition();
        while (pos != NULL) {
            pContainerObject = lContainers.GetNext(pos);
            if (pContainerObject->m_pickPointCount > 0) {
                // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
                // where the original stores the pointer. A handle the table does
                // not know gives NULL, and the bytes then stay zero.
                CAreaPoint* pPoints = static_cast<CAreaPoint*>(iwd2r::UnmarshalPtr(pContainerObject->m_pickPointStart));
                if (pPoints != NULL) {
                    memcpy(*pArea + cnt, pPoints, sizeof(CAreaPoint) * pContainerObject->m_pickPointCount);
                }
                delete[] pPoints;
                pContainerObject->m_pickPointStart = nIndex;
                cnt += sizeof(CAreaPoint) * pContainerObject->m_pickPointCount;
                nIndex += pContainerObject->m_pickPointCount;
            }
        }

        pos = lDoors.GetHeadPosition();
        while (pos != NULL) {
            pDoorObject = lDoors.GetNext(pos);

            // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
            // where the original stores the pointer. A handle the table does
            // not know gives NULL, and the bytes then stay zero.
            if (pDoorObject->m_openSelectionPointCount > 0) {
                CAreaPoint* pPoints = static_cast<CAreaPoint*>(iwd2r::UnmarshalPtr(pDoorObject->m_openSelectionPointStart));
                if (pPoints != NULL) {
                    memcpy(*pArea + cnt, pPoints, sizeof(CAreaPoint) * pDoorObject->m_openSelectionPointCount);
                }
                delete[] pPoints;
                pDoorObject->m_openSelectionPointStart = nIndex;
                cnt += sizeof(CAreaPoint) * pDoorObject->m_openSelectionPointCount;
                nIndex += pDoorObject->m_openSelectionPointCount;
            }

            if (pDoorObject->m_closedSelectionPointCount > 0) {
                CAreaPoint* pPoints = static_cast<CAreaPoint*>(iwd2r::UnmarshalPtr(pDoorObject->m_closedSelectionPointStart));
                if (pPoints != NULL) {
                    memcpy(*pArea + cnt, pPoints, sizeof(CAreaPoint) * pDoorObject->m_closedSelectionPointCount);
                }
                delete[] pPoints;
                pDoorObject->m_closedSelectionPointStart = nIndex;
                cnt += sizeof(CAreaPoint) * pDoorObject->m_closedSelectionPointCount;
                nIndex += pDoorObject->m_closedSelectionPointCount;
            }

            if (pDoorObject->m_openSearchSquaresCount > 0) {
                CAreaPoint* pPoints = static_cast<CAreaPoint*>(iwd2r::UnmarshalPtr(pDoorObject->m_openSearchSquaresStart));
                if (pPoints != NULL) {
                    memcpy(*pArea + cnt, pPoints, sizeof(CAreaPoint) * pDoorObject->m_openSearchSquaresCount);
                }
                delete[] pPoints;
                pDoorObject->m_openSearchSquaresStart = nIndex;
                cnt += sizeof(CAreaPoint) * pDoorObject->m_openSearchSquaresCount;
                nIndex += pDoorObject->m_openSearchSquaresCount;
            }

            if (pDoorObject->m_closedSearchSquaresCount > 0) {
                CAreaPoint* pPoints = static_cast<CAreaPoint*>(iwd2r::UnmarshalPtr(pDoorObject->m_closedSearchSquaresStart));
                if (pPoints != NULL) {
                    memcpy(*pArea + cnt, pPoints, sizeof(CAreaPoint) * pDoorObject->m_closedSearchSquaresCount);
                }
                delete[] pPoints;
                pDoorObject->m_closedSearchSquaresStart = nIndex;
                cnt += sizeof(CAreaPoint) * pDoorObject->m_closedSearchSquaresCount;
                nIndex += pDoorObject->m_closedSearchSquaresCount;
            }
        }

        pos = lTriggers.GetHeadPosition();
        while (pos != NULL) {
            pTriggerObject = lTriggers.GetNext(pos);
            if (pTriggerObject->m_pickPointCount > 0) {
                // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
                // where the original stores the pointer. A handle the table does
                // not know gives NULL, and the bytes then stay zero.
                CAreaPoint* pPoints = static_cast<CAreaPoint*>(iwd2r::UnmarshalPtr(pTriggerObject->m_pickPointStart));
                if (pPoints != NULL) {
                    memcpy(*pArea + cnt, pPoints, sizeof(CAreaPoint) * pTriggerObject->m_pickPointCount);
                }
                delete[] pPoints;
                pTriggerObject->m_pickPointStart = nIndex;
                cnt += sizeof(CAreaPoint) * pTriggerObject->m_pickPointCount;
                nIndex += pTriggerObject->m_pickPointCount;
            }
        }
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // DOORS
    if (lDoors.GetCount() > 0) {
        pOffsets->m_doorObjectListOffset = cnt;
        pOffsets->m_doorObjectListCount = lDoors.GetCount();

        pos = lDoors.GetHeadPosition();
        while (pos != NULL) {
            pDoorObject = lDoors.GetNext(pos);
            memcpy(*pArea + cnt, pDoorObject, sizeof(CAreaFileDoorObject));
            cnt += sizeof(CAreaFileDoorObject);
            delete pDoorObject;
        }

        lDoors.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // TRIGGERS
    if (lTriggers.GetCount() > 0) {
        pOffsets->m_triggerObjectListOffset = cnt;
        pOffsets->m_triggerObjectListCount = static_cast<WORD>(lTriggers.GetCount());

        pos = lTriggers.GetHeadPosition();
        while (pos != NULL) {
            pTriggerObject = lTriggers.GetNext(pos);
            memcpy(*pArea + cnt, pTriggerObject, sizeof(CAreaFileTriggerObject));
            cnt += sizeof(CAreaFileTriggerObject);
            delete pTriggerObject;
        }

        lTriggers.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // ITEMS
    if (nItems > 0) {
        pOffsets->m_itemObjectsOffset = cnt;
        pOffsets->m_itemObjectsCount = static_cast<WORD>(nItems);

        nIndex = 0;

        pos = lContainers.GetHeadPosition();
        while (pos != NULL) {
            pContainerObject = lContainers.GetNext(pos);
            if (pContainerObject->m_itemCount > 0) {
                // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
                // where the original stores the pointer. A handle the table does
                // not know gives NULL, and the bytes then stay zero.
                CCreatureFileItem* pItems = static_cast<CCreatureFileItem*>(iwd2r::UnmarshalPtr(pContainerObject->m_startingItem));
                if (pItems != NULL) {
                    memcpy(*pArea + cnt, pItems, sizeof(CCreatureFileItem) * pContainerObject->m_itemCount);
                }
                delete[] pItems;
                pContainerObject->m_startingItem = nIndex;
                cnt += sizeof(CCreatureFileItem) * pContainerObject->m_itemCount;
                nIndex += pContainerObject->m_itemCount;
            }
        }
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // CONTAINERS
    if (lContainers.GetCount() > 0) {
        pOffsets->m_containerListOffset = cnt;
        pOffsets->m_containerListCount = static_cast<WORD>(lContainers.GetCount());

        pos = lContainers.GetHeadPosition();
        while (pos != NULL) {
            pContainerObject = lContainers.GetNext(pos);
            memcpy(*pArea + cnt, pContainerObject, sizeof(CAreaFileContainer));
            cnt += sizeof(CAreaFileContainer);
            delete pContainerObject;
        }

        lContainers.RemoveAll();
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // VARIABLES
    if (nVariables > 0) {
        pOffsets->m_areaScriptVariablesOffset = cnt;
        pOffsets->m_areaScriptVariablesCount = nVariables;
        memcpy(*pArea + cnt, pVariables, sizeof(CVariable) * nVariables);
        cnt += sizeof(CVariable) * nVariables;
        delete[] pVariables;
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    // ENTRY POINTS
    if (m_entryPoints.GetCount() > 0) {
        pOffsets->m_characterEntryPointTableOffset = cnt;
        pOffsets->m_characterEntryPointTableCount = m_entryPoints.GetCount();

        pos = m_entryPoints.GetHeadPosition();
        while (pos != NULL) {
            CAreaFileCharacterEntryPoint* pEntryPoint = m_entryPoints.GetNext(pos);
            memcpy(*pArea + cnt, pEntryPoint, sizeof(CAreaFileCharacterEntryPoint));
            cnt += sizeof(CAreaFileCharacterEntryPoint);
        }
    }

    // USER NOTES
    if (m_cGameAreaNotes.m_areaNoteList.GetCount() > 0) {
        pOffsets->m_userMapNotesCount = m_cGameAreaNotes.m_areaNoteList.GetCount();
        pOffsets->m_userMapNotesOffset = cnt;
        m_cGameAreaNotes.Marshal(pArea, cnt);
    } else {
        pOffsets->m_userMapNotesOffset = 0;
        pOffsets->m_userMapNotesCount = 0;
    }

    if (bProgressBarInPlace == TRUE) {
        ProgressBarCallback(dwProgressUnit, FALSE);
    }

    CGameAIBase* pAIArea;
    if (m_pGame->GetObjectArray()->GetShare(m_nAIIndex,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pAIArea),
            INFINITE)
        == CGameObjectArray::SUCCESS) {
        CResRef cResScript = pAIArea->m_movementScript->m_cResRef;
        cResScript.GetResRef(pOffsets->m_script);
    }

    m_pGame->GetObjectArray()->ReleaseShare(m_nAIIndex,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 4913
    UTIL_ASSERT(cnt == (DWORD)*nArea);
}

// 0x472DE0
void CGameArea::Unmarshal(BYTE* pArea, LONG areaSize, const CString& sName, BOOLEAN bProgressBarInPlace)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 4939
    UTIL_ASSERT(pArea != NULL && areaSize > 0);

    DWORD dwPerArea;
    if (bProgressBarInPlace == TRUE) {
        dwPerArea = CInfGame::PROGRESSBAR_AREA_ADDITIONAL / 22;
        ProgressBarCallback(0, TRUE);
    }

    if (m_bAreaLoaded) {
        ClearMarshal();
    }

    m_pGame = g_pBaldurChitin->GetObjectGame();

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    DWORD cnt = 8;

    areaSize -= cnt;
    areaSize -= sizeof(CAreaFileHeader);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 4939
    UTIL_ASSERT(areaSize > 0);

    memcpy(&m_header, pArea + cnt, sizeof(CAreaFileHeader));
    cnt += sizeof(CAreaFileHeader);

    DWORD deltaTime;
    if (m_header.m_lastSaved) {
        deltaTime = m_pGame->GetWorldTimer()->m_gameTime - m_header.m_lastSaved * CTimerWorld::TIMESCALE_MSEC_PER_SEC + 1;
    } else {
        deltaTime = 0;
    }

    // NOTE: The original copies eight bytes (two dwords, 0x472DE0) from its
    // third argument, which LoadArea (0x5A24D0) builds from the area's resref
    // (CResRef::GetResRefStr). Ours goes through a CResRef, so the name is
    // upper-cased and zero-padded to eight bytes and nothing past the
    // CString's buffer is read ("AR1000" is seven bytes with its NUL).
    CResRef(sName).GetResRef(m_header.m_areaName);

    CString cStr(CString((char*)m_header.m_areaName, RESREF_SIZE).GetBuffer(RESREF_SIZE + 1));

    m_bmLum.SetResRef(CResRef(cStr + "LM"), TRUE, TRUE);
    m_bmLum.m_bDoubleSize = FALSE;
    m_bmLum.GetRes()->Demand();

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    m_bmHeight.SetResRef(CResRef(cStr + "HT"), TRUE, TRUE);
    m_bmHeight.m_bDoubleSize = FALSE;
    m_bmHeight.GetRes()->Demand();

    m_search.Init(this, cStr + "SR");

    CPoint ptLocation(-1, -1);
    if (m_pGame->GetRuleTables().m_tMasterArea.Find(cStr, ptLocation, TRUE)) {
        m_pGame->m_pGameAreaMaster = this;
    }

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    m_visibility.m_pDynamicHeight = new BYTE[m_search.m_GridSquareDimensions.cx * m_search.m_GridSquareDimensions.cy];
    if (m_visibility.m_pDynamicHeight == NULL) {
        return;
    }

    memset(m_visibility.m_pDynamicHeight, 0, m_search.m_GridSquareDimensions.cx * m_search.m_GridSquareDimensions.cy);

    m_pResWED = static_cast<CResWED*>(g_pChitin->cDimm.GetResObject(CResRef(m_header.m_areaName), 1001, TRUE));
    if (m_pResWED == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 5033
        UTIL_ASSERT_MSG(FALSE, (LPCSTR)("Missing WED file: " + cStr));
    }

    m_pResWED->Request();

    m_cInfinity.AttachWED(m_pResWED, m_header.m_areaType, TRUE);
    m_cInfinity.AttachVRamPool(&(g_pBaldurChitin->GetObjectGame()->m_cVRamPool));
    m_cInfinity.InitViewPort(CInfinity::stru_8E79B8);
    m_cInfinity.SetViewPosition(0, 0, TRUE);

    m_ptOldViewPos.x = 0;
    m_ptOldViewPos.y = 0;

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    CAreaFileOffsets* offsets = reinterpret_cast<CAreaFileOffsets*>(pArea + cnt);
    areaSize -= sizeof(CAreaFileOffsets);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
    // __LINE__: 5054
    UTIL_ASSERT(areaSize >= 0);

    m_visibility.Init(this);

    if (offsets->m_visibilityMapCount != 0) {
        m_visibility.Unmarshal(pArea + offsets->m_visibilityMapOffset,
            offsets->m_visibilityMapCount);
    }

    if (offsets->m_areaSoundsAndMusicOffset != 0) {
        memcpy(&m_headerSound, pArea + offsets->m_areaSoundsAndMusicOffset, sizeof(CAreaSoundsAndMusic));
        areaSize -= sizeof(CAreaSoundsAndMusic);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 5065
        UTIL_ASSERT(areaSize >= 0);

        if (m_headerSound.m_dayAmbientVolume > 100) {
            m_headerSound.m_dayAmbientVolume = 100;
        }

        if (m_headerSound.m_nightAmbientVolume > 100) {
            m_headerSound.m_nightAmbientVolume = 100;
        }
    } else {
        memset(&m_headerSound, 0, sizeof(CAreaSoundsAndMusic));
        m_headerSound.m_dayMusic = -1;
        m_headerSound.m_nightMusic = -1;
        m_headerSound.m_battleWinningMusic = -1;
        m_headerSound.m_battleStandOffMusic = -1;
        m_headerSound.m_battleLosingMusic = -1;
        m_headerSound.m_alt1Music0 = -1;
        m_headerSound.m_alt1Music1 = -1;
        m_headerSound.m_alt1Music2 = -1;
        m_headerSound.m_alt1Music3 = -1;
        m_headerSound.m_alt1Music4 = -1;
    }

    m_startedMusicCounter = 10;

    ULONG nDayTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->GetCurrentDayTime();
    m_nCurrentSong = (nDayTime < CTimerWorld::TIME_DAY || nDayTime >= CTimerWorld::TIME_DUSK)
        && (nDayTime >= CTimerWorld::TIME_DAY || nDayTime < CTimerWorld::TIME_DAWN);

    if (offsets->m_restingEncounterOffset != 0) {
        memcpy(&m_headerRestEncounter, pArea + offsets->m_restingEncounterOffset, sizeof(CAreaFileRestEncounter));
        areaSize -= sizeof(CAreaFileRestEncounter);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 5106
        UTIL_ASSERT(areaSize >= 0);
    } else {
        memset(&m_headerRestEncounter, 0, sizeof(CAreaFileRestEncounter));
    }

    m_pObjectMarker = new CObjectMarker();
    if (m_pObjectMarker == NULL) {
        return;
    }

    m_pObjectMarker->AddToArea(this, CPoint(-1, -1), 0, CGameObject::LIST_BACK);

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    INIFile.SetFileName(sName);
    INIFile.Load(sName);

    if (mpSpawner != NULL) {
        delete mpSpawner;
        mpSpawner = NULL;
    }

    mpSpawner = new CSpawnFile(&INIFile);

    INT nPartyLevel = g_pBaldurChitin->GetObjectGame()->GetAveragePartyLevel();
    INT nDifficulty;
    if (nPartyLevel < m_header.field_4D) {
        nDifficulty = (nPartyLevel >= m_header.field_4C) + 1;
    } else {
        nDifficulty = 3;
    }

    POSITION posAreaLevel = g_pBaldurChitin->GetObjectGame()->field_4BDC.GetHeadPosition();
    CInfGameAreaLevel* pAreaLevel = NULL;
    while (posAreaLevel != NULL) {
        pAreaLevel = g_pBaldurChitin->GetObjectGame()->field_4BDC.GetNext(posAreaLevel);
        if (pAreaLevel != NULL && pAreaLevel->m_sArea.CompareNoCase(cStr) == 0) {
            break;
        }
        pAreaLevel = NULL;
    }

    if (pAreaLevel != NULL) {
        if ((pAreaLevel->m_dwFlags & 0x1) != 0) {
            nDifficulty = pAreaLevel->m_nLevel;
        } else if ((pAreaLevel->m_dwFlags & 0x2) != 0) {
            nDifficulty += pAreaLevel->m_nLevel;
        }

        // NOTE: The original dereferences a NULL entry here after removing
        // it; there are none.
        posAreaLevel = g_pBaldurChitin->GetObjectGame()->field_4BDC.GetHeadPosition();
        while (posAreaLevel != NULL) {
            POSITION posOld = posAreaLevel;
            CInfGameAreaLevel* pEntry = g_pBaldurChitin->GetObjectGame()->field_4BDC.GetNext(posAreaLevel);
            if (pEntry == NULL) {
                g_pBaldurChitin->GetObjectGame()->field_4BDC.RemoveAt(posOld);
                continue;
            }

            if (pEntry->m_sArea.CompareNoCase(cStr) == 0) {
                g_pBaldurChitin->GetObjectGame()->field_4BDC.RemoveAt(posOld);
                delete pEntry;
            }
        }
    }

    BYTE nDifficultyMask;
    if (nDifficulty < 3) {
        nDifficultyMask = (nDifficulty >= 2) + 1;
    } else {
        nDifficultyMask = 4;
    }

    m_header.field_4E = static_cast<BYTE>(nPartyLevel);

    // CREATURES
    CCreatureFile creatureData;
    CVariable creatureName;

    if (offsets->m_creatureTableCount != 0) {
        for (DWORD nCreature = 0; nCreature < offsets->m_creatureTableCount; nCreature++) {
            CAreaFileCreature* pCreature = reinterpret_cast<CAreaFileCreature*>(pArea + offsets->m_creatureTableOffset) + nCreature;

            areaSize -= sizeof(CAreaFileCreature);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5197
            UTIL_ASSERT(areaSize >= 0);

            if (bProgressBarInPlace) {
                ProgressBarCallback(8 * dwPerArea / offsets->m_creatureTableCount, FALSE);
            }

            CGameSprite* pSprite = NULL;
            if (pCreature->m_creatureData[0] != '\0') {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5211
                UTIL_ASSERT(m_header.m_lastSaved == 0);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5211
                UTIL_ASSERT(pCreature->m_creatureSize == 0);

                // NOTE: The byte at 0x2F holds the difficulties the creature
                // appears at.
                if ((pCreature->structureAlignment1[0] & nDifficultyMask) != 0
                    && (pCreature->m_expirationTime == -1
                        || pCreature->m_expirationTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC > m_pGame->GetWorldTimer()->m_gameTime)) {
                    // NOTE: Uninline.
                    creatureData.SetResRef(CResRef(pCreature->m_creatureData), TRUE, TRUE);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                    // __LINE__: 5237
                    UTIL_ASSERT_MSG(creatureData.GetDataSize(),
                        CString("Creature file not found ") + CString(reinterpret_cast<char*>(pCreature->m_creatureData), RESREF_SIZE).GetBuffer(RESREF_SIZE + 1));

                    pSprite = new CGameSprite(creatureData.GetData(),
                        creatureData.GetDataSize(),
                        0,
                        pCreature->m_type,
                        -1,
                        pCreature->m_huntingRange,
                        pCreature->m_followRange,
                        pCreature->m_timeOfDayVisible,
                        CPoint(pCreature->m_startingPosX, pCreature->m_startingPosY),
                        pCreature->m_facing);

                    pSprite->SetResRef(CResRef(pCreature->m_creatureData));

                    if ((pCreature->m_dwFlags & 0x8) != 0) {
                        pSprite->GetBaseStats()->m_flags |= 0x8000;

                        CAIObjectType typeAI = pSprite->GetAIType();
                        typeAI.m_nEnemyAlly = CAIObjectType::EA_EVILCUTOFF;
                        pSprite->SetAIType(typeAI, TRUE, TRUE);
                    }

                    if ((pCreature->m_dwFlags & 0x2) != 0) {
                        pSprite->GetBaseStats()->m_flags |= 0x10000;
                    }

                    if ((pCreature->m_dwFlags & 0x4) != 0) {
                        pSprite->GetBaseStats()->m_flags |= 0x20000;
                    }

                    creatureData.ReleaseData();
                }
            } else {
                areaSize -= pCreature->m_creatureSize;

                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5309
                UTIL_ASSERT(areaSize >= 0);

                if (pCreature->m_expirationTime == -1
                    || pCreature->m_expirationTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC > m_pGame->GetWorldTimer()->m_gameTime) {
                    pSprite = new CGameSprite(pArea + pCreature->m_creatureOffset,
                        pCreature->m_creatureSize,
                        0,
                        pCreature->m_type,
                        pCreature->m_expirationTime != -1
                            ? pCreature->m_expirationTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC
                            : -1,
                        pCreature->m_huntingRange,
                        pCreature->m_followRange,
                        pCreature->m_timeOfDayVisible,
                        CPoint(pCreature->m_startingPosX, pCreature->m_startingPosY),
                        pCreature->m_facing);

                    pSprite->SetResRef(CResRef(pCreature->m_creatureData));

                    if ((pCreature->m_dwFlags & 0x8) != 0) {
                        pSprite->GetBaseStats()->m_flags |= 0x8000;

                        CAIObjectType typeAI = pSprite->GetAIType();
                        typeAI.m_nEnemyAlly = CAIObjectType::EA_EVILCUTOFF;
                        pSprite->SetAIType(typeAI, TRUE, TRUE);
                    }

                    if ((pCreature->m_dwFlags & 0x2) != 0) {
                        pSprite->GetBaseStats()->m_flags |= 0x10000;
                    }

                    if ((pCreature->m_dwFlags & 0x4) != 0) {
                        pSprite->GetBaseStats()->m_flags |= 0x20000;
                    }
                }
            }

            if (pSprite != NULL) {
                if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) != 0
                    && pSprite->GetAnimation()->CanLieDown()) {
                    pSprite->AddToArea(this,
                        CPoint(pCreature->m_posX, pCreature->m_posY),
                        0,
                        CGameObject::LIST_BACK);
                } else {
                    pSprite->AddToArea(this,
                        CPoint(pCreature->m_posX, pCreature->m_posY),
                        0,
                        pSprite->GetAnimation()->GetListType());
                }

                pSprite->LoadAreaInformation(pCreature);

                if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_CONTROLCUTOFF
                    && pSprite->GetAIType().m_nEnemyAlly == CAIObjectType::EA_FAMILIAR) {
                    g_pBaldurChitin->GetObjectGame()->AddCharacterToAllies(pSprite->GetId());
                }

                if (pSprite->CompressTime(deltaTime)) {
                    creatureName.SetName(CString(pSprite->GetScriptName()));
                    creatureName.m_intValue = pSprite->GetId();
                    m_namedCreatures.AddKey(creatureName);
                }
            }
        }
    }

    // VARIABLES
    CVariable variable;
    g_pBaldurChitin->GetObjectGame()->field_1B84 = 1;

    if (offsets->m_areaScriptVariablesCount != 0) {
        for (DWORD nVariable = 0; nVariable < offsets->m_areaScriptVariablesCount; nVariable++) {
            CAreaVariable* pAreaVariable = reinterpret_cast<CAreaVariable*>(pArea + offsets->m_areaScriptVariablesOffset) + nVariable;

            // NOTE: Uninline.
            variable = *static_cast<CVariable*>(pAreaVariable);

            areaSize -= sizeof(CAreaVariable);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5398
            UTIL_ASSERT(areaSize >= 0);

            m_variables.AddKey(variable);
        }
    }

    g_pBaldurChitin->GetObjectGame()->field_1B84 = 0;

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // ENTRY POINTS
    if (offsets->m_characterEntryPointTableCount != 0) {
        for (DWORD nEntryPoint = 0; nEntryPoint < offsets->m_characterEntryPointTableCount; nEntryPoint++) {
            CAreaFileCharacterEntryPoint* pEntryPoint = new CAreaFileCharacterEntryPoint();

            if (pEntryPoint == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5416
                UTIL_ASSERT(FALSE);
            }

            memcpy(pEntryPoint,
                pArea + offsets->m_characterEntryPointTableOffset + sizeof(CAreaFileCharacterEntryPoint) * nEntryPoint,
                sizeof(CAreaFileCharacterEntryPoint));

            areaSize -= sizeof(CAreaFileCharacterEntryPoint);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5421
            UTIL_ASSERT(areaSize >= 0);

            m_entryPoints.AddTail(pEntryPoint);
        }
    }

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // DOORS
    if (offsets->m_doorObjectListCount != 0) {
        for (DWORD nDoor = 0; nDoor < offsets->m_doorObjectListCount; nDoor++) {
            CAreaFileDoorObject* pDoorObject = reinterpret_cast<CAreaFileDoorObject*>(pArea + offsets->m_doorObjectListOffset) + nDoor;
            areaSize -= sizeof(CAreaFileDoorObject);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5439
            UTIL_ASSERT(areaSize >= 0);

            areaSize -= sizeof(CAreaPoint) * (pDoorObject->m_openSelectionPointCount + pDoorObject->m_closedSelectionPointCount + pDoorObject->m_openSearchSquaresCount + pDoorObject->m_closedSearchSquaresCount);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5444
            UTIL_ASSERT(areaSize >= 0);

            CGameDoor* pDoor = new CGameDoor(this,
                pDoorObject,
                reinterpret_cast<CAreaPoint*>(pArea + offsets->m_pointsOffset),
                offsets->m_pointsCount);

            if (pDoor == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5449
                UTIL_ASSERT(FALSE);
            }

            pDoor->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_doorObjectListCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_doorObjectListCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // TRIGGERS
    if (offsets->m_triggerObjectListCount != 0) {
        for (DWORD nTrigger = 0; nTrigger < offsets->m_triggerObjectListCount; nTrigger++) {
            CAreaFileTriggerObject* pTriggerObject = reinterpret_cast<CAreaFileTriggerObject*>(pArea + offsets->m_triggerObjectListOffset) + nTrigger;
            areaSize -= sizeof(CAreaFileTriggerObject);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5480
            UTIL_ASSERT(areaSize >= 0);

            areaSize -= sizeof(CAreaPoint) * pTriggerObject->m_pickPointCount;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5482
            UTIL_ASSERT(areaSize >= 0);

            CGameTrigger* pTrigger = new CGameTrigger(this,
                pTriggerObject,
                reinterpret_cast<CAreaPoint*>(pArea + offsets->m_pointsOffset),
                offsets->m_pointsCount);

            if (pTrigger == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5487
                UTIL_ASSERT(FALSE);
            }

            pTrigger->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_triggerObjectListCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_triggerObjectListCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // TILED
    if (offsets->m_tiledObjectListCount != 0) {
        for (DWORD nTiledObject = 0; nTiledObject < offsets->m_tiledObjectListCount; nTiledObject++) {
            CAreaFileTiledObject* pTiledObject = reinterpret_cast<CAreaFileTiledObject*>(pArea + offsets->m_tiledObjectListOffset) + nTiledObject;
            areaSize -= sizeof(CAreaFileTiledObject);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5513
            UTIL_ASSERT(areaSize >= 0);

            areaSize -= sizeof(CAreaPoint) * pTiledObject->m_primarySearchSquaresCount;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5515
            UTIL_ASSERT(areaSize >= 0);

            areaSize -= sizeof(CAreaPoint) * pTiledObject->m_secondarySearchSquaresCount;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5517
            UTIL_ASSERT(areaSize >= 0);

            CGameTiledObject* pTiled = new CGameTiledObject(this,
                pTiledObject,
                reinterpret_cast<CAreaPoint*>(pArea + offsets->m_pointsOffset),
                offsets->m_pointsCount);

            if (pTiled == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5522
                UTIL_ASSERT(FALSE);
            }

            pTiled->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_tiledObjectListCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_tiledObjectListCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // SPAWNINGS
    if (offsets->m_randomMonsterSpawningPointTableCount != 0) {
        for (DWORD nSpawning = 0; nSpawning < offsets->m_randomMonsterSpawningPointTableCount; nSpawning++) {
            CAreaFileRandomMonsterSpawningPoint* pSpawnObject = reinterpret_cast<CAreaFileRandomMonsterSpawningPoint*>(pArea + offsets->m_randomMonsterSpawningPointTableOffset) + nSpawning;
            areaSize -= sizeof(CAreaFileRandomMonsterSpawningPoint);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5554
            UTIL_ASSERT(areaSize >= 0);

            CGameSpawning* pSpawning = new CGameSpawning(this, pSpawnObject);

            if (pSpawning == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5564
                UTIL_ASSERT(FALSE);
            }

            pSpawning->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_randomMonsterSpawningPointTableCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_randomMonsterSpawningPointTableCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // SOUNDS
    if (offsets->m_soundObjectCount != 0) {
        for (DWORD nSound = 0; nSound < offsets->m_soundObjectCount; nSound++) {
            CAreaFileSoundObject* pSoundObject = reinterpret_cast<CAreaFileSoundObject*>(pArea + offsets->m_soundObjectOffset) + nSound;
            areaSize -= sizeof(CAreaFileSoundObject);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5591
            UTIL_ASSERT(areaSize >= 0);

            CGameSound* pSound = new CGameSound(this, pSoundObject);

            if (pSound == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5600
                UTIL_ASSERT(FALSE);
            }

            pSound->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_soundObjectCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_soundObjectCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // STATICS
    if (offsets->m_staticObjectListCount != 0) {
        for (DWORD nStatic = 0; nStatic < offsets->m_staticObjectListCount; nStatic++) {
            CAreaFileStaticObject* pStaticObject = reinterpret_cast<CAreaFileStaticObject*>(pArea + offsets->m_staticObjectListOffset) + nStatic;
            areaSize -= sizeof(CAreaFileStaticObject);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5623
            UTIL_ASSERT(areaSize >= 0);

            CGameStatic* pStatic = new CGameStatic(this, pStaticObject);

            if (pStatic == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5627
                UTIL_ASSERT(FALSE);
            }

            pStatic->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_staticObjectListCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_staticObjectListCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    // CONTAINERS
    if (offsets->m_containerListCount != 0) {
        for (DWORD nContainer = 0; nContainer < offsets->m_containerListCount; nContainer++) {
            CAreaFileContainer* pContainerObject = reinterpret_cast<CAreaFileContainer*>(pArea + offsets->m_containerListOffset) + nContainer;
            areaSize -= sizeof(CAreaFileContainer);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5660
            UTIL_ASSERT(areaSize >= 0);

            areaSize -= sizeof(CAreaPoint) * pContainerObject->m_pickPointCount;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5662
            UTIL_ASSERT(areaSize >= 0);

            areaSize -= sizeof(CCreatureFileItem) * pContainerObject->m_itemCount;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5664
            UTIL_ASSERT(areaSize >= 0);

            CGameContainer* pContainer = new CGameContainer(this,
                pContainerObject,
                reinterpret_cast<CAreaPoint*>(pArea + offsets->m_pointsOffset),
                offsets->m_pointsCount,
                reinterpret_cast<CCreatureFileItem*>(pArea + offsets->m_itemObjectsOffset),
                offsets->m_itemObjectsCount);

            if (pContainer == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 5669
                UTIL_ASSERT(FALSE);
            }

            pContainer->CompressTime(deltaTime);

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwPerArea / offsets->m_containerListCount, FALSE);
            }
        }
    }

    if (bProgressBarInPlace && offsets->m_containerListCount == 0) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    CGameAIArea* pAIArea = new CGameAIArea();
    pAIArea->SetDefaultScript(new CAIScript(CResRef(cStr)));
    m_nAIIndex = pAIArea->GetId();
    pAIArea->AddToArea(this, CPoint(0, 0), 0, CGameObject::LIST_BACK);

    m_firstRender = 8;
    m_bAreaLoaded = TRUE;
    m_nCharacters = -1;

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwPerArea, FALSE);
    }

    if (g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster == this) {
        g_pBaldurChitin->m_pEngineWorld->m_weather.ResetWeather(this);
    }

    m_cInfinity.CacheTiles();

    g_pBaldurChitin->GetObjectGame()->m_cMoveList.CheckLoad(this);

    if (offsets->m_userMapNotesCount != 0) {
        CAreaUserNote cAreaNote;
        for (DWORD nNote = 0; nNote < offsets->m_userMapNotesCount; nNote++) {
            areaSize -= sizeof(CAreaUserNote);
            memcpy(&cAreaNote,
                pArea + offsets->m_userMapNotesOffset + sizeof(CAreaUserNote) * nNote,
                sizeof(CAreaUserNote));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 5733
            UTIL_ASSERT(areaSize >= 0);

            m_cGameAreaNotes.AddANote(cAreaNote);
        }
    }

    CString sExtraVarName;

    // FIXME: Unsafe (UB when area name is exactly 8 chars).
    sExtraVarName = (LPCSTR)m_header.m_areaName;
    sExtraVarName += "_Visited";

    CVariable* pVisited = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sExtraVarName);
    if (pVisited != NULL) {
        pVisited->m_intValue = 1;
    } else {
        CVariable cVisited;
        cVisited.SetName(sExtraVarName);
        cVisited.m_intValue = 1;
        g_pBaldurChitin->GetObjectGame()->GetVariables()->AddKey(cVisited);
    }

    // FIXME: Unsafe (UB when area name is exactly 8 chars).
    sExtraVarName = (LPCSTR)m_header.m_areaName;
    sExtraVarName += "_REVEALED";

    CVariable* pRevealed = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sExtraVarName);
    if (pRevealed != NULL) {
        if (pRevealed->m_intValue == 0) {
            pRevealed->m_intValue = 1;
        }
    } else {
        CVariable cRevealed;
        cRevealed.SetName(sExtraVarName);
        cRevealed.m_intValue = 1;
        g_pBaldurChitin->GetObjectGame()->GetVariables()->AddKey(cRevealed);
    }

    g_pBaldurChitin->GetObjectGame()->field_1B86 = "";
}

// NOTE: Similar to `CInfGame::ProgressBarCallback`.
//
// 0x474E10
void CGameArea::ProgressBarCallback(DWORD dwSize, BOOLEAN bInitialize)
{
    if (bInitialize == TRUE) {
        m_dwLastProgressRenderTickCount = GetTickCount() - 250;
        m_dwLastProgressMsgTickCount = GetTickCount() - 5000;
    }

    g_pChitin->cProgressBar.m_nActionProgress += dwSize;

    if (GetTickCount() - m_dwLastProgressMsgTickCount >= 1000) {
        m_dwLastProgressMsgTickCount = GetTickCount();

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            g_pBaldurChitin->GetBaldurMessage()->SendProgressBarStatus(g_pChitin->cProgressBar.m_nActionProgress,
                g_pChitin->cProgressBar.m_nActionTarget,
                g_pChitin->cProgressBar.m_bWaiting,
                g_pChitin->cProgressBar.m_nWaitingReason,
                g_pChitin->cProgressBar.m_bTimeoutVisible,
                g_pChitin->cProgressBar.m_nSecondsToTimeout);
        }
    }

    if (GetTickCount() - m_dwLastProgressRenderTickCount >= 250) {
        m_dwLastProgressRenderTickCount = GetTickCount();

        g_pChitin->m_bDisplayStale = TRUE;
        g_pChitin->cDimm.field_0 = 1;
        g_pChitin->cDimm.field_4 = 1;
        SleepEx(25, TRUE);
    }
}

// 0x474F00
void CGameArea::SetListenPosition()
{
    BYTE cnt;
    BYTE range;
    CPoint ptListen;
    CPoint ptCheck;

    ptListen.x = m_cInfinity.nNewX + m_cInfinity.rViewPort.Width() / 2;
    ptListen.y = m_cInfinity.nNewY + m_cInfinity.rViewPort.Height() / 2;

    if (m_visibility.IsTileExplored(m_visibility.PointToTile(ptListen))) {
        g_pBaldurChitin->cSoundMixer.SetListenPosition(ptListen.x, ptListen.y, 0);
        return;
    }

    cnt = 0;
    range = m_cInfinity.rViewPort.Width() / CVisibilityMap::SQUARE_SIZEX;
    ptCheck.x = m_cInfinity.nNewX + CVisibilityMap::SQUARE_SIZEX / 2;
    ptCheck.y = m_cInfinity.nNewY + m_cInfinity.rViewPort.Height() / 2;
    while (cnt < range) {
        if (m_visibility.IsTileExplored(m_visibility.PointToTile(ptCheck))) {
            g_pBaldurChitin->cSoundMixer.SetListenPosition(ptListen.x, ptListen.y, 0);
            return;
        }
        ptCheck.x += CVisibilityMap::SQUARE_SIZEX;
        cnt++;
    }

    cnt = 0;
    range = m_cInfinity.rViewPort.Width() / CVisibilityMap::SQUARE_SIZEX;
    ptCheck.x = m_cInfinity.nNewX + m_cInfinity.rViewPort.Width() / 2;
    ptCheck.y = m_cInfinity.nNewY + CVisibilityMap::SQUARE_SIZEY / 2;
    while (cnt < range) {
        if (m_visibility.IsTileExplored(m_visibility.PointToTile(ptCheck))) {
            g_pBaldurChitin->cSoundMixer.SetListenPosition(ptListen.x, ptListen.y, 0);
            return;
        }
        ptCheck.y += CVisibilityMap::SQUARE_SIZEY;
        cnt++;
    }
}

// 0x4750E0
void CGameArea::OnActivation()
{
    SetListenPosition();

    m_ptMousePos = g_pChitin->m_ptPointer;
    m_nScrollState = 0;
    m_nKeyScrollState = 0;
    m_cInfinity.m_nScrollDelay = CInfinity::SCROLL_DELAY;
    m_cInfinity.bRefreshVRamRect = TRUE;

    // NOTE: The original stops the world timer when the world screen is
    // not paused (`cmp [world+0x13E], 0; jnz` at 0x475133). The timer starts
    // again at the area's first render (m_firstRender), so the AI updates of
    // the countdown run with the world stopped.
    if (!g_pBaldurChitin->m_pEngineWorld->m_bPaused) {
        g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->StopTime();
    }

    if (g_pBaldurChitin->GetObjectGame()->GetVisibleArea() == this) {
        g_pBaldurChitin->cSoundMixer.m_nActiveArea = reinterpret_cast<DWORD_PTR>(this);
        g_pBaldurChitin->cSoundMixer.UpdateSoundList();

        if (g_pBaldurChitin->field_1C4A == 1 && byte_8D2138 == TRUE) {
            if (dword_8D212C == g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nVolumeAmbients) {
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(16, dword_8D212C);
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(17, dword_8D212C);
            } else {
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(16, g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nVolumeAmbients);
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(17, g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nVolumeAmbients);
            }
            byte_8D2138 = FALSE;
        } else {
            g_pBaldurChitin->cSoundMixer.SetChannelVolume(16, g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nVolumeAmbients);
            g_pBaldurChitin->cSoundMixer.SetChannelVolume(17, g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nVolumeAmbients);
        }

        SetTimeOfDay(m_pGame->GetWorldTimer()->m_gameTime % CTimerWorld::TIMESCALE_MSEC_PER_DAY, FALSE);
        m_cInfinity.UpdateLightning();
        SetSoundEnvironment();
        g_pBaldurChitin->m_pEngineWorld->m_weather.OnAreaChange(FALSE);
        m_firstRender = 8;

        if (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay()) {
            m_nCurrentSong = 0;
        } else if (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsNight()) {
            m_nCurrentSong = 1;
        }

        if (m_nCurrentSong != -1) {
            if (!g_pBaldurChitin->cSoundMixer.sub_7ACA30()) {
                PlaySong(m_nCurrentSong, 5);
            }
        }

        g_pBaldurChitin->GetObjectGame()->ApplyVolumeSliders(TRUE);
    }
}

// 0x475330
void CGameArea::OnDeactivation()
{
    m_selectSquare.left = -1;
    m_selectSquare.top = -1;
    m_selectSquare.right = -1;
    m_selectSquare.bottom = -1;
    m_groupMove = FALSE;

    if (!m_pGame->GetGameSave()->m_cutScene) {
        m_pGame->m_tempCursor = 4;
    }

    if (!g_pBaldurChitin->field_1C4A) {
        g_pBaldurChitin->cSoundMixer.m_nActiveArea = -1;
        g_pBaldurChitin->cSoundMixer.UpdateSoundList();
    } else {
        if (!byte_8D2138) {
            dword_8D212C = g_pBaldurChitin->cSoundMixer.GetChannelVolume(16);
            if (dword_8D212C > 0) {
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(16, dword_8D212C / 2);
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(17, dword_8D212C / 2);
            } else {
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(16, 0);
                g_pBaldurChitin->cSoundMixer.SetChannelVolume(17, 0);
            }
            byte_8D2138 = TRUE;
        }
    }

    g_pBaldurChitin->AddMusicPosition(m_resRef, g_pBaldurChitin->cSoundMixer.m_nCurrentSong);

    m_firstRender = 0;
}

// 0x475440
void CGameArea::OnActionButtonDown(const CPoint& pt)
{
    CPoint ptWorld = m_cInfinity.GetWorldCoordinates(pt);

    if (pt.x >= m_cInfinity.rViewPort.left
        && pt.x < m_cInfinity.rViewPort.right
        && pt.y >= m_cInfinity.rViewPort.top
        && pt.y < m_cInfinity.rViewPort.bottom) {
        CPoint mouseSearchSquare;
        SHORT searchSquareCode;

        mouseSearchSquare.x = ptWorld.x / CPathSearch::GRID_SQUARE_SIZEX;
        mouseSearchSquare.y = ptWorld.y / CPathSearch::GRID_SQUARE_SIZEY;

        if (m_pGame->GetState() == 3
            && m_iPicked == CGameObjectArray::INVALID_INDEX
            && !m_visibility.IsTileExplored(m_visibility.PointToTile(ptWorld))
            && m_search.GetLOSCost(mouseSearchSquare, m_terrainTable, searchSquareCode, FALSE) == CPathSearch::COST_IMPASSABLE) {
            return;
        }

        m_selectSquare.left = ptWorld.x;
        m_selectSquare.right = ptWorld.x;
        m_selectSquare.top = ptWorld.y;
        m_selectSquare.bottom = ptWorld.y;
        m_iPickedOnDown = m_iPicked;
        m_moveDest = ptWorld;
        field_241 = CTimerWorld::TIMESCALE_MSEC_PER_SEC;

        if (g_pBaldurChitin->GetObjectCursor()->m_nCurrentCursor == 4) {
            m_pGame->GetGroup()->GroupDrawMove(ptWorld,
                m_pGame->GetGameSave()->m_curFormation,
                CPoint(-1, -1));
        }

        if (m_pGame->GetState() == 0) {
            if (m_pGame->GetButtonArray()->field_19B2 != 0) {
                m_pGame->GetButtonArray()->SetSelectedButton(100);
                m_pGame->GetButtonArray()->ResetState();
            }
        }
    }
}

// 0x475DC0
void CGameArea::OnActionButtonUp(const CPoint& pt)
{
    CPoint ptWorld = m_cInfinity.GetWorldCoordinates(pt);
    SHORT gameState = m_pGame->GetState();
    BOOL firstSelect = TRUE;

    if (m_selectSquare.left != -1) {
        m_selectSquare.NormalizeRect();
        if (gameState == 0 || gameState == 3) {
            if (m_selectSquare.Width() <= 8
                && m_selectSquare.Height() <= 8
                && ptWorld.x != -1) {
                if (m_visibility.IsTileExplored(m_visibility.PointToTile(m_moveDest))) {
                    if (m_iPickedOnDown != CGameObjectArray::INVALID_INDEX) {
                        CGameObject* pObject;

                        BYTE rc = m_pGame->GetObjectArray()->GetShare(m_iPickedOnDown,
                            CGameObjectArray::THREAD_ASYNCH,
                            &pObject,
                            INFINITE);
                        if (rc == CGameObjectArray::SUCCESS) {
                            if (pObject->GetArea() == this) {
                                pObject->OnActionButton(m_moveDest);
                            }
                            m_pGame->GetObjectArray()->ReleaseShare(m_iPickedOnDown,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    } else {
                        if (gameState == 3) {
                            CPoint ptCursor(2 * m_moveDest.x - ptWorld.x,
                                2 * m_moveDest.y - ptWorld.y);
                            int nWidth = m_selectSquare.Width();
                            int nHeight = 4 * m_selectSquare.Height() / 3;
                            int nRange = static_cast<int>(sqrt(static_cast<double>(nWidth * nWidth + nHeight * nHeight)) + 0.5);
                            m_pGame->GetGroup()->GroupProtectPoint(m_moveDest,
                                m_pGame->GetGameSave()->m_curFormation,
                                ptCursor,
                                nRange);
                            m_groupMove = FALSE;
                            m_pGame->SetState(0);
                            m_pGame->GetButtonArray()->SetSelectedButton(100);
                            m_pGame->GetButtonArray()->UpdateState();
                        } else {
                            if (m_bTravelSquare) {
                                OnActionButtonClickTravel(m_moveDest);
                            } else {
                                OnActionButtonClickGround(m_moveDest);
                            }
                        }
                    }
                }
            } else {
                if (gameState == 3) {
                    CPoint ptCursor(2 * m_moveDest.x - ptWorld.x,
                        2 * m_moveDest.y - ptWorld.y);
                    int nWidth = m_selectSquare.Width();
                    int nHeight = 4 * m_selectSquare.Height() / 3;
                    int nRange = static_cast<int>(sqrt(static_cast<double>(nWidth * nWidth + nHeight * nHeight)) + 0.5);
                    m_pGame->GetGroup()->GroupProtectPoint(m_moveDest,
                        m_pGame->GetGameSave()->m_curFormation,
                        ptCursor,
                        nRange);
                    m_groupMove = FALSE;
                    m_pGame->SetState(0);
                    m_pGame->GetButtonArray()->SetSelectedButton(100);
                    m_pGame->GetButtonArray()->UpdateState();
                } else {
                    POSITION pos;
                    CGameSprite* pSprite;
                    LONG nSprite;
                    BYTE rc;
                    BOOL bPlaySelectSound;

                    for (SHORT nPortrait = 0; nPortrait < m_pGame->GetNumCharacters(); nPortrait++) {
                        nSprite = m_pGame->GetCharacterId(nPortrait);
                        rc = m_pGame->GetObjectArray()->GetShare(nSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                        if (rc == CGameObjectArray::SUCCESS) {
                            if (pSprite->m_canBeSeen > 0
                                && pSprite->GetArea() == this
                                && pSprite->DoesIntersect(m_selectSquare)
                                && pSprite->Orderable(FALSE)) {
                                if (firstSelect) {
                                    firstSelect = FALSE;

                                    if (!g_pBaldurChitin->GetScreenWorld()->GetCtrlKey()
                                        && !g_pBaldurChitin->GetScreenWorld()->GetShiftKey()) {
                                        m_pGame->UnselectAll();
                                    }

                                    bPlaySelectSound = TRUE;
                                } else {
                                    bPlaySelectSound = FALSE;
                                }
                                m_pGame->SelectCharacter(nSprite, bPlaySelectSound);
                            }
                            m_pGame->GetObjectArray()->ReleaseShare(nSprite,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    }

                    pos = m_pGame->m_familiars.GetHeadPosition();
                    while (pos != NULL) {
                        nSprite = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_pGame->m_familiars.GetNext(pos)));
                        rc = m_pGame->GetObjectArray()->GetShare(nSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                        if (rc == CGameObjectArray::SUCCESS) {
                            if (pSprite->m_canBeSeen > 0
                                && pSprite->GetArea() == this
                                && pSprite->DoesIntersect(m_selectSquare)
                                && pSprite->Orderable(FALSE)) {
                                if (firstSelect) {
                                    firstSelect = FALSE;

                                    if (!g_pBaldurChitin->GetScreenWorld()->GetCtrlKey()
                                        && !g_pBaldurChitin->GetScreenWorld()->GetShiftKey()) {
                                        m_pGame->UnselectAll();
                                    }

                                    bPlaySelectSound = TRUE;
                                } else {
                                    bPlaySelectSound = FALSE;
                                }
                                m_pGame->SelectCharacter(nSprite, bPlaySelectSound);
                            }
                            m_pGame->GetObjectArray()->ReleaseShare(nSprite,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    }

                    pos = m_pGame->m_allies.GetHeadPosition();
                    while (pos != NULL) {
                        nSprite = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_pGame->m_allies.GetNext(pos)));
                        rc = m_pGame->GetObjectArray()->GetShare(nSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                        if (rc == CGameObjectArray::SUCCESS) {
                            if (pSprite->m_canBeSeen > 0
                                && pSprite->GetArea() == this
                                && pSprite->DoesIntersect(m_selectSquare)
                                && pSprite->Orderable(FALSE)) {
                                if (firstSelect) {
                                    firstSelect = FALSE;

                                    if (!g_pBaldurChitin->GetScreenWorld()->GetCtrlKey()
                                        && !g_pBaldurChitin->GetScreenWorld()->GetShiftKey()) {
                                        m_pGame->UnselectAll();
                                    }

                                    bPlaySelectSound = TRUE;
                                } else {
                                    bPlaySelectSound = FALSE;
                                }
                                m_pGame->SelectCharacter(nSprite, bPlaySelectSound);
                            }
                            m_pGame->GetObjectArray()->ReleaseShare(nSprite,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    }

                    if (!firstSelect) {
                        m_pGame->SelectToolbar();
                    }
                }
            }
            m_pGame->GetGroup()->GroupCancelMove();
        } else {
            if (m_selectSquare.Width() <= 8
                && m_selectSquare.Height() <= 8
                && ptWorld.x != -1
                && m_visibility.IsTileExplored(m_visibility.PointToTile(m_moveDest))) {
                if (gameState == 2) {
                    if (m_iPickedOnDown != CGameObjectArray::INVALID_INDEX) {
                        CGameObject* pObject;

                        BYTE rc = m_pGame->GetObjectArray()->GetShare(m_iPickedOnDown,
                            CGameObjectArray::THREAD_ASYNCH,
                            &pObject,
                            INFINITE);
                        if (rc == CGameObjectArray::SUCCESS) {
                            if (pObject->GetArea() == this) {
                                pObject->OnActionButton(m_moveDest);
                            }
                            m_pGame->GetObjectArray()->ReleaseShare(m_iPickedOnDown,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    }
                } else {
                    OnActionButtonClickGround(m_moveDest);
                }
            }
        }

        m_selectSquare.left = -1;
        m_selectSquare.top = -1;
        m_selectSquare.right = -1;
        m_selectSquare.bottom = -1;
        m_pGame->SetTempCursor(4);
    }
}

// 0x475600
void CGameArea::OnActionButtonClickGround(const CPoint& pt)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CAIGroup* pGroup = pGame->GetGroup();
    SHORT nTableIndex;

    switch (pGame->GetState()) {
    case 0:
        if (pGroup->m_groupChanged || pGame->m_lastClick != pt) {
            pGroup->m_groupChanged = FALSE;

            // NOTE: Uninline.
            pGame->SetLastClick(pt);

            if (pGroup->GetCount() > 0) {
                BOOL bShift = g_pBaldurChitin->GetScreenWorld()->GetShiftKey();
                if (!bShift) {
                    pGroup->ClearActions();
                    bShift = FALSE;
                }

                if (pGroup->GetCount() == 1) {
                    pGroup->GroupSetTarget(pt,
                        bShift,
                        CAIGroup::FORMATION_NONE,
                        CPoint(-1, -1));
                } else {
                    if (m_groupMove) {
                        CPoint ptCursor(2 * m_moveDest.x - pt.x, 2 * m_moveDest.y - pt.y);
                        pGroup->GroupDrawMove(m_moveDest,
                            pGame->GetGameSave()->m_curFormation,
                            ptCursor);
                        pGroup->GroupSetTarget(m_moveDest,
                            bShift,
                            pGame->GetGameSave()->m_curFormation,
                            ptCursor);
                    } else {
                        pGroup->GroupSetTarget(pt,
                            bShift,
                            pGame->GetGameSave()->m_curFormation,
                            CPoint(-1, -1));
                    }
                }
            }
        }
        break;
    case 1:
        // NOTE: Uninline.
        pGame->SetLastClick(CPoint(-1, -1));

        if (m_search.GetLOSCost(CPoint(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY), m_terrainTable, nTableIndex, FALSE) != CPathSearch::COST_IMPASSABLE) {
            BYTE nIconIndex = pGame->GetIconIndex();
            if (nIconIndex != 20 && nIconIndex != -1) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 6200
                UTIL_ASSERT(FALSE);
            }

            pGame->UseMagicOnGround(pt);
            pGame->SetState(0);
            pGame->GetButtonArray()->SetSelectedButton(100);
            pGame->GetButtonArray()->UpdateState();
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 6210
        UTIL_ASSERT(FALSE);
    }
}

// 0x475870
void CGameArea::OnActionButtonClickTravel(const CPoint& pt)
{
    BYTE flags;
    SHORT direction;
    CPoint searchSquare(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY);

    if (searchSquare.x < CSearchBitmap::TRAVEL_WIDTH) {
        flags = static_cast<BYTE>(m_header.m_flagsEdgeWest);
        direction = 4;
    } else if (searchSquare.x >= m_cInfinity.nAreaX / CPathSearch::GRID_SQUARE_SIZEX - CSearchBitmap::TRAVEL_WIDTH) {
        flags = static_cast<BYTE>(m_header.m_flagsEdgeEast);
        direction = 12;
    } else if (searchSquare.y < CSearchBitmap::TRAVEL_WIDTH) {
        flags = static_cast<BYTE>(m_header.m_flagsEdgeNorth);
        direction = 8;
    } else if (searchSquare.y >= m_cInfinity.nAreaY / CPathSearch::GRID_SQUARE_SIZEY - CSearchBitmap::TRAVEL_WIDTH) {
        flags = static_cast<BYTE>(m_header.m_flagsEdgeSouth);
        direction = 0;
    } else {
        flags = static_cast<BYTE>(m_header.m_flagsEdgeWest);
        direction = 4;
    }

    CAIAction leave(CAIAction::LEAVEAREA,
        CString(""),
        pt,
        direction | ((flags & 0x1) << 16));

    switch (direction) {
    case 0:
        leave.SetString1(CString(m_header.m_areaEdgeSouth));
        break;
    case 4:
        leave.SetString1(CString(m_header.m_areaEdgeWest));
        break;
    case 8:
        leave.SetString1(CString(m_header.m_areaEdgeNorth));
        break;
    case 12:
        leave.SetString1(CString(m_header.m_areaEdgeEast));
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 6334
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    m_pGame->SetLastClick(CPoint(-1, -1));

    m_pGame->GetGroup()->GroupAction(leave, TRUE, NULL);

    CAIAction move(CAIAction::MOVETOPOINT, pt, 0, -1);
    m_pGame->GetGroup()->GroupAction(move, FALSE, NULL);
}

// 0x4765C0
void CGameArea::OnActionButtonDblClk(const CPoint& pt)
{
    if (m_cInfinity.rViewPort.PtInRect(pt)
        && g_pBaldurChitin->GetObjectGame()->GetState() == 0) {
        CRect rView(m_cInfinity.rViewPort);
        LONG v1 = rView.Width() / -2;
        LONG v2 = rView.Height() / -2;
        CPoint ptWorld = m_cInfinity.GetWorldCoordinates(pt);
        m_cInfinity.SetViewPosition(ptWorld.x + v1, ptWorld.y + v2, TRUE);
    }
}

// 0x476680
void CGameArea::OnFormationButtonDown(const CPoint& pt)
{
    SHORT gameState = g_pBaldurChitin->GetObjectGame()->GetState();
    CPoint worldMousePoint = m_cInfinity.GetWorldCoordinates(pt);
    if (pt.x >= m_cInfinity.rViewPort.left
        && pt.x < m_cInfinity.rViewPort.right
        && pt.y >= m_cInfinity.rViewPort.top
        && pt.y < m_cInfinity.rViewPort.bottom
        && gameState == 0) {
        if (m_selectSquare.left == -1
            && m_visibility.IsTileExplored(m_visibility.PointToTile(worldMousePoint))) {
            // FIXME: Why not `worldMousePoint`?
            m_moveDest = m_cInfinity.GetWorldCoordinates(pt);
            if (g_pBaldurChitin->GetObjectGame()->GetGroup()->GetCount() != 0) {
                if (g_pBaldurChitin->GetObjectCursor()->m_nCurrentCursor == 4) {
                    m_pGame->GetGroup()->GroupDrawMove(m_moveDest,
                        m_pGame->GetGameSave()->m_curFormation,
                        CPoint(-1, -1));
                    m_groupMove = TRUE;
                }
            }
        } else {
            m_moveDest.x = -9;
            m_moveDest.y = -9;
        }
    }
}

// 0x4767E0
void CGameArea::OnFormationButtonUp(const CPoint& pt)
{
    SHORT gameState = g_pBaldurChitin->GetObjectGame()->GetState();
    CPoint worldMousePoint = m_cInfinity.GetWorldCoordinates(pt);
    if (gameState == 0) {
        if (abs(m_moveDest.x - worldMousePoint.x) <= 8
            && abs(m_moveDest.y - worldMousePoint.y) <= 8
            && m_iPicked != CGameObjectArray::INVALID_INDEX) {
            CGameObject* pObject;

            BYTE rc = m_pGame->GetObjectArray()->GetShare(m_iPicked,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                pObject->OnFormationButton(worldMousePoint);
                m_pGame->GetObjectArray()->ReleaseShare(m_iPicked,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        } else {
            if (m_groupMove) {
                OnActionButtonClickGround(worldMousePoint);
            }
        }

        if (m_groupMove) {
            m_groupMove = FALSE;
            m_pGame->GetGroup()->GroupCancelMove();
            m_pGame->SetTempCursor(4);
        }
    } else if (gameState != 3 || m_selectSquare.left == -1) {
        m_pGame->SetState(0);
        m_pGame->m_cButtonArray.m_nSelectedButton = 100;
        m_pGame->m_cButtonArray.UpdateState();
        m_pGame->SetTempCursor(4);
    }
}

// 0x476970
void CGameArea::OnMouseMove(const CPoint& pt)
{
    if (!m_bAreaLoaded) {
        return;
    }

    INT x;
    INT y;
    m_cInfinity.GetViewPosition(x, y);

    CPoint ptWorld;
    if (pt.x != -1) {
        ptWorld = m_cInfinity.GetWorldCoordinates(pt);
    } else {
        ptWorld.x = -2;
        ptWorld.y = -2;
    }

    m_ptMousePos = pt;
    m_nToolTip = 0;

    if (m_groupMove && ptWorld.x > -2) {
        if (abs(ptWorld.x - m_moveDest.x) > 8 || abs(ptWorld.y - m_moveDest.y) > 8) {
            CPoint ptCursor;
            if (pt.x >= m_cInfinity.rViewPort.left
                && pt.x < m_cInfinity.rViewPort.right
                && pt.y >= m_cInfinity.rViewPort.top
                && pt.y < m_cInfinity.rViewPort.bottom) {
                ptCursor.x = 2 * m_moveDest.x - ptWorld.x;
                ptCursor.y = 2 * m_moveDest.y - ptWorld.y;
            } else {
                CPoint ptScreen;
                ptScreen.x = min(max(pt.x, m_cInfinity.rViewPort.left), m_cInfinity.rViewPort.right - 1);
                ptScreen.y = min(max(pt.y, m_cInfinity.rViewPort.top), m_cInfinity.rViewPort.bottom - 1);

                ptWorld = m_cInfinity.GetWorldCoordinates(ptScreen);

                ptCursor.x = 2 * m_moveDest.x - ptWorld.x;
                ptCursor.y = 2 * m_moveDest.y - ptWorld.y;
            }
            m_pGame->GetGroup()->GroupDrawMove(m_moveDest,
                m_pGame->GetGameSave()->m_curFormation,
                ptCursor);
            if (m_pGame->GetState() == 0) {
                m_pGame->SetTempCursor(8);
            }
        } else {
            INT nCursor = g_pBaldurChitin->GetObjectCursor()->m_nCurrentCursor;
            if (nCursor == 4 || nCursor == 8) {
                m_pGame->GetGroup()->GroupDrawMove(m_moveDest,
                    m_pGame->GetGameSave()->m_curFormation,
                    CPoint(-1, -1));
            } else {
                m_pGame->GetGroup()->GroupCancelMove();
            }
            if (m_pGame->GetState() == 0) {
                m_pGame->SetTempCursor(4);
            }
        }

        if (m_pGame->GetState() == 3 && m_selectSquare.left != -1) {
            m_selectSquare.right = min(max(pt.x - m_cInfinity.rViewPort.left, 0), m_cInfinity.rViewPort.Width() - 1) + x;
            m_selectSquare.bottom = min(max(pt.y - m_cInfinity.rViewPort.top, 0), m_cInfinity.rViewPort.Height() - 1) + y;
        }
    } else if (m_selectSquare.left != -1 && ptWorld.x > -2) {
        m_selectSquare.right = min(max(pt.x - m_cInfinity.rViewPort.left, 0), m_cInfinity.rViewPort.Width() - 1) + x;
        m_selectSquare.bottom = min(max(pt.y - m_cInfinity.rViewPort.top, 0), m_cInfinity.rViewPort.Height() - 1) + y;

        if (abs(m_selectSquare.Width()) > 8 || abs(m_selectSquare.Height()) > 8) {
            if (m_pGame->GetState() == 0) {
                m_pGame->SetTempCursor(0);
            }
            m_pGame->GetGroup()->GroupCancelMove();
        } else {
            if (g_pBaldurChitin->GetObjectCursor()->m_nCurrentCursor == 4) {
                m_pGame->GetGroup()->GroupDrawMove(m_moveDest,
                    m_pGame->GetGameSave()->m_curFormation,
                    CPoint(-1, -1));
            } else {
                m_pGame->GetGroup()->GroupCancelMove();
            }
            if (m_pGame->GetState() == 0) {
                m_pGame->SetTempCursor(4);
            }
        }
    }

    if (m_firstRender) {
        m_nScrollState = 0;
        m_cInfinity.m_nScrollDelay = CInfinity::SCROLL_DELAY;
        return;
    }

    if (pt.x == 0) {
        if (pt.y > CVideo::SCREENHEIGHT / 12) {
            if (pt.y < 11 * CVideo::SCREENHEIGHT / 12 - 1) {
                if (x > 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 7;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 6;
            } else {
                if (x > 0 || y < m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 6;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 5;
            }
        } else {
            if (x > 0 || y > 0) {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                m_nScrollState = 8;
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                m_nScrollState = 9;
            }
            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 7;
        }
    } else if (pt.x == CVideo::SCREENWIDTH - 1) {
        if (pt.y > CVideo::SCREENHEIGHT / 12) {
            if (pt.y < 11 * CVideo::SCREENHEIGHT / 12 - 1) {
                if (x < m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 3;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 2;
            } else {
                if (x < m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1
                    || y < m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 4;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 3;
            }
        } else {
            if (x < m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1 || y > 0) {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                m_nScrollState = 2;
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                m_nScrollState = 9;
            }
            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 1;
        }
    } else if (pt.y == 0) {
        if (pt.x > CVideo::SCREENWIDTH / 16) {
            if (pt.x < 15 * CVideo::SCREENWIDTH / 16 - 1) {
                if (y > 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 1;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 0;
            } else {
                if (x < m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1 || y > 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 2;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 1;
            }
        } else {
            if (x > 0 || y > 0) {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                m_nScrollState = 8;
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                m_nScrollState = 9;
            }
            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 7;
        }
    } else if (pt.y == CVideo::SCREENHEIGHT - 1) {
        if (pt.x > CVideo::SCREENWIDTH / 16) {
            if (pt.x < 15 * CVideo::SCREENWIDTH / 16 - 1) {
                if (y < m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 5;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 4;
            } else {
                if (x < m_cInfinity.nAreaX - m_cInfinity.rViewPort.Width() - 1
                    || y < m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                    m_nScrollState = 4;
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                    m_nScrollState = 9;
                }
                g_pBaldurChitin->GetObjectCursor()->m_nDirection = 3;
            }
        } else {
            if (x > 0 || y < m_cInfinity.nAreaY - m_cInfinity.rViewPort.Height() - 1) {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                m_nScrollState = 6;
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                m_nScrollState = 9;
            }
            g_pBaldurChitin->GetObjectCursor()->m_nDirection = 5;
        }
    } else {
        m_nScrollState = 0;
        m_cInfinity.m_nScrollDelay = CInfinity::SCROLL_DELAY;
    }
}

// 0x477550
void CGameArea::RemoveObject(POSITION posVertList, BYTE listType, LONG id)
{
    POSITION pos;

    EnterCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));

    if (g_pBaldurChitin->cNetwork.GetSessionOpen()) {
        switch (listType) {
        case CGAMEOBJECT_LIST_FRONT:
            pos = m_lVertSortAdd.GetHeadPosition();
            while (pos != NULL) {
                if (static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortAdd.GetAt(pos))) == id) {
                    m_lVertSortAdd.RemoveAt(pos);
                    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
                    return;
                }

                m_lVertSortAdd.GetNext(pos);
            }
            break;
        case CGAMEOBJECT_LIST_BACK:
            pos = m_lVertSortBackAdd.GetHeadPosition();
            while (pos != NULL) {
                if (static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBackAdd.GetAt(pos))) == id) {
                    m_lVertSortBackAdd.RemoveAt(pos);
                    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
                    return;
                }

                m_lVertSortBackAdd.GetNext(pos);
            }
            break;
        case CGAMEOBJECT_LIST_FLIGHT:
            pos = m_lVertSortFlightAdd.GetHeadPosition();
            while (pos != NULL) {
                if (static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlightAdd.GetAt(pos))) == id) {
                    m_lVertSortFlightAdd.RemoveAt(pos);
                    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
                    return;
                }

                m_lVertSortFlightAdd.GetNext(pos);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 7070
            UTIL_ASSERT(FALSE);
        }
    }

    // TODO: `Find` calls look redunant.
    switch (listType) {
    case CGAMEOBJECT_LIST_FRONT:
        if (m_lVertSortRemove.Find(posVertList) == NULL) {
            m_lVertSortRemove.AddTail(posVertList);
        }
        break;
    case CGAMEOBJECT_LIST_BACK:
        if (m_lVertSortBackRemove.Find(posVertList) == NULL) {
            m_lVertSortBackRemove.AddTail(posVertList);
        }
        break;
    case CGAMEOBJECT_LIST_FLIGHT:
        if (m_lVertSortFlightRemove.Find(posVertList) == NULL) {
            m_lVertSortFlightRemove.AddTail(posVertList);
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 7095
        UTIL_ASSERT(FALSE);
    }

    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
}

// 0x4774B0
void CGameArea::OnLightningStrike()
{
    if (!m_lVertSort.IsEmpty()) {
        POSITION pos = m_lVertSort.FindIndex(rand() % m_lVertSort.GetCount());
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetAt(pos)));

        CGameObject* pObject;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                static_cast<CGameSprite*>(pObject)->OnLightningStrike();
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x477740
void CGameArea::Render(CVidMode* pVidMode, INT nSurface)
{
    POSITION pos;
    LONG iObject;
    CGameObject* pObject;
    BYTE rc;

    if (!m_bAreaLoaded) {
        return;
    }

    g_pBaldurChitin->GetObjectGame()->BeginListManipulation(this);
    EnterCriticalSection(&field_1FC);

    if (CCacheStatus::dword_8D0BA8) {
        CCacheStatus::dword_8D0BA8 = FALSE;

        // NOTE: Uninline.
        g_pChitin->GetCurrentVideoMode()->SetFade(FALSE, 0);

        CScreenWorld::dword_8F85BC = 3;
    }

    if (m_firstRender > 0) {
        if (m_cInfinity.pVRPool != NULL) {
            m_cInfinity.pVRPool->ClearAll();
        }
    }

    pos = m_lTiledObjects.GetHeadPosition();
    while (pos != NULL) {
        CTiledObject* pTiledObject = m_lTiledObjects.GetNext(pos);
        pTiledObject->CheckTileState(m_cInfinity);
    }

    if (m_nKeyScrollState != 0) {
        m_cInfinity.Render(pVidMode, nSurface, m_nKeyScrollState, &m_visibility);
        m_nKeyScrollState = 0;
    } else {
        m_cInfinity.Render(pVidMode, nSurface, m_nScrollState, &m_visibility);
    }

    CInfTileSet::m_nTextures = 0;

    if (m_cInfinity.Render(pVidMode, nSurface, m_nScrollState, &m_visibility) != CInfinity::RENDER_MESSAGESCREEN) {
        pos = m_lVertSort.GetHeadPosition();
        while (pos != NULL) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

            do {
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_1,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                BYTE nObjectType = pObject->GetObjectType();
                if (pObject->m_posZ <= 0) {
                    switch (nObjectType) {
                    case 17:
                    case 33:
                    case 65:
                        pObject->Render(this, pVidMode, nSurface);
                        break;
                    case 48:
                        if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bStaticAnimations
                            || (static_cast<CGameStatic*>(pObject)->m_header.m_dwFlags & 0x10) == 16) {
                            pObject->Render(this, pVidMode, nSurface);
                        }
                        break;
                    }
                }

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_1,
                    INFINITE);
            }
        }

        pos = m_lVertSortBack.GetHeadPosition();
        while (pos != NULL) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBack.GetNext(pos)));

            do {
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_1,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                pObject->Render(this, pVidMode, nSurface);

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_1,
                    INFINITE);
            }
        }

        pos = m_lVertSort.GetHeadPosition();
        while (pos != NULL) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(pos)));

            do {
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_1,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                BYTE nObjectType = pObject->GetObjectType();
                switch (nObjectType) {
                case 17:
                case 33:
                case 65:
                    if (pObject->m_posZ > 0) {
                        pObject->Render(this, pVidMode, nSurface);
                    }
                    break;
                case 48:
                    if (pObject->m_posZ > 0) {
                        if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bStaticAnimations
                            || (static_cast<CGameStatic*>(pObject)->m_header.m_dwFlags & 0x10) == 16) {
                            pObject->Render(this, pVidMode, nSurface);
                        }
                    }
                    break;
                default:
                    pObject->Render(this, pVidMode, nSurface);
                    break;
                }

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_1,
                    INFINITE);
            }
        }

        pos = m_lVertSortFlight.GetHeadPosition();
        while (pos != NULL) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlight.GetNext(pos)));

            do {
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_1,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                pObject->Render(this, pVidMode, nSurface);

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_1,
                    INFINITE);
            }
        }

        m_cInfinity.PostRender(pVidMode, nSurface, &m_search);

        if (g_pChitin->cVideo.Is3dAccelerated()) {
            CInfTileSet::sub_5D2DE0();
        }

        switch (m_pGame->GetState()) {
        case 0:
            if (abs(m_selectSquare.Width()) > 8 || abs(m_selectSquare.Height()) > 8) {
                CRect highlightRect;
                highlightRect.left = m_selectSquare.left;
                highlightRect.top = m_selectSquare.top;
                highlightRect.right = m_cInfinity.nCurrentX + min(max(m_selectSquare.right - m_cInfinity.nCurrentX, 0), m_cInfinity.rViewPort.Width() - 1);
                highlightRect.bottom = m_cInfinity.nCurrentY + min(max(m_selectSquare.bottom - m_cInfinity.nCurrentY, 0), m_cInfinity.rViewPort.Height() - 1);
                m_cInfinity.DrawHighlightRect(highlightRect, RGB(0, 255, 0), 1);
            }
            break;
        case 3:
            if (abs(m_selectSquare.Width()) > 8 || abs(m_selectSquare.Height()) > 8) {
                int width = abs(m_selectSquare.Width());
                int height = abs(m_selectSquare.Height());

                CSize axes;
                axes.cx = static_cast<LONG>(sqrt(width * width + (4 * height / 3) * (4 * height / 3)));
                axes.cy = 3 * axes.cx / 4;
                m_cInfinity.DrawEllipse(m_moveDest, axes, RGB(0, 255, 0));
            }
            break;
        }

        for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
            iObject = g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait);

            do {
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_1,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                static_cast<CGameSprite*>(pObject)->RenderDamageArrow(this, pVidMode, nSurface);

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_1,
                    INFINITE);
            }
        }
    }

    if (m_firstRender > 0) {
        if (!g_pBaldurChitin->m_pEngineWorld->m_bPaused) {
            g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->StartTime();
        }
        m_firstRender = 0;
    }

    LeaveCriticalSection(&field_1FC);
    g_pBaldurChitin->GetObjectGame()->EndListManipulation(this);
}

// 0x477EE0
void CGameArea::SetDay()
{
    m_sndAmbientNight.Stop();
    // TODO: Check cast.
    m_sndAmbientDayVolume = static_cast<WORD>(m_headerSound.m_dayAmbientVolume);

    if (m_sndAmbientDay.IsSoundPlaying()) {
        m_sndAmbientDay.SetVolume(m_sndAmbientDayVolume * m_sndAmbientVolume / 100);
    } else {
        if (g_pChitin->cDimm.GetMemoryAmount() == 1) {
            m_sndAmbientDay.SetResRef(CResRef(m_headerSound.m_dayAmbient), TRUE, TRUE);
        } else {
            m_sndAmbientDay.SetResRef(CResRef(m_headerSound.m_dayAmbientExtended), TRUE, TRUE);
        }

        if (m_sndAmbientDay.GetResRef() != "") {
            m_sndAmbientDay.SetLoopingFlag(TRUE);
            m_sndAmbientDay.SetChannel(1, reinterpret_cast<DWORD_PTR>(this));
            m_sndAmbientDay.SetVolume(m_sndAmbientDayVolume * m_sndAmbientVolume / 100);
            m_sndAmbientDay.Play(FALSE);
        }
    }

    if ((m_cInfinity.m_areaType & 2) != 0) {
        m_cInfinity.SetDay();
    }
}

// 0x4781B0
void CGameArea::SetNight()
{
    m_sndAmbientDay.Stop();
    // TODO: Check cast.
    m_sndAmbientNightVolume = static_cast<WORD>(m_headerSound.m_nightAmbientVolume);

    if (m_sndAmbientNight.IsSoundPlaying()) {
        m_sndAmbientNight.SetVolume(m_sndAmbientNightVolume * m_sndAmbientVolume / 100);
    } else {
        if (g_pChitin->cDimm.GetMemoryAmount() == 1) {
            m_sndAmbientNight.SetResRef(CResRef(m_headerSound.m_nightAmbient), TRUE, TRUE);
            if (m_sndAmbientNight.GetResRef() != "") {
                m_sndAmbientNight.SetLoopingFlag(TRUE);
                m_sndAmbientNight.SetChannel(1, reinterpret_cast<DWORD_PTR>(this));
                m_sndAmbientNight.SetVolume(m_sndAmbientNightVolume * m_sndAmbientVolume / 100);
                m_sndAmbientNight.Play(FALSE);
            }
        }
    }

    if ((m_cInfinity.m_areaType & 2) != 0) {
        m_cInfinity.SetNight();
    }
}

// 0x478480
void CGameArea::SetApproachingDawn()
{
    m_cInfinity.SetApproachingDawn();
}

// 0x478480
void CGameArea::SetApproachingDusk()
{
    m_cInfinity.SetApproachingDusk();
}

// 0x478490
void CGameArea::SetDawn(BYTE nIntensity, BOOLEAN bPlayDayNightMovie)
{
    m_sndAmbientDayVolume = static_cast<WORD>(nIntensity * m_headerSound.m_dayAmbientVolume / 256);
    if (m_sndAmbientDay.IsSoundPlaying()) {
        m_sndAmbientDay.SetVolume(m_sndAmbientVolume * m_sndAmbientDayVolume / 100);
    } else {
        if (m_sndAmbientVolume * m_sndAmbientDayVolume / 100 > 0) {
            if (g_pChitin->cDimm.GetMemoryAmount() == 1) {
                // NOTE: Uninline.
                m_sndAmbientDay.SetResRef(CResRef(m_headerSound.m_dayAmbient), TRUE, TRUE);
            } else {
                // NOTE: Uninline.
                m_sndAmbientDay.SetResRef(CResRef(m_headerSound.m_dayAmbientExtended), TRUE, TRUE);
            }

            if (m_sndAmbientDay.cResRef != "") {
                m_sndAmbientDay.SetLoopingFlag(1);
                m_sndAmbientDay.SetChannel(1, reinterpret_cast<DWORD_PTR>(this));
                m_sndAmbientDay.SetVolume(m_sndAmbientVolume * m_sndAmbientDayVolume / 100);
                m_sndAmbientDay.Play(FALSE);
            }
        }
    }

    m_sndAmbientNightVolume = static_cast<WORD>(nIntensity * m_headerSound.m_nightAmbientVolume / 256);
    if (m_sndAmbientNight.IsSoundPlaying()) {
        m_sndAmbientNight.SetVolume(m_sndAmbientVolume * m_sndAmbientNightVolume / 100);
    } else {
        if (m_sndAmbientVolume * m_sndAmbientNightVolume / 100 > 0) {
            if (g_pChitin->cDimm.GetMemoryAmount() == 1) {
                // NOTE: Uninline.
                m_sndAmbientNight.SetResRef(CResRef(m_headerSound.m_nightAmbient), TRUE, TRUE);
            } else {
                // NOTE: Uninline.
                m_sndAmbientNight.SetResRef(CResRef(m_headerSound.m_nightAmbientExtended), TRUE, TRUE);
            }

            if (m_sndAmbientNight.cResRef != "") {
                m_sndAmbientNight.SetLoopingFlag(1);
                m_sndAmbientNight.SetChannel(1, reinterpret_cast<DWORD_PTR>(this));
                m_sndAmbientNight.SetVolume(m_sndAmbientVolume * m_sndAmbientNightVolume / 100);
                m_sndAmbientNight.Play(FALSE);
            }
        }
    }

    if (nIntensity == CInfinity::TRUE_DAWNDUSK_INTENSITY) {
        char nSong = GetSong(0);
        g_pBaldurChitin->RemoveMusicPosition(m_resRef, nSong);
        PlaySong(0, 0x4 | 0x2);
    }

    if ((m_cInfinity.m_areaType & 0x2) != 0) {
        m_cInfinity.SetDawn(nIntensity, bPlayDayNightMovie);
    }
}

// 0x478AC0
void CGameArea::SetDusk(BYTE nIntensity, BOOLEAN bPlayDayNightMovie)
{
    m_sndAmbientDayVolume = static_cast<WORD>(nIntensity * m_headerSound.m_dayAmbientVolume / 256);
    if (m_sndAmbientDay.IsSoundPlaying()) {
        m_sndAmbientDay.SetVolume(m_sndAmbientVolume * m_sndAmbientDayVolume / 100);
    } else {
        if (m_sndAmbientVolume * m_sndAmbientDayVolume / 100 > 0) {
            if (g_pChitin->cDimm.GetMemoryAmount() == 1) {
                // NOTE: Uninline.
                m_sndAmbientDay.SetResRef(CResRef(m_headerSound.m_dayAmbient), TRUE, TRUE);
            } else {
                // NOTE: Uninline.
                m_sndAmbientDay.SetResRef(CResRef(m_headerSound.m_dayAmbientExtended), TRUE, TRUE);
            }

            if (m_sndAmbientDay.cResRef != "") {
                m_sndAmbientDay.SetLoopingFlag(1);
                m_sndAmbientDay.SetChannel(1, reinterpret_cast<DWORD_PTR>(this));
                m_sndAmbientDay.SetVolume(m_sndAmbientVolume * m_sndAmbientDayVolume / 100);
                m_sndAmbientDay.Play(FALSE);
            }
        }
    }

    m_sndAmbientNightVolume = static_cast<WORD>(nIntensity * m_headerSound.m_nightAmbientVolume / 256);
    if (m_sndAmbientNight.IsSoundPlaying()) {
        m_sndAmbientNight.SetVolume(m_sndAmbientVolume * m_sndAmbientNightVolume / 100);
    } else {
        if (m_sndAmbientVolume * m_sndAmbientNightVolume / 100 > 0) {
            if (g_pChitin->cDimm.GetMemoryAmount() == 1) {
                // NOTE: Uninline.
                m_sndAmbientNight.SetResRef(CResRef(m_headerSound.m_nightAmbient), TRUE, TRUE);
            } else {
                // NOTE: Uninline.
                m_sndAmbientNight.SetResRef(CResRef(m_headerSound.m_nightAmbientExtended), TRUE, TRUE);
            }

            if (m_sndAmbientNight.cResRef != "") {
                m_sndAmbientNight.SetLoopingFlag(1);
                m_sndAmbientNight.SetChannel(1, reinterpret_cast<DWORD_PTR>(this));
                m_sndAmbientNight.SetVolume(m_sndAmbientVolume * m_sndAmbientNightVolume / 100);
                m_sndAmbientNight.Play(FALSE);
            }
        }
    }

    if (nIntensity == CInfinity::TRUE_DAWNDUSK_INTENSITY) {
        PlaySong(1, 0x4 | 0x2);
    }

    if ((m_cInfinity.m_areaType & 0x2) != 0) {
        m_cInfinity.SetDusk(nIntensity, bPlayDayNightMovie);
    }
}

// 0x4790D0
void CGameArea::SetNewResWED(CResWED* pNewResWED)
{
    POSITION pos;
    CTiledObject* pTiledObject;

    m_pResWED = pNewResWED;

    pos = m_lTiledObjects.GetHeadPosition();
    while (pos != NULL) {
        pTiledObject = m_lTiledObjects.GetNext(pos);
        pTiledObject->SetNewResWED(pNewResWED);
    }
}

// 0x479110
void CGameArea::SetTimeOfDay(ULONG nTimeOfDay, BOOLEAN bPlayDayNightMovie)
{
    m_sndAmbientVolume = g_pBaldurChitin->m_pEngineWorld->m_weather.GetWind();
    m_sndAmbientDay.SetVolume(m_sndAmbientDayVolume * m_sndAmbientVolume / 100);
    m_sndAmbientNight.SetVolume(m_sndAmbientNightVolume * m_sndAmbientVolume / 100);

    if (CTimerWorld::IsDay(nTimeOfDay)) {
        SetDay();
    } else if (CTimerWorld::IsDusk(nTimeOfDay)) {
        SetDusk(255 - static_cast<BYTE>(((nTimeOfDay - CTimerWorld::TIME_DUSK) * 256) / (CTimerWorld::TIME_NIGHT - CTimerWorld::TIME_DUSK)), bPlayDayNightMovie);
    } else if (CTimerWorld::IsNight(nTimeOfDay)) {
        SetNight();
    } else if (CTimerWorld::IsDawn(nTimeOfDay)) {
        SetDawn(static_cast<BYTE>(((nTimeOfDay - CTimerWorld::TIME_DAWN) * 256) / (CTimerWorld::TIME_DAY - CTimerWorld::TIME_DAWN)), bPlayDayNightMovie);
    }
}

// 0x479260
void CGameArea::SetSoundEnvironment()
{
    // NOTE: Not sure why they are explicit. Probably remapping from one enum
    // to other.
    switch (m_headerSound.m_reverb) {
    case 0:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_GENERIC);
        break;
    case 1:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_PADDEDCELL);
        break;
    case 2:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_ROOM);
        break;
    case 3:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_BATHROOM);
        break;
    case 4:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_LIVINGROOM);
        break;
    case 5:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_STONEROOM);
        break;
    case 6:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_AUDITORIUM);
        break;
    case 7:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_CONCERTHALL);
        break;
    case 8:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_CAVE);
        break;
    case 9:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_ARENA);
        break;
    case 10:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_HANGAR);
        break;
    case 11:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_CARPETEDHALLWAY);
        break;
    case 12:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_HALLWAY);
        break;
    case 13:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_STONECORRIDOR);
        break;
    case 14:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_ALLEY);
        break;
    case 15:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_FOREST);
        break;
    case 16:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_CITY);
        break;
    case 17:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_MOUNTAINS);
        break;
    case 18:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_QUARRY);
        break;
    case 19:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_PLAIN);
        break;
    case 20:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_PARKINGLOT);
        break;
    case 21:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_SEWERPIPE);
        break;
    case 22:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_UNDERWATER);
        break;
    case 23:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_DRUGGED);
        break;
    case 24:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_DIZZY);
        break;
    case 25:
        g_pBaldurChitin->cSoundMixer.m_cSoundProperties.SetEnvironmentPreset(EAX_ENVIRONMENT_PSYCHOTIC);
        break;
    }
}

// 0x4794E0
void CGameArea::SortLists()
{
    CTypedPtrList<CPtrList, CGameObject*> sortObjectsAdd;
    CGameObject** pObjects = NULL;
    LONG iObject;
    CGameObject* pObject;
    BYTE rc;
    POSITION pos;
    INT cnt;
    INT cnt2;

    EnterCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));

    if (AfxIsValidAddress(&m_header, sizeof(m_header), 1)) {
        g_pBaldurChitin->GetObjectGame()->BeginListManipulation(this);

        INT nFrontAdd = m_lVertSortAdd.GetCount();
        INT nFront = m_lVertSort.GetCount() - m_lVertSortRemove.GetCount();

        BOOLEAN bSort;
        // The periodic re-sort runs while the byte at 0x3DA is clear
        // (`*(char *)(this + 0x3da) != '\0'` skips it, 0x4794E0).
        if (nFrontAdd != 0
            || (m_firstRender == 0
                && m_pGame->GetWorldTimer()->m_gameTime % 5 == 0
                && g_pBaldurChitin->GetObjectGame()->GetVisibleArea() == this)) {
            bSort = TRUE;

            // With nothing to sort the original allocates nothing and goes
            // on to the back and flight lists.
            if (nFront + nFrontAdd != 0) {
                pObjects = new CGameObject*[nFront + nFrontAdd];
                if (pObjects == NULL) {
                    g_pBaldurChitin->GetObjectGame()->EndListManipulation(this);
                    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));
                    return;
                }
            }
        } else {
            bSort = FALSE;
        }

        while (!m_lVertSortAdd.IsEmpty()) {
            iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortAdd.RemoveHead()));

            do {
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                pos = sortObjectsAdd.GetHeadPosition();
                while (pos != NULL) {
                    if (sortObjectsAdd.GetAt(pos)->GetPos().y >= pObject->GetPos().y) {
                        break;
                    }
                    sortObjectsAdd.GetNext(pos);
                }

                if (pos != NULL) {
                    sortObjectsAdd.InsertBefore(pos, pObject);
                } else {
                    sortObjectsAdd.AddTail(pObject);
                }

                m_pGame->GetObjectArray()->ReleaseShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            } else {
                // A queued object deleted meanwhile is not sorted: one
                // fewer queued object (`local_40 = iVar6 - 1`).
                nFrontAdd--;

                if (rc != CGameObjectArray::DELETED) {
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                    // __LINE__: 8076
                    UTIL_ASSERT(FALSE);
                }
            }
        }

        cnt = 0;
        while (!sortObjectsAdd.IsEmpty()) {
            pObjects[cnt++] = sortObjectsAdd.RemoveHead();
        }

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 8099
        UTIL_ASSERT(cnt == nFrontAdd);

        while (!m_lVertSortBackRemove.IsEmpty()) {
            m_lVertSortBack.RemoveAt(m_lVertSortBackRemove.RemoveHead());
        }

        while (!m_lVertSortBackAdd.IsEmpty()) {
            do {
                // FIXME: Should be outside of the loop.
                iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortBackAdd.RemoveHead()));
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS && rc != CGameObjectArray::DELETED) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 8148
                UTIL_ASSERT(FALSE);
            }

            if (rc != CGameObjectArray::DELETED) {
                POSITION posVertList;
                if (m_lVertSortBack.IsEmpty()) {
                    posVertList = m_lVertSortBack.AddTail(reinterpret_cast<int*>(pObject->GetId()));
                } else {
                    posVertList = m_lVertSortBack.InsertAfter(m_lVertSortBack.GetTailPosition(),
                        reinterpret_cast<int*>(pObject->GetId()));
                }
                pObject->SetVertListPos(posVertList);

                m_pGame->GetObjectArray()->ReleaseShare(pObject->GetId(),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        while (!m_lVertSortFlightRemove.IsEmpty()) {
            m_lVertSortFlight.RemoveAt(m_lVertSortFlightRemove.RemoveHead());
        }

        while (!m_lVertSortFlightAdd.IsEmpty()) {
            do {
                // FIXME: Should be outside of the loop.
                iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSortFlightAdd.RemoveHead()));
                rc = m_pGame->GetObjectArray()->GetShare(iObject,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS && rc != CGameObjectArray::DELETED) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                // __LINE__: 8195
                UTIL_ASSERT(FALSE);
            }

            if (rc != CGameObjectArray::DELETED) {
                POSITION posVertList;
                if (m_lVertSortFlight.IsEmpty()) {
                    posVertList = m_lVertSortFlight.AddTail(reinterpret_cast<int*>(pObject->GetId()));
                } else {
                    posVertList = m_lVertSortFlight.InsertAfter(m_lVertSortFlight.GetTailPosition(),
                        reinterpret_cast<int*>(pObject->GetId()));
                }
                pObject->SetVertListPos(posVertList);

                m_pGame->GetObjectArray()->ReleaseShare(pObject->GetId(),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        while (!m_lVertSortRemove.IsEmpty()) {
            m_lVertSort.RemoveAt(m_lVertSortRemove.RemoveHead());
        }

        if (bSort) {
            while (!m_lVertSort.IsEmpty()) {
                iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.RemoveHead()));

                do {
                    rc = m_pGame->GetObjectArray()->GetShare(iObject,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::DENIED);

                if (rc != CGameObjectArray::SUCCESS) {
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
                    // __LINE__: 8263
                    UTIL_ASSERT(FALSE);
                }

                // NOTE: Stored after the assert as well, as the original
                // does, so the count stays aligned.
                pObjects[cnt++] = pObject;
            }

            m_lVertSort.RemoveAll();

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
            // __LINE__: 8268
            UTIL_ASSERT(cnt == nFront + nFrontAdd);

            // The front objects follow the `nFrontAdd` queued ones in
            // `pObjects`: a bubble sort of them by y (a pass with no swap
            // ends it), then a merge with the queued ones, which go first
            // on equal y (0x4794E0).
            if (nFront > 1) {
                for (cnt = 0; cnt < nFront - 1; cnt++) {
                    BOOL bDone = TRUE;

                    for (cnt2 = 1; cnt2 < nFront - cnt; cnt2++) {
                        if (pObjects[nFrontAdd + cnt2 - 1]->GetPos().y > pObjects[nFrontAdd + cnt2]->GetPos().y) {
                            pObject = pObjects[nFrontAdd + cnt2 - 1];
                            pObjects[nFrontAdd + cnt2 - 1] = pObjects[nFrontAdd + cnt2];
                            pObjects[nFrontAdd + cnt2] = pObject;
                            bDone = FALSE;
                        }
                    }

                    if (bDone) {
                        break;
                    }
                }
            }

            cnt = 0;
            cnt2 = 0;
            while (cnt < nFrontAdd || cnt2 < nFront) {
                if (cnt2 != nFront
                    && (cnt == nFrontAdd || pObjects[cnt]->GetPos().y > pObjects[nFrontAdd + cnt2]->GetPos().y)) {
                    pos = m_lVertSort.AddTail(reinterpret_cast<int*>(pObjects[nFrontAdd + cnt2]->GetId()));
                    pObjects[nFrontAdd + cnt2]->SetVertListPos(pos);
                    m_pGame->GetObjectArray()->ReleaseShare(pObjects[nFrontAdd + cnt2]->GetId(),
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    cnt2++;
                } else {
                    pos = m_lVertSort.AddTail(reinterpret_cast<int*>(pObjects[cnt]->GetId()));
                    pObjects[cnt]->SetVertListPos(pos);
                    m_pGame->GetObjectArray()->ReleaseShare(pObjects[cnt]->GetId(),
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    cnt++;
                }
            }
        }

        g_pBaldurChitin->GetObjectGame()->EndListManipulation(this);
    }

    LeaveCriticalSection(&(g_pBaldurChitin->m_pEngineWorld->field_106));

    if (pObjects != NULL) {
        delete pObjects;
    }
}

// 0x479CC0
BOOL CGameArea::SetSong(SHORT slot, BYTE song)
{
    switch (slot) {
    case 0:
        m_headerSound.m_dayMusic = song;
        return TRUE;
    case 1:
        m_headerSound.m_nightMusic = song;
        return TRUE;
    case 2:
        m_headerSound.m_battleWinningMusic = song;
        return TRUE;
    case 3:
        m_headerSound.m_battleStandOffMusic = song;
        return TRUE;
    case 4:
        m_headerSound.m_battleLosingMusic = song;
        return TRUE;
    case 5:
        m_headerSound.m_alt1Music0 = song;
        return TRUE;
    case 6:
        m_headerSound.m_alt1Music1 = song;
        return TRUE;
    case 7:
        m_headerSound.m_alt1Music2 = song;
        return TRUE;
    case 8:
        m_headerSound.m_alt1Music3 = song;
        return TRUE;
    case 9:
        m_headerSound.m_alt1Music4 = song;
        return TRUE;
    default:
        return FALSE;
    }
}

// 0x479DB0
BYTE CGameArea::GetSong(SHORT slot)
{
    if (slot > 256) {
        return -1;
    }

    // TODO: Check casts.

    BYTE song;
    if (slot != 256) {
        switch (slot) {
        case 0:
            song = static_cast<BYTE>(m_headerSound.m_dayMusic);
            break;
        case 1:
            song = static_cast<BYTE>(m_headerSound.m_nightMusic);
            break;
        case 2:
            song = static_cast<BYTE>(m_headerSound.m_battleWinningMusic);
            break;
        case 3:
            song = static_cast<BYTE>(m_headerSound.m_battleStandOffMusic);
            break;
        case 4:
            song = static_cast<BYTE>(m_headerSound.m_battleLosingMusic);
            break;
        case 5:
            song = static_cast<BYTE>(m_headerSound.m_alt1Music0);
            break;
        case 6:
            song = static_cast<BYTE>(m_headerSound.m_alt1Music1);
            break;
        case 7:
            song = static_cast<BYTE>(m_headerSound.m_alt1Music2);
            break;
        case 8:
            song = static_cast<BYTE>(m_headerSound.m_alt1Music3);
            break;
        case 9:
            song = static_cast<BYTE>(m_headerSound.m_alt1Music4);
            break;
        default:
            return -1;
        }
    } else {
        if (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay() || g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDusk()) {
            song = static_cast<BYTE>(m_headerSound.m_dayMusic);
        } else {
            song = static_cast<BYTE>(m_headerSound.m_nightMusic);
        }
    }

    if (song == 0) {
        song = -1;
    }

    return song;
}

// 0x479E80
void CGameArea::PlaySong(SHORT slot, DWORD flags)
{
    if (slot == 2 || slot == 3 || slot == 4) {
        if (m_headerSound.m_battleStandOffMusic == 0
            || m_headerSound.m_battleStandOffMusic == -1) {
            return;
        }
    }

    m_nCurrentSong = slot;

    if (slot == 3) {
        m_nBattleSongCounter = 150;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetVisibleArea() == this) {
        INT nSong = GetSong(slot);
        if (nSong != g_pChitin->cSoundMixer.m_nCurrentSong) {
            INT nSection;
            INT nPosition;
            g_pBaldurChitin->GetMusicPosition(m_resRef, nSong, nSection, nPosition);

            if (slot == 2 || slot == 3 || slot == 4) {
                g_pBaldurChitin->cSoundMixer.StartSong(nSong, 0x1);
            } else if (nSong == -1 && flags == 0x1) {
                g_pBaldurChitin->cSoundMixer.StartSong(-1, nSection, nPosition, 0x2 | 0x1);
            } else {
                g_pBaldurChitin->cSoundMixer.StartSong(nSong, nSection, nPosition, flags);
            }

            if (g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE) {
                CMessageStartCombatMusic* pStartCombatMusic = new CMessageStartCombatMusic(slot,
                    m_nAIIndex,
                    m_nAIIndex);

                g_pBaldurChitin->GetMessageHandler()->AddMessage(pStartCombatMusic, FALSE);
            }
        }
    }
}

// 0x68BFF0
CAreaFileHeader* CGameArea::GetHeader()
{
    return &m_header;
}

// 0x453080
CInfinity* CGameArea::GetInfinity()
{
    return &m_cInfinity;
}

// 0x453090
CVariableHash* CGameArea::GetVariables()
{
    return &m_variables;
}

// 0x4530A0
CAreaFileRestEncounter* CGameArea::GetHeaderRestEncounter()
{
    return &m_headerRestEncounter;
}

// 0x40CAF0
CNamedCreatureVariableHash* CGameArea::GetNamedCreatures()
{
    return &m_namedCreatures;
}

// 0x43A5B0
CGameAreaNotes* CGameArea::GetAreaNotes()
{
    return &m_cGameAreaNotes;
}

// 0x45B670
LONG CGameArea::GetBattleSongCounter()
{
    return m_nBattleSongCounter;
}

// 0x453070;
BYTE CGameArea::GetId()
{
    return m_id;
}

// 0x47A060
void CGameArea::sub_47A060()
{
    CMemINISection* pSection = INIFile.GetSection(CString("locals"));
    if (pSection != NULL && pSection->GetCount() > 0) {
        POSITION pos = pSection->GetHeadPosition();
        while (pos != NULL) {
            CMemINIValue* pValue = pSection->GetNext(pos);
            if (pValue != NULL) {
                CVariable* pVariable = CGameAIBase::GetGlobalVariable(CString("GLOBAL"),
                    pValue->GetName(),
                    0);
                if (pVariable != NULL) {
                    pVariable->m_intValue = atoi(pValue->GetValue());
                }
            }
        }
    }
}

// 0x47A140
int CGameArea::ExitSpawn()
{
    if (mpSpawner != NULL) {
        return mpSpawner->ExitSpawn(this);
    } else {
        return 0;
    }
}

// 0x47A160
int CGameArea::EnterSpawn()
{
    if (mpSpawner != NULL && !g_pBaldurChitin->m_pEngineLoad->m_bInLoadGame) {
        return mpSpawner->EnterSpawn(this);
    } else {
        return 0;
    }
}

// 0x47A190
void CGameArea::sub_47A190()
{
    g_pBaldurChitin->AddMusicPosition(m_resRef, g_pBaldurChitin->cSoundMixer.m_nCurrentSong);
}

// NOTE: Inlined.
void CGameArea::AddToMarkers(LONG id)
{
    if (m_pObjectMarker != NULL) {
        m_pObjectMarker->AddObject(id);
    }
}

// NOTE: Inlined.
void CGameArea::RemoveFromMarkers(LONG id)
{
    if (m_pObjectMarker) {
        m_pObjectMarker->RemoveObject(id);
    }
}

// -----------------------------------------------------------------------------

// The first live creature next to an object (in the front vertical-sort
// list, looking back from the object first) whose personal space covers one
// of the points: the creature standing in a closing door's way (ToggleDoor
// 0x489AB0). The creature is returned shared; NULL when there is none.
//
// 0x47A1C0
CGameSprite* CGameArea::sub_47A1C0(LONG nObjectId, const CPoint* pPoints, USHORT nPoints)
{
    CAIObjectType typeAI(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    CGameObject* pObject;
    CGameObjectArray* pObjectArray = m_pGame->GetObjectArray();

    if (pObjectArray->GetShare(nObjectId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
        return NULL;
    }

    typeAI.Set(pObject->GetAIType());

    if (pObject->GetVertListType() != CGameObject::LIST_FRONT) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameArea.cpp
        // __LINE__: 9002
        UTIL_ASSERT(FALSE);
    }

    pObject->GetPos();
    POSITION posStart = pObject->GetVertListPos();
    pObjectArray->ReleaseShare(nObjectId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    // NOTE: The original walks the list's nodes directly: back from the
    // object through pPrev, and forward through pNext.
    POSITION posNext = posStart;
    POSITION posPrev = posStart;
    m_lVertSort.GetNext(posNext);
    m_lVertSort.GetPrev(posPrev);

    while (posPrev != NULL || posNext != NULL) {
        BOOL bNext = posPrev == NULL;
        if (!bNext) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetPrev(posPrev)));
            if (pObjectArray->GetShare(nId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
                pObjectArray->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                continue;
            }

            CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
            if (pSprite->m_active
                && pSprite->m_activeAI
                && pSprite->m_activeImprisonment
                && pSprite->GetVertListPos() != NULL) {
                const CPoint& pos = pSprite->GetPos();
                CPoint ptGrid(pos.x / CPathSearch::GRID_SQUARE_SIZEX, pos.y / CPathSearch::GRID_SQUARE_SIZEY);
                if (m_search.sub_54A210(ptGrid, pSprite->GetAnimation()->GetPersonalSpace(), pPoints, nPoints) == TRUE) {
                    return pSprite;
                }
            }

            pObjectArray->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (posNext == NULL) {
                continue;
            }
        }

        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lVertSort.GetNext(posNext)));
        if (pObjectArray->GetShare(nId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
            if (pSprite->m_active
                && pSprite->m_activeAI
                && pSprite->m_activeImprisonment
                && pSprite->GetVertListPos() != NULL) {
                const CPoint& pos = pSprite->GetPos();
                CPoint ptGrid(pos.x / CPathSearch::GRID_SQUARE_SIZEX, pos.y / CPathSearch::GRID_SQUARE_SIZEY);
                if (m_search.sub_54A210(ptGrid, pSprite->GetAnimation()->GetPersonalSpace(), pPoints, nPoints) == TRUE) {
                    return pSprite;
                }
            }
        }

        pObjectArray->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }

    return NULL;
}

// 0x47A020
BYTE* CAreaFile::GetData()
{
    if (pRes != NULL) {
        return static_cast<BYTE*>(pRes->Demand());
    } else {
        return NULL;
    }
}

// 0x47A030
DWORD CAreaFile::GetDataSize()
{
    if (pRes == NULL) {
        return 0;
    }

    if (pRes->Demand() == NULL) {
        return 0;
    }

    DWORD nSize = pRes->m_nResSizeActual;

    pRes->Release();

    return nSize;
}

// 0x5C7B00
void CAreaFile::ReleaseData()
{
    if (pRes != NULL) {
        pRes->Release();
    }
}
