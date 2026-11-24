#include "CScreenWorldMap.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CRuleTables.h"
#include "CSearchBitmap.h"
#include "CWorldMap.h"
#include "CScreenCharacter.h"
#include "CScreenMap.h"
#include "CScreenWorld.h"
#include "CTimerWorld.h"
#include "CUIControlEdit.h"
#include "CUIControlScrollBar.h"
#include "CUIControlTextDisplay.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVidInf.h"

// 0x8F89DC
const LONG CScreenWorldMap::FATIGUE_TIME_CAP = 24 * CTimerWorld::TIMESCALE_MSEC_PER_HOUR;

// 0x698860
CScreenWorldMap::CScreenWorldMap()
{
    m_nEngineState = 0;
    m_pMapControl = NULL;
    field_FF2 = 0;
    field_FF6 = 0;
    m_nLeavingEdge = 0;
    m_nCurrentLink = 0;
    m_bInControl = FALSE;
    m_bClickedArea = FALSE;
    field_1050 = 0;

    SetVideoMode(0);

    m_pVirtualKeys[0] = CKeyInfo(VK_ESCAPE, -1, 0);
    m_pVirtualKeys[1] = CKeyInfo('1', 0, 0);
    m_pVirtualKeys[2] = CKeyInfo('2', 0, 0);
    m_pVirtualKeys[3] = CKeyInfo('3', 0, 0);
    m_pVirtualKeys[4] = CKeyInfo('4', 0, 0);
    m_pVirtualKeys[5] = CKeyInfo('5', 0, 0);
    m_pVirtualKeys[6] = CKeyInfo('6', 0, 0);
    m_pVirtualKeys[7] = CKeyInfo('7', 0, 0);
    m_pVirtualKeys[8] = CKeyInfo('8', 0, 0);
    m_pVirtualKeys[9] = CKeyInfo('9', 0, 0);
    m_pVirtualKeys[10] = CKeyInfo(VK_SPACE, -1, 0);
    m_pVirtualKeys[11] = CKeyInfo('A', 0, 0);
    m_pVirtualKeys[12] = CKeyInfo('B', 0, 0);
    m_pVirtualKeys[13] = CKeyInfo('C', 0, 0);
    m_pVirtualKeys[14] = CKeyInfo('D', 0, 0);
    m_pVirtualKeys[15] = CKeyInfo('E', 0, 0);
    m_pVirtualKeys[16] = CKeyInfo('F', 0, 0);
    m_pVirtualKeys[17] = CKeyInfo('G', 0, 0);
    m_pVirtualKeys[18] = CKeyInfo('H', 0, 0);
    m_pVirtualKeys[19] = CKeyInfo('I', 0, 0);
    m_pVirtualKeys[20] = CKeyInfo('J', 0, 0);
    m_pVirtualKeys[21] = CKeyInfo('K', 0, 0);
    m_pVirtualKeys[22] = CKeyInfo('L', 0, 0);
    m_pVirtualKeys[23] = CKeyInfo('M', 0, 0);
    m_pVirtualKeys[24] = CKeyInfo('N', 0, 0);
    m_pVirtualKeys[25] = CKeyInfo('O', 0, 0);
    m_pVirtualKeys[26] = CKeyInfo('P', 0, 0);
    m_pVirtualKeys[27] = CKeyInfo('Q', 0, 0);
    m_pVirtualKeys[28] = CKeyInfo('R', 0, 0);
    m_pVirtualKeys[29] = CKeyInfo('S', 0, 0);
    m_pVirtualKeys[30] = CKeyInfo('T', 0, 0);
    m_pVirtualKeys[31] = CKeyInfo('U', 0, 0);
    m_pVirtualKeys[32] = CKeyInfo('V', 0, 0);
    m_pVirtualKeys[33] = CKeyInfo('W', 0, 0);
    m_pVirtualKeys[34] = CKeyInfo('X', 0, 0);
    m_pVirtualKeys[35] = CKeyInfo('Y', 0, 0);
    m_pVirtualKeys[36] = CKeyInfo('Z', 0, 0);
    m_pVirtualKeys[37] = CKeyInfo('0', 0, 0);
    m_pVirtualKeys[38] = CKeyInfo(VK_BACK, 0, 0);
    m_pVirtualKeys[39] = CKeyInfo(VK_END, -1, 0);
    m_pVirtualKeys[40] = CKeyInfo(VK_HOME, -1, 0);
    m_pVirtualKeys[41] = CKeyInfo(VK_LEFT, 0, 0);
    m_pVirtualKeys[42] = CKeyInfo(VK_UP, 0, 0);
    m_pVirtualKeys[43] = CKeyInfo(VK_RIGHT, 0, 0);
    m_pVirtualKeys[44] = CKeyInfo(VK_DOWN, 0, 0);
    m_pVirtualKeys[45] = CKeyInfo(VK_SNAPSHOT, -1, 0);
    m_pVirtualKeys[46] = CKeyInfo(VK_INSERT, -1, 0);
    m_pVirtualKeys[47] = CKeyInfo(VK_DELETE, 0, 0);
    m_pVirtualKeys[48] = CKeyInfo(VK_NUMPAD0, -1, 0);
    m_pVirtualKeys[49] = CKeyInfo(VK_NUMPAD1, -1, 0);
    m_pVirtualKeys[50] = CKeyInfo(VK_NUMPAD2, -1, 0);
    m_pVirtualKeys[51] = CKeyInfo(VK_NUMPAD3, -1, 0);
    m_pVirtualKeys[52] = CKeyInfo(VK_NUMPAD4, -1, 0);
    m_pVirtualKeys[53] = CKeyInfo(VK_NUMPAD5, -1, 0);
    m_pVirtualKeys[54] = CKeyInfo(VK_NUMPAD6, -1, 0);
    m_pVirtualKeys[55] = CKeyInfo(VK_NUMPAD7, -1, 0);
    m_pVirtualKeys[56] = CKeyInfo(VK_NUMPAD8, -1, 0);
    m_pVirtualKeys[57] = CKeyInfo(VK_NUMPAD9, -1, 0);
    m_pVirtualKeys[58] = CKeyInfo(VK_MULTIPLY, -1, 0);
    m_pVirtualKeys[59] = CKeyInfo(VK_ADD, -1, 0);
    m_pVirtualKeys[60] = CKeyInfo(VK_SEPARATOR, -1, 0);
    m_pVirtualKeys[61] = CKeyInfo(VK_SUBTRACT, -1, 0);
    m_pVirtualKeys[62] = CKeyInfo(VK_DECIMAL, -1, 0);
    m_pVirtualKeys[63] = CKeyInfo(VK_DIVIDE, -1, 0);
    m_pVirtualKeys[64] = CKeyInfo(VK_F1, -1, 0);
    m_pVirtualKeys[65] = CKeyInfo(VK_F2, -1, 0);
    m_pVirtualKeys[66] = CKeyInfo(VK_F3, -1, 0);
    m_pVirtualKeys[67] = CKeyInfo(VK_F4, -1, 0);
    m_pVirtualKeys[68] = CKeyInfo(VK_F5, -1, 0);
    m_pVirtualKeys[69] = CKeyInfo(VK_F6, -1, 0);
    m_pVirtualKeys[70] = CKeyInfo(VK_F7, -1, 0);
    m_pVirtualKeys[71] = CKeyInfo(VK_F8, -1, 0);
    m_pVirtualKeys[72] = CKeyInfo(VK_F9, -1, 0);
    m_pVirtualKeys[73] = CKeyInfo(VK_F10, -1, 0);
    m_pVirtualKeys[74] = CKeyInfo(VK_F11, -1, 0);
    m_pVirtualKeys[75] = CKeyInfo(VK_F12, -1, 0);
    m_pVirtualKeys[76] = CKeyInfo(VK_NUMLOCK, -1, 0);
    m_pVirtualKeys[77] = CKeyInfo(VK_RETURN, -1, 0);
    m_pVirtualKeys[78] = CKeyInfo(VK_OEM_3, 0, 0);
    m_pVirtualKeys[79] = CKeyInfo(VK_OEM_MINUS, 0, 0);
    m_pVirtualKeys[80] = CKeyInfo(VK_OEM_PLUS, 0, 0);
    m_pVirtualKeys[81] = CKeyInfo(VK_OEM_4, 0, 0);
    m_pVirtualKeys[82] = CKeyInfo(VK_OEM_6, 0, 0);
    m_pVirtualKeys[83] = CKeyInfo(VK_OEM_5, 0, 0);
    m_pVirtualKeys[84] = CKeyInfo(VK_OEM_1, 0, 0);
    m_pVirtualKeys[85] = CKeyInfo(VK_OEM_7, 0, 0);
    m_pVirtualKeys[86] = CKeyInfo(VK_OEM_COMMA, 0, 0);
    m_pVirtualKeys[87] = CKeyInfo(VK_OEM_PERIOD, 0, 0);
    m_pVirtualKeys[88] = CKeyInfo(VK_OEM_2, 0, 0);
    m_pVirtualKeys[89] = CKeyInfo(VK_TAB, -1, 0);

    m_bCtrlKeyDown = FALSE;
    m_bShiftKeyDown = FALSE;
    m_ptMapView.x = 0;
    m_ptMapView.y = 0;
    field_FCA = RGB(224, 180, 15);
    m_vfLabel.SetColor(field_FCA, 0, FALSE);
    m_nHighlightArea = -1;
    m_nSelectedArea = -1;
    m_bOverSelectedArea = FALSE;
    m_nToolTip = 0;
    m_ptMapStartMousePos.x = 0;
    m_ptMapStartMousePos.y = 0;
    m_ptMapStartView.x = 0;
    m_ptMapStartView.y = 0;
    m_bMapLeftDown = FALSE;
    m_bMapDragging = FALSE;
    m_pPath = NULL;
    m_pMainPanel = NULL;
    m_pChatDisplay = NULL;
    m_nChatMessageCount = 0;
}

// 0x49FC40
BOOL CScreenWorldMap::CheckSystemKeyShift()
{
    return TRUE;
}

// 0x6995A0
void CScreenWorldMap::SetSystemKeyShift(BOOL bValue)
{
    m_bShiftKeyDown = bValue;
}

// 0x6995B0
BOOL CScreenWorldMap::GetShiftKey()
{
    return m_bShiftKeyDown;
}

// 0x49FC40
BOOL CScreenWorldMap::CheckSystemKeyCapsLock()
{
    return TRUE;
}

// 0x6995C0
void CScreenWorldMap::SetSystemKeyCapsLock(BOOL bValue)
{
    m_bCapsLockKeyOn = bValue;
}

// 0x6995D0
BOOL CScreenWorldMap::GetCapsLockKey()
{
    return m_bCapsLockKeyOn;
}

// 0x49FC40
BOOL CScreenWorldMap::CheckSystemKeyCtrl()
{
    return TRUE;
}

// 0x6995E0
void CScreenWorldMap::SetSystemKeyCtrl(BOOLEAN bValue)
{
    m_bCtrlKeyDown = bValue;
}

// 0x6995F0
BOOL CScreenWorldMap::GetCtrlKey()
{
    return m_bCtrlKeyDown;
}

// 0x699600
SHORT CScreenWorldMap::GetNumVirtualKeys()
{
    return CSCREENWORLDMAP_VIRTUAL_KEYS;
}

// 0x699610
CKeyInfo* CScreenWorldMap::GetVirtualKeys()
{
    return m_pVirtualKeys;
}

// 0x699620
BYTE* CScreenWorldMap::GetVirtualKeysFlags()
{
    return m_pVirtualKeysFlags;
}

// 0x699650
CScreenWorldMap::~CScreenWorldMap()
{
    // NOTE: The original only runs the members' destructors; `m_pPath` is
    // not freed here.
}

// 0x699760
void CScreenWorldMap::EngineActivated()
{
    if (CChitin::byte_8FB950
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0);
    }

    if (m_cUIManager.m_bInitialized) {
        m_preLoadFontRealms.SetResRef(CResRef("REALMS"), FALSE, TRUE);
        m_preLoadFontRealms.RegisterFont();

        m_preLoadFontTool.SetResRef(CResRef("TOOLFONT"), FALSE, TRUE);
        m_preLoadFontTool.RegisterFont();

        CUIPanel* pLeftPanel = m_cUIManager.GetPanel(0);
        CUIPanel* pRightPanel = m_cUIManager.GetPanel(1);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 328
        UTIL_ASSERT(pLeftPanel != NULL);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 329
        UTIL_ASSERT(pRightPanel != NULL);

        pLeftPanel->SetEnabled(FALSE);
        pRightPanel->SetEnabled(FALSE);

        CheckEnablePortaits(1);
        CheckEnableLeftPanel();

        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(m_pMainPanel->GetControl(6));

        UpdateCursorShape(0);
        g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);

        m_nToolTip = 0;
        m_cUIManager.InvalidateRect(NULL);

        if (m_pChatDisplay != NULL) {
            m_nChatMessageCount = g_pBaldurChitin->GetBaldurMessage()->m_cChatBuffer.UpdateTextDisplay(m_pChatDisplay,
                m_nChatMessageCount);
            m_pChatDisplay->ScrollToBottom();
        }

        CResRef cResRef = "WorldE";

        C2DArray tWorldE;
        tWorldE.Load(cResRef);
        if (tWorldE.Demand()) {
            tWorldE.Release();

            CString sResArea;

            CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

            pGame->GetVisibleArea()->m_resRef.CopyToString(sResArea);

            CWorldMap* pWorldMap = pGame->GetWorldMap(sResArea);
            DWORD nMap = pWorldMap->sub_55A3A0();

            for (INT y = 0; y < tWorldE.GetHeight(); y++) {
                CString sOtherResArea;
                sOtherResArea = tWorldE.GetAt(CPoint(1, y));
                if (sOtherResArea.GetLength() > 0) {
                    DWORD nMap = pWorldMap->sub_55A450(sOtherResArea);

                    pWorldMap->SetExplorable(nMap,
                        CResRef(sOtherResArea),
                        FALSE);

                    CString sVarName = tWorldE.GetAt(CPoint(0, y));

                    CVariable* pVar = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sVarName);
                    if (pVar != NULL) {
                        if (pVar->GetName() != "") {
                            if (pVar->m_intValue != 0) {
                                pWorldMap->EnableArea(nMap,
                                    CResRef(sOtherResArea),
                                    TRUE);

                                g_pBaldurChitin->GetBaldurMessage()->SendMapWorldRevealArea(sOtherResArea);
                            } else {
                                pWorldMap->EnableArea(nMap,
                                    CResRef(sOtherResArea),
                                    FALSE);
                            }
                        }
                    }
                }
            }

            pWorldMap->SetExplorable(nMap, CResRef(sResArea), TRUE);

            DWORD nArea;
            if (pWorldMap->GetAreaIndex(nMap, CResRef(sResArea), nArea)) {
                // FIXME: Unused.
                pWorldMap->GetArea(nMap, nArea);

                DWORD nMap = pWorldMap->sub_55A3A0();
                CWorldMapList* pLinks = pWorldMap->GetAllLinks(nMap, nArea);

                POSITION pos = pLinks->GetHeadPosition();
                while (pos != NULL) {
                    DWORD nLink = pLinks->GetNext(pos);
                    CWorldMapLinks* pLink = pWorldMap->GetLink(nMap, nLink);
                    CWorldMapArea* pArea = pWorldMap->GetArea(nMap, pLink->m_nArea);
                    if (pArea != NULL) {
                        pWorldMap->SetExplorable(nMap,
                            CResRef(pArea->m_resCurrentArea),
                            (pArea->m_dwFlags & 0x1) != 0);
                    }
                }

                delete pLinks;
            }
        }
    }
}

// 0x699E40
void CScreenWorldMap::EngineDeactivated()
{
    if (CChitin::byte_8FB950
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        if (!g_pBaldurChitin->m_pEngineWorld->m_bPaused) {
            g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0);
        }
    }

    m_preLoadFontRealms.Unload();
    m_preLoadFontTool.Unload();

    field_FE = 0;
    field_102 = 0;

    // NOTE: Uninline.
    m_cUIManager.KillCapture();

    m_bOverSelectedArea = FALSE;
    m_bMapLeftDown = FALSE;
    m_bMapDragging = FALSE;
    m_nHighlightArea = -1;
    m_nSelectedArea = -1;

    g_pBaldurChitin->GetObjectGame()->m_tempCursor = 4;
}

// 0x699F10
void CScreenWorldMap::EngineGameInit()
{
    m_cUIManager.fInit(this, CResRef("GUIWMAP"), g_pBaldurChitin->field_4A28);

    CPoint pt;
    if (g_pBaldurChitin->field_4A28) {
        pt.x = CVideo::SCREENWIDTH / 2 - CBaldurChitin::DEFAULT_SCREEN_WIDTH;
        pt.y = CVideo::SCREENHEIGHT / 2 - CBaldurChitin::DEFAULT_SCREEN_HEIGHT;
    } else {
        pt.x = (CVideo::SCREENWIDTH - CBaldurChitin::DEFAULT_SCREEN_WIDTH) / 2;
        pt.y = (CVideo::SCREENHEIGHT - CBaldurChitin::DEFAULT_SCREEN_HEIGHT) / 2;
    }

    m_cUIManager.ShiftPanels(pt);

    for (int side = 0; side < 4; side++) {
        m_cUIManager.AddPanel(&(g_pBaldurChitin->m_aBorderPanels[side]));
    }

    m_ptMapView.x = 0;
    m_ptMapView.y = 0;
    m_nSelectedCharacter = 0;
    m_pCurrentScrollBar = NULL;
    field_FCA = RGB(224, 180, 15);
    m_vfLabel.SetColor(field_FCA, field_FCA, FALSE);
    m_nHighlightArea = 0;
    m_nSelectedArea = -1;
    m_ptMapStartMousePos.x = 0;
    m_ptMapStartMousePos.y = 0;
    m_bOverSelectedArea = FALSE;
    m_bMapLeftDown = FALSE;
    m_bMapDragging = FALSE;
    m_pPath = NULL;
    m_pMainPanel = NULL;
    m_pChatDisplay = NULL;
    m_nChatMessageCount = 0;
    m_ptMapStartView.x = 0;
    m_ptMapStartView.y = 0;
}

// 0x5D72C0
void CScreenWorldMap::EngineGameUninit()
{
    m_cUIManager.fUninit();
}

// 0x49FC40
BOOL CScreenWorldMap::CheckMouseMove()
{
    return TRUE;
}

// 0x69A0A0
void CScreenWorldMap::OnKeyDown(SHORT nKeysFlags)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 688
    UTIL_ASSERT(pGame != NULL);

    for (SHORT nKeyFlag = 0; nKeyFlag < nKeysFlags; nKeyFlag++) {
        if (!m_cUIManager.OnKeyDown(m_pVirtualKeysFlags[nKeyFlag])) {
            switch (m_pVirtualKeysFlags[nKeyFlag]) {
            case VK_TAB:
                m_cUIManager.ForceToolTip();
                m_nToolTip = pGame->GetOptions()->m_nTooltips;
                break;
            case VK_RETURN:
                OnDoneButtonClick();
                break;
            case VK_ESCAPE:
                OnCancelButtonClick();
                break;
            case VK_UP:
                if (!m_bMapDragging) {
                    SetMapView(CPoint(m_ptMapView.x, m_ptMapView.y - 20));
                }
                break;
            case VK_DOWN:
                if (!m_bMapDragging) {
                    SetMapView(CPoint(m_ptMapView.x, m_ptMapView.y + 20));
                }
                break;
            case VK_SNAPSHOT:
                g_pBaldurChitin->GetCurrentVideoMode()->PrintScreen();
                break;
            default:
                for (SHORT index = 0; index < CINFGAME_KEYMAP_SIZE; index++) {
                    // NOTE: Uninline.
                    if (pGame->GetKeymap(index) == m_pVirtualKeysFlags[nKeyFlag]
                        && pGame->GetKeymapFlag(index) == m_bCtrlKeyDown) {
                        switch (index) {
                        case 24:
                            FocusChatEditBox();
                            break;
                        }
                        break;
                    }
                }
            }
        }
    }
}

// 0x69A400
void CScreenWorldMap::OnMouseMove(CPoint pt)
{
    m_cUIManager.OnMouseMove(pt);
    m_nToolTip = 0;
}

// 0x49FC40
BOOL CScreenWorldMap::CheckMouseLButton()
{
    return TRUE;
}

// 0x5D76C0
void CScreenWorldMap::OnLButtonDblClk(CPoint pt)
{
    m_cUIManager.OnLButtonDblClk(pt);
}

// 0x636970
void CScreenWorldMap::OnLButtonDown(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    m_cUIManager.OnLButtonDown(pt);
}

// 0x6369A0
void CScreenWorldMap::OnLButtonUp(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 0;
    m_cUIManager.OnLButtonUp(pt);
}

// 0x49FC40
BOOL CScreenWorldMap::CheckMouseRButton()
{
    return TRUE;
}

// 0x6369D0
void CScreenWorldMap::OnRButtonDown(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    m_cUIManager.OnRButtonDown(pt);
}

// 0x636A00
void CScreenWorldMap::OnRButtonUp(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 0;
    m_cUIManager.OnRButtonUp(pt);
}

// 0x69A430
void CScreenWorldMap::TimerAsynchronousUpdate()
{
    if (m_pChatDisplay != NULL) {
        m_nChatMessageCount = g_pBaldurChitin->GetBaldurMessage()->m_cChatBuffer.UpdateTextDisplay(m_pChatDisplay, m_nChatMessageCount);
    }

    if (m_nEngineState == 0) {
        g_pBaldurChitin->m_pEngineWorld->AsynchronousUpdate(FALSE);
    }

    OnMapAsyncUpdate();

    UpdateCursorShape(0);
    m_cUIManager.TimerAsynchronousUpdate();
    g_pBaldurChitin->GetObjectCursor()->CursorUpdate(pVidMode);
}

// 0x63BAA0
void CScreenWorldMap::TimerSynchronousUpdate()
{
    g_pBaldurChitin->GetObjectGame()->SynchronousUpdate();
    m_cUIManager.Render();
    pVidMode->Flip(TRUE);
}

// 0x69A4B0
void CScreenWorldMap::UpdateMainPanel()
{
    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(m_pMainPanel->GetControl(6));
}

// NOTE: Inlined.
void CScreenWorldMap::OnCancelButtonClick()
{
    switch (m_nEngineState) {
    case 0:
        StopWorldMap(FALSE);
        SelectEngine(g_pBaldurChitin->m_pEngineMap);
        break;
    case 1:
        if (m_bInControl && !m_bClickedArea) {
            StopWorldMap(FALSE);
            SelectEngine(g_pBaldurChitin->m_pEngineWorld);
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 1362
        UTIL_ASSERT(FALSE);
    }
}

// NOTE: Inlined.
void CScreenWorldMap::OnDoneButtonClick()
{
    switch (m_nEngineState) {
    case 0:
        StopWorldMap(FALSE);
        SelectEngine(g_pBaldurChitin->m_pEngineMap);
        break;
    case 1:
        if (m_bInControl && !m_bClickedArea) {
            StopWorldMap(FALSE);
            SelectEngine(g_pBaldurChitin->m_pEngineWorld);
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 1405
        UTIL_ASSERT(FALSE);
    }
}

// 0x69A4D0
void CScreenWorldMap::SetMapView(const CPoint& ptMapView)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 1438
    UTIL_ASSERT(pGame != NULL);

    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

    if (pGame->sub_5C79C0(sArea)) {
        CSize cViewSize = m_pMapControl->m_size;
        cViewSize.cx += 1;
        cViewSize.cy += 1;

        CSize cMapSize = pWorldMap->GetMapSize(pWorldMap->sub_55A3A0());

        CPoint ptNewView;
        ptNewView.x = max(min(ptMapView.x, cMapSize.cx - cViewSize.cx), 0);
        ptNewView.y = max(min(ptMapView.y, cMapSize.cy - cViewSize.cy), 0);

        if (ptNewView != m_ptMapView) {
            m_ptMapView = ptNewView;
            m_pMapControl->InvalidateRect();
            m_nToolTip = 0;
        }
    }
}

// 0x69A640
void CScreenWorldMap::OnMapMouseDown(const CPoint& ptMousePos)
{
    m_ptMapStartMousePos = ptMousePos;
    m_ptMapStartView = m_ptMapView;
    m_bMapLeftDown = TRUE;
    m_bMapDragging = FALSE;
    m_bOverSelectedArea = FALSE;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 1495
    UTIL_ASSERT(pGame != NULL);

    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

    if (m_nEngineState == 1
        && m_bInControl
        && !m_bClickedArea) {
        m_nSelectedArea = -1;

        if (m_pMapControl->IsOver(ptMousePos)) {
            m_nSelectedArea = FindAreaHit(ptMousePos - m_pMapControl->m_ptOrigin);
        }

        if (m_nSelectedArea != -1) {
            CWorldMapArea* pArea = pWorldMap->GetArea(pWorldMap->sub_55A3A0(), m_nSelectedArea);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 1513
            UTIL_ASSERT(pArea != NULL);

            if ((pArea->m_dwFlags & 0x1) != 0 && (pArea->m_dwFlags & 0x4) != 0) {
                m_bOverSelectedArea = TRUE;
                InvalidateArea(m_nSelectedArea);
            } else {
                m_nSelectedArea = -1;
                m_bOverSelectedArea = FALSE;
            }
        }
    }
}

// 0x69A840
void CScreenWorldMap::OnMapMouseUp(const CPoint& ptMousePos)
{
    if (m_nEngineState == 1
        && m_bInControl
        && !m_bClickedArea
        && m_nSelectedArea != -1) {
        DWORD nSelectedArea = -1;
        if (m_pMapControl->IsOver(ptMousePos)) {
            nSelectedArea = FindAreaHit(ptMousePos - m_pMapControl->m_ptOrigin);
        }

        if (nSelectedArea == m_nSelectedArea) {
            OnAreaMouseClick(m_nSelectedArea, ptMousePos);
        }

        InvalidateArea(m_nSelectedArea);

        m_nSelectedArea = -1;
        m_bOverSelectedArea = FALSE;
    }

    m_bMapLeftDown = FALSE;
    m_bMapDragging = FALSE;
}

// 0x69A920
void CScreenWorldMap::OnMapMouseMove(const CPoint& ptMousePos)
{
    if (m_bMapLeftDown) {
        if (m_nEngineState == 0
            || !m_bInControl
            || m_bClickedArea
            || m_nEngineState != 1
            || m_nSelectedArea == -1) {
            CPoint pt = ptMousePos - m_ptMapStartMousePos;
            if (pt.x < -5 || pt.x > 5 || pt.y <= -5 || pt.y >= 5) {
                m_bMapDragging = TRUE;
            }

            if (m_bMapDragging) {
                SetMapView(m_ptMapStartView - pt);
            }
        }
    }
}

// 0x69A9D0
void CScreenWorldMap::OnMapAsyncUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 1644
    UTIL_ASSERT(pGame != NULL);

    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

    CPoint pt(g_pChitin->m_ptPointer.x - m_pMapControl->m_pPanel->m_ptOrigin.x,
        g_pChitin->m_ptPointer.y - m_pMapControl->m_pPanel->m_ptOrigin.y);

    BOOL bInside = !(pt.x < m_pMapControl->m_ptOrigin.x
        || pt.y < m_pMapControl->m_ptOrigin.y
        || m_pMapControl->m_ptOrigin.x + m_pMapControl->m_size.cx < pt.x
        || m_pMapControl->m_ptOrigin.y + m_pMapControl->m_size.cy < pt.y);

    BOOL bHighlight = FALSE;

    if (bInside) {
        if (m_nEngineState == 1 && m_bInControl && !m_bClickedArea) {
            CPoint ptMap(pt.x - m_pMapControl->m_ptOrigin.x,
                pt.y - m_pMapControl->m_ptOrigin.y);

            DWORD nArea = FindAreaHit(ptMap);

            if (m_nSelectedArea != -1 && m_bOverSelectedArea != (m_nSelectedArea == nArea)) {
                m_bOverSelectedArea = m_nSelectedArea == nArea;
                InvalidateArea(m_nSelectedArea);
            }

            if (m_nHighlightArea != -1) {
                InvalidateArea(m_nHighlightArea);
            }

            BOOL bHighlighted = FALSE;
            if (nArea != -1) {
                CWorldMapArea* pArea = pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nArea);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
                // __LINE__: 1682
                UTIL_ASSERT(pArea != NULL);

                if ((pArea->m_dwFlags & 0x1) != 0 && (pArea->m_dwFlags & 0x4) != 0) {
                    bHighlight = TRUE;

                    if (nArea != m_nHighlightArea) {
                        InvalidateArea(nArea);
                    }

                    m_nHighlightArea = nArea;
                    bHighlighted = TRUE;
                }
            }

            if (!bHighlighted) {
                m_nHighlightArea = -1;
            }
        }

        if (m_nEngineState == 0
            || !m_bInControl
            || m_bClickedArea
            || m_nEngineState != 1
            || m_bMapDragging
            || (m_nSelectedArea == -1 && !bHighlight)) {
            pGame->m_tempCursor = 44;
            return;
        }

        // The travel time tool tip, in hours.
        m_nToolTip++;
        if (m_nToolTip >= static_cast<DWORD>(pGame->m_cOptions.m_nTooltips)) {
            CString sTime;
            sTime.Format("%d", GetTravelTime(m_nHighlightArea) / CTimerWorld::TIMESCALE_MSEC_PER_HOUR);

            STRREF strToolTip = 23084;
            g_pBaldurChitin->m_pObjectCursor->SetToolTip(strToolTip, NULL, sTime);
            return;
        }
    } else if (m_bMapDragging) {
        pGame->m_tempCursor = 44;
        return;
    }

    pGame->m_tempCursor = 0;
}

// 0x69ACF0
void CScreenWorldMap::OnAreaMouseClick(DWORD nArea, const CPoint& ptMousePos)
{
    CResRef aRandomAreas[5];
    CString sEntryPoint;
    CResRef cResArea;
    BOOL bRandomEncounter = FALSE;
    DWORD nSkipLink = -1;
    DWORD nTravelTime = 0;
    DWORD nEncounterLink;
    DWORD nEdge;
    DWORD nSrcArea;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 1787
    UTIL_ASSERT(pGame != NULL);

    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

    // The protagonist must be reachable.
    LONG nProtagonistId = pGame->GetProtagonist();

    CGameObject* pProtagonist;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nProtagonistId,
            CGameObjectArray::THREAD_ASYNCH,
            &pProtagonist,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    pGame->GetObjectArray()->ReleaseDeny(nProtagonistId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    if (m_nEngineState != 1 || !m_bInControl || m_bClickedArea) {
        return;
    }

    CWorldMapArea* pArea = pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nArea);
    pGame->field_1B86 = reinterpret_cast<char*>(pArea->m_strName);

    // NOTE: When the party is not on a link this stays the clicked area,
    // whose bytes are read as link flags below if the path is empty (as the
    // original).
    CWorldMapLinks* pLink = reinterpret_cast<CWorldMapLinks*>(pArea);

    if (m_nCurrentLink != -1) {
        // On the way: travel on from the link's far end, or turn back.
        nSrcArea = pWorldMap->FindSourceAreaIndex(pWorldMap->sub_55A3A0(), m_nCurrentLink);

        pLink = pWorldMap->GetLink(pWorldMap->sub_55A3A0(), m_nCurrentLink);
        if (nSrcArea == nArea) {
            nSrcArea = pLink->m_nArea;
            nSkipLink = pWorldMap->GetLinkIndex(pWorldMap->sub_55A3A0(), pLink->m_nArea, nArea);
        }

        nTravelTime = (pLink->m_nDistanceScale * CRuleTables::TIME_TRAVELLING) >> 1;
    } else {
        BOOL bResult = pWorldMap->GetAreaIndex(pWorldMap->sub_55A3A0(), m_cResCurrentArea, nSrcArea);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 1842
        UTIL_ASSERT(bResult);
    }

    pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nSrcArea);

    if ((pArea->m_dwFlags & 0x1) == 0 || (pArea->m_dwFlags & 0x4) == 0) {
        return;
    }

    if (nSrcArea != nArea) {
        m_pPath = pWorldMap->FindPath(pWorldMap->sub_55A3A0(), nSrcArea, nArea);
        if (m_pPath == NULL) {
            return;
        }

        while (m_pPath->GetCount() > 0 && !bRandomEncounter) {
            nEncounterLink = m_pPath->GetHead();
            pLink = pWorldMap->GetLink(pWorldMap->sub_55A3A0(), nEncounterLink);

            if (m_nCurrentLink != nEncounterLink && nSkipLink != nEncounterLink) {
                // The encounter areas not loaded yet.
                DWORD nRandomAreas = 0;
                const RESREF* pRandomAreas[5] = {
                    &(pLink->m_resRandomEncounterArea0),
                    &(pLink->m_resRandomEncounterArea1),
                    &(pLink->m_resRandomEncounterArea2),
                    &(pLink->m_resRandomEncounterArea3),
                    &(pLink->m_resRandomEncounterArea4),
                };

                for (INT nIndex = 0; nIndex < 5; nIndex++) {
                    CString sRandomArea;
                    cResArea = *pRandomAreas[nIndex];
                    cResArea.CopyToString(sRandomArea);
                    if (cResArea != "") {
                        if (pGame->GetArea(sRandomArea) == NULL) {
                            aRandomAreas[nRandomAreas] = cResArea;
                            nRandomAreas++;
                        }
                    }
                }

                if (nRandomAreas != 0
                    && pLink->m_nEncounterProbability > static_cast<DWORD>(rand() % 101)) {
                    cResArea = aRandomAreas[static_cast<DWORD>(rand()) % nRandomAreas];
                    nTravelTime += (pLink->m_nDistanceScale * CRuleTables::TIME_TRAVELLING) >> 1;
                    nEdge = 10;
                    sEntryPoint = "";
                    bRandomEncounter = TRUE;
                } else {
                    nTravelTime += pLink->m_nDistanceScale * CRuleTables::TIME_TRAVELLING;
                }
            }

            m_pPath->RemoveHead();
        }

        if (!bRandomEncounter) {
            // The edge of the destination the party comes in from.
            if ((pLink->m_dwLinkFlags & 0x1) != 0) {
                nEdge = 0;
            } else if ((pLink->m_dwLinkFlags & 0x4) != 0) {
                nEdge = 8;
            } else if ((pLink->m_dwLinkFlags & 0x8) != 0) {
                nEdge = 12;
            } else if ((pLink->m_dwLinkFlags & 0x2) != 0) {
                nEdge = 4;
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
                // __LINE__: 1963
                UTIL_ASSERT(FALSE);
                nEdge = 10;
            }

            sEntryPoint = reinterpret_cast<char*>(pLink->m_entryPoint);
            cResArea = pArea->m_resCurrentArea;
            nEncounterLink = -1;
        }

        if (m_pPath != NULL) {
            delete m_pPath;
        }
        m_pPath = NULL;

        g_pBaldurChitin->m_pEngineWorld->m_weather.OnAreaChange(TRUE);

        g_pBaldurChitin->cSoundMixer.m_nActiveArea = -1;
        g_pBaldurChitin->cSoundMixer.UpdateSoundList();

        // Auto-save before leaving (not from an area flagged so).
        if ((pGame->GetVisibleArea()->m_header.m_flags & 0x1) == 0
            && (!g_pChitin->cNetwork.GetSessionOpen()
                || g_pChitin->cNetwork.GetSessionHosting() == TRUE)) {
            pGame->m_sSaveGame = CInfGame::AUTO_SAVE_NAME;
            CScreenCharacter::SAVE_NAME = CInfGame::AUTO_SAVE_NAME;

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
                pGame->field_366E = 1;
                pGame->field_50DC = 0;

                if (!g_pChitin->cVideo.Is3dAccelerated()) {
                    if (!pGame->field_50D8) {
                        pGame->SynchronousUpdate();
                    }
                    pGame->field_50D8 = TRUE;
                }
            }

            pGame->SaveGame(1, 0, 1);

            // NOTE: Never true together with the first block's check; the
            // inner branch cannot run (as the original).
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                && g_pChitin->cVideo.Is3dAccelerated()) {
                pGame->field_366E = 1;
                pGame->field_50DC = 1;
            }
        }

        pGame->GetGameSave()->m_cResCurrentWorldArea = cResArea;
        pGame->GetGameSave()->m_nCurrentWorldLink = nEncounterLink;

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && !g_pChitin->cNetwork.GetSessionHosting()) {
            // NOTE: A client asks the host to travel (0x69B545-0x69B5C4,
            // CBaldurMessage 0x4356E0); not ported, iwd2r has no multiplayer.
            m_bClickedArea = TRUE;
            return;
        }

        MoveToArea(sEntryPoint, cResArea, nEdge, bRandomEncounter, nTravelTime);

        StopWorldMap(nSrcArea != nArea);
    } else {
        StopWorldMap(FALSE);
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 2085
    UTIL_ASSERT(pWorld != NULL);

    SelectEngine(pWorld);

    if (bRandomEncounter) {
        STR_RES strRes;
        g_pBaldurChitin->GetTlkTable().Fetch(16493, strRes);

        pWorld->DisplayText(CString(""), strRes.szText, -1, FALSE);

        strRes.cSound.SetFireForget(TRUE);
        strRes.cSound.SetChannel(5, 0);
        strRes.cSound.Play(FALSE);
    }
}

// 0x69B7F0
DWORD CScreenWorldMap::GetTravelTime(DWORD nArea)
{
    DWORD nTime = 0;
    DWORD nSkipLink = -1;
    DWORD nSrcArea;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 2133
    UTIL_ASSERT(pGame != NULL);

    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

    pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nArea);

    if (m_nCurrentLink != -1) {
        // On the way: count from the link's far end, or turn back.
        nSrcArea = pWorldMap->FindSourceAreaIndex(pWorldMap->sub_55A3A0(), m_nCurrentLink);

        CWorldMapLinks* pLink = pWorldMap->GetLink(pWorldMap->sub_55A3A0(), m_nCurrentLink);
        if (nSrcArea == nArea) {
            nSrcArea = pLink->m_nArea;
            nSkipLink = pWorldMap->GetLinkIndex(pWorldMap->sub_55A3A0(), pLink->m_nArea, nArea);
        }

        nTime = (pLink->m_nDistanceScale * CRuleTables::TIME_TRAVELLING) >> 1;
    } else {
        BOOL bResult = pWorldMap->GetAreaIndex(pWorldMap->sub_55A3A0(), m_cResCurrentArea, nSrcArea);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2164
        UTIL_ASSERT(bResult);
    }

    pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nSrcArea);

    if (nSrcArea != nArea) {
        m_pPath = pWorldMap->FindPath(pWorldMap->sub_55A3A0(), nSrcArea, nArea);
        if (m_pPath != NULL) {
            while (m_pPath->GetCount() > 0) {
                DWORD nLink = m_pPath->GetHead();

                CWorldMapLinks* pLink = pWorldMap->GetLink(pWorldMap->sub_55A3A0(), nLink);
                if (m_nCurrentLink != nLink && nSkipLink != nLink) {
                    nTime += pLink->m_nDistanceScale * CRuleTables::TIME_TRAVELLING;
                }

                m_pPath->RemoveHead();
            }

            delete m_pPath;
            m_pPath = NULL;
        }
    }

    return nTime;
}

// NOTE: Inlined twice in 0x69D0F0, for the party and for the familiars.
//
// Moves one party member (already denied) into `pArea` next to `ptEntry`,
// keeping the formation and line of sight to the previous one.
static void MoveSpriteToArea(CGameArea* pArea,
    CGameSprite* pSprite,
    LONG nSpriteId,
    const CPoint& ptEntry,
    const CPoint* pOffsets,
    SHORT nOffsetRead,
    SHORT& nOffsetIndex,
    CPoint& ptPrev,
    BOOLEAN& bMarshalled,
    SHORT nFacing)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    pSprite->ClearActions(FALSE);

    CGameArea* pOldArea = pSprite->m_pArea;

    // The area left behind is saved to the temp folder once.
    CAreaFile cAreaFile;
    if (pOldArea != NULL
        && (!g_pChitin->cNetwork.GetSessionOpen()
            || g_pChitin->cNetwork.GetSessionHosting() == TRUE)
        && !bMarshalled
        && (pOldArea->m_header.m_flags & 0x1) == 0
        && !pOldArea->field_436) {
        BYTE* pData;
        DWORD nSize;
        pOldArea->Marshal(&pData, &nSize, TRUE);
        if (pData != NULL) {
            cAreaFile.SetResRef(pOldArea->m_resRef, TRUE, TRUE);

            // NOTE: The original writes through a NULL resource when the
            // area file is missing; bounded here (P4-10).
            if (cAreaFile.pRes != NULL) {
                static_cast<CRes*>(cAreaFile.pRes)->Write(pGame->m_sTempDir, pData, nSize);
            }

            pOldArea->field_436 = 1;
            pOldArea->field_438 = pGame->m_worldTime.m_gameTime;

            delete pData;

            bMarshalled = TRUE;
        }
    }

    pGame->m_bInAreaTransition = TRUE;

    if (pSprite->m_pArea != NULL) {
        pSprite->RemoveFromArea();

        // NOTE: A connected host indexes the formation by the member's own
        // slot, not by the running count (0x69DAE5-0x69DB07 for the party,
        // 0x69E336-0x69E35A for the familiars); the running count steps
        // either way, and only a session that is not connected reads it.
        CPoint ptOffset(pOffsets[nOffsetRead].x / CAIGroup::OFFSET_MULTIPLIER,
            pOffsets[nOffsetRead].y / CAIGroup::OFFSET_MULTIPLIER);
        nOffsetIndex++;

        BYTE nPersonalSpace = pSprite->m_animation.GetPersonalSpace();

        CPoint ptOpen = pArea->m_search.GetNearestOpenSquare(CPoint(ptOffset.x + ptEntry.x, ptOffset.y + ptEntry.y),
            pSprite->GetTerrainTable(),
            nPersonalSpace,
            -1);

        if (ptPrev.x != -1) {
            // Within sight of the one placed before, else next to them, else
            // at the entry point.
            CPoint ptPrevCenter(ptPrev.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                ptPrev.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);
            CPoint ptOpenCenter(ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

            if (!pArea->CheckLOS(ptPrevCenter, ptOpenCenter, pSprite->GetVisibleTerrainTable(), TRUE)) {
                ptOpen = pArea->m_search.GetNearestOpenSquare(ptPrev,
                    pSprite->GetTerrainTable(),
                    pSprite->m_animation.GetPersonalSpace(),
                    -1);

                CPoint ptNextCenter(ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                    ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

                if (!pArea->CheckLOS(ptPrevCenter, ptNextCenter, pSprite->GetVisibleTerrainTable(), TRUE)) {
                    ptOpen = pArea->m_search.GetNearestOpenSquare(ptEntry,
                        pSprite->GetTerrainTable(),
                        pSprite->m_animation.GetPersonalSpace(),
                        -1);
                }
            }
        }

        ptPrev = ptOpen;

        CPoint pos(ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
            ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

        if (!pSprite->Animate() && pSprite->m_animation.CanLieDown()) {
            // The dead lie in the back.
            pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_BACK);
        } else {
            pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_FRONT);
            pSprite->SetFacing(nFacing);

            if (pGame->GetVisibleArea() != pArea) {
                CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

                // NOTE: The original compares the address of this copy with
                // the old area's view port, so it always resizes.
                CRect rView = pWorld->GetNewViewSize();
                pWorld->SetNewViewSize(rView, FALSE);

                pArea->m_cInfinity.SetViewPosition(pos.x - rView.Width() / 2,
                    pos.y - rView.Height() / 2,
                    TRUE);

                pGame->UnselectAll();
            } else if (pOldArea == pArea) {
                CRect rView = pArea->m_cInfinity.rViewPort;
                pArea->m_cInfinity.SetViewPosition(pos.x - rView.Width() / 2,
                    pos.y - rView.Height() / 2,
                    TRUE);
            }

            pGame->SelectCharacter(pSprite->m_id, FALSE);
        }
    }

    pGame->m_bInAreaTransition = FALSE;

    CPoint& ptPos = pSprite->GetPos();
    if (ptPos.x != -1 || ptPos.y != -1 || pSprite->m_pArea != NULL) {
        CMessage* message = new CMessageSpriteUpdate(pSprite, nSpriteId, nSpriteId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }
}

// 0x69D0F0
void CScreenWorldMap::MoveToArea(CString sEntryPoint, const CResRef& cResArea, DWORD nEdge, BOOL bRandomEncounter, DWORD nTravelTime)
{
    // NOTE: The multiplayer parts (telling the clients 0x69D18A-0x69D1F2,
    // waiting for them 0x69D2CB-0x69D416 and 0x69EADF-0x69F4F6, the
    // client's side 0x69F509-0x69F834, the host's control and action hand
    // out 0x69F9FC-0x69FF38) are not ported: iwd2r has no multiplayer.

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 2933
    UTIL_ASSERT(pGame != NULL && pMessage != NULL);

    CString sArea;
    cResArea.CopyToString(sArea);

    DWORD nProgressTarget = CInfGame::PROGRESSBAR_AREA_ADDITIONAL + CInfGame::PROGRESSBAR_CACHING_ADDITIONAL;
    BOOLEAN bMarshalled = FALSE;

    pGame->sub_59FA00(TRUE);

    g_pBaldurChitin->GetObjectGame()->m_nTravelScreenImageToUse = static_cast<BYTE>(rand() % 5);

    g_pChitin->SetProgressBar(TRUE, 9891, 0, 0, TRUE, 9885, FALSE, 0, FALSE, FALSE, 255);
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    g_pChitin->cProgressBar.m_nActionTarget = nProgressTarget;
    g_pChitin->cProgressBar.m_bDisableMinibars = FALSE;

    CGameArea* pArea = pGame->LoadArea(sArea, 255, FALSE, TRUE);

    // The first character this machine controls.
    BOOLEAN bFound = FALSE;
    BYTE rc = CGameObjectArray::SUCCESS;
    CGameSprite* pFoundSprite = NULL;
    for (INT nTries = 10; nTries > 0 && !bFound; nTries--) {
        for (SHORT nCharacter = 0; nCharacter < 6 && !bFound; nCharacter++) {
            field_FF6 = nCharacter < 6
                ? pGame->m_characters[nCharacter]
                : CGameObjectArray::INVALID_INDEX;
            if (field_FF6 != CGameObjectArray::INVALID_INDEX) {
                do {
                    rc = pGame->GetObjectArray()->GetDeny(field_FF6,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pFoundSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED);

                // NOTE: The sprite is read even when the deny failed (as the
                // original).
                if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                    || (pFoundSprite != NULL
                        && g_pChitin->cNetwork.m_idLocalPlayer == pFoundSprite->m_remotePlayerID)) {
                    bFound = TRUE;
                } else {
                    rc = pGame->GetObjectArray()->ReleaseDeny(field_FF6,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }
    }

    if (bFound == TRUE && rc != CGameObjectArray::SUCCESS) {
        if (g_pChitin->cNetwork.GetSessionOpen()) {
            g_pChitin->cNetwork.CloseSession(TRUE);
        }
        g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
        return;
    }

    if (pArea == NULL) {
        if (g_pChitin->cNetwork.GetSessionOpen()) {
            g_pChitin->cNetwork.CloseSession(TRUE);
        }
        g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);

        if (bFound == TRUE) {
            pGame->GetObjectArray()->ReleaseDeny(field_FF6,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
        return;
    }

    // Where the party comes in.
    SHORT nFacing = static_cast<SHORT>(nEdge);

    // NOTE: The original leaves the point uninitialized on a bad edge;
    // bounded here (P4-10).
    CPoint ptEntry(0, 0);
    if (bRandomEncounter) {
        ptEntry.x = pArea->m_cInfinity.nAreaX >> 1;
        ptEntry.y = pArea->m_cInfinity.nAreaY >> 1;
    } else if (strcmp(sEntryPoint, "") != 0) {
        pArea->GetEntryPoint(sEntryPoint, ptEntry, nFacing);
    } else {
        switch (nFacing) {
        case 0:
            ptEntry.x = pArea->m_cInfinity.nAreaX >> 1;
            ptEntry.y = CPathSearch::GRID_SQUARE_SIZEY * CSearchBitmap::TRAVEL_WIDTH * 5;
            break;
        case 4:
            ptEntry.x = pArea->m_cInfinity.nAreaX - CPathSearch::GRID_SQUARE_SIZEX * CSearchBitmap::TRAVEL_WIDTH * 5 - 1;
            ptEntry.y = pArea->m_cInfinity.nAreaY >> 1;
            break;
        case 8:
            ptEntry.x = pArea->m_cInfinity.nAreaX >> 1;
            ptEntry.y = pArea->m_cInfinity.nAreaY - CPathSearch::GRID_SQUARE_SIZEY * CSearchBitmap::TRAVEL_WIDTH * 5 - 1;
            break;
        case 12:
            ptEntry.x = CPathSearch::GRID_SQUARE_SIZEX * CSearchBitmap::TRAVEL_WIDTH * 5;
            ptEntry.y = pArea->m_cInfinity.nAreaY >> 1;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 3186
            UTIL_ASSERT(FALSE);
            break;
        }
    }

    ptEntry.x /= CPathSearch::GRID_SQUARE_SIZEX;
    ptEntry.y /= CPathSearch::GRID_SQUARE_SIZEY;

    if (bFound == TRUE) {
        CPoint ptPrev(-1, -1);

        // NOTE: A single-player game is a connected host session (the null
        // provider), so the connected placing runs there: the party comes in
        // the fixed order of m_characters[0..5], one slot per character, an
        // empty slot skipped without consuming a place, the formation offset
        // indexed by the slot itself, and the familiars from offset 6
        // (0x69D7E8-0x69D87D, 0x69DAE5-0x69DB07, 0x69E336-0x69E35A). Only a
        // session that is not connected places the party in portrait order
        // with a running offset.
        BOOLEAN bConnected = g_pChitin->cNetwork.GetSessionOpen();

        SHORT nFormation = bConnected
            ? CAIGroup::FORMATION_2BY3
            : pGame->GetGameSave()->m_curFormation;

        SHORT nDirection = static_cast<SHORT>((nFacing + 8) * 360 / 16 % 360);

        CPoint* pOffsets = pGame->m_group.GetOffsets(nFormation, nDirection, TRUE);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 3206
        UTIL_ASSERT(pOffsets != NULL);

        SHORT nOffsetIndex = 0;

        SHORT nPartySlots = bConnected ? 6 : pGame->m_nCharacters;

        // The familiars' first offset: the party's slot count (0x69E336 reads
        // the count the party loop left, 6 when connected).
        SHORT nFamiliarOffset = nPartySlots;

        // NOTE: GetOffsets(TRUE) returns max(6, the group's member count)
        // places, so a familiar's offset 6 is inside the array exactly when
        // the group counts its familiars; the original relies on that, and
        // keeps reading past the end otherwise.
        for (SHORT nCharacter = 0; nCharacter < nPartySlots; nCharacter++) {
            // NOTE: Uninline. The fixed order when connected, the portrait
            // order otherwise.
            LONG nCharacterId = bConnected
                ? pGame->GetFixedOrderCharacterId(nCharacter)
                : pGame->GetCharacterId(nCharacter);

            if (nCharacterId == CGameObjectArray::INVALID_INDEX) {
                continue;
            }

            CGameSprite* pSprite;
            do {
                rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED);

            if (rc != CGameObjectArray::SUCCESS) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
                // __LINE__: 3236
                UTIL_ASSERT(FALSE);

                pGame->SelectToolbar();
                g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
                delete pOffsets;
                pGame->GetObjectArray()->ReleaseDeny(field_FF6,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return;
            }

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                MoveSpriteToArea(pArea,
                    pSprite,
                    nCharacterId,
                    ptEntry,
                    pOffsets,
                    bConnected ? nCharacter : nOffsetIndex,
                    nOffsetIndex,
                    ptPrev,
                    bMarshalled,
                    nFacing);
            }

            pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        POSITION pos = pGame->m_familiars.GetHeadPosition();
        while (pos != NULL) {
            LONG nFamiliarId = static_cast<LONG>(reinterpret_cast<INT_PTR>(pGame->m_familiars.GetNext(pos)));

            CGameSprite* pSprite;
            do {
                rc = pGame->GetObjectArray()->GetDeny(nFamiliarId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED);

            if (rc != CGameObjectArray::SUCCESS) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
                // __LINE__: 3372
                UTIL_ASSERT(FALSE);

                pGame->SelectToolbar();
                g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
                delete pOffsets;
                pGame->GetObjectArray()->ReleaseDeny(field_FF6,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return;
            }

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                MoveSpriteToArea(pArea,
                    pSprite,
                    nFamiliarId,
                    ptEntry,
                    pOffsets,
                    bConnected ? nFamiliarOffset : nOffsetIndex,
                    nOffsetIndex,
                    ptPrev,
                    bMarshalled,
                    nFacing);
            }

            // The familiars' offsets step per familiar, placed or not
            // (0x69E8AE).
            nFamiliarOffset++;

            pGame->GetObjectArray()->ReleaseDeny(nFamiliarId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        delete pOffsets;

        pGame->SelectToolbar();

        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pFoundSprite->m_remotePlayerID) {
            CPoint& ptPos = pFoundSprite->GetPos();
            if (ptPos.x != -1 || ptPos.y != -1 || pFoundSprite->m_pArea != NULL) {
                CMessage* message = new CMessageSpriteUpdate(pFoundSprite, field_FF6, field_FF6);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        pGame->GetObjectArray()->ReleaseDeny(field_FF6,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget - 1;
    g_pChitin->m_bDisplayStale = TRUE;

    pGame->sub_59FA00(TRUE);

    g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;

    g_pChitin->cProgressBar.m_bTimeoutVisible = FALSE;
    g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;
    g_pChitin->m_bDisplayStale = TRUE;

    pGame->sub_59FA00(TRUE);

    g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);

    if (nTravelTime > 0) {
        // Travel tires the party, up to a day's worth.
        for (SHORT nCharacter = 0; nCharacter < pGame->m_nCharacters; nCharacter++) {
            // NOTE: Uninline.
            LONG nCharacterId = pGame->GetCharacterId(nCharacter);

            CGameSprite* pSprite;
            do {
                rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                return;
            }

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                LONG nFatigue = pGame->m_worldTime.m_gameTime - pSprite->field_722A - FATIGUE_TIME_CAP + nTravelTime;
                if (nFatigue < 0) {
                    nFatigue = 0;
                } else if (nFatigue >= static_cast<LONG>(nTravelTime)) {
                    nFatigue = nTravelTime;
                }
                pSprite->field_722A += nFatigue;
            }

            pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        if (!g_pChitin->cNetwork.GetSessionOpen()
            || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            pGame->m_worldTime.AddCurrentTime(nTravelTime);
            pGame->FeedBack(CInfGame::FEEDBACK_TIMEPASSEDJOURNEY, nTravelTime, TRUE);
        }
    }
}

// 0x69BA70
BOOL CScreenWorldMap::DrawMap(const CRect& r)
{
    BOOL bResult;

    CSingleLock renderLock(&field_1022, FALSE);
    renderLock.Lock(INFINITE);

    if (m_aAreaRect.GetCount() != 0) {
        CString sLabel;

        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2244
        UTIL_ASSERT(pGame != NULL);

        CString sArea;
        pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

        CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

        CVidInf* pVidInf = static_cast<CVidInf*>(g_pBaldurChitin->GetCurrentVideoMode());

        CRect rControl(m_pMapControl->m_pPanel->m_ptOrigin + m_pMapControl->m_ptOrigin,
            m_pMapControl->m_size);

        CRect rMap1(r);
        rMap1.OffsetRect(rControl.TopLeft());

        CRect rClip;
        rClip.IntersectRect(rControl, rMap1);

        CSize mapSize = pWorldMap->GetMapSize(pWorldMap->sub_55A3A0());

        if (mapSize.cx >= m_pMapControl->m_size.cx) {
            mapSize.cx = m_pMapControl->m_size.cx;
        }

        if (mapSize.cy >= m_pMapControl->m_size.cy) {
            mapSize.cy = m_pMapControl->m_size.cy;
        }

        CRect rMap2(r);
        rMap2.OffsetRect(m_ptMapView);

        CRect rMosaic;
        rMosaic.IntersectRect(CRect(m_ptMapView, mapSize), rMap2);

        BOOL bResult = m_vmMap.Render(CVIDINF_SURFACE_BACK,
            rControl.left - m_ptMapView.x,
            rControl.top - m_ptMapView.y,
            rMosaic,
            rClip,
            0,
            FALSE);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2266
        UTIL_ASSERT(bResult);

        if (pVidInf->BKLock(rClip)) {
            int v1 = rControl.left - rClip.left;
            int v2 = rControl.top - rClip.top;
            rClip.OffsetRect(-rClip.left, -rClip.top);

            m_vcAreas.pRes->Demand();
            m_vfLabel.pRes->Demand();

            if (field_FCA != RGB(250, 250, 250)) {
                field_FCA = RGB(250, 250, 250);
                m_vfLabel.SetColor(field_FCA, RGB(0, 0, 0), FALSE);
            }

            for (DWORD nArea = 0; nArea < pWorldMap->GetNumAreas(pWorldMap->sub_55A3A0()); nArea++) {
                CWorldMapArea* pArea = pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nArea);
                if ((pArea->m_dwFlags & 0x1) != 0) {
                    CRect rArea = m_aAreaRect[nArea];

                    CRect rAreaDirty;
                    rAreaDirty.IntersectRect(rMosaic, rArea);

                    if (!rAreaDirty.IsRectEmpty()) {
                        COLORREF rgbColor;

                        if (pArea->m_sequence == 23) {
                            rgbColor = RGB(170, 170, 170);
                        } else if ((pArea->m_dwFlags & 0x8) != 0) {
                            rgbColor = RGB(0, 0, 0);
                        } else if ((pArea->m_dwFlags & 0x4) != 0) {
                            rgbColor = RGB(255, 220, 90);
                        } else {
                            rgbColor = RGB(224, 180, 15);
                        }

                        if (!m_bMapDragging
                            && m_nEngineState == 1
                            && m_bInControl
                            && !m_bClickedArea) {
                            if (nArea == m_nSelectedArea && m_bOverSelectedArea) {
                                rgbColor = RGB(255, 255, 255);
                            } else if (nArea == m_nHighlightArea && m_nSelectedArea == -1) {
                                rgbColor = sub_69CB40(rgbColor);
                            }
                        }

                        m_vcAreas.SequenceSet(static_cast<SHORT>(pArea->m_sequence));
                        m_vcAreas.RealizePalette(0x1);

                        bResult = pVidInf->BKRender(&m_vcAreas,
                            v1 - m_ptMapView.x + pWorldMap->GetAreaPosition(pArea).cx,
                            v2 - m_ptMapView.y + pWorldMap->GetAreaPosition(pArea).cy,
                            rClip,
                            TRUE,
                            0);

                        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
                        // __LINE__: 2358
                        UTIL_ASSERT(bResult);

                        if (field_FCA != rgbColor) {
                            field_FCA = rgbColor;
                            m_vfLabel.SetColor(field_FCA, RGB(0, 0, 0), FALSE);
                        }

                        if (pArea->m_strLabel != -1) {
                            sLabel = FetchString(pArea->m_strLabel);

                            bResult = pVidInf->BKTextOut(&m_vfLabel,
                                sLabel,
                                v1 + rArea.left + (rArea.Width() - m_vfLabel.GetStringLength(sLabel, FALSE)) / 2 - m_ptMapView.x,
                                v2 + rArea.bottom - m_vfLabel.GetFontHeight(FALSE) / (g_pBaldurChitin->field_4A28 ? 2 : 1) - m_ptMapView.y,
                                rClip,
                                0,
                                TRUE);

                            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
                            // __LINE__: 2372
                            UTIL_ASSERT(bResult);
                        }
                    }
                }
            }

            m_vcAreas.pRes->Release();
            m_vfLabel.pRes->Release();

            CPoint ptMarker;
            GetMarkerPosition(ptMarker);

            m_vcMarker.pRes->Demand();
            m_vcMarker.RealizePalette(0x1);
            bResult = pVidInf->BKRender(&m_vcMarker,
                v1 + ptMarker.x - m_ptMapView.x,
                v2 + ptMarker.y - m_ptMapView.y,
                rClip,
                FALSE, // FIXME: Should be `TRUE`, res is demanded.
                0);
            m_vcMarker.pRes->Release();

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 2390
            UTIL_ASSERT(bResult);

            pVidInf->BKUnlock();
        }

        bResult = TRUE;
    } else {
        bResult = FALSE;
    }

    renderLock.Unlock();

    return bResult;
}

// 0x69C2A0
void CScreenWorldMap::StartWorldMap(INT nEngineState, LONG nLeavingEdge, BOOLEAN bInControl)
{
    m_nEngineState = nEngineState;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 2478
    UTIL_ASSERT(pGame != NULL);

    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);

    // NOTE: Unused.
    CResRef v1;

    if (g_pBaldurChitin->cNetwork.GetSessionOpen()
        && g_pBaldurChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        m_pMainPanel = m_cUIManager.GetPanel(3);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2491
        UTIL_ASSERT(m_pMainPanel != NULL);

        m_pMapControl = static_cast<CUIControlButtonWorldMapWorldMap*>(m_pMainPanel->GetControl(4));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2493
        UTIL_ASSERT(m_pMapControl != NULL);

        m_pChatDisplay = static_cast<CUIControlTextDisplay*>(m_pMainPanel->GetControl(5));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2495
        UTIL_ASSERT(m_pChatDisplay != NULL);

        m_cUIManager.GetPanel(2)->SetActive(FALSE);
    } else {
        m_pMainPanel = m_cUIManager.GetPanel(2);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2502
        UTIL_ASSERT(m_pMainPanel != NULL);

        m_pMapControl = static_cast<CUIControlButtonWorldMapWorldMap*>(m_pMainPanel->GetControl(4));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2504
        UTIL_ASSERT(m_pMapControl != NULL);

        m_pChatDisplay = NULL;

        m_cUIManager.GetPanel(3)->SetActive(FALSE);
    }

    m_pMainPanel->SetActive(TRUE);

    m_cUIManager.GetPanel(0)->GetControl(10)->SetActive(TRUE);

    m_vmMap.SetResRef(CResRef(pWorldMap->GetMap(pWorldMap->sub_55A3A0())->m_resMosaic), FALSE, TRUE);
    m_vmMap.m_bDoubleSize = g_pBaldurChitin->field_4A28;

    // NOTE: Uninline.
    m_vcAreas.SetResRef(CResRef(pWorldMap->GetMap(pWorldMap->sub_55A3A0())->m_resAreaIcons),
        g_pBaldurChitin->field_4A28,
        FALSE,
        TRUE);

    // NOTE: Uninline.
    m_vcMarker.SetResRef(CResRef("WMDAG"), g_pBaldurChitin->field_4A28, TRUE, TRUE);

    m_vfLabel.SetResRef(CResRef("INFOFONT"), g_pBaldurChitin->field_4A28, TRUE);

    // NOTE: Uninline.
    m_aAreaRect.SetSize(pWorldMap->GetNumAreas(pWorldMap->sub_55A3A0()));

    for (DWORD nArea = 0; nArea < pWorldMap->GetNumAreas(pWorldMap->sub_55A3A0()); nArea++) {
        m_aAreaRect[nArea] = GetAreaRect(pWorldMap->sub_55A3A0(), nArea);
    }

    m_nSelectedArea = -1;
    m_nHighlightArea = -1;
    m_nLeavingEdge = nLeavingEdge;
    m_nCurrentLink = pGame->GetGameSave()->m_nCurrentWorldLink;

    for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
        LONG nCharacterId = pGame->GetCharacterId(nPortrait);

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS || pSprite == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 2566
            UTIL_ASSERT(FALSE);
        }

        if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) == 0
            && (pSprite->GetBaseStats()->m_generalState & (STATE_STONE_DEATH | STATE_FROZEN_DEATH)) == 0
            && (pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) == 0) {
            m_cResCurrentArea = pSprite->m_currentArea;

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            break;
        }

        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    m_bInControl = bInControl;
    m_bClickedArea = FALSE;

    if (m_nEngineState == 1 && m_nCurrentLink == 1) {
        switch (m_nLeavingEdge) {
        case 0x0:
        case 0x4:
        case 0x8:
        case 0xC:
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 2594
            UTIL_ASSERT(FALSE);
        }
    }

    CPoint pt;
    GetMarkerPosition(pt);
    pt.x -= (m_pMapControl->m_size.cx + 1) / 2;
    pt.y -= (m_pMapControl->m_size.cy + 1) / 2;
    SetMapView(pt);
}

// 0x69C9E0
void CScreenWorldMap::StopWorldMap(BOOLEAN bAreaClicked)
{
    if (m_bInControl == TRUE && !bAreaClicked) {
        if (!g_pChitin->cNetwork.GetSessionOpen()
            || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            g_pBaldurChitin->GetBaldurMessage()->SendMapWorldAnnounceStatus(FALSE, 0, 0);
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && !g_pChitin->cNetwork.GetSessionHosting()) {
            g_pBaldurChitin->GetBaldurMessage()->SendMapWorldCancelRequestToServer();
        }
    }

    if (m_nEngineState == 1) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 2646
        UTIL_ASSERT(pGame != NULL);

        pGame->SelectAll(FALSE);
        pGame->GetGroup()->ClearActions();
    }

    CSingleLock lock(&field_1022, FALSE);
    lock.Lock(INFINITE);

    // NOTE: Uninline.
    m_aAreaRect.SetSize(0);

    lock.Unlock();

    m_cUIManager.GetPanel(2)->SetActive(FALSE);
    m_cUIManager.GetPanel(3)->SetActive(FALSE);
}

// 0x69CB40
COLORREF CScreenWorldMap::sub_69CB40(COLORREF rgb)
{
    // NOTE: An arithmetic shift, as the original.
    WORD v1 = static_cast<WORD>((g_pBaldurChitin->nAUCounter >> 1) & 7);
    INT v2 = v1 < 4 ? 3 - v1 : v1 - 4;

    return RGB(GetRValue(rgb) + v2 * (191 - GetRValue(rgb)) / 3,
        GetGValue(rgb) - v2 * GetGValue(rgb) / 3,
        GetBValue(rgb) - v2 * GetBValue(rgb) / 3);
}

// 0x69CC10
CRect CScreenWorldMap::GetAreaRect(DWORD nMap, DWORD nArea)
{
    CString sLabel;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 2768
    UTIL_ASSERT(pGame != NULL);

    CString sResArea;

    pGame->GetVisibleArea()->m_resRef.CopyToString(sResArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sResArea);

    CWorldMapArea* pArea = pWorldMap->GetArea(nMap, nArea);

    CSize frameCenter;
    m_vcAreas.GetFrameCenterPoint(static_cast<SHORT>(pArea->m_sequence), 0, frameCenter, FALSE);

    CSize frameSize;
    m_vcAreas.GetFrameSize(static_cast<SHORT>(pArea->m_sequence), 0, frameSize, FALSE);

    CSize areaPos = pWorldMap->GetAreaPosition(pArea);

    CRect rArea;
    rArea.left = areaPos.cx;
    rArea.top = areaPos.cy;
    rArea.right = rArea.left + frameSize.cx;
    rArea.bottom = rArea.top + frameSize.cy;
    rArea.OffsetRect(-frameCenter.cx, -frameCenter.cy);

    if (pArea->m_strLabel != -1) {
        sLabel = FetchString(pArea->m_strLabel);
        LONG nLength = m_vfLabel.GetStringLength(sLabel, FALSE);
        LONG nHeight = m_vfLabel.GetFontHeight(FALSE) + 11;
        LONG nSideExpand = max(nLength - rArea.Width(), 0) / 2 + 1;
        rArea.InflateRect(nSideExpand,
            0,
            nSideExpand,
            nHeight);
    }

    return rArea;
}

// 0x69CE20
DWORD CScreenWorldMap::FindAreaHit(const CPoint& pt)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 2824
    UTIL_ASSERT(pGame != NULL);

    CString sResArea;

    pGame->GetVisibleArea()->m_resRef.CopyToString(sResArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sResArea);

    CPoint ptMapPos = m_ptMapView + pt;
    DWORD nFoundArea = -1;

    DWORD nNumAreas = pWorldMap->GetNumAreas(pWorldMap->sub_55A3A0());
    for (DWORD nArea = 0; nArea < nNumAreas; nArea++) {
        CRect rArea = m_aAreaRect[nArea];
        if (rArea.PtInRect(ptMapPos)
            && (pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nArea)->m_dwFlags & 0x4) != 0) {
            nFoundArea = nArea;
            break;
        }
    }

    return nFoundArea;
}

// 0x69CF90
void CScreenWorldMap::InvalidateArea(DWORD nArea)
{
    if (nArea != -1 && m_aAreaRect.GetCount() > 0) {
        CRect rArea = m_aAreaRect[nArea];
        CRect rControl(m_pMapControl->m_ptOrigin, m_pMapControl->m_size);

        rArea.OffsetRect(-m_ptMapView);
        rArea.OffsetRect(m_pMapControl->m_ptOrigin);

        CRect rDirty;
        rDirty.IntersectRect(rArea, rControl);

        if (!rDirty.IsRectNull()) {
            m_pMapControl->m_pPanel->InvalidateRect(&rDirty);
        }
    }
}

// 0x69FFC0
void CScreenWorldMap::GetMarkerPosition(CPoint& ptMarker)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 4208
    UTIL_ASSERT(pGame != NULL);

    CString sResArea;

    pGame->GetVisibleArea()->m_resRef.CopyToString(sResArea);

    CWorldMap* pWorldMap = pGame->GetWorldMap(sResArea);

    if (m_nCurrentLink != -1) {
        DWORD nSrcArea = pWorldMap->FindSourceAreaIndex(pWorldMap->sub_55A3A0(), m_nCurrentLink);
        DWORD nDstArea = pWorldMap->GetLink(pWorldMap->sub_55A3A0(), m_nCurrentLink)->m_nArea;
        CWorldMapArea* pSrcArea = pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nSrcArea);
        CWorldMapArea* pDstArea = pWorldMap->GetArea(pWorldMap->sub_55A3A0(), nDstArea);
        ptMarker.y = (pSrcArea->m_mapLocationY + pDstArea->m_mapLocationY) / 2;
        ptMarker.x = (pWorldMap->GetAreaPosition(pSrcArea).cx + pWorldMap->GetAreaPosition(pDstArea).cx) / 2;
    } else {
        if (m_cResCurrentArea == "") {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 4258
            UTIL_ASSERT(FALSE);
        }

        DWORD nArea;
        if (pWorldMap->GetAreaIndex(pWorldMap->sub_55A3A0(), m_cResCurrentArea, nArea)
            || pWorldMap->GetAreaIndex(pWorldMap->sub_55A3A0(), CResRef("AR2626"), nArea)) {
            CRect rArea = m_aAreaRect[nArea];
            ptMarker.x = (rArea.left + rArea.right) / 2;
            ptMarker.y = rArea.top;
        } else {
            ptMarker.x = 0;
            ptMarker.y = 0;
        }
    }
}

// 0x6A01C0
void CScreenWorldMap::ClearChatMessages()
{
    m_nChatMessageCount = 0;
}

// NOTE: Inlined.
void CScreenWorldMap::FocusChatEditBox()
{
    if (g_pBaldurChitin->cNetwork.GetSessionOpen()) {
        if (m_lPopupStack.GetTailPosition() == NULL || m_lPopupStack.GetTail() == NULL) {
            CUIPanel* pPanel = m_cUIManager.GetPanel(3);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 4403
            UTIL_ASSERT(pPanel != NULL);

            CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(7));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 4405
            UTIL_ASSERT(pEdit != NULL);

            m_cUIManager.SetCapture(pEdit, CUIManager::KEYBOARD);
        }
    }
}

// 0x6A01D0
void CScreenWorldMap::GetChatEditBoxStatus(CString& sChatText, BOOL& bInputCapture)
{
    if (m_pMainPanel->m_nID == 3) {
        CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(m_pMainPanel->GetControl(7));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 4442
        UTIL_ASSERT(pEdit != NULL);

        sChatText = pEdit->GetText();
        bInputCapture = m_cUIManager.m_pFocusedControl == pEdit;
    } else {
        sChatText = "";
        bInputCapture = FALSE;
    }
}

// 0x6A02A0
void CScreenWorldMap::SetChatEditBoxStatus(const CString& sChatText, BOOL bInputCapture)
{
    if (m_pMainPanel->m_nID == 3) {
        CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(m_pMainPanel->GetControl(7));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 4480
        UTIL_ASSERT(pEdit != NULL);

        pEdit->SetText(sChatText);

        if (bInputCapture) {
            m_cUIManager.SetCapture(pEdit, CUIManager::KEYBOARD);
        }
    }
}

// -----------------------------------------------------------------------------

// 0x6A0310
CUIControlButtonWorldMapScroll::CUIControlButtonWorldMapScroll(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButtonPlusMinus(panel, controlInfo)
{
}

// 0x6A0310
CUIControlButtonWorldMapScroll::~CUIControlButtonWorldMapScroll()
{
}

// 0x6A03F0
void CUIControlButtonWorldMapScroll::AdjustValue()
{
    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 4543
    UTIL_ASSERT(pWorldMap != NULL);

    switch (m_nID) {
    case 1:
        if (!pWorldMap->m_bMapDragging) {
            pWorldMap->SetMapView(pWorldMap->m_ptMapView - CPoint(0, 20));
        }
        break;
    case 2:
        if (!pWorldMap->m_bMapDragging) {
            pWorldMap->SetMapView(pWorldMap->m_ptMapView + CPoint(0, 20));
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
        // __LINE__: 4556
        UTIL_ASSERT(FALSE);
    }
}

// -----------------------------------------------------------------------------

// 0x6A04B0
CUIControlButtonWorldMapDone::CUIControlButtonWorldMapDone(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
}

// 0x6A0500
CUIControlButtonWorldMapDone::~CUIControlButtonWorldMapDone()
{
}

// 0x6A05A0
void CUIControlButtonWorldMapDone::OnLButtonClick(CPoint pt)
{
    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 4609
    UTIL_ASSERT(pWorldMap != NULL);

    CSingleLock renderLock(&(pWorldMap->GetManager()->field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    pWorldMap->OnDoneButtonClick();

    renderLock.Unlock();
}

// -----------------------------------------------------------------------------

// 0x6A06B0
CUIControlButtonWorldMapWorldMap::CUIControlButtonWorldMapWorldMap(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    field_676 = 0;
    field_67A = 0;
}

// 0x6A0710
CUIControlButtonWorldMapWorldMap::~CUIControlButtonWorldMapWorldMap()
{
}

// 0x6A07B0
BOOL CUIControlButtonWorldMapWorldMap::Render(BOOL bForce)
{
    if (!m_bActive && !m_bInactiveRender) {
        return FALSE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return FALSE;
    }

    if (m_nRenderCount != 0) {
        CSingleLock lock(&(m_pPanel->m_pManager->field_56), FALSE);
        lock.Lock(INFINITE);
        m_nRenderCount--;
        lock.Unlock();
    }

    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 4701
    UTIL_ASSERT(pWorldMap != NULL);

    CRect r = m_rDirty;
    r.OffsetRect(-m_ptOrigin);
    r.OffsetRect(-m_pPanel->m_ptOrigin);

    return pWorldMap->DrawMap(r);
}

// 0x6A0940
void CUIControlButtonWorldMapWorldMap::OnLButtonUp(CPoint pt)
{
    if (m_bActive) {
        if ((m_nMouseButtons & LBUTTON) != 0) {
            // NOTE: Uninline.
            m_pPanel->m_pManager->KillCapture();

            CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
            // __LINE__: 4740
            UTIL_ASSERT(pWorldMap != NULL);

            pWorldMap->OnMapMouseUp(pt);
        }
    }
}

// 0x6A09B0
BOOL CUIControlButtonWorldMapWorldMap::OnLButtonDown(CPoint pt)
{
    if (!m_bActive) {
        return FALSE;
    }

    if ((m_nMouseButtons & LBUTTON) == 0) {
        return FALSE;
    }

    m_pPanel->m_pManager->SetCapture(this, CUIManager::MOUSELBUTTON);
    m_pPanel->m_pManager->field_2D = 0;
    m_pPanel->m_pManager->field_32 = m_nID;
    m_pPanel->m_pManager->field_1C = 0;

    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 4780
    UTIL_ASSERT(pWorldMap != NULL);

    pWorldMap->OnMapMouseDown(pt);

    return TRUE;
}

// 0x6A0A40
void CUIControlButtonWorldMapWorldMap::OnMouseMove(CPoint pt)
{
    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorldMap.cpp
    // __LINE__: 4832
    UTIL_ASSERT(pWorldMap != NULL);

    pWorldMap->OnMapMouseMove(pt);
}
