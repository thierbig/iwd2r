#include "CNetwork.h"

#include "CChitin.h"
#include "CUtil.h"

// 0x85E610
const DWORD CNetworkWindow::DEFAULT_PACKET_TIMEOUT = 3200;

// 0x85E614
const DWORD CNetworkWindow::DEFAULT_SENDPACKET_TIMEOUT = 200;

// 0x85E618
const DWORD CNetworkWindow::DEFAULT_PLAYER_TIMEOUT = 25000;

// 0x8FB9C4
const DWORD CNetworkWindow::MAX_TIMEOUT_TICK_COUNT = CNetwork::MAX_TIMEOUT_TICK_COUNT;

// 0x8FB9CC
const CString CNetwork::MG("MG");

// 0x8FB9D0
const CString CNetwork::JM("JM");

// 0x8FB9C8
const CString CNetwork::JB("JB");

// 0x8FB9D4
BYTE CNetwork::STATIC_MESSAGE_BUFFER[STATIC_MESSAGE_SIZE];

// 0x9039D4
DWORD CNetwork::DYNAMIC_MESSAGE_SIZE;

// 0x85E634
const INT CNetwork::MAX_PLAYERS = CNETWORK_MAX_PLAYERS;

// 0x85E638
const INT CNetwork::MAX_SESSIONS = CNETWORK_MAX_SESSIONS;

// 0x85E63C
const INT CNetwork::MAX_SERVICE_PROVIDERS = CNETWORK_MAX_SERVICE_PROVIDERS;

// 0x85E640
const INT CNetwork::MAX_STRING_LENGTH = 200;

// 0x85E644
const DWORD CNetwork::MAX_TIMEOUT_TICK_COUNT = DWORD_MAX - 1024;

// 0x85E64C
const INT CNetwork::SERV_PROV_TCP_IP = 8;

// 0x85E650
const INT CNetwork::SERV_PROV_MODEM = 4;

// 0x85E654
const INT CNetwork::SERV_PROV_SERIAL = 2;

// 0x85E658
const INT CNetwork::SERV_PROV_IPX = 1;

// 0x85E65C
const INT CNetwork::SERV_PROV_NULL = 0;

// 0x85E660
const INT CNetwork::SEND_ALL_PLAYERS = 0x100;

// 0x85E664
const INT CNetwork::SEND_GUARANTEED = 0x200;

// 0x85E668
const INT CNetwork::SEND_RAW = 0x400;

// 0x85E66C
const INT CNetwork::SEND_JOINING_PLAYERS = 0x800;

// 0x85E670
const INT CNetwork::SPEC_MSG_HEADER_LENGTH = 3;

// 0x85E674
const INT CNetwork::SPEC_MSG_FLAG = 0;

// 0x85E678
const INT CNetwork::SPEC_MSG_TYPE = 1;

// 0x85E67C
const INT CNetwork::SPEC_MSG_SUBTYPE = 2;

// 0x85E680
const BYTE CNetwork::SPEC_MSG_FLAG_ENABLED = 0xFF;

// 0x85E684
const DWORD CNetwork::MINIMAL_PACKET_SIZE = 128;

// 0x85E688
const DWORD CNetwork::MAXIMAL_PACKET_SIZE = 1024;

// 0x85E68C
const DWORD CNetwork::dword_85E68C = 16;

// 0x85E690
const DWORD CNetwork::dword_85E690 = 12;

// 0x85E694
const DWORD CNetwork::dword_85E694 = 12;

// 0x85E698
const DWORD CNetwork::dword_85E698 = 10;

// 0x85E69C
const DWORD CNetwork::dword_85E69C = 2;

// 0x85E6A0
const DWORD CNetwork::dword_85E6A0 = 2;

// 0x85E6A4
const DWORD CNetwork::dword_85E6A4 = 2;

// 0x85E6A8
const INT CNetwork::ERROR_NONE = 0;

// 0x85E6AC
const INT CNetwork::ERROR_PLAYEREXISTS = 1;

// 0x85E6B0
const INT CNetwork::ERROR_CANNOTCREATEPLAYER = 2;

// 0x85E6B4
const INT CNetwork::ERROR_CANNOTCONNECT = 3;

// 0x85E6B8
const INT CNetwork::ERROR_INVALIDPASSWORD = 4;

// #binary-identical
// 0x7A1E50
CNetworkWindow::CNetworkWindow()
{
    m_bInitialized = FALSE;
    m_bVSSent = FALSE;
    m_bVSReceived = FALSE;
    m_nPlayerNumber = 0;
    m_nAckExpected = 0;
    m_nNextFrameToSend = 0;
    m_nFrameExpected = 0;
    m_nTooFar = 0;
    m_nOldestFrame = 0;
    m_pbTimeOutSet[0] = FALSE;
    m_pnTimeOut[0] = 0;
    m_pbArrived[0] = FALSE;
    m_nNumBuffered = 0;
    m_bNoNak = FALSE;
    m_nPacketTimeout = 0;
    m_nAckTimer = 0;
    m_bAckTimerSet = FALSE;
    m_bSomethingHappened = 0;
    m_nNextEvent = 0;
    m_nPlayerTimeout = 0;
    m_nNoMessageTimeout = 0;
}

// #binary-identical
// 0x7A1F00
CNetworkWindow::~CNetworkWindow()
{
    m_bInitialized = FALSE;
}

// 0x7A1F50
void CNetworkWindow::AddToIncomingQueue(PLAYER_ID idFrom, PLAYER_ID idTo, BYTE* pData, DWORD dwDataSize)
{
    if (dwDataSize >= CNetwork::dword_85E68C
        && memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0
        && dwDataSize > (pData[CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0] << 8)
                + pData[CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0 + 1]
                + CNetwork::dword_85E68C) {
        DWORD dwOffset = 0;
        DWORD dwRemainingSize = dwDataSize;
        while (1) {
            DWORD dwChunkSize = (pData[dwOffset + CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0] << 8)
                + pData[CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0 + 1]
                + CNetwork::dword_85E68C;
            if (dwChunkSize > dwRemainingSize) {
                break;
            }

            BYTE* pChunkData = new BYTE[dwChunkSize];

            if (pChunkData == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                // __LINE__: 344
                UTIL_ASSERT(FALSE);
            }

            memcpy(pChunkData, pData + dwOffset, dwChunkSize);

            CNETWORKWINDOW_QUEUEENTRY* pEntry = new CNETWORKWINDOW_QUEUEENTRY();
            pEntry->idFrom = idFrom;
            pEntry->idTo = idTo;
            pEntry->pData = pChunkData;
            pEntry->dwSize = dwChunkSize;
            m_lQueueIncomingMessages.AddTail(pEntry);

            dwOffset += dwChunkSize;
            dwRemainingSize -= dwChunkSize;

            if (dwRemainingSize == 0) {
                delete pData;
                return;
            }
        }
    } else {
        CNETWORKWINDOW_QUEUEENTRY* pEntry = new CNETWORKWINDOW_QUEUEENTRY();
        pEntry->idFrom = idFrom;
        pEntry->idTo = idTo;
        pEntry->pData = pData;
        pEntry->dwSize = dwDataSize;
        m_lQueueIncomingMessages.AddTail(pEntry);
    }
}

// 0x7A2100
BOOLEAN CNetworkWindow::CheckIncomingQueue()
{
    if (m_lQueueIncomingMessages.IsEmpty()) {
        return FALSE;
    }

    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    POSITION pos = m_lQueueIncomingMessages.GetHeadPosition();
    BYTE* pData = m_lQueueIncomingMessages.GetAt(pos)->pData;

    if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::MG, CNetwork::dword_85E69C) == 0) {
        LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        return TRUE;
    }

    if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0) {
        int cnt = 0;
        for (int offset = CNetwork::dword_85E69C + CNetwork::dword_85E698; offset < CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0; offset++) {
            cnt = pData[offset] + (cnt << 8);
        }

        while (pos != NULL && cnt > 0) {
            m_lQueueIncomingMessages.GetNext(pos);
            cnt--;
        }

        LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        return cnt == 0;
    }

    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));

    CString v1;
    CString v2;
    CString v3;
    CString v4;
    CString v5;

    v1.Format("Messages have been misordered.  Please report this bug to Interplay.\n");
    v2.Format("It is very important that the numbers below are recorded correctly.\n");
    v3.Format("Communication Parameters: PN:%d FTS:%d TF:%d AE:%d NB:%d FE:%d\n",
        m_nPlayerNumber,
        m_nNextFrameToSend,
        m_nTooFar,
        m_nAckExpected,
        m_nNumBuffered,
        m_nFrameExpected);
    v4.Format("Message Prefix: %x %x %x %x %x %x %x %x\n",
        pData[0],
        pData[1],
        pData[2],
        pData[3],
        pData[4],
        pData[5],
        pData[6],
        pData[7]);
    v5.Format("%s %s %s %s",
        (LPCSTR)v1,
        (LPCSTR)v2,
        (LPCSTR)v3,
        (LPCSTR)v4);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
    // __LINE__: 541
    UTIL_ASSERT_MSG(FALSE, (LPCSTR)v5);
}

// 0x7A2370
BOOLEAN CNetworkWindow::CheckIncomingQueueSpecific(BYTE nSpecMsgType, BYTE nSpecMsgSubType)
{
    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    if (!m_lQueueIncomingMessages.IsEmpty()) {
        POSITION pos = m_lQueueIncomingMessages.GetHeadPosition();
        while (pos != NULL) {
            BYTE* pData = m_lQueueIncomingMessages.GetNext(pos)->pData;

            if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::MG, CNetwork::dword_85E69C) == 0) {
                pData += CNetwork::dword_85E694;
                if (pData[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED
                    && pData[CNetwork::SPEC_MSG_TYPE] == nSpecMsgType
                    && pData[CNetwork::SPEC_MSG_SUBTYPE] == nSpecMsgSubType) {
                    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
                    return TRUE;
                }
            }

            if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0) {
                int cnt = 0;
                for (int offset = CNetwork::dword_85E69C + CNetwork::dword_85E698; offset < CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0; offset++) {
                    cnt = pData[offset] + (cnt << 8);
                }

                pData += CNetwork::dword_85E68C;
                if (pData[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED
                    && pData[CNetwork::SPEC_MSG_TYPE] == nSpecMsgType
                    && pData[CNetwork::SPEC_MSG_SUBTYPE] == nSpecMsgSubType) {
                    while (pos != NULL && cnt > 0) {
                        m_lQueueIncomingMessages.GetNext(pos);
                        cnt--;
                    }

                    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
                    return cnt == 0;
                }
            }
        }
    }

    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
    return FALSE;
}

// 0x7A2510
BOOLEAN CNetworkWindow::CheckOutgoingQueueForAppendableMsgs(DWORD dwSize)
{
    if (m_lQueueOutgoingMessages.IsEmpty()) {
        return FALSE;
    }

    CNETWORKWINDOW_QUEUEENTRY* pEntry = m_lQueueOutgoingMessages.GetHead();

    // NOTE: Original code is slightly different.
    if (pEntry->dwSize > CNetwork::dword_85E68C
        && pEntry->dwSize < dwSize
        && memcmp(pEntry->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0
        && pEntry->pData[CNetwork::dword_85E698 + CNetwork::dword_85E69C] == 0
        && pEntry->pData[CNetwork::dword_85E698 + CNetwork::dword_85E69C + 1] == 1) {
        return TRUE;
    }

    return FALSE;
}

// 0x7A2590
void CNetworkWindow::FrameSend(BYTE nFrameKind, WORD nFrameNumber)
{
    BYTE* pData;
    DWORD dwSize;

    if (nFrameKind != 0) {
        pData = new BYTE[CNetwork::dword_85E698];
        dwSize = CNetwork::dword_85E698;
        pData[5] = 0;
    } else {
        pData = m_pOutgoingBuffers[0].pData;
        dwSize = m_pOutgoingBuffers[0].dwSize;
    }

    if (pData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 913
        UTIL_ASSERT_MSG(FALSE, "FrameSend:: Null Message!\n");
    }

    pData[0] = nFrameKind;
    pData[1] = static_cast<BYTE>(nFrameNumber >> 8);
    pData[2] = static_cast<BYTE>(nFrameNumber);
    pData[3] = static_cast<BYTE>((m_nFrameExpected - 1) >> 8);
    pData[4] = static_cast<BYTE>(m_nFrameExpected - 1);

    if (dwSize >= 10) {
        DWORD dwCrc = 0;

        pData[6] = 0;
        pData[7] = 0;
        pData[8] = 0;
        pData[9] = 0;

        for (DWORD k = 0; k < dwSize; k++) {
            dwCrc = g_pChitin->cNetwork.m_dwCRC32[(dwCrc & 0xFF) ^ pData[k]] ^ (dwCrc >> 8);
        }

        pData[6] = static_cast<BYTE>(dwCrc >> 24);
        pData[7] = static_cast<BYTE>(dwCrc >> 16);
        pData[8] = static_cast<BYTE>(dwCrc >> 8);
        pData[9] = static_cast<BYTE>(dwCrc);
    }

    if (nFrameKind == 2) {
        m_bNoNak = FALSE;
    }

    // NOTE: Uninline.
    PLAYER_ID idTo = g_pChitin->cNetwork.GetPlayerID(m_nPlayerNumber);

    // NOTE: iwd2r: The original calls the CNetwork vtable slot `+0x68` (the
    // DirectPlay `Send` wrapper) inside this critical section,
    // unconditionally, as `Send(m_idLocalPlayer, idTo, 0, pData, dwSize)`.
    // DirectPlay is gone, so nothing is sent.
    EnterCriticalSection(&(g_pChitin->cNetwork.field_F6A));
    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F6A));

    if (nFrameKind == 0) {
        m_pbTimeOutSet[0] = TRUE;
        m_pnTimeOut[0] = m_nPacketTimeout + GetTickCount();
        if (m_pnTimeOut[0] > CNetwork::MAX_TIMEOUT_TICK_COUNT) {
            m_pnTimeOut[0] = 0;
        }
    } else {
        delete pData;
    }

    m_bAckTimerSet = FALSE;

    // NOTE: Uninline.
    SetNoMessageTimeout();
}

// #binary-identical
// 0x7A2770
void CNetworkWindow::Initialize(INT nIndex)
{
    if (m_bInitialized) {
        RemoveFromAllQueues();
    }

    m_bVSSent = FALSE;
    m_bVSReceived = FALSE;
    m_nPlayerNumber = static_cast<BYTE>(nIndex);
    m_nNextFrameToSend = 0;
    m_nTooFar = 1;
    m_nAckExpected = 0;
    m_nNumBuffered = 0;
    m_nFrameExpected = 0;
    m_bNoNak = TRUE;
    m_nPacketTimeout = DEFAULT_PACKET_TIMEOUT;
    m_bAckTimerSet = FALSE;

    // NOTE: Uninline.
    SetPlayerTimeout();

    // NOTE: Uninline.
    SetNoMessageTimeout();

    m_pbTimeOutSet[0] = FALSE;
    m_pbArrived[0] = FALSE;
    m_pOutgoingBuffers[0].dwSize = 0;
    m_pOutgoingBuffers[0].pData = NULL;
    m_pIncomingBuffers[0].dwSize = 0;
    m_pIncomingBuffers[0].pData = NULL;
    m_pnTimeOut[0] = 0;

    m_bInitialized = TRUE;
}

// 0x7A2810
void CNetworkWindow::ShutDown()
{
    PLAYER_ID nServerID = g_pChitin->cNetwork.GetHostPlayerID();
    PLAYER_ID nPlayerID = g_pChitin->cNetwork.m_idLocalPlayer;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
    // __LINE__: 1133
    UTIL_ASSERT(nPlayerID != 0 && nServerID != 0);

    SHORT nServerLocation = g_pChitin->cNetwork.FindPlayerLocationByID(nServerID, TRUE);

    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    // NOTE: Lots of jumps and conditions, probably wrong.
    while (m_nAckExpected != m_nNextFrameToSend) {
        BYTE* pData = m_pOutgoingBuffers[0].pData;
        DWORD dwDataSize = m_pOutgoingBuffers[0].dwSize;
        if (dwDataSize >= CNetwork::dword_85E68C
            && memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0
            && dwDataSize > ((pData[CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0] << 8)
                   + pData[CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0 + 1]
                   + CNetwork::dword_85E68C)) {
            DWORD dwOffset = 0;
            DWORD dwRemainingSize = dwDataSize;
            while (1) {
                DWORD dwChunkSize = (pData[dwOffset + CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0] << 8)
                    + pData[CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0 + 1]
                    + CNetwork::dword_85E68C;
                if (dwChunkSize > dwRemainingSize) {
                    break;
                }

                BYTE* pChunkData = new BYTE[dwChunkSize];

                if (pChunkData == NULL) {
                    // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                    // __LINE__: 1194
                    UTIL_ASSERT(FALSE);
                }

                memcpy(pChunkData, pData + dwOffset, dwChunkSize);

                if (g_pChitin->MessageCallback(pChunkData + CNetwork::dword_85E68C, dwChunkSize - CNetwork::dword_85E68C) == TRUE) {
                    CNETWORKWINDOW_QUEUEENTRY* pEntry = new CNETWORKWINDOW_QUEUEENTRY();
                    pEntry->idFrom = nPlayerID;
                    pEntry->idTo = nServerID;
                    pEntry->pData = pChunkData;
                    pEntry->dwSize = dwChunkSize;
                    m_lQueueIncomingMessages.AddTail(pEntry);
                } else {
                    delete pChunkData;
                }

                dwOffset += dwChunkSize;
                dwRemainingSize -= dwChunkSize;

                if (dwRemainingSize == 0) {
                    break;
                }
            }
            delete m_pOutgoingBuffers[0].pData;
        } else {
            BYTE* pNewData = NULL;
            DWORD dwNewDataSize;
            if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::MG, CNetwork::dword_85E69C) == 0) {
                pNewData = pData + CNetwork::dword_85E694;
                dwNewDataSize = dwDataSize - CNetwork::dword_85E694;
            } else if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0) {
                pNewData = pData + CNetwork::dword_85E68C;
                dwNewDataSize = dwDataSize - CNetwork::dword_85E68C;
            } else {
                delete m_pOutgoingBuffers[0].pData;
                m_pOutgoingBuffers[0].pData = NULL;
            }

            if (pNewData != NULL) {
                if (g_pChitin->MessageCallback(pNewData, dwNewDataSize) == TRUE) {
                    CNETWORKWINDOW_QUEUEENTRY* newNode = new CNETWORKWINDOW_QUEUEENTRY();
                    newNode->idFrom = nPlayerID;
                    newNode->idTo = nServerID;
                    newNode->pData = pData;
                    newNode->dwSize = dwDataSize;
                    m_lQueueOutgoingMessages.AddTail(newNode);
                } else {
                    delete m_pOutgoingBuffers[0].pData;
                }
            }
        }

        m_pOutgoingBuffers[0].pData = NULL;
        m_pOutgoingBuffers[0].dwSize = 0;

        m_nAckExpected++;
    }

    POSITION pos = m_lQueueOutgoingMessages.GetHeadPosition();
    while (pos != NULL) {
        POSITION posOld = pos;
        CNETWORKWINDOW_QUEUEENTRY* node = m_lQueueOutgoingMessages.GetNext(pos);

        DWORD v1;
        if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::MG, CNetwork::dword_85E69C) == 0) {
            v1 = CNetwork::dword_85E694;
        } else if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0) {
            v1 = CNetwork::dword_85E68C;
        } else {
            v1 = -1;
        }

        if (v1 != -1) {
            BYTE* pData = node->pData + v1;
            if (pData != NULL && g_pChitin->MessageCallback(pData, node->dwSize - v1) == 1) {
                CNETWORKWINDOW_QUEUEENTRY* newNode = new CNETWORKWINDOW_QUEUEENTRY();
                newNode->idFrom = nPlayerID;
                newNode->idTo = nServerID;
                newNode->pData = node->pData;
                newNode->dwSize = node->dwSize;
                m_lQueueOutgoingMessages.AddTail(newNode);
            }
        } else {
            delete node->pData;
            node->pData = NULL;
        }

        m_lQueueOutgoingMessages.RemoveAt(posOld);
        delete node;
    }

    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));

    RemoveFromAllQueues();

    m_nNextFrameToSend = 0;
    m_nTooFar = 1;
    m_nAckExpected = 0;
    m_nNumBuffered = 0;
    m_nFrameExpected = 0;
    m_bNoNak = TRUE;
    m_nPacketTimeout = DEFAULT_PACKET_TIMEOUT;
    m_bAckTimerSet = FALSE;

    // NOTE: Uninline.
    SetPlayerTimeout();

    // NOTE: Uninline.
    SetNoMessageTimeout();

    m_pbTimeOutSet[0] = 0;
    m_pbArrived[0] = FALSE;
    m_pOutgoingBuffers[0].dwSize = 0;
    m_pOutgoingBuffers[0].pData = NULL;
    m_pIncomingBuffers[0].dwSize = 0;
    m_pIncomingBuffers[0].pData = NULL;
    m_pnTimeOut[0] = 0;

    m_bInitialized = TRUE;
}

// 0x7A2D10
void CNetworkWindow::RemoveFromAllQueues()
{
    POSITION pos;
    POSITION oldPos;
    CNETWORKWINDOW_QUEUEENTRY* node;

    if (m_pOutgoingBuffers[0].pData != NULL) {
        delete m_pOutgoingBuffers[0].pData;
        m_pOutgoingBuffers[0].pData = NULL;
        m_pOutgoingBuffers[0].dwSize = 0;
    }

    if (m_pIncomingBuffers[0].pData != NULL) {
        delete m_pIncomingBuffers[0].pData;
        m_pIncomingBuffers[0].pData = NULL;
        m_pIncomingBuffers[0].dwSize = 0;
    }

    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    pos = m_lQueueIncomingMessages.GetHeadPosition();
    while (pos != NULL) {
        oldPos = pos;
        node = m_lQueueIncomingMessages.GetNext(pos);
        m_lQueueIncomingMessages.RemoveAt(oldPos);
        delete node;
    }

    pos = m_lQueueOutgoingMessages.GetHeadPosition();
    while (pos != NULL) {
        oldPos = pos;
        node = m_lQueueOutgoingMessages.GetNext(pos);
        m_lQueueOutgoingMessages.RemoveAt(oldPos);
        delete node;
    }

    m_bInitialized = FALSE;
    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
}

// 0x7A2DE0
BYTE* CNetworkWindow::RemoveFromIncomingQueue(PLAYER_ID& idDPFrom, PLAYER_ID& idDPTo, DWORD& dwDataSize, BOOLEAN& bCompressed)
{
    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    if (m_lQueueIncomingMessages.IsEmpty()) {
        LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        return NULL;
    }

    CNETWORKWINDOW_QUEUEENTRY* node = m_lQueueIncomingMessages.GetHead();
    if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::MG, CNetwork::dword_85E69C) == 0) {
        idDPFrom = node->idFrom;
        idDPTo = node->idTo;
        dwDataSize = node->dwSize - CNetwork::dword_85E694;
        bCompressed = node->pData[5];

        BYTE* pData = new BYTE[dwDataSize];
        if (pData != NULL) {
            memcpy(pData, node->pData + CNetwork::dword_85E694, dwDataSize);

            node = m_lQueueIncomingMessages.RemoveHead();
            delete node->pData;
            delete node;
        }

        LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        return pData;
    }

    if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0) {
        idDPFrom = node->idFrom;
        idDPTo = node->idTo;

        int cnt = 0;
        for (int offset = CNetwork::dword_85E69C + CNetwork::dword_85E698; offset < CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0; offset++) {
            cnt = node->pData[offset] + (cnt << 8);
        }

        POSITION pos = m_lQueueIncomingMessages.GetHeadPosition();
        int remaining = cnt;

        dwDataSize = 0;

        while (pos != NULL && remaining > 0) {
            dwDataSize += node->dwSize;

            if (remaining == cnt) {
                dwDataSize -= CNetwork::dword_85E68C;
            } else {
                if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JB, CNetwork::dword_85E69C) != 0) {
                    CString v1;
                    CString v2;
                    CString v3;
                    CString v4;
                    CString v5;

                    v1.Format("Messages have been misordered.  Please report this bug to Interplay.\n");
                    v2.Format("It is very important that the numbers below are recorded correctly.\n");
                    v3.Format("Communication Parameters: PN:%d FTS:%d TF:%d AE:%d NB:%d FE:%d\n",
                        m_nPlayerNumber,
                        m_nNextFrameToSend,
                        m_nTooFar,
                        m_nAckExpected,
                        m_nNumBuffered,
                        m_nFrameExpected);
                    v4.Format("Message Prefix: %x %x %x %x %x %x %x %x\n",
                        node->pData[0],
                        node->pData[1],
                        node->pData[2],
                        node->pData[3],
                        node->pData[4],
                        node->pData[5],
                        node->pData[6],
                        node->pData[7]);
                    v5.Format("%s %s %s %s",
                        (LPCSTR)v1,
                        (LPCSTR)v2,
                        (LPCSTR)v3,
                        (LPCSTR)v4);

                    // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                    // __LINE__: 1673
                    UTIL_ASSERT_MSG(FALSE, (LPCSTR)v5);
                }

                dwDataSize -= CNetwork::dword_85E690;
            }

            remaining--;
            m_lQueueIncomingMessages.GetNext(pos);
        }

        if (remaining == 0) {
            BYTE* pMessage = new BYTE[dwDataSize];
            if (pMessage != NULL) {
                DWORD dwOffset = 0;
                DWORD dwHeaderLength;

                remaining = cnt;

                while (remaining > 0) {
                    node = m_lQueueIncomingMessages.RemoveHead();

                    if (remaining == cnt) {
                        bCompressed = node->pData[5];
                        dwHeaderLength = CNetwork::dword_85E68C;
                    } else {
                        dwHeaderLength = CNetwork::dword_85E690;
                    }

                    memcpy(pMessage + dwOffset,
                        node->pData + dwHeaderLength,
                        node->dwSize - dwHeaderLength);
                    dwOffset += node->dwSize - dwHeaderLength;

                    delete node->pData;
                    delete node;

                    remaining--;
                }
            }

            LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
            return pMessage;
        }

        LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        return NULL;
    }

    if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JB, CNetwork::dword_85E69C) == 0) {
        CString v1;
        CString v2;
        CString v3;
        CString v4;
        CString v5;

        v1.Format("Messages have been misordered.  Please report this bug to Interplay.\n");
        v2.Format("It is very important that the numbers below are recorded correctly.\n");
        v3.Format("Communication Parameters: PN:%d FTS:%d TF:%d AE:%d NB:%d FE:%d\n",
            m_nPlayerNumber,
            m_nNextFrameToSend,
            m_nTooFar,
            m_nAckExpected,
            m_nNumBuffered,
            m_nFrameExpected);
        v4.Format("Message Prefix: %x %x %x %x %x %x %x %x\n",
            node->pData[0],
            node->pData[1],
            node->pData[2],
            node->pData[3],
            node->pData[4],
            node->pData[5],
            node->pData[6],
            node->pData[7]);
        v5.Format("%s %s %s %s",
            (LPCSTR)v1,
            (LPCSTR)v2,
            (LPCSTR)v3,
            (LPCSTR)v4);

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 1807
        UTIL_ASSERT_MSG(FALSE, (LPCSTR)v5);
    }

    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
    return NULL;
}

// 0x7A3380
BYTE* CNetworkWindow::RemoveFromIncomingQueueSpecific(BYTE nSpecMsgType, BYTE nSpecMsgSubType, PLAYER_ID& idDPFrom, PLAYER_ID& idDPTo, DWORD& dwDataSize, BOOLEAN& bCompressed)
{
    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    if (m_lQueueIncomingMessages.IsEmpty()) {
        LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        return NULL;
    }

    POSITION pos = m_lQueueIncomingMessages.GetHeadPosition();
    while (pos != NULL) {
        CNETWORKWINDOW_QUEUEENTRY* node = m_lQueueIncomingMessages.GetAt(pos);
        BYTE* pData = node->pData;
        if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::MG, CNetwork::dword_85E69C) == 0) {
            pData += CNetwork::dword_85E694;
            if (pData[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED
                && pData[CNetwork::SPEC_MSG_TYPE] == nSpecMsgType
                && pData[CNetwork::SPEC_MSG_SUBTYPE] == nSpecMsgSubType) {
                idDPFrom = node->idFrom;
                idDPTo = node->idTo;
                dwDataSize = node->dwSize - CNetwork::dword_85E694;

                BYTE* pMessage = new BYTE[dwDataSize];
                if (pMessage != NULL) {
                    bCompressed = node->pData[5];
                    memcpy(pMessage, pData + CNetwork::dword_85E694, dwDataSize);

                    m_lQueueIncomingMessages.RemoveAt(pos);
                    delete node->pData;
                    delete node;
                }

                LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
                return pMessage;
            }
        }

        if (memcmp(pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JM, CNetwork::dword_85E69C) == 0) {
            idDPFrom = node->idFrom;
            idDPTo = node->idTo;

            int cnt = 0;
            for (int offset = CNetwork::dword_85E69C + CNetwork::dword_85E698; offset < CNetwork::dword_85E69C + CNetwork::dword_85E698 + CNetwork::dword_85E6A0; offset++) {
                cnt = pData[offset] + (cnt << 8);
            }

            pData += CNetwork::dword_85E68C;
            if (pData[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED
                && pData[CNetwork::SPEC_MSG_TYPE] == nSpecMsgType
                && pData[CNetwork::SPEC_MSG_SUBTYPE] == nSpecMsgSubType) {
                POSITION otherPos = pos;
                int remaining = cnt;

                dwDataSize = 0;

                while (otherPos != NULL && remaining > 0) {
                    node = m_lQueueIncomingMessages.GetAt(otherPos);
                    dwDataSize += node->dwSize + CNetwork::SPEC_MSG_HEADER_LENGTH;

                    if (remaining == cnt) {
                        dwDataSize -= CNetwork::dword_85E68C;
                    } else {
                        if (memcmp(node->pData + CNetwork::dword_85E698, (LPCSTR)CNetwork::JB, CNetwork::dword_85E69C) != 0) {
                            CString v1;
                            CString v2;
                            CString v3;
                            CString v4;
                            CString v5;

                            v1.Format("Messages have been misordered.  Please report this bug to Interplay.\n");
                            v2.Format("It is very important that the numbers below are recorded correctly.\n");
                            v3.Format("Communication Parameters: PN:%d FTS:%d TF:%d AE:%d NB:%d FE:%d\n",
                                m_nPlayerNumber,
                                m_nNextFrameToSend,
                                m_nTooFar,
                                m_nAckExpected,
                                m_nNumBuffered,
                                m_nFrameExpected);
                            v4.Format("Message Prefix: %x %x %x %x %x %x %x %x\n",
                                node->pData[0],
                                node->pData[1],
                                node->pData[2],
                                node->pData[3],
                                node->pData[4],
                                node->pData[5],
                                node->pData[6],
                                node->pData[7]);
                            v5.Format("%s %s %s %s",
                                (LPCSTR)v1,
                                (LPCSTR)v2,
                                (LPCSTR)v3,
                                (LPCSTR)v4);

                            // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                            // __LINE__: 2049
                            UTIL_ASSERT_MSG(FALSE, (LPCSTR)v5);
                        }

                        dwDataSize -= CNetwork::dword_85E690;
                    }

                    remaining--;
                }

                if (remaining == 0) {
                    BYTE* pMessage = new BYTE[dwDataSize];
                    if (pMessage != NULL) {
                        DWORD dwOffset = 0;
                        DWORD dwHeaderLength;

                        otherPos = pos;
                        remaining = cnt;

                        while (remaining > 0) {
                            POSITION posToRemove = otherPos;
                            node = m_lQueueIncomingMessages.GetNext(otherPos);

                            if (remaining == cnt) {
                                bCompressed = node->pData[5];
                                dwHeaderLength = CNetwork::dword_85E68C;
                            } else {
                                dwHeaderLength = CNetwork::dword_85E690;
                            }

                            memcpy(pMessage + dwOffset,
                                node->pData + dwHeaderLength,
                                node->dwSize - dwHeaderLength);
                            dwOffset += node->dwSize - dwHeaderLength;

                            m_lQueueIncomingMessages.RemoveAt(posToRemove);
                            delete node->pData;
                            delete node;

                            remaining--;
                        }
                    }

                    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
                    return pMessage;
                }

                LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
                return NULL;
            } else {
                while (cnt - 1 > 0) {
                    m_lQueueIncomingMessages.GetNext(pos);
                    cnt--;
                }
            }
        }

        m_lQueueIncomingMessages.GetNext(pos);
    }

    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
    return NULL;
}

// NOTE: Inlined.
void CNetworkWindow::SetPlayerTimeout()
{
    DWORD dwTickCount = GetTickCount();
    DWORD dwMaxTickCount = GetTickCount() + DEFAULT_PLAYER_TIMEOUT;
    if (dwTickCount > dwMaxTickCount) {
        dwMaxTickCount = -1;
    }
    m_nPlayerTimeout = dwMaxTickCount;
}

// NOTE: Inlined.
void CNetworkWindow::SetNoMessageTimeout()
{
    DWORD dwTickCount = GetTickCount();
    DWORD dwMaxTickCount = GetTickCount() + DEFAULT_SENDPACKET_TIMEOUT;
    if (dwTickCount > dwMaxTickCount) {
        dwMaxTickCount = DWORD_MAX;
    }
    m_nNoMessageTimeout = dwMaxTickCount;
}

// 0x7A3B50
void CNetworkWindow::ReceiveCall(BYTE* pData, DWORD dwSize)
{
    BOOLEAN v1 = FALSE;

    EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

    BOOLEAN v2 = TRUE;
    m_bSomethingHappened = TRUE;

    if (pData[0] == 3) {
        AddToIncomingQueue(g_pChitin->cNetwork.GetPlayerID(m_nPlayerNumber),
            g_pChitin->cNetwork.m_idLocalPlayer,
            pData,
            dwSize);
    } else {
        WORD nFrame = pData[2] + (pData[1] << 8);
        WORD nAck = pData[4] + (pData[3] << 8);

        if (pData[0] == 0) {
            if (nFrame != m_nFrameExpected && m_bNoNak == TRUE) {
                FrameSend(2, 0);
            }

            if ((m_nFrameExpected <= nFrame && nFrame < m_nTooFar)
                || (m_nTooFar < m_nFrameExpected && m_nFrameExpected <= nFrame)
                || (nFrame < m_nTooFar && m_nTooFar < m_nFrameExpected)) {
                if (!m_pbArrived[0]) {
                    m_pbArrived[0] = TRUE;
                    m_pIncomingBuffers[0].idFrom = g_pChitin->cNetwork.GetPlayerID(m_nPlayerNumber);
                    m_pIncomingBuffers[0].idTo = g_pChitin->cNetwork.m_idLocalPlayer;
                    m_pIncomingBuffers[0].pData = pData;
                    m_pIncomingBuffers[0].dwSize = dwSize;

                    v1 = TRUE;

                    while (m_pbArrived[0] == TRUE) {
                        AddToIncomingQueue(m_pIncomingBuffers[0].idFrom,
                            m_pIncomingBuffers[0].idTo,
                            m_pIncomingBuffers[0].pData,
                            m_pIncomingBuffers[0].dwSize);
                        m_bNoNak = TRUE;
                        m_pbArrived[0] = FALSE;
                        m_pIncomingBuffers[0].pData = NULL;
                        m_pIncomingBuffers[0].dwSize = 0;
                        m_nFrameExpected++;
                        m_nTooFar++;
                        m_bAckTimerSet = TRUE;
                        m_nAckTimer = m_nPacketTimeout + GetTickCount();
                        if (m_nAckTimer > CNetworkWindow::MAX_TIMEOUT_TICK_COUNT) {
                            m_nAckTimer = 0;
                        }
                    }
                }
            }
        } else if (pData[0] == 2) {
            WORD nAckNext = nAck + 1;

            if ((m_nAckExpected <= nAckNext && nAckNext < m_nNextFrameToSend)
                || (m_nNextFrameToSend < m_nAckExpected && m_nAckExpected <= nAckNext)
                || (nAckNext < m_nNextFrameToSend && m_nNextFrameToSend < m_nAckExpected)) {
                FrameSend(0, nAckNext);
            }
        }

        while ((m_nAckExpected <= nAck && nAck < m_nNextFrameToSend)
            || (m_nNextFrameToSend < m_nAckExpected && m_nAckExpected <= nAck)
            || (nAck < m_nNextFrameToSend && m_nNextFrameToSend < m_nAckExpected)) {
            m_nNumBuffered--;
            m_pbTimeOutSet[0] = FALSE;
            m_pnTimeOut[0] = 0;

            if (m_pOutgoingBuffers[0].pData != NULL) {
                delete m_pOutgoingBuffers[0].pData;
                m_pOutgoingBuffers[0].pData = NULL;
            }

            m_pOutgoingBuffers[0].dwSize = 0;
            m_nAckExpected++;
        }

        v2 = v1;
    }

    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));

    // NOTE: Uninline.
    SetPlayerTimeout();

    if (!v2) {
        delete pData;
    }
}

// 0x7A3DF0
void CNetworkWindow::TimeoutCall()
{
    DWORD dwTickCount = GetTickCount();
    BOOLEAN v1 = FALSE;
    WORD nFrameNumber;

    if (m_pbTimeOutSet[0] && dwTickCount > m_pnTimeOut[0]) {
        v1 = TRUE;
        nFrameNumber = m_pOutgoingBuffers[0].pData[2] + (m_pOutgoingBuffers[0].pData[1] << 8);
    }

    if (v1 == TRUE) {
        FrameSend(0, nFrameNumber);
        m_bSomethingHappened = TRUE;
    }

    if (m_bAckTimerSet == TRUE && GetTickCount() >= m_nAckTimer) {
        FrameSend(1, 0);
        m_bSomethingHappened = TRUE;
    }

    if (m_nNoMessageTimeout < GetTickCount()) {
        if (g_pChitin->cNetwork.GetPlayerID(m_nPlayerNumber) != g_pChitin->cNetwork.m_idLocalPlayer) {
            FrameSend(1, 0);
            m_bSomethingHappened = TRUE;
        }

        // NOTE: Uninline.
        SetNoMessageTimeout();
    }

    if (m_bSomethingHappened == TRUE) {
        m_nNextEvent = GetTickCount();
        return;
    }

    BOOLEAN bHasNextEvent = FALSE;
    DWORD nNextEvent;

    if (m_bAckTimerSet == TRUE) {
        bHasNextEvent = TRUE;
        nNextEvent = m_nAckTimer;
    }

    if (m_pbTimeOutSet[0] == TRUE) {
        if (bHasNextEvent) {
            if (m_pnTimeOut[0] < nNextEvent) {
                nNextEvent = m_pnTimeOut[0];
            }
        } else {
            nNextEvent = m_pnTimeOut[0];
            bHasNextEvent = TRUE;
        }
    }

    if (!bHasNextEvent) {
        nNextEvent = -1;
    }

    m_nNextEvent = nNextEvent;
}

// -----------------------------------------------------------------------------

// 0x7A3FD0
CNetwork::CNetwork()
{
    field_9D = 0;
    field_9E = 0;
    field_796 = 0;
    InitializeCriticalSection(&field_F52);
    InitializeCriticalSection(&field_F6A);
    m_nApplicationGuid = GUID_NULL;
    m_bApplicationGuidDefined = FALSE;
    field_19 = 0;
    m_bServiceProviderEnumerated = FALSE;
    m_bServiceProviderSelected = FALSE;
    m_nServiceProvider = -1;
    m_nTotalServiceProviders = 0;
    m_serviceProviderGuids[m_nTotalServiceProviders++] = GUID_NULL;
    m_bConnectionInitialized = FALSE;
    m_pServiceProviderConnections[0] = NULL;
    m_pServiceProviderConnections[1] = NULL;
    m_pServiceProviderConnections[2] = NULL;
    m_pServiceProviderConnections[3] = NULL;
    m_pServiceProviderConnections[4] = NULL;
    m_bModemEnumerated = 0;
    m_bModemAddressSelected = FALSE;
    m_nModemAddress = -1;
    m_nTotalModemAddresses = 0;
    m_sIPAddress = "";
    m_sPhoneNumber = "";
    m_nSerialBaudRate = 57600;
    m_nSerialPort = 1;
    m_bSessionEnumerated = FALSE;

    for (INT nSession = 0; nSession < CNETWORK_MAX_SESSIONS; nSession++) {
        m_psSessionName[nSession] = "";
        m_pSessionGuid[nSession] = GUID_NULL;
        m_pbSessionPasswordRequired[nSession] = FALSE;
    }

    m_bSessionSelected = FALSE;
    m_nSession = -1;
    m_nTotalSessions = 0;
    m_guidSession = GUID_NULL;
    m_bSessionNameToMake = FALSE;
    m_sSessionNameToMake = "";
    m_bSessionPasswordEnabled = FALSE;
    m_sSessionPassword = "";
    m_bAllowNewConnections = TRUE;
    m_pLastSessionBuffer = NULL;
    m_dwLastSessionBufferSize = 0;
    m_bConnectionEstablished = FALSE;
    m_bIsHost = FALSE;
    field_6EA = 0;
    m_bSessionOptionsDefined = 0;
    m_nMaxPlayers = CNETWORK_MAX_PLAYERS;
    m_dwSessionFlags = 0;
    m_bPlayerNameToMake = FALSE;
    m_bPlayerCreated = FALSE;
    m_idLocalPlayer = 0;
    m_sLocalPlayerName = "";
    m_nHostPlayer = -1;
    m_nLocalPlayer = -1;
    m_nTotalPlayers = 0;

    for (INT nPlayerNumber = 0; nPlayerNumber < CNETWORK_MAX_PLAYERS; nPlayerNumber++) {
        m_pPlayerID[nPlayerNumber] = 0;
        field_772[nPlayerNumber] = 0;
        m_psPlayerName[nPlayerNumber] = "";
        m_pbPlayerEnumerateFlag[nPlayerNumber] = FALSE;
        m_pbPlayerVisible[nPlayerNumber] = FALSE;
        m_pSlidingWindow[nPlayerNumber].Initialize(nPlayerNumber);
    }

    m_SystemWindow.Initialize(-1);

    for (unsigned int m = 0; m <= 255; m++) {
        unsigned int v1 = m;
        for (unsigned int bit = 8; bit > 0; bit--) {
            if ((v1 & 1) != 0) {
                v1 >>= 1;
                v1 ^= 0xEDB88320;
            } else {
                v1 >>= 1;
            }
        }
        m_dwCRC32[m] = v1;
    }

    for (INT nSlot = 0; nSlot < CNETWORK_MAX_PLAYERS; nSlot++) {
        field_71A[nSlot] = FALSE;
    }
}

// 0x7A4440
CNetwork::~CNetwork()
{
    if (m_bConnectionEstablished) {
        OnCloseSession();
    }

    m_bServiceProviderEnumerated = FALSE;

    for (INT nServiceProvider = 0; nServiceProvider < CNETWORK_MAX_SERVICE_PROVIDERS; nServiceProvider++) {
        if (m_pServiceProviderConnections[nServiceProvider] != NULL) {
            operator delete(m_pServiceProviderConnections[nServiceProvider]);
        }
    }

    DeleteCriticalSection(&field_F6A);
    DeleteCriticalSection(&field_F52);
}

// 0x7A46E0
void CNetwork::AddServiceProviderToList(const CString& sServiceProviderName, const GUID& sServiceProviderGuid, LPVOID lpConnection, size_t size)
{
    if (m_pServiceProviderConnections[m_nTotalServiceProviders] != NULL) {
        operator delete(m_pServiceProviderConnections[m_nTotalServiceProviders]);
    }

    m_pServiceProviderConnections[m_nTotalServiceProviders] = operator new(size);
    if (m_pServiceProviderConnections[m_nTotalServiceProviders] != NULL) {
        memcpy(m_pServiceProviderConnections[m_nTotalServiceProviders], lpConnection, size);

        m_serviceProviderGuids[m_nTotalServiceProviders] = sServiceProviderGuid;
        m_serviceProviderNames[m_nTotalServiceProviders] = sServiceProviderName;

        m_nTotalServiceProviders++;
    }
}

// 0x7A5340
BOOLEAN CNetwork::EnumerateServiceProviders()
{
    m_bServiceProviderEnumerated = TRUE;
    m_bServiceProviderSelected = FALSE;
    m_nServiceProvider = -1;
    m_nTotalServiceProviders = 0;
    m_serviceProviderGuids[0] = GUID_NULL;
    m_nTotalServiceProviders++;

    return TRUE;
}

// 0x7A5480
INT CNetwork::FindModemAddress(const CString& sAddress)
{
    if (!m_bModemEnumerated) {
        return -1;
    }

    for (INT nAddressNumber = 0; nAddressNumber < m_nTotalModemAddresses; nAddressNumber++) {
        if (m_psModemAddress[nAddressNumber].Compare(sAddress) == 0) {
            return nAddressNumber;
        }
    }

    return -1;
}

// 0x7A54E0
BOOLEAN CNetwork::GetModemAddress(INT nAddressNumber, CString& sAddress)
{
    if (!m_bModemEnumerated) {
        return FALSE;
    }

    if (!(nAddressNumber >= 0 && nAddressNumber < m_nTotalModemAddresses)) {
        return FALSE;
    }

    sAddress = m_psModemAddress[nAddressNumber];

    return TRUE;
}

// 0x7A5520
BOOLEAN CNetwork::GetServiceProviderType(INT nServiceProviderNumber, INT& nServiceProviderType)
{
    if (!(nServiceProviderNumber >= 0 && nServiceProviderNumber < m_nTotalServiceProviders)) {
        nServiceProviderType = SERV_PROV_NULL;
        return FALSE;
    }

    nServiceProviderType = SERV_PROV_NULL;

    if (IsEqualGUID(m_serviceProviderGuids[nServiceProviderNumber], GUID_NULL)) {
        nServiceProviderType = SERV_PROV_NULL;
    }

    return TRUE;
}

// 0x7A5600
BOOLEAN CNetwork::InitializeConnectionToServiceProvider(BOOLEAN bHostingGame)
{
    m_bConnectionInitialized = TRUE;
    return TRUE;
}

// 0x7A5720
void CNetwork::RemoveInitializeConnection()
{
    m_bConnectionInitialized = FALSE;
}

// 0x7A5760
BOOLEAN CNetwork::SelectModemAddress(INT nAddressNumber)
{
    if (!m_bModemEnumerated) {
        return FALSE;
    }

    if (!(nAddressNumber >= 0 && nAddressNumber < m_nTotalModemAddresses)) {
        return FALSE;
    }

    m_nModemAddress = nAddressNumber;
    m_bModemAddressSelected = TRUE;

    return TRUE;
}

// 0x7A57A0
BOOLEAN CNetwork::SelectServiceProvider(INT nServiceProviderNumber)
{
    if (!m_bServiceProviderEnumerated) {
        return FALSE;
    }

    if (!(nServiceProviderNumber >= 0 && nServiceProviderNumber < m_nTotalServiceProviders)) {
        return FALSE;
    }

    m_nServiceProvider = nServiceProviderNumber;
    m_bServiceProviderSelected = TRUE;

    return m_bServiceProviderEnumerated;
}

// 0x7A57D0
void CNetwork::SetApplicationOptions(BOOLEAN bKeepAlive, BOOLEAN bMigrateHost)
{
    m_bSessionOptionsDefined = TRUE;
    m_dwSessionFlags = 0;
}

// 0x7A5800
void CNetwork::SetJoinEnabled(BOOLEAN bJoinEnabled)
{
    if (m_bAllowNewConnections != bJoinEnabled) {
        m_bAllowNewConnections = bJoinEnabled;

        g_pChitin->cGameSpy.sub_4D1EC0(TRUE);
    }
}

// 0x7A5830
BOOLEAN CNetwork::SetSerialBaudRate(UINT nBaudRate)
{
    switch (nBaudRate) {
    case 14400:
    case 19200:
    case 38400:
    case 56000:
    case 57600:
    case 115200:
    case 128000:
    case 256000:
        m_nSerialBaudRate = nBaudRate;
        return TRUE;
    }

    return FALSE;
}

// 0x7A5890
BOOLEAN CNetwork::SetSerialPort(UINT nPort)
{
    if (nPort != 0 || nPort > 4) {
        return FALSE;
    }

    m_nSerialPort = nPort;

    return TRUE;
}

// 0x7A58B0
void CNetwork::UnselectModemAddress()
{
    m_bModemAddressSelected = FALSE;
    m_nModemAddress = -1;
}

// 0x7A5A20
void CNetwork::CloseSession(BOOLEAN bAIResponsible)
{
    if (bAIResponsible != TRUE) {
        g_pChitin->OnMultiplayerSessionToClose();
        SleepEx(1000, FALSE);
        if (m_bConnectionEstablished == TRUE) {
            OnCloseSession();
        }
    } else {
        OnCloseSession();
    }
}

// 0x7A5A60
void CNetwork::OnCloseSession()
{
    INT nPlayer;

    if (!m_bConnectionEstablished) {
        return;
    }

    g_pChitin->OnMultiplayerSessionClose();

    for (nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        m_pPlayerID[nPlayer] = 0;
        m_psPlayerName[nPlayer] = "";
        m_pbPlayerEnumerateFlag[nPlayer] = FALSE;
        m_pbPlayerVisible[nPlayer] = FALSE;
    }

    m_nTotalPlayers = 0;

    m_bConnectionInitialized = FALSE;
    m_bSessionEnumerated = FALSE;

    for (INT nSession = 0; nSession < CNETWORK_MAX_SESSIONS; nSession++) {
        m_psSessionName[nSession] = "";
        m_pSessionGuid[nSession] = GUID_NULL;
        m_pbSessionPasswordRequired[nSession] = FALSE;
    }

    m_bSessionSelected = FALSE;
    m_nSession = -1;
    m_nTotalSessions = 0;
    m_guidSession = GUID_NULL;
    m_bSessionNameToMake = FALSE;
    m_sSessionNameToMake = "";
    m_bAllowNewConnections = FALSE;
    m_bSessionPasswordEnabled = FALSE;
    m_sSessionPassword = "";

    EnterCriticalSection(&field_F6A);

    if (m_pLastSessionBuffer != NULL) {
        delete m_pLastSessionBuffer;
        m_pLastSessionBuffer = NULL;
    }

    m_dwLastSessionBufferSize = 0;

    LeaveCriticalSection(&field_F6A);

    m_bConnectionEstablished = FALSE;
    m_bIsHost = FALSE;
    field_6EA = 0;
    m_bSessionOptionsDefined = FALSE;
    m_nMaxPlayers = CNETWORK_MAX_PLAYERS;
    m_dwSessionFlags = 0;
    m_bPlayerNameToMake = FALSE;
    m_bPlayerCreated = FALSE;
    m_idLocalPlayer = 0;
    m_sLocalPlayerName = "";
    m_nHostPlayer = -1;
    m_nLocalPlayer = -1;
    m_nTotalPlayers = 0;

    for (nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        m_pPlayerID[nPlayer] = 0;
        m_psPlayerName[nPlayer] = "";
        m_pbPlayerEnumerateFlag[nPlayer] = FALSE;
        m_pbPlayerVisible[nPlayer] = FALSE;
        m_pSlidingWindow[nPlayer].Initialize(nPlayer);
    }

    m_SystemWindow.RemoveFromAllQueues();

    for (int index = 0; index < 6; index++) {
        field_71A[index] = FALSE;
    }
}

// 0x7A5D00
BOOLEAN CNetwork::CheckSessionStatus(BOOLEAN bInThreadLoop)
{
    if (m_bConnectionEstablished != TRUE) {
        return TRUE;
    }

    return TRUE;
}

// 0x7A5F00
BOOLEAN CNetwork::EnumerateSessions(BOOLEAN a1, BOOLEAN a2)
{
    return FALSE;
}

// 0x7A61D0
BOOLEAN CNetwork::sub_7A61D0()
{
    return TRUE;
}

// 0x7A61E0
BOOLEAN CNetwork::GetPasswordRequiredForSelectedSession()
{
    if (!(m_nSession >= 0 && m_nSession < m_nTotalSessions)) {
        return FALSE;
    }

    return m_pbSessionPasswordRequired[m_nSession];
}

// 0x7A6200
BOOLEAN CNetwork::GetSessionName(INT nSession, CString& sSessionName)
{
    if (!(nSession >= 0 && nSession < m_nTotalSessions)) {
        return FALSE;
    }

    sSessionName = m_psSessionName[nSession];

    return TRUE;
}

// 0x7A6230
BOOLEAN CNetwork::GetSessionGUID(INT nSession, GUID& sessionGuid)
{
    if (!(nSession >= 0 && nSession < m_nTotalSessions)) {
        return FALSE;
    }

    sessionGuid = m_pSessionGuid[nSession];

    return TRUE;
}

// 0x7A6270
BOOLEAN CNetwork::HostNewSession()
{
    if (m_bConnectionInitialized != TRUE) {
        return FALSE;
    }

    if (m_bConnectionEstablished) {
        return FALSE;
    }

    if (!m_bSessionNameToMake) {
        return FALSE;
    }

    m_bAllowNewConnections = TRUE;
    m_bConnectionEstablished = TRUE;
    m_bIsHost = TRUE;
    m_nHostPlayer = -1;
    m_nTotalPlayers = 0;
    m_nLocalPlayer = -1;

    for (int index = 0; index < CNETWORK_MAX_PLAYERS; index++) {
        m_pPlayerID[index] = 0;
        m_psPlayerName[index] = "";
        m_pbPlayerEnumerateFlag[index] = FALSE;
        m_pbPlayerVisible[index] = FALSE;
        m_pSlidingWindow[index].Initialize(index);
    }

    m_SystemWindow.Initialize(-1);

    g_pChitin->OnMultiplayerSessionOpen(m_sJoinedGame, m_sDroppedGame, m_sLeftGame);

    return TRUE;
}

// 0x7A64B0
BOOLEAN CNetwork::JoinSelectedSession(INT& nErrorCode)
{
    return FALSE;
}

// 0x7A6680
BOOLEAN CNetwork::SelectSession(INT nSession)
{
    if (!m_bSessionEnumerated) {
        return FALSE;
    }

    if (!(nSession >= 0 && nSession < m_nTotalSessions)) {
        return FALSE;
    }

    m_nSession = nSession;
    m_guidSession = m_pSessionGuid[nSession];
    m_bSessionSelected = TRUE;

    return TRUE;
}

// 0x7A66E0
BOOLEAN CNetwork::SetInSessionOptions()
{
    if (m_bConnectionEstablished != TRUE) {
        return FALSE;
    }

    if (m_bIsHost != TRUE) {
        return FALSE;
    }

    return TRUE;
}

// 0x7A6800
void CNetwork::SetMaxPlayers(INT nMaxPlayers)
{
    if (nMaxPlayers <= 6) {
        m_nMaxPlayers = nMaxPlayers;
    }
}

// 0x7A6820
void CNetwork::UnselectSession()
{
    m_bSessionSelected = FALSE;
    m_nSession = -1;
}

// 0x7A6960
BOOLEAN CNetwork::CreatePlayer(INT& nErrorCode)
{
    if (m_bConnectionEstablished != TRUE) {
        m_bPlayerCreated = FALSE;
        return FALSE;
    }

    if (!m_bPlayerNameToMake) {
        m_bPlayerCreated = FALSE;
        return FALSE;
    }

    if (FindPlayerIDByName(m_sLocalPlayerName, TRUE) != 0) {
        m_bPlayerCreated = FALSE;
        nErrorCode = ERROR_PLAYEREXISTS;
        return FALSE;
    }

    char szPlayerName[256];
    memcpy(szPlayerName, m_sLocalPlayerName.GetBuffer(m_sLocalPlayerName.GetLength()), m_sLocalPlayerName.GetLength());
    szPlayerName[m_sLocalPlayerName.GetLength()] = '\0';

    INT nSlot;
    for (nSlot = 0; nSlot < CNETWORK_MAX_PLAYERS; nSlot++) {
        if (!field_71A[nSlot]) {
            break;
        }
    }

    if (nSlot >= CNETWORK_MAX_PLAYERS) {
        return FALSE;
    }

    m_idLocalPlayer = nSlot + 1;
    field_702[nSlot] = szPlayerName;
    field_71A[nSlot] = TRUE;

    m_bPlayerCreated = TRUE;
    AddPlayerToList(m_idLocalPlayer, m_sLocalPlayerName, m_bIsHost, TRUE);

    nErrorCode = ERROR_NONE;
    return TRUE;
}

// 0x7A6BC0
BOOLEAN CNetwork::AddPlayerToList(PLAYER_ID dpID, const CString& sPlayerName, BOOLEAN bIsHost, BOOLEAN bMakeVisible)
{
    INT nPlayerNumber;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
    // __LINE__: 6907
    UTIL_ASSERT(dpID != 0);

    INT nFound = -1;
    BOOLEAN bFound = FALSE;

    for (nPlayerNumber = 0; nPlayerNumber < CNETWORK_MAX_PLAYERS; nPlayerNumber++) {
        if (bFound) {
            break;
        }

        if (nFound == -1 && m_pPlayerID[nPlayerNumber] == 0) {
            nFound = nPlayerNumber;
        }

        if (m_pPlayerID[nPlayerNumber] != 0) {
            if (dpID == m_pPlayerID[nPlayerNumber]) {
                bFound = TRUE;
                m_pbPlayerEnumerateFlag[nPlayerNumber] = TRUE;

                if (bIsHost == TRUE) {
                    m_nHostPlayer = nPlayerNumber;
                }
            }

            if (m_idLocalPlayer == m_pPlayerID[nPlayerNumber]) {
                m_nLocalPlayer = nPlayerNumber;
            }
        }
    }

    if (bFound) {
        return FALSE;
    }

    for (nPlayerNumber = 0; nPlayerNumber < CNETWORK_MAX_PLAYERS; nPlayerNumber++) {
        if (field_772[nPlayerNumber] == dpID) {
            return FALSE;
        }
    }

    if (nFound == -1) {
        return FALSE;
    }

    m_pPlayerID[nFound] = dpID;
    m_psPlayerName[nFound] = sPlayerName;
    m_pbPlayerVisible[nFound] = bMakeVisible;

    if (bIsHost == TRUE) {
        m_nHostPlayer = nFound;
    }

    if (dpID == m_idLocalPlayer) {
        m_nLocalPlayer = nFound;
    }

    m_pbPlayerEnumerateFlag[nFound] = TRUE;
    m_nTotalPlayers++;

    m_pSlidingWindow[nFound].Initialize(nFound);

    g_pChitin->OnMultiplayerPlayerJoin(m_pPlayerID[nFound], m_psPlayerName[nFound]);

    return TRUE;
}

// 0x7A6D10
BOOLEAN CNetwork::KickPlayer(PLAYER_ID dpID, BOOLEAN bAIResponsible)
{
    BOOLEAN v1;
    BOOLEAN v2;
    CString sKickedPlayerName;
    INT nPlayer;

    for (nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pPlayerID[nPlayer] == dpID) {
            sKickedPlayerName = m_psPlayerName[nPlayer];
        }

        if (field_772[nPlayer] == dpID) {
            v1 = TRUE;
        }
    }

    if (RemovePlayerFromList(dpID, bAIResponsible) == TRUE) {
        if (v1 == TRUE) {
            v2 = TRUE;
        } else {
            for (nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
                if (field_772[nPlayer] == 0) {
                    field_772[nPlayer] = dpID;
                    v2 = TRUE;
                    break;
                }

                if (field_772[nPlayer] == dpID) {
                    v2 = TRUE;
                    break;
                }
            }
        }
    } else {
        v2 = FALSE;
    }

    if (v2 == TRUE) {
        CString sText;
        sText.Format(" %s: %s", (LPCSTR)sKickedPlayerName, (LPCSTR)m_sDroppedGame);
        sText.SetAt(0, sText.GetLength() - 1);

        BYTE* pData = CreateCopyMessage((LPCSTR)sText,
            sText.GetLength(),
            0,
            0,
            1);
        if (pData != NULL) {
            m_SystemWindow.AddToIncomingQueue(0,
                m_idLocalPlayer,
                pData,
                sText.GetLength() + 12);
        }
    }

    return v2;
}

// 0x7A6EA0
BOOLEAN CNetwork::RemovePlayerFromList(PLAYER_ID dpID, BOOLEAN bAIResponsible)
{
    INT nPlayerToRemove = -1;
    BOOLEAN bFound = FALSE;

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pPlayerID[nPlayer] == dpID) {
            nPlayerToRemove = nPlayer;
            bFound = TRUE;
        }
    }

    if (bFound != TRUE) {
        return bFound;
    }

    if (nPlayerToRemove == m_nHostPlayer) {
        if (bAIResponsible != TRUE) {
            g_pChitin->OnMultiplayerSessionToClose();
            SleepEx(1000, FALSE);

            if (m_bConnectionEstablished == TRUE) {
                OnCloseSession();
            }
        } else {
            OnCloseSession();
        }
    } else {
        g_pChitin->OnMultiplayerPlayerLeave(m_pPlayerID[nPlayerToRemove], m_psPlayerName[nPlayerToRemove]);
        m_pSlidingWindow[nPlayerToRemove].ShutDown();
        m_pPlayerID[nPlayerToRemove] = 0;
        m_psPlayerName[nPlayerToRemove] = "";
        m_pbPlayerEnumerateFlag[nPlayerToRemove] = FALSE;
        m_pbPlayerVisible[nPlayerToRemove] = FALSE;
        m_nTotalPlayers--;
    }

    return TRUE;
}

// 0x7A6F80
void CNetwork::EnumeratePlayers(BOOLEAN bProtectList)
{
    if (!m_bConnectionEstablished) {
        return;
    }

    if (!bProtectList) {
        for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
            m_pPlayerID[nPlayer] = 0;
            m_psPlayerName[nPlayer] = "";
            m_pbPlayerVisible[nPlayer] = FALSE;
        }

        m_nTotalPlayers = 0;
    }

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        m_pbPlayerEnumerateFlag[nPlayer] = FALSE;
    }
}

// NOTE: Inlined.
BOOLEAN CNetwork::MakePlayerVisible(PLAYER_ID playerID)
{
    BOOLEAN bResult = FALSE;

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pPlayerID[nPlayer] == playerID && !m_pbPlayerVisible[nPlayer]) {
            m_pbPlayerVisible[nPlayer] = TRUE;
            g_pChitin->OnMultiplayerPlayerVisible(playerID);

            bResult = TRUE;
        }
    }

    return bResult;
}

// 0x7A7160
BOOLEAN CNetwork::MakePlayersVisible()
{
    BOOLEAN bResult = FALSE;

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pPlayerID[nPlayer] != 0 && !m_pbPlayerVisible[nPlayer]) {
            // NOTE: Uninline.
            bResult |= MakePlayerVisible(m_pPlayerID[nPlayer]);
        }
    }

    return bResult;
}

// 0x7A7220
PLAYER_ID CNetwork::GetPlayerID(INT nPlayerNumber)
{
    if (m_pbPlayerVisible[nPlayerNumber] == TRUE) {
        return m_pPlayerID[nPlayerNumber];
    } else {
        return 0;
    }
}

// 0x7A7240
void CNetwork::GetPlayerName(INT nPlayerNumber, CString& sPlayerName)
{
    if (m_pbPlayerVisible[nPlayerNumber] == TRUE) {
        sPlayerName = m_psPlayerName[nPlayerNumber];
    } else {
        sPlayerName = "";
    }
}

// 0x7A7280
PLAYER_ID CNetwork::GetRawPlayerID(INT nPlayerNumber)
{
    return m_pPlayerID[nPlayerNumber];
}

// 0x7A7290
void CNetwork::GetRawPlayerName(INT nPlayerNumber, CString& sPlayerName)
{
    sPlayerName = m_psPlayerName[nPlayerNumber];
}

// 0x7A72B0
PLAYER_ID CNetwork::FindPlayerIDByName(const CString& sPlayerName, BOOLEAN bInvisible)
{
    for (INT nPlayerNumber = 0; nPlayerNumber < CNETWORK_MAX_PLAYERS; nPlayerNumber++) {
        if (m_psPlayerName[nPlayerNumber].Compare(sPlayerName) == 0) {
            if (bInvisible || m_pbPlayerVisible[nPlayerNumber]) {
                return m_pPlayerID[nPlayerNumber];
            } else {
                return 0;
            }
        }
    }

    return 0;
}

// 0x7A7320
INT CNetwork::FindPlayerLocationByID(PLAYER_ID playerID, BOOLEAN bInvisible)
{
    if (playerID == 0) {
        return -1;
    }

    for (INT nPlayerNumber = 0; nPlayerNumber < CNETWORK_MAX_PLAYERS; nPlayerNumber++) {
        if (m_pPlayerID[nPlayerNumber] == playerID) {
            if (bInvisible || m_pbPlayerVisible[nPlayerNumber]) {
                return nPlayerNumber;
            } else {
                return -1;
            }
        }
    }

    return -1;
}

// 0x7A7360
INT CNetwork::FindPlayerLocationByName(const CString& sPlayerName, BOOLEAN bInvisible)
{
    if (sPlayerName.Compare("") == 0) {
        return -1;
    }

    for (INT nPlayerNumber = 0; nPlayerNumber < CNETWORK_MAX_PLAYERS; nPlayerNumber++) {
        if (m_psPlayerName[nPlayerNumber].Compare(sPlayerName) == 0) {
            if (bInvisible || m_pbPlayerVisible[nPlayerNumber]) {
                return nPlayerNumber;
            } else {
                return -1;
            }
        }
    }

    return -1;
}

// 0x7A73D0
void CNetwork::sub_7A73D0(CString& a1)
{
    a1 = "";
}

// 0x7A75B0
INT CNetwork::ThreadLoop()
{
    INT nPlayer;

    DWORD dwNextEvent = GetTickCount() + 10;

    for (nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pSlidingWindow[nPlayer].m_bInitialized == TRUE
            && m_pSlidingWindow[nPlayer].m_nNextEvent <= dwNextEvent) {
            dwNextEvent = m_pSlidingWindow[nPlayer].m_nNextEvent;
        }
    }

    INT nDelta = max(min(dwNextEvent - GetTickCount(), 10), 0);

    if (!m_bConnectionEstablished) {
        nDelta = -1;
    }

    return nDelta;
}

// 0x7A77D0
void CNetwork::SlidingWindowTimeouts()
{
    if (m_bConnectionEstablished == TRUE) {
        for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
            if (m_pPlayerID[nPlayer] != 0 && m_pSlidingWindow[nPlayer].m_bInitialized == TRUE) {
                m_pSlidingWindow[nPlayer].TimeoutCall();
                if (m_nHostPlayer == m_nLocalPlayer) {
                    if (m_nHostPlayer != nPlayer && m_pSlidingWindow[nPlayer].m_nPlayerTimeout < GetTickCount()) {
                        KickPlayer(m_pPlayerID[nPlayer], FALSE);
                    }
                } else {
                    if (m_nHostPlayer == nPlayer && m_pSlidingWindow[nPlayer].m_nPlayerTimeout < GetTickCount()) {
                        g_pChitin->OnMultiplayerSessionToClose();
                        SleepEx(1000, FALSE);
                        if (m_bConnectionEstablished == TRUE) {
                            OnCloseSession();
                        }
                    }
                }
            }
        }
    }
}

// 0x7A7890
BYTE* CNetwork::FetchFrame(PLAYER_ID& dpIDFrom, DWORD& dwSize)
{
    BYTE* pData = new BYTE[MINIMAL_PACKET_SIZE];
    if (pData == NULL) {
        return NULL;
    }

    // NOTE: iwd2r: The original calls the CNetwork vtable slot `+0x64` (the
    // DirectPlay `Receive` method on m_lpDirectPlay, COM stdcall) in a loop
    // inside this critical section — `Receive(&dpIDFrom_local, &to_local, 0,
    // pData, &dwDataSize)` — reallocating pData on DPERR_BUFFERTOOSMALL
    // (0x8877001E, DPERR_NOMEMORY 0x8007000E on a failed realloc) and
    // returning the frame on DP_OK. DirectPlay is gone, so no frame can be
    // received and the function always takes the exe's failure path: the
    // caller's dwSize is zeroed, the buffer is deleted and NULL is returned.
    EnterCriticalSection(&(g_pChitin->cNetwork.field_F6A));
    LeaveCriticalSection(&(g_pChitin->cNetwork.field_F6A));

    dwSize = 0;
    delete pData;

    return NULL;
}

// 0x7A7980
BOOLEAN CNetwork::HandleSystemMessage(BYTE* pData, DWORD dwSize)
{
    switch (*reinterpret_cast<DWORD*>(pData)) {
    case 3: /* DPSYS_CREATEPLAYERORGROUP */
        if (1) {
            CString sText;
            CString sPlayerName;

            PLAYER_ID dpId = *reinterpret_cast<PLAYER_ID*>(pData + 8);

            // NOTE: the exe reads the DPNAME long-name pointer (at +0x20 in the
            // create message), not the short-name slot the stock SDK structs
            // would suggest.
            sPlayerName = *reinterpret_cast<char**>(pData + 0x20);
            if (AddPlayerToList(dpId, sPlayerName, FALSE, FALSE) == TRUE) {
                sText.Format(" %s: %s", (LPCSTR)sPlayerName, (LPCSTR)m_sJoinedGame);  // re-lint: disable RE001 RE003 (exe is ANSI; Ghidra recovers the narrow bytes)
                sText.SetAt(0, sText.GetLength() - 1);

                BYTE* pMessage = CreateCopyMessage(sText,
                    sText.GetLength(),
                    0,
                    0,
                    1);
                if (pMessage != NULL) {
                    m_SystemWindow.AddToIncomingQueue(0,
                        m_idLocalPlayer,
                        pMessage,
                        sText.GetLength() + 12);
                }
            }
        }
        return TRUE;
    case 5: /* DPSYS_DESTROYPLAYERORGROUP */
        if (1) {
            CString sText;
            CString sPlayerName;

            PLAYER_ID dpId = *reinterpret_cast<PLAYER_ID*>(pData + 8);

            // NOTE: the exe reads the DPNAME long-name pointer (at +0x24 in the
            // destroy message), not the short-name slot the stock SDK structs
            // would suggest.
            sPlayerName = *reinterpret_cast<char**>(pData + 0x24);
            if (RemovePlayerFromList(dpId, FALSE) == TRUE) {
                sText.Format(" %s: %s", (LPCSTR)sPlayerName, (LPCSTR)m_sDroppedGame);  // re-lint: disable RE001 RE003 (exe is ANSI; Ghidra recovers the narrow bytes)
                sText.SetAt(0, sText.GetLength() - 1);

                BYTE* pMessage = CreateCopyMessage(sText,
                    sText.GetLength(),
                    0,
                    0,
                    1);
                if (pMessage != NULL) {
                    m_SystemWindow.AddToIncomingQueue(0,
                        m_idLocalPlayer,
                        pMessage,
                        sText.GetLength() + 12);
                }
            } else {
                for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
                    if (field_772[nPlayer] == dpId) {
                        field_772[nPlayer] = 0;
                    }
                }
            }
        }
        return TRUE;
    case 0x31: /* DPSYS_SESSIONLOST */
        g_pChitin->OnMultiplayerSessionToClose();
        SleepEx(1000, FALSE);
        if (m_bConnectionEstablished == TRUE) {
            OnCloseSession();
        }
        return TRUE;
    case 0x101: /* DPSYS_HOST */
    case 0x103: /* DPSYS_SETPLAYERORGROUPNAME */
    case 0x104: /* DPSYS_SETSESSIONDESC */
        return TRUE;
    }

    return FALSE;
}

// 0x7A7BF0
void CNetwork::AddMessageToWindow(PLAYER_ID idTo, DWORD dwFlags, BYTE* pData, DWORD dwSize)
{
    // NOTE: Uninline.
    INT nPlayer = FindPlayerLocationByID(idTo, TRUE);

    if (nPlayer == -1) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 8339
        UTIL_ASSERT_MSG(FALSE, "CNetwork::AddMessageToWindow: Invalid SendTo ID.");
        return;
    }

    BOOLEAN bRaw = (dwFlags & SEND_RAW) != 0;

    INT nPackets;
    if (dwSize + 16 >= MAXIMAL_PACKET_SIZE) {
        nPackets = dwSize / MAXIMAL_PACKET_SIZE + 1;
        while (nPackets >= 2 && dwSize - (nPackets - 1) * MAXIMAL_PACKET_SIZE < MINIMAL_PACKET_SIZE) {
            nPackets--;
        }
    } else {
        nPackets = 1;
    }

    DWORD dwOffset = 0;
    DWORD dwRemainingSize = dwSize;
    for (INT nPacket = 0; nPacket < nPackets; nPacket++) {
        DWORD dwChunkSize = MAXIMAL_PACKET_SIZE;
        if (nPacket == nPackets - 1) {
            dwChunkSize = dwRemainingSize;
        }

        if (nPacket != 0) {
            BYTE* pMessage = CreateCopyMessage(pData + dwOffset,
                dwChunkSize,
                bRaw,
                TRUE,
                0);

            EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

            CNETWORKWINDOW_QUEUEENTRY* pEntry = new CNETWORKWINDOW_QUEUEENTRY();
            pEntry->idFrom = m_idLocalPlayer;
            pEntry->idTo = idTo;
            pEntry->pData = pMessage;
            pEntry->dwSize = dwChunkSize + 12;
            m_pSlidingWindow[nPlayer].m_lQueueOutgoingMessages.AddTail(pEntry);

            LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        } else {
            BYTE* pMessage = CreateCopyMessage(pData + dwOffset,
                dwChunkSize,
                bRaw,
                TRUE,
                nPackets);

            EnterCriticalSection(&(g_pChitin->cNetwork.field_F52));

            CNETWORKWINDOW_QUEUEENTRY* pEntry = new CNETWORKWINDOW_QUEUEENTRY();
            pEntry->idFrom = m_idLocalPlayer;
            pEntry->idTo = idTo;
            pEntry->pData = pMessage;
            pEntry->dwSize = dwChunkSize + 16;
            m_pSlidingWindow[nPlayer].m_lQueueOutgoingMessages.AddTail(pEntry);

            LeaveCriticalSection(&(g_pChitin->cNetwork.field_F52));
        }

        dwOffset += dwChunkSize;
        dwRemainingSize -= dwChunkSize;
    }
}

// 0x7A7DF0
BYTE* CNetwork::CreateCopyMessage(const void* lpData, DWORD dwDataSize, BOOLEAN bCompressed, unsigned char a4, INT nFrame)
{
    DWORD dwNewDataSize;
    BYTE* pByteNewData;

    if (a4 == TRUE) {
        if (nFrame == 0) {
            dwNewDataSize = dwDataSize + 12;
        } else {
            dwNewDataSize = dwDataSize + 16;
        }

        pByteNewData = new BYTE[dwNewDataSize];

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 8516
        UTIL_ASSERT_MSG(pByteNewData != NULL, "CNetwork::CreateCopyMessage: new failed.");

        pByteNewData[5] = bCompressed;

        if (nFrame == 0) {
            memcpy(pByteNewData + 10, (LPCSTR)JB, 2);
            memcpy(pByteNewData + 12, lpData, dwDataSize);
        } else {
            memcpy(pByteNewData + 10, (LPCSTR)JM, 2);
            pByteNewData[12] = static_cast<BYTE>(nFrame >> 8);
            pByteNewData[13] = static_cast<BYTE>(nFrame);
            pByteNewData[14] = static_cast<BYTE>(dwDataSize >> 8);
            pByteNewData[15] = static_cast<BYTE>(dwDataSize);
            memcpy(pByteNewData + 16, lpData, dwDataSize);
        }
    } else {
        dwNewDataSize = dwDataSize + 12;
        pByteNewData = new BYTE[dwNewDataSize];
        if (pByteNewData != NULL) {
            pByteNewData[0] = 3;
            pByteNewData[1] = 3;
            pByteNewData[2] = 3;
            pByteNewData[5] = bCompressed;
            memcpy(pByteNewData + 10, (LPCSTR)MG, 2);
            memcpy(pByteNewData + 12, lpData, dwDataSize);
        }
    }

    return pByteNewData;
}

// 0x7A7F20
BYTE* CNetwork::FetchMessage(INT& nMsgFrom, INT& nMsgTo, DWORD& dwSize)
{
    PLAYER_ID idDPFrom;
    PLAYER_ID idDPTo;
    BYTE* pData;
    BYTE* pUncompressedData;
    BOOLEAN bCompressed;

    if (m_SystemWindow.m_bInitialized == TRUE
        && m_SystemWindow.CheckIncomingQueue() == TRUE) {
        pData = m_SystemWindow.RemoveFromIncomingQueue(idDPFrom,
            idDPTo,
            dwSize,
            bCompressed);

        nMsgFrom = -1;
        nMsgTo = m_nLocalPlayer;

        if (!bCompressed) {
            return pData;
        }

        pUncompressedData = UncompressMessage(pData, dwSize);
        delete pData;
        return pUncompressedData;
    }

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pPlayerID[nPlayer] != 0
            && m_pSlidingWindow[nPlayer].m_bInitialized
            && m_pSlidingWindow[nPlayer].CheckIncomingQueue() == TRUE) {
            pData = m_pSlidingWindow[nPlayer].RemoveFromIncomingQueue(idDPFrom,
                idDPTo,
                dwSize,
                bCompressed);

            nMsgFrom = nPlayer;
            nMsgTo = m_nLocalPlayer;

            if (!bCompressed) {
                return pData;
            }

            pUncompressedData = UncompressMessage(pData, dwSize);
            delete pData;
            return pUncompressedData;
        }
    }

    return NULL;
}

// 0x7A8060
BYTE* CNetwork::FetchSpecificMessage(const CString& sPlayerName, BYTE nSpecMsgType, BYTE nSpecMsgSubType, DWORD& dwSize)
{
    // NOTE: Uninline.
    INT nPlayer = FindPlayerLocationByName(sPlayerName, FALSE);
    if (nPlayer == -1) {
        return NULL;
    }

    if (m_pSlidingWindow[nPlayer].m_bInitialized != TRUE) {
        return NULL;
    }

    if (m_pSlidingWindow[nPlayer].CheckIncomingQueueSpecific(nSpecMsgType, nSpecMsgSubType) != TRUE) {
        return NULL;
    }

    PLAYER_ID idDPFrom;
    PLAYER_ID idDPTo;
    DWORD dwDataSize;
    BOOLEAN bCompressed;
    BYTE* pData = m_pSlidingWindow[nPlayer].RemoveFromIncomingQueueSpecific(nSpecMsgType,
        nSpecMsgSubType,
        idDPFrom,
        idDPTo,
        dwDataSize,
        bCompressed);
    if (!bCompressed) {
        return pData;
    }

    BYTE* pUncompressedData = UncompressMessage(pData, dwDataSize);
    delete pData;
    return pUncompressedData;
}

// NOTE: Trailing underscore prevents clashes with `PeekMessage` macro.
//
// 0x7A8150
BOOLEAN CNetwork::PeekMessage_()
{
    if (!m_bConnectionEstablished) {
        return FALSE;
    }

    if (m_SystemWindow.m_bInitialized == TRUE
        && m_SystemWindow.CheckIncomingQueue() == TRUE) {
        return TRUE;
    }

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_pPlayerID[nPlayer] != 0
            && m_pSlidingWindow[nPlayer].m_bInitialized == TRUE
            && m_pSlidingWindow[nPlayer].CheckIncomingQueue() == TRUE) {
            return TRUE;
        }
    }

    return FALSE;
}

// 0x7A81C0
BOOLEAN CNetwork::PeekSpecificMessage(const CString& sPlayerName, BYTE nSpecMsgType, BYTE nSpecMsgSubType)
{
    if (sPlayerName == "") {
        return FALSE;
    }

    for (INT nPlayer = 0; nPlayer < CNETWORK_MAX_PLAYERS; nPlayer++) {
        if (m_psPlayerName[nPlayer] == sPlayerName
            && m_pbPlayerVisible[nPlayer] == TRUE) {
            // NOTE: Probably inlining.
            if (nPlayer != -1
                && m_pSlidingWindow[nPlayer].m_bInitialized == TRUE
                && m_pSlidingWindow[nPlayer].CheckIncomingQueueSpecific(nSpecMsgType, nSpecMsgSubType) == TRUE) {
                return TRUE;
            } else {
                return FALSE;
            }
        }
    }

    return FALSE;
}

// NOTE: Trailing underscore prevents clashes with `SendMessage` macro.
//
// 0x7A8250
BOOLEAN CNetwork::SendMessage_(const CString& sPlayerName, DWORD dwFlags, BYTE* pData, DWORD dwSize)
{
    INT nPlayer;

    if (m_nServiceProvider == SERV_PROV_NULL) {
        return TRUE;
    }

    BYTE* pCompressedData;
    BOOLEAN bCompressed = FALSE;

    PLAYER_ID idTo = 0;
    if ((dwFlags & SEND_ALL_PLAYERS) == 0) {
        // NOTE: Uninline.
        idTo = FindPlayerIDByName(sPlayerName, (dwFlags & SEND_JOINING_PLAYERS) != 0);

        if (idTo == 0) {
            return FALSE;
        }
    }

    if ((dwFlags & SEND_RAW) == 0 && dwSize >= MINIMAL_PACKET_SIZE) {
        DWORD nCompressedLength = dwSize + 16;

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 8937
        UTIL_ASSERT_MSG(nCompressedLength <= STATIC_MESSAGE_SIZE, "CNetwork::SendMessage() method, nCompressedLength > STATIC_MESSAGE_SIZE.");

        pCompressedData = CNetwork::STATIC_MESSAGE_BUFFER;
        bCompressed = TRUE;

        int err = CUtil::Compress(pCompressedData + 4,
            &nCompressedLength,
            pData,
            dwSize,
            7);

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 8944
        UTIL_ASSERT(err == 0);

        nCompressedLength += 4;

        pCompressedData[0] = static_cast<BYTE>(dwSize >> 24);
        pCompressedData[1] = static_cast<BYTE>(dwSize >> 16);
        pCompressedData[2] = static_cast<BYTE>(dwSize >> 8);
        pCompressedData[3] = static_cast<BYTE>(dwSize);
    }

    // NOTE: iwd2r: DirectPlay is gone, so the original's
    // `m_lpDirectPlay == NULL` check always holds here.
    return FALSE;
}

// 0x7A84F0
BOOLEAN CNetwork::SendSpecificMessage(const CString& sPlayerName, DWORD dwFlags, BYTE nSpecMsgType, BYTE nSpecMsgSubType, LPVOID lpData, DWORD nDataSize)
{
    BOOLEAN bSuccess;

    if (m_nServiceProvider == SERV_PROV_NULL) {
        return TRUE;
    }

    BYTE* pMessage = STATIC_MESSAGE_BUFFER;
    BYTE* pMessageDynamicBuffer = NULL;
    DWORD dwSize = nDataSize + SPEC_MSG_HEADER_LENGTH;
    if ((dwFlags & SEND_RAW) == 0 && nDataSize >= MINIMAL_PACKET_SIZE) {
        dwSize += 16;
        if (dwSize > STATIC_MESSAGE_SIZE) {
            pMessageDynamicBuffer = new BYTE[dwSize];
            pMessage = pMessageDynamicBuffer;

            // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
            // __LINE__: 9156
            UTIL_ASSERT(pMessage != NULL);
        }

        pMessage[SPEC_MSG_FLAG] = SPEC_MSG_FLAG_ENABLED;
        pMessage[SPEC_MSG_TYPE] = nSpecMsgType;
        pMessage[SPEC_MSG_SUBTYPE] = nSpecMsgSubType;

        int err = CUtil::Compress(pMessage + 7,
            &dwSize,
            reinterpret_cast<BYTE*>(lpData),
            nDataSize,
            7);

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 9166
        UTIL_ASSERT(err == 0);

        pMessage[SPEC_MSG_HEADER_LENGTH] = static_cast<BYTE>(nDataSize >> 24);
        pMessage[SPEC_MSG_HEADER_LENGTH + 1] = static_cast<BYTE>(nDataSize >> 16);
        pMessage[SPEC_MSG_HEADER_LENGTH + 2] = static_cast<BYTE>(nDataSize >> 8);
        pMessage[SPEC_MSG_HEADER_LENGTH + 3] = static_cast<BYTE>(nDataSize);

        bSuccess = SendMessage_(sPlayerName,
            dwFlags | SEND_RAW | SEND_GUARANTEED,
            pMessage,
            dwSize + 7);
    } else {
        if (dwSize > STATIC_MESSAGE_SIZE) {
            DYNAMIC_MESSAGE_SIZE = dwSize;

            pMessageDynamicBuffer = new BYTE[dwSize];
            pMessage = pMessageDynamicBuffer;

            // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
            // __LINE__: 9191
            UTIL_ASSERT(pMessage != NULL);
        }

        pMessage[SPEC_MSG_FLAG] = SPEC_MSG_FLAG_ENABLED;
        pMessage[SPEC_MSG_TYPE] = nSpecMsgType;
        pMessage[SPEC_MSG_SUBTYPE] = nSpecMsgSubType;

        if (nDataSize != 0) {
            // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
            // __LINE__: 9206
            UTIL_ASSERT_MSG(lpData != NULL, "CNetwork::SendSpecificMessage:  Data size != 0, message = NULL.");
            memcpy(pMessage + SPEC_MSG_HEADER_LENGTH, lpData, nDataSize);
        }

        bSuccess = SendMessage_(sPlayerName,
            dwFlags | SEND_GUARANTEED,
            pMessage,
            dwSize);
    }

    if (pMessageDynamicBuffer != NULL) {
        delete pMessageDynamicBuffer;
    }

    return bSuccess;
}

// 0x7A86E0
BYTE* CNetwork::UncompressMessage(BYTE* pData, DWORD& dwSize)
{
    DWORD dwUncompressedSize;
    BYTE* pUncompressedData;
    int err;

    if (pData[SPEC_MSG_FLAG] == 255) {
        dwUncompressedSize = (pData[SPEC_MSG_HEADER_LENGTH] << 24)
            + (pData[SPEC_MSG_HEADER_LENGTH + 1] << 16)
            + (pData[SPEC_MSG_HEADER_LENGTH + 2] << 8)
            + pData[SPEC_MSG_HEADER_LENGTH + 3];

        pUncompressedData = new BYTE[dwUncompressedSize + SPEC_MSG_HEADER_LENGTH];
        pUncompressedData[SPEC_MSG_FLAG] = pData[SPEC_MSG_FLAG];
        pUncompressedData[SPEC_MSG_TYPE] = pData[SPEC_MSG_TYPE];
        pUncompressedData[SPEC_MSG_SUBTYPE] = pData[SPEC_MSG_SUBTYPE];

        err = CUtil::Uncompress(pUncompressedData + SPEC_MSG_HEADER_LENGTH,
            &dwUncompressedSize,
            pData + 7,
            dwSize - 7);
        if (err != 0) {
            if (err != -5) {
                CString sError;
                sError.Format("z_uncompress returned %d", err);
                sError.Format("Uncompress in network layer failed.  Error code %d.\n", err);
                sError += "Please report this error and look at icewind2.log file, too!";

                // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                // __LINE__: 9320
                UTIL_ASSERT_MSG(FALSE, (LPCSTR)sError);
            }

            delete pUncompressedData;

            dwUncompressedSize = 2 * ((pData[SPEC_MSG_HEADER_LENGTH] << 24) + (pData[SPEC_MSG_HEADER_LENGTH + 1] << 16) + (pData[SPEC_MSG_HEADER_LENGTH + 2] << 8) + pData[SPEC_MSG_HEADER_LENGTH + 3]);

            pUncompressedData = new BYTE[dwUncompressedSize + SPEC_MSG_HEADER_LENGTH];
            pUncompressedData[SPEC_MSG_FLAG] = pData[SPEC_MSG_FLAG];
            pUncompressedData[SPEC_MSG_TYPE] = pData[SPEC_MSG_TYPE];
            pUncompressedData[SPEC_MSG_SUBTYPE] = pData[SPEC_MSG_SUBTYPE];

            err = CUtil::Uncompress(pUncompressedData + SPEC_MSG_HEADER_LENGTH,
                &dwUncompressedSize,
                pData + 7,
                dwSize - 7);
            if (err != 0) {
                CString sError;

                // FIXME: Useless.
                sError.Format("z_uncompress returned %d", err);

                sError.Format("Uncompress in network layer failed.  Error code %d.\n", err);
                sError += "Please report this error and look at icewind2.log file, too!";

                // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                // __LINE__: 9359
                UTIL_ASSERT_MSG(FALSE, (LPCSTR)sError);
            }

            // FIXME: Not used.
            CString sError;
            sError.Format("Network Layer: size of %d succeeded, half size failed\n", err);
        }

        dwSize = dwUncompressedSize + SPEC_MSG_HEADER_LENGTH;
        return pUncompressedData;
    } else {
        dwUncompressedSize = (pData[0] << 24)
            + (pData[1] << 16)
            + (pData[2] << 8)
            + pData[3];

        pUncompressedData = new BYTE[dwUncompressedSize + SPEC_MSG_HEADER_LENGTH];

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
        // __LINE__: 9384
        UTIL_ASSERT(pUncompressedData != NULL);

        err = CUtil::Uncompress(pUncompressedData,
            &dwUncompressedSize,
            pData + 4,
            dwSize - 4);
        if (err != 0) {
            if (err != -5) {
                CString sError;

                // FIXME: Useless.
                sError.Format("z_uncompress returned %d", err);

                sError.Format("Uncompress in network layer failed.  Error code %d.\n", err);
                sError += "Please report this error and look at icewind2.log file, too!";

                // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                // __LINE__: 9396
                UTIL_ASSERT_MSG(FALSE, (LPCSTR)sError);
            }

            delete pUncompressedData;

            dwUncompressedSize = 2 * ((pData[0] << 24) + (pData[1] << 16) + (pData[2] << 8) + pData[3]);

            pUncompressedData = new BYTE[dwUncompressedSize];

            // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
            // __LINE__: 9419
            UTIL_ASSERT(pUncompressedData != NULL);

            err = CUtil::Uncompress(pUncompressedData,
                &dwUncompressedSize,
                pData + 4,
                dwSize - 4);
            if (err != 0) {
                CString sError;

                // FIXME: Useless.
                sError.Format("z_uncompress returned %d", err);

                sError.Format("Uncompress in network layer failed.  Error code %d.\n", err);
                sError += "Please report this error and look at icewind2.log file, too!";

                // __FILE__: C:\Projects\Icewind2\src\chitin\ChNetwork.cpp
                // __LINE__: 9431
                UTIL_ASSERT_MSG(FALSE, (LPCSTR)sError);
            }

            // FIXME: Not used.
            CString sError;
            sError.Format("Network Layer: size of %d succeeded, half size failed\n", err);
        }

        dwSize = dwUncompressedSize;
        return pUncompressedData;
    }
}

// 0x452B40
INT CNetwork::GetServiceProvider()
{
    return m_nServiceProvider;
}

// 0x452B50
BOOLEAN CNetwork::GetSessionOpen()
{
    return m_bConnectionEstablished;
}

// 0x452B60
BOOLEAN CNetwork::GetSessionHosting()
{
    return m_bIsHost;
}

// NOTE: Inlined.
PLAYER_ID CNetwork::GetHostPlayerID()
{
    return m_nHostPlayer != -1 ? m_pPlayerID[m_nHostPlayer] : 0;
}

// NOTE: Inlined.
void CNetwork::GetHostPlayerName(CString& sHostName)
{
    if (m_nHostPlayer != -1) {
        sHostName = m_psPlayerName[m_nHostPlayer];
    } else {
        sHostName = "";
    }
}
