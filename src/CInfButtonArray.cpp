#include "CInfButtonArray.h"

#include "CAIAction.h"
#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CButtonData.h"
#include "CGameButtonList.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CIcon.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CScreenWorld.h"
#include "CSpell.h"
#include "CUIControlButton.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "CUtil.h"

// 0x851700
const BYTE CInfButtonArray::STATE_NONE = 0;

// 0x8E6820
CGameButtonList* CInfButtonArray::dword_8E6820;

// 0x85BCB4
static const UINT dword_85BCB4 = 5;

// 0x85BD1C
//
// NOTE: The stealth grey out after leaving stealth (a CGameSprite constant in
// the original, not in our CGameSprite.h).
static const LONG dword_85BD1C = 90;

// 0x84CEB8
//
// NOTE: The icon flags of a greyed out action button.
static const DWORD dword_84CEB8 = 2;

// 0x84EBB0
//
// NOTE: The tint of a quick item the character cannot use now.
static const COLORREF dword_84EBB0 = RGB(255, 255, 0);

// 0x8477A6
//
// NOTE: `CAIAction::HIDE`, not in our CAIAction.h.
static const SHORT CAIACTION_HIDE = 18;

// 0x587960
CInfButtonSettings::CInfButtonSettings()
{
    field_1C8 = 0;
    field_1CC = 0;
    field_1D8 = 0;
    field_0 = 0;
    field_4 = 0;
    field_8 = 0;
    field_1D0 = 0;
    m_bGreyOut = FALSE;
    field_C = -1;
    field_10 = -1;

    // NOTE: Not in the original (see the header).
    field_1D4 = 0;
}

// 0x587B80
CInfButtonSettings::~CInfButtonSettings()
{
}

// 0x5879E0
CInfButtonArray::CInfButtonArray()
{
    field_19B2 = 0;
    field_1976 = 0;
    field_197A = 0;
    m_nSelectedButton = 100;
    field_16E0 = -1;
    field_16E4 = 0;
    m_nState = 0;

    // NOTE: The original leaves `m_nCurrentSelectedSpellClass`, `field_19DC`
    // and `m_nSelectedWeaponSet` unset until after this call; SetState(0)
    // reads none of them.
    m_nCurrentSelectedSpellClass = 0;
    field_19DC = 0;
    m_nSelectedWeaponSet = 0;

    SetState(0, 0);

    m_customizableButtonTypes[0] = 5;
    m_customizableButtonTypes[1] = 3;
    m_customizableButtonTypes[2] = 70;
    m_customizableButtonTypes[3] = 71;
    m_customizableButtonTypes[4] = 80;
    m_customizableButtonTypes[5] = 81;
    m_customizableButtonTypes[6] = 90;
    m_customizableButtonTypes[7] = 91;
    m_customizableButtonTypes[8] = 10;
    m_nCurrentSelectedSpellClass = 0;
    field_19DC = 0;
    field_19E0 = CResRef();
    m_nSelectedWeaponSet = 0;
}

// 0x59E980
CInfButtonArray::~CInfButtonArray()
{
    // The only explicit statement of the original's destructor. Everything
    // after it in the listing is the compiler-generated member destruction in
    // reverse declaration order: the state stack at +0x1986 (the original's
    // std::deque<INT> block teardown at +0x198A..+0x19B2), then the three
    // CVidCells at +0x189C/+0x17C2/+0x16E8, then the array of twelve
    // CInfButtonSettings at +0x0 (stride 0x1E0, ~CInfButtonSettings 0x587B80).
    sub_587BD0();
}

// 0x587BD0
void CInfButtonArray::sub_587BD0()
{
    if (dword_8E6820 != NULL) {
        while (dword_8E6820->GetCount() != 0) {
            delete dword_8E6820->RemoveHead();
        }
        dword_8E6820->RemoveAll();
        delete dword_8E6820;
        dword_8E6820 = NULL;
    }
}

// 0x587C20
CGameButtonList* CInfButtonArray::sub_587C20(INT nIndex, INT nType, BYTE& nClass, DWORD nDomain, BOOLEAN bItemUsages)
{
    CGameButtonList* pList = NULL;

    if (nType != 2
        && nType != 1
        && nType != 3
        && nType != 4
        && nType != 5
        && nType != 6) {
        return NULL;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return NULL;
    }

    LONG* pGroupList = pGame->GetGroup()->GetGroupList();
    LONG nCharacterId = pGroupList[0];
    delete[] pGroupList;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return NULL;
    }

    switch (nType) {
    case 1:
        if (bItemUsages) {
            pList = pSprite->GetItemUsages(static_cast<SHORT>(nIndex), 1, -1);
        } else {
            pList = pSprite->sub_717250(FALSE);
        }
        break;
    case 2:
        if (nClass == CAIOBJECTTYPE_C_CLERIC && nDomain != 0) {
            pList = pSprite->GetDomainQuickButtons(nClass, nDomain);
        } else {
            pList = pSprite->GetSpellQuickButtons(nClass);
        }
        break;
    case 3:
        if (!pGame->m_cOptions.m_bQuickItemMapping || bItemUsages) {
            pList = pSprite->sub_716E80(FALSE);
        } else {
            pList = pSprite->GetItemUsages(static_cast<SHORT>(nIndex + 15), 3, -1);
        }
        break;
    case 4:
        pList = pSprite->GetInnateQuickButtons();
        break;
    case 5:
        pList = pSprite->GetInternalButtonList();
        break;
    case 6:
        pList = pSprite->GetSongsButtonList();
        break;
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return pList;
}

// 0x587DF0
CGameButtonList* CInfButtonArray::sub_587DF0(const CResRef& cResRef)
{
    CGameButtonList* pList = NULL;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return NULL;
    }

    LONG* pGroupList = pGame->GetGroup()->GetGroupList();
    LONG nCharacterId = pGroupList[0];
    delete[] pGroupList;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return NULL;
    }

    int nLevel;
    int nUnused1;
    int nUnused2;
    if (cResRef == CGameSprite::SPIN275) {
        if (pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
            pGame->GetRuleTables().sub_546B60(pSprite, nLevel, nUnused1, nUnused2, FALSE);
            // NOTE: Unsigned compare.
            UINT nCount = static_cast<UINT>(nLevel) > dword_85BCB4 ? dword_85BCB4 : static_cast<UINT>(nLevel);
            pList = pSprite->sub_717FC0(cResRef, nCount + 1);
        }
    } else if (cResRef == CGameSprite::SPIN276) {
        if (pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
            pGame->GetRuleTables().sub_546B60(pSprite, nLevel, nUnused2, nUnused1, FALSE);
            // NOTE: Unsigned compare.
            UINT nCount = static_cast<UINT>(nLevel) > dword_85BCB4 ? dword_85BCB4 : static_cast<UINT>(nLevel);
            pList = pSprite->sub_717FC0(cResRef, nCount + 1);
        }
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return pList;
}

// 0x587F80
void CInfButtonArray::sub_587F80(BYTE buttonNum, CButtonData& buttonData, INT nType)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return;
    }

    LONG* pGroupList = pGame->GetGroup()->GetGroupList();
    LONG nCharacterId = pGroupList[0];
    delete[] pGroupList;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    switch (nType) {
    case 1:
        pSprite->GetQuickWeapon(buttonNum, buttonData);
        break;
    case 2:
        pSprite->GetQuickSpell(buttonNum, buttonData);
        break;
    case 3:
        pSprite->GetQuickItem(buttonNum, buttonData);
        break;
    case 4:
        pSprite->GetQuickAbility(buttonNum, buttonData);
        break;
    case 6:
        pSprite->GetQuickSong(buttonNum, buttonData);
        break;
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x588240
void CInfButtonArray::GetSelectedQuickWeaponData(CButtonData& cButtonData)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() != 0) {
        LONG* groupList = pGame->GetGroup()->GetGroupList();
        LONG nCharacterId = groupList[0];
        delete[] groupList;

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->GetSelectedWeaponButton(cButtonData);

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x588300
void CInfButtonArray::sub_588300(BYTE buttonNum, CButtonData& buttonData)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() != 0) {
        LONG* pGroupList = pGame->GetGroup()->GetGroupList();
        LONG nCharacterId = pGroupList[0];
        delete[] pGroupList;

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->GetWeaponButton(buttonNum, buttonData);

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x5883C0
BYTE CInfButtonArray::GetSelectedModalMode()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    BYTE modalState = 0;

    if (pGame->GetGroup()->GetCount() != 0) {
        LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            modalState = pSprite->GetModalState();

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    return modalState;
}

// 0x588460
BYTE CInfButtonArray::GetButtonId(INT buttonType)
{
    for (BYTE id = 0; id < 12; id++) {
        if (m_buttonTypes[id] == buttonType && !m_buttonArray[id].m_bGreyOut) {
            return id;
        }
    }
    return -1;
}

// 0x5884B0
BOOLEAN CInfButtonArray::sub_5884B0(CButtonData* pButtonData, BOOL bFlag)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return FALSE;
    }

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    pSprite->ReadyOffInternalList(*pButtonData, bFlag == FALSE);

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x588570
void CInfButtonArray::sub_588570(SHORT buttonNum, INT nType)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return;
    }

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    switch (nType) {
    case 1:
        pSprite->SetSelectedWeaponButton(buttonNum);
        break;
    case 2:
        pSprite->ReadySpell(buttonNum, 2, FALSE);
        break;
    case 3:
        pSprite->ReadyItem(buttonNum, FALSE);
        break;
    case 4:
        pSprite->ReadySpell(buttonNum, 4, FALSE);
        break;
    case 6:
        pSprite->ReadySpell(buttonNum, 6, FALSE);
        break;
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x5886A0
BOOLEAN CInfButtonArray::sub_5886A0(CButtonData* pButtonData, BOOL bFlag)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return FALSE;
    }

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    pSprite->sub_71A0E0(*pButtonData, bFlag == FALSE);

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x588760
BOOLEAN CInfButtonArray::sub_588760(CButtonData* pButtonData, BOOL bFlag)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return FALSE;
    }

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    pSprite->sub_71A550(*pButtonData, bFlag == FALSE);

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x588820
BOOLEAN CInfButtonArray::sub_588820(CButtonData* pButtonData, BOOL bFlag)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return FALSE;
    }

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    UINT nSongIndex = 0;
    if (pGame->GetMasterSongLookup().Find(pButtonData->m_abilityId.m_res, nSongIndex) == TRUE
        && bFlag) {
        if ((pSprite->m_derivedStats.m_generalState & STATE_SILENCED) == 0
            && (pSprite->m_baseStats.m_generalState & STATE_SILENCED) == 0) {
            if (pSprite->m_nModalState == 1) {
                pSprite->SetModalState(0, FALSE);
                m_nSelectedButton = 100;
            } else {
                pSprite->m_nLastSong = static_cast<BYTE>(nSongIndex);
                pSprite->SetModalState(1, FALSE);
                m_nSelectedButton = 2;

                CSound cSound;
                cSound.SetResRef(CResRef("ACT_01"), TRUE, TRUE);
                cSound.SetFireForget(TRUE);
                cSound.SetChannel(5, 0);
                cSound.Play(FALSE);

                CAIAction action(CAIAction::SMALLWAIT, CPoint(-1, -1), 1, -1);

                CMessageAddAction* pMessage = new CMessageAddAction(action,
                    pSprite->GetId(),
                    pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }
        } else {
            pSprite->FeedBack(CGameSprite::FEEDBACK_SILENCED, 0, 0, 0, -1, 0, 0);
            pSprite->SetModalState(0, FALSE);
            m_nSelectedButton = 100;
            pGame->m_nState = 0;
            UpdateButtons();
        }
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x588CB0
void CInfButtonArray::sub_588CB0(CButtonData* pButtonData, BYTE buttonNum, INT nType)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return;
    }

    LONG* pGroupList = pGame->GetGroup()->GetGroupList();
    LONG nCharacterId = pGroupList[0];
    delete[] pGroupList;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    switch (nType) {
    case 1:
        pSprite->SetQuickWeapon(buttonNum, *pButtonData);
        if (pButtonData->m_abilityId.m_itemNum >= 11 && pButtonData->m_abilityId.m_itemNum <= 14) {
            pSprite->SetQuickWeapon(buttonNum, static_cast<BYTE>(pButtonData->m_abilityId.m_itemNum));
        } else {
            pSprite->SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
        }
        // NOTE: The original stores the slot's low byte a second time,
        // whatever the range check above chose.
        pSprite->SetQuickWeapon(buttonNum, static_cast<BYTE>(pButtonData->m_abilityId.m_itemNum));
        break;
    case 2:
        pSprite->SetQuickSpell(buttonNum, *pButtonData);
        break;
    case 3:
        pSprite->SetQuickItem(buttonNum, *pButtonData);
        break;
    case 4:
        pSprite->SetQuickAbility(buttonNum, *pButtonData);
        break;
    case 6:
        pSprite->SetQuickSong(buttonNum, *pButtonData);
        break;
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x588FF0
BOOL CInfButtonArray::ResetState()
{
    if (field_19B2 != 0) {
        INT nState = m_stateStack.RemoveHead();
        field_19B2 = static_cast<int>(m_stateStack.GetCount());
        SetState(nState, 0);
        return TRUE;
    }

    return TRUE;
}

// 0x589100
void CInfButtonArray::UpdateState()
{
    SetState(m_nState, 0);
}

// 0x589110
BOOL CInfButtonArray::SetState(INT nState, BYTE bPush)
{
    field_16E8.SetResRef(CResRef("GUIBTBUT"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
    field_17C2.SetResRef(CResRef("GUIBTACT"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);

    if (g_pBaldurChitin->pActiveEngine != NULL
        && g_pBaldurChitin->pActiveEngine->pVidMode != NULL) {
        g_pBaldurChitin->m_pEngineWorld->GetManager()->ClearTooltip();
    }

    sub_587BD0();

    if (bPush == TRUE && nState != m_nState && m_nState != 0) {
        m_stateStack.AddHead(m_nState);
        field_19B2 = static_cast<int>(m_stateStack.GetCount());
    }

    INT nType;
    BYTE nClass;
    BYTE* pClass;
    DWORD nDomain;
    BOOLEAN bItemUsages;

    switch (nState) {
    case 101:
        nClass = 0;
        dword_8E6820 = sub_587C20(field_1976 + 43, 1, nClass, 0, TRUE);
        m_buttonTypes[0] = 21;
        m_buttonTypes[1] = 22;
        m_buttonTypes[2] = 23;
        m_buttonTypes[3] = 24;
        m_buttonTypes[4] = 25;
        m_buttonTypes[5] = 26;
        m_buttonTypes[6] = 27;
        m_buttonTypes[7] = 28;
        m_buttonTypes[8] = 29;
        m_buttonTypes[9] = 30;
        m_buttonTypes[10] = 31;
        m_buttonTypes[11] = 32;
        m_nState = 101;
        return UpdateButtons();
    case 102:
    case 103:
        pClass = &m_nCurrentSelectedSpellClass;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
        // __LINE__: 1370
        UTIL_ASSERT(m_nCurrentSelectedSpellClass != 0);

        nDomain = field_19DC;
        bItemUsages = FALSE;
        nType = 2;
        break;
    case 104:
    case 105:
        bItemUsages = nState == 105;
        nClass = 0;
        pClass = &nClass;
        nDomain = 0;
        nType = 3;
        break;
    case 106:
    case 107:
        bItemUsages = FALSE;
        nDomain = 0;
        nClass = 0;
        pClass = &nClass;
        nType = 4;
        break;
    case 108:
        m_buttonTypes[0] = 21;
        m_buttonTypes[1] = 22;
        m_buttonTypes[2] = 23;
        m_buttonTypes[3] = 24;
        m_buttonTypes[4] = 25;
        m_buttonTypes[5] = 26;
        m_buttonTypes[6] = 27;
        m_buttonTypes[7] = 28;
        m_buttonTypes[8] = 29;
        m_buttonTypes[9] = 30;
        m_buttonTypes[10] = 31;
        m_buttonTypes[11] = 32;
        m_nState = 108;
        return UpdateButtons();
    case 109:
        m_buttonTypes[0] = 21;
        m_buttonTypes[1] = 22;
        m_buttonTypes[2] = 23;
        m_buttonTypes[3] = 24;
        m_buttonTypes[4] = 25;
        m_buttonTypes[5] = 26;
        m_buttonTypes[6] = 27;
        m_buttonTypes[7] = 28;
        m_buttonTypes[8] = 29;
        m_buttonTypes[9] = 30;
        m_buttonTypes[10] = 31;
        m_buttonTypes[11] = 32;
        m_nState = 109;
        return UpdateButtons();
    case 110:
        m_buttonTypes[0] = 7;
        m_buttonTypes[1] = 8;
        m_buttonTypes[2] = 15;
        m_buttonTypes[3] = 16;
        m_buttonTypes[4] = 17;
        m_buttonTypes[5] = 18;
        m_buttonTypes[6] = 19;
        m_buttonTypes[7] = 20;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 100;
        m_buttonTypes[10] = 100;
        m_buttonTypes[11] = 100;
        m_nState = nState;
        return UpdateButtons();
    case 111:
        m_buttonTypes[0] = 100;
        m_buttonTypes[1] = 100;
        m_buttonTypes[2] = 100;
        m_buttonTypes[3] = 100;
        m_buttonTypes[4] = 100;
        m_buttonTypes[5] = 100;
        m_buttonTypes[6] = 100;
        m_buttonTypes[7] = 100;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 100;
        m_buttonTypes[10] = 100;
        m_buttonTypes[11] = 100;
        m_nState = 111;
        return UpdateButtons();
    case 112:
        bItemUsages = FALSE;
        nDomain = 0;
        nClass = 0;
        pClass = &nClass;
        nType = 5;
        break;
    case 113:
    case 122:
        bItemUsages = FALSE;
        nDomain = 0;
        nClass = 0;
        pClass = &nClass;
        nType = 6;
        break;
    case 114:
        sub_5960E0();
        m_buttonTypes[0] = 7;
        m_nState = nState;
        m_buttonTypes[1] = m_nSelectedWeaponSet * 2 + 60;
        m_buttonTypes[2] = m_nSelectedWeaponSet * 2 + 61;
        m_buttonTypes[3] = m_customizableButtonTypes[0];
        m_buttonTypes[4] = m_customizableButtonTypes[1];
        m_buttonTypes[5] = m_customizableButtonTypes[2];
        m_buttonTypes[6] = m_customizableButtonTypes[3];
        m_buttonTypes[7] = m_customizableButtonTypes[4];
        m_buttonTypes[8] = m_customizableButtonTypes[5];
        m_buttonTypes[9] = m_customizableButtonTypes[6];
        m_buttonTypes[10] = m_customizableButtonTypes[7];
        m_buttonTypes[11] = m_customizableButtonTypes[8];
        return UpdateButtons();
    case 115:
    case 116:
        m_buttonTypes[0] = 11;
        m_buttonTypes[1] = 4;
        m_buttonTypes[2] = 12;
        m_buttonTypes[3] = 119;
        m_buttonTypes[4] = 13;
        m_buttonTypes[5] = 100;
        m_buttonTypes[6] = 100;
        m_buttonTypes[7] = 100;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 100;
        m_buttonTypes[10] = 100;
        m_buttonTypes[11] = 100;
        if (m_nState != nState) {
            field_197A = 0;
        }
        m_nState = nState;
        return UpdateButtons();
    case 117: {
        CGameSprite* pSprite = NULL;
        LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetGroup()->GetGroupLeader();
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            return FALSE;
        }

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
        // __LINE__: 1631
        UTIL_ASSERT(pSprite != NULL);

        BOOLEAN bFlag = pSprite->sub_724900();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        m_buttonTypes[0] = 35;
        if (bFlag == TRUE) {
            m_buttonTypes[1] = 36;
            m_buttonTypes[2] = 37;
            m_buttonTypes[3] = 38;
            m_buttonTypes[4] = 39;
            if (sub_595C80() == TRUE) {
                m_buttonTypes[5] = 40;
            } else {
                m_buttonTypes[5] = 100;
            }
        } else {
            m_buttonTypes[1] = 37;
            m_buttonTypes[2] = 38;
            m_buttonTypes[3] = 39;
            if (sub_595C80() == TRUE) {
                m_buttonTypes[4] = 40;
            } else {
                m_buttonTypes[4] = 100;
            }
            m_buttonTypes[5] = 100;
        }
        m_nState = nState;
        m_buttonTypes[6] = 100;
        m_buttonTypes[7] = 100;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 100;
        m_buttonTypes[10] = 41;
        m_buttonTypes[11] = 42;
        return UpdateButtons();
    }
    case 118:
    case 119: {
        UINT nIndex = 0;
        BYTE nSpellClass;

        nSpellClass = CAIOBJECTTYPE_C_BARD;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 50;
        }

        nSpellClass = CAIOBJECTTYPE_C_CLERIC;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 51;
            m_buttonTypes[nIndex++] = 57;
        }

        nSpellClass = CAIOBJECTTYPE_C_DRUID;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 52;
        }

        nSpellClass = CAIOBJECTTYPE_C_PALADIN;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 53;
        }

        nSpellClass = CAIOBJECTTYPE_C_RANGER;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 54;
        }

        nSpellClass = CAIOBJECTTYPE_C_SORCERER;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 55;
        }

        nSpellClass = CAIOBJECTTYPE_C_WIZARD;
        if (sub_595D60(nSpellClass) == TRUE) {
            m_buttonTypes[nIndex++] = 56;
        }

        while (nIndex < 12) {
            m_buttonTypes[nIndex++] = 100;
        }

        if (m_nState != nState) {
            field_197A = 0;
        }
        m_nState = nState;
        return UpdateButtons();
    }
    case 120:
        m_buttonTypes[0] = 80;
        m_buttonTypes[1] = 81;
        m_buttonTypes[2] = 82;
        m_buttonTypes[3] = 100;
        m_buttonTypes[4] = 100;
        m_buttonTypes[5] = 100;
        m_buttonTypes[6] = 100;
        m_buttonTypes[7] = 100;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 100;
        m_buttonTypes[10] = 100;
        m_buttonTypes[11] = 100;
        if (m_nState != nState) {
            field_197A = 0;
        }
        m_nState = nState;
        return UpdateButtons();
    case 121:
        m_buttonTypes[0] = 60;
        m_buttonTypes[1] = 61;
        m_buttonTypes[2] = 100;
        m_buttonTypes[3] = 62;
        m_buttonTypes[4] = 63;
        m_buttonTypes[5] = 100;
        m_buttonTypes[6] = 64;
        m_buttonTypes[7] = 65;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 66;
        m_buttonTypes[10] = 67;
        m_buttonTypes[11] = 100;
        m_nState = 121;
        return UpdateButtons();
    case 123:
        dword_8E6820 = sub_587DF0(field_19E0);
        m_buttonTypes[0] = 21;
        m_buttonTypes[1] = 22;
        m_buttonTypes[2] = 23;
        m_buttonTypes[3] = 24;
        m_buttonTypes[4] = 25;
        m_buttonTypes[5] = 26;
        m_buttonTypes[6] = 27;
        m_buttonTypes[7] = 28;
        m_buttonTypes[8] = 29;
        m_buttonTypes[9] = 30;
        m_buttonTypes[10] = 31;
        m_buttonTypes[11] = 32;
        if (m_nState != nState) {
            field_197A = 0;
        }
        m_nState = nState;
        return UpdateButtons();
    default:
        m_buttonTypes[0] = 100;
        m_buttonTypes[1] = 100;
        m_buttonTypes[2] = 100;
        m_buttonTypes[3] = 100;
        m_buttonTypes[4] = 100;
        m_buttonTypes[5] = 100;
        m_buttonTypes[6] = 100;
        m_buttonTypes[7] = 100;
        m_buttonTypes[8] = 100;
        m_buttonTypes[9] = 100;
        m_buttonTypes[10] = 100;
        m_buttonTypes[11] = 100;
        m_nState = 0;
        return UpdateButtons();
    }

    dword_8E6820 = sub_587C20(field_1976, nType, *pClass, nDomain, bItemUsages);
    if (dword_8E6820 == NULL || dword_8E6820->GetCount() < 13) {
        m_buttonTypes[0] = 21;
        m_buttonTypes[1] = 22;
        m_buttonTypes[2] = 23;
        m_buttonTypes[3] = 24;
        m_buttonTypes[4] = 25;
        m_buttonTypes[5] = 26;
        m_buttonTypes[6] = 27;
        m_buttonTypes[7] = 28;
        m_buttonTypes[8] = 29;
        m_buttonTypes[9] = 30;
        m_buttonTypes[10] = 31;
        m_buttonTypes[11] = 32;
    } else {
        m_buttonTypes[0] = 33;
        m_buttonTypes[1] = 21;
        m_buttonTypes[2] = 22;
        m_buttonTypes[3] = 23;
        m_buttonTypes[4] = 24;
        m_buttonTypes[5] = 25;
        m_buttonTypes[6] = 26;
        m_buttonTypes[7] = 27;
        m_buttonTypes[8] = 28;
        m_buttonTypes[9] = 29;
        m_buttonTypes[10] = 30;
        m_buttonTypes[11] = 34;
    }

    if (m_nState != nState) {
        field_197A = 0;
    }
    m_nState = nState;
    return UpdateButtons();
}

// 0x589FF0
void CInfButtonArray::sub_589FF0(BYTE bPush, BYTE bAll)
{
    if (field_19B2 != 0) {
        if (bAll == TRUE) {
            INT nState = m_stateStack.GetTail();
            m_stateStack.RemoveAll();
            field_19B2 = 0;
            SetState(nState, 0);
            return;
        }

        INT nState = m_stateStack.RemoveHead();
        field_19B2 = static_cast<int>(m_stateStack.GetCount());
        SetState(nState, bPush);
    }
}

// 0x595C80
BOOL CInfButtonArray::sub_595C80()
{
    BOOL bResult = FALSE;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return FALSE;
    }

    LONG* pGroupList = pGame->GetGroup()->GetGroupList();
    LONG nCharacterId = pGroupList[0];
    delete[] pGroupList;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    if (pSprite->GetAIType().IsClassValid(CAIObjectType::C_BARD)) {
        bResult = TRUE;
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return bResult;
}

// 0x595D60
BOOL CInfButtonArray::sub_595D60(const BYTE& nClass)
{
    BOOL bResult = FALSE;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        return FALSE;
    }

    LONG* pGroupList = pGame->GetGroup()->GetGroupList();
    LONG nCharacterId = pGroupList[0];
    delete[] pGroupList;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    switch (nClass) {
    case CAIOBJECTTYPE_C_BARD:
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
    case CAIOBJECTTYPE_C_SORCERER:
    case CAIOBJECTTYPE_C_WIZARD:
        bResult = pSprite->GetSpells(nClass)->m_nHighestLevel != 0;
        break;
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return bResult;
}

// 0x595E70
void CInfButtonArray::UpdateCustomizableButtonTypes(INT* pButtonList)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 7128
    UTIL_ASSERT(pButtonList != NULL);

    for (INT nButton = 0; nButton < 9; nButton++) {
        m_customizableButtonTypes[nButton] = pButtonList[nButton];
    }
}

// 0x595EB0
BOOLEAN CInfButtonArray::sub_595EB0(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 7136
    UTIL_ASSERT(pSprite != NULL);

    if (pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK) > 0
        && pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
        return TRUE;
    }

    if (pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE) > 0
        && pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
        return TRUE;
    }

    if (pSprite->sub_726270(CGAMESPRITE_FEAT_ARTERIAL_STRIKE) > 0
        && pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)) {
        return TRUE;
    }

    if (pSprite->sub_726270(CGAMESPRITE_FEAT_HAMSTRING) > 0
        && pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)) {
        return TRUE;
    }

    if (pSprite->sub_726270(CGAMESPRITE_FEAT_RAPID_SHOT) > 0
        && pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)) {
        return TRUE;
    }

    return FALSE;
}

// 0x595F70
void CInfButtonArray::UpdateSelectedWeaponSet(BYTE nIndex)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 7154
    UTIL_ASSERT(nIndex < CCREATUREFILEEQUIPMENT21_WEAPON_NUM_SETS);

    m_nSelectedWeaponSet = nIndex;
}

// 0x595FB0
INT CInfButtonArray::sub_595FB0(CGameButtonList* pButtonList)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 7162
    UTIL_ASSERT(pButtonList != NULL);

    // NOTE: The list walked is `dword_8E6820`, not `pButtonList`.
    INT nCount = 0;
    BYTE nLevel = 0;
    POSITION pos = dword_8E6820->FindIndex(field_197A);
    while (pos != NULL) {
        CButtonData* pButtonData = dword_8E6820->GetNext(pos);
        if (pButtonData != NULL) {
            if (nCount == 0) {
                nLevel = pButtonData->m_abilityId.field_1D;
            } else if (pButtonData->m_abilityId.field_1D != nLevel) {
                return field_197A + nCount;
            }
            nCount++;
        }
    }

    return 0;
}

// 0x596040
INT CInfButtonArray::sub_596040(CGameButtonList* pButtonList)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 7203
    UTIL_ASSERT(pButtonList != NULL);

    // NOTE: The list walked is `dword_8E6820`, not `pButtonList`.
    INT nCount = 0;
    UINT nFirstLevel = 0;
    UINT nPrevLevel = static_cast<UINT>(-1);
    POSITION pos = dword_8E6820->FindIndex(field_197A);
    while (pos != NULL) {
        CButtonData* pButtonData = dword_8E6820->GetPrev(pos);
        if (pButtonData != NULL) {
            if (nCount == 0) {
                nFirstLevel = pButtonData->m_abilityId.field_1D;
            } else if (nPrevLevel == static_cast<UINT>(-1)
                && pButtonData->m_abilityId.field_1D != nFirstLevel) {
                nPrevLevel = pButtonData->m_abilityId.field_1D;
            } else if (pButtonData->m_abilityId.field_1D != nPrevLevel) {
                INT nIndex = field_197A - nCount + 1;
                return nIndex < 0 ? 0 : nIndex;
            }
            nCount++;
        }
    }

    return 0;
}

// 0x5960E0
void CInfButtonArray::sub_5960E0()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() != 0) {
        LONG* pGroupList = pGame->GetGroup()->GetGroupList();
        LONG nCharacterId = pGroupList[0];
        delete[] pGroupList;

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            m_nSelectedWeaponSet = pSprite->m_nWeaponSet;

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x452C50
void CInfButtonArray::SetSelectedButton(INT nSelectedButton)
{
    m_nSelectedButton = nSelectedButton;
}

// 0x5959A0
BOOL CInfButtonArray::sub_5959A0(INT nButtonType)
{
    SHORT nButton;

    switch (nButtonType) {
    case 2:
        nButton = 12;
        break;
    case 3:
        nButton = 2;
        break;
    case 11:
        nButton = 0;
        break;
    case 12:
        nButton = 1;
        break;
    case 70:
        nButton = 3;
        break;
    case 71:
        nButton = 4;
        break;
    case 72:
        nButton = 5;
        break;
    case 73:
        nButton = 6;
        break;
    case 74:
        nButton = 7;
        break;
    case 75:
        nButton = 8;
        break;
    case 76:
        nButton = 9;
        break;
    case 77:
        nButton = 10;
        break;
    case 78:
        nButton = 11;
        break;
    case 90:
        nButton = 31;
        break;
    case 91:
        nButton = 32;
        break;
    case 92:
        nButton = 33;
        break;
    case 93:
        nButton = 34;
        break;
    case 94:
        nButton = 35;
        break;
    case 95:
        nButton = 36;
        break;
    case 96:
        nButton = 37;
        break;
    case 97:
        nButton = 38;
        break;
    case 98:
        nButton = 39;
        break;
    case 110:
        nButton = 13;
        break;
    case 111:
        nButton = 14;
        break;
    case 112:
        nButton = 15;
        break;
    case 113:
        nButton = 16;
        break;
    case 114:
        nButton = 17;
        break;
    case 115:
        nButton = 18;
        break;
    case 116:
        nButton = 19;
        break;
    case 117:
        nButton = 20;
        break;
    case 118:
        nButton = 21;
        break;
    default:
        return TRUE;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() != 0) {
        LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            BOOL bDisabled = pSprite->GetActiveStats()->m_disabledButtons[nButton];

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            return bDisabled == FALSE;
        }
    }

    return TRUE;
}

// 0x58A340
BOOL CInfButtonArray::UpdateButtons()
{
    CButtonData buttonData;
    CButtonData selectedWeaponData;
    CString sKey;
    CString sForm;

    CUIPanel* pPanel = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(1);
    if (pPanel == NULL) {
        return TRUE;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();
    CGameSprite* pSprite = NULL;
    BYTE rc = pGame->GetObjectArray()->GetShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pSprite),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 1800
    UTIL_ASSERT(pSprite != NULL);

    // One per spellcaster class index, the last one for the cleric domain.
    BOOLEAN bClassSpells[8] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
    BOOLEAN bSpells = FALSE;
    BOOLEAN bSongs = FALSE;

    if (pSprite->sub_724900() == TRUE) {
        for (UINT nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
            CGameSpriteGroupedSpellList* pSpells = &(pSprite->m_spells.m_spellsByClass[nClassIndex]);
            for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++) {
                bClassSpells[nClassIndex] |= pSpells->GetSpellsAtLevel(nLevel)->field_10 != 0;
            }
        }

        for (UINT nLevel = 0; nLevel < pSprite->m_domainSpells.m_nHighestLevel; nLevel++) {
            // __FILE__: .\Include\FileFormat.h
            // __LINE__: 2521
            UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

            bClassSpells[7] |= pSprite->m_domainSpells.m_lists[nLevel].field_10 != 0;
        }

        BYTE nClass;
        UINT nLevel;

        nLevel = 0;
        nClass = CAIOBJECTTYPE_C_BARD;
        while (nLevel < pSprite->GetSpells(nClass)->m_nHighestLevel) {
            nClass = CAIOBJECTTYPE_C_BARD;
            bClassSpells[0] |= pSprite->GetSpellsAtLevel(nClass, nLevel)->field_18 != 0;
            nLevel++;
            nClass = CAIOBJECTTYPE_C_BARD;
        }

        nLevel = 0;
        nClass = CAIOBJECTTYPE_C_SORCERER;
        while (nLevel < pSprite->GetSpells(nClass)->m_nHighestLevel) {
            nClass = CAIOBJECTTYPE_C_SORCERER;
            bClassSpells[5] |= pSprite->GetSpellsAtLevel(nClass, nLevel)->field_18 != 0;
            nLevel++;
            nClass = CAIOBJECTTYPE_C_SORCERER;
        }

        for (UINT nIndex = 0; nIndex < 8; nIndex++) {
            if (bSpells) {
                break;
            }
            bSpells = bClassSpells[nIndex] != FALSE;
        }
    }

    BOOLEAN bInnates;
    if (pSprite->m_innateSpells.field_10 != 0
        || pSprite->m_shapeshifts.field_18 != 0
        || pSprite->m_shapeshifts.field_10 != 0) {
        bInnates = TRUE;
    } else {
        bInnates = FALSE;
    }

    if (pSprite->sub_724920() == TRUE) {
        bSongs = pSprite->m_songs.m_List.size() != 0;
    }

    for (INT nButton = 0; nButton < 12; nButton++) {
        CUIControlButton* pControl = static_cast<CUIControlButton*>(pPanel->GetControl(nButton + 6));
        if (pControl == NULL) {
            continue;
        }

        sKey.Format("F%d", nButton + 1);

        CInfButtonSettings* pSettings = &(m_buttonArray[nButton]);
        INT* pButtonType = &(m_buttonTypes[nButton]);
        pSettings->field_1CC = m_nSelectedButton == *pButtonType;
        pSettings->m_bGreyOut = FALSE;

        STRREF strTip;
        STRREF strSecondaryTip;
        BYTE nModalState;
        BYTE nSpellClass;

        switch (*pButtonType) {
        case 0:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 48;
            pSettings->field_10 = 48;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(-1, -1, -1);
            break;
        case 1:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 52;
            pSettings->field_10 = 52;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(-1, -1, -1);
            break;
        case 2:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef("STONSONG"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 20;
            pSettings->field_10 = 22;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pSettings->field_1CC = 0;
            if (!bSongs) {
                pSettings->m_bGreyOut = TRUE;
            }

            strTip = 4918;
            strSecondaryTip = -1;
            if (pGame->GetGroup()->GetCount() != 0) {
                LONG nLeaderId = pGame->GetGroup()->GetGroupLeader();
                CGameSprite* pLeader;
                if (pGame->GetObjectArray()->GetShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pLeader),
                        INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    nModalState = pLeader->m_nModalState;
                    pGame->GetObjectArray()->ReleaseShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    if (nModalState == 1) {
                        pSettings->field_1CC = 1;

                        UINT nSongIndex = pSprite->m_nLastSong;

                        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
                        // __LINE__: 2184
                        UTIL_ASSERT_MSG(nSongIndex < pGame->GetMasterSongLookup().m_nCount, "Invalid Song Index!");

                        const CResRef& cSongResRef = pGame->GetMasterSongLookup().Get(nSongIndex);

                        CGameButtonList* pSongs = pSprite->GetSongsButtonList();
                        if (pSongs != NULL) {
                            if (cSongResRef != "") {
                                CButtonData* pFound = NULL;
                                POSITION pos = pSongs->GetHeadPosition();
                                while (pos != NULL) {
                                    pFound = pSongs->GetNext(pos);
                                    if (pFound != NULL) {
                                        if (pFound->m_abilityId.m_res == cSongResRef) {
                                            break;
                                        }
                                        pFound = NULL;
                                    }
                                }

                                if (pFound != NULL) {
                                    strTip = pFound->m_name;
                                    strSecondaryTip = 39824;
                                }
                            }

                            while (pSongs->GetCount() != 0) {
                                delete pSongs->RemoveHead();
                            }
                            pSongs->RemoveAll();
                            delete pSongs;
                        }
                    }
                }
            }

            pControl->SetToolTipStrRef(strTip, strSecondaryTip, -1);
            pControl->SetToolTipHotKey(10, 0xFFFF, sKey);
            break;
        case 3:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 8;
            pSettings->field_10 = 10;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;

            strTip = 4688;
            if (!bSpells) {
                pSettings->m_bGreyOut = TRUE;
                strTip = 37450;
            }
            pControl->SetToolTipStrRef(strTip, -1, -1);
            pControl->SetToolTipHotKey(11, 0xFFFF, sKey);
            break;
        case 4:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 36;
            pSettings->field_10 = 38;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;

            pSettings->field_1CC = 0;
            if (pGame->GetGroup()->GetCount() != 0) {
                LONG nLeaderId = pGame->GetGroup()->GetGroupLeader();
                CGameSprite* pLeader;
                if (pGame->GetObjectArray()->GetShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pLeader),
                        INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    nModalState = pLeader->m_nModalState;
                    pGame->GetObjectArray()->ReleaseShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    if (nModalState == 2) {
                        pSettings->field_1CC = 1;
                        m_nSelectedButton = 5;
                    }
                }
            }

            pControl->SetToolTipStrRef(4927, -1, -1);
            pControl->SetToolTipHotKey(16, 0xFFFF, sKey);
            break;
        case 5:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 96;
            pSettings->field_10 = 98;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;

            strTip = 4933;
            nModalState = 0;
            if (pGame->GetGroup()->GetCount() != 0) {
                LONG nLeaderId = pGame->GetGroup()->GetGroupLeader();
                CGameSprite* pLeader;
                if (pGame->GetObjectArray()->GetShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pLeader),
                        INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    nModalState = pLeader->m_nModalState;
                    pGame->GetObjectArray()->ReleaseShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }

            if (nModalState == 2) {
                strTip = 39826;
                m_nSelectedButton = 5;
            } else if (nModalState == 3) {
                strTip = 39825;
                m_nSelectedButton = 5;
            }
            pControl->SetToolTipStrRef(strTip, -1, -1);
            pControl->SetToolTipHotKey(13, 0xFFFF, sKey);
            break;
        case 7:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 0;
            pSettings->field_10 = 2;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(15925, -1, -1);
            pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
            break;
        case 8:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 12;
            pSettings->field_10 = 14;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4666, -1, -1);
            pControl->SetToolTipHotKey(18, 0xFFFF, sKey);
            break;
        case 9:
            if (pSprite->GetShapeshifts()->field_18 == 0) {
                pSettings->m_bGreyOut = TRUE;
            }
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 40;
            pSettings->field_10 = 40;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4958, -1, -1);
            pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
            break;
        case 10:
            strTip = 4954;
            if (!bInnates || pSprite->m_derivedStats.m_disabledSpellTypes[2] == TRUE) {
                pSettings->m_bGreyOut = TRUE;
                strTip = 37443;
            }
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 40;
            pSettings->field_10 = 42;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            if (sub_595EB0(pSprite) == TRUE) {
                pSettings->field_1CC = 1;
                strTip = 39827;
            }
            pControl->SetToolTipStrRef(strTip, -1, -1);
            pControl->SetToolTipHotKey(19, 0xFFFF, sKey);
            break;
        case 11:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 28;
            pSettings->field_10 = 30;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            if (pSprite->m_derivedStats.m_disabledButtons[0] == TRUE
                || pSprite->m_nStealthGreyOut > 0) {
                pSettings->m_bGreyOut = TRUE;
            }

            pSettings->field_1CC = 0;
            if (pGame->GetGroup()->GetCount() != 0) {
                LONG nLeaderId = pGame->GetGroup()->GetGroupLeader();
                CGameSprite* pLeader;
                if (pGame->GetObjectArray()->GetShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pLeader),
                        INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    nModalState = pLeader->m_nModalState;
                    pGame->GetObjectArray()->ReleaseShare(nLeaderId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    if (nModalState == 3) {
                        pSettings->field_1CC = 1;
                        m_nSelectedButton = 5;
                    }
                }
            }

            pControl->SetToolTipStrRef(4968, -1, -1);
            pControl->SetToolTipHotKey(15, 0xFFFF, sKey);
            break;
        case 12:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 24;
            pSettings->field_10 = 26;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4971, -1, -1);
            pControl->SetToolTipHotKey(14, 0xFFFF, sKey);
            break;
        case 13:
            if (pSprite->m_derivedStats.m_nSkills[1] < 1) {
                pSettings->m_bGreyOut = TRUE;
            }
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 124;
            pSettings->field_10 = 126;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4974, -1, -1);
            pControl->SetToolTipHotKey(9, 0xFFFF, sKey);
            break;
        case 14: {
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            strTip = 4978;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 16;
            pSettings->field_10 = 18;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;

            CGameButtonList* pItems = pSprite->sub_716E80(TRUE);
            if (pItems == NULL
                || pItems->GetCount() == 0
                || pSprite->m_derivedStats.m_bPolymorphed == TRUE) {
                pSettings->m_bGreyOut = TRUE;
                strTip = 37449;
            }

            if (pItems != NULL) {
                while (pItems->GetCount() != 0) {
                    delete pItems->RemoveHead();
                }
                pItems->RemoveAll();
                delete pItems;
            }

            pControl->SetToolTipStrRef(strTip, -1, -1);
            pControl->SetToolTipHotKey(12, 0xFFFF, sKey);
            break;
        }
        case 15:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 44;
            pSettings->field_10 = 44;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(15924, -1, -1);
            pControl->SetToolTipHotKey(17, 0xFFFF, sKey);
            break;
        case 16:
        case 17:
        case 18:
        case 19:
        case 20: {
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            pSettings->field_1C8 = 1;
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1D0 = 0;

            SHORT nQuickFormation = static_cast<SHORT>(*pButtonType - 16);

            // __FILE__: .\Include\InfGame.h
            // __LINE__: 1498
            UTIL_ASSERT(nQuickFormation < CGAMESAVE_NUM_QUICK_FORMATIONS);

            INT nFormation = pGame->m_gameSave.m_quickFormations[nQuickFormation];
            INT nCurrentFormation = pGame->m_gameSave.m_curFormation;
            if (nFormation == -1) {
                pSettings->field_14.SetResRef(CResRef("STONFORM"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                nFormation = -1;
            } else if (nFormation < 10) {
                sForm.Format("FORM%d", nFormation);
                pSettings->field_14.SetResRef(CResRef(sForm), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else {
                sForm.Format("FORM%c", static_cast<char>(nFormation) + '7');
                pSettings->field_14.SetResRef(CResRef(sForm), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            }
            pSettings->field_1D8 = 0;
            pSettings->field_1CC = nFormation == nCurrentFormation;
            pControl->SetToolTipStrRef(4935, -1, -1);
            pControl->SetToolTipHotKey(static_cast<USHORT>(*pButtonType + 32), 0xFFFF, sKey);
            break;
        }
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            pSettings->field_1C8 = 0;
            pSettings->field_1CC = 0;
            pSettings->field_1D0 = 0;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1D8 = 0;
            pControl->SetToolTipStrRef(-1, -1, -1);
            break;
        case 33:
        case 34:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = *pButtonType == 33 ? 48 : 52;
            pSettings->field_10 = *pButtonType == 33 ? 48 : 52;
            pSettings->field_1C8 = 0;
            pSettings->field_1D0 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1CC = 0;
            pControl->SetToolTipStrRef(-1, -1, -1);
            break;
        case 35:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 96;
            pSettings->field_10 = 98;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4933, -1, -1);
            break;
        case 36:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            strTip = 4688;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 8;
            pSettings->field_10 = 10;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            if (pSprite->sub_724900() == FALSE) {
                pSettings->m_bGreyOut = TRUE;
                strTip = 37450;
            }
            pControl->SetToolTipStrRef(strTip, -1, -1);
            break;
        case 37:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 16;
            pSettings->field_10 = 18;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4978, -1, -1);
            break;
        case 38:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 112;
            pSettings->field_10 = 114;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4937, -1, -1);
            break;
        case 39:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 40;
            pSettings->field_10 = 42;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4954, -1, -1);
            break;
        case 40:
            pSettings->field_4 = 1;
            pSettings->field_0 = 1;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 20;
            pSettings->field_10 = 22;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(4918, -1, -1);
            break;
        case 41:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 116;
            pSettings->field_10 = 118;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(39723, -1, -1);
            break;
        case 42:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 120;
            pSettings->field_10 = 122;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(39724, -1, -1);
            break;
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57: {
            static const SHORT FRAMES[8] = { 56, 60, 64, 68, 72, 76, 80, 84 };
            static const BYTE CLASSES[8] = {
                CAIOBJECTTYPE_C_BARD,
                CAIOBJECTTYPE_C_CLERIC,
                CAIOBJECTTYPE_C_DRUID,
                CAIOBJECTTYPE_C_PALADIN,
                CAIOBJECTTYPE_C_RANGER,
                CAIOBJECTTYPE_C_SORCERER,
                CAIOBJECTTYPE_C_WIZARD,
                CAIOBJECTTYPE_C_CLERIC,
            };
            static const STRREF TIPS[8] = { 39709, 39710, 39711, 39712, 39713, 39714, 39715, 39940 };

            // NOTE: The original has eight copies of this case, one per
            // class tab (the last one is the cleric's domain).
            INT nTab = *pButtonType - 50;
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = FRAMES[nTab];
            pSettings->field_10 = FRAMES[nTab] + 2;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            nSpellClass = CLASSES[nTab];
            if (sub_595D60(nSpellClass) == FALSE || bClassSpells[nTab] == FALSE) {
                pSettings->m_bGreyOut = TRUE;
            }
            pControl->SetToolTipStrRef(TIPS[nTab], -1, -1);
            pControl->SetToolTipHotKey(11, 0xFFFF, sKey);
            break;
        }
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = 104;
            pSettings->field_10 = 106;
            pSettings->field_1C8 = 1;
            GetSelectedQuickWeaponData(selectedWeaponData);
            sub_587F80(static_cast<BYTE>(*pButtonType - 60), buttonData, 1);

            if (m_nState == 121 && buttonData.m_icon == "FIST") {
                CButtonData weaponData;

                sub_588300(static_cast<BYTE>(*pButtonType - 60), weaponData);
                if (weaponData.m_icon != "") {
                    buttonData = weaponData;
                }
            }

            if (buttonData.m_icon != "") {
                pSettings->field_14.SetResRef(buttonData.m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else if (*pButtonType == 61
                || *pButtonType == 63
                || *pButtonType == 65
                || *pButtonType == 67) {
                pSettings->field_14.SetResRef(CResRef("STONSHIL"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else {
                pSettings->field_14.SetResRef(CResRef("STONWEAP"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            }

            pSettings->field_EE.SetResRef(buttonData.m_launcherIcon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);

            if (buttonData.m_bDisplayCount) {
                pSettings->field_1D8 = buttonData.m_count;
            }

            if (pSprite->m_nWeaponSet == (*pButtonType - 60) / 2) {
                pSettings->field_1D0 = 1;
            } else {
                pSettings->field_1D0 = 0;
            }

            // The selected weapon's button shows the selected frame only
            // while a weapon button is the selected button (0x58D2C0 tests
            // m_nSelectedButton, not the button's type).
            if (buttonData.m_abilityId.m_abilityNum == selectedWeaponData.m_abilityId.m_abilityNum
                && buttonData.m_abilityId.m_itemNum == selectedWeaponData.m_abilityId.m_itemNum) {
                if (m_nSelectedButton == 60
                    || m_nSelectedButton == 61
                    || m_nSelectedButton == 62
                    || m_nSelectedButton == 63
                    || m_nSelectedButton == 64
                    || m_nSelectedButton == 65
                    || m_nSelectedButton == 66
                    || m_nSelectedButton == 67) {
                    pSettings->field_1CC = 1;
                }
            } else {
                pSettings->field_1CC = 0;
            }

            if (buttonData.m_name == -1) {
                pControl->SetToolTipStrRef(4950, -1, -1);
            } else {
                pControl->SetToolTipStrRef(buttonData.m_name, -1, -1);
            }

            if (buttonData.m_abilityId.m_abilityNum != -1) {
                pControl->SetToolTipHotKey(static_cast<USHORT>(*pButtonType - 19), 0xFFFF, sKey);
            }
            break;
        case 70:
        case 71:
        case 72:
        case 73:
        case 74:
        case 75:
        case 76:
        case 77:
        case 78: {
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            strSecondaryTip = 4688;
            pSettings->field_1C8 = 0;
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1D0 = 0;
            sub_587F80(static_cast<BYTE>(*pButtonType - 70), buttonData, 2);

            if (buttonData.m_icon != "") {
                pSettings->field_14.SetResRef(buttonData.m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else {
                pSettings->field_14.SetResRef(CResRef("STONSPEL"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            }

            if (buttonData.m_bDisplayCount) {
                pSettings->field_1D8 = buttonData.m_count;
            } else {
                pSettings->field_1D8 = 0;
            }

            BOOL bDisabled = FALSE;
            BOOLEAN bCanCast = FALSE;

            CSpell* pSpell = new CSpell(buttonData.m_abilityId.m_res);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
            // __LINE__: 2729
            UTIL_ASSERT(pSpell != NULL);

            if (pSpell->Demand()) {
                if (pSpell->pRes != NULL) {
                    SHORT nCasterType = pSpell->GetCasterType();
                    bDisabled = pSprite->GetDerivedStats()->m_disabledSpellTypes[nCasterType];
                }
                pSpell->Release();

                DWORD nSpecializationMask = pGame->GetRuleTables().GetSpecializationMask(buttonData.m_abilityId.m_nClass,
                    static_cast<BYTE>(buttonData.m_abilityId.field_1E));
                bCanCast = pSprite->CanCast(buttonData.m_abilityId.m_nClass,
                    nSpecializationMask,
                    pSpell);
            }

            if (pSpell != NULL) {
                delete pSpell;
            }

            STRREF strTip;
            if (bSpells && bDisabled != TRUE) {
                pSettings->m_bGreyOut = bCanCast != TRUE;
                strTip = strSecondaryTip;
            } else {
                pSettings->m_bGreyOut = TRUE;
                strTip = 37450;
            }

            if (pSettings->field_1D8 == 0) {
                pSettings->m_bGreyOut = TRUE;
            }

            if (buttonData.m_name == -1) {
                pControl->SetToolTipStrRef(strTip, -1, -1);
            } else {
                if (bCanCast) {
                    buttonData.m_abilityId.field_18 = -1;
                }
                pControl->SetToolTipStrRef(buttonData.m_name, -1, buttonData.m_abilityId.field_18);
            }

            USHORT nHotKey = 53;
            while (nHotKey < 372) {
                if (buttonData.m_abilityId.m_res == CString(off_8B4F84[nHotKey - 53])) {
                    break;
                }
                nHotKey++;
            }

            USHORT nHotKeyCount = 38;
            for (INT nPrev = nButton; nPrev != 0; nPrev--) {
                if (m_buttonTypes[nPrev - 1] >= 70
                    && m_buttonTypes[nPrev - 1] <= 78
                    && m_buttonArray[nPrev - 1].m_bGreyOut == FALSE) {
                    nHotKeyCount++;
                }
            }

            pControl->SetToolTipHotKey(nHotKey < 372 ? nHotKey : 0xFFFF, nHotKeyCount, sKey);
            break;
        }
        case 80:
        case 81:
        case 82:
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            strTip = 4978;
            pSettings->field_1C8 = 1;
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1D0 = 0;
            sub_587F80(static_cast<BYTE>(*pButtonType - 80), buttonData, 3);

            if (buttonData.m_icon != "") {
                pSettings->field_14.SetResRef(buttonData.m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else {
                pSettings->field_14.SetResRef(CResRef("STONITEM"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            }

            if (buttonData.m_bDisplayCount) {
                pSettings->field_1D8 = buttonData.m_count;
            } else {
                pSettings->field_1D8 = 0;
            }

            if (pSettings->field_1D8 < 1) {
                // NOTE: The original compares `field_1D4`, which nothing
                // ever sets.
                if (pSettings->field_1D4 == 24727) {
                    pSettings->m_bGreyOut = FALSE;
                } else {
                    pSettings->m_bGreyOut = TRUE;
                    strTip = 37449;
                }
            } else {
                pSettings->m_bGreyOut = FALSE;
            }

            if (buttonData.m_name != -1) {
                strTip = buttonData.m_name;
            }
            pControl->SetToolTipStrRef(strTip, -1, -1);
            pControl->SetToolTipHotKey(static_cast<USHORT>(*pButtonType - 35), 0xFFFF, sKey);

            if (pSprite->m_derivedStats.m_bPolymorphed == TRUE) {
                pSettings->m_bGreyOut = TRUE;
            }
            break;
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
        case 98:
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            strTip = 4954;
            pSettings->field_1C8 = 0;
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1D0 = 0;
            sub_587F80(static_cast<BYTE>(*pButtonType - 90), buttonData, 4);

            if (buttonData.m_icon != "") {
                pSettings->field_14.SetResRef(buttonData.m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else {
                pSettings->field_14.SetResRef(CResRef("STONSPEC"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            }

            if (buttonData.m_bDisplayCount) {
                if (buttonData.m_abilityId.m_res == CGameSprite::SPIN275) {
                    if (pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
                        buttonData.m_count = static_cast<SHORT>(pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK));
                        pSettings->field_1CC = buttonData.m_count > 0;
                        pSettings->field_1D8 = buttonData.m_count;
                    } else {
                        pSettings->field_1D8 = buttonData.m_count;
                    }
                } else if (buttonData.m_abilityId.m_res == CGameSprite::SPIN276
                    && pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
                    buttonData.m_count = static_cast<SHORT>(pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE));
                    pSettings->field_1CC = buttonData.m_count > 0;
                    pSettings->field_1D8 = buttonData.m_count;
                } else {
                    pSettings->field_1D8 = buttonData.m_count;
                }
            } else {
                if (buttonData.m_abilityId.m_res == CGameSprite::SPIN277) {
                    if (pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)) {
                        buttonData.m_count = static_cast<SHORT>(pSprite->sub_726270(CGAMESPRITE_FEAT_ARTERIAL_STRIKE));
                        pSettings->field_1CC = pSprite->sub_726270(CGAMESPRITE_FEAT_ARTERIAL_STRIKE) > 0;
                    }
                } else if (buttonData.m_abilityId.m_res == CGameSprite::SPIN278) {
                    if (pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)) {
                        buttonData.m_count = static_cast<SHORT>(pSprite->sub_726270(CGAMESPRITE_FEAT_HAMSTRING));
                        pSettings->field_1CC = pSprite->sub_726270(CGAMESPRITE_FEAT_HAMSTRING) > 0;
                    }
                } else if (buttonData.m_abilityId.m_res == CGameSprite::SPIN279
                    && pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)) {
                    buttonData.m_count = static_cast<SHORT>(pSprite->sub_726270(CGAMESPRITE_FEAT_RAPID_SHOT));
                    pSettings->field_1CC = pSprite->sub_726270(CGAMESPRITE_FEAT_RAPID_SHOT) > 0;
                }
                pSettings->field_1D8 = 0;
            }

            if (!bInnates || buttonData.m_bDisabled) {
                pSettings->m_bGreyOut = TRUE;
                strTip = 37443;
            }

            if (buttonData.m_name == -1) {
                pControl->SetToolTipStrRef(strTip, -1, -1);
            } else {
                pControl->SetToolTipStrRef(buttonData.m_name, -1, -1);
            }
            break;
        case 100:
            pSettings->field_0 = 0;
            pSettings->field_4 = 0;
            pSettings->field_8 = 0;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            pSettings->field_1C8 = 0;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pSettings->field_1CC = 0;
            pSettings->m_bGreyOut = TRUE;
            if (m_nState == 114) {
                pControl->SetToolTipStrRef(40976, -1, -1);
            } else {
                pControl->SetToolTipStrRef(-1, -1, -1);
            }
            break;
        case 110:
        case 111:
        case 112:
        case 113:
        case 114:
        case 115:
        case 116:
        case 117:
        case 118:
            pSettings->field_0 = 1;
            pSettings->field_4 = 1;
            pSettings->field_8 = 0;
            pSettings->field_C = 20;
            pSettings->field_10 = 22;
            pSettings->field_1C8 = 0;
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1D0 = 0;
            sub_587F80(static_cast<BYTE>(*pButtonType - 110), buttonData, 6);

            if (buttonData.m_icon != "") {
                pSettings->field_14.SetResRef(buttonData.m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            } else {
                pSettings->field_14.SetResRef(CResRef("STONSONG"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
            }

            if (!bSongs) {
                pSettings->m_bGreyOut = TRUE;
            }

            if (GetSelectedModalMode() == 1) {
                UINT nSongIndex = 0;
                if (pGame->GetMasterSongLookup().Find(buttonData.m_abilityId.m_res, nSongIndex) == TRUE
                    && nSongIndex == pSprite->GetLastSong()) {
                    pSettings->field_1CC = 1;
                }
            }

            if (buttonData.m_name == -1) {
                pControl->SetToolTipStrRef(37436, -1, -1);
            } else {
                pControl->SetToolTipStrRef(buttonData.m_name, -1, -1);
            }
            break;
        case 119:
            pSettings->field_0 = 1;
            pSettings->field_4 = 0;
            pSettings->field_8 = 1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_C = 92;
            pSettings->field_10 = 94;
            pSettings->field_1C8 = 1;
            pSettings->field_1D8 = 0;
            pSettings->field_1D0 = 0;
            pControl->SetToolTipStrRef(32186, -1, -1);
            pControl->SetToolTipHotKey(54, 0xFFFF, sKey);
            break;
        default:
            pSettings->field_0 = 0;
            pSettings->field_4 = 0;
            pSettings->field_8 = 0;
            pSettings->field_C = -1;
            pSettings->field_10 = -1;
            pSettings->field_14.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_EE.SetResRef(CResRef(""), FALSE, TRUE, TRUE);
            pSettings->field_1C8 = 0;
            pSettings->field_1CC = 0;
            pSettings->field_1D0 = 0;
            pSettings->field_1D8 = 0;
            pControl->SetToolTipStrRef(-1, -1, -1);
            break;
        }

        if (pSettings->field_0 != 0 && pSettings->m_bGreyOut == FALSE) {
            pSettings->m_bGreyOut = sub_5959A0(*pButtonType) == FALSE;
        }
    }

    switch (m_nState) {
    case 101:
        if (dword_8E6820 != NULL) {
            POSITION pos = dword_8E6820->GetHeadPosition();
            for (INT nButton = 1; nButton <= 12; nButton++) {
                CUIControlButton* pControl = static_cast<CUIControlButton*>(pPanel->GetControl(nButton + 5));
                if (pControl != NULL) {
                    sKey.Format("F%d", nButton);

                    CInfButtonSettings* pSettings = &(m_buttonArray[nButton - 1]);
                    if (pos == NULL) {
                        pSettings->field_0 = 0;
                    } else {
                        CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                        if (pButtonData != NULL) {
                            if (pButtonData->m_icon != "") {
                                pSettings->field_14.SetResRef(pButtonData->m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                            } else {
                                pSettings->field_14.SetResRef(CResRef("STONWEAP"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                            }

                            if (pButtonData->m_bDisplayCount) {
                                pSettings->field_1D8 = pButtonData->m_count;
                            }

                            if (pButtonData->m_abilityId.m_itemNum == pSprite->GetEquipment()->m_selectedWeapon
                                && pButtonData->m_abilityId.m_abilityNum == pSprite->GetEquipment()->m_selectedWeaponAbility) {
                                pSettings->field_1CC = 1;
                            } else {
                                pSettings->field_1CC = 0;
                            }

                            if (pButtonData->m_abilityId.field_10 == -1) {
                                pControl->SetToolTipStrRef(4950, -1, -1);
                            } else {
                                pControl->SetToolTipStrRef(pButtonData->m_abilityId.field_10, -1, -1);
                            }
                            pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
                        }
                        pSettings->field_0 = 1;
                    }
                }
            }
        }
        break;
    case 102:
    case 103:
    case 104:
    case 105:
    case 112:
    case 113:
    case 122:
        // NOTE: The original has a copy of this loop for each list; they
        // differ only in the empty icon, the grey out and the tool tip.
        if (dword_8E6820 != NULL) {
            INT nFirst;
            INT nLast;
            if (dword_8E6820->GetCount() > 12) {
                nFirst = 1;
                nLast = 10;
            } else {
                nFirst = 0;
                nLast = 11;
            }

            POSITION pos = dword_8E6820->FindIndex(field_197A);
            for (INT nButton = nFirst; nButton <= nLast; nButton++) {
                CUIControlButton* pControl = static_cast<CUIControlButton*>(pPanel->GetControl(nButton + 6));
                if (pControl == NULL) {
                    continue;
                }

                sKey.Format("F%d", nButton + 1);

                CInfButtonSettings* pSettings = &(m_buttonArray[nButton]);
                if (pos == NULL) {
                    pSettings->field_0 = 0;
                    continue;
                }

                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (pButtonData != NULL) {
                    const char* szEmpty;
                    switch (m_nState) {
                    case 104:
                    case 105:
                        szEmpty = "STONITEM";
                        break;
                    case 113:
                    case 122:
                        szEmpty = "STONSONG";
                        break;
                    default:
                        szEmpty = "STONSPEL";
                        break;
                    }

                    if (pButtonData->m_icon != "") {
                        pSettings->field_14.SetResRef(pButtonData->m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    } else {
                        pSettings->field_14.SetResRef(CResRef(szEmpty), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    }

                    if (pButtonData->m_bDisplayCount) {
                        pSettings->field_1D8 = pButtonData->m_count;
                    }

                    switch (m_nState) {
                    case 102:
                    case 103:
                        pSettings->m_bGreyOut = pButtonData->m_bDisabled;
                        if (pButtonData->m_name == -1) {
                            pControl->SetToolTipStrRef(4938, -1, -1);
                        } else {
                            pControl->SetToolTipStrRef(pButtonData->m_name, -1, pButtonData->m_abilityId.field_18);
                        }
                        break;
                    case 104:
                    case 105:
                        if (pButtonData->m_abilityId.field_10 == -1) {
                            pControl->SetToolTipStrRef(4950, -1, -1);
                        } else {
                            pControl->SetToolTipStrRef(pButtonData->m_abilityId.field_10, -1, -1);
                        }
                        break;
                    case 112:
                        pSettings->m_bGreyOut = pButtonData->m_bDisabled;
                        if (pButtonData->m_abilityId.field_10 == -1) {
                            pControl->SetToolTipStrRef(4938, -1, -1);
                        } else {
                            pControl->SetToolTipStrRef(pButtonData->m_abilityId.field_10, -1, -1);
                        }
                        break;
                    default:
                        pSettings->m_bGreyOut = pButtonData->m_bDisabled;
                        if (pButtonData->m_name == -1) {
                            pControl->SetToolTipStrRef(37436, -1, -1);
                        } else {
                            pControl->SetToolTipStrRef(pButtonData->m_name, -1, -1);
                        }
                        break;
                    }
                    pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
                }
                pSettings->field_0 = 1;
            }
        }
        break;
    case 106:
    case 107:
        if (dword_8E6820 != NULL) {
            INT nFirst;
            INT nLast;
            if (dword_8E6820->GetCount() > 12) {
                nFirst = 1;
                nLast = 10;
            } else {
                nFirst = 0;
                nLast = 11;
            }

            POSITION pos = dword_8E6820->FindIndex(field_197A);
            for (INT nButton = nFirst; nButton <= nLast; nButton++) {
                CUIControlButton* pControl = static_cast<CUIControlButton*>(pPanel->GetControl(nButton + 6));
                if (pControl == NULL) {
                    continue;
                }

                sKey.Format("F%d", nButton + 1);

                CInfButtonSettings* pSettings = &(m_buttonArray[nButton]);
                if (pos == NULL) {
                    pSettings->field_0 = 0;
                    continue;
                }

                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (pButtonData != NULL) {
                    const CResRef& cResRef = pButtonData->m_abilityId.m_res;
                    std::bitset<256>& spellStates = pSprite->GetDerivedStats()->m_spellStates;

                    // NOTE: The original tests the feat's spell states in
                    // order and stops at the first one set.
                    INT nResult = -1; // -1: untouched, 0: available, 1: active, 2: unavailable
                    if (cResRef == CGameSprite::SPIN275) {
                        if (pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
                            nResult = spellStates[SPLSTATE_FEAT_POWER_ATTACK_1]
                                    || spellStates[SPLSTATE_FEAT_POWER_ATTACK_2]
                                    || spellStates[SPLSTATE_FEAT_POWER_ATTACK_3]
                                    || spellStates[SPLSTATE_FEAT_POWER_ATTACK_4]
                                    || spellStates[SPLSTATE_FEAT_POWER_ATTACK_5]
                                ? 1
                                : 0;
                        } else {
                            nResult = 2;
                        }
                    } else if (cResRef == CGameSprite::SPIN276) {
                        if (pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
                            nResult = spellStates[SPLSTATE_FEAT_EXPERTISE_1]
                                    || spellStates[SPLSTATE_FEAT_EXPERTISE_2]
                                    || spellStates[SPLSTATE_FEAT_EXPERTISE_3]
                                    || spellStates[SPLSTATE_FEAT_EXPERTISE_4]
                                    || spellStates[SPLSTATE_FEAT_EXPERTISE_5]
                                ? 1
                                : 0;
                        } else {
                            nResult = 2;
                        }
                    } else if (cResRef == CGameSprite::SPIN277) {
                        if (pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)) {
                            nResult = spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE] ? 1 : 0;
                        } else {
                            nResult = 2;
                        }
                    } else if (cResRef == CGameSprite::SPIN278) {
                        if (pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)) {
                            nResult = spellStates[SPLSTATE_FEAT_HAMSTRING] ? 1 : 0;
                        } else {
                            nResult = 2;
                        }
                    } else if (cResRef == CGameSprite::SPIN279) {
                        if (pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)) {
                            nResult = spellStates[SPLSTATE_FEAT_RAPID_SHOT] ? 1 : 0;
                        } else {
                            nResult = 2;
                        }
                    }

                    switch (nResult) {
                    case 0:
                        pSettings->m_bGreyOut = FALSE;
                        break;
                    case 1:
                        pSettings->field_1CC = 1;
                        pSettings->m_bGreyOut = FALSE;
                        break;
                    case 2:
                        pSettings->field_1CC = 0;
                        pSettings->m_bGreyOut = TRUE;
                        break;
                    }

                    if (pButtonData->m_icon != "") {
                        pSettings->field_14.SetResRef(pButtonData->m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    } else {
                        pSettings->field_14.SetResRef(CResRef("STONSPEC"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    }

                    if (pButtonData->m_bDisplayCount) {
                        pSettings->field_1D8 = pButtonData->m_count;
                    }

                    if (pButtonData->m_abilityId.field_10 == -1) {
                        pControl->SetToolTipStrRef(37435, -1, -1);
                    } else {
                        pControl->SetToolTipStrRef(pButtonData->m_abilityId.field_10, -1, -1);
                    }
                    pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
                }
                pSettings->field_0 = 1;
            }
        }
        break;
    case 108:
    case 109:
        m_buttonArray[0].field_14.SetResRef(CResRef("FORM0"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[1].field_14.SetResRef(CResRef("FORM1"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[2].field_14.SetResRef(CResRef("FORM2"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[3].field_14.SetResRef(CResRef("FORM3"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[4].field_14.SetResRef(CResRef("FORM4"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[5].field_14.SetResRef(CResRef("FORM5"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[6].field_14.SetResRef(CResRef("FORM6"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[7].field_14.SetResRef(CResRef("FORM7"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[8].field_14.SetResRef(CResRef("FORM8"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[9].field_14.SetResRef(CResRef("FORM9"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[10].field_14.SetResRef(CResRef("FORMA"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        m_buttonArray[11].field_14.SetResRef(CResRef("FORMB"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        break;
    case 115:
        if (dword_8E6820 != NULL && dword_8E6820->GetCount() < 10) {
            POSITION pos = dword_8E6820->FindIndex(field_197A);
            for (INT nButton = 6; nButton <= 12; nButton++) {
                CUIControlButton* pControl = static_cast<CUIControlButton*>(pPanel->GetControl(nButton + 5));
                if (pControl == NULL) {
                    continue;
                }

                sKey.Format("F%d", nButton);

                CInfButtonSettings* pSettings = &(m_buttonArray[nButton - 1]);
                if (pos == NULL) {
                    pSettings->field_0 = 0;
                    continue;
                }

                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (pButtonData != NULL) {
                    if (pButtonData->m_icon != "") {
                        pSettings->field_14.SetResRef(pButtonData->m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    } else {
                        pSettings->field_14.SetResRef(CResRef("STONSPEL"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    }

                    if (pButtonData->m_bDisplayCount) {
                        pSettings->field_1D8 = pButtonData->m_count;
                    }

                    pSettings->m_bGreyOut = pButtonData->m_bDisabled;

                    if (pButtonData->m_name == -1) {
                        pControl->SetToolTipStrRef(37434, -1, -1);
                    } else {
                        pControl->SetToolTipStrRef(pButtonData->m_name, -1, -1);
                    }
                    pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
                }
                pSettings->field_0 = 1;
            }
        }
        break;
    case 123:
        if (dword_8E6820 != NULL) {
            INT nFirst;
            INT nLast;
            if (dword_8E6820->GetCount() > 12) {
                nFirst = 1;
                nLast = 10;
            } else {
                nFirst = 0;
                nLast = 11;
            }

            POSITION pos = dword_8E6820->FindIndex(field_197A);
            for (INT nButton = nFirst; nButton <= nLast; nButton++) {
                CUIControlButton* pControl = static_cast<CUIControlButton*>(pPanel->GetControl(nButton + 6));
                if (pControl == NULL) {
                    continue;
                }

                sKey.Format("F%d", nButton + 1);

                CInfButtonSettings* pSettings = &(m_buttonArray[nButton]);
                if (pos == NULL) {
                    pSettings->field_0 = 0;
                    continue;
                }

                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (pButtonData != NULL) {
                    const CResRef& cResRef = pButtonData->m_abilityId.m_res;
                    if ((cResRef == CGameSprite::SPIN275
                            && pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)
                            && pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK) > 0
                            && pButtonData->m_count == pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK))
                        || (cResRef == CGameSprite::SPIN276
                            && pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)
                            && pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE) > 0
                            && pButtonData->m_count == pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE))
                        || (cResRef == CGameSprite::SPIN277
                            && pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)
                            && pSprite->sub_726270(CGAMESPRITE_FEAT_ARTERIAL_STRIKE) > 0)
                        || (cResRef == CGameSprite::SPIN278
                            && pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)
                            && pSprite->sub_726270(CGAMESPRITE_FEAT_HAMSTRING) > 0)
                        || (cResRef == CGameSprite::SPIN279
                            && pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)
                            && pSprite->sub_726270(CGAMESPRITE_FEAT_RAPID_SHOT) > 0)) {
                        pSettings->field_1CC = 1;
                    }

                    if (pButtonData->m_icon != "") {
                        pSettings->field_14.SetResRef(pButtonData->m_icon, g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    } else {
                        pSettings->field_14.SetResRef(CResRef("STONSPEC"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
                    }

                    if (pButtonData->m_bDisplayCount) {
                        pSettings->field_1D8 = pButtonData->m_count;
                    }

                    if (pButtonData->m_abilityId.field_10 == -1) {
                        pControl->SetToolTipStrRef(37435, -1, -1);
                    } else {
                        pControl->SetToolTipStrRef(pButtonData->m_abilityId.field_10, -1, pButtonData->m_abilityId.field_18);
                    }
                    pControl->SetToolTipHotKey(0xFFFF, 0xFFFF, sKey);
                }
                pSettings->field_0 = 1;
            }
        }
        break;
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    // NOTE: Inlined `CScreenWorld::GetActivePanelID` (0x691130).
    if (g_pBaldurChitin->m_pEngineWorld->field_EA4 == -1) {
        pPanel->InvalidateRect(NULL);
    }

    return TRUE;
}

// 0x58FF20
BOOL CInfButtonArray::OnLButtonPressed(int buttonID)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: The original accepts 12 too, one past the last button; nothing
    // passes it (the action buttons are controls 6 to 17).
    if (buttonID < 0 || buttonID >= 12) {
        return FALSE;
    }

    CInfButtonSettings* pSettings = &(m_buttonArray[buttonID]);
    BOOL* pGreyOut = &(pSettings->m_bGreyOut);
    if (*pGreyOut) {
        if (m_nState == 114) {
            return TRUE;
        }

        INT nType = m_buttonTypes[buttonID];
        if (m_nState == 102) {
            if (nType < 21 || nType > 32) {
                return TRUE;
            }
        } else if ((nType < 50 || nType > 56)
            && (nType < 80 || nType > 82)
            && nType != 100) {
            return TRUE;
        }
    }

    pGame->m_lastClick.x = -1;
    pGame->m_lastClick.y = -1;
    pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;
    pGame->m_lastClick.x = -1;
    pGame->m_lastClick.y = -1;
    pGame->m_lastTarget = CGameObjectArray::INVALID_INDEX;

    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    INT* pButtonType = &(m_buttonTypes[buttonID]);
    INT nType;
    BOOLEAN bDone;
    BOOL bFlag;
    CResRef cResRef;
    ITEM_EFFECT effect;
    CGameEffect* pEffect;
    CMessage* pMessage;

    switch (m_nState) {
    case 101: {
        BOOLEAN bFound = FALSE;
        if (dword_8E6820 != NULL) {
            POSITION pos = dword_8E6820->GetHeadPosition();
            for (UINT nIndex = 0; nIndex < static_cast<UINT>(dword_8E6820->GetCount()); nIndex++) {
                if (pos != NULL) {
                    CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                    if (nIndex == static_cast<UINT>(buttonID) && pButtonData != NULL) {
                        bFound = TRUE;
                        sub_588CB0(pButtonData, static_cast<BYTE>(field_1976), 1);
                        sub_588570(static_cast<SHORT>(field_1976), 1);
                        break;
                    }
                }
            }
        }

        UpdateButtons();
        sub_587BD0();
        // NOTE: Inlined `m_stateStack` pop (as 0x589FF0).
        sub_589FF0(0, FALSE);

        if (!bFound) {
            m_nSelectedButton = 100;
        }
        break;
    }
    case 102:
    case 103:
    case 104:
    case 105:
    case 112:
    case 113:
    case 122:
        bFlag = FALSE;
        if (m_nState == 103 || m_nState == 105 || m_nState == 122 || m_nState == 112) {
            bFlag = TRUE;
        }

        bDone = FALSE;
        if (dword_8E6820 == NULL) {
            m_nSelectedButton = 100;
            UpdateButtons();
            break;
        }

        switch (*pButtonType) {
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32: {
            INT nIndex = dword_8E6820->GetCount() > 12 ? 1 : 0;
            POSITION pos = dword_8E6820->FindIndex(field_197A);
            while (pos != NULL) {
                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (nIndex == buttonID && pButtonData != NULL) {
                    BOOL bReady = TRUE;
                    if (m_nState == 103 && *pGreyOut == FALSE) {
                        CSpell cSpell;
                        cSpell.SetResRef(pButtonData->m_abilityId.m_res, TRUE, TRUE);
                        cSpell.Demand();

                        BYTE nClass = pButtonData->m_abilityId.m_nClass;
                        DWORD nSpecialization = pGame->GetRuleTables().GetSpecializationMask(nClass,
                            static_cast<BYTE>(pButtonData->m_abilityId.field_1E));
                        cSpell.Release();

                        if (!pSprite->CanCast(nClass, nSpecialization, &cSpell)) {
                            pSprite->FeedBack(CGameSprite::FEEDBACK_89,
                                0,
                                0,
                                0,
                                pGame->GetRuleTables().GetClassBeyondCastingAbilityStringRef(nClass),
                                0,
                                0);
                            bReady = FALSE;
                        } else if (g_pBaldurChitin->pActiveEngine->GetShiftKey() == TRUE
                            && bFlag == TRUE
                            && pButtonData->m_abilityId.m_nClass == CAIOBJECTTYPE_C_CLERIC
                            && pButtonData->m_abilityId.field_1E == 0
                            && !pButtonData->m_bDisabled) {
                            pSprite->sub_716770(pButtonData);
                            bReady = FALSE;
                        }
                    }

                    if (bReady) {
                        INT nCustomType = -1;
                        if (m_nState == 102) {
                            sub_588CB0(pButtonData, static_cast<BYTE>(field_1976), 2);
                            nCustomType = field_1976 + 70;
                        } else if (m_nState == 104) {
                            sub_588CB0(pButtonData, static_cast<BYTE>(field_1976), 3);
                            nCustomType = field_1976 + 80;
                        } else if (m_nState == 113) {
                            sub_588CB0(pButtonData, static_cast<BYTE>(field_1976), 6);
                            nCustomType = field_1976 + 110;
                        }

                        if (nCustomType != -1) {
                            m_customizableButtonTypes[field_1976] = nCustomType;
                            pSprite->sub_594120(static_cast<BYTE>(field_1976), nCustomType);
                            bDone = TRUE;
                        }

                        if (*pGreyOut == FALSE) {
                            if (m_nState == 102 || m_nState == 103) {
                                bDone = sub_5886A0(pButtonData, bFlag);
                            } else if (m_nState == 104 || m_nState == 105) {
                                bDone = sub_5884B0(pButtonData, bFlag);
                            } else if (m_nState == 113 || m_nState == 122) {
                                bDone = sub_588820(pButtonData, bFlag);
                            } else {
                                bDone = sub_588760(pButtonData, bFlag);
                            }
                        }
                    }
                    break;
                }
                nIndex++;
            }

            sub_587BD0();

            if (bDone) {
                if (field_19B2 != 0) {
                    // NOTE: Inlined `m_stateStack` unwind (as 0x589FF0).
                    sub_589FF0(0, TRUE);
                }
            } else if (field_19B2 != 0) {
                // NOTE: Inlined `m_stateStack` pop (as 0x589FF0).
                sub_589FF0(0, FALSE);
                m_nSelectedButton = 100;
                UpdateButtons();
            } else {
                m_nSelectedButton = 100;
                UpdateButtons();
            }
            break;
        }
        case 33:
            if (field_197A <= 0) {
                break;
            }

            if ((m_nState == 103 || m_nState == 102)
                && g_pBaldurChitin->pActiveEngine->GetShiftKey() == TRUE) {
                field_197A = sub_596040(dword_8E6820);
                UpdateButtons();
                break;
            }

            field_197A = max(field_197A - 10, 0);
            UpdateButtons();
            break;
        case 34:
            if (field_197A >= dword_8E6820->GetCount() - 10) {
                break;
            }

            if ((m_nState == 103 || m_nState == 102)
                && g_pBaldurChitin->pActiveEngine->GetShiftKey() == TRUE) {
                field_197A = sub_595FB0(dword_8E6820);
                UpdateButtons();
                break;
            }

            if (dword_8E6820->GetCount() - 10 >= field_197A + 10) {
                field_197A += 10;
            } else {
                field_197A = dword_8E6820->GetCount() - 10;
            }
            UpdateButtons();
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
            // __LINE__: 4454
            UTIL_ASSERT(FALSE);

            m_nSelectedButton = 100;
            UpdateButtons();
            break;
        }
        break;
    case 106:
    case 107:
        bFlag = m_nState == 106;
        bDone = FALSE;
        if (dword_8E6820 == NULL) {
            m_nSelectedButton = 100;
            UpdateButtons();
            break;
        }

        switch (*pButtonType) {
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32: {
            BOOLEAN bFeatList = FALSE;
            INT nIndex = dword_8E6820->GetCount() > 12 ? 1 : 0;
            POSITION pos = dword_8E6820->FindIndex(field_197A);
            while (pos != NULL) {
                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (nIndex == buttonID && pButtonData != NULL) {
                    if (m_nState == 107) {
                        sub_588CB0(pButtonData, static_cast<BYTE>(field_1976), 4);
                        m_customizableButtonTypes[field_1976] = field_1976 + 90;
                        pSprite->sub_594120(static_cast<BYTE>(field_1976), field_1976 + 90);
                    }

                    const CResRef& cAbility = pButtonData->m_abilityId.m_res;
                    if (m_nState == 106
                        && ((cAbility == CGameSprite::SPIN275 && pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK))
                            || (cAbility == CGameSprite::SPIN276 && pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)))) {
                        field_19E0 = cAbility;
                        bFeatList = TRUE;
                        bDone = TRUE;
                    } else if (m_nState == 106
                        && (cAbility == CGameSprite::SPIN277
                            || cAbility == CGameSprite::SPIN278
                            || cAbility == CGameSprite::SPIN279)) {
                        UINT nFeat;
                        WORD nEffect;
                        if (cAbility == CGameSprite::SPIN277) {
                            nFeat = CGAMESPRITE_FEAT_ARTERIAL_STRIKE;
                            nEffect = 455;
                        } else if (cAbility == CGameSprite::SPIN278) {
                            nFeat = CGAMESPRITE_FEAT_HAMSTRING;
                            nEffect = 456;
                        } else {
                            nFeat = CGAMESPRITE_FEAT_RAPID_SHOT;
                            nEffect = 457;
                        }

                        // NOTE: The original has a copy of this for each of
                        // the three feats.
                        pSprite->sub_726330(nFeat, pSprite->sub_726270(nFeat) > 0 ? 0 : 1);

                        CGameEffect::ClearItemEffect(&effect, nEffect);
                        effect.durationType = 1;
                        pEffect = CGameEffect::DecodeEffect(&effect,
                            pSprite->GetPos(),
                            pSprite->GetId(),
                            CPoint(-1, -1));
                        pMessage = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
                        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
                        bDone = TRUE;
                    } else if (m_nState == 107 || m_nState == 106) {
                        bDone = sub_5886A0(pButtonData, bFlag);
                    }
                    break;
                }
                nIndex++;
            }

            sub_587BD0();

            if (bDone) {
                if (bFeatList == TRUE) {
                    SetState(123, TRUE);
                } else {
                    sub_589FF0(0, TRUE);
                }
            } else {
                sub_589FF0(0, FALSE);
                m_nSelectedButton = 100;
                UpdateButtons();
            }
            break;
        }
        case 33:
            if (field_197A <= 0) {
                break;
            }
            field_197A = max(field_197A - 10, 0);
            UpdateButtons();
            break;
        case 34:
            if (field_197A >= dword_8E6820->GetCount() - 10) {
                break;
            }
            if (dword_8E6820->GetCount() - 10 < field_197A + 10) {
                field_197A = dword_8E6820->GetCount() - 10;
            } else {
                field_197A += 10;
            }
            UpdateButtons();
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
            // __LINE__: 4827
            UTIL_ASSERT(FALSE);

            SetSelectedButton(100);
            UpdateButtons();
            break;
        }
        break;
    case 108:
        if (buttonID < 12) {
            SHORT nQuickFormation = static_cast<SHORT>(field_1976);

            // __FILE__: .\Include\InfGame.h
            // __LINE__: 1499
            UTIL_ASSERT(nQuickFormation < CGAMESAVE_NUM_QUICK_FORMATIONS);

            pGame->m_gameSave.m_quickFormations[nQuickFormation] = static_cast<SHORT>(buttonID);

            nQuickFormation = static_cast<SHORT>(field_1976);

            // __FILE__: .\Include\InfGame.h
            // __LINE__: 1500
            UTIL_ASSERT(nQuickFormation < CGAMESAVE_NUM_QUICK_FORMATIONS);

            pGame->m_gameSave.m_curFormation = pGame->m_gameSave.m_quickFormations[nQuickFormation];

            sub_589FF0(0, FALSE);
        }
        break;
    case 109:
        if (buttonID < 12) {
            pGame->m_gameSave.m_curFormation = static_cast<SHORT>(buttonID);
            sub_589FF0(0, FALSE);
        }
        break;
    case 110:
        switch (*pButtonType) {
        case 6:
            SetState(109, FALSE);
            m_nSelectedButton = *pButtonType;
            UpdateButtons();
            break;
        case 7:
            if (pGame->m_nState != 3) {
                pGame->m_nState = 3;
                m_nSelectedButton = 7;
                UpdateButtons();
            } else {
                pGame->m_nState = 0;
                m_nSelectedButton = 100;
                UpdateButtons();
            }
            break;
        case 8:
            if (pGame->m_nState != 2 || pGame->m_iconIndex != 12) {
                pGame->m_nState = 2;
                pGame->m_iconIndex = 12;
                pGame->m_iconResRef = "";
                m_nSelectedButton = 8;
                UpdateButtons();
            } else {
                pGame->m_nState = 0;
                m_nSelectedButton = 100;
                UpdateButtons();
            }
            break;
        case 15:
            pGame->m_nState = 0;
            m_nSelectedButton = 100;
            pGame->GetGroup()->ClearActions();
            UpdateButtons();
            break;
        case 16:
        case 17:
        case 18:
        case 19:
        case 20: {
            SHORT nQuickFormation = static_cast<SHORT>(*pButtonType - 16);

            // __FILE__: .\Include\InfGame.h
            // __LINE__: 1500
            UTIL_ASSERT(nQuickFormation < CGAMESAVE_NUM_QUICK_FORMATIONS);

            pGame->m_gameSave.m_curFormation = pGame->m_gameSave.m_quickFormations[nQuickFormation];
            SetState(110, FALSE);
            UpdateButtons();
            break;
        }
        default:
            UpdateButtons();
            break;
        }
        break;
    case 115:
        nType = *pButtonType;
        switch (nType) {
        case 11:
            if (pSprite->m_nModalState == 3) {
                pSprite->SetModalState(0, FALSE);
                pSprite->m_nStealthGreyOut = dword_85BD1C;
                m_nSelectedButton = 100;

                CGameEffect::ClearItemEffect(&effect, 136);
                effect.durationType = 1;
                pEffect = CGameEffect::DecodeEffect(&effect,
                    pSprite->GetPos(),
                    pSprite->m_id,
                    CPoint(-1, -1));
                pMessage = new CMessageAddEffect(pEffect, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            } else {
                pSprite->SetModalState(3, FALSE);
                m_nSelectedButton = 5;

                CAIAction action(CAIACTION_HIDE, CPoint(-1, -1), 0, -1);
                pMessage = new CMessageAddAction(action, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }

            pGame->m_nState = 0;
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 4:
            if (pSprite->m_nModalState == 2) {
                pSprite->SetModalState(0, FALSE);
                m_nSelectedButton = 100;
            } else {
                pSprite->FeedBack(CGameSprite::FEEDBACK_SEARCHSTART, 0, 0, 0, -1, 0, 0);
                pSprite->SetModalState(2, FALSE);
                m_nSelectedButton = 5;
            }

            pGame->m_nState = 0;
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 12:
            if (pGame->m_nState == 2
                && (pGame->m_iconIndex == 36 || pGame->m_iconIndex == 40)) {
                pGame->m_nState = 0;
                m_nSelectedButton = 100;
            } else {
                pGame->m_nState = 2;
                pGame->m_iconIndex = 36;
                pGame->m_iconResRef = "";
                m_nSelectedButton = 12;
            }

            pSprite->SetModalState(0, FALSE);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 119:
            CGameEffect::ClearItemEffect(&effect, 286);
            effect.targetType = 1;
            pEffect = CGameEffect::DecodeEffect(&effect,
                CPoint(-1, -1),
                -1,
                CPoint(-1, -1));
            pEffect->SetSource(pSprite->GetPos());
            pEffect->sub_594020(FALSE);
            pEffect->SetSourceId(pSprite->m_id);
            pMessage = new CMessageAddEffect(pEffect, pSprite->m_id, pSprite->m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            m_nSelectedButton = 100;

            pGame->m_nState = 0;
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 13:
            if (pSprite->m_derivedStats.m_nSkills[1] > 0) {
                CButtonData buttonData;
                pSprite->sub_718390(CGameSprite::SPIN108, 0, 0, buttonData);
                sub_5886A0(&buttonData, TRUE);
            }

            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        default:
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        }
        break;
    case 116:
        nType = *pButtonType;
        switch (nType) {
        case 4:
        case 11:
        case 12:
        case 13:
        case 119:
            // NOTE: The original has a copy of this for each button type.
            m_customizableButtonTypes[field_1976] = nType;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), *pButtonType);
            pGame->m_nState = 0;
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, TRUE);
            break;
        default:
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        }
        break;
    case 117:
        switch (*pButtonType) {
        case 35:
            m_customizableButtonTypes[field_1976] = 5;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), 5);
            break;
        case 36:
            m_customizableButtonTypes[field_1976] = 3;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), 3);
            break;
        case 37:
            m_customizableButtonTypes[field_1976] = 14;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), 14);
            break;
        case 38:
            pGame->SetState(0);
            UpdateButtons();
            SetState(120, TRUE);
            pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return TRUE;
        case 39:
            m_customizableButtonTypes[field_1976] = 10;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), 10);
            break;
        case 40:
            m_customizableButtonTypes[field_1976] = 2;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), 2);
            break;
        case 41:
            m_customizableButtonTypes[field_1976] = 100;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), 100);
            break;
        case 42:
            for (BYTE nButton = 0; nButton < 9; nButton++) {
                pSprite->sub_594120(nButton, 0);
            }

            pSprite->ResetQuickSlots();

            for (BYTE nButton = 0; nButton < 9; nButton++) {
                m_customizableButtonTypes[nButton] = pSprite->sub_5940E0(nButton);
            }
            break;
        }

        UpdateButtons();
        sub_587BD0();
        sub_589FF0(0, FALSE);
        break;
    case 118:
        switch (*pButtonType) {
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
            switch (*pButtonType) {
            case 50:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_BARD;
                field_19DC = 0;
                break;
            case 51:
                field_19DC = 0;
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_CLERIC;
                break;
            case 57:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_CLERIC;
                field_19DC = pSprite->m_baseStats.m_specialization;
                break;
            case 52:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_DRUID;
                field_19DC = 0;
                break;
            case 53:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_PALADIN;
                field_19DC = 0;
                break;
            case 54:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_RANGER;
                field_19DC = 0;
                break;
            case 55:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_SORCERER;
                field_19DC = 0;
                break;
            case 56:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_WIZARD;
                field_19DC = 0;
                break;
            }

            pGame->m_nState = 0;
            UpdateButtons();
            SetState(103, TRUE);
            break;
        default:
            m_nCurrentSelectedSpellClass = 0;
            field_19DC = 0;
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        }
        break;
    case 119:
        nType = *pButtonType;
        switch (nType) {
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
            // NOTE: The original has a copy of this for each class tab.
            m_customizableButtonTypes[field_1976] = nType;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), nType);
            pGame->m_nState = 0;
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, TRUE);
            break;
        default:
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        }
        break;
    case 120:
        nType = *pButtonType;
        if (nType >= 80 && nType <= 82) {
            m_customizableButtonTypes[field_1976] = nType;
            pSprite->sub_594120(static_cast<BYTE>(field_1976), *pButtonType);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, TRUE);
        } else {
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
        }
        break;
    case 121:
        nType = *pButtonType;
        if (nType >= 60 && nType <= 67) {
            pSprite->sub_726810(static_cast<BYTE>((nType - 60) / 2));

            // NOTE: Inlined `UpdateSelectedWeaponSet` (0x595F70).
            UpdateSelectedWeaponSet(pSprite->m_nWeaponSet);
        }

        UpdateButtons();
        sub_587BD0();
        sub_589FF0(0, FALSE);
        break;
    case 123:
        if (dword_8E6820 == NULL) {
            SetSelectedButton(100);
            UpdateButtons();
            break;
        }

        switch (*pButtonType) {
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32: {
            INT nIndex = dword_8E6820->GetCount() > 12 ? 1 : 0;
            POSITION pos = dword_8E6820->FindIndex(field_197A);
            while (pos != NULL) {
                CButtonData* pButtonData = dword_8E6820->GetNext(pos);
                if (nIndex == buttonID && pButtonData != NULL) {
                    if (m_nState == 123) {
                        WORD nEffect = 0;
                        const CResRef& cAbility = pButtonData->m_abilityId.m_res;
                        if (cAbility == CGameSprite::SPIN275 && pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
                            nEffect = 453;
                            pSprite->sub_726330(CGAMESPRITE_FEAT_POWER_ATTACK, pButtonData->m_count);
                        } else if (cAbility == CGameSprite::SPIN276 && pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
                            nEffect = 454;
                            pSprite->sub_726330(CGAMESPRITE_FEAT_EXPERTISE, pButtonData->m_count);
                        }

                        if (pButtonData->m_count == 0) {
                            field_19E0 = CResRef();
                        }

                        // NOTE: The original applies the effect even when
                        // neither feat matched (opcode 0).
                        CGameEffect::ClearItemEffect(&effect, nEffect);
                        effect.durationType = 1;
                        pEffect = CGameEffect::DecodeEffect(&effect,
                            pSprite->GetPos(),
                            pSprite->GetId(),
                            CPoint(-1, -1));
                        pMessage = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
                        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
                    }
                    break;
                }
                nIndex++;
            }

            sub_587BD0();
            sub_589FF0(0, FALSE);
            SetSelectedButton(100);
            UpdateButtons();
            break;
        }
        case 33:
            if (field_197A <= 0) {
                break;
            }
            field_197A = max(field_197A - 10, 0);
            UpdateButtons();
            break;
        case 34:
            if (field_197A >= dword_8E6820->GetCount() - 10) {
                break;
            }
            if (dword_8E6820->GetCount() - 10 < field_197A + 10) {
                field_197A = dword_8E6820->GetCount() - 10;
            } else {
                field_197A += 10;
            }
            UpdateButtons();
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
            // __LINE__: 4952
            UTIL_ASSERT(FALSE);

            SetSelectedButton(100);
            UpdateButtons();
            break;
        }
        break;
    default:
        nType = *pButtonType;
        switch (nType) {
        case 2:
            if (pSprite->GetModalState() == 1) {
                pSprite->SetModalState(0, FALSE);
                SetSelectedButton(100);
                UpdateButtons();
            } else {
                pGame->SetState(0);
                UpdateButtons();
                SetState(122, TRUE);
            }
            break;
        case 3:
            sub_594280(3, pSprite);
            break;
        case 4:
            if (pSprite->GetModalState() == 2) {
                pSprite->SetModalState(0, FALSE);
                SetSelectedButton(100);
            } else {
                pSprite->FeedBack(CGameSprite::FEEDBACK_SEARCHSTART, 0, 0, 0, -1, 0, 0);
                pSprite->SetModalState(2, FALSE);
                SetSelectedButton(5);
            }
            pGame->SetState(0);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 5:
            sub_594280(5, pSprite);
            break;
        case 7:
            if (pGame->GetState() == 3) {
                pGame->SetState(0);
                SetSelectedButton(100);
            } else {
                pGame->SetState(3);
                SetSelectedButton(7);
            }
            pSprite->SetModalState(0, FALSE);
            UpdateButtons();
            break;
        case 8:
            if (pGame->GetState() == 2 && pGame->GetIconIndex() == 12) {
                pGame->SetState(0);
                SetSelectedButton(100);
            } else {
                pGame->SetState(2);
                pGame->SetIconIndex(12);
                SetSelectedButton(8);
            }
            pSprite->SetModalState(0, FALSE);
            UpdateButtons();
            break;
        case 10:
            sub_594280(10, pSprite);
            break;
        case 11:
            if (pSprite->GetModalState() == 3) {
                pSprite->SetModalState(0, FALSE);
                pSprite->SetStealthGreyOut(dword_85BD1C);
                SetSelectedButton(100);

                CGameEffect::ClearItemEffect(&effect, 136);
                effect.durationType = 1;
                pEffect = CGameEffect::DecodeEffect(&effect,
                    pSprite->GetPos(),
                    pSprite->GetId(),
                    CPoint(-1, -1));
                pMessage = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            } else {
                pSprite->SetModalState(3, FALSE);
                SetSelectedButton(5);

                CAIAction action(CAIACTION_HIDE, CPoint(-1, -1), 0, -1);
                pMessage = new CMessageAddAction(action, pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }

            pGame->SetState(0);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 12:
            if (pGame->GetState() == 2
                && (pGame->GetIconIndex() == 36 || pGame->GetIconIndex() == 40)) {
                pGame->SetState(0);
                SetSelectedButton(100);
            } else {
                pGame->SetState(2);
                pGame->SetIconIndex(36);
                SetSelectedButton(12);
            }
            pSprite->SetModalState(0, FALSE);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 13:
            if (pSprite->GetDerivedStats()->m_nSkills[1] > 0) {
                CButtonData buttonData;
                pSprite->sub_718390(CGameSprite::SPIN108, 0, 0, buttonData);
                sub_5886A0(&buttonData, TRUE);
            }
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        case 14:
            sub_594280(14, pSprite);
            break;
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
            switch (nType) {
            case 50:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_BARD;
                field_19DC = 0;
                break;
            case 51:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_CLERIC;
                field_19DC = 0;
                break;
            case 57:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_CLERIC;
                field_19DC = pSprite->sub_5940D0();
                break;
            case 52:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_DRUID;
                field_19DC = 0;
                break;
            case 53:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_PALADIN;
                field_19DC = 0;
                break;
            case 54:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_RANGER;
                field_19DC = 0;
                break;
            case 55:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_SORCERER;
                field_19DC = 0;
                break;
            case 56:
                m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_WIZARD;
                field_19DC = 0;
                break;
            }

            pGame->SetState(0);
            UpdateButtons();
            SetState(103, TRUE);
            break;
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
            if (g_pBaldurChitin->GetActiveEngine()->GetShiftKey() == TRUE) {
                pGame->SetState(0);
                SetSelectedButton(100);
                field_1976 = *pButtonType - 60;
                UpdateButtons();
                SetState(101, TRUE);
                break;
            }

            nType = *pButtonType;
            if (nType == 61 || nType == 63 || nType == 65 || nType == 67) {
                CItem* pItem = pSprite->GetEquipment()->m_items[*pButtonType - 17];
                if (pItem == NULL
                    || pItem->GetItemType() == 47
                    || pItem->GetItemType() == 53
                    || pItem->GetItemType() == 49
                    || pItem->GetItemType() == 41) {
                    nType--;
                }
            }

            if (m_nSelectedButton == nType) {
                pGame->SetState(0);
                SetSelectedButton(100);
                UpdateButtons();
                break;
            }

            pGame->SetState(0);
            pSprite->SetModalState(0, FALSE);
            SetSelectedButton(nType);
            sub_588570(static_cast<SHORT>(nType - 60), 1);
            if (pGame->GetState() == 0) {
                SetSelectedButton(100);
            }
            UpdateButtons();
            break;
        case 70:
        case 71:
        case 72:
        case 73:
        case 74:
        case 75:
        case 76:
        case 77:
        case 78: {
            if (m_nSelectedButton == nType) {
                pGame->SetState(0);
                SetSelectedButton(100);
                UpdateButtons();
                break;
            }

            CButtonData* pButtonData = new CButtonData();
            sub_587F80(static_cast<BYTE>(*pButtonType - 70), *pButtonData, 2);

            CSpell cSpell(pButtonData->m_abilityId.m_res);
            cSpell.Demand();

            BYTE nClass = pButtonData->m_abilityId.m_nClass;
            DWORD nSpecialization = pGame->GetRuleTables().GetSpecializationMask(nClass,
                static_cast<BYTE>(pButtonData->m_abilityId.field_1E));
            cSpell.Release();

            if (!pSprite->CanCast(nClass, nSpecialization, &cSpell)) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_89,
                    0,
                    0,
                    0,
                    pGame->GetRuleTables().GetClassBeyondCastingAbilityStringRef(nClass),
                    0,
                    0);

                // NOTE: The original leaks `pButtonData` here.
                delete pButtonData;
                break;
            }

            if (m_nState == 114
                && pButtonData->m_abilityId.m_nClass == CAIOBJECTTYPE_C_CLERIC
                && pButtonData->m_abilityId.field_1E == 0
                && g_pBaldurChitin->GetActiveEngine()->GetShiftKey() == TRUE
                && !pButtonData->m_bDisabled) {
                pSprite->sub_716770(pButtonData);
            } else {
                pGame->SetState(0);
                SetSelectedButton(*pButtonType);
                pSprite->SetModalState(0, FALSE);
                UpdateButtons();
                if (pSettings->field_1D8 > 0) {
                    sub_588570(static_cast<SHORT>(*pButtonType - 70), 2);
                }
                if (pGame->GetState() == 0) {
                    SetSelectedButton(100);
                }
            }

            delete pButtonData;
            break;
        }
        case 80:
        case 81:
        case 82:
            sub_594280(nType, pSprite);
            break;
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
        case 98: {
            if (m_nSelectedButton == nType) {
                pGame->SetState(0);
                SetSelectedButton(100);
                UpdateButtons();
                break;
            }

            pGame->SetState(0);
            SetSelectedButton(*pButtonType);
            pSprite->SetModalState(0, FALSE);

            CButtonData* pButtonData = new CButtonData();
            sub_587F80(static_cast<BYTE>(*pButtonType - 90), *pButtonData, 4);

            const CResRef& cAbility = pButtonData->m_abilityId.m_res;
            if ((cAbility == CGameSprite::SPIN275 && pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK))
                || (cAbility == CGameSprite::SPIN276 && pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE))) {
                field_19E0 = cAbility;
                SetState(123, TRUE);
                delete pButtonData;
                break;
            }

            BOOL bReady = pSettings->field_1D8 > 0;

            UINT nFeat = 0;
            WORD nEffect = 0;
            if (cAbility == CGameSprite::SPIN277) {
                nFeat = CGAMESPRITE_FEAT_ARTERIAL_STRIKE;
                nEffect = 455;
            } else if (cAbility == CGameSprite::SPIN278) {
                nFeat = CGAMESPRITE_FEAT_HAMSTRING;
                nEffect = 456;
            } else if (cAbility == CGameSprite::SPIN279) {
                nFeat = CGAMESPRITE_FEAT_RAPID_SHOT;
                nEffect = 457;
            }

            if (nEffect != 0) {
                // NOTE: The original has a copy of this for each of the
                // three feats.
                pSprite->sub_726330(nFeat, pSprite->sub_726270(nFeat) > 0 ? 0 : 1);

                CGameEffect::ClearItemEffect(&effect, nEffect);
                effect.durationType = 1;
                pEffect = CGameEffect::DecodeEffect(&effect,
                    pSprite->GetPos(),
                    pSprite->GetId(),
                    CPoint(-1, -1));
                pMessage = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }

            if (bReady == TRUE) {
                sub_588570(static_cast<SHORT>(*pButtonType - 90), 4);
            }

            if (pGame->GetState() == 0) {
                SetSelectedButton(100);
            }

            delete pButtonData;
            break;
        }
        case 110:
        case 111:
        case 112:
        case 113:
        case 114:
        case 115:
        case 116:
        case 117:
        case 118: {
            if (pSprite->GetModalState() == 1) {
                pSprite->SetModalState(0, FALSE);
                SetSelectedButton(100);
                pGame->SetState(0);
                UpdateButtons();
                sub_587BD0();
                sub_589FF0(0, FALSE);
                break;
            }

            CButtonData* pButtonData = new CButtonData();
            sub_587F80(static_cast<BYTE>(*pButtonType - 110), *pButtonData, 6);
            sub_588820(pButtonData, TRUE);
            delete pButtonData;

            pGame->SetState(0);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        }
        case 119:
            CGameEffect::ClearItemEffect(&effect, 286);
            effect.targetType = 1;
            pEffect = CGameEffect::DecodeEffect(&effect,
                CPoint(-1, -1),
                -1,
                CPoint(-1, -1));
            pEffect->SetSource(pSprite->GetPos());
            pEffect->SetSourceId(pSprite->GetId());
            pEffect->sub_594020(FALSE);
            pMessage = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            SetSelectedButton(100);
            pGame->SetState(0);
            UpdateButtons();
            sub_587BD0();
            sub_589FF0(0, FALSE);
            break;
        }
        break;
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x594170
BOOL CInfButtonArray::sub_594170(INT nButtonType)
{
    if (m_nState != 114) {
        return FALSE;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    switch (nButtonType) {
    case 3:
    case 5:
    case 10:
    case 14:
    case 80:
    case 81:
    case 82:
        sub_594280(nButtonType, pSprite);
        break;
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x594280
void CInfButtonArray::sub_594280(INT nButtonType, CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfButtonArray.cpp
    // __LINE__: 5711
    UTIL_ASSERT(pSprite != NULL);

    switch (nButtonType) {
    case 3: {
        pGame->m_nState = 0;
        UpdateButtons();

        BYTE nClass = 0;
        UINT nCount = 0;
        for (UINT nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
            if (nCount > 1) {
                break;
            }

            CGameSpriteGroupedSpellList* pSpells = pSprite->m_spells.Get(nClassIndex);
            for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++) {
                if (pSpells->GetSpellsAtLevel(nLevel)->field_10 != 0) {
                    nClass = pGame->GetSpellcasterClass(nClassIndex);
                    nCount++;
                    break;
                }
            }
        }

        for (UINT nLevel = 0; nLevel < pSprite->m_domainSpells.m_nHighestLevel; nLevel++) {
            // __FILE__: .\Include\FileFormat.h
            // __LINE__: 2521
            UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

            if (pSprite->m_domainSpells.m_lists[nLevel].field_10 != 0) {
                if (nCount == 0) {
                    nClass = CAIOBJECTTYPE_C_CLERIC;
                    field_19DC = pSprite->m_baseStats.m_specialization;
                }
                nCount++;
                break;
            }
            field_19DC = 0;
        }

        if (nCount > 1) {
            SetState(118, TRUE);
            return;
        }

        if (nCount == 1) {
            m_nCurrentSelectedSpellClass = nClass;
            SetState(103, TRUE);
            return;
        }
        break;
    }
    case 5:
        if (pSprite->m_nModalState != 0) {
            if (pSprite->m_nModalState == 3) {
                pSprite->SetModalState(0, FALSE);
                pSprite->m_nStealthGreyOut = dword_85BD1C;
            } else {
                pSprite->SetModalState(0, FALSE);
            }
            m_nSelectedButton = 100;
            UpdateButtons();
            return;
        }

        pGame->m_nState = 0;
        UpdateButtons();
        SetState(115, TRUE);
        return;
    case 10:
        pGame->m_nState = 0;
        UpdateButtons();
        SetState(106, TRUE);
        return;
    case 14:
        pGame->m_nState = 0;
        m_nSelectedButton = 14;
        pSprite->SetModalState(0, FALSE);
        UpdateButtons();
        SetState(105, TRUE);
        return;
    case 80:
    case 81:
    case 82: {
        CGameButtonList* pList = pSprite->GetItemUsages(static_cast<SHORT>(nButtonType - 65), 3, -1);
        if (pList == NULL || pList->GetCount() < 2) {
            INT nSelectedButton = m_nSelectedButton;
            pGame->m_nState = 0;
            if (nSelectedButton == nButtonType) {
                m_nSelectedButton = 100;
                UpdateButtons();
            } else {
                m_nSelectedButton = nButtonType;
                pSprite->SetModalState(0, FALSE);
                sub_588570(static_cast<SHORT>(nButtonType - 80), 3);
                if (pGame->m_nState == 0) {
                    m_nSelectedButton = 100;
                }
            }
        } else {
            m_nSelectedButton = nButtonType;
            field_1976 = nButtonType - 80;
            SetState(104, TRUE);
        }

        if (pList != NULL) {
            while (pList->GetCount() != 0) {
                delete pList->RemoveHead();
            }
            pList->RemoveAll();
            delete pList;
        }
        break;
    }
    }
}

// 0x594720
BOOL CInfButtonArray::OnRButtonPressed(int buttonID)
{
    // NOTE: The original accepts 12 too, one past the last button; nothing
    // passes it (the action buttons are controls 6 to 17).
    if (buttonID < 0 || buttonID >= 12) {
        return FALSE;
    }

    // NOTE: The original's test for a greyed out button that only quick
    // spells, items, abilities and songs pass is written with `&&` where it
    // needs `||`; no button type satisfies it, so every greyed out button
    // goes on.
    if (m_buttonArray[buttonID].m_bGreyOut) {
        INT nType = m_buttonTypes[buttonID];
        if ((nType < 70 && nType > 78)
            || (nType < 80 && nType > 82)
            || (nType < 90 && nType > 98)
            || (nType < 110 && nType > 118)) {
            return TRUE;
        }
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    LONG nCharacterId = pGame->GetGroup()->GetGroupLeader();

    // NOTE: The original goes on whether the share succeeds or not, with
    // `pSprite` left unset on failure; ours stops there.
    CGameSprite* pSprite = NULL;
    if (pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE)
        != CGameObjectArray::SUCCESS) {
        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return TRUE;
    }

    INT nNewState = -1;

    switch (m_nState) {
    case 110:
        if (m_buttonTypes[buttonID] >= 16 && m_buttonTypes[buttonID] <= 20) {
            field_1976 = m_buttonTypes[buttonID] - 16;
            nNewState = 108;
        }
        break;
    case 114:
        switch (m_buttonTypes[buttonID]) {
        case 2:
        case 3:
        case 4:
        case 5:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
        case 70:
        case 71:
        case 72:
        case 73:
        case 74:
        case 75:
        case 76:
        case 77:
        case 78:
        case 80:
        case 81:
        case 82:
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
        case 98:
        case 100:
        case 110:
        case 111:
        case 112:
        case 113:
        case 114:
        case 115:
        case 116:
        case 117:
        case 118:
        case 119:
            field_1976 = buttonID - 3;
            nNewState = 117;
            break;
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
            if (pSprite->sub_737910(FALSE) != TRUE) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_ITEMCURSED, 0, 0, 0, -1, 0, 0);
                break;
            }

            if (pSprite->m_equipment.m_items[42] != NULL) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_MAGICALWEAPONINUSE, 0, 0, 0, -1, 0, 0);
                break;
            }

            field_1976 = m_buttonTypes[buttonID] - 60;
            nNewState = 121;
            break;
        }
        break;
    case 117:
        switch (m_buttonTypes[buttonID]) {
        case 35:
            nNewState = 116;
            break;
        case 36: {
            BYTE nClass = 0;
            UINT nCount = 0;
            for (UINT nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
                if (nCount > 1) {
                    break;
                }

                CGameSpriteGroupedSpellList* pSpells = &(pSprite->m_spells.m_spellsByClass[nClassIndex]);
                for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++) {
                    if (pSpells->GetSpellsAtLevel(nLevel)->m_List.size() != 0) {
                        nClass = pGame->GetSpellcasterClass(nClassIndex);
                        nCount++;
                        break;
                    }
                }
            }

            for (UINT nLevel = 0; nLevel < pSprite->m_domainSpells.m_nHighestLevel; nLevel++) {
                // __FILE__: .\Include\FileFormat.h
                // __LINE__: 2521
                UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

                if (pSprite->m_domainSpells.m_lists[nLevel].m_List.size() != 0) {
                    if (nCount == 0) {
                        nClass = CAIOBJECTTYPE_C_CLERIC;
                        field_19DC = pSprite->m_baseStats.m_specialization;
                    }
                    nCount++;
                    break;
                }
                field_19DC = 0;
            }

            if (nCount >= 2) {
                nNewState = 119;
            } else if (nCount == 1) {
                if (pSprite->GetSpells(nClass)->sub_725F00() != 0) {
                    m_nCurrentSelectedSpellClass = nClass;
                    nNewState = 102;
                }
            }
            break;
        }
        case 38:
            nNewState = 120;
            break;
        case 39:
            if (pSprite->m_innateSpells.field_10 != 0) {
                nNewState = 107;
            }
            break;
        case 40:
            if (pSprite->m_songs.m_List.size() != 0) {
                nNewState = 113;
            }
            break;
        }
        break;
    case 119: {
        BOOLEAN bSpells = FALSE;
        BOOLEAN bFound = FALSE;
        BYTE nClass;
        UINT nLevel;

        switch (m_buttonTypes[buttonID]) {
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
            // NOTE: The original has a copy of this for each class tab; the
            // bard and the sorcerer count the known spells (`field_18`),
            // the others the memorized ones (`field_10`).
            switch (m_buttonTypes[buttonID]) {
            case 50:
                nClass = CAIOBJECTTYPE_C_BARD;
                break;
            case 51:
                nClass = CAIOBJECTTYPE_C_CLERIC;
                break;
            case 52:
                nClass = CAIOBJECTTYPE_C_DRUID;
                break;
            case 53:
                nClass = CAIOBJECTTYPE_C_PALADIN;
                break;
            case 54:
                nClass = CAIOBJECTTYPE_C_RANGER;
                break;
            case 55:
                nClass = CAIOBJECTTYPE_C_SORCERER;
                break;
            default:
                nClass = CAIOBJECTTYPE_C_WIZARD;
                break;
            }

            m_nCurrentSelectedSpellClass = nClass;
            field_19DC = 0;

            nLevel = 0;
            if (nLevel >= pSprite->GetSpells(nClass)->m_nHighestLevel) {
                break;
            }

            do {
                bSpells = TRUE;
                if (bFound) {
                    break;
                }

                CGameSpriteSpellList* pList = pSprite->GetSpellsAtLevel(nClass, nLevel);
                if (nClass == CAIOBJECTTYPE_C_BARD || nClass == CAIOBJECTTYPE_C_SORCERER) {
                    bFound = pList->field_18 != 0;
                } else {
                    bFound = pList->field_10 != 0;
                }
                nLevel++;
                bSpells = bFound;
            } while (nLevel < pSprite->GetSpells(nClass)->m_nHighestLevel);

            if (bSpells) {
                nNewState = 102;
            }
            break;
        case 57:
            m_nCurrentSelectedSpellClass = CAIOBJECTTYPE_C_CLERIC;
            field_19DC = pSprite->m_baseStats.m_specialization;

            nLevel = 0;
            if (pSprite->m_domainSpells.m_nHighestLevel == 0) {
                break;
            }

            do {
                bSpells = TRUE;
                if (bFound) {
                    break;
                }

                // __FILE__: .\Include\FileFormat.h
                // __LINE__: 2521
                UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

                bFound = pSprite->m_domainSpells.m_lists[nLevel].field_10 != 0;
                nLevel++;
                bSpells = bFound;
            } while (nLevel < pSprite->m_domainSpells.m_nHighestLevel);

            if (bSpells) {
                nNewState = 102;
            }
            break;
        }
        break;
    }
    }

    if (nNewState != -1) {
        SetState(nNewState, TRUE);
    }

    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x5950F0
BOOL CInfButtonArray::sub_5950F0(const CPoint& pt, const CRect& rClip, BOOL bPressed, INT nButton)
{
    CVidCell vcHighlight;
    CResRef cResRef;
    DWORD dwFlags = 0;

    CSize size;
    size.cx = (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * CIcon::ICON_SIZE_SM.cx;
    size.cy = (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * CIcon::ICON_SIZE_SM.cy;

    if (nButton < 0 || nButton >= 12) {
        return TRUE;
    }

    CInfButtonSettings* pSettings = &(m_buttonArray[nButton]);
    DWORD dwHighlightFlags = pSettings->m_bGreyOut ? 0x80000 : 0;

    if (g_pBaldurChitin->pActiveEngine == NULL
        || g_pBaldurChitin->pActiveEngine->pVidMode == NULL) {
        return FALSE;
    }

    CPoint ptIcon;
    ptIcon.x = (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * 3 + pt.x;
    ptIcon.y = (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * 3 + pt.y;
    if (bPressed) {
        ptIcon.x += (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * 2;
        ptIcon.y += (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * 2;
    }

    CRect rIcon(ptIcon, size);
    CRect rIconClip;
    rIconClip.IntersectRect(rIcon, rClip);

    if (pSettings->field_0 == 0) {
        return TRUE;
    }

    if (pSettings->field_4 == 0) {
        return FALSE;
    }

    if (pSettings->field_1D0 != 0 && pSettings->field_1CC == 0) {
        vcHighlight.SetResRef(CResRef("HIGHLGHT"), g_pBaldurChitin->field_4A2C, TRUE, TRUE);
        vcHighlight.Render(0, ptIcon.x, ptIcon.y, rIconClip, NULL, 0, dwHighlightFlags, -1);
    }

    if (pSettings->m_bGreyOut) {
        dwFlags = dword_84CEB8;
    }

    CIcon::RenderIcon(0,
        ptIcon,
        size,
        rIconClip,
        pSettings->field_EE.GetResRef(),
        g_pBaldurChitin->field_4A2C,
        dwFlags,
        0,
        FALSE,
        0,
        FALSE,
        0);

    switch (m_buttonTypes[nButton]) {
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
    case 98: {
        BOOL bForceCount;
        if (pSettings->field_14.GetResRef() == "STONSPEL"
            || pSettings->field_14.GetResRef() == "STONSPEC") {
            bForceCount = TRUE;
        } else {
            bForceCount = FALSE;
        }

        CIcon::RenderIcon(0,
            ptIcon,
            size,
            rIconClip,
            pSettings->field_14.GetResRef(),
            g_pBaldurChitin->field_4A2C,
            dwFlags,
            static_cast<WORD>(pSettings->field_1D8),
            bForceCount,
            0,
            FALSE,
            0);
        break;
    }
    case 80:
    case 81:
    case 82: {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        SHORT nPortrait = static_cast<SHORT>(g_pBaldurChitin->m_pEngineWorld->GetSelectedCharacter());

        LONG nCharacterId;
        if (nPortrait < pGame->m_nCharacters) {
            nCharacterId = pGame->m_characterPortraits[nPortrait];
        } else {
            nCharacterId = CGameObjectArray::INVALID_INDEX;
        }

        BOOL bTint = FALSE;
        CGameSprite* pSprite = NULL;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        INT nUsable = 1;
        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite != NULL) {
                CItem* pItem = pSprite->GetQuickItem(static_cast<BYTE>(m_buttonTypes[nButton] - 80));
                if (pItem != NULL) {
                    nUsable = pGame->CheckItemUsable(pSprite, pItem);
                    bTint = nUsable == 2;
                }
            }

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        CIcon::RenderIcon(0,
            ptIcon,
            size,
            rIconClip,
            pSettings->field_14.GetResRef(),
            g_pBaldurChitin->field_4A2C,
            dwFlags,
            static_cast<WORD>(pSettings->field_1D8),
            FALSE,
            0,
            FALSE,
            bTint == TRUE ? dword_84EBB0 : 0);

        if (nUsable == 2) {
            CVidCell vcTint(CResRef("STORTIN4"),
                g_pBaldurChitin->m_pEngineWorld->GetManager()->m_bDoubleSize);
            vcTint.Render(0, ptIcon.x, ptIcon.y, rIconClip, NULL, 0, 2, 192);
        } else if (nUsable == 0) {
            CVidCell vcTint(CResRef("STORTINT"),
                g_pBaldurChitin->m_pEngineWorld->GetManager()->m_bDoubleSize);
            vcTint.Render(0, ptIcon.x, ptIcon.y, rIconClip, NULL, 0, 2, 192);
        }
        break;
    }
    default:
        CIcon::RenderIcon(0,
            ptIcon,
            size,
            rIconClip,
            pSettings->field_14.GetResRef(),
            g_pBaldurChitin->field_4A2C,
            dwFlags,
            static_cast<WORD>(pSettings->field_1D8),
            FALSE,
            0,
            FALSE,
            0);
        break;
    }

    return TRUE;
}

// 0x5957C0
BOOL CInfButtonArray::sub_5957C0(const CPoint& pt, const CRect& rClip, BOOL bPressed, INT nButton)
{
    if (nButton < 0 || nButton >= 12) {
        return TRUE;
    }

    CInfButtonSettings* pSettings = &(m_buttonArray[nButton]);

    DWORD dwFlags;
    if (pSettings->m_bGreyOut) {
        field_17C2.SetTintColor(CUIControlButton::TINT_DISABLED);
        field_16E8.SetTintColor(CUIControlButton::TINT_DISABLED);
        dwFlags = 0xA0000;
    } else {
        dwFlags = 0;
    }

    if (g_pBaldurChitin->pActiveEngine == NULL
        || g_pBaldurChitin->pActiveEngine->pVidMode == NULL) {
        return FALSE;
    }

    INT nSize = (g_pBaldurChitin->field_4A2C != FALSE ? 2 : 1) * 38;
    CRect rButton(pt.x, pt.y, pt.x + nSize, pt.y + nSize);
    CRect rButtonClip;
    rButtonClip.IntersectRect(rButton, rClip);

    if (pSettings->field_0 == 0) {
        return TRUE;
    }

    if (pSettings->field_8 == 0 || pSettings->field_C < 0) {
        if (pSettings->field_1CC == 0) {
            return FALSE;
        }

        field_16E8.SequenceSet(0);

        SHORT nFrame = static_cast<SHORT>(nButton * 2 + 24);
        if (bPressed) {
            nFrame++;
        }
        field_16E8.FrameSet(nFrame);
        field_16E8.Render(0, pt.x, pt.y, rButtonClip, NULL, 0, dwFlags, -1);
        return TRUE;
    }

    field_17C2.SequenceSet(0);

    INT nFrame;
    if (pSettings->field_1CC != 0) {
        nFrame = pSettings->field_10;
    } else {
        nFrame = pSettings->field_C;
    }
    if (bPressed) {
        nFrame++;
    }
    field_17C2.FrameSet(static_cast<SHORT>(nFrame));
    field_17C2.Render(0, pt.x, pt.y, rButtonClip, NULL, 0, dwFlags, -1);

    return TRUE;
}
