#include "CScreenWorld.h"

#include "CBaldurChitin.h"
#include "CBaldurProjector.h"
#include "CDeathSound.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CIcon.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CScreenChapter.h"
#include "CScreenCharacter.h"
#include "CScreenConnection.h"
#include "CScreenInventory.h"
#include "CScreenLoad.h"
#include "CScreenMap.h"
#include "CScreenSinglePlayer.h"
#include "CScreenStore.h"
#include "CScreenWorldMap.h"
#include "CStore.h"
#include "CUIControlFactory.h"
#include "CUIControlTextDisplay.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "ScreenLayout.h"

// 0x85A1EC
const LONG CScreenWorld::BORED_TIME = 3000;

// 0x8F85BC
int CScreenWorld::dword_8F85BC;
BOOL CScreenWorld::bMultiplayerStore; // 0x8F85B8

// 0x685610
CScreenWorld::CScreenWorld()
{
    field_11E = 0;
    m_bHardPaused = FALSE;
    field_14E = 0;
    m_bPausedBeforePickParty = FALSE;
    field_15C = 0;
    field_15D = 0;
    field_15E = 0;
    field_160 = 0;
    field_162 = 0;
    field_164 = 0;
    m_nTopContainerRow = 0;
    m_nTopGroupRow = 0;
    field_10CC = 0;
    m_ambianceForce = FALSE;
    m_bInControlOfDialog = FALSE;
    m_bInControlOfStore = FALSE;
    field_110C = 0;
    field_114C = 0;
    field_1150 = 0;
    m_nPendingMapWorldDirection = 0;
    m_nLeaveAreaLuaPanicDirection = 0;
    m_comingOutOfDialog = 0;

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
    m_pVirtualKeys[90] = CKeyInfo(VK_PRIOR, 0, 0);
    m_pVirtualKeys[91] = CKeyInfo(VK_NEXT, 0, 0);

    // FIXME: Buffer overrun.
    m_pVirtualKeys[92] = CKeyInfo(VK_PROCESSKEY, -1, 0);

    m_bored = FALSE;
    m_boredCount = 0;
    m_playerShutdown = FALSE;
    m_bShiftKeyDown = FALSE;
    m_bCtrlKeyDown = FALSE;
    m_bCapsLockKeyOn = FALSE;
    m_bMenuKeyDown = FALSE;
    m_bPaused = FALSE;
    field_142 = 0;
    field_14A = 0;
    m_pActiveDialogDisplay = NULL;
    m_pActiveChatDisplay = NULL;
    field_156 = 0;
    field_F37 = 0;
    field_15A = 1;
    m_newViewSize.left = 0;
    m_newViewSize.top = 0;
    m_newViewSize.right = 1;
    m_newViewSize.bottom = 1;
    m_bForceViewSize = FALSE;
    m_waitingOnResize = 0;
    field_F44 = 0;
    m_scrollLockId = CGameObjectArray::INVALID_INDEX;
    field_EA4 = -1;
    field_10B2 = -1;
    field_10B4 = 0;
    m_interactionIndex = CGameObjectArray::INVALID_INDEX;
    m_interactionTarget = CGameObjectArray::INVALID_INDEX;
    field_10C0 = "";
    field_10C4 = 0;
    field_10C8 = 0;
    field_10D0 = CGameObjectArray::INVALID_INDEX;
    m_deltaTime = 0;
    m_nChatMessageCount = 0;
    m_movie = "";
    field_1108 = 0;
    m_bGameOverPanel = 0;
    field_10F0 = 0;
    field_10F4 = -1;
    m_autoPauseId = CGameObjectArray::INVALID_INDEX;
    m_autoPauseRef = -1;
    m_nPickPartyNumCharacters = -1;
    m_bSetNightOnActivate = FALSE;
    m_bSetDayOnActivate = FALSE;
    m_bEndMajorEventListenToJoin = FALSE;
    m_bEndMajorEventPauseStatus = FALSE;
    m_bChapterTransitionPending = FALSE;
    m_nChapterTransition = 255;
    m_bMoviePending = FALSE;
    m_bRestPending = FALSE;
    m_bRestRenting = FALSE;
    m_nRestHP = 0;
    m_bRestMovie = FALSE;
    m_bPendingMapWorld = FALSE;
    m_bPendingReformParty = FALSE;
    field_119D = 0;
    InitializeCriticalSection(&field_106);
    m_nBattleCryTimeOut = 0;
    m_nPartySizeCheckStartDelay = 0;
    m_bLeaveAreaLuaPanicPending = FALSE;
    m_ulLeaveAreaLuaPanicTimer = -1;
    m_bPlayEndCredits = FALSE;
    field_11B6 = -1;
    field_11BA = -1;
    field_11BE = 0;
    m_lastAmbiance = CGameObjectArray::INVALID_INDEX;
    m_comingOutOfDialog = 0;
    m_nAutoHideInterface = 0;
    m_nAutoUnhideInterface = 0;
    field_11D0 = CInfinity::stru_8E79B8;
    field_11F0 = 1;
    field_11F4 = 1;
    field_11F8 = CInfinity::stru_8E79B8.bottom;
    field_11FC = CInfinity::stru_8E79E0.bottom;
    field_11EC = CInfinity::stru_8E79B8.right;
    m_nBlackOutCountDown = 0;
    field_1204 = 0;
}

// 0x49FC40
BOOL CScreenWorld::CheckSystemKeyCtrl()
{
    return TRUE;
}

// 0x66F230
void CScreenWorld::SetSystemKeyCtrl(BOOLEAN bValue)
{
    m_bCtrlKeyDown = bValue;
}

// 0x686610
BOOL CScreenWorld::GetCtrlKey()
{
    return m_bCtrlKeyDown;
}

// 0x49FC40
BOOL CScreenWorld::CheckSystemKeyShift()
{
    return TRUE;
}

// 0x686600
void CScreenWorld::SetSystemKeyShift(BOOL bValue)
{
    m_bShiftKeyDown = bValue;
}

// 0x6865F0
BOOL CScreenWorld::GetShiftKey()
{
    return m_bShiftKeyDown;
}

// 0x49FC40
BOOL CScreenWorld::CheckSystemKeyCapsLock()
{
    return TRUE;
}

// 0x686620
void CScreenWorld::SetSystemKeyCapsLock(BOOL bValue)
{
    m_bCapsLockKeyOn = bValue;
}

// 0x686630
BOOL CScreenWorld::GetCapsLockKey()
{
    return m_bCapsLockKeyOn;
}

// 0x49FC40
BOOL CScreenWorld::CheckSystemKeyMenu()
{
    return TRUE;
}

// 0x686650
void CScreenWorld::SetSystemKeyMenu(BOOL bValue)
{
    m_bMenuKeyDown = bValue;
}

// 0x686640
BOOL CScreenWorld::GetMenuKey()
{
    return m_bMenuKeyDown;
}

// 0x686660
SHORT CScreenWorld::GetNumVirtualKeys()
{
    return CSCREENWORLD_VIRTUAL_KEYS;
}

// 0x686670
CKeyInfo* CScreenWorld::GetVirtualKeys()
{
    return m_pVirtualKeys;
}

// 0x686680
BYTE* CScreenWorld::GetVirtualKeysFlags()
{
    return m_pVirtualKeysFlags;
}

// 0x686690
void CScreenWorld::TimerAsynchronousUpdate()
{
    AsynchronousUpdate(TRUE);
}

// 0x6866C0
CScreenWorld::~CScreenWorld()
{
    DeleteCriticalSection(&field_106);

    while (!m_deathSoundList.IsEmpty()) {
        CDeathSound* pDeathSound = m_deathSoundList.RemoveHead();
        delete pDeathSound;
    }
}

// 0x686870
int CScreenWorld::GetSelectedCharacter()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    LONG iSprite = pGame->GetGroup()->GetGroupLeader();
    SHORT nCharacter;

    if (iSprite == CGameObjectArray::INVALID_INDEX) {
        BOOLEAN bFoundControlledCharacter = FALSE;

        for (nCharacter = 0; nCharacter < CINFGAME_MAXCHARACTERS; nCharacter++) {
            // NOTE: Uninline.
            LONG nCharacterId = pGame->GetCharacterId(nCharacter);

            if (nCharacterId != CGameObjectArray::INVALID_INDEX) {
                CGameSprite* pSprite;

                BYTE rc;
                do {
                    rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    // NOTE: Uninline.
                    if (pSprite->InControl()) {
                        bFoundControlledCharacter = TRUE;
                    }

                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }

            if (bFoundControlledCharacter) {
                break;
            }
        }

        if (!bFoundControlledCharacter) {
            nCharacter = 0;
        }
    } else {
        nCharacter = pGame->GetCharacterPortraitNum(iSprite);
    }

    m_nSelectedCharacter = nCharacter;

    return m_nSelectedCharacter;
}

// 0x71E750
//
// NOTE: the exe's CScreenWorld vtable overrides CWarp slot 0x0010 with the
// ICF-folded empty body at this address (one shared no-op; see
// docs/wso/import-parked/71E750.json and CBaldurEngine::SetChatEditBoxStatus
// for the same pattern). Recovered as this empty override.
void CScreenWorld::NormalizePanelRect(DWORD nID, CRect& rect)
{
}

// 0x6869C0
void CScreenWorld::EngineActivated()
{
    if (CChitin::byte_8FB950
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && field_142) {
        g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(1, 1, 0);
        field_142 = FALSE;
    }

    if (m_cUIManager.m_bInitialized) {
        if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded == TRUE) {
            g_pBaldurChitin->field_F9 = TRUE;
            g_pBaldurChitin->GetObjectGame()->WorldEngineActivated(pVidMode);
            field_119D = TRUE;

            g_pBaldurChitin->GetObjectCursor()->SetCursor(0, TRUE);
            g_pBaldurChitin->GetObjectCursor()->m_bVisible = TRUE;

            m_cUIManager.InvalidateRect(NULL);

            // NOTE: Uninline.
            SetNewViewSize(GetNewViewSize(), TRUE);

            if (m_bSetNightOnActivate) {
                g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetInfinity()->SetNight();
                m_bSetNightOnActivate = FALSE;
            }

            if (m_bSetDayOnActivate) {
                g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetInfinity()->SetDay();
                g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetInfinity()->SetDawn(CInfinity::TRUE_DAWNDUSK_INTENSITY, TRUE);
                m_bSetDayOnActivate = FALSE;
            }

            if (!m_bHardPaused
                && m_nAutoHideInterface != 0) {
                DWORD dwInputMode = g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode;
                if (dwInputMode != 0x182 && dwInputMode != 0x502) {
                    // NOTE: Uninline.
                    CancelPopup();
                }

                m_nAutoHideInterface--;
                HideInterface();
            }

            CUIControlTextDisplay* pText = m_pActiveChatDisplay;
            if (pText == NULL) {
                pText = m_pActiveDialogDisplay;
            }

            if (pText != NULL) {
                m_nChatMessageCount = g_pBaldurChitin->GetBaldurMessage()->m_cChatBuffer.UpdateTextDisplay(pText,
                    m_nChatMessageCount);
            }
        }
    }
}

// 0x686D00
void CScreenWorld::EngineDeactivated()
{
    if (CChitin::byte_8FB950
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0);
    }

    // NOTE: Uninline.
    m_cUIManager.KillCapture();

    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded == TRUE) {
        if (!m_bPaused && g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
            g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->StopTime();
        }

        g_pBaldurChitin->GetObjectGame()->WorldEngineDeactivated();
        g_pBaldurChitin->field_F9 = 0;
    }
}

// 0x686DE0
void CScreenWorld::EngineGameInit()
{
    g_pBaldurChitin->GetObjectGame()->m_bForceDither = FALSE;

    m_cUIManager.fInit(this,
        CResRef(CString("GUIW") + CBaldurChitin::CHUI_GUIEXT),
        g_pBaldurChitin->field_4A2C);

    // iwd2r: GUIW08 and GUIW10 are laid out for 800x600 and 1024x768, every
    // panel along the bottom edge. At other sizes they move to the bottom
    // centre, where the viewport rectangles (iwd2r::ComputeScreenLayout)
    // leave room for them. Zero at the original four sizes.
    const iwd2r::ScreenLayout& screenLayout = iwd2r::CurrentScreenLayout();
    if (screenLayout.worldUiShiftX != 0 || screenLayout.worldUiShiftY != 0) {
        m_cUIManager.ShiftPanels(CPoint(screenLayout.worldUiShiftX, screenLayout.worldUiShiftY));
    }

    m_pCurrentScrollBar = NULL;
    m_bored = FALSE;
    m_boredCount = 0;
    m_playerShutdown = FALSE;
    m_bPaused = FALSE;
    field_14A = 0;
    m_pActiveDialogDisplay = NULL;
    m_pActiveChatDisplay = NULL;
    field_156 = 0;
    field_F37 = 0;
    field_15A = 1;
    m_newViewSize.left = 0;
    m_newViewSize.top = 0;
    m_newViewSize.right = 1;
    m_newViewSize.bottom = 1;
    m_bForceViewSize = FALSE;
    m_waitingOnResize = 0;
    field_F44 = 0;
    m_scrollLockId = CGameObjectArray::INVALID_INDEX;
    field_EA4 = -1;
    field_10B2 = -1;
    field_10B4 = 0;
    m_interactionIndex = CGameObjectArray::INVALID_INDEX;
    m_interactionTarget = CGameObjectArray::INVALID_INDEX;
    field_10C0 = "";
    field_10C4 = 0;
    field_10C8 = 0;
    field_10D8 = CGameObjectArray::INVALID_INDEX;
    m_deltaTime = 0;
    m_nChatMessageCount = 0;
    m_movie = "";
    field_1108 = 0;
    m_bGameOverPanel = FALSE;
    field_10F0 = 0;
    field_10F4 = -1;
    m_autoPauseId = CGameObjectArray::INVALID_INDEX;
    m_autoPauseRef = -1;
    m_nPickPartyNumCharacters = -1;
    m_bSetNightOnActivate = 0;
    m_bSetDayOnActivate = 0;
    m_bEndMajorEventListenToJoin = 0;
    m_bEndMajorEventPauseStatus = 0;
    m_bChapterTransitionPending = 0;
    m_nChapterTransition = 255;
    m_bMoviePending = 0;
    m_bRestPending = 0;
    m_bRestRenting = 0;
    m_nRestHP = 0;
    m_bRestMovie = 0;
    m_bPendingMapWorld = 0;
    m_bPendingReformParty = 0;
    field_119D = 0;
    m_nBattleCryTimeOut = 0;
    m_nPartySizeCheckStartDelay = 0;
    m_bLeaveAreaLuaPanicPending = 0;
    m_ulLeaveAreaLuaPanicTimer = -1;
    m_bPlayEndCredits = FALSE;
    field_11B6 = -1;
    field_11BA = -1;
    field_11BE = 0;

    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(19)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(21)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(7)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(6)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(8)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(17)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(22)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(0)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(GetPanel_22_0())->SetActive(TRUE);

    m_pActiveDialogDisplay = static_cast<CUIControlTextDisplay*>(g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(GetPanel_22_0())->GetControl(1));
    m_nAutoHideInterface = 0;
    m_nAutoUnhideInterface = 0;

    if (!field_11F0) {
        field_11F0 = TRUE;
        CInfinity::stru_8E79B8.bottom = field_11F8;
    }

    if (!field_11F4) {
        field_11F4 = TRUE;
        CInfinity::stru_8E79B8.bottom = field_11F8;
    }

    while (!m_deathSoundList.IsEmpty()) {
        CDeathSound* pDeathSound = m_deathSoundList.RemoveHead();
        if (pDeathSound != NULL) {
            delete pDeathSound;
        }
    }
    m_deathSoundList.RemoveAll();
}

// 0x6872D0
void CScreenWorld::OnKeyDown(SHORT nKeysFlags)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Unused.
    CAITrigger trigger(CAITrigger::NO_TRIGGER, 0);

    if ((pGame->GetGameSave()->m_mode & 0x1) == 0) {
        // Without game input (a dialog, a cut scene): the tool tips, the
        // screenshot, the dialog's buttons and the reply numbers.
        for (SHORT nKeyFlag = 0; nKeyFlag < nKeysFlags; nKeyFlag++) {
            BYTE nKey = m_pVirtualKeysFlags[nKeyFlag];

            if ((pGame->GetGameSave()->m_mode & 0x2) != 0
                && m_cUIManager.OnKeyDown(nKey)) {
                continue;
            }

            if (nKey == VK_TAB) {
                CGameArea* pArea = pGame->GetVisibleArea();
                const CPoint& ptPointer = g_pChitin->m_ptPointer;
                if (pArea->m_iPicked != CGameObjectArray::INVALID_INDEX
                    || ptPointer.x < pArea->GetInfinity()->rViewPort.left
                    || ptPointer.x >= pArea->GetInfinity()->rViewPort.right
                    || ptPointer.y < pArea->GetInfinity()->rViewPort.top
                    || ptPointer.y >= pArea->GetInfinity()->rViewPort.bottom) {
                    m_cUIManager.ForceToolTip();
                }
                continue;
            }

            if (nKey == VK_SNAPSHOT) {
                g_pBaldurChitin->GetCurrentVideoMode()->PrintScreen();
                continue;
            }

            if (nKey == VK_RETURN) {
                CUIControlButton* pButton = static_cast<CUIControlButton*>(m_cUIManager.GetPanel(9)->GetControl(0));
                if (pButton->m_bActive) {
                    if (GetTickCount() > field_11BA + 600) {
                        pButton->OnLButtonClick(CPoint(0, 0));
                        field_11BA = GetTickCount();
                    }
                    continue;
                }
            }

            if (nKey == pGame->m_pKeymap[24]
                && pGame->m_pKeymapFlags[24] == m_bCtrlKeyDown) {
                // NOTE: 0x694510 (the multiplayer chat's panel) runs only
                // under a provider other than the null one (its gate at
                // 0x6945C1), so a single-player host session returns from
                // it at once; not called.
                sub_693710();
                continue;
            }

            // A number picks the dialog's reply of that number.
            LONG nDialogPanel = -1;
            if (m_pActiveDialogDisplay != NULL && m_pActiveDialogDisplay->m_pPanel != NULL) {
                nDialogPanel = m_pActiveDialogDisplay->m_pPanel->m_nID;
            }

            if (((nKey >= '1' && nKey <= '9') || (nKey >= VK_NUMPAD1 && nKey <= VK_NUMPAD9))
                && g_pBaldurChitin->m_pEngineWorld->m_bInControlOfDialog
                && (nDialogPanel == static_cast<LONG>(GetPanel_21_7())
                    || nDialogPanel == static_cast<LONG>(GetPanel_21_0())
                    || nDialogPanel == static_cast<LONG>(GetPanel_19_0()))) {
                CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
                CGameDialogEntry* pEntry = pWorld->m_internalLoadedDialog.m_dialogEntries[pWorld->m_internalLoadedDialog.m_currentEntryIndex];

                SHORT nNumber;
                if (nKey >= '1' && nKey <= '9') {
                    nNumber = nKey - '0';
                } else {
                    nNumber = nKey - '`';
                }

                for (SHORT nReply = 0; nReply < pEntry->GetSize(); nReply++) {
                    if (pEntry->GetAt(nReply)->m_displayListId == nNumber) {
                        if (GetTickCount() > field_11BA + 600) {
                            g_pBaldurChitin->m_pEngineWorld->m_internalLoadedDialog.m_responseMarker = nReply;
                            field_11BA = GetTickCount();
                        }
                        break;
                    }
                }
            }

            if (m_cUIManager.m_bHidden) {
                continue;
            }

            // NOTE: The original walks one-entry tables of panels and
            // controls.
            if (nKey == VK_NEXT) {
                CUIPanel* pPanel = m_cUIManager.GetPanel(19);
                if (pPanel->m_bActive) {
                    CUIControlBase* pControl = pPanel->GetControl(0);
                    if (pControl->m_bActive) {
                        pControl->OnLButtonUp(pControl->m_ptOrigin);
                    }
                }
            }

            if (nKey == VK_PRIOR) {
                CUIPanel* pPanel = m_cUIManager.GetPanel(21);
                if (pPanel->m_bActive) {
                    CUIControlBase* pControl = pPanel->GetControl(0);
                    if (pControl->m_bActive) {
                        pControl->OnLButtonUp(pControl->m_ptOrigin);
                    }
                }
            }
        }

        return;
    }

    for (SHORT nKeyFlag = 0; nKeyFlag < nKeysFlags; nKeyFlag++) {
        if ((pGame->GetGameSave()->m_mode & 0x2) != 0
            && m_cUIManager.OnKeyDown(m_pVirtualKeysFlags[nKeyFlag])) {
            continue;
        }

        BYTE nKey = m_pVirtualKeysFlags[nKeyFlag];

        // Scripts see the key (the HotKey trigger).
        CAITrigger hotKey(CAITRIGGER_HOTKEY, nKey);
        pGame->m_group.AddTrigger(hotKey);

        if (field_156 && m_bCtrlKeyDown && nKey != pGame->m_pKeymap[22]) {
            // TODO: Incomplete. 0x68956A-0x68BC5D: the cheat keys (Ctrl
            // with `field_156`, which ours never sets) for the party leader
            // and the area; the original returns after one.
            return;
        }

        CGameSprite* pSprite;
        BYTE nButtonId;

        switch (nKey) {
        case VK_SNAPSHOT:
            g_pBaldurChitin->GetCurrentVideoMode()->PrintScreen();
            break;
        case VK_ESCAPE:
            if (field_EA4 == -1) {
                if (pGame->m_nState != 0) {
                    pGame->m_nState = 0;
                    pGame->m_cButtonArray.m_nSelectedButton = 100;
                    pGame->m_cButtonArray.UpdateState();
                    pGame->m_tempCursor = 4;
                } else if (pGame->m_cButtonArray.field_19B2 != 0) {
                    pGame->m_cButtonArray.m_nSelectedButton = 100;
                    pGame->m_cButtonArray.ResetState();
                } else {
                    g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(9);
                }
                break;
            }
            // FALLTHROUGH
        case VK_RETURN:
            if (field_EA4 == -1) {
                break;
            }

            if (field_EA4 == static_cast<int>(GetPanel_21_7())) {
                // The dialog's continue or end.
                field_1150 = 1;
                if (!field_10B4) {
                    m_internalLoadedDialog.m_responseMarker = field_10B2;
                }
                break;
            }

            switch (field_EA4) {
            case 0:
            case 7:
            case 17:
            case 19:
            case 21:
            case 22:
                break;
            case 6:
                StopCommand();
                field_EA4 = -1;
                break;
            case 8:
                StopContainer();
                field_EA4 = -1;
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 5469
                UTIL_ASSERT(FALSE);
            }
            break;
        case VK_TAB:
            if (pGame->m_cOptions.m_bCheatsEnabled && m_bCtrlKeyDown) {
                // The command line.
                if (!m_cUIManager.m_bHidden) {
                    field_F37 = ~field_F37;
                    if (field_F37 != 0) {
                        StartCommand();
                        CUIControlEditLua* pEdit = static_cast<CUIControlEditLua*>(m_cUIManager.GetPanel(6)->GetControl(0));
                        pEdit->field_8A6 = 0;
                        pEdit->field_8A2 = CGameObjectArray::INVALID_INDEX;
                    } else if (field_EA4 == 6) {
                        StopCommand();
                    }
                }
            } else {
                CGameArea* pArea = pGame->GetVisibleArea();
                const CPoint& ptPointer = g_pChitin->m_ptPointer;
                if (pArea->m_iPicked != CGameObjectArray::INVALID_INDEX
                    || ptPointer.x < pArea->GetInfinity()->rViewPort.left
                    || ptPointer.x >= pArea->GetInfinity()->rViewPort.right
                    || ptPointer.y < pArea->GetInfinity()->rViewPort.top
                    || ptPointer.y >= pArea->GetInfinity()->rViewPort.bottom) {
                    m_cUIManager.ForceToolTip();
                    pArea->m_nToolTip = pArea->m_pGame->m_cOptions.m_nTooltips;
                }
            }
            break;
        case VK_SPACE:
            TogglePauseGame(1, 1, 0);
            break;
        case VK_NEXT:
            if (!m_cUIManager.m_bHidden) {
                CUIPanel* pPanel = m_cUIManager.GetPanel(19);
                if (pPanel->m_bActive) {
                    CUIControlBase* pControl = pPanel->GetControl(0);
                    if (pControl->m_bActive) {
                        pControl->OnLButtonUp(pControl->m_ptOrigin);
                    }
                }
            }
            break;
        case VK_PRIOR:
            if (!m_cUIManager.m_bHidden) {
                CUIPanel* pPanel = m_cUIManager.GetPanel(21);
                if (pPanel->m_bActive) {
                    CUIControlBase* pControl = pPanel->GetControl(0);
                    if (pControl->m_bActive) {
                        pControl->OnLButtonUp(pControl->m_ptOrigin);
                    }
                }
            }
            break;
        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
        case VK_F10:
        case VK_F11:
        case VK_F12:
            // The action bar's buttons.
            if (m_cUIManager.GetPanel(1)->m_bActive) {
                if (m_bShiftKeyDown) {
                    pGame->m_cButtonArray.OnRButtonPressed(nKey - VK_F1);
                } else {
                    pGame->m_cButtonArray.OnLButtonPressed(nKey - VK_F1);
                }
            }
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
            if (pGame->m_nState == 0) {
                // NOTE: Uninline (GetCharacterId).
                if (pGame->m_group.GetCount() == 1
                    && pGame->GetCharacterId(nKey - '1') == pGame->m_group.GetGroupLeader()) {
                    pGame->OnPortraitLDblClick(nKey - '1');
                } else {
                    if (!m_bCtrlKeyDown && !m_bShiftKeyDown) {
                        pGame->UnselectAll();
                    }
                    pGame->SelectCharacter(pGame->GetCharacterId(nKey - '1'), TRUE);
                    pGame->SelectToolbar();
                }
            } else {
                // The character is the target of the action under way.
                if (pGame->GetObjectArray()->GetShare(pGame->GetCharacterId(nKey - '1'),
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    CGameArea* pArea = pGame->GetVisibleArea();
                    if (pArea == pSprite->GetArea()) {
                        if (pGame->m_nState == 1) {
                            pArea->OnActionButtonClickGround(pSprite->GetPos());
                        } else {
                            pSprite->OnActionButton(pSprite->GetPos());
                        }
                    }

                    pGame->GetObjectArray()->ReleaseShare(pGame->GetCharacterId(nKey - '1'),
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
            break;
        case VK_NUMPAD1:
            pGame->GetVisibleArea()->m_nKeyScrollState = 6;
            break;
        case VK_DOWN:
        case VK_NUMPAD2:
            pGame->GetVisibleArea()->m_nKeyScrollState = 5;
            break;
        case VK_NUMPAD3:
            pGame->GetVisibleArea()->m_nKeyScrollState = 4;
            break;
        case VK_LEFT:
        case VK_NUMPAD4:
            pGame->GetVisibleArea()->m_nKeyScrollState = 7;
            break;
        case VK_RIGHT:
        case VK_NUMPAD6:
            pGame->GetVisibleArea()->m_nKeyScrollState = 3;
            break;
        case VK_NUMPAD7:
            pGame->GetVisibleArea()->m_nKeyScrollState = 8;
            break;
        case VK_UP:
        case VK_NUMPAD8:
            pGame->GetVisibleArea()->m_nKeyScrollState = 1;
            break;
        case VK_NUMPAD9:
            pGame->GetVisibleArea()->m_nKeyScrollState = 2;
            break;
        default: {
            // The keymap (Keymap.ini): the first entry with this key and
            // the Ctrl state.
            SHORT index;
            for (index = 0; index < CINFGAME_KEYMAP_SIZE; index++) {
                // NOTE: Uninline.
                if (pGame->GetKeymap(index) == nKey
                    && pGame->GetKeymapFlag(index) == static_cast<BYTE>(m_bCtrlKeyDown)) {
                    break;
                }
            }

            if (index >= CINFGAME_KEYMAP_SIZE) {
                break;
            }

            switch (index) {
            case 0:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(5);
                break;
            case 1:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(8);
                break;
            case 2:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(0);
                break;
            case 3:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(6);
                break;
            case 4:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(7);
                break;
            case 5:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(4);
                break;
            case 6:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(9);
                break;
            case 7:
                g_pBaldurChitin->GetActiveEngine()->OnLeftPanelButtonClick(13);
                break;
            case 8:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(7);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    }
                }
                break;
            case 9:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(13);
                    if (nButtonId == 0xFF) {
                        // Open the list first (button type 5), then press.
                        nButtonId = pGame->GetButtonArray()->GetButtonId(5);
                        if (nButtonId == 0xFF) {
                            break;
                        }
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                        nButtonId = pGame->GetButtonArray()->GetButtonId(13);
                        if (nButtonId == 0xFF) {
                            break;
                        }
                    }
                    pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                }
                break;
            case 10:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(2);
                    if (nButtonId != 0xFF) {
                        if (m_bShiftKeyDown) {
                            pGame->GetButtonArray()->OnRButtonPressed(nButtonId);
                        } else {
                            pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                        }
                    }
                }
                break;
            case 11:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(3);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    } else {
                        // The customize state's key.
                        pGame->GetButtonArray()->sub_594170(3);
                    }
                }
                break;
            case 12:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(14);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    } else {
                        pGame->GetButtonArray()->sub_594170(14);
                    }
                }
                break;
            case 13:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(5);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    } else {
                        pGame->GetButtonArray()->sub_594170(5);
                    }
                }
                break;
            case 14:
            case 15:
            case 16: {
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    INT nButtonType;
                    switch (index) {
                    case 14:
                        nButtonType = 12;
                        break;
                    case 15:
                        nButtonType = 11;
                        break;
                    default:
                        nButtonType = 4;
                        break;
                    }

                    nButtonId = pGame->GetButtonArray()->GetButtonId(nButtonType);
                    if (nButtonId == 0xFF) {
                        // Open the list first (button type 5), then press.
                        nButtonId = pGame->GetButtonArray()->GetButtonId(5);
                        if (nButtonId == 0xFF) {
                            break;
                        }
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                        nButtonId = pGame->GetButtonArray()->GetButtonId(nButtonType);
                        if (nButtonId == 0xFF) {
                            break;
                        }
                    }
                    pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                }
                break;
            }
            case 17:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(15);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    }
                }
                break;
            case 18:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(8);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    } else {
                        pGame->GetButtonArray()->OnLButtonPressed(1);
                    }
                }
                break;
            case 19:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(10);
                    if (nButtonId != 0xFF) {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    } else {
                        pGame->GetButtonArray()->sub_594170(10);
                    }
                }
                break;
            case 20:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    if (pGame->GetObjectArray()->GetDeny(pGame->GetGroup()->GetGroupLeader(),
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE)
                        == CGameObjectArray::SUCCESS) {
                        pSprite->EquipMostDamagingMelee();

                        pGame->GetObjectArray()->ReleaseDeny(pGame->GetGroup()->GetGroupLeader(),
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                }
                break;
            case 21:
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    if (pGame->GetObjectArray()->GetDeny(pGame->GetGroup()->GetGroupLeader(),
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE)
                        == CGameObjectArray::SUCCESS) {
                        // 0x6893E8
                        pSprite->EquipRanged();

                        pGame->GetObjectArray()->ReleaseDeny(pGame->GetGroup()->GetGroupLeader(),
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                }
                break;
            case 22:
                // Quick load.
                if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                    && pGame->SaveGameExists(CInfGame::QUICK_SAVE_NAME)) {
                    pGame->DestroyGame(TRUE, FALSE);
                    pGame->m_sSaveGame = CInfGame::QUICK_SAVE_NAME;
                    CScreenCharacter::SAVE_NAME = CInfGame::QUICK_SAVE_NAME;
                    pGame->LoadGame(TRUE, FALSE);
                    g_pBaldurChitin->GetObjectGame()->MultiplayerSetCharacterCreationLocation();
                    pGame->SetupCharacters(FALSE);

                    for (INT nSlot = 0; nSlot < 6; nSlot++) {
                        if (pGame->GetCharacterSlot(nSlot) != CGameObjectArray::INVALID_INDEX) {
                            pGame->GetMultiplayerSettings()->SignalCharacterStatus(nSlot,
                                CMultiplayerSettings::CHARSTATUS_SIGNAL_IMPORTED,
                                TRUE,
                                TRUE);
                        } else {
                            pGame->GetMultiplayerSettings()->SignalCharacterStatus(nSlot,
                                CMultiplayerSettings::CHARSTATUS_SIGNAL_DELETED,
                                TRUE,
                                TRUE);
                        }
                    }

                    if (pGame->m_bGameLoaded == TRUE) {
                        EngineActivated();
                    }
                }
                break;
            case 23:
                // Hide or show the interface.
                m_nAutoHideInterface = 0;
                m_nAutoUnhideInterface = 0;
                if (m_cUIManager.m_bHidden) {
                    UnhideInterface();
                } else {
                    HideInterface();
                    m_cUIManager.ClearTooltip();
                }
                break;
            case 24:
                // NOTE: 0x694510 (the multiplayer chat's panel) runs only
                // under a provider other than the null one (its gate at
                // 0x6945C1), so a single-player host session returns from
                // it at once; not called.
                sub_693710();
                break;
            case 25: {
                // The pointer's area position.
                CString sText;
                CString sArea;
                CPoint ptWorld = pGame->GetVisibleArea()->GetInfinity()->GetWorldCoordinates(g_pChitin->m_ptPointer);
                sText.Format("Mouse Position: x=%ld, y=%ld  ", ptWorld.x, ptWorld.y);
                pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);
                sText += " Area: " + sArea;
                DisplayText(CString(""), sText, -1, FALSE);
                break;
            }
            case 26: {
                // Quick save.
                STR_RES strRes;
                STRREF strError;
                if (pGame->CanSaveGame(strError, FALSE, FALSE)) {
                    pGame->BackupQuickSave();
                    pGame->m_sSaveGame = CInfGame::QUICK_SAVE_NAME;
                    CScreenCharacter::SAVE_NAME = CInfGame::QUICK_SAVE_NAME;

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

                    if (pGame->SaveGame(TRUE, FALSE, TRUE)) {
                        strError = 10237;
                        g_pBaldurChitin->GetTlkTable().Fetch(strError, strRes);
                        DisplayText(CString(""),
                            strRes.szText,
                            0,
                            RGB(63, 255, 12),
                            -1,
                            FALSE);
                    }

                    // NOTE: The inner block cannot run: it needs the flag
                    // the outer test found set.
                    if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                        && g_pChitin->cVideo.Is3dAccelerated()) {
                        pGame->field_366E = 1;
                        pGame->field_50DC = 1;

                        if (!g_pChitin->cVideo.Is3dAccelerated()) {
                            if (!pGame->field_50D8) {
                                pGame->SynchronousUpdate();
                            }
                            pGame->field_50D8 = TRUE;
                        }
                    }
                } else {
                    g_pBaldurChitin->GetTlkTable().Fetch(strError, strRes);
                    DisplayText(CString(""),
                        strRes.szText,
                        0,
                        RGB(63, 255, 12),
                        -1,
                        FALSE);
                }
                break;
            }
            case 27: {
                CUIControlButton* pButton = static_cast<CUIControlButton*>(m_cUIManager.GetPanel(GetPanel_22_0())->GetControl(14));
                pButton->OnLButtonClick(CPoint(0, 0));
                break;
            }
            case 28: {
                // Cycle the action bar and the portraits: both shown, the
                // portraits hidden, the action bar hidden.
                CUIPanel* pLeftPanel = m_cUIManager.GetPanel(1);
                CUIPanel* pBottomPanel = m_cUIManager.GetPanel(GetPanel_22_0());
                if (pLeftPanel == NULL || pBottomPanel == NULL) {
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                    // __LINE__: 2094
                    UTIL_ASSERT(FALSE);
                }

                if (g_pBaldurChitin->field_4A24
                    && !m_cUIManager.m_bHidden
                    && !m_cUIManager.GetPanel(8)->m_bActive) {
                    CRect rViewSize;
                    if (m_waitingOnResize) {
                        rViewSize = m_newViewSize;
                    } else if (pGame->GetVisibleArea() != NULL) {
                        CopyRect(&rViewSize, &(pGame->GetVisibleArea()->GetInfinity()->rViewPort));
                    } else {
                        rViewSize = CInfinity::stru_8E79B8;
                    }

                    LONG nBottom;
                    if (field_11F0 && field_11F4) {
                        field_11F0 = 0;
                        pLeftPanel->m_bActive = FALSE;
                        field_11F8 = rViewSize.bottom;
                        nBottom = CInfinity::stru_8E79E0.bottom;
                    } else if (!field_11F0 && field_11F4) {
                        field_11F4 = 0;
                        field_11F0 = 1;
                        pLeftPanel->m_bActive = TRUE;
                        pBottomPanel->m_bActive = FALSE;
                        // NOTE: Uninline (0x68BFA0, 0x68BFD0).
                        pLeftPanel->m_ptOrigin = pLeftPanel->m_ptBaseOrigin + CPoint(0, pBottomPanel->m_size.cy);
                        field_11FC = rViewSize.bottom;
                        nBottom = CInfinity::stru_8E79C8.bottom;
                    } else {
                        field_11F4 = 1;
                        field_11F0 = 1;
                        // NOTE: Uninline (0x68BFC0).
                        pBottomPanel->m_bActive = TRUE;
                        pBottomPanel->SetActive(TRUE);
                        pLeftPanel->m_ptOrigin = pLeftPanel->m_ptBaseOrigin + CPoint(0, 0);
                        nBottom = field_11F8;
                    }

                    m_cUIManager.InvalidateRect(NULL);
                    CInfinity::stru_8E79B8.bottom = nBottom;
                    SetNewViewSize(CRect(rViewSize.left, rViewSize.top, rViewSize.right, nBottom), FALSE);
                }
                break;
            }
            case 29:
                OnRestButtonClick();
                break;
            case 30: {
                CGameSprite::SHOW_CHARACTER_HP = !CGameSprite::SHOW_CHARACTER_HP;
                for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
                    pGame->UpdatePortrait(nPortrait, 1);
                }

                // NOTE: The original formats the value with _itoa(value,
                // buf, 10).
                CString sValue;
                sValue.Format("%d", CGameSprite::SHOW_CHARACTER_HP);
                WritePrivateProfileStringA("Game Options",
                    "Show Character HP",
                    sValue,
                    g_pBaldurChitin->GetIniFileName());
                break;
            }
            case 31:
                // The formation buttons: right click the selected one.
                if (m_cUIManager.GetPanel(1)->m_bActive && pGame->GetGroup()->GetCount() > 1) {
                    CInfButtonArray* pButtonArray = pGame->GetButtonArray();
                    for (INT nButtonType = 16; nButtonType <= 20; nButtonType++) {
                        nButtonId = pGame->GetButtonArray()->GetButtonId(nButtonType);
                        if (nButtonId != 0xFF && pButtonArray->m_buttonArray[nButtonId].field_1CC == 1) {
                            pGame->GetButtonArray()->OnRButtonPressed(nButtonId);
                            break;
                        }
                    }
                }
                break;
            case 32:
            case 33:
            case 34: {
                // Select a pair of characters (1-2, 3-4, 5-6), or open the
                // first's sheet when the pair alone is selected already.
                SHORT nFirst = static_cast<SHORT>(2 * (index - 32));
                if (pGame->m_nCharacters > nFirst) {
                    INT nCount = min(pGame->m_nCharacters - nFirst, 2);
                    if (pGame->m_group.GetCount() == nCount) {
                        LONG* pGroupList = pGame->m_group.GetGroupList();
                        BYTE nMember = 0;
                        while (nMember < pGame->m_group.GetCount()
                            && pGroupList[nMember] == pGame->GetCharacterId(nFirst + nMember)) {
                            nMember++;
                        }

                        // NOTE: The original leaks the group list.
                        if (nMember == pGame->m_group.GetCount()) {
                            delete[] pGroupList;
                            pGame->OnPortraitLDblClick(nFirst);
                            break;
                        }
                        delete[] pGroupList;
                    }

                    if (!m_bCtrlKeyDown && !m_bShiftKeyDown) {
                        pGame->UnselectAll();
                    }
                    pGame->SelectCharacter(pGame->GetCharacterId(nFirst), TRUE);
                    if (pGame->m_nCharacters > nFirst + 1) {
                        pGame->SelectCharacter(pGame->GetCharacterId(nFirst + 1), FALSE);
                    }
                    pGame->SelectToolbar();
                }
                break;
            }
            case 35:
            case 36: {
                // Select a triple (1-3, 4-6), as the pairs.
                SHORT nFirst = static_cast<SHORT>(3 * (index - 35));
                if (pGame->m_nCharacters > nFirst) {
                    INT nCount = min(pGame->m_nCharacters - nFirst, 3);
                    if (pGame->m_group.GetCount() == nCount) {
                        LONG* pGroupList = pGame->m_group.GetGroupList();
                        BYTE nMember = 0;
                        while (nMember < pGame->m_group.GetCount()
                            && pGroupList[nMember] == pGame->GetCharacterId(nFirst + nMember)) {
                            nMember++;
                        }

                        // NOTE: The original leaks the group list.
                        if (nMember == pGame->m_group.GetCount()) {
                            delete[] pGroupList;
                            pGame->OnPortraitLDblClick(nFirst);
                            break;
                        }
                        delete[] pGroupList;
                    }

                    if (!m_bCtrlKeyDown && !m_bShiftKeyDown) {
                        pGame->UnselectAll();
                    }
                    pGame->SelectCharacter(pGame->GetCharacterId(nFirst), TRUE);
                    if (pGame->m_nCharacters > nFirst + 1) {
                        pGame->SelectCharacter(pGame->GetCharacterId(nFirst + 1), FALSE);
                    }
                    if (pGame->m_nCharacters > nFirst + 2) {
                        pGame->SelectCharacter(pGame->GetCharacterId(nFirst + 2), FALSE);
                    }
                    pGame->SelectToolbar();
                }
                break;
            }
            case 37:
                // Select everyone, or open the first's sheet when everyone
                // is selected already.
                if (pGame->m_group.GetCount() == pGame->m_nCharacters) {
                    LONG* pGroupList = pGame->m_group.GetGroupList();
                    BYTE nMember = 0;
                    while (nMember < pGame->m_group.GetCount()
                        && pGroupList[nMember] == pGame->GetCharacterId(nMember)) {
                        nMember++;
                    }

                    // NOTE: The original leaks the group list.
                    delete[] pGroupList;

                    if (nMember == pGame->m_group.GetCount()) {
                        pGame->OnPortraitLDblClick(0);
                        break;
                    }
                }
                pGame->SelectAll(TRUE);
                break;
            case 38:
            case 39:
            case 40:
                // The first, second or third shown button of types 70-78.
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    INT nSkip = index - 38;
                    for (INT nButtonType = 70; nButtonType <= 78; nButtonType++) {
                        nButtonId = pGame->GetButtonArray()->GetButtonId(nButtonType);
                        if (nButtonId != 0xFF) {
                            if (nSkip-- == 0) {
                                if (m_bShiftKeyDown) {
                                    pGame->GetButtonArray()->OnRButtonPressed(nButtonId);
                                } else {
                                    pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                                }
                                break;
                            }
                        }
                    }
                }
                break;
            case 41:
            case 42:
            case 43:
            case 44:
                // The quick spells: open the list of button 1, then press.
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    pGame->GetButtonArray()->OnRButtonPressed(1);
                    if (!m_bShiftKeyDown) {
                        pGame->GetButtonArray()->OnLButtonPressed(3 * index - 123);
                    }
                }
                break;
            case 45:
            case 46:
            case 47: {
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    INT nButtonType = index + 35;
                    nButtonId = pGame->GetButtonArray()->GetButtonId(nButtonType);
                    if (nButtonId == 0xFF) {
                        // The customize state's key.
                        pGame->GetButtonArray()->sub_594170(nButtonType);
                    } else if (m_bShiftKeyDown) {
                        pGame->GetButtonArray()->OnRButtonPressed(nButtonId);
                    } else {
                        pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                    }
                }
                break;
            }
            case 48:
            case 49:
            case 50:
            case 51:
            case 52:
                // The formations.
                if (m_cUIManager.GetPanel(1)->m_bActive) {
                    nButtonId = pGame->GetButtonArray()->GetButtonId(index - 32);
                    if (nButtonId != 0xFF) {
                        if (m_bShiftKeyDown) {
                            pGame->GetButtonArray()->OnRButtonPressed(nButtonId);
                        } else {
                            pGame->GetButtonArray()->OnLButtonPressed(nButtonId);
                        }
                    }
                }
                break;
            default:
                // TODO: Incomplete. 0x6893FD-0x689502: the keymap's spell
                // keys (53 to 371) cast the entry's spell (0x8B4EB0) through
                // the leader's CGameSprite 0x719180, not ported; the
                // original first checks the action bar's button type 3 and
                // ends the modal state.
                break;
            }
            break;
        }
        }
    }
}

// 0x5D72C0
void CScreenWorld::EngineGameUninit()
{
    m_cUIManager.fUninit();
}

// 0x49FC40
BOOL CScreenWorld::CheckMouseLButton()
{
    return TRUE;
}

// 0x68C020
void CScreenWorld::OnLButtonDblClk(CPoint pt)
{
    m_cUIManager.OnLButtonDblClk(pt);

    if ((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & 0x1) != 0) {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
        if (pArea != NULL) {
            pArea->OnActionButtonDblClk(pt);
        }

        g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    }
}

// 0x68C080
void CScreenWorld::OnLButtonDown(CPoint pt)
{
    m_cUIManager.field_20 = m_bShiftKeyDown;
    m_cUIManager.field_24 = m_bCtrlKeyDown;
    m_cUIManager.OnLButtonDown(pt);

    if ((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & 0x1) != 0) {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
        if (pArea != NULL) {
            pArea->OnActionButtonDown(pt);
        }

        g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    }
}

// 0x68C100
void CScreenWorld::OnLButtonUp(CPoint pt)
{
    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode == 0x142) {
        // NOTE: Uninline.
        m_cUIManager.KillCapture();
    } else {
        m_cUIManager.field_20 = m_bShiftKeyDown;
        m_cUIManager.field_24 = m_bCtrlKeyDown;
        m_cUIManager.OnLButtonUp(pt);

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
        if (pArea != NULL) {
            pArea->OnActionButtonUp(pt);
        }

        g_pBaldurChitin->GetObjectCursor()->m_nState = 0;
    }
}

// 0x49FC40
BOOL CScreenWorld::CheckMouseMove()
{
    return TRUE;
}

// 0x68C1A0
void CScreenWorld::OnMouseMove(CPoint pt)
{
    if (m_cUIManager.GetPanel(8)->m_bActive) {
        CUIPanel* pPanel = m_cUIManager.GetPanel(8);
        CUIControlBase* pControl = pPanel->GetControl(52);

        if (pControl != NULL && pt.x - pPanel->m_ptOrigin.x < pControl->m_ptOrigin.x + pControl->m_size.cx) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(52));
        } else {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(53));
        }
    }

    m_cUIManager.OnMouseMove(pt);

    CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
    if (pArea != NULL) {
        pArea->OnMouseMove(pt);
    }
}

// 0x49FC40
BOOL CScreenWorld::CheckMouseRButton()
{
    return TRUE;
}

// 0x68C250
void CScreenWorld::OnRButtonDblClk(CPoint pt)
{
    if ((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & 0x1) != 0) {
        g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    }
}

// 0x68C280
void CScreenWorld::OnRButtonDown(CPoint pt)
{
    m_cUIManager.OnRButtonDown(pt);

    if ((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & 0x1) != 0) {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
        if (pArea != NULL) {
            pArea->OnFormationButtonDown(pt);
        }

        g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    }
}

// 0x68C2E0
void CScreenWorld::OnRButtonUp(CPoint pt)
{
    m_cUIManager.OnRButtonUp(pt);

    if ((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & 0x1) != 0) {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
        if (pArea != NULL) {
            pArea->OnFormationButtonUp(pt);
        }

        g_pBaldurChitin->GetObjectCursor()->m_nState = 0;
    }
}

// 0x68C340
void CScreenWorld::StartScroll(CPoint dest, SHORT speed)
{
    CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
    CInfinity* pInfinity = pArea->GetInfinity();

    pInfinity->m_ptScrollDest = dest;
    pInfinity->m_nLastTickCount = GetTickCount();
    pInfinity->m_autoScrollSpeed = static_cast<SHORT>(CChitin::TIMER_UPDATES_PER_SECOND * speed / 24);
}

// 0x68C3D0
void CScreenWorld::AsynchronousUpdate(BOOL bActiveEngine)
{
    CGameSprite* pSprite;
    BYTE rc;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 2887
    UTIL_ASSERT(pGame != NULL);

    DWORD nTickCount = GetTickCount();
    g_pBaldurChitin->field_1AB2[0] += nTickCount - g_pBaldurChitin->field_1AF2[0];
    g_pBaldurChitin->field_1AF2[0] = nTickCount;

    if (m_bPlayEndCredits) {
        if (g_pBaldurChitin->GetBaldurMessage()->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_SERVER, CBaldurMessage::SIGNAL_END_GAME)) {
            g_pBaldurChitin->m_pEngineConnection->ReadyEndCredits();
            g_pBaldurChitin->m_pEngineConnection->field_FB0 = 0;
            g_pBaldurChitin->GetActiveEngine()->SelectEngine(g_pBaldurChitin->m_pEngineConnection);

            if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
                if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                    g_pBaldurChitin->m_pEngineConnection->ShowSessionTerminatedMessage();
                }

                g_pChitin->cNetwork.CloseSession(TRUE);
                g_pBaldurChitin->GetBaldurMessage()->m_bPlayerShutdown = FALSE;
            }

            pGame->DestroyGame(TRUE, FALSE);
            m_bPlayEndCredits = FALSE;
        }
        return;
    }

    if (m_playerShutdown) {
        g_pBaldurChitin->m_pEngineConnection->field_FB0 = 0;
        SelectEngine(g_pBaldurChitin->m_pEngineConnection);
        pGame->DestroyGame(TRUE, FALSE);
    }

    if (pGame->m_bGameLoaded) {
        if (pGame->GetWorldTimer()->m_active
            && field_EA4 != 0
            && m_nPartySizeCheckStartDelay <= 0
            && pGame->m_characterOverflow.GetCount() > 0) {
            if (!g_pChitin->cNetwork.GetSessionOpen()) {
                m_bPausedBeforePickParty = m_bPaused;
            }

            if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
                m_bPendingReformParty = TRUE;
            }
        }

        if (m_nPartySizeCheckStartDelay > 0) {
            m_nPartySizeCheckStartDelay -= 1;
        }

        pGame->GetWorldTimer()->UpdateTime(FALSE);

        if (g_pBaldurChitin->cNetwork.GetSessionOpen()
            && g_pBaldurChitin->cNetwork.GetSessionHosting()) {
            if ((pGame->GetWorldTimer()->m_gameTime % CTimerWorld::MULTIPLAYER_TIME_SYNCH_INTERVAL) == 0) {
                g_pBaldurChitin->GetBaldurMessage()->TimeSynchBroadcast(pGame->GetWorldTimer()->m_gameTime,
                    FALSE);
            }
        }

        if (bActiveEngine) {
            // NOTE: Uninline.
            CheckPanelInputMode(1, 0x20);

            // NOTE: Uninline.
            CheckPanelInputMode(GetPanel_22_0(), 0x8);

            // NOTE: Uninline.
            CheckPanelInputMode(6, 0x80);

            // NOTE: Uninline.
            CheckPanelInputMode(GetPanel_21_7(), 0x100);

            // NOTE: Uninline.
            CheckPanelInputMode(GetPanel_19_0(), 0x100);

            // NOTE: Uninline.
            CheckPanelInputMode(GetPanel_21_0(), 0x100);

            // NOTE: Uninline.
            CheckPanelInputMode(8, 0x200);

            // NOTE: Uninline.
            CheckPanelInputMode(17, 0x1000);

            CheckEnablePortaits(1);
            CheckEnableLeftPanel();
            m_cUIManager.TimerAsynchronousUpdate();
        }

        if (m_deltaTime != 0) {
            for (INT nArea = 0; nArea < CINFGAME_MAX_AREAS; nArea++) {
                CGameArea* pArea = pGame->m_gameAreas[nArea];
                if (pArea != NULL) {
                    pArea->CompressTime(m_deltaTime);

                    if (bActiveEngine) {
                        if (pArea == pGame->GetVisibleArea()) {
                            pArea->SortLists();
                        }
                    }
                }
            }

            m_deltaTime = 0;

            m_weather.CompressTime();
        }

        for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
            LONG nCharacterId = pGame->m_characterPortraits[nPortrait];

            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                if (pSprite->GetArea() == NULL) {
                    // NOTE: Uninline.
                    CDerivedStats* pDStats = pSprite->GetActiveStats();

                    if ((pDStats->m_generalState & STATE_DEAD) != 0) {
                        if (pSprite->DoAIUpdate(pGame->GetWorldTimer()->m_active, g_pChitin->nAUCounter)) {
                            do {
                                rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
                                    CGameObjectArray::THREAD_ASYNCH,
                                    reinterpret_cast<CGameObject**>(&pSprite),
                                    INFINITE);
                            } while (rc == CGameObjectArray::SHARED);

                            if (rc == CGameObjectArray::SUCCESS) {
                                pSprite->ProcessAI();

                                pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                                    CGameObjectArray::THREAD_ASYNCH,
                                    INFINITE);
                            }
                        }
                    }
                }

                pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        CGameAIGame* pAIGame;

        do {
            rc = pGame->GetObjectArray()->GetDeny(pGame->m_nAIIndex,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pAIGame),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pAIGame->GetVertListPos() == NULL) {
                if (pGame->GetVisibleArea() != NULL) {
                    pAIGame->AddToArea(pGame->GetVisibleArea(),
                        CPoint(0, 0),
                        0, CGameObject::LIST_FRONT);
                }
            }

            pAIGame->ProcessAI();

            pGame->GetObjectArray()->ReleaseDeny(pGame->m_nAIIndex,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        pGame->sub_5AC0D0();

        for (INT nArea = 0; nArea < CINFGAME_MAX_AREAS; nArea++) {
            CGameArea* pArea = pGame->GetArea(nArea);
            if (pArea != NULL) {
                if (pArea == pGame->GetVisibleArea()
                    && pArea->m_firstRender == 0) {
                    pArea->m_bPicked = FALSE;
                    CUIPanel* pPanel = m_cUIManager.GetPanel(1);
                    if (!m_cUIManager.m_bHidden
                        && pPanel->m_bActive
                        && pPanel->IsOver(g_pChitin->m_ptPointer)) {
                        for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
                            CUIControlBase* pPortrait = pPanel->GetControl(nPortrait);
                            if (pPortrait->IsOver(g_pChitin->m_ptPointer - pPanel->m_ptOrigin)) {
                                do {
                                    rc = pGame->GetObjectArray()->GetShare(pGame->GetCharacterId(nPortrait),
                                        CGameObjectArray::THREAD_ASYNCH,
                                        reinterpret_cast<CGameObject**>(&pSprite),
                                        INFINITE);
                                } while (rc == CGameObjectArray::SHARED);

                                if (rc == CGameObjectArray::SUCCESS) {
                                    if (pSprite->GetArea() == pArea
                                        || (pSprite->GetActiveStats()->m_generalState & STATE_DEAD) != 0) {
                                        pArea->m_iPicked = pGame->GetCharacterId(nPortrait);
                                        pArea->m_nToolTip = 0;

                                        if (pSprite->Orderable(FALSE)) {
                                            pArea->m_iPickedTarget = pSprite->GetTargetId();
                                        }

                                        pArea->m_bPicked = TRUE;
                                    }
                                    pGame->GetObjectArray()->ReleaseShare(pGame->GetCharacterId(nPortrait),
                                        CGameObjectArray::THREAD_ASYNCH,
                                        INFINITE);
                                }
                                break;
                            }
                        }
                    }
                }

                pArea->AIUpdate();

                BOOL bAreaCanBeDeleted;
                if (pArea->m_nCharacters != 0
                    || pArea == pGame->m_pGameAreaMaster
                    || pGame->m_nCharacterTerminationSequenceDelay != 0) {
                    bAreaCanBeDeleted = FALSE;
                } else {
                    INT nAreas = 0;
                    for (INT nArea = 0; nArea < CINFGAME_MAX_AREAS; nArea++) {
                        if (pGame->GetArea(nArea) != NULL) {
                            nAreas++;
                        }
                    }

                    if (nAreas > 1) {
                        // NOTE: Calls unknown empty `CGameArea` function,
                        // probably `ForceChaseCode` which was likely
                        // superceeded by INI spawner.
                        // pArea->ForceChaseCode();

                        pArea->ExitSpawn();
                        pArea->sub_47A060();

                        if (g_pChitin->cNetwork.GetSessionOpen()) {
                            bAreaCanBeDeleted = FALSE;
                        } else {
                            bAreaCanBeDeleted = TRUE;
                        }
                    } else {
                        bAreaCanBeDeleted = FALSE;
                    }
                }

                if (!bAreaCanBeDeleted) {
                    if (m_waitingOnResize > 0) {
                        EnterCriticalSection(&(pArea->field_1FC));

                        if (!EqualRect(pArea->GetInfinity()->rViewPort, m_newViewSize)
                            || m_bForceViewSize) {
                            INT x;
                            INT y;
                            pArea->GetInfinity()->GetViewPosition(x, y);
                            pArea->GetInfinity()->SetViewPort(m_newViewSize);
                            pArea->GetInfinity()->SetViewPosition(x, y, TRUE);
                        }
                        m_cUIManager.InvalidateRect(NULL);

                        LeaveCriticalSection(&(pArea->field_1FC));
                    }
                } else {
                    DeleteArea(pArea);
                }
            }
        }

        if (m_waitingOnResize > 0) {
            m_waitingOnResize--;
        }

        if (m_waitingOnResize == 0) {
            m_bForceViewSize = FALSE;
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            DeleteAreaMultiplayer();
        }

        if (bActiveEngine) {
            g_pBaldurChitin->GetObjectCursor()->CursorUpdate(pVidMode);

            if (m_bored) {
                if (pGame->GetWorldTimer()->m_active) {
                    m_boredCount++;
                }

                for (SHORT nAttempt = 0; m_boredCount > pGame->GetOptions()->m_nBoredTime && nAttempt < 2 * pGame->GetNumCharacters(); nAttempt++) {
                    SHORT nPortrait = pGame->GetNumCharacters() != 0
                        ? rand() % pGame->GetNumCharacters()
                        : 0;

                    do {
                        rc = pGame->GetObjectArray()->GetShare(pGame->GetCharacterId(nPortrait),
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        if (pSprite->Orderable(FALSE)) {
                            CMessagePlaySound* pPlaySound = new CMessagePlaySound(4,
                                TRUE,
                                TRUE,
                                pSprite->GetId(),
                                pSprite->GetId());
                            g_pBaldurChitin->GetMessageHandler()->AddMessage(pPlaySound, FALSE);
                            m_boredCount = pGame->GetOptions()->m_nBoredTime
                                - pGame->GetOptions()->m_nBoredTime / 4;
                        }

                        pGame->GetObjectArray()->ReleaseShare(pGame->GetCharacterId(nPortrait),
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    } else {
                        break;
                    }
                }
            } else {
                m_boredCount = 0;
                m_bored = TRUE;
            }
        }

        if (m_scrollLockId != CGameObjectArray::INVALID_INDEX) {
            CGameObject* pObject;

            // FIXME: Result is ignored.
            pGame->GetObjectArray()->GetShare(m_scrollLockId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);

            CPoint ptPos = pObject->GetPos();
            CRect rViewPort = pObject->GetArea()->GetInfinity()->rViewPort;

            CGameArea* pVisibleArea = pGame->GetVisibleArea();
            pVisibleArea->GetInfinity()->m_ptScrollDest.x = ptPos.x - rViewPort.Width() / 2;
            pVisibleArea->GetInfinity()->m_ptScrollDest.y = ptPos.y - rViewPort.Height() / 2;
            pVisibleArea->GetInfinity()->m_nLastTickCount = GetTickCount();
            pVisibleArea->GetInfinity()->m_autoScrollSpeed = 0;

            pGame->GetObjectArray()->ReleaseShare(m_scrollLockId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        if (pGame->GetWorldTimer()->m_active) {
            pGame->m_cLimboList.AIUpdate();
        }

        m_internalLoadedDialog.AsynchronousUpdate();

        if (m_pActiveDialogDisplay != NULL) {
            CUIControlTextDisplay* pText;
            switch (m_pActiveDialogDisplay->m_pPanel->m_nID) {
            case 0:
            case 4:
            case 7:
            case 22:
                pText = m_pActiveDialogDisplay;
                break;
            case 19:
                pText = static_cast<CUIControlTextDisplay*>(m_pActiveDialogDisplay->m_pPanel->GetControl(3));

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 3288
                UTIL_ASSERT(pText == m_pActiveChatDisplay);

                break;
            case 21:
                pText = static_cast<CUIControlTextDisplay*>(m_pActiveDialogDisplay->m_pPanel->GetControl(3));

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 3293
                UTIL_ASSERT(pText == m_pActiveChatDisplay);

                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 3297
                UTIL_ASSERT(FALSE);
            }

            if (pText != NULL) {
                m_nChatMessageCount = g_pBaldurChitin->GetBaldurMessage()->m_cChatBuffer.UpdateTextDisplay(pText,
                    m_nChatMessageCount);
            }
        }

        if (pGame->GetGameSave()->m_cutScene || !pGame->GetWorldTimer()->m_active) {
            m_interactionIndex = CGameObjectArray::INVALID_INDEX;
            m_interactionTarget = CGameObjectArray::INVALID_INDEX;
        } else {
            HandleAmbiance();
            HandleDeathReaction();
        }

        if (pGame->m_nCharacterTerminationSequenceDelay > 0) {
            if (--pGame->m_nCharacterTerminationSequenceDelay <= 0) {
                CInfGame::StartCharacterTerminationSequence();
            }
        }

        if (m_autoPauseId != CGameObjectArray::INVALID_INDEX) {
            if (!pGame->GetGameSave()->m_cutScene) {
                if (m_autoPauseRef != -1) {
                    g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(m_autoPauseName,
                        m_autoPauseRef,
                        m_autoPauseColor,
                        RGB(215, 215, 190),
                        -1,
                        m_autoPauseId,
                        m_autoPauseId);
                }

                if (!g_pBaldurChitin->m_pEngineWorld->m_bPaused) {
                    if (!g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0)) {
                        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(m_autoPauseName,
                            17122,
                            m_autoPauseColor,
                            RGB(215, 215, 190),
                            -1,
                            m_autoPauseId,
                            m_autoPauseId);
                    }
                }

                if (pGame->GetOptions()->m_bAutoPauseCenter) {
                    SHORT nPortrait = pGame->GetCharacterPortraitNum(m_autoPauseId);
                    if (nPortrait != -1) {
                        pGame->OnPortraitLDblClick(nPortrait);
                    }
                }
            }
            m_autoPauseId = CGameObjectArray::INVALID_INDEX;
        }

        if (field_EA4 == 17) {
            CUIPanel* pPanel = m_cUIManager.GetPanel(17);

            if (m_bHardPaused) {
                static_cast<CUIControlButton*>(pPanel->GetControl(1))->SetEnabled(FALSE);
                static_cast<CUIControlButton*>(pPanel->GetControl(2))->SetEnabled(FALSE);
            } else {
                static_cast<CUIControlButton*>(pPanel->GetControl(1))->SetEnabled(TRUE);
                static_cast<CUIControlButton*>(pPanel->GetControl(2))->SetEnabled(TRUE);
            }

            if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
                && !g_pChitin->cNetwork.GetSessionHosting()) {
                static_cast<CUIControlButton*>(pPanel->GetControl(1))->SetEnabled(FALSE);
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bLeaveAreaLuaPanicPending == TRUE
            && pGame->GetWorldTimer()->m_gameTime >= m_ulLeaveAreaLuaPanicTimer) {
            m_bLeaveAreaLuaPanicPending = FALSE;
            m_ulLeaveAreaLuaPanicTimer = -1;
            LeaveAreaLuaPanic(m_ptLeaveAreaLuaPanicLocation,
                m_nLeaveAreaLuaPanicDirection,
                m_sLeaveAreaLuaPanicAreaName,
                m_sLeaveAreaLuaPanicParchment);
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bChapterTransitionPending == TRUE) {
            BOOLEAN bPermitted = TRUE;
            if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
                bPermitted = FALSE;
            }
            if (pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
                bPermitted = FALSE;
            }

            if (m_bHardPaused != TRUE && bPermitted) {
                m_bChapterTransitionPending = FALSE;

                if (pGame->GetCurrentChapter() < m_nChapterTransition) {
                    g_pBaldurChitin->m_pEngineChapter->StartChapterMultiplayerHost(m_nChapterTransition,
                        m_szChapterTransitionResRef);
                    m_bChapterTransitionPending = FALSE;
                    m_nChapterTransition = 255;
                }
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bMoviePending == TRUE) {
            BOOLEAN bPermitted = TRUE;
            if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
                bPermitted = FALSE;
            }
            if (pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
                bPermitted = FALSE;
            }

            if (m_bHardPaused != TRUE && bPermitted == TRUE) {
                StartMovieMultiplayerHost(m_szMovieResRef);
                m_bMoviePending = FALSE;
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bPendingMapWorld == TRUE) {
            BOOLEAN bPermitted = TRUE;
            if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
                bPermitted = FALSE;
            }
            if (pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
                bPermitted = FALSE;
            }

            if (m_bHardPaused != TRUE && bPermitted == TRUE) {
                // NOTE: Uninline.
                StartMapWorldMultiplayerHost(m_idPendingMapWorldController,
                    m_nPendingMapWorldDirection);

                m_bPendingMapWorld = FALSE;
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bRestPending == TRUE) {
            BOOLEAN bPermitted = TRUE;
            if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
                bPermitted = FALSE;
            }
            if (pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
                bPermitted = FALSE;
            }

            if (m_bHardPaused != TRUE && bPermitted == TRUE) {
                StartRestMultiplayerHost(m_nRestHP,
                    m_bRestRenting,
                    m_bRestMovie);
                m_bRestPending = FALSE;
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bPendingReformParty == TRUE) {
            BOOLEAN bPermitted = TRUE;
            if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
                bPermitted = FALSE;
            }
            if (pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
                bPermitted = FALSE;
            }

            if (m_bHardPaused != TRUE && bPermitted == TRUE) {
                // NOTE: Uninline.
                StartReformPartyMultiplayerHost();

                m_bPendingReformParty = FALSE;
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
            if (pGame->GetGameSave()->m_mode != 386 && pGame->GetGameSave()->m_mode != 1282) {
                if (++pGame->GetMultiplayerSettings()->m_nHostPermittedDialogDelay >= 30) {
                    g_pBaldurChitin->GetBaldurMessage()->DialogRequestKillOrUse();
                    pGame->GetMultiplayerSettings()->m_nHostPermittedDialogDelay = 0;
                }
            } else {
                pGame->GetMultiplayerSettings()->m_nHostPermittedDialogDelay = 0;
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && m_bHardPaused) {
            // NOTE: Uninline.
            CheckEndOfHardPause();
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && g_pBaldurChitin->GetBaldurMessage()->m_bMultiplayerSynchServerPending == TRUE
            && g_pBaldurChitin->GetBaldurMessage()->m_bMultiplayerSynchServerFinished == FALSE) {
            // NOTE: Uninline.
            CheckEndOfMultiplayerSynch();
        }

        m_weather.Update();

        if (m_bGameOverPanel && m_movie == "") {
            m_bGameOverPanel = FALSE;
            StartDeath();
            field_10F0 = 0;
        }

        if (m_nBattleCryTimeOut > 0) {
            m_nBattleCryTimeOut--;
        }

        DWORD nOtherTickCount = GetTickCount();
        g_pBaldurChitin->field_1A72[0] += nOtherTickCount - g_pBaldurChitin->field_1AF2[0];
        g_pBaldurChitin->field_1AF2[0] = nOtherTickCount;

        if (m_comingOutOfDialog > 0) {
            if (--m_comingOutOfDialog == 0) {
                if (pGame->GetGameSave()->m_mode != 322) {
                    pGame->GetGameSave()->m_cutScene = 0;
                }
            }
        }

        BOOL fullyFaded = g_pChitin->GetCurrentVideoMode()->GetFullyFaded();

        if (fullyFaded) {
            if (m_nBlackOutCountDown == 0) {
                m_nBlackOutCountDown = 150;
            }
        } else {
            m_nBlackOutCountDown = 0;
        }

        if (m_nBlackOutCountDown > 0) {
            if (pGame->GetWorldTimer()->m_active) {
                if (--m_nBlackOutCountDown == 0) {
                    if (fullyFaded) {
                        // NOTE: Uninline.
                        g_pChitin->GetCurrentVideoMode()->SetFade(FALSE, 1);

                        for (int i = 0; i < 20; i++) {
                            // NOTE: Uninline.
                            g_pChitin->GetCurrentVideoMode()->UpdateFade();

                            g_pChitin->m_bDisplayStale = TRUE;
                            SleepEx(60, FALSE);
                        }
                    }
                }
            }
        }
    }
}

// 0x68DEC0
void CScreenWorld::TimerSynchronousUpdate()
{
    // 0x8B5480
    static BYTE byte_8B5480[3] = {
        7,
        0,
        22,
    };

    // 0x8B5484
    static BYTE byte_8B5484[3] = {
        2,
        2,
        0,
    };

    for (int index = 0; index < 3; index++) {
        CUIPanel* pPanel = m_cUIManager.GetPanel(byte_8B5480[index]);
        if (pPanel->m_bActive) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(byte_8B5484[index]));
        }
    }

    if (field_119D) {
        pVidMode->LoadFogOWarSurfaces(CInfGame::FOGOWAR_RESREF);
        field_119D = FALSE;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->SynchronousUpdate();

    CGameArea* pArea = pGame->GetVisibleArea();
    if (pArea == NULL || pArea->m_firstRender < 2) {
        if (field_EA4 == 6) {
            SHORT nPortrait;

            for (nPortrait = 0; nPortrait <= 5; nPortrait++) {
                pGame->sub_5AF420(nPortrait, 1);
            }

            for (nPortrait = 50; nPortrait <= 55; nPortrait++) {
                pGame->sub_5AF420(nPortrait, 1);
            }
        }

        m_cUIManager.Render();

        CGameArea* pArea = pGame->GetVisibleArea();
        if (pArea != NULL) {
            pVidMode->EnableScissoring();
            pArea->Render(pVidMode, 0);
            pVidMode->DisableScissoring();
        }

        pVidMode->Flip(TRUE);
    }
}

// 0x68DFD0
BOOL CScreenWorld::TogglePauseGame(char bVisualPause, char bSendMessage, int idPlayerPausing)
{
    STR_RES strRes;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !g_pChitin->cNetwork.GetSessionHosting()
        && bSendMessage == TRUE) {
        if (!pGame->m_singlePlayerPermissions.GetSinglePermission(CGamePermission::PAUSING)) {
            STR_RES strNoPermission;
            g_pBaldurChitin->GetTlkTable().Fetch(20701, strNoPermission);
            DisplayText(CString(""),
                strNoPermission.szText,
                RGB(255, 0, 0),
                RGB(255, 0, 0),
                -1,
                FALSE);
            return FALSE;
        }

        g_pBaldurChitin->GetBaldurMessage()->SendPauseRequestToServer(bVisualPause, m_bPaused == FALSE);
        return TRUE;
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        BOOLEAN bPermitted = TRUE;
        if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
            bPermitted = FALSE;
        }
        if (pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
            bPermitted = FALSE;
        }
        if (m_bPaused == TRUE && m_bHardPaused == TRUE) {
            bPermitted = FALSE;
        }

        if ((bVisualPause == TRUE && m_comingOutOfDialog > 0) || !bPermitted) {
            return TRUE;
        }
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && bVisualPause == TRUE) {
        if (m_bPaused == TRUE) {
            if (field_EA4 != 17 && field_EA4 != GetPanel_21_7()) {
                if (field_11B6 < GetTickCount() && GetTickCount() - field_11B6 < 1500) {
                    return TRUE;
                }
            }
        } else {
            if (GetTickCount() > field_11BA && GetTickCount() - field_11BA < 1000) {
                return TRUE;
            }
        }
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
        && bSendMessage == TRUE) {
        if (idPlayerPausing == 0) {
            g_pBaldurChitin->GetBaldurMessage()->PauseAnnounceStatus(static_cast<BOOLEAN>(m_bHardPaused),
                m_bPaused == FALSE,
                g_pChitin->cNetwork.m_idLocalPlayer);
        } else {
            g_pBaldurChitin->GetBaldurMessage()->PauseAnnounceStatus(static_cast<BOOLEAN>(m_bHardPaused),
                m_bPaused == FALSE,
                idPlayerPausing);
        }
    }

    CString sName("");

    if (!m_bPaused) {
        pGame->GetWorldTimer()->StopTime();
        m_bPaused = TRUE;
        if (bVisualPause) {
            field_14A = TRUE;
        }
        pGame->m_bForceDither = TRUE;
        g_pBaldurChitin->GetTlkTable().Fetch(16321, strRes); // "Paused"

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            field_11B6 = GetTickCount();
        }
    } else {
        if (field_EA4 != 17 && field_EA4 != GetPanel_21_7()) {
            switch (field_EA4) {
            case 0:
            case 7:
            case 19:
            case 21:
            case 22:
            case -1:
                break;
            case 6:
                StopCommand();
                field_EA4 = -1;
                break;
            case 8:
                StopContainer();
                field_EA4 = -1;
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 5469
                UTIL_ASSERT(FALSE);
            }

            pGame->GetWorldTimer()->StartTime();
            m_bPaused = FALSE;
            if (!field_F44) {
                pGame->m_bForceDither = FALSE;
            }
            field_14A = FALSE;
            g_pBaldurChitin->GetTlkTable().Fetch(16322, strRes); // "Unpaused"
        }
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        if (g_pChitin->cNetwork.GetSessionHosting() == TRUE && idPlayerPausing == 0) {
            sName = g_pChitin->cNetwork.m_sLocalPlayerName;
        } else {
            INT nPlayerNumber = g_pChitin->cNetwork.FindPlayerLocationByID(idPlayerPausing, FALSE);
            if (nPlayerNumber != -1) {
                g_pChitin->cNetwork.GetPlayerName(nPlayerNumber, sName);
            }
        }
    }

    g_pBaldurChitin->m_pEngineWorld->DisplayText(sName,
        strRes.szText,
        RGB(255, 0, 0),
        RGB(255, 0, 0),
        -1,
        FALSE);

    return TRUE;
}

// 0x68E630
void CScreenWorld::UpdateContainerStatus(LONG nContainerId, SHORT nSlotNum)
{
    SHORT nIndex = nSlotNum - 5 * m_nTopContainerRow;
    CUIPanel* pPanel = m_cUIManager.GetPanel(8);
    if (pPanel->m_bActive) {
        if (nContainerId == g_pBaldurChitin->GetObjectGame()->m_iContainer) {
            if (nSlotNum == -1) {
                // NOTE: Signed compare.
                for (INT nButtonID = 0; nButtonID <= 9; nButtonID++) {
                    pPanel->GetControl(nButtonID)->InvalidateRect();
                }
            } else if (nSlotNum >= 0 && nSlotNum <= 9) {
                CUIControlBase* pControl = pPanel->GetControl(nSlotNum);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 4044
                UTIL_ASSERT(pControl != NULL);

                pControl->InvalidateRect();
            }
        }
    }
}

// 0x68E6E0
BOOLEAN CScreenWorld::ReadyMovie(const CResRef& res, BOOLEAN bForcedFromServer)
{
    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !g_pChitin->cNetwork.GetSessionHosting()
        && !bForcedFromServer) {
        if (res != CInfinity::DAWN_MOVIE
            && res != CInfinity::DUSK_MOVIE
            && res != "DEATH1") {
            g_pBaldurChitin->GetBaldurMessage()->SendMovieRequestToServer(res);
        }
        return FALSE;
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        CString cResString;
        res.CopyToString(cResString);

        BYTE movieResRef[RESREF_SIZE];
        memcpy(movieResRef, cResString.GetBuffer(RESREF_SIZE), RESREF_SIZE);

        g_pBaldurChitin->m_pEngineWorld->StartMovieMultiplayerHost(movieResRef);
        return FALSE;
    }

    m_movie = res;
    if (m_movie == CInfinity::DAWN_MOVIE
        || m_movie == CInfinity::DUSK_MOVIE
        || m_movie == "DEATH1") {
        g_pBaldurChitin->m_pEngineProjector->field_145 = 1;
    }

    if (bForcedFromServer == TRUE) {
        if (m_movie == CInfinity::DAWN_MOVIE) {
            g_pBaldurChitin->m_pEngineWorld->m_bSetDayOnActivate = TRUE;
        } else if (m_movie == CInfinity::DUSK_MOVIE) {
            g_pBaldurChitin->m_pEngineWorld->m_bSetNightOnActivate = TRUE;
        }

        if (m_movie == "DEATH1") {
            g_pBaldurChitin->GetActiveEngine()->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
            g_pBaldurChitin->m_pEngineWorld->m_bGameOverPanel = TRUE;
        }
    }

    g_pBaldurChitin->GetObjectCursor()->m_bVisible = FALSE;
    g_pBaldurChitin->m_pEngineProjector->PlayMovie(m_movie);
    m_movie = "";

    return TRUE;
}

// 0x68E950
BOOLEAN CScreenWorld::ReadyEndCredits(BOOLEAN bForcedFromServer)
{
    // NOTE: Unused.
    CString v1;

    if (!g_pChitin->cNetwork.GetSessionOpen()
        || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        g_pBaldurChitin->GetBaldurMessage()->RequestClientSignal(CBaldurMessage::SIGNAL_END_GAME);
        m_bPlayEndCredits = TRUE;
        return TRUE;
    } else {
        return FALSE;
    }
}

// 0x68EA00
BOOL CScreenWorld::StartDialog(CGameSprite* pCharacter, CGameSprite* pTarget, BOOLEAN bPlayerInitiated, BOOLEAN bInteraction)
{
    POSITION posTopString;
    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        posTopString = m_pActiveDialogDisplay->m_plstStrings->FindIndex(m_pActiveDialogDisplay->field_5A);
    } else {
        posTopString = NULL;
    }

    // NOTE: Unused.
    STR_RES strRes;

    SHORT nPortrait;
    for (nPortrait = 0; nPortrait < g_pBaldurChitin->GetObjectGame()->GetNumCharacters(); nPortrait++) {
        LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait);
        CMessage101* pMessage = new CMessage101(FALSE, nCharacterId, nCharacterId, TRUE);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    INT x;
    INT y;
    g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetInfinity()->GetViewPosition(x, y);
    CPoint ptViewPosition(x, y);

    SetDialogTokens(pCharacter);

    field_1150 = 0;
    g_pBaldurChitin->GetBaldurMessage()->m_bDialogRequestPending = FALSE;
    g_pBaldurChitin->GetBaldurMessage()->m_bDialogReplyReturned = FALSE;
    g_pBaldurChitin->GetBaldurMessage()->m_bDialogReplyValue = FALSE;
    g_pBaldurChitin->GetBaldurMessage()->m_nDialogReplyUpdates = 0;

    if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_nHostPermittedDialogDelay = 0;
        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_bHostPermittedDialog = TRUE;
        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_idHostPermittedDialog = pCharacter->m_remotePlayerID;
    }

    if (!bInteraction
        && bPlayerInitiated
        && pTarget->GetAIType().m_nEnemyAlly >= CAIObjectType::EA_EVILCUTOFF) {
        if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_nHostPermittedDialogDelay = 0;
            g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_bHostPermittedDialog = FALSE;
            g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_idHostPermittedDialog = 0;
        }

        g_pBaldurChitin->GetBaldurMessage()->CancelDialogRequestToServer();
        return FALSE;
    }

    CUIPanel* pDialogPanel = GetManager()->GetPanel(GetPanel_21_7());

    switch (field_EA4) {
    case 0:
    case 7:
    case 17:
    case 19:
    case 21:
    case 22:
    case -1:
        break;
    case 6:
        StopCommand();
        field_EA4 = -1;
        break;
    case 8:
        StopContainer();
        field_EA4 = -1;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 5469
        UTIL_ASSERT(FALSE);
    }

    field_EA4 = pDialogPanel->m_nID;
    m_cUIManager.ClearTooltip();

    for (DWORD nPortrait = 0; nPortrait < 6; nPortrait++) {
        LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(static_cast<SHORT>(nPortrait));
        if (nCharacterId != CGameObjectArray::INVALID_INDEX) {
            CGameSprite* pSprite = NULL;

            BYTE rc;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pSprite != NULL) {
                    CMessageDropPath* pDropPath = new CMessageDropPath(pSprite->GetId(), pSprite->GetId());
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(pDropPath, FALSE);

                    pSprite->DropSearchRequest();
                    pSprite->ClearActions(TRUE);
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }

    CMessageEnterDialogMode* pEnterDialogMode = new CMessageEnterDialogMode(g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_nPartyGold,
        pCharacter->GetId(),
        pCharacter->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pEnterDialogMode, FALSE);

    if (g_pChitin->cNetwork.GetServiceProvider() == 0
        || g_pChitin->cNetwork.m_idLocalPlayer == pCharacter->m_remotePlayerID) {
        m_bInControlOfDialog = TRUE;
    } else {
        m_bInControlOfDialog = FALSE;
    }

    g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
    g_pBaldurChitin->GetObjectGame()->m_tempCursor = 4;
    pCharacter->GetArea()->m_nScrollState = 0;

    m_bPaused = TRUE;
    g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->StopTime();

    CUIPanel* pActionPanel = m_cUIManager.GetPanel(1);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10166
    UTIL_ASSERT(pActionPanel != NULL);

    CUIPanel* pCommandPanel = m_cUIManager.GetPanel(GetPanel_22_0());

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10168
    UTIL_ASSERT(pCommandPanel != NULL);

    pActionPanel->SetEnabled(TRUE);
    pCommandPanel->SetEnabled(TRUE);

    sub_695570(FALSE, FALSE);

    GetManager()->GetPanel(9)->SetActive(TRUE);
    GetManager()->GetPanel(9)->GetControl(0)->SetActive(FALSE);
    GetManager()->GetPanel(9)->InvalidateRect(NULL);

    if (GetManager()->GetPanel(GetPanel_21_7())->m_bActive == TRUE) {
        field_114C = GetPanel_21_7();
    } else if (GetManager()->GetPanel(GetPanel_22_0())->m_bActive == TRUE) {
        field_114C = GetPanel_22_0();
    } else {
        field_114C = GetPanel_21_7();
    }

    GetManager()->GetPanel(GetPanel_21_7())->SetActive(FALSE);
    GetManager()->GetPanel(GetPanel_19_0())->SetActive(FALSE);
    GetManager()->GetPanel(GetPanel_21_0())->SetActive(FALSE);
    GetManager()->GetPanel(GetPanel_22_0())->SetActive(FALSE);

    pDialogPanel->SetActive(TRUE);

    if (!g_pChitin->cNetwork.GetSessionOpen()
        || g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
        pDialogPanel->GetControl(0)->SetActive(FALSE);
    }

    CUIControlTextDisplay* pOldDialogDisplay = m_pActiveDialogDisplay;
    if (g_pChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        m_pActiveDialogDisplay = static_cast<CUIControlTextDisplay*>(pDialogPanel->GetControl(1));
        static_cast<CUIControlTextDisplay*>(pDialogPanel->GetControl(1))->CopyDisplay(pOldDialogDisplay);
        m_pActiveChatDisplay = static_cast<CUIControlTextDisplay*>(pDialogPanel->GetControl(3));
    } else {
        m_pActiveDialogDisplay = static_cast<CUIControlTextDisplay*>(pDialogPanel->GetControl(1));
        static_cast<CUIControlTextDisplay*>(pDialogPanel->GetControl(1))->CopyDisplay(pOldDialogDisplay);
    }

    CopyChatEditBox(pOldDialogDisplay->m_pPanel, pDialogPanel);

    // NOTE: Uninline.
    g_pBaldurChitin->m_pEngineWorld->SetNewViewSize(CInfinity::stru_8E7958, FALSE);

    POSITION posFirstLine = DisplayText(CString(""), CString(""), RGB(0, 0, 0), RGB(0, 0, 0), -1, FALSE);
    POSITION posSecondLine = DisplayText(CString(""), CString(""), RGB(0, 0, 0), RGB(0, 0, 0), -1, TRUE);

    CString sDialog;
    if (bInteraction) {
        pTarget->field_56E4.CopyToString(sDialog);
    } else {
        pTarget->m_dialog.CopyToString(sDialog);
    }

    CMessageLoadDialog* pLoadDialog = new CMessageLoadDialog(sDialog,
        pTarget->GetId(),
        pCharacter->GetId(),
        pCharacter->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pLoadDialog, FALSE);

    m_internalLoadedDialog.Load(CResRef(sDialog), pCharacter->GetId(), pTarget->GetId());

    if (pTarget->m_nDirection != pTarget->GetDirection(pCharacter->GetPos())
        && (pTarget->GetDerivedStats()->m_generalState & STATE_SLEEPING) == 0
        && pTarget->m_nSequence != CGameSprite::SEQ_SLEEP) {
        CMessageSetDirection* pMessage = new CMessageSetDirection(pCharacter->GetPos(),
            pTarget->GetId(),
            pTarget->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (pCharacter->m_nDirection != pCharacter->GetDirection(pTarget->GetPos())
        && (pCharacter->GetDerivedStats()->m_generalState & STATE_SLEEPING) == 0
        && pCharacter->m_nSequence != CGameSprite::SEQ_SLEEP) {
        CMessageSetDirection* pMessage = new CMessageSetDirection(pTarget->GetPos(),
            pCharacter->GetId(),
            pCharacter->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    field_F12 = ptViewPosition;

    CRect rViewPort;
    rViewPort.CopyRect(g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetInfinity()->rViewPort);
    field_F12.x -= rViewPort.left;
    field_F12.y -= rViewPort.top;

    field_F1A = g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->m_resRef;

    g_pBaldurChitin->GetObjectGame()->m_tempCursor = 0;

    if (m_bInControlOfDialog) {
        g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode = 0x502;
    } else {
        g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode = 0x182;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetVisibleArea() != NULL) {
        g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->ClearInput();
    }

    if (m_internalLoadedDialog.StartDialog(pTarget)) {
        if (g_pBaldurChitin->pActiveEngine != g_pBaldurChitin->m_pEngineWorld) {
            g_pBaldurChitin->pActiveEngine->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
        }

        UpdatePartyGoldStatus();

        if (field_EA4 == 7 || field_EA4 == 21 || field_EA4 == 19) {
            CResRef portraitResRef;

            CUIPanel* pPanel = m_cUIManager.GetPanel(field_EA4);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
            // __LINE__: 4508
            UTIL_ASSERT(pPanel != NULL);

            CUIControlButtonSinglePlayerPortrait* pPortrait = static_cast<CUIControlButtonSinglePlayerPortrait*>(pPanel->GetControl(11));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
            // __LINE__: 4511
            UTIL_ASSERT(pPortrait != NULL);

            pPortrait->SetEnabled(FALSE);
            portraitResRef = pCharacter->GetBaseStats()->m_portraitSmall;
            pPortrait->SetPortrait(portraitResRef);
        }

        return TRUE;
    }

    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        g_pBaldurChitin->m_pEngineWorld->m_pActiveDialogDisplay->RemoveString(posFirstLine);
    }

    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        g_pBaldurChitin->m_pEngineWorld->m_pActiveDialogDisplay->RemoveString(posSecondLine);
    }

    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        m_pActiveDialogDisplay->SetTopString(posTopString);
    }

    EndDialog(FALSE, FALSE);

    return FALSE;
}

// 0x68F9D0
void CScreenWorld::EndDialog(BOOLEAN bForceExecution, BOOLEAN fullEnd)
{
    CString sChatText;
    LONG nChatLines;
    LONG nMaxChatLines;

    m_nPartySizeCheckStartDelay = 6;

    if (!field_1204) {
        m_comingOutOfDialog = 30;
    }

    if (g_pChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        nMaxChatLines = 11;
        nChatLines = 6;
    } else {
        nMaxChatLines = 12;
        nChatLines = 7;
    }

    if (!m_bInControlOfDialog && !bForceExecution) {
        return;
    }

    if (m_bInControlOfDialog) {
        CMessageExitDialogMode* pMessage = new CMessageExitDialogMode(field_1150,
            CGameObjectArray::INVALID_INDEX,
            CGameObjectArray::INVALID_INDEX);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    g_pBaldurChitin->GetObjectGame()->m_tempCursor = 4;
    g_pBaldurChitin->GetTlkTable().field_58 = byte_8FB954;

    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->SetActive(FALSE);
    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->SetInactiveRender(FALSE);

    sub_695570(TRUE, TRUE);

    // The last non-empty string of the dialog display, so the world screen's
    // display can show it at the top.
    INT nIndex = m_pActiveDialogDisplay->m_plstStrings->GetCount();
    BOOLEAN bFound = FALSE;
    POSITION posTopString;
    while (nIndex > 0) {
        if (bFound) {
            break;
        }

        CString sString;
        nIndex--;
        m_pActiveDialogDisplay->GetString(nIndex, sString);
        if (sString.GetLength() >= 1) {
            bFound = TRUE;
        }
    }

    if (bFound == TRUE) {
        posTopString = m_pActiveDialogDisplay->GetBossPosition(nIndex);

        // NOTE: The result is unused.
        m_pActiveDialogDisplay->GetNumLines(posTopString);
    } else {
        posTopString = NULL;
    }

    LONG nLines;
    if (field_114C == GetPanel_21_7()) {
        nLines = nMaxChatLines;
    } else if (field_114C == GetPanel_22_0()) {
        nLines = nChatLines;
    } else {
        nLines = 2;
    }

    BOOL bInputCapture;
    GetChatEditBoxStatus(sChatText, bInputCapture);

    // NOTE: The original compares the chat text with "" here and ignores the
    // result.
    if (nLines < nChatLines && !bInputCapture) {
        sChatText.Compare("");
    }

    CUIPanel* pPanel = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(GetPanel_22_0());

    if (g_pChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(GetPanel_19_0())->SetActive(FALSE);
        g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(GetPanel_21_0())->SetActive(FALSE);
    } else {
        g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(GetPanel_21_7())->SetActive(FALSE);
    }

    CUIControlTextDisplay* pOldDialogDisplay = m_pActiveDialogDisplay;
    g_pBaldurChitin->m_pEngineWorld->m_pActiveDialogDisplay = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(1));
    static_cast<CUIControlTextDisplay*>(pPanel->GetControl(1))->CopyDisplay(pOldDialogDisplay);

    // NOTE: Uninline.
    g_pBaldurChitin->m_pEngineWorld->SetNewViewSize(CInfinity::stru_8E79B8, FALSE);

    CRect rViewPort(CInfinity::stru_8E79B8);

    CopyChatEditBox(pOldDialogDisplay->m_pPanel, pPanel);

    g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->StartTime();
    m_bPaused = FALSE;

    if (m_cUIManager.m_bHidden) {
        rViewPort = CInfinity::stru_8E7548;
    }

    if (m_pActiveChatDisplay != NULL) {
        CString sLabel;
        CString sString;
        COLORREF rgbTextColor;

        for (INT nItem = 0; nItem < m_pActiveChatDisplay->field_A6C; nItem++) {
            if (!m_pActiveChatDisplay->GetItem(nItem, sLabel, sString)) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 4773
                UTIL_ASSERT(FALSE);
            }

            if (!m_pActiveChatDisplay->GetItemTextColor(nItem, rgbTextColor)) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
                // __LINE__: 4775
                UTIL_ASSERT(FALSE);
            }

            m_pActiveDialogDisplay->DisplayString(sLabel,
                sString,
                rgbTextColor,
                rgbTextColor,
                -1,
                FALSE,
                TRUE);
        }

        m_pActiveChatDisplay->RemoveAll();

        if (m_pActiveChatDisplay->m_pPanel->m_nID != GetPanel_21_0()) {
            static_cast<CUIControlTextDisplay*>(m_cUIManager.GetPanel(GetPanel_21_0())->GetControl(3))->CopyDisplay(m_pActiveChatDisplay);
        }

        m_pActiveChatDisplay = NULL;
    }

    if (fullEnd && posTopString != NULL) {
        m_pActiveDialogDisplay->SetTopString(posTopString);
    }

    if (field_11F4) {
        pPanel->SetActive(TRUE);
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (field_1204 == TRUE) {
        pGame->GetGameSave()->m_mode = 0x142;
        pGame->GetGameSave()->m_cutScene = TRUE;
        g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
        pGame->m_tempCursor = 4;
        pGame->GetVisibleArea()->m_nScrollState = 0;
        field_1204 = FALSE;
    } else {
        pGame->GetGameSave()->m_mode = -1;
    }

    pGame->GetVisibleArea()->OnMouseMove(g_pChitin->m_ptPointer);

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        field_11BA = GetTickCount();
        pGame->GetMultiplayerSettings()->m_nHostPermittedDialogDelay = 0;
        pGame->GetMultiplayerSettings()->m_bHostPermittedDialog = FALSE;
        pGame->GetMultiplayerSettings()->m_idHostPermittedDialog = 0;
    }

    CString sAreaName;
    field_F1A.CopyToString(sAreaName);
    if (g_pBaldurChitin->GetObjectGame()->GetArea(sAreaName) != NULL) {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sAreaName);
        if (pArea != pGame->GetVisibleArea()) {
            pGame->GetVisibleArea()->m_bPicked = FALSE;
            pGame->GetVisibleArea()->m_iPicked = CGameObjectArray::INVALID_INDEX;
            pGame->GetVisibleArea()->m_nToolTip = 0;
            pGame->GetVisibleArea()->OnDeactivation();
            pGame->m_visibleArea = g_pBaldurChitin->GetObjectGame()->GetArea(sAreaName)->m_id;
            pGame->GetVisibleArea()->OnActivation();
        }

        INT x;
        INT y;
        pGame->GetVisibleArea()->GetInfinity()->GetViewPosition(x, y);

        field_F12.y += rViewPort.top;
        field_F12.x += rViewPort.left;

        LONG dy = y - field_F12.y;
        LONG dx = x - field_F12.x;

        // iwd2r: the squared range (700 * 700 at 0x6902D4) takes
        // iwd2r::CurrentScreenLayout().dialogAutoScrollRange, as IEex
        // patches it to scale with the resolution.
        if (dx * dx + dy * dy > iwd2r::CurrentScreenLayout().dialogAutoScrollRange) {
            // NOTE: Uninline.
            StartScroll(field_F12, 0);
        } else {
            // NOTE: Uninline.
            StartScroll(field_F12, 16);
        }
    }

    m_internalLoadedDialog.EndDialog();
    field_EA4 = -1;
}

// FIXME: `areaName` should be reference.
//
// 0x690410
void CScreenWorld::MoveToArea(CString areaName)
{
    // NOTE: Uninline.
    CAIAction leave(CAIAction::LEAVEAREALUA, areaName, CPoint(0, 0), 0);
    leave.m_specificID2 = 0;

    g_pBaldurChitin->GetObjectGame()->GetGroup()->GroupAction(leave, TRUE, NULL);
}

// 0x690640
void CScreenWorld::SaveScreen()
{
    INT x;
    INT y;
    CRect rViewPort;

    CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
    pArea->GetInfinity()->GetViewPosition(x, y);
    rViewPort.CopyRect(pArea->GetInfinity()->rViewPort);

    pArea->GetInfinity()->SetViewPort(CInfinity::stru_8E79A8);

    // NOTE: Some inlining, not sure which.
    if (pArea != NULL) {
        pArea->Render(pVidMode, 0);
    }

    pArea->GetInfinity()->SetViewPort(rViewPort);
    pArea->GetInfinity()->SetViewPosition(x, y, TRUE);
}

// 0x6906F0
void CScreenWorld::SetCurrentXP(DWORD dwCurrentXP)
{
    CAIGroup* pGroup = g_pBaldurChitin->GetObjectGame()->GetGroup();
    LONG* pSpriteList = pGroup->GetGroupList();
    INT nCount = pGroup->GetCount();

    CGameSprite* pSprite;

    for (INT nIndex = 0; nIndex < nCount; nIndex++) {
        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(pSpriteList[nIndex],
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->GetBaseStats()->m_xp = dwCurrentXP;
            pSprite->field_562C = 1;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(pSpriteList[nIndex],
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    delete pSpriteList;
}

// 0x6907E0
void CScreenWorld::AddGold(DWORD dwGold)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 5054
    UTIL_ASSERT(pGame != NULL);

    pGame->AddPartyGold(dwGold);
}

// 0x690820
void CScreenWorld::ConfigureContainer()
{
    // Close whatever popup sub-panel is already up before showing the container.
    switch (field_EA4) {
    case 0:
    case 7:
    case 17:
    case 19:
    case 21:
    case 22:
    case -1:
        break;
    case 6:
        StopCommand();
        field_EA4 = -1;
        break;
    case 8:
        StopContainer();
        field_EA4 = -1;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 5469
        UTIL_ASSERT(FALSE);
        break;
    }

    // NOTE: field_EA4 is the popup sub-panel state (6 command, 8 container,
    // 17 death, -1 none).
    field_EA4 = 8;
    m_cUIManager.ClearTooltip();

    if (!GetManager()->m_bHidden) {
        if (m_nAutoHideInterface > 0) {
            m_nAutoHideInterface++;
        }
    } else {
        m_nAutoHideInterface++;
        UnhideInterface();
    }

    // NOTE: the original also selects the right-side info panel here (id 7, or
    // 15 when two engine globals read non-zero), re-checks it through a
    // vtable+0xCC call that takes an argument our GetManager() does not, and
    // records the id in field_114C when the panel's +0xF4 flag reads 1; those
    // globals and that argument-taking call are unmapped, so the panel swap is
    // left unrecovered.

    sub_695570(FALSE, FALSE);  // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 5172
    UTIL_ASSERT(pGame != NULL);

    CUIPanel* pPanel = m_cUIManager.GetPanel(8);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 5174
    UTIL_ASSERT(pPanel != NULL);

    pPanel->SetActive(TRUE);
    pPanel->InvalidateRect(NULL);

    // Shrink the world viewport to make room for the container panel.
    SetNewViewSize(CInfinity::stru_8E79F8, FALSE);

    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(0x34));

    m_nTopContainerRow = 0;
    static_cast<CUIControlScrollBarWorldContainer*>(pPanel->GetControl(0x34))->UpdateScrollBar();

    m_nTopGroupRow = 0;
    static_cast<CUIControlScrollBarWorldContainer*>(pPanel->GetControl(0x35))->UpdateScrollBar();

    // Open sound + container graphic (control 0x32) by container type.
    CResRef cContainerBam;
    switch (pGame->GetContainerType(pGame->m_iContainer)) {
    case 1:  PlayGUISound(CResRef("GAM_12A1")); cContainerBam = CResRef("CONTSACK"); break;
    case 2:  PlayGUISound(CResRef("AMB_D05A")); cContainerBam = CResRef("CONTCHST"); break;
    case 3:  PlayGUISound(CResRef("AMB_D05A")); cContainerBam = CResRef("CONTDRWR"); break;
    case 4:  PlayGUISound(CResRef("AMB_D18"));  cContainerBam = CResRef("CONTGRND"); break;
    case 5:  PlayGUISound(CResRef("AMB_D08"));  cContainerBam = CResRef("CONTTABL"); break;
    case 6:  PlayGUISound(CResRef("AMB_D07"));  cContainerBam = CResRef("CONTSHLF"); break;
    case 7:  PlayGUISound(CResRef("AMB_D07"));  cContainerBam = CResRef("CONTALTR"); break;
    case 8:  PlayGUISound(CResRef("AMB_D18"));  cContainerBam = CResRef(""); break;
    case 9:  PlayGUISound(CResRef("GAM_06"));   cContainerBam = CResRef("CONTBOOK"); break;
    case 10: PlayGUISound(CResRef("AMB_D08G")); cContainerBam = CResRef("CONTBODY"); break;
    case 11: PlayGUISound(CResRef("AMB_D12"));  cContainerBam = CResRef("CONTBARL"); break;
    case 12: PlayGUISound(CResRef("AMB_D05A")); cContainerBam = CResRef("CONTCRAT"); break;
    default: cContainerBam = CResRef("CONTBODY"); break;
    }

    static_cast<CUIControlButtonWorldContainerIcon*>(pPanel->GetControl(0x32))->m_resRef = cContainerBam;

    // Looting character's encumbrance/volume (control 0x36) and party gold
    // (control 0x10000036); inlined in the original.
    LONG nLooterId = pGame->m_iContainerSprite;

    CGameSprite* pLooter;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nLooterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pLooter),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        CUIControlEncumbrance* pEncumbrance =
            static_cast<CUIControlEncumbrance*>(pPanel->GetControl(0x36));

        INT nWeight = static_cast<INT>(pLooter->GetCarriedWeight());

        CDerivedStats* pDStats = pLooter->GetDerivedStats();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 5290
        UTIL_ASSERT(pDStats != NULL);

        SHORT nStr = pDStats->m_nSTR;

        // Max carry weight: the STRENGTH_MODIFIERS table's max-load column (3)
        // for this STR, scaled by the character's encumbrance modifier (percent).
        INT nMaxWeight = static_cast<INT>(
            static_cast<float>(pGame->GetRuleTables().GetEncumbranceMod(pLooter)) / 100.0f
            * static_cast<float>(atol(pGame->GetRuleTables().m_tStrengthMod.GetAt(CPoint(3, nStr)))));

        INT nUsedSlots;
        INT nTotalSlots;
        pLooter->GetNumInventoryPersonalSlots(nUsedSlots, nTotalSlots);

        pEncumbrance->SetEncumbrance(nWeight, nMaxWeight);
        pEncumbrance->SetVolume(nUsedSlots, nTotalSlots);

        pGame->GetObjectArray()->ReleaseShare(nLooterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        UpdateLabel(pPanel, 0x10000036, "%d", pGame->GetGameSave()->m_nPartyGold);
    }
}

// 0x690EB0
void CScreenWorld::StopContainer()
{
    if (!GetManager()->m_bHidden) {
        if (m_nAutoHideInterface > 0) {
            m_nAutoHideInterface++;
        }
    } else {
        m_nAutoHideInterface++;
        UnhideInterface();
    }

    // NOTE: Uninline.
    SetNewViewSize(CInfinity::stru_8E79B8, FALSE);

    sub_695570(TRUE, FALSE);

    m_cUIManager.GetPanel(8)->SetActive(FALSE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 5353
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 5355
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    switch (pGame->GetContainerType(nContainer)) {
    case 1:
        PlayGUISound(CResRef("GAM_12A"));
        break;
    case 2:
        PlayGUISound(CResRef("AMB_D05B"));
        break;
    case 3:
        PlayGUISound(CResRef("AMB_D13"));
        break;
    case 9:
        PlayGUISound(CResRef("GAM_05"));
        break;
    case 11:
        PlayGUISound(CResRef("AMB_D13"));
        break;
    case 12:
        PlayGUISound(CResRef("AMB_D05B"));
        break;
    }

    pGame->CompressContainer(nContainer);

    if (m_nAutoHideInterface > 0) {
        m_nAutoHideInterface--;
        HideInterface();
    }

    field_EA4 = -1;
}

// 0x691090
void CScreenWorld::CancelPopup()
{
    switch (field_EA4) {
    case -1:
    case 0:
    case 7:
        break;
    case 6:
        StopCommand();
        field_EA4 = -1;
        break;
    case 8:
        StopContainer();
        field_EA4 = -1;
        break;
    case 15:
    case 17:
        StopDeath();
        field_EA4 = -1;
        break;
    case 19:
    case 21:
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 5469
        UTIL_ASSERT(FALSE);
    }
}

// 0x691130
int CScreenWorld::GetActivePanelID()
{
    return field_EA4;
}

// 0x691140
void CScreenWorld::StartCommand()
{
    // NOTE: Uninline.
    CancelPopup();

    field_EA4 = 6;
    m_cUIManager.ClearTooltip();

    for (INT nId = 6; nId <= 17; nId++) {
        // FIXME: `GetPanel` should be outside of the loop.
        CUIControlBase* pControl = m_cUIManager.GetPanel(1)->GetControl(nId);
        if (pControl != NULL) {
            pControl->SetActive(FALSE);
        }
    }

    m_cUIManager.GetPanel(GetPanel_22_0())->SetActive(FALSE);
    m_cUIManager.GetPanel(6)->SetActive(TRUE);
    m_cUIManager.SetCapture(m_cUIManager.GetPanel(6)->GetControl(0), CUIManager::KEYBOARD);
    m_cUIManager.GetPanel(6)->InvalidateRect(NULL);
}

// 0x6912A0
void CScreenWorld::StopCommand()
{
    field_F37 = 0;

    // NOTE: Uninline.
    m_cUIManager.KillCapture();

    m_cUIManager.GetPanel(6)->SetActive(FALSE);

    for (INT nId = 6; nId <= 17; nId++) {
        // FIXME: `GetPanel` should be outside of the loop.
        CUIControlBase* pControl = m_cUIManager.GetPanel(1)->GetControl(nId);
        if (pControl != NULL) {
            pControl->SetActive(TRUE);
        }
    }

    // FIXME: Global.
    if (g_pBaldurChitin->m_pEngineWorld->field_11F0) {
        m_cUIManager.GetPanel(1)->InvalidateRect(NULL);
    }

    // FIXME: Global.
    if (g_pBaldurChitin->m_pEngineWorld->field_11F4) {
        // FIXME: Repeats.
        m_cUIManager.GetPanel(GetPanel_22_0())->SetActive(TRUE);
        m_cUIManager.GetPanel(GetPanel_22_0())->InvalidateRect(NULL);
    }

    field_EA4 = -1;
}

// FIXME: `cResStore` should be reference.
//
// 0x6913C0
void CScreenWorld::StartStore(const CAIObjectType& cAIProprietor, const CAIObjectType& cAICustomer, CResRef cResStore, BOOLEAN bInitiate)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    LONG nCustomerId = cAICustomer.m_nInstance;
    bMultiplayerStore = g_pChitin->cNetwork.m_nServiceProvider != CNetwork::SERV_PROV_NULL;

    if (g_pChitin->cNetwork.m_bConnectionEstablished == 1) {
        if (!g_pChitin->cNetwork.m_bIsHost) {
            if (!g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(cResStore.GetResRefStr(),
                    1014, TRUE, TRUE, TRUE)) {
                g_pChitin->cNetwork.CloseSession(TRUE);
                return;
            }
        } else {
            pGame->DemandServerStore(cResStore, TRUE);
        }
    }

    CStore* pStore = new CStore(cResStore);
    // A bag is always driven locally, never handed to the session host.
    if (pStore->m_header.m_nStoreType == CSTOREFILEHEADER_STORETYPE_BAG) {
        bMultiplayerStore = FALSE;
    }

    if (!g_pChitin->cNetwork.m_bIsHost) {
        CMessageStoreRelease* pMessage = new CMessageStoreRelease(pStore->m_resRef,
            CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    } else {
        pGame->ReleaseServerStore(pStore->m_resRef);
    }
    delete pStore;

    BOOLEAN bInControl;
    if (g_pChitin->cNetwork.m_bConnectionEstablished == 1) {
        CGameObject* pCustomer;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCustomerId,
                CGameObjectArray::THREAD_ASYNCH, &pCustomer, INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            bInControl = pCustomer->m_remotePlayerID == g_pChitin->cNetwork.m_idLocalPlayer;
            if (bInitiate == 1) {
                INT nPlayer = g_pChitin->cNetwork.FindPlayerLocationByID(pCustomer->m_remotePlayerID, FALSE);
                if (bMultiplayerStore) {
                    if (!pGame->m_multiplayerSettings.GetPermission(nPlayer, CGamePermission::PURCHASING)) {
                        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1, 0x2A5F, 0xFF, 0xFF, -1,
                            CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX);
                        if (g_pChitin->cNetwork.m_bIsHost) {
                            pGame->m_multiplayerSettings.m_bHostPermittedStore = FALSE;
                            pGame->m_multiplayerSettings.m_idHostPermittedStore = 0;
                        }
                        pGame->GetObjectArray()->ReleaseShare(nCustomerId,
                            CGameObjectArray::THREAD_ASYNCH, INFINITE);
                        return;
                    }
                    STRREF strError;
                    if (bMultiplayerStore
                        && pGame->m_multiplayerSettings.m_bRestrictStoreOption == 1
                        && !pGame->CanEnterStore(strError)) {
                        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1, strError, 0xFF, 0xFF, -1,
                            CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX);
                        if (g_pChitin->cNetwork.m_bIsHost) {
                            pGame->m_multiplayerSettings.m_bHostPermittedStore = FALSE;
                            pGame->m_multiplayerSettings.m_idHostPermittedStore = 0;
                        }
                        pGame->GetObjectArray()->ReleaseShare(nCustomerId,
                            CGameObjectArray::THREAD_ASYNCH, INFINITE);
                        return;
                    }
                }
            }
            if (g_pChitin->cNetwork.m_bIsHost
                && ((bMultiplayerStore && pGame->m_multiplayerSettings.m_bRestrictStoreOption)
                    || bInControl)) {
                pGame->m_multiplayerSettings.m_bHostPermittedStore = TRUE;
                pGame->m_multiplayerSettings.m_idHostPermittedStore = pCustomer->m_remotePlayerID;
            }
            pGame->GetObjectArray()->ReleaseShare(nCustomerId,
                CGameObjectArray::THREAD_ASYNCH, INFINITE);
        } else {
            bInControl = FALSE;
            if (bInitiate == 1) {
                g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1, 0x2A5F, 0xFF, 0xFF, -1,
                    CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX);
                if (g_pChitin->cNetwork.m_bIsHost) {
                    pGame->m_multiplayerSettings.m_bHostPermittedStore = FALSE;
                    pGame->m_multiplayerSettings.m_idHostPermittedStore = 0;
                }
                return;
            }
            if (g_pChitin->cNetwork.m_bIsHost
                && bMultiplayerStore
                && pGame->m_multiplayerSettings.m_bRestrictStoreOption) {
                pGame->m_multiplayerSettings.m_bHostPermittedStore = TRUE;
                pGame->m_multiplayerSettings.m_idHostPermittedStore = 0;
            }
        }
    } else {
        bInControl = TRUE;
    }

    if (bInitiate == 1
        && ((bMultiplayerStore && pGame->m_multiplayerSettings.m_bRestrictStoreOption)
            || !bInControl)) {
        CMessageEnterStoreMode* pMessage = new CMessageEnterStoreMode(cAIProprietor,
            cAICustomer, cResStore, nCustomerId, nCustomerId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if ((bMultiplayerStore && pGame->m_multiplayerSettings.m_bRestrictStoreOption)
        || bInControl) {
        m_bInControlOfStore = bInControl;
        if (!g_pChitin->cNetwork.m_bConnectionEstablished
            || (bMultiplayerStore && pGame->m_multiplayerSettings.m_bRestrictStoreOption)) {
            pGame->GetWorldTimer()->StopTime();
            m_bPaused = TRUE;
        }
        CScreenStore* pScreenStore = g_pBaldurChitin->m_pEngineStore;
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 5914
        UTIL_ASSERT(pScreenStore != NULL);
        BOOLEAN bConnected = g_pChitin->cNetwork.m_bConnectionEstablished;
        pScreenStore->StartStore(cAIProprietor, cAICustomer, cResStore);
        if (bConnected == g_pChitin->cNetwork.m_bConnectionEstablished) {
            DWORD dwPanelId = pScreenStore->GetDefaultMainPanel();
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
            // __LINE__: 5933
            UTIL_ASSERT(dwPanelId != MAXDWORD);
            pScreenStore->SwitchMainPanel(dwPanelId);
            g_pBaldurChitin->GetActiveEngine()->SelectEngine(pScreenStore);
        }
    }
}

// 0x691B50
void CScreenWorld::StopStore()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (m_bInControlOfStore == 1
        && bMultiplayerStore
        && pGame->m_multiplayerSettings.m_bRestrictStoreOption) {
        CMessageExitStoreMode* pMessage = new CMessageExitStoreMode(CGameObjectArray::INVALID_INDEX,
            CGameObjectArray::INVALID_INDEX);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (g_pChitin->cNetwork.m_bIsHost == 1) {
        pGame->m_multiplayerSettings.m_bHostPermittedStore = FALSE;
        pGame->m_multiplayerSettings.m_idHostPermittedStore = 0;
    } else if (g_pChitin->cNetwork.m_nServiceProvider != CNetwork::SERV_PROV_NULL) {
        // NOTE: A client of a multiplayer game releases the store permission the
        // host granted here (0x436C00). Multiplayer is removed (ADR 0002).
    }

    if (!g_pChitin->cNetwork.m_bConnectionEstablished
        || (bMultiplayerStore && pGame->m_multiplayerSettings.m_bRestrictStoreOption)) {
        pGame->GetWorldTimer()->StartTime();
        m_bPaused = FALSE;
    }

    g_pBaldurChitin->m_pEngineStore->StopStore();
}

// 0x691C70
CRect CScreenWorld::GetNewViewSize()
{
    if (m_waitingOnResize) {
        return m_newViewSize;
    }

    CGameArea* pVisibleArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();
    if (pVisibleArea != NULL) {
        return pVisibleArea->GetInfinity()->rViewPort;
    }

    return CInfinity::stru_8E79B8;
}

// 0x691D10
void CScreenWorld::HandleAmbiance()
{
    if (m_boredCount != 0) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        if (pGame->GetWorldTimer()->m_active
            && (pGame->GetWorldTimer()->m_gameTime % 600 == 0 || m_ambianceForce)
            && (rand() % 100 <= 15 || m_ambianceForce)) {
            m_ambianceForce = FALSE;
            g_pBaldurChitin->m_pEngineWorld->m_bored = FALSE;
            g_pBaldurChitin->m_pEngineWorld->m_boredCount = 0;

            SHORT nPortrait;
            if (pGame->m_nCharacters != 0) {
                nPortrait = rand() % pGame->m_nCharacters;
            } else {
                nPortrait = 0;
            }

            LONG nCharacterId = pGame->GetCharacterId(nPortrait);
            if (m_lastAmbiance != nCharacterId || rand() % 10 == 0) {
                m_lastAmbiance = nCharacterId;

                CGameSprite* pSprite;

                BYTE rc;
                do {
                    rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    // NOTE: Looks like left over from earlier games.

                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }
    }
}

// 0x691E80
void CScreenWorld::StartDeathReaction(const CString& sName)
{
    // FIXME: Unused.
    CString v1;
    CString v2;
    CString v3(sName);
    v3.MakeUpper();

    if (m_deathSoundList.IsEmpty()) {
        CDeathSound* pDeathSound = new CDeathSound();
        pDeathSound->m_soundNum = -1;
        pDeathSound->m_soundLength = 36;
        pDeathSound->m_characterId = CGameObjectArray::INVALID_INDEX;
        pDeathSound->m_started = TRUE;
        m_deathSoundList.AddTail(pDeathSound);
    }

    SHORT nPortrait;
    if (g_pBaldurChitin->GetObjectGame()->GetNumCharacters() != 0) {
        nPortrait = rand() % g_pBaldurChitin->GetObjectGame()->GetNumCharacters();
    } else {
        nPortrait = 0;
    }

    LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    // FIXME: Share is not released in some cases.
    if (rc == CGameObjectArray::SUCCESS
        && pSprite->Orderable(FALSE)
        && sName != pSprite->m_scriptName) {
        CDeathSound* pDeathSound = new CDeathSound();
        pDeathSound->m_soundNum = pSprite->GetSound(CGameSprite::SOUND_REACT_TO_DEATH);
        pDeathSound->m_soundLength = pSprite->GetLength(pDeathSound->m_soundNum);
        pDeathSound->m_characterId = pSprite->GetId();
        pDeathSound->m_started = FALSE;
        m_deathSoundList.AddTail(pDeathSound);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x692090
void CScreenWorld::HandleDeathReaction()
{
    if (!m_deathSoundList.IsEmpty()) {
        CDeathSound* pDeathSound = m_deathSoundList.GetHead();
        if (!pDeathSound->m_started) {
            pDeathSound->m_started = TRUE;
            if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pDeathSound->m_characterId) != -1) {
                CMessageVerbalConstant* pVerbalConstant = new CMessageVerbalConstant(pDeathSound->m_soundNum,
                    pDeathSound->m_characterId,
                    pDeathSound->m_characterId);

                g_pBaldurChitin->GetMessageHandler()->AddMessage(pVerbalConstant, FALSE);

                if (pDeathSound->m_pTrigger != NULL) {
                    CMessageSetTrigger* pSetTrigger = new CMessageSetTrigger(*pDeathSound->m_pTrigger,
                        pDeathSound->m_characterId,
                        pDeathSound->m_characterId);

                    g_pBaldurChitin->GetMessageHandler()->AddMessage(pSetTrigger, FALSE);

                    delete pDeathSound->m_pTrigger;
                    pDeathSound->m_pTrigger = NULL;
                }
            }
        } else {
            if (pDeathSound->m_soundLength-- <= 0) {
                delete m_deathSoundList.RemoveHead();
            }
        }
    }
}

// 0x692290
POSITION CScreenWorld::DisplayText(const CString& sName, const CString& sText, COLORREF rgbNameColor, COLORREF rgbTextColor, LONG lMarker, BOOLEAN bMoveToTop)
{
    if (!g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        return NULL;
    }

    POSITION pos = m_pActiveDialogDisplay->DisplayString(sName,
        sText,
        rgbNameColor,
        rgbTextColor,
        lMarker,
        bMoveToTop,
        TRUE);

    if (m_pActiveDialogDisplay->GetNumLines(pos) > 2) {
        if (GetManager()->GetPanel(GetPanel_22_0())->m_bActive
            && !m_cUIManager.m_bHidden) {
            // FIXME: Same panels?
            CUIPanel* pPanel1 = GetManager()->GetPanel(GetPanel_22_0());
            CUIPanel* pPanel2 = GetManager()->GetPanel(GetPanel_22_0());

            pPanel2->SetActive(FALSE);
            pPanel1->SetActive(TRUE);

            CUIControlTextDisplay* pOldDialogDisplay = m_pActiveDialogDisplay;
            m_pActiveDialogDisplay = static_cast<CUIControlTextDisplay*>(pPanel1->GetControl(1));

            // FIXME: Obtains same control twice.
            static_cast<CUIControlTextDisplay*>(pPanel1->GetControl(1))->CopyDisplay(pOldDialogDisplay);

            m_pActiveDialogDisplay->SetTopString(pos);

            // NOTE: Uninline.
            SetNewViewSize(CInfinity::stru_8E79B8, FALSE);
        }
    }

    return pos;
}

// 0x692460
POSITION CScreenWorld::DisplayText(const CString& sName, const CString& sText, LONG lMarker, BOOLEAN bMoveToTop)
{
    if (!g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        return NULL;
    }

    POSITION pos = m_pActiveDialogDisplay->DisplayString(sName,
        sText,
        m_pActiveDialogDisplay->m_rgbLabelColor,
        m_pActiveDialogDisplay->m_rgbTextColor,
        lMarker,
        bMoveToTop,
        TRUE);

    if (m_pActiveDialogDisplay->GetNumLines(pos) > 2) {
        if (GetManager()->GetPanel(GetPanel_22_0())->m_bActive
            && !m_cUIManager.m_bHidden) {
            // FIXME: Same panels?
            CUIPanel* pPanel1 = GetManager()->GetPanel(GetPanel_22_0());
            CUIPanel* pPanel2 = GetManager()->GetPanel(GetPanel_22_0());

            pPanel2->SetActive(FALSE);
            pPanel1->SetActive(TRUE);

            CUIControlTextDisplay* pOldDialogDisplay = m_pActiveDialogDisplay;
            m_pActiveDialogDisplay = static_cast<CUIControlTextDisplay*>(pPanel1->GetControl(1));

            // FIXME: Obtains same control twice.
            static_cast<CUIControlTextDisplay*>(pPanel1->GetControl(1))->CopyDisplay(pOldDialogDisplay);

            // NOTE: No `SetTopString` as in the function above.

            // NOTE: Uninline.
            SetNewViewSize(CInfinity::stru_8E79B8, FALSE);
        }
    }

    return pos;
}

// 0x692630
void CScreenWorld::RemoveText(POSITION pos)
{
    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        m_pActiveDialogDisplay->RemoveString(pos);
    }
}

// 0x692660
void CScreenWorld::SetItemMarker(POSITION pos, LONG lMarker)
{
    if (g_pBaldurChitin->GetObjectGame()->m_bGameLoaded) {
        m_pActiveDialogDisplay->SetItemMarker(pos, lMarker);
    }
}

// 0x692690
void CScreenWorld::ClearChatMessages()
{
    m_nChatMessageCount = 0;
}

// 0x6926A0
void CScreenWorld::HideInterface()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (!g_pBaldurChitin->field_4A24) {
        return;
    }

    if (m_cUIManager.m_bHidden) {
        return;
    }

    // NOTE: Uninline.
    field_11D0 = GetNewViewSize();

    INT x;
    INT y;
    pGame->GetVisibleArea()->GetInfinity()->GetViewPosition(x, y);

    // NOTE: Uninline.
    SetNewViewSize(CInfinity::stru_8E7548, FALSE);

    pGame->GetVisibleArea()->GetInfinity()->SetViewPosition(x + m_newViewSize.left - field_11D0.left,
        y + m_newViewSize.top - field_11D0.top,
        TRUE);

    m_cUIManager.m_bHidden = TRUE;

    if (pGame->GetGameSave()->m_mode == 386 || pGame->GetGameSave()->m_mode == 1282) {
        m_cUIManager.InvalidateRect(NULL);
    }
}

// 0x692850
void CScreenWorld::UnhideInterface()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (!g_pBaldurChitin->field_4A24) {
        return;
    }

    if (!m_cUIManager.m_bHidden) {
        return;
    }

    m_cUIManager.m_bHidden = FALSE;

    if (pGame->GetVisibleArea() != NULL) {
        // NOTE: Uninline.
        CRect oldViewSize = GetNewViewSize();

        INT x;
        INT y;
        pGame->GetVisibleArea()->GetInfinity()->GetViewPosition(x, y);

        // NOTE: Uninline.
        SetNewViewSize(field_11D0, FALSE);

        pGame->GetVisibleArea()->GetInfinity()->SetViewPosition(x + m_newViewSize.left - oldViewSize.left,
            y + m_newViewSize.top - oldViewSize.top,
            TRUE);
    } else {
        // NOTE: Uninline.
        SetNewViewSize(field_11D0, FALSE);
    }

    m_cUIManager.InvalidateRect(NULL);
}

// FIXME: `newViewSize` should be reference.
//
// 0x692A10
void CScreenWorld::SetNewViewSize(CRect newViewSize, BOOL bForceViewSize)
{
    m_newViewSize = newViewSize;
    m_waitingOnResize = 2;
    m_bForceViewSize = bForceViewSize;
}

// 0x692A50
void CScreenWorld::StartDeath()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 7811
    UTIL_ASSERT(pGame != NULL);

    pGame->SetTempCursor(4);
    g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);

    // NOTE: Uninline.
    CancelPopup();

    field_EA4 = 17;

    m_cUIManager.ClearTooltip();
    pGame->GetWorldTimer()->StopTime();

    m_bPaused = TRUE;
    m_nAutoHideInterface = 0;

    pGame->GetGameSave()->m_mode = 4096;
    sub_695570(FALSE, TRUE);

    m_cUIManager.GetPanel(GetPanel_21_7())->SetActive(FALSE);
    m_cUIManager.GetPanel(GetPanel_21_7())->SetInactiveRender(FALSE);
    m_cUIManager.GetPanel(1)->SetInactiveRender(FALSE);
    m_cUIManager.GetPanel(GetPanel_22_0())->SetInactiveRender(FALSE);

    // NOTE: Uninline.
    SetNewViewSize(CInfinity::stru_8E7988, FALSE);

    CUIPanel* pPanel = m_cUIManager.GetPanel(17);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 7843
    UTIL_ASSERT(pPanel != NULL);

    pPanel->SetActive(TRUE);

    if (m_bHardPaused) {
        static_cast<CUIControlButton*>(pPanel->GetControl(1))->SetEnabled(FALSE);
        static_cast<CUIControlButton*>(pPanel->GetControl(2))->SetEnabled(FALSE);
    } else {
        static_cast<CUIControlButton*>(pPanel->GetControl(1))->SetEnabled(TRUE);
        static_cast<CUIControlButton*>(pPanel->GetControl(2))->SetEnabled(TRUE);
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !g_pChitin->cNetwork.GetSessionHosting()) {
        static_cast<CUIControlButton*>(pPanel->GetControl(1))->SetEnabled(FALSE);
    }

    STRREF strRef;
    switch (field_10F0) {
    case 1:
        strRef = 32848;
        break;
    case 3:
        strRef = field_10F4;
        break;
    default:
        if (g_pChitin->cNetwork.GetSessionOpen()) {
            if (g_pChitin->cNetwork.GetSessionHosting()) {
                strRef = 19377;
            } else {
                strRef = 11331;
            }
        } else {
            strRef = 16498;
        }
        break;
    }

    UpdateLabel(pPanel,
        -1,
        "%s",
        (LPCSTR)FetchString(strRef));

    CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(2));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 7901
    UTIL_ASSERT(pButton != NULL);

    if (g_pChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        pButton->SetText(FetchString(13906)); // "Exit"
    } else {
        pButton->SetText(FetchString(15417)); // "Quit"
    }
}

// 0x692E80
void CScreenWorld::StopDeath()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 7935
    UTIL_ASSERT(pGame != NULL);

    pGame->SetTempCursor(4);

    pGame->GetWorldTimer()->StartTime();

    m_bPaused = FALSE;
    pGame->GetGameSave()->m_mode = -1;

    m_cUIManager.GetPanel(17)->SetActive(FALSE);
    m_cUIManager.GetPanel(1)->SetActive(TRUE);
    m_cUIManager.GetPanel(GetPanel_22_0())->SetActive(TRUE);
    m_cUIManager.GetPanel(1)->InvalidateRect(NULL);
    m_cUIManager.GetPanel(GetPanel_22_0())->InvalidateRect(NULL);

    field_EA4 = -1;

    if (m_cUIManager.m_pFocusedControl != NULL) {
        // NOTE: Uninline.
        m_cUIManager.KillCapture();
    }
}

// 0x692FD0
DWORD CScreenWorld::GetPanel_21_7()
{
    if (g_pBaldurChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        return 21;
    } else {
        return 7;
    }
}

// 0x693000
DWORD CScreenWorld::GetPanel_22_0()
{
    if (g_pBaldurChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        return 22;
    } else {
        return 0;
    }
}

// 0x693030
DWORD CScreenWorld::GetPanel_21_0()
{
    if (g_pBaldurChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        return 21;
    } else {
        return 0;
    }
}

// 0x693060
DWORD CScreenWorld::GetPanel_19_0()
{
    if (g_pBaldurChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        return 19;
    } else {
        return 0;
    }
}

// 0x693090
void CScreenWorld::SetPendingChapterChange(BYTE nChapter, BYTE* szChapterResRef)
{
    if (nChapter <= CGameJournal::NUM_CHAPTERS) {
        m_bChapterTransitionPending = TRUE;
        m_nChapterTransition = nChapter;
        memcpy(m_szChapterTransitionResRef, szChapterResRef, RESREF_SIZE);
    } else {
        m_bChapterTransitionPending = FALSE;
        m_nChapterTransition = -1;
        memset(m_szChapterTransitionResRef, 0, RESREF_SIZE);
    }
}

// 0x6930F0
void CScreenWorld::SetPendingMovie(BYTE* szMovieResRef)
{
    m_bMoviePending = TRUE;
    memcpy(m_szMovieResRef, szMovieResRef, RESREF_SIZE);
}

// 0x693110
void CScreenWorld::StartSaveGameMultiplayerHost()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // FIXME: What for?
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    BOOLEAN bListenToJoin = pGame->GetMultiplayerSettings()->GetListenToJoinOption();
    pGame->GetMultiplayerSettings()->SetListenToJoinOption(FALSE, TRUE);

    pWorld->m_bEndMajorEventListenToJoin = bListenToJoin;
    pWorld->m_bEndMajorEventPauseStatus = pWorld->m_bPaused;
    pWorld->m_bHardPaused = TRUE;

    if (!pWorld->m_bPaused) {
        // "Paused for saving game"
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
            19716,
            RGB(255, 0, 0),
            RGB(255, 0, 0),
            -1,
            CGameObjectArray::INVALID_INDEX,
            CGameObjectArray::INVALID_INDEX);

        pWorld->TogglePauseGame(0, 1, g_pChitin->cNetwork.m_idLocalPlayer);
    }
}

// 0x6931E0
void CScreenWorld::StartMovieMultiplayerHost(BYTE* cMovieResRef)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // FIXME: What for?
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    BOOLEAN bListenToJoin = pGame->GetMultiplayerSettings()->GetListenToJoinOption();
    pGame->GetMultiplayerSettings()->SetListenToJoinOption(FALSE, TRUE);

    pWorld->m_bEndMajorEventListenToJoin = bListenToJoin;
    pWorld->m_bEndMajorEventPauseStatus = pWorld->m_bPaused;
    pWorld->m_bHardPaused = TRUE;

    if (!pWorld->m_bPaused) {
        // "Paused for movie"
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
            17557,
            RGB(255, 0, 0),
            RGB(255, 0, 0),
            -1,
            CGameObjectArray::INVALID_INDEX,
            CGameObjectArray::INVALID_INDEX);

        pWorld->TogglePauseGame(0, 1, g_pChitin->cNetwork.m_idLocalPlayer);
    }

    g_pBaldurChitin->GetBaldurMessage()->MovieAnnounceStatus(CResRef(cMovieResRef));

    m_movie = cMovieResRef;

    if (m_movie == CInfinity::DAWN_MOVIE
        || m_movie == CInfinity::DUSK_MOVIE
        || m_movie == "DEATH1") {
        g_pBaldurChitin->m_pEngineProjector->field_145 = 1;
    }

    if (m_movie == CInfinity::DAWN_MOVIE) {
        pWorld->m_bSetDayOnActivate = TRUE;
    } else if (m_movie == CInfinity::DUSK_MOVIE) {
        pWorld->m_bSetNightOnActivate = TRUE;
    } else if (m_movie == "DEATH1") {
        g_pBaldurChitin->GetActiveEngine()->SelectEngine(pWorld);
        pWorld->m_bGameOverPanel = TRUE;
    }

    g_pBaldurChitin->GetObjectCursor()->m_bVisible = FALSE;

    g_pBaldurChitin->m_pEngineProjector->PlayMovie(m_movie);

    m_movie = "";
}

// 0x6933F0
void CScreenWorld::SetPendingMapWorld(PLAYER_ID idController, SHORT facingDirection)
{
    m_bPendingMapWorld = TRUE;
    m_idPendingMapWorldController = idController;
    m_nPendingMapWorldDirection = facingDirection;
}

// 0x693410
void CScreenWorld::StartMapWorldMultiplayerHost(PLAYER_ID idController, SHORT facingDirection)
{
    m_idPendingMapWorldController = idController;
    m_nPendingMapWorldDirection = facingDirection;

    g_pBaldurChitin->GetBaldurMessage()->SendMapWorldAnnounceStatus(1, idController, facingDirection);

    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 8748
    UTIL_ASSERT(pWorldMap != NULL);

    pWorldMap->StartWorldMap(1, facingDirection, idController == g_pChitin->cNetwork.m_idLocalPlayer);

    g_pBaldurChitin->GetActiveEngine()->SelectEngine(pWorldMap);
}

// 0x6934A0
void CScreenWorld::SetPendingReformParty()
{
    m_bPendingReformParty = TRUE;
}

// 0x6934B0
void CScreenWorld::StartReformPartyMultiplayerHost()
{
    g_pBaldurChitin->GetBaldurMessage()->SendReformPartyAnnounceStatus();

    m_bPausedBeforePickParty = m_bPaused;

    g_pBaldurChitin->GetActiveEngine()->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
}

// 0x6934F0
void CScreenWorld::SetPendingRest(INT nHP, BOOLEAN bRenting, BOOLEAN bMovie)
{
    m_bRestPending = TRUE;
    m_nRestHP = nHP;
    m_bRestRenting = bRenting;
    m_bRestMovie = bMovie;
}

// 0x693520
void CScreenWorld::StartRestMultiplayerHost(INT nHP, BOOLEAN bRenting, BOOLEAN bMovie)
{
    // The listen-to-join save and restore run only under a real provider
    // (the `je` over the block at 0x693535); the null provider of a
    // single-player session never listens to join.
    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        BOOL bListenToJoin = pGame->GetMultiplayerSettings()->GetListenToJoinOption();
        pGame->GetMultiplayerSettings()->SetListenToJoinOption(FALSE, TRUE);
        g_pBaldurChitin->m_pEngineWorld->m_bEndMajorEventListenToJoin = bListenToJoin;
        g_pBaldurChitin->m_pEngineWorld->m_bEndMajorEventPauseStatus = g_pBaldurChitin->m_pEngineWorld->m_bPaused;
    }

    for (SHORT nPortrait = 0; nPortrait < g_pBaldurChitin->GetObjectGame()->GetNumCharacters(); nPortrait++) {
        LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait);

        CMessage101* pMessage = new CMessage101(FALSE,
            nCharacterId,
            nCharacterId,
            TRUE);

        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    CInfGame::m_bHealPartyOnRest = bMovie;
    g_pBaldurChitin->GetObjectGame()->RestPartyPrivate(nHP, bRenting);
    CInfGame::m_bHealPartyOnRest = FALSE;

    m_bRestMovie = FALSE;

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        BOOL bListenToJoin = g_pBaldurChitin->m_pEngineWorld->m_bEndMajorEventListenToJoin;
        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->SetListenToJoinOption(bListenToJoin, TRUE);
    }
}

// 0x693680
void CScreenWorld::CheckEndOfHardPause()
{
    if (g_pBaldurChitin->GetBaldurMessage()->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_SERVER, CBaldurMessage::SIGNAL_END_MAJOR_EVENT) == TRUE) {
        m_bHardPaused = FALSE;

        if (!m_bEndMajorEventPauseStatus
            && g_pBaldurChitin->m_pEngineWorld->m_bPaused == TRUE) {
            TogglePauseGame(0, 1, g_pBaldurChitin->cNetwork.m_idLocalPlayer);
        }

        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->SetListenToJoinOption(m_bEndMajorEventListenToJoin, TRUE);
    }
}

// 0x693710
void CScreenWorld::sub_693710()
{
    CUIPanel* pPanel;
    CUIControlBase* pControl;

    if (g_pBaldurChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        pPanel = m_cUIManager.GetPanel(21);
        if (pPanel->m_bActive) {
            pControl = pPanel->GetControl(3);
            if (pControl != NULL) {
                m_cUIManager.SetCapture(pControl, CUIManager::KEYBOARD);
                return;
            }
        }

        pPanel = m_cUIManager.GetPanel(GetPanel_22_0());
        if (pPanel->m_bActive) {
            pControl = pPanel->GetControl(3);
            if (pControl != NULL) {
                m_cUIManager.SetCapture(pControl, CUIManager::KEYBOARD);
                return;
            }
        }

        pPanel = m_cUIManager.GetPanel(GetPanel_19_0());
        if (pPanel->m_bActive) {
            pControl = pPanel->GetControl(5);
            if (pControl != NULL) {
                m_cUIManager.SetCapture(pControl, CUIManager::KEYBOARD);
                return;
            }
        }

        pPanel = m_cUIManager.GetPanel(GetPanel_21_0());
        if (pPanel->m_bActive) {
            pControl = pPanel->GetControl(5);
            if (pControl != NULL) {
                m_cUIManager.SetCapture(pControl, CUIManager::KEYBOARD);
                return;
            }
        }
    }
}

// NOTE: Inlined.
void CScreenWorld::CheckEndOfMultiplayerSynch()
{
    if (g_pBaldurChitin->GetBaldurMessage()->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_SERVER, CBaldurMessage::SIGNAL_MPSYNCH) == TRUE) {
        g_pBaldurChitin->GetBaldurMessage()->m_bMultiplayerSynchServerFinished = TRUE;
        if (!g_pBaldurChitin->GetBaldurMessage()->m_bMultiplayerSynchClientPending) {
            g_pBaldurChitin->GetBaldurMessage()->MultiplayerSynchReply();
        }
    }
}

// 0x693860
void CScreenWorld::CancelEngine()
{
    switch (field_EA4) {
    case -1:
    case 0:
    case 7:
        break;
    case 6:
        StopCommand();
        field_EA4 = -1;
        break;
    case 8:
        StopContainer();
        field_EA4 = -1;
        break;
    case 15:
    case 17:
        StopDeath();
        field_EA4 = -1;
        break;
    case 19:
    case 21:
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 9124
        UTIL_ASSERT(FALSE);
    }
}

// 0x693910
void CScreenWorld::DeleteAreaMultiplayer()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();

    if (!pMessage->m_bDeleteAreaPolling) {
        // One roll every call: the slot the search for an unused area starts
        // at, and the slot before it, where it stops.
        BYTE nArea = static_cast<BYTE>(rand() % CINFGAME_MAX_AREAS);
        BYTE nLast = nArea == 0 ? CINFGAME_MAX_AREAS - 1 : nArea - 1;

        for (; nArea != nLast; nArea = (nArea + 1) % CINFGAME_MAX_AREAS) {
            CGameArea* pArea = pGame->m_gameAreas[nArea];
            if (pArea != NULL
                && pArea->m_nCharacters == 0
                && pArea != pGame->m_pGameAreaMaster) {
                CString sArea;
                pArea->m_resRef.CopyToString(sArea);
                pArea->field_AE6 = 0;

                pMessage->StartDeleteAreaPoll(sArea, FALSE);
                break;
            }
        }
    }

    if (pMessage->m_bDeleteAreaPolling == TRUE) {
        if (pMessage->CanResolveDeleteAreaPoll() == TRUE) {
            // NOTE: The resolve frame draws no rand, where a frame without a
            // running poll draws the slot roll above.
            BOOLEAN bBallot = pMessage->DeleteAreaBallotResult();
            CString sArea = pMessage->GetDeleteAreaString();

            // The area named by the poll is deleted only when the party
            // left it (m_nCharacters), it is not the master area, no
            // character-termination sequence is running, and more than one
            // area is loaded; otherwise it is kept but flagged.
            BOOLEAN bDelete = FALSE;
            CGameArea* pPolled = pGame->GetArea(sArea);
            if (pPolled != NULL
                && pPolled->m_nCharacters == 0
                && pPolled != pGame->m_pGameAreaMaster
                && pGame->m_nCharacterTerminationSequenceDelay == 0) {
                INT nAreas = 0;
                for (INT nSlot = 0; nSlot < CINFGAME_MAX_AREAS; nSlot++) {
                    if (pGame->m_gameAreas[nSlot] != NULL) {
                        nAreas++;
                    }
                }

                if (nAreas > 1) {
                    bDelete = TRUE;
                }
            }

            bDelete = static_cast<BOOLEAN>(bDelete != FALSE && bBallot != FALSE);

            CGameArea* pArea = pGame->GetArea(sArea);
            if (bDelete == TRUE) {
                if (pArea != NULL) {
                    DeleteArea(pArea);
                }
            } else if (pArea != NULL) {
                pArea->field_AE6 = 1;
            }

            pMessage->ResolveDeleteAreaPoll(sArea, bDelete);
        } else {
            CString sArea = pMessage->GetDeleteAreaString();

            CGameArea* pArea = pGame->GetArea(sArea);
            if (pArea == NULL) {
                pMessage->CancelDeleteAreaPoll();
            } else if (GetTickCount() > pMessage->m_dwDeleteAreaTimeout) {
                pMessage->StartDeleteAreaPoll(sArea, TRUE);
            }
        }
    }
}

// 0x693C60
void CScreenWorld::DeleteArea(CGameArea* pArea)
{
    CAreaFile areaFile;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 9297
    UTIL_ASSERT(pArea != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    EnterCriticalSection(&field_106);
    pArea->SortLists();

    // The area is written to the temp folder unless this is a client, the
    // area is flagged not to be saved, or it was left less than 45 game
    // seconds ago.
    if ((!g_pChitin->cNetwork.m_bConnectionEstablished
            || g_pChitin->cNetwork.m_bIsHost == TRUE)
        && (pArea->m_header.m_flags & 1) == 0
        && (pArea->field_436 == 0
            || static_cast<DWORD>(pGame->m_worldTime.m_gameTime - pArea->field_438) >= 0x2d)) {
        BYTE* pAreaData = NULL;
        DWORD nAreaData = 0;
        pArea->Marshal(&pAreaData, &nAreaData, FALSE);
        if (pAreaData != NULL) {
            areaFile.SetResRef(pArea->m_resRef, TRUE, TRUE);
            areaFile.GetRes()->Write(pGame->m_sTempDir, pAreaData, nAreaData);
            delete pAreaData;
        }
    }

    // Find the area's slot.
    BYTE nSlot = 0;
    BOOL bFound = FALSE;
    while (nSlot < CINFGAME_MAX_AREAS) {
        if (pArea == pGame->m_gameAreas[nSlot]) {
            bFound = TRUE;
            break;
        }
        nSlot++;
    }

    if (bFound) {
        if (pArea == pGame->m_gameAreas[pGame->m_visibleArea]) {
            pGame->UnselectAll();
            CGameArea* pVisibleArea = pGame->m_gameAreas[pGame->m_visibleArea];
            pVisibleArea->m_bPicked = FALSE;
            pVisibleArea->m_iPicked = CGameObjectArray::INVALID_INDEX;
            pVisibleArea->m_nToolTip = 0;
            pVisibleArea->OnDeactivation();

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
                // The next visible area is the area of the first party
                // member found; the original releases that member's share
                // twice (0x694098 and 0x6940BB).
                BOOL bMoved = FALSE;
                for (SHORT nCharacter = 0; nCharacter < 6 && bMoved == FALSE; nCharacter++) {
                    LONG nCharacterId = pGame->GetFixedOrderCharacterId(nCharacter);
                    if (nCharacterId == CGameObjectArray::INVALID_INDEX) {
                        continue;
                    }

                    CGameObject* pObject;
                    BYTE rc;
                    do {
                        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            &pObject,
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        if (pObject->m_pArea != NULL) {
                            pGame->m_visibleArea = pObject->m_pArea->m_id;
                            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                            bMoved = TRUE;
                        } else {
                            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    }
                }

                if (bMoved == FALSE) {
                    // No party member's area: the first loaded slot.
                    for (BYTE nArea = 0; nArea < CINFGAME_MAX_AREAS; nArea++) {
                        if (pGame->m_gameAreas[nArea] != NULL) {
                            pGame->m_visibleArea = nArea;
                        }
                    }
                }
            } else {
                for (BYTE nArea = 0; nArea < CINFGAME_MAX_AREAS; nArea++) {
                    if (pGame->m_gameAreas[nArea] != NULL) {
                        pGame->m_visibleArea = nArea;
                    }
                }
            }

            pGame->m_gameAreas[pGame->m_visibleArea]->OnActivation();
        }

        if (nSlot >= CINFGAME_MAX_AREAS || pGame->m_gameAreas[nSlot] == NULL) {
            // __FILE__: .\Include\InfGame.h
            // __LINE__: 1389
            UTIL_ASSERT(FALSE);
        }
        pGame->m_gameAreas[nSlot] = NULL;
    }

    if (pArea != NULL) {
        delete pArea;
    }

    LeaveCriticalSection(&field_106);
}

// 0x694210
void CScreenWorld::CopyChatEditBox(CUIPanel* pFrom, CUIPanel* pTo)
{
    DWORD dwFromEditId = -1;
    if (pFrom != NULL) {
        switch (pFrom->m_nID) {
        case 19:
        case 21:
            dwFromEditId = 5;
            break;
        case 22:
            dwFromEditId = 3;
            break;
        }
    }

    DWORD dwToEditId = -1;
    if (pTo != NULL) {
        switch (pTo->m_nID) {
        case 19:
        case 21:
            dwToEditId = 5;
            break;
        case 22:
            dwToEditId = 3;
            break;
        }
    }

    CString sText;
    CUIControlEdit* pToEdit = NULL;
    CUIControlEdit* pFromEdit = NULL;

    if (dwToEditId != -1) {
        pToEdit = static_cast<CUIControlEdit*>(pTo->GetControl(dwToEditId));
    }

    if (dwFromEditId != -1) {
        pFromEdit = static_cast<CUIControlEdit*>(pFrom->GetControl(dwFromEditId));
    }

    if (pToEdit != NULL) {
        if (pFromEdit != NULL) {
            sText = pFromEdit->GetText();
            pToEdit->SetText(sText);

            if (m_cUIManager.m_pFocusedControl == pFromEdit) {
                m_cUIManager.SetCapture(pToEdit, CUIManager::KEYBOARD);
            }
        } else {
            pToEdit->SetText(CString(""));
        }
    }
}

// 0x694350
void CScreenWorld::GetChatEditBoxStatus(CString& sChatText, BOOL& bInputCapture)
{
    CUIPanel* pPanel = GetManager()->GetPanel(GetPanel_22_0());

    DWORD dwEditId = -1;
    if (pPanel != NULL) {
        switch (pPanel->m_nID) {
        case 19:
        case 21:
            dwEditId = 5;
            break;
        case 22:
            dwEditId = 3;
            break;
        }
    }

    CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(dwEditId));
    if (pEdit != NULL) {
        sChatText = pEdit->GetText();
        bInputCapture = m_cUIManager.m_pFocusedControl == pEdit;
    } else {
        sChatText = "";
        bInputCapture = FALSE;
    }
}

// 0x694460
void CScreenWorld::SetChatEditBoxStatus(const CString& sChatText, BOOL bInputCapture)
{
    CUIPanel* pPanel = GetManager()->GetPanel(GetPanel_22_0());

    DWORD dwEditId = -1;
    if (pPanel != NULL) {
        switch (pPanel->m_nID) {
        case 19:
        case 21:
            dwEditId = 5;
            break;
        case 22:
            dwEditId = 3;
            break;
        }
    }

    CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(dwEditId));
    if (pEdit != NULL) {
        pEdit->SetText(sChatText);

        if (bInputCapture) {
            m_cUIManager.SetCapture(pEdit, CUIManager::KEYBOARD);
        }
    }
}

// 0x694620
void CScreenWorld::LeaveAreaLuaPanic(CPoint ptLocation, SHORT nDirection, CString sAreaName, CString sParchment)
{
    // The Ghidra decompile of this function fails; the body below is from the
    // llvm-objdump listing (0x694620-0x694AD4). `nDirection` and `sParchment`
    // reach it but the original never reads them (only the by-value arguments
    // are destroyed at 0x694A92/0x694AA5).
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    UTIL_ASSERT(pGame != NULL);

    CGameArea* pArea = pGame->GetArea(sAreaName);

    for (INT nIndex = 0; nIndex < CMultiplayerSettings::MAX_CHARACTERS; nIndex++) {
        LONG characterId = nIndex < CMultiplayerSettings::MAX_PLAYERS
            ? pGame->m_characters[nIndex]
            : CGameObjectArray::INVALID_INDEX;

        BOOL bLeaves = FALSE;
        BOOL bControlledByLocalPlayer = FALSE;
        CGameSprite* pSprite = NULL;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(characterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        // The character leaves when the animation is live and it stands in an
        // area other than the one being left (0x69470E-0x69473F).
        if (pSprite->Animate() != 0 && pSprite->m_pArea != NULL && pSprite->m_pArea != pArea) {
            bLeaves = TRUE;
            if (g_pChitin->cNetwork.m_nServiceProvider == 0) {
                bControlledByLocalPlayer = TRUE;
            } else {
                bControlledByLocalPlayer = g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID;
            }
        }

        pGame->GetObjectArray()->ReleaseShare(characterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        if (!bLeaves) {
            continue;
        }

        if (!bControlledByLocalPlayer) {
            pGame->m_multiplayerSettings.SetCharacterControlledByPlayer(nIndex,
                g_pChitin->cNetwork.m_nLocalPlayer,
                TRUE,
                TRUE);
        }

        do {
            rc = pGame->GetObjectArray()->GetDeny(characterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        pSprite->FeedBack(CGameSprite::FEEDBACK_CHARACTER_CONTROL_SEIZURE, 0, 0, 0, -1, 0, 0);
        pSprite->m_cLastSpriteUpdate.Initialize(TRUE);

        // The 0x5BC220 constructor, which the exe inlines here (SetString1 at
        // 0x6948B4; m_specificID3 and m_internalFlags zeroed, m_specificID2 = 1).
        CAIAction action(CAIAction::LEAVEAREALUA, sAreaName, ptLocation, 0, 1);

        pSprite->ClearActions(FALSE);
        pSprite->m_interrupt = TRUE;
        pSprite->SetCurrAction(action);
        pSprite->ExecuteAction();

        // Bring the visible area to the area the character is leaving for
        // when it is empty (0x69492F-0x694A00).
        if (pGame->GetVisibleArea()->m_nCharacters == 0) {
            pGame->UnselectAll();
            pGame->GetVisibleArea()->m_bPicked = FALSE;
            pGame->GetVisibleArea()->m_iPicked = CGameObjectArray::INVALID_INDEX;
            pGame->GetVisibleArea()->m_nToolTip = 0;
            pGame->GetVisibleArea()->OnDeactivation();
            pGame->m_visibleArea = pSprite->m_pArea->m_id;
            pGame->GetVisibleArea()->OnActivation();
        }

        pGame->GetObjectArray()->ReleaseDeny(characterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x694AE0
void CScreenWorld::UpdatePartyGoldStatus()
{
    CUIPanel* pPanel;

    switch (field_EA4) {
    case 7:
    case 19:
    case 21:
        pPanel = m_cUIManager.GetPanel(field_EA4);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 9851
        UTIL_ASSERT(pPanel != NULL);

        UpdateLabel(pPanel,
            0x10000009,
            "%d",
            g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_nPartyGold);
        break;
    case 8:
        pPanel = m_cUIManager.GetPanel(field_EA4);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 9841
        UTIL_ASSERT(pPanel != NULL);

        UpdateLabel(pPanel,
            0x10000036,
            "%d",
            g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_nPartyGold);
        break;
    }
}

// 0x694BD0
void CScreenWorld::SetDialogTokens(CGameSprite* pCharacter)
{
    STR_RES strRes;

    CTlkTable& tlk = g_pBaldurChitin->GetTlkTable();

    tlk.SetToken(CInfGame::TOKEN_GABBER, pCharacter->GetName());

    CGameSprite* pSprite;
    BYTE rc;

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[0],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        tlk.SetToken(CInfGame::TOKEN_PLAYER1, pSprite->GetName());

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[0],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[1],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        tlk.SetToken(CInfGame::TOKEN_PLAYER2, pSprite->GetName());

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[1],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[2],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        tlk.SetToken(CInfGame::TOKEN_PLAYER3, pSprite->GetName());

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[2],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[3],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        tlk.SetToken(CInfGame::TOKEN_PLAYER4, pSprite->GetName());

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[3],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[4],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        tlk.SetToken(CInfGame::TOKEN_PLAYER5, pSprite->GetName());

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[4],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[5],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        tlk.SetToken(CInfGame::TOKEN_PLAYER6, pSprite->GetName());

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[5],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    STRREF strRace;
    switch (pCharacter->GetAIType().m_nRace) {
    case CAIOBJECTTYPE_R_HUMAN:
        strRace = 7193;
        break;
    case CAIOBJECTTYPE_R_ELF:
        strRace = 7194;
        break;
    case CAIOBJECTTYPE_R_HALF_ELF:
        strRace = 7197;
        break;
    case CAIOBJECTTYPE_R_DWARF:
        strRace = 7182;
        break;
    case CAIOBJECTTYPE_R_HALFLING:
        strRace = 7195;
        break;
    case CAIOBJECTTYPE_R_GNOME:
        strRace = 7196;
        break;
    case CAIOBJECTTYPE_R_HALF_ORC:
        strRace = 22;
        break;
    default:
        strRace = -1;
        break;
    }

    tlk.Fetch(strRace, strRes);
    tlk.SetToken(CInfGame::TOKEN_GABBER_RACE, strRes.szText);

    if (pCharacter->GetAIType().m_nGender == CAIObjectType::SEX_FEMALE) {
        tlk.field_58 = byte_8BA28C;

        tlk.Fetch(27475, strRes); // "ma'am"
        tlk.SetToken(CInfGame::TOKEN_GABBER_SIRMAAM, strRes.szText);

        tlk.Fetch(27476, strRes); // "girl"
        tlk.SetToken(CInfGame::TOKEN_GABBER_GIRLBOY, strRes.szText);

        tlk.Fetch(27479, strRes); // "sister"
        tlk.SetToken(CInfGame::TOKEN_GABBER_BROTHERSISTER, strRes.szText);

        tlk.Fetch(27480, strRes); // "lady"
        tlk.SetToken(CInfGame::TOKEN_GABBER_LADYLORD, strRes.szText);

        tlk.Fetch(27483, strRes); // "female"
        tlk.SetToken(CInfGame::TOKEN_GABBER_MALEFEMALE, strRes.szText);

        tlk.Fetch(27485, strRes); // "she"
        tlk.SetToken(CInfGame::TOKEN_GABBER_HESHE, strRes.szText);

        tlk.Fetch(27487, strRes); // "her"
        tlk.SetToken(CInfGame::TOKEN_GABBER_HISHER, strRes.szText);

        tlk.Fetch(strRace, strRes);
        tlk.SetToken(CInfGame::TOKEN_GABBER_RACE, strRes.szText);

        tlk.Fetch(27487, strRes); // "her"
        tlk.SetToken(CInfGame::TOKEN_GABBER_HIMHER, strRes.szText);

        tlk.Fetch(27490, strRes); // "woman"
        tlk.SetToken(CInfGame::TOKEN_GABBER_MANWOMAN, strRes.szText);
    } else {
        tlk.field_58 = byte_8FB954;

        tlk.Fetch(27473, strRes); // "sir"
        tlk.SetToken(CInfGame::TOKEN_GABBER_SIRMAAM, strRes.szText);

        tlk.Fetch(27477, strRes); // "boy"
        tlk.SetToken(CInfGame::TOKEN_GABBER_GIRLBOY, strRes.szText);

        tlk.Fetch(27478, strRes); // "brother"
        tlk.SetToken(CInfGame::TOKEN_GABBER_BROTHERSISTER, strRes.szText);

        tlk.Fetch(27481, strRes); // "lord"
        tlk.SetToken(CInfGame::TOKEN_GABBER_LADYLORD, strRes.szText);

        tlk.Fetch(27482, strRes); // "male"
        tlk.SetToken(CInfGame::TOKEN_GABBER_MALEFEMALE, strRes.szText);

        tlk.Fetch(27484, strRes); // "he"
        tlk.SetToken(CInfGame::TOKEN_GABBER_HESHE, strRes.szText);

        tlk.Fetch(27486, strRes); // "his"
        tlk.SetToken(CInfGame::TOKEN_GABBER_HISHER, strRes.szText);

        tlk.Fetch(27488, strRes); // "him"
        tlk.SetToken(CInfGame::TOKEN_GABBER_HIMHER, strRes.szText);

        tlk.Fetch(27489, strRes); // "man"
        tlk.SetToken(CInfGame::TOKEN_GABBER_MANWOMAN, strRes.szText);
    }
}

// 0x695570
void CScreenWorld::sub_695570(BOOL bActive, BOOL bInvalidate)
{
    CUIPanel* pActionPanel = m_cUIManager.GetPanel(1);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10118
    UTIL_ASSERT(pActionPanel != NULL);

    CUIPanel* pCommandPanel = m_cUIManager.GetPanel(GetPanel_22_0());

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10120
    UTIL_ASSERT(pCommandPanel != NULL);

    if (field_11F0) {
        pActionPanel->SetActive(bActive);

        if (bInvalidate) {
            pActionPanel->InvalidateRect(NULL);
        }
    }

    if (field_11F4) {
        pCommandPanel->SetActive(bActive);

        if (bInvalidate) {
            pCommandPanel->InvalidateRect(NULL);
        }
    }
}

// 0x695650
void CScreenWorld::sub_695650(BOOL bEnable)
{
    CUIPanel* pActionPanel = m_cUIManager.GetPanel(1);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10166
    UTIL_ASSERT(pActionPanel != NULL);

    CUIPanel* pCommandPanel = m_cUIManager.GetPanel(GetPanel_22_0());

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10168
    UTIL_ASSERT(pCommandPanel != NULL);

    pActionPanel->SetEnabled(bEnable);
    pCommandPanel->SetEnabled(bEnable);
}

// 0x6984F0
void CScreenWorld::OnRestButtonClick()
{
    CSingleLock renderLock(&(GetManager()->field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 12373
    UTIL_ASSERT(pGame != NULL);

    STRREF strError;
    if (pGame->CanRestParty(strError, 0, 0, 0)) {
        CInfGame::m_bHealPartyOnRest = GetPrivateProfileIntA("Game Options",
            "Heal Party on Rest",
            0,
            g_pBaldurChitin->GetIniFileName());
        pGame->RestParty(1, 0);
        CInfGame::m_bHealPartyOnRest = FALSE;
    } else {
        STR_RES strRes;
        g_pBaldurChitin->GetTlkTable().Fetch(strError, strRes);
        DisplayText(CString(""),
            strRes.szText,
            0,
            RGB(63, 255, 12),
            -1,
            FALSE);
    }

    renderLock.Unlock();
}

// 0x6986A0
void CScreenWorld::EnableKeyRepeat()
{
    m_pVirtualKeys[49].SetRepeat(1, 1);
    m_pVirtualKeys[50].SetRepeat(1, 1);
    m_pVirtualKeys[51].SetRepeat(1, 1);
    m_pVirtualKeys[52].SetRepeat(1, 1);
    m_pVirtualKeys[53].SetRepeat(1, 1);
    m_pVirtualKeys[54].SetRepeat(1, 1);
    m_pVirtualKeys[55].SetRepeat(1, 1);
    m_pVirtualKeys[56].SetRepeat(1, 1);
    m_pVirtualKeys[57].SetRepeat(1, 1);
}

// 0x698730
void CScreenWorld::DisableKeyRepeat()
{
    m_pVirtualKeys[49].SetRepeat(0, 0);
    m_pVirtualKeys[50].SetRepeat(0, 0);
    m_pVirtualKeys[51].SetRepeat(0, 0);
    m_pVirtualKeys[52].SetRepeat(0, 0);
    m_pVirtualKeys[53].SetRepeat(0, 0);
    m_pVirtualKeys[54].SetRepeat(0, 0);
    m_pVirtualKeys[55].SetRepeat(0, 0);
    m_pVirtualKeys[56].SetRepeat(0, 0);
    m_pVirtualKeys[57].SetRepeat(0, 0);
}

// 0x4384B0
void CScreenWorld::SetLeaveAreaLuaPanicData(CPoint ptValue, SHORT nValue, CString sValue1, CString sValue2)
{
    m_ptLeaveAreaLuaPanicLocation = ptValue;
    m_nLeaveAreaLuaPanicDirection = nValue;
    m_sLeaveAreaLuaPanicAreaName = sValue1;
    m_sLeaveAreaLuaPanicParchment = sValue2;
}

// 0x453780
BOOL CScreenWorld::sub_453780()
{
    return m_nAutoUnhideInterface > 0;
}

// 0x453790
void CScreenWorld::sub_453790(BOOL bEnabled)
{
    if (bEnabled) {
        m_nAutoUnhideInterface++;
    } else {
        m_nAutoUnhideInterface--;
    }
}

// 0x503030
CGameDialogSprite* CScreenWorld::GetInternalLoadedDialog()
{
    return &m_internalLoadedDialog;
}

// NOTE: Inlined.
void CScreenWorld::CheckPanelInputMode(DWORD dwPanelId, DWORD dwinputModeMask)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(dwPanelId);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 2820
    UTIL_ASSERT(pPanel != NULL);

    if (pPanel->m_bActive || pPanel->m_bInactiveRender) {
        pPanel->SetEnabled((g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode & dwinputModeMask) != 0);
    }
}

// NOTE: Inlined.
INT CScreenWorld::GetNumContainerRows(LONG nContainer)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 6075
    UTIL_ASSERT(pGame != NULL);

    return (pGame->GetNumGroundSlots(nContainer) + 4) / 5 + 2;
}

// NOTE: Inlined.
INT CScreenWorld::GetNumGroupRows()
{
    return (CScreenInventory::PERSONAL_INVENTORY_SIZE + 1) / 2;
}

// -----------------------------------------------------------------------------

// 0x6956F0
CUIControlButtonWorldContainerSlot::CUIControlButtonWorldContainerSlot(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
}

// 0x695740
CUIControlButtonWorldContainerSlot::~CUIControlButtonWorldContainerSlot()
{
}

// 0x6957E0
void CUIControlButtonWorldContainerSlot::OnLButtonClick(CPoint pt)
{
    CScreenInventory* pInventory = g_pBaldurChitin->m_pEngineInventory;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10270
    UTIL_ASSERT(pInventory != NULL);

    CResRef cResIcon;
    CResRef cResItem;

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10299
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10301
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10304
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    LONG nLooterId = pGame->m_iContainerSprite;
    SHORT nPortrait = static_cast<SHORT>(pGame->GetCharacterPortraitNum(nLooterId));

    // Only the controlling player may loot with this character (MP gate; always
    // true in single-player, where the session is not hosting).
    CGameSprite* pLooter;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nLooterId, CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pLooter), INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }
    BOOL bInControl = pLooter->InControl();
    pGame->GetObjectArray()->ReleaseShare(nLooterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    if (!bInControl) {
        return;
    }

    switch (m_nID) {
    case 0: case 1: case 2: case 3: case 4:
    case 5: case 6: case 7: case 8: case 9: {
        SHORT nGroundSlot = static_cast<SHORT>(pWorld->m_nTopContainerRow * 5 + m_nID);

        // Re-share to confirm the looter still has room for this item.
        do {
            rc = pGame->GetObjectArray()->GetShare(nLooterId, CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pLooter), INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }
        BOOL bCanTake = pGame->CanTakeContainerItem(nContainer, nGroundSlot, pLooter, nPortrait);
        pGame->GetObjectArray()->ReleaseShare(nLooterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        if (!bCanTake) {
            return;
        }

        CItem* pItem = NULL;
        STRREF errorCode;
        if (pGame->SwapItemGround(nContainer, nGroundSlot, pItem, errorCode, 0xFFFF, FALSE) && pItem != NULL) {
            pInventory->PlaySwapSound(NULL, pItem);

            if (pItem->GetResRef() != "MISC07") {
                // Find the first free backpack slot (18 .. 18 + PERSONAL_INVENTORY_SIZE).
                CItem* pScan;
                STRREF description;
                WORD wCount;
                SHORT nDestSlot = 18;
                do {
                    pGame->InventoryInfoPersonal(nPortrait, nDestSlot, pScan, description, cResIcon, cResItem, wCount, TRUE);
                    if (cResIcon != "") {
                        nDestSlot++;
                    }
                } while (cResIcon != "" && nDestSlot < CScreenInventory::PERSONAL_INVENTORY_SIZE + 18);

                if (nDestSlot >= CScreenInventory::PERSONAL_INVENTORY_SIZE + 18) {
                    // No room after all -- return the item to the container.
                    if ((!pGame->SwapItemGround(nContainer, nGroundSlot, pItem, errorCode, 0xFFFF, FALSE) || pItem != NULL)
                        && !pGame->SwapItemGround(nContainer, SHORT_MAX, pItem, errorCode, 0xFFFF, FALSE)) {
                        pGame->PutBackItem(pItem, nLooterId);
                    }
                    m_pPanel->InvalidateRect(NULL);
                    break;
                }

                // Scroll the personal view so the destination slot is visible.
                INT nGroupBase = pWorld->m_nTopGroupRow * 2;
                INT nRel = nDestSlot - 18;
                if (nRel < nGroupBase || nGroupBase + 3 < nRel) {
                    INT nNewTop = nRel / 2;
                    INT nMaxTop = (CScreenInventory::PERSONAL_INVENTORY_SIZE + 1) / 2 - 2;
                    if (nMaxTop <= nNewTop) {
                        nNewTop = nMaxTop;
                    }
                    if (nNewTop != pWorld->m_nTopGroupRow) {
                        pWorld->m_nTopGroupRow = nNewTop;
                        static_cast<CUIControlScrollBarWorldContainer*>(m_pPanel->GetControl(0x35))->UpdateScrollBar();
                    }
                }

                if (!pGame->SwapItemPersonalInventory(nPortrait, pItem, nDestSlot, errorCode, 0xFFFF, FALSE, TRUE)) {
                    // Placement failed -- return the item to the container.
                    if ((!pGame->SwapItemGround(nContainer, nGroundSlot, pItem, errorCode, 0xFFFF, FALSE) || pItem != NULL)
                        && !pGame->SwapItemGround(nContainer, SHORT_MAX, pItem, errorCode, 0xFFFF, FALSE)) {
                        pGame->PutBackItem(pItem, nLooterId);
                    }
                    m_pPanel->InvalidateRect(NULL);
                    break;
                }
            } else {
                // Gold merges into the party total; the item object is discarded.
                pGame->AddPartyGold(pItem->GetUsageCount(0));
                pGame->AddDisposableItem(pItem);
                pItem = NULL;
            }

            // Resync / compress the container (MP echo; an empty item makes the
            // receiver's Run() compress only, which is the correct SP cleanup).
            CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(CItem(),
                SHORT_MAX,
                TRUE,
                nContainer,
                nContainer);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            m_pPanel->InvalidateRect(NULL);
        }
        break;
    }
    case 10: case 11: case 12: case 13: {
        CItem* pItem = NULL;
        STRREF errorCode;
        SHORT nPersonalSlot = static_cast<SHORT>(pWorld->m_nTopGroupRow * 2 + m_nID + 8);

        if (pGame->SwapItemPersonalInventory(nPortrait, pItem, nPersonalSlot, errorCode, 0xFFFF, FALSE, TRUE) && pItem != NULL) {
            CItem* pSwapSoundItem = pItem;

            // Find the first free container slot.
            CItem* pScan;
            STRREF description;
            WORD wCount;
            SHORT nDestSlot = 0;
            do {
                pGame->InventoryInfoGround(nContainer, nDestSlot, pScan, description, cResIcon, cResItem, wCount);
                if (cResIcon != "") {
                    nDestSlot++;
                }
            } while (cResIcon != "");

            // Scroll the container view so the destination slot is visible.
            INT nContainerBase = 5 * pWorld->m_nTopContainerRow;
            if (nDestSlot < nContainerBase || nContainerBase + 9 < nDestSlot) {
                pWorld->m_nTopContainerRow = nDestSlot / 5;
                static_cast<CUIControlScrollBarWorldContainer*>(m_pPanel->GetControl(0x34))->UpdateScrollBar();
            }

            if (!pGame->SwapItemGround(nContainer, nDestSlot, pItem, errorCode, 0xFFFF, FALSE) || pItem != NULL) {
                // Chosen slot failed/bounced -- try any slot, then back to the looter.
                if (!pGame->SwapItemGround(nContainer, SHORT_MAX, pItem, errorCode, 0xFFFF, FALSE)
                    && !pGame->SwapItemPersonalInventory(nPortrait, pItem, nPersonalSlot, errorCode, 0xFFFF, FALSE, TRUE)) {
                    pGame->PutBackItem(pItem, nLooterId);
                    m_pPanel->InvalidateRect(NULL);
                } else {
                    pInventory->PlaySwapSound(pSwapSoundItem, NULL);
                    m_pPanel->InvalidateRect(NULL);
                }
            } else {
                pInventory->PlaySwapSound(pSwapSoundItem, NULL);
                m_pPanel->InvalidateRect(NULL);
            }
        }
        break;
    }
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 10534
        UTIL_ASSERT(FALSE);
        break;
    }

    // Refresh the looter's encumbrance/volume and the party-gold readout; the
    // original inlines UpdateContainerEncumbrance here after every loot
    // transfer (ConfigureContainer holds the open path's copy of this block).
    CUIControlEncumbrance* pEncumbrance =
        static_cast<CUIControlEncumbrance*>(m_pPanel->GetControl(0x36));
    INT nWeight = static_cast<INT>(pLooter->GetCarriedWeight());
    CDerivedStats* pDStats = pLooter->GetActiveStats();
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10556
    UTIL_ASSERT(pDStats != NULL);
    SHORT nStr = pDStats->m_nSTR;
    INT nMaxWeight = static_cast<INT>(
        static_cast<float>(pGame->GetRuleTables().GetEncumbranceMod(pLooter)) / 100.0f
        * static_cast<float>(atol(pGame->GetRuleTables().m_tStrengthMod.GetAt(CPoint(3, nStr)))));
    INT nUsedSlots;
    INT nTotalSlots;
    pLooter->GetNumInventoryPersonalSlots(nUsedSlots, nTotalSlots);
    pEncumbrance->SetVolume(nUsedSlots, nTotalSlots);
    pEncumbrance->SetEncumbrance(nWeight, nMaxWeight);
    pGame->GetObjectArray()->ReleaseShare(nLooterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    CBaldurEngine::UpdateLabel(m_pPanel, 0x10000036, "%d", pGame->GetGameSave()->m_nPartyGold);
}

// 0x696140
BOOL CUIControlButtonWorldContainerSlot::Render(BOOL bForce)
{
    CResRef cResIcon;
    CResRef cResItem;

    if (!m_bActive && !m_bInactiveRender) {
        return FALSE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return FALSE;
    }

    if (!CUIControlButton::Render(bForce)) {
        return FALSE;
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10646
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10648
    UTIL_ASSERT(pGame != NULL);

    SHORT nPortrait = pGame->GetCharacterPortraitNum(pGame->m_iContainerSprite);

    CItem* pItem = NULL;
    STRREF description = -1;
    WORD wCount = 0;
    BOOL bHasIcon = FALSE;

    switch (m_nID) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    {
        LONG nContainer = pGame->m_iContainer;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 10673
        UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

        // 5 = ground container columns per row (0x85a1f4).
        SHORT nSlot = static_cast<SHORT>(pWorld->m_nTopContainerRow * 5 + m_nID);
        pGame->InventoryInfoGround(nContainer,
            nSlot,
            pItem,
            description,
            cResIcon,
            cResItem,
            wCount);
        bHasIcon = cResIcon != "";
        break;
    }
    case 10:
    case 11:
    case 12:
    case 13:
    {
        // 2 = personal columns per row (0x85a1fc); +8 maps button ids 10-13
        // onto backpack slots (the looter's equipment occupies slots 0-17).
        SHORT nSlot = static_cast<SHORT>(pWorld->m_nTopGroupRow * 2 + m_nID + 8);
        pGame->InventoryInfoPersonal(nPortrait,
            nSlot,
            pItem,
            description,
            cResIcon,
            cResItem,
            wCount,
            TRUE);
        bHasIcon = cResIcon != "";
        break;
    }
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 10709
        UTIL_ASSERT(FALSE);
        break;
    }

    if (bHasIcon) {
        CRect rControlFrame(m_pPanel->m_ptOrigin + m_ptOrigin, m_size);
        CRect rClip;
        rClip.IntersectRect(rControlFrame, m_rDirty);

        CIcon::RenderIcon(0,
            rControlFrame.TopLeft(),
            m_size,
            rClip,
            cResIcon,
            m_pPanel->m_pManager->m_bDoubleSize,
            0,
            wCount,
            FALSE,
            0,
            FALSE,
            0);

        if (pItem != NULL) {
            CPoint pos = rControlFrame.TopLeft();
            BOOL bDoubleSize = m_pPanel->m_pManager->m_bDoubleSize;
            DWORD dwItemFlags = pItem->m_flags;
            STRREF errorCode = -1;

            if ((dwItemFlags & 0x1) == 0) {
                CVidCell cState(CResRef("STORTIN2"), bDoubleSize);
                cState.Render(0, pos.x, pos.y, rClip, NULL, 0, 2, 0xC0);
            } else if (pGame->CheckIfPlayerInPortraitSlotCanUseItem(nPortrait,
                    pItem,
                    &errorCode,
                    FALSE)
                    == 0) {
                CVidCell cState(CResRef("STORTINT"), bDoubleSize);
                cState.Render(0, pos.x, pos.y, rClip, NULL, 0, 2, 0xC0);
            } else {
                SHORT nSelected = static_cast<SHORT>(pWorld->GetSelectedCharacter());
                LONG nLooterId = CGameObjectArray::INVALID_INDEX;

                if (nSelected < pGame->m_nCharacters) {
                    nLooterId = pGame->m_characterPortraits[nSelected];
                }

                if (nLooterId != CGameObjectArray::INVALID_INDEX) {
                    CGameSprite* pLooter;
                    BYTE share;
                    do {
                        pLooter = NULL;
                        share = pGame->GetObjectArray()->GetShare(nLooterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pLooter),
                            INFINITE);
                    } while (share == CGameObjectArray::SHARED
                        || share == CGameObjectArray::DENIED);

                    if (share == CGameObjectArray::SUCCESS) {
                        INT nUsable = pGame->CheckItemUsable(pLooter, pItem);
                        CResRef cStateOverlay;

                        if (nUsable == 2) {
                            cStateOverlay = CResRef("STORTIN4");
                        } else if (nUsable == 0) {
                            cStateOverlay = CResRef("STORTINT");
                        }

                        if (cStateOverlay != "") {
                            CVidCell cTint(cStateOverlay, bDoubleSize);
                            cTint.Render(0, pos.x, pos.y, rClip, NULL, 0, 2, 0xC0);
                        }

                        pGame->GetObjectArray()->ReleaseShare(nLooterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                }
            }

            if ((dwItemFlags & 0x8) != 0 && (dwItemFlags & 0x1) != 0) {
                CVidCell cState(CResRef("STORTIN3"), bDoubleSize);
                cState.Render(0, pos.x, pos.y, rClip, NULL, 0, 2, 0xC0);
            }
        }
    }

    SetToolTipStrRef(description, -1, -1);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x6967A0
CUIControlButtonDialog::CUIControlButtonDialog(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
}

// 0x6967F0
CUIControlButtonDialog::~CUIControlButtonDialog()
{
}

// 0x696890
void CUIControlButtonDialog::OnLButtonClick(CPoint pt)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10842
    UTIL_ASSERT(pWorld != NULL);

    if (GetTickCount() > pWorld->field_11BA + 600) {
        pWorld->field_1150 = 1;
        if (!pWorld->field_10B4) {
            pWorld->m_internalLoadedDialog.m_responseMarker = pWorld->field_10B2;
        }
        pWorld->field_11BA = GetTickCount();
    }

    SetActive(FALSE);
    m_pPanel->InvalidateRect(NULL);
}

// -----------------------------------------------------------------------------

// 0x696930
CUIControlScrollBarWorldContainer::CUIControlScrollBarWorldContainer(CUIPanel* panel, UI_CONTROL_SCROLLBAR* controlInfo)
    : CUIControlScrollBar(panel, controlInfo)
{
}

// 0x632C00
CUIControlScrollBarWorldContainer::~CUIControlScrollBarWorldContainer()
{
}

// 0x696950
void CUIControlScrollBarWorldContainer::OnPageUp(DWORD nLines)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10907
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10909
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10912
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    INT nNewTopContainerRow;
    INT nNewTopGroupRow;

    // FIXME: What for?
    INT nStep = abs(static_cast<INT>(nLines));

    switch (m_nID) {
    case 52:
        // NOTE: Uninline.
        // NOTE: Unused.
        pWorld->GetNumContainerRows(nContainer);

        nNewTopContainerRow = max(pWorld->m_nTopGroupRow - (nStep != 0 ? nStep : 2), 0);

        if (pWorld->m_nTopContainerRow != nNewTopContainerRow) {
            pWorld->m_nTopContainerRow = nNewTopContainerRow;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    case 53:
        nNewTopGroupRow = max(pWorld->m_nTopGroupRow - (nStep != 0 ? nStep : 2), 0);
        if (pWorld->m_nTopGroupRow != nNewTopGroupRow) {
            pWorld->m_nTopGroupRow = nNewTopGroupRow;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 10945
        UTIL_ASSERT(FALSE);
    }
}

// 0x696B70
void CUIControlScrollBarWorldContainer::OnPageDown(DWORD nLines)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10977
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10979
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 10982
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    INT nNumContainerRows;
    INT nNewTopContainerRow;
    INT nNumGroupRows;
    INT nNewTopGroupRow;

    // FIXME: What for?
    INT nStep = abs(static_cast<INT>(nLines));

    switch (m_nID) {
    case 52:
        // NOTE: Uninline.
        nNumContainerRows = pWorld->GetNumContainerRows(nContainer);
        nNewTopContainerRow = min(nNumContainerRows,
            pWorld->m_nTopContainerRow + (nStep != 0 ? nStep : 2));

        if (pWorld->m_nTopContainerRow != nNewTopContainerRow) {
            pWorld->m_nTopContainerRow = nNewTopContainerRow;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    case 53:
        // NOTE: Uninline.
        nNumGroupRows = pWorld->GetNumGroupRows() - 2;
        nNewTopGroupRow = min(nNumGroupRows,
            pWorld->m_nTopGroupRow + (nStep != 0 ? nStep : 2));

        if (pWorld->m_nTopGroupRow != nNewTopGroupRow) {
            pWorld->m_nTopGroupRow = nNewTopGroupRow;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 11015
        UTIL_ASSERT(FALSE);
    }
}

// 0x696DC0
void CUIControlScrollBarWorldContainer::OnScrollUp()
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11046
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11048
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11051
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    switch (m_nID) {
    case 52:
        if (pWorld->m_nTopContainerRow > 0) {
            pWorld->m_nTopContainerRow--;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    case 53:
        if (pWorld->m_nTopGroupRow > 0) {
            pWorld->m_nTopGroupRow -= 1;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 11080
        UTIL_ASSERT(FALSE);
    }
}

// 0x696F80
void CUIControlScrollBarWorldContainer::OnScrollDown()
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11111
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11113
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11116
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    switch (m_nID) {
    case 52:
        // NOTE: Uninline.
        if (pWorld->m_nTopContainerRow < pWorld->GetNumContainerRows(nContainer)) {
            pWorld->m_nTopContainerRow++;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    case 53:
        // NOTE: Uninline.
        if (pWorld->m_nTopGroupRow < pWorld->GetNumGroupRows() - 2) {
            pWorld->m_nTopGroupRow++;

            // NOTE: Uninline.
            InvalidateSlots();

            UpdateScrollBar();
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 11311
        UTIL_ASSERT(FALSE);
    }
}

// 0x697170
void CUIControlScrollBarWorldContainer::OnScroll()
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11177
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11179
    UTIL_ASSERT(pGame != NULL);

    LONG nContainer = pGame->m_iContainer;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 1182
    UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

    switch (m_nID) {
    case 52:
        // NOTE: Uninline.
        pWorld->m_nTopContainerRow = pWorld->GetNumContainerRows(nContainer) * field_144 / field_142;

        // NOTE: Uninline.
        InvalidateSlots();

        UpdateScrollBar();
        break;
    case 53:
        // NOTE: Uninline.
        pWorld->m_nTopGroupRow = (pWorld->GetNumGroupRows() - 2) * field_144 / field_142;

        // NOTE: Uninline.
        InvalidateSlots();

        UpdateScrollBar();
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 11311
        UTIL_ASSERT(FALSE);
    }
}

// 0x697380
void CUIControlScrollBarWorldContainer::UpdateScrollBar()
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11283
    UTIL_ASSERT(pWorld != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11285
    UTIL_ASSERT(pGame != NULL);

    switch (m_nID) {
    case 52:
        if (1) {
            LONG nContainer = pGame->m_iContainer;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
            // __LINE__: 11296
            UTIL_ASSERT(nContainer != CGameObjectArray::INVALID_INDEX);

            // NOTE: Uninline.
            AdjustScrollBar(pWorld->m_nTopContainerRow,
                pWorld->GetNumContainerRows(nContainer),
                2);
        }
        break;
    case 53:
        // NOTE: Uninline.
        AdjustScrollBar(pWorld->m_nTopGroupRow,
            pWorld->GetNumGroupRows(),
            2);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 11311
        UTIL_ASSERT(FALSE);
    }
}

// NOTE: Inlined.
void CUIControlScrollBarWorldContainer::InvalidateSlots()
{
    DWORD nFirstID;
    DWORD nLastID;

    switch (m_nID) {
    case 52:
        nFirstID = 0;
        nLastID = 9;
        break;
    case 53:
        nFirstID = 10;
        nLastID = 13;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
        // __LINE__: 11245
        UTIL_ASSERT(FALSE);
    }

    for (DWORD nID = nFirstID; nID <= nLastID; nID++) {
        CUIControlBase* pControl = m_pPanel->GetControl(nID);
        pControl->InvalidateRect();
    }
}

// -----------------------------------------------------------------------------

// 0x6974B0
CUIControlButtonWorldContainerIcon::CUIControlButtonWorldContainerIcon(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    field_662 = 0;
}

// 0x697540
CUIControlButtonWorldContainerIcon::~CUIControlButtonWorldContainerIcon()
{
}

// 0x6975E0
BOOL CUIControlButtonWorldContainerIcon::Render(BOOL bForce)
{
    CVidCell vcIcon;

    if (!m_bActive && !m_bInactiveRender) {
        return FALSE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return FALSE;
    }

    CVidMode* pVidMode = g_pBaldurChitin->GetCurrentVideoMode();

    if (m_resRef == "") {
        return FALSE;
    }

    // NOTE: Uninline.
    vcIcon.SetResRef(m_resRef, m_pPanel->m_pManager->m_bDoubleSize, TRUE, TRUE);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11411
    UTIL_ASSERT(vcIcon.GetRes() != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11412
    UTIL_ASSERT(pVidMode != NULL);

    CPoint pt = m_pPanel->m_ptOrigin + m_ptOrigin;

    CSize size;
    vcIcon.GetCurrentFrameSize(size, FALSE);

    CRect rFrame(pt, size);

    CRect rClip;
    rClip.IntersectRect(rFrame, m_rDirty);

    vcIcon.Render(0, pt.x, pt.y, rClip, NULL, 0, 0, -1);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x6977E0
CUIControlButtonWorldContainerClose::CUIControlButtonWorldContainerClose(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
}

// 0x697830
CUIControlButtonWorldContainerClose::~CUIControlButtonWorldContainerClose()
{
}

// 0x6978D0
void CUIControlButtonWorldContainerClose::OnLButtonClick(CPoint pt)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11471
    UTIL_ASSERT(pWorld != NULL);

    CSingleLock renderLock(&(pWorld->GetManager()->field_36), FALSE);
    renderLock.Lock(INFINITE);

    pWorld->StopContainer();

    renderLock.Unlock();
}

// -----------------------------------------------------------------------------

// 0x697970
CUIControlButtonClock::CUIControlButtonClock(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton3State(panel, controlInfo, LBUTTON, 0)
{
    // NOTE: Uninline.
    m_vcGear.SetResRef(CResRef("CGEAR"), g_pBaldurChitin->field_4A28, TRUE, TRUE);

    SetNeedAsyncUpdate();

    m_bSelected = TRUE;
}

// 0x697C20
CUIControlButtonClock::~CUIControlButtonClock()
{
}

// 0x697D80
void CUIControlButtonClock::TimerAsynchronousUpdate(BOOLEAN bInside)
{
    if (m_bActive) {
        if (bInside
            && !m_bToolTipActive
            && m_pPanel->m_pManager->field_1C >= g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nTooltips) {
            ActivateToolTip();
            m_bToolTipActive = TRUE;
        }

        CTimerWorld* pTimerWorld = g_pBaldurChitin->GetObjectGame()->GetWorldTimer();

        BYTE nSequenceLength = m_vcGear.GetSequenceLength(m_vcGear.m_nCurrentSequence, FALSE);
        m_vcGear.FrameSet(pTimerWorld->m_gameTime % nSequenceLength);

        InvalidateRect();

        m_bSelected = pTimerWorld->m_active;
    }
}

// 0x697CE0
void CUIControlButtonClock::ActivateToolTip()
{
    CString sTime;

    CTimerWorld::GetCurrentTimeString(g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime,
        16410,
        sTime);

    g_pBaldurChitin->GetObjectCursor()->SetToolTip(-1, this, sTime);
}

// NOTE: Always returns `TRUE` which is a bit unusual for `Render` function.
//
// 0x697E40
BOOL CUIControlButtonClock::Render(BOOL bForce)
{
    if (!m_bActive && !m_bInactiveRender) {
        return TRUE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return TRUE;
    }

    if (m_nRenderCount != 0) {
        CSingleLock lock(&(m_pPanel->m_pManager->field_56), FALSE);
        lock.Lock(INFINITE);
        m_nRenderCount--;
        lock.Unlock();
    }

    CPoint pt = m_pPanel->m_ptOrigin + m_ptOrigin;

    CRect rControlFrame(pt, m_size);

    CRect rClip;
    rClip.IntersectRect(rControlFrame, m_rDirty);

    m_vcGear.Render(0, pt.x, pt.y, rClip, NULL, 0, 0, -1);

    return TRUE;
}

// 0x697F90
void CUIControlButtonClock::OnLButtonClick(CPoint)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 11721
    UTIL_ASSERT(pWorld != NULL);

    if (g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineWorld
        || g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineInventory
        || g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineMap) {
        pWorld->TogglePauseGame(1, 1, 0);
    }
}

// -----------------------------------------------------------------------------

// 0x697FF0
CUIControlButtonWorldDeathLoad::CUIControlButtonWorldDeathLoad(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    SetText(CBaldurEngine::FetchString(15590)); // "Load"
}

// 0x6980A0
CUIControlButtonWorldDeathLoad::~CUIControlButtonWorldDeathLoad()
{
}

// 0x698140
void CUIControlButtonWorldDeathLoad::OnLButtonClick(CPoint pt)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 12190
    UTIL_ASSERT(pWorld != NULL);

    pWorld->StopDeath();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 12195
    UTIL_ASSERT(pGame != NULL);

    CScreenLoad* pLoad = g_pBaldurChitin->m_pEngineLoad;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 12208
    UTIL_ASSERT(pLoad != NULL);

    pLoad->StartLoad(g_pChitin->cNetwork.GetSessionOpen() ? 3 : 2);
    pWorld->SelectEngine(pLoad);

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !pGame->GetMultiplayerSettings()->m_bArbitrationLockStatus) {

        for (BYTE nSlot = 0; nSlot < CINFGAME_MAXCHARACTERS; nSlot++) {
            // NOTE: Uninline.
            LONG nCharacterId = pGame->GetFixedOrderCharacterId(nSlot);

            if (nCharacterId != CGameObjectArray::INVALID_INDEX) {
                g_pBaldurChitin->GetBaldurMessage()->ObjectControlRequest(nCharacterId);
            }
        }

        g_pBaldurChitin->GetBaldurMessage()->ObjectControl();
        pGame->GetMultiplayerSettings()->SetArbitrationLockAllowInput(FALSE);
        pGame->GetMultiplayerSettings()->SetArbitrationLockStatus(TRUE, 1);
    }

    pGame->DestroyGame(TRUE, FALSE);
}

// -----------------------------------------------------------------------------

// 0x6982C0
CUIControlButtonWorldDeathQuit::CUIControlButtonWorldDeathQuit(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    SetText(CBaldurEngine::FetchString(15417)); // "Quit"
}

// 0x698370
CUIControlButtonWorldDeathQuit::~CUIControlButtonWorldDeathQuit()
{
}

// 0x698410
void CUIControlButtonWorldDeathQuit::OnLButtonClick(CPoint pt)
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 12285
    UTIL_ASSERT(pWorld != NULL);

    pWorld->StopDeath();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenWorld.cpp
    // __LINE__: 12289
    UTIL_ASSERT(pGame != NULL);

    pWorld->SelectEngine(g_pBaldurChitin->m_pEngineConnection);

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
            g_pBaldurChitin->m_pEngineConnection->ShowSessionTerminatedMessage();
        }

        g_pChitin->cNetwork.CloseSession(TRUE);
        g_pBaldurChitin->GetBaldurMessage()->m_bPlayerShutdown = FALSE;
    }

    pGame->DestroyGame(TRUE, FALSE);
}
