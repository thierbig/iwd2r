#include "CGameDialog.h"

#include "CAIScriptFile.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CResDLG.h"
#include "CScreenWorld.h"
#include "CUIControlButton.h"
#include "CUIControlTextDisplay.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVidPalette.h"
#include "ScreenLayout.h"

// NOTE: Inlined.
CGameDialogSprite::CGameDialogSprite()
{
    ClearMarshal();

    // The bytes show the constructor (not ClearMarshal) zeroing field_54: at
    // both inlining sites the ClearMarshal call is followed by a byte store to
    // +0x54 (CScreenWorld::CScreenWorld 0x6856CD, CScreenStore's local dialog
    // 0x67C9AA). ClearMarshal itself leaves the byte alone, so `Load`'s empty
    // case and the destructor keep whatever it held. field_55 is written
    // nowhere in the original either (and read nowhere here); clearing it too
    // keeps a fresh dialog fully defined.
    field_54 = 0;
    field_55 = 0;
}

// 0x67CB50
CGameDialogSprite::~CGameDialogSprite()
{
    ClearMarshal();
}

// 0x482210
void CGameDialogSprite::Load(CResRef file, LONG characterIndex, LONG talkerIndex)
{
    if (!(file != "")) {
        ClearMarshal();
        return;
    }

    if (file == m_file) {
        m_characterIndex = characterIndex;
        g_pBaldurChitin->GetObjectGame()->SetProtagonist(characterIndex);

        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            CMessageSetProtagonist* pMessage = new CMessageSetProtagonist(m_characterIndex,
                m_characterIndex,
                m_characterIndex);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }

        m_talkerIndex = talkerIndex;
        return;
    }

    CResHelper<CResDLG, 1011> dialog;
    dialog.SetResRef(file, TRUE, TRUE);

    DWORD dialogSize;
    if (dialog.GetRes() != NULL
        && dialog.GetRes()->Demand() != NULL
        && (dialogSize = dialog.GetRes()->m_nResSizeActual, dialog.GetRes()->Release(), dialogSize != 0)) {
        m_file = file;

        BYTE* pDialog;
        dialogSize = 0;
        if (dialog.GetRes() != NULL) {
            if (dialog.GetRes()->Demand() != NULL) {
                dialogSize = dialog.GetRes()->m_nResSizeActual;
                dialog.GetRes()->Release();
            }

            pDialog = dialog.GetRes() != NULL
                ? reinterpret_cast<BYTE*>(dialog.GetRes()->Demand())
                : NULL;
        } else {
            pDialog = NULL;
        }

        Unmarshal(pDialog, dialogSize, characterIndex, talkerIndex);

        if (dialog.GetRes() != NULL) {
            dialog.GetRes()->Release();
        }
    } else {
        ClearMarshal();
    }
}

// A dialog script of the file (a trigger or an action block) as
// CAIScriptFile parses it: trimmed, one statement per line, each ending at
// its closing parenthesis.
//
// NOTE: Inlined three times in `CGameDialogSprite::Unmarshal` (0x48305C,
// 0x48328F, 0x483537), each with its own error text and assert line.
static CString ReadDialogScript(BYTE* pDialog, const CGameDialogFileScript* pScript, const char* szError, int nLine)
{
    CString sScript;
    CString sData(reinterpret_cast<const char*>(pDialog + pScript->m_offset), pScript->m_length);
    sData.TrimLeft();
    sData.TrimRight();

    while (sData.GetLength() > 0) {
        INT index = sData.Find(')');
        if (index < 0) {
            CString sError(szError);
            sError += sScript + sData;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
            // __LINE__: 620 (conditions), 677 (responses), 740 (starting conditions)
            CUtil::UtilAssert(nLine,
                "C:\\Projects\\Icewind2\\src\\Baldur\\CGameDialog.cpp",
                "index >= 0",
                sError);

            // NOTE: UtilAssert does not return (the original's shuts the
            // engine down and exits, ours aborts), so a script without ')'
            // never reaches the Left/Right below with index -1, which would
            // loop forever on the same text.
        }

        sScript += sData.Left(index + 1);
        sScript += '\n';
        sData = sData.Right(sData.GetLength() - index - 1);
        sData.TrimLeft();
    }

    return sScript;
}

// 0x482A40
void CGameDialogSprite::Unmarshal(BYTE* pDialog, DWORD dialogSize, LONG characterIndex, LONG talkerIndex)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
    // __LINE__: 427
    UTIL_ASSERT(pDialog != NULL);

    CTypedPtrArray<CPtrArray, CGameDialogFileEntry*> rawEntries;
    CTypedPtrArray<CPtrArray, CGameDialogFileReply*> rawReplies;
    CTypedPtrArray<CPtrArray, CGameDialogFileScript*> rawStartConditions;
    CTypedPtrArray<CPtrArray, CGameDialogFileScript*> rawReplyConditions;
    CTypedPtrArray<CPtrArray, CGameDialogFileScript*> rawResponses;

    if (dialogSize == 0) {
        return;
    }

    ClearMarshal();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
    // __LINE__: 448
    UTIL_ASSERT(static_cast<LONG>(dialogSize - 0x30) > 0);

    CGameDialogFileHeader header = *reinterpret_cast<CGameDialogFileHeader*>(pDialog + 8);

    DWORD nIndex;

    CGameDialogFileEntry* pFileEntries = reinterpret_cast<CGameDialogFileEntry*>(pDialog + header.m_entriesOffset);
    for (nIndex = 0; nIndex < header.m_nEntries; nIndex++) {
        CGameDialogFileEntry* pEntry = new CGameDialogFileEntry;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
        // __LINE__: 459
        UTIL_ASSERT(pEntry != NULL);

        memset(pEntry, 0, sizeof(CGameDialogFileEntry));
        *pEntry = pFileEntries[nIndex];
        rawEntries.SetAtGrow(rawEntries.GetSize(), pEntry);
    }

    CGameDialogFileReply* pFileReplies = reinterpret_cast<CGameDialogFileReply*>(pDialog + header.m_repliesOffset);
    for (nIndex = 0; nIndex < header.m_nReplies; nIndex++) {
        CGameDialogFileReply* pReply = new CGameDialogFileReply;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
        // __LINE__: 475
        UTIL_ASSERT(pReply != NULL);

        memset(pReply, 0, sizeof(CGameDialogFileReply));
        *pReply = pFileReplies[nIndex];
        rawReplies.SetAtGrow(rawReplies.GetSize(), pReply);
    }

    CGameDialogFileScript* pFileStartConditions = reinterpret_cast<CGameDialogFileScript*>(pDialog + header.m_startConditionsOffset);
    for (nIndex = 0; nIndex < header.m_nStartConditions; nIndex++) {
        CGameDialogFileScript* pScript = new CGameDialogFileScript;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
        // __LINE__: 491
        UTIL_ASSERT(pScript != NULL);

        memset(pScript, 0, sizeof(CGameDialogFileScript));
        *pScript = pFileStartConditions[nIndex];
        rawStartConditions.SetAtGrow(rawStartConditions.GetSize(), pScript);
    }

    CGameDialogFileScript* pFileReplyConditions = reinterpret_cast<CGameDialogFileScript*>(pDialog + header.m_replyConditionsOffset);
    for (nIndex = 0; nIndex < header.m_nReplyConditions; nIndex++) {
        CGameDialogFileScript* pScript = new CGameDialogFileScript;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
        // __LINE__: 507
        UTIL_ASSERT(pScript != NULL);

        memset(pScript, 0, sizeof(CGameDialogFileScript));
        *pScript = pFileReplyConditions[nIndex];
        rawReplyConditions.SetAtGrow(rawReplyConditions.GetSize(), pScript);
    }

    CGameDialogFileScript* pFileResponses = reinterpret_cast<CGameDialogFileScript*>(pDialog + header.m_responsesOffset);
    for (nIndex = 0; nIndex < header.m_nResponses; nIndex++) {
        CGameDialogFileScript* pScript = new CGameDialogFileScript;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
        // __LINE__: 523
        UTIL_ASSERT(pScript != NULL);

        memset(pScript, 0, sizeof(CGameDialogFileScript));
        *pScript = pFileResponses[nIndex];
        rawResponses.SetAtGrow(rawResponses.GetSize(), pScript);
    }

    m_characterIndex = characterIndex;
    m_talkerIndex = talkerIndex;

    CGameSprite* pCharacter;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(characterIndex,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pCharacter),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        m_playerColor = CVidPalette::RANGE_COLORS[pCharacter->GetBaseStats()->m_colors[CVIDPALETTE_RANGE_MAIN_CLOTH]];
        m_playerName = pCharacter->GetName();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(characterIndex,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        CAIScriptFile scriptFile;

        for (DWORD nEntry = 0; nEntry < header.m_nEntries; nEntry++) {
            CGameDialogEntry* pEntry = new CGameDialogEntry();
            CGameDialogFileEntry* pFileEntry = rawEntries[nEntry];
            pEntry->m_dialogText = pFileEntry->m_dialogText;

            for (DWORD nReply = 0; nReply < pFileEntry->m_nReplies; nReply++) {
                CGameDialogFileReply* pFileReply = rawReplies[pFileEntry->m_startingReply + nReply];

                CGameDialogReply* pReply = new CGameDialogReply();
                pReply->m_flags = pFileReply->m_flags;

                if ((pFileReply->m_flags & 0x1) != 0) {
                    pReply->m_replyText = pFileReply->m_replyText;
                } else {
                    pReply->m_replyText = -1;
                }

                if ((pFileReply->m_flags & 0x10) != 0) {
                    pReply->m_journalEntry = pFileReply->m_journalEntry;
                } else {
                    pReply->m_journalEntry = -1;
                }

                if ((pFileReply->m_flags & 0x2) != 0) {
                    CString sScript;
                    if (pFileReply->m_conditionalScript != -1) {
                        sScript = ReadDialogScript(pDialog,
                            rawReplyConditions[pFileReply->m_conditionalScript],
                            "Error found while parsing dialog conditions:",
                            620);
                    } else {
                        sScript = "False()\n";
                    }

                    scriptFile.ParseConditionalString(sScript);
                    pReply->m_condition.Set(*scriptFile.m_curCondition);
                }

                if ((pFileReply->m_flags & 0x4) != 0) {
                    CString sScript;
                    if (pFileReply->m_responseScript != -1) {
                        sScript = ReadDialogScript(pDialog,
                            rawResponses[pFileReply->m_responseScript],
                            "Error found while parsing dialog responses:",
                            677);
                    } else {
                        sScript = "\n";
                    }

                    scriptFile.ParseResponseString(sScript);
                    pReply->m_response.Set(*scriptFile.m_curResponse);
                }

                pReply->m_nextDialog = CResRef(pFileReply->m_nextDialog);
                pReply->m_nextEntryIndex = pFileReply->m_nextEntry;

                pEntry->SetAtGrow(pEntry->GetSize(), pReply);
            }

            CString sScript;
            if (pFileEntry->m_StartingConditionalScript != -1) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
                // __LINE__: 719
                UTIL_ASSERT_MSG(pFileEntry->m_StartingConditionalScript < (DWORD)rawStartConditions.GetSize(),
                    "Blank Starting condition in Dialog file");

                sScript = ReadDialogScript(pDialog,
                    rawStartConditions[pFileEntry->m_StartingConditionalScript],
                    "Error found while parsing dialog conditions:",
                    740);

                if (strcmp(sScript, "") == 0) {
                    sScript = "False()";
                }
            } else {
                sScript = "False()\n";
            }

            scriptFile.ParseConditionalString(sScript);
            pEntry->m_startCondition.Set(*scriptFile.m_curCondition);

            pEntry->m_dialogIndex = m_dialogEntries.GetSize();
            m_dialogEntries.SetAtGrow(m_dialogEntries.GetSize(), pEntry);

            pEntry->m_conditionPriority = pFileEntry->m_StartingConditionalScript;

            INT nOrdered;
            for (nOrdered = 0; nOrdered < m_dialogEntriesOrdered.GetSize(); nOrdered++) {
                if (pEntry->m_conditionPriority < m_dialogEntriesOrdered[nOrdered]->m_conditionPriority) {
                    m_dialogEntriesOrdered.InsertAt(nOrdered, pEntry);
                    break;
                }
            }

            if (nOrdered == m_dialogEntriesOrdered.GetSize()) {
                m_dialogEntriesOrdered.SetAtGrow(m_dialogEntriesOrdered.GetSize(), pEntry);
            }
        }

        for (INT nRaw = 0; nRaw < rawEntries.GetSize(); nRaw++) {
            if (rawEntries[nRaw] != NULL) {
                delete rawEntries[nRaw];
            }
        }
        rawEntries.SetSize(0);

        for (INT nRaw = 0; nRaw < rawReplies.GetSize(); nRaw++) {
            if (rawReplies[nRaw] != NULL) {
                delete rawReplies[nRaw];
            }
        }
        rawReplies.SetSize(0);

        for (INT nRaw = 0; nRaw < rawStartConditions.GetSize(); nRaw++) {
            if (rawStartConditions[nRaw] != NULL) {
                delete rawStartConditions[nRaw];
            }
        }
        rawStartConditions.SetSize(0);

        for (INT nRaw = 0; nRaw < rawReplyConditions.GetSize(); nRaw++) {
            if (rawReplyConditions[nRaw] != NULL) {
                delete rawReplyConditions[nRaw];
            }
        }
        rawReplyConditions.SetSize(0);

        for (INT nRaw = 0; nRaw < rawResponses.GetSize(); nRaw++) {
            if (rawResponses[nRaw] != NULL) {
                delete rawResponses[nRaw];
            }
        }
        rawResponses.SetSize(0);
    } else {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
        // __LINE__: 547
        UTIL_ASSERT(FALSE);
    }
}

// 0x4824F0
void CGameDialogSprite::Reload(CResRef file, CGameSprite* pTalker)
{
    LONG nTalkerId;

    if (!(file != "")) {
        nTalkerId = m_talkerIndex;
    } else {
        if (pTalker->m_dialog == file || pTalker->field_56E4 == file) {
            nTalkerId = pTalker->GetId();
        } else {
            CGameArea* pArea = pTalker->m_pArea;
            if (pArea->m_cInfinity.nAreaX > pArea->m_cInfinity.nAreaY) {
                nTalkerId = pArea->GetNearestDialogMatch(pTalker->GetId(),
                    file,
                    pArea->m_cInfinity.nAreaX);
            } else {
                nTalkerId = pArea->GetNearestDialogMatch(pTalker->GetId(),
                    file,
                    pArea->m_cInfinity.nAreaY);
            }
        }

        if (nTalkerId == CGameObjectArray::INVALID_INDEX) {
            // No creature of the area has the dialog: the DIALOGMO
            // understudy is made next to the talker and speaks it.
            CString sFile;
            file.CopyToString(sFile);

            // NOTE: Unused.
            CString sError = "No sprite with dialog file: " + sFile;

            CPoint ptTalker = pTalker->GetPos();
            LONG nGridY = ptTalker.y / CPathSearch::GRID_SQUARE_SIZEY;
            LONG nGridX = ptTalker.x / CPathSearch::GRID_SQUARE_SIZEX;
            BYTE nPersonalSpace = pTalker->m_animation.GetPersonalSpace();
            CPoint ptOpen = pTalker->m_pArea->m_search.GetNearestOpenSquare(CPoint(nGridX, nGridY),
                pTalker->GetTerrainTable(),
                nPersonalSpace,
                -1);

            CCreatureFile creatureData;

            // NOTE: Uninline.
            creatureData.SetResRef(CResRef("DIALOGMO"), TRUE, TRUE);

            CGameSprite* pUnderstudy = new CGameSprite(creatureData.GetData(),
                creatureData.GetDataSize(),
                0,
                0,
                -1,
                0,
                0,
                0x7FFFFFFF,
                CPoint(-1, -1),
                0);

            pUnderstudy->SetResRef(CResRef(creatureData.GetResRef().GetResRefStr()));

            creatureData.ReleaseData();

            if (pUnderstudy == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
                // __LINE__: 349
                UTIL_ASSERT_MSG(FALSE, "Where's Mo the understudy?");
                return;
            }

            LONG nUnderstudy = pUnderstudy->GetId();
            CString sName("DIALOGMO");
            pUnderstudy->SetResRef(CResRef(sName));

            if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nUnderstudy,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pUnderstudy),
                    INFINITE)
                != CGameObjectArray::SUCCESS) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
                // __LINE__: 360
                UTIL_ASSERT_MSG(FALSE, "Where's Mo the understudy?");
                return;
            }

            pUnderstudy->AddToArea(pTalker->m_pArea,
                CPoint(ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX, ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY),
                0,
                pUnderstudy->m_animation.GetListType());
            pUnderstudy->m_dialog = CResRef(sFile);

            nUnderstudy = pUnderstudy->GetId();
            pUnderstudy->SetSequence(CGameSprite::SEQ_READY);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nUnderstudy,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            nTalkerId = nUnderstudy;
        }
    }

    m_talkerIndex = nTalkerId;
    g_pBaldurChitin->GetObjectGame()->SetProtagonist(m_characterIndex);

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        CMessageSetProtagonist* pMessage = new CMessageSetProtagonist(m_characterIndex,
            m_characterIndex,
            m_characterIndex);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }
}

// 0x483970
void CGameDialogSprite::ClearMarshal()
{
    m_characterIndex = 0;
    m_talkerIndex = 0;
    m_dialogFreezeCounter = 6;
    m_dialogFreezeMultiplayer = 0;
    m_file = "";
    m_waitingForResponse = FALSE;
    m_currentEntryIndex = 0;
    m_responseMarker = -1;

    for (INT nIndex = 0; nIndex < m_dialogEntries.GetCount(); nIndex++) {
        CGameDialogEntry* pEntry = m_dialogEntries.GetAt(nIndex);
        if (pEntry != NULL) {
            delete pEntry;
        }
    }

    m_dialogEntries.RemoveAll();
    m_dialogEntriesOrdered.RemoveAll();
}

// 0x4839F0
BOOL CGameDialogSprite::StartDialog(CGameSprite* pSprite)
{
    g_pBaldurChitin->m_pEngineWorld->DisableKeyRepeat();

    m_nMusicThreadPriority = GetThreadPriority(g_pChitin->m_hMusicThread);
    if (GetPrivateProfileIntA("Program Options", "Volume Music", 0, g_pChitin->GetIniFileName())) {
        m_bMusicThreadPriorityChanged = SetThreadPriority(g_pChitin->m_hMusicThread, 15);
    }

    for (INT nIndex = 0; nIndex < m_dialogEntriesOrdered.GetCount(); nIndex++) {
        CGameDialogEntry* pEntry = m_dialogEntriesOrdered.GetAt(nIndex);
        CTypedPtrList<CPtrList, CAITrigger*> triggerList;
        if (pEntry != NULL
            && pEntry->m_startCondition.Hold(triggerList, pSprite)) {
            // FIXME: Unused.
            LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetProtagonist();

            CMessage* pMessage = new CMessageEnterDialog(pEntry->m_dialogIndex,
                TRUE,
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            field_54 = 1;
            field_56 = "";

            return TRUE;
        }
    }

    return FALSE;
}

// 0x483B70
BOOL CGameDialogSprite::FetchRumor(const CResRef& file, CGameSprite* pSprite, LONG& nIndex, STR_RES& strRes)
{
    CTypedPtrArray<CPtrArray, CGameDialogEntry*> entries;

    Load(file, pSprite->GetId(), pSprite->GetId());

    for (INT nEntry = 0; nEntry < m_dialogEntriesOrdered.GetSize(); nEntry++) {
        if (m_dialogEntriesOrdered[nEntry] != NULL) {
            CTypedPtrList<CPtrList, CAITrigger*> triggerList(10);
            if (m_dialogEntriesOrdered[nEntry]->m_startCondition.Hold(triggerList, pSprite)) {
                entries.SetAtGrow(entries.GetSize(), m_dialogEntriesOrdered[nEntry]);
            }
        }
    }

    if (entries.GetSize() <= 0) {
        return FALSE;
    }

    if (nIndex < 0) {
        nIndex = rand() % entries.GetSize();
    }

    if (nIndex >= entries.GetSize()) {
        nIndex %= entries.GetSize();
    }

    CGameDialogEntry* pEntry = entries[nIndex];
    g_pBaldurChitin->GetTlkTable().Fetch(pEntry->m_dialogText, strRes);

    if (pEntry->GetSize() > 0 && (pEntry->GetAt(0)->m_flags & 0x10) != 0) {
        g_pBaldurChitin->GetObjectGame()->m_cJournal.AddEntry(pEntry->GetAt(0)->m_journalEntry, 0);
    }

    nIndex++;

    return TRUE;
}

// 0x483CF0
void CGameDialogSprite::EndDialog()
{
    if (m_bMusicThreadPriorityChanged == TRUE) {
        SetThreadPriority(g_pChitin->m_hMusicThread, m_nMusicThreadPriority);
    }

    g_pBaldurChitin->m_pEngineWorld->EnableKeyRepeat();

    m_waitingForResponse = FALSE;
    m_responseMarker = -1;
    field_54 = 0;
    field_56 = "";

    CResRef cResRef("SilentDH");

    CSound cSound;
    cSound.SetResRef(cResRef, TRUE, TRUE);
    if (cSound.m_nLooping == 0) {
        cSound.SetFireForget(TRUE);
    }
    cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(g_pBaldurChitin->GetObjectGame()->GetVisibleArea()));
    cSound.Play(FALSE);
}

// 0x483EB0
BOOL CGameDialogSprite::EnterDialog(DWORD index, CGameSprite* pSprite, int a3)
{
    CGameDialogEntry* pEntry = m_dialogEntries[index];
    if (pEntry == NULL) {
        m_currentEntryIndex = 0;
        return FALSE;
    }

    m_currentEntryIndex = index;
    m_waitingForResponse = TRUE;
    m_responseMarker = -1;
    pEntry->Handle(pSprite, m_playerColor, a3);
    return TRUE;
}

// 0x483F00
void CGameDialogSprite::AsynchronousUpdate()
{
    DWORD nMode = g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode;

    if (!m_waitingForResponse) {
        if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode != 0x502) {
            return;
        }

        if (!g_pChitin->cNetwork.GetSessionOpen()) {
            if (m_dialogFreezeCounter-- > 0) {
                return;
            }

            m_dialogFreezeCounter = 6;
        } else {
            if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode != 0x502) {
                return;
            }

            if (m_dialogFreezeMultiplayer++ < 10) {
                return;
            }

            m_dialogFreezeMultiplayer = 0;

            if (m_dialogFreezeCounter-- > 0) {
                return;
            }

            // NOTE: The original builds this message and does not show it.
            CString sMessage;
            CGameSprite* pTalker;

            BYTE rc;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_talkerIndex,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pTalker),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                sMessage = CString("Sprite trying to execute dialog with none") + pTalker->GetName();

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_talkerIndex,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            } else {
                sMessage = "Unknown sprite trying to execute dialog with none";
            }

            m_dialogFreezeCounter = 6;
        }

        g_pBaldurChitin->m_pEngineWorld->EndDialog(FALSE, TRUE);
        return;
    }

    if (strcmp(field_56, "") != 0) {
        CString sString;
        CUIControlTextDisplay* pDisplay = g_pBaldurChitin->m_pEngineWorld->m_pActiveDialogDisplay;
        for (INT nIndex = pDisplay->m_plstStrings->GetCount() - 1; nIndex >= 0; nIndex--) {
            pDisplay->GetString(nIndex, sString);
            if (strcmp(sString, field_56) == 0) {
                pDisplay->SetTopString(pDisplay->m_plstStrings->FindIndex(nIndex));
                break;
            }
        }

        field_56 = "";
    }

    m_dialogFreezeCounter = 6;

    CGameDialogEntry* pEntry = m_dialogEntries[m_currentEntryIndex];

    CUIControlBase* pButton = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->GetControl(0);
    if (pEntry->m_bDisplayButton && !pButton->m_bActive && nMode == 0x502) {
        pButton->SetActive(TRUE);
        g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->InvalidateRect(NULL);
    } else if ((!pEntry->m_bDisplayButton && pButton->m_bActive) || nMode != 0x502) {
        pButton->SetActive(FALSE);
        g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->InvalidateRect(NULL);
    }

    g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->InvalidateRect(NULL);

    LONG nTalkerId = m_talkerIndex;
    CGameSprite* pTalker;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nTalkerId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pTalker),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        g_pBaldurChitin->m_pEngineWorld->EndDialog(FALSE, TRUE);
        return;
    }

    CGameDialogNextEntry* pNext;
    if (m_responseMarker == -1) {
        pNext = NULL;
        pEntry->m_picked = FALSE;
    } else {
        pEntry->m_picked = TRUE;
        if (m_responseMarker == -2) {
            pNext = NULL;
        } else {
            pNext = pEntry->GetAt(m_responseMarker)->Pick(pTalker);
        }
    }

    if (pEntry->m_picked) {
        if (pNext != NULL) {
            Reload(pNext->m_file, pTalker);

            CString sNextDialog;
            pNext->m_file.CopyToString(sNextDialog);

            CMessagePickedDialogResponse* pMessage = new CMessagePickedDialogResponse(m_playerName,
                m_currentEntryIndex,
                m_responseMarker,
                m_playerColor,
                pTalker->GetId(),
                sNextDialog,
                m_talkerIndex,
                m_characterIndex,
                pNext->m_entryIndex,
                m_talkerIndex,
                m_talkerIndex);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            m_responseMarker = -1;
            delete pNext;
        } else {
            CMessageRemoveReplies* pMessage = new CMessageRemoveReplies(m_currentEntryIndex,
                m_responseMarker,
                m_playerColor,
                m_playerName,
                pTalker->GetId(),
                pTalker->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            g_pBaldurChitin->m_pEngineWorld->EndDialog(FALSE, TRUE);
        }
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nTalkerId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x4845C0
void CGameDialogSprite::UpdateDialogColors()
{
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameDialog.cpp
    // __LINE__: 1295
    UTIL_ASSERT(pWorld != NULL);

    if (m_waitingForResponse && pWorld->m_pActiveDialogDisplay != NULL) {
        COLORREF rgb = RGB(215, 215, 190);
        if (pWorld->m_bInControlOfDialog) {
            rgb = RGB(255, 46, 33);
        }

        CGameDialogEntry* pEntry = m_dialogEntries[m_currentEntryIndex];

        for (INT nIndex = 0; nIndex < pEntry->GetCount(); nIndex++) {
            CGameDialogReply* pReply = pEntry->GetAt(nIndex);
            if (pReply->m_displayPosition != NULL) {
                pWorld->m_pActiveDialogDisplay->SetItemTextColor(pReply->m_displayPosition, rgb);
            }
        }
    }
}

// NOTE: Inlined in `CGameDialogSprite::Unmarshal` (0x482F06).
CGameDialogEntry::CGameDialogEntry()
{
    m_picked = FALSE;
    m_conditionPriority = 0;
    m_dialogIndex = 0;
    m_bDisplayButton = FALSE;
}

// 0x484680
CGameDialogEntry::~CGameDialogEntry()
{
    for (INT nIndex = 0; nIndex < GetCount(); nIndex++) {
        CGameDialogReply* pReply = GetAt(nIndex);
        if (pReply != NULL) {
            delete pReply;
        }
    }
    RemoveAll();
}

// 0x484730
void CGameDialogEntry::RemoveReplies(LONG lMarker, COLORREF rgbNameColor, const CString& sName)
{
    STR_RES strRes;
    BOOLEAN bRemoveIfPicked = FALSE;
    STRREF strReplyText;

    for (INT nIndex = 0; nIndex < GetCount(); nIndex++) {
        CGameDialogReply* pReply = GetAt(nIndex);
        if ((pReply->m_flags & 0x20) == 0) {
            if (pReply->m_displayPosition != NULL) {
                if (nIndex == lMarker) {
                    bRemoveIfPicked = pReply->m_removeIfPicked;
                    strReplyText = pReply->m_replyText;
                }

                g_pBaldurChitin->m_pEngineWorld->RemoveText(pReply->m_displayPosition);
                pReply->m_displayPosition = NULL;
            }
        }
    }

    if (!bRemoveIfPicked) {
        if (lMarker >= 0 && lMarker < GetCount()) {
            strReplyText = GetAt(lMarker)->m_replyText;
        }

        g_pBaldurChitin->GetTlkTable().Fetch(strReplyText, strRes);

        g_pBaldurChitin->m_pEngineWorld->DisplayText(sName,
            strRes.szText,
            rgbNameColor,
            RGB(160, 200, 215),
            -1,
            FALSE);

        g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
            CString(""),
            -1,
            FALSE);
    }
}

// Plays the speech of a dialog string fetched into `strRes`.
//
// NOTE: Inlined four times in `CGameDialogEntry::Handle` (0x48499A,
// 0x484ED7, 0x485019, 0x485468).
static void PlayDialogSpeech(STR_RES& strRes)
{
    if (strRes.cSound.GetRes() != NULL) {
        if (strRes.cSound.m_nLooping == 0) {
            strRes.cSound.SetFireForget(TRUE);
        }

        SleepEx(10, FALSE);
        strRes.cSound.Play(FALSE);

        if (strRes.cSound.m_nLooping == 0 && strRes.cSound.GetRes() != NULL) {
            strRes.cSound.GetRes()->Dump(TRUE);
        }
    }
}

// 0x484900
void CGameDialogEntry::Handle(CGameSprite* pSprite, COLORREF playerColor, int a3)
{
    m_bDisplayButton = FALSE;

    STR_RES strRes;
    DWORD nMode = g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode;

    g_pBaldurChitin->GetTlkTable().Fetch(m_dialogText, strRes);
    strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));

    CPoint ptSpeaker = pSprite->GetPos();

    PlayDialogSpeech(strRes);

    CPoint ptReference(0, 0);
    if (pSprite->m_animation.m_animation != NULL) {
        CRect rFx;
        pSprite->m_animation.CalculateFxRect(rFx, ptReference, 0);
    }

    ptSpeaker.y += -(ptReference.y / 2);

    // NOTE: Unused.
    CRect rViewPort;
    rViewPort.CopyRect(pSprite->GetArea()->GetInfinity()->rViewPort);

    ptSpeaker.y -= (CInfinity::stru_8E7958.bottom - CInfinity::stru_8E7958.top) >> 1;
    ptSpeaker.x -= (CInfinity::stru_8E7958.right - CInfinity::stru_8E7958.left) >> 1;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    BOOLEAN bMessageScreen = FALSE;

    if (pSprite->GetArea() != pGame->GetVisibleArea()) {
        pGame->GetVisibleArea()->m_bPicked = FALSE;
        pGame->GetVisibleArea()->m_iPicked = CGameObjectArray::INVALID_INDEX;
        pGame->GetVisibleArea()->m_nToolTip = 0;
        pGame->GetVisibleArea()->OnDeactivation();
        pGame->m_visibleArea = pSprite->GetArea()->m_id;
        pGame->GetVisibleArea()->OnActivation();
        bMessageScreen = g_pChitin->cNetwork.GetSessionOpen();
    }

    if (pSprite->GetArea() == pGame->GetVisibleArea()) {
        INT x;
        INT y;
        pGame->GetVisibleArea()->GetInfinity()->GetViewPosition(x, y);

        // iwd2r: the squared range at 0x484BA2 (cmp edx, 490000: a smooth
        // rather than instant scroll to the speaker within 700 pixels)
        // takes iwd2r::CurrentScreenLayout().dialogAutoScrollRange, as IEex
        // patches it to scale with the resolution.
        if ((x - ptSpeaker.x) * (x - ptSpeaker.x) + (y - ptSpeaker.y) * (y - ptSpeaker.y) > iwd2r::CurrentScreenLayout().dialogAutoScrollRange) {
            g_pBaldurChitin->m_pEngineWorld->StartScroll(ptSpeaker, 0);
            bMessageScreen = g_pChitin->cNetwork.GetSessionOpen();
        } else {
            g_pBaldurChitin->m_pEngineWorld->StartScroll(ptSpeaker, 16);
        }
    }

    if (bMessageScreen) {
        g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetInfinity()->SetMessageScreen(CResRef(""), 10467, 1000);
    }

    COLORREF rgbNameColor = CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[CVIDPALETTE_RANGE_MAIN_CLOTH]];
    g_pBaldurChitin->m_pEngineWorld->field_10B4 = 1;

    if (strcmp(strRes.szText, "") != 0) {
        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
            && !g_pBaldurChitin->m_pEngineWorld->m_bInControlOfDialog) {
            g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                CString(""),
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                FALSE);
        } else {
            g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                CString(""),
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                TRUE);
        }

        g_pBaldurChitin->m_pEngineWorld->DisplayText(pSprite->GetName(),
            strRes.szText,
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            FALSE);

        g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
            CString(""),
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            FALSE);
    }

    strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));
    PlayDialogSpeech(strRes);

    CString sReplyNumber;
    INT nReplies = 0;

    // NOTE: The original writes m_removeIfPicked of the last reply it looked
    // at when none is valid (0x4855C8); with no reply at all its register
    // holds a stack value, which iwd2r does not write through.
    CGameDialogReply* pLastReply = NULL;

    INT nIndex;
    for (nIndex = 0; nIndex < GetSize(); nIndex++) {
        CGameDialogReply* pReply = GetAt(nIndex);
        if (pReply != NULL) {
            pLastReply = pReply;
            if ((pReply->m_flags & 0x20) != 0) {
                CTypedPtrList<CPtrList, CAITrigger*> triggerList(10);
                BOOL bValid;
                if ((pReply->m_flags & 0x2) == 0 || pReply->m_condition.Hold(triggerList, pSprite)) {
                    bValid = TRUE;
                } else {
                    bValid = FALSE;
                }

                if (bValid) {
                    nReplies++;
                    if ((pReply->m_flags & 0x1) != 0) {
                        g_pBaldurChitin->GetTlkTable().Fetch(pReply->m_replyText, strRes);
                        sReplyNumber.Format("    %d:", static_cast<SHORT>(nReplies));
                        pReply->m_removeIfPicked = FALSE;
                        pReply->m_displayListId = static_cast<BYTE>(nReplies);
                    }

                    strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));
                    PlayDialogSpeech(strRes);

                    COLORREF rgbTextColor = g_pBaldurChitin->m_pEngineWorld->m_bInControlOfDialog
                        ? RGB(255, 46, 33)
                        : RGB(215, 215, 190);
                    if (strcmp(strRes.szText, "") != 0) {
                        pReply->m_displayPosition = g_pBaldurChitin->m_pEngineWorld->DisplayText(sReplyNumber,
                            strRes.szText,
                            playerColor,
                            rgbTextColor,
                            nIndex,
                            FALSE);
                    } else {
                        pReply->m_displayPosition = g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                            CString(""),
                            playerColor,
                            rgbTextColor,
                            nIndex,
                            FALSE);
                    }
                } else {
                    pReply->m_displayListId = 0xFF;
                }
            }
        }
    }

    for (nIndex = 0; nIndex < GetSize(); nIndex++) {
        CGameDialogReply* pReply = GetAt(nIndex);
        if (pReply != NULL) {
            pLastReply = pReply;

            CTypedPtrList<CPtrList, CAITrigger*> triggerList(10);
            BOOL bValid;
            if ((pReply->m_flags & 0x2) == 0 || pReply->m_condition.Hold(triggerList, pSprite)) {
                bValid = TRUE;
            } else {
                bValid = FALSE;
            }

            if (bValid) {
                nReplies++;
                if ((pReply->m_flags & 0x1) != 0) {
                    g_pBaldurChitin->GetTlkTable().Fetch(pReply->m_replyText, strRes);
                    sReplyNumber.Format("    %d:", static_cast<SHORT>(nReplies));
                    pReply->m_removeIfPicked = FALSE;
                    pReply->m_displayListId = static_cast<BYTE>(nReplies);
                } else {
                    m_bDisplayButton = TRUE;

                    if ((pReply->m_flags & 0x8) != 0) {
                        g_pBaldurChitin->GetTlkTable().Fetch(9371, strRes); // "End Dialog"
                        sReplyNumber = "";
                        pReply->m_removeIfPicked = TRUE;
                        g_pBaldurChitin->m_pEngineWorld->field_10B2 = static_cast<SHORT>(nIndex);
                        g_pBaldurChitin->m_pEngineWorld->field_10B4 = 0;

                        CUIControlButton* pButton = static_cast<CUIControlButton*>(g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->GetControl(0));
                        pButton->SetText(strRes.szText);

                        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                            pButton->SetActive(nMode == 0x502);
                            g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->InvalidateRect(NULL);
                        }

                        strRes.szText = "";

                        // StartCutSceneMode() (ACTION.IDS 121).
                        if (pReply->m_response.InList(121) == TRUE) {
                            g_pBaldurChitin->m_pEngineWorld->field_1204 = TRUE;
                        }
                    } else {
                        g_pBaldurChitin->GetTlkTable().Fetch(9372, strRes); // "Continue"
                        sReplyNumber = "";
                        pReply->m_removeIfPicked = TRUE;
                        g_pBaldurChitin->m_pEngineWorld->field_10B2 = static_cast<SHORT>(nIndex);
                        g_pBaldurChitin->m_pEngineWorld->field_10B4 = 0;

                        CUIControlButton* pButton = static_cast<CUIControlButton*>(g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->GetControl(0));
                        pButton->SetText(strRes.szText);

                        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                            pButton->SetActive(nMode == 0x502);
                            g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->InvalidateRect(NULL);
                        }

                        strRes.szText = "";
                    }
                }

                strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));
                PlayDialogSpeech(strRes);

                COLORREF rgbTextColor = g_pBaldurChitin->m_pEngineWorld->m_bInControlOfDialog
                    ? RGB(255, 46, 33)
                    : RGB(215, 215, 190);
                if (strcmp(strRes.szText, "") != 0) {
                    pReply->m_displayPosition = g_pBaldurChitin->m_pEngineWorld->DisplayText(sReplyNumber,
                        strRes.szText,
                        playerColor,
                        rgbTextColor,
                        nIndex,
                        FALSE);
                } else {
                    pReply->m_displayPosition = g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                        CString(""),
                        playerColor,
                        rgbTextColor,
                        nIndex,
                        FALSE);
                }
            } else {
                pReply->m_displayListId = 0xFF;
            }
        }
    }

    if (static_cast<SHORT>(nReplies) == 0) {
        m_bDisplayButton = TRUE;

        if (pLastReply != NULL) {
            pLastReply->m_removeIfPicked = TRUE;
        }

        g_pBaldurChitin->m_pEngineWorld->field_10B2 = -2;
        g_pBaldurChitin->m_pEngineWorld->field_10B4 = 0;

        CUIControlButton* pButton = static_cast<CUIControlButton*>(g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->GetControl(0));
        pButton->SetText(strRes.szText);

        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
            pButton->SetActive(nMode == 0x502);
            g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(9)->InvalidateRect(NULL);
        }

        g_pBaldurChitin->m_pEngineWorld->DisplayText(CString("NO VALID REPLIES OR LINKS"),
            CString(""),
            playerColor,
            RGB(255, 46, 33),
            -2,
            FALSE);
    }
}

// NOTE: Inlined in `CGameDialogSprite::Unmarshal` (0x482F9D).
CGameDialogReply::CGameDialogReply()
{
    m_flags = 0;
    m_nextEntryIndex = 0;
    m_displayPosition = NULL;
    m_removeIfPicked = FALSE;
    m_displayListId = 0xFF;
    m_journalEntry = 0;
}

// 0x485750
CGameDialogNextEntry* CGameDialogReply::Pick(CGameSprite* pSprite)
{
    if ((m_flags & 0x4) != 0) {
        if (m_response.m_actionList.GetCount() != 0) {
            CMessageInsertResponse* pInsertResponse = new CMessageInsertResponse(m_response,
                FALSE,
                TRUE,
                TRUE,
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pInsertResponse, FALSE);

            CMessageSetCommandPause* pCommandPause = new CMessageSetCommandPause(75,
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pCommandPause, FALSE);
        }

        CMessageSetDialogWait* pDialogWait = new CMessageSetDialogWait(0,
            CGameObjectArray::INVALID_INDEX,
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pDialogWait, FALSE);
    }

    if ((m_flags & 0x10) != 0) {
        if (g_pBaldurChitin->GetObjectGame()->m_cJournal.AddEntry(m_journalEntry, 0) == 1) {
            STR_RES strRes;
            if (g_pBaldurChitin->GetTlkTable().Fetch(11359, strRes)) { // "Your journal has been updated."
                g_pBaldurChitin->m_pEngineWorld->m_internalLoadedDialog.field_56 = strRes.szText;
            }
        }
    }

    if (m_displayPosition != NULL) {
        g_pBaldurChitin->m_pEngineWorld->SetItemMarker(m_displayPosition, -1);
    }

    if ((m_flags & 0x8) != 0) {
        return NULL;
    }

    CGameDialogNextEntry* pNext = new CGameDialogNextEntry;
    pNext->m_entryIndex = 0;
    pNext->m_file = m_nextDialog;
    pNext->m_entryIndex = m_nextEntryIndex;
    return pNext;
}
