#include "CGameSpy.h"

#include "CBaldurChitin.h"
#include "CChitin.h"

// #binary-identical
// 0x4D0930
CGameSpy::CGameSpy()
{
    m_dwLastHeartbeat = 0;
    m_nPort = 0;
    m_nResponseLength = 0;
    m_pResponse = NULL;
    m_bHeartbeatActive = 0;
    m_nQueryCounter = 0;
    m_nQueryId = 0;
    m_nRetries = 0;
    m_bWSAStarted = 0;
    m_bHostResolved = 0;
}

// #binary-identical
// 0x4D0960
CGameSpy::~CGameSpy()
{
    if (m_bWSAStarted == 1) {
        WSACleanup();
    }
    m_bWSAStarted = 0;
    m_bHeartbeatActive = 0;
}

// The GameSpy query code's stream encoding: 0x4D0520 is the RC4 key schedule
// (a 256-byte S-box plus the two cursor bytes at +0x100/+0x101) and 0x4D05A0
// the stream step, used on the six-byte `secure` challenge.
//
// NOTE: The stream step adds the *data byte* to i (0x4D05D8:
// `lea ecx, [ebx + ecx + 1]` with ebx = pData[n]), which standard RC4 does
// not. Both sides of the protocol encode the same plaintext (the client's
// challenge), so the exchange stays consistent; the port keeps the form the
// bytes have.
//
// 0x4D0520
static void GameSpyRc4KeySchedule(const BYTE* pKey, INT nKeyLength, BYTE* pState)
{
    for (INT i = 0; i < 0x100; i++) {
        pState[i] = static_cast<BYTE>(i);
    }
    pState[0x100] = 0;
    pState[0x101] = 0;

    BYTE nJ = 0;
    for (INT i = 0; i < 0x100; i++) {
        nJ = static_cast<BYTE>(nJ + pKey[i % nKeyLength] + pState[i]);
        BYTE nSwap = pState[i];
        pState[i] = pState[nJ];
        pState[nJ] = nSwap;
    }
}

// 0x4D05A0
static void GameSpyRc4Crypt(BYTE* pData, INT nLength, BYTE* pState)
{
    BYTE nI = pState[0x100];
    BYTE nJ = pState[0x101];

    for (INT n = 0; n < nLength; n++) {
        nI = static_cast<BYTE>(nI + pData[n] + 1);
        nJ = static_cast<BYTE>(nJ + pState[nI]);
        BYTE nSwap = pState[nI];
        pState[nI] = pState[nJ];
        pState[nJ] = nSwap;
        pData[n] ^= pState[static_cast<BYTE>(pState[nI] + pState[nJ])];
    }

    pState[0x100] = nI;
    pState[0x101] = nJ;
}

// Three bytes to the four 6-bit values the GameSpy number encoding carries.
//
// 0x4D0680
static void GameSpyDecode6Bits(const BYTE* pIn, BYTE* pOut)
{
    pOut[0] = static_cast<BYTE>(pIn[0] >> 2);
    pOut[1] = static_cast<BYTE>(((pIn[0] & 0x03) << 4) + (pIn[1] >> 4));
    pOut[2] = static_cast<BYTE>(((pIn[1] & 0x0F) << 2) + (pIn[2] >> 6));
    pOut[3] = static_cast<BYTE>(pIn[2] & 0x3F);
}

// A 6-bit value to its ASCII digit: A-Z, a-z, 0-9, '+', '/', and a space past
// the range.
//
// 0x4D06D0
static BYTE GameSpyEncode6Bits(BYTE nValue)
{
    if (nValue < 26) {
        return static_cast<BYTE>('A' + nValue);
    }
    if (nValue < 52) {
        return static_cast<BYTE>('a' + nValue - 26);
    }
    if (nValue < 62) {
        return static_cast<BYTE>('0' + nValue - 52);
    }
    if (nValue == 62) {
        return '+';
    }
    if (nValue == 63) {
        return '/';
    }
    return ' ';
}

// Starts winsock if it is not running and resolves master.gamespy.com through
// WSAAsyncGetHostByName: the request posts the 0x402 message to the game
// window, whose handler sets +0x26 (CChitin::WndProc), and this waits for it
// in up to ten 300 ms slices before cancelling the request. On success the
// first address of the returned HOSTENT is kept at +0x06.
//
// NOTE: The original returns from its two failure paths without leaving the
// reply section (#design-flaw). The port leaves it: the paths are unreachable
// when winsock starts, and on this engine the inert WSAStartup always takes
// the first one, so reproducing the leak would strand the section.
//
// 0x4D0980
BOOLEAN CGameSpy::sub_4D0980()
{
    CString sLocation("master.gamespy.com");

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    if (m_bWSAStarted == FALSE) {
        WSADATA wsaData;
        WSAStartup(2, &wsaData);

        if (wsaData.wVersion == 2) {
            m_bWSAStarted = TRUE;
        } else {
            WSACleanup();
            m_bWSAStarted = FALSE;
        }

        if (m_bWSAStarted == FALSE) {
            LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
            return FALSE;
        }
    }

    HWND hWnd = NULL;
    if (g_pChitin->GetWnd() != NULL) {
        hWnd = g_pChitin->GetWnd()->GetSafeHwnd();
    }

    BYTE hostBuffer[0x400];
    HANDLE hTask = WSAAsyncGetHostByName(hWnd, 0x402, sLocation,
        reinterpret_cast<char*>(hostBuffer), 0x400);
    m_bHostResolved = FALSE;

    BYTE* pHostent = NULL;
    if (hTask == NULL) {
        m_bWSAStarted = FALSE;
        WSACleanup();
    } else {
        BYTE nTries = 0;
        do {
            nTries++;
            SleepEx(300, FALSE);
            if (m_bHostResolved != FALSE) {
                break;
            }
        } while (nTries < 10);

        if (m_bHostResolved == FALSE) {
            WSACancelAsyncRequest(hTask);
            m_bWSAStarted = FALSE;
            WSACleanup();
        } else {
            pHostent = hostBuffer;
        }
    }

    if (m_bWSAStarted == TRUE) {
        if (pHostent != NULL) {
            // HOSTENT: h_addr_list at +0x0C, its first entry the address in
            // network order.
            BYTE** ppAddrList = *reinterpret_cast<BYTE***>(pHostent + 0xC);
            m_nMasterAddress = *reinterpret_cast<unsigned long*>(ppAddrList[0]);
        } else {
            m_bWSAStarted = FALSE;
            WSACleanup();
        }
    }

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return m_bWSAStarted;
}

// Takes one datagram off the query socket (+0x0E): a peek first (flags 2)
// proves one is pending, then the zeroed buffer is read for real (flags 0).
// The sender's address, its host-order port and the length go back to the
// caller, which owns the buffer.
//
// 0x4D0B80
BOOLEAN CGameSpy::sub_4D0B80(SOCKET hSocket, unsigned long* pAddress, unsigned short* pPort, BYTE** ppData, DWORD* pLength)
{
    BYTE* pData = new BYTE[0x100];
    if (pData == NULL) {
        return FALSE;
    }
    memset(pData, 0, 0x100);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    int nFromLength = sizeof(addr);
    int nResult = recvfrom(hSocket, reinterpret_cast<char*>(pData), 0x100, 2, &addr, &nFromLength);
    if (nResult == -1) {
        delete[] pData;
        return FALSE;
    }

    memset(pData, 0, 0x100);
    nFromLength = sizeof(addr);
    nResult = recvfrom(hSocket, reinterpret_cast<char*>(pData), 0x100, 0, &addr, &nFromLength);
    if (nResult == -1) {
        delete[] pData;
        return FALSE;
    }

    *pAddress = addr.sin_addr.s_addr;
    *pPort = ntohs(addr.sin_port);
    *pLength = static_cast<DWORD>(nResult);
    *ppData = pData;
    return TRUE;
}

// Appends the query id (`\queryid\<id>.<counter>` and a newline) to the reply
// buffer at +0x22, sends the whole buffer to the client from the query
// socket, then zeroes it and resets its length.
//
// 0x4D0C70
BOOLEAN CGameSpy::sub_4D0C70(unsigned long nAddress, unsigned short nPort)
{
    if (m_pResponse == NULL) {
        return FALSE;
    }

    m_nQueryCounter++;
    CString sQueryId;
    sQueryId.Format("\\queryid\\%d.%d\n", m_nQueryId, m_nQueryCounter);
    INT nLength = sQueryId.GetLength();
    memcpy(m_pResponse + m_nResponseLength, sQueryId.GetBuffer(), nLength);
    m_nResponseLength += nLength;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(nPort);
    addr.sin_addr.s_addr = nAddress;
    sendto(m_hQuerySocket, reinterpret_cast<const char*>(m_pResponse), m_nResponseLength, 0,
        &addr, sizeof(addr));

    memset(m_pResponse, 0, 0x578);
    m_nResponseLength = 0;
    return TRUE;
}

// Finds `\<key>\` in the query and copies the value up to the next backslash.
// An empty value returns TRUE with no buffer.
//
// NOTE: The scan walks with a BYTE index compared against the full length
// (0x4D0E5C: `and eax, 0xff`), so a query longer than 255 bytes wraps the
// scan instead of rejecting it; kept as the bytes have it.
//
// 0x4D0DA0
BOOLEAN CGameSpy::sub_4D0DA0(const char* pData, DWORD nLength, const CString& sKey, BYTE** ppValue, DWORD* pValueLength)
{
    *ppValue = NULL;
    *pValueLength = 0;

    CString sFind("\\");
    sFind += sKey;
    sFind += "\\";

    CString sData(pData, static_cast<int>(nLength));
    int nIndex = sData.Find(sFind);
    if (nIndex == -1) {
        return FALSE;
    }

    BYTE nStart = static_cast<BYTE>(nIndex + sFind.GetLength());
    BYTE nEnd = nStart;
    while (nEnd < nLength && pData[nEnd] != '\\') {
        nEnd++;
    }

    DWORD nValueLength = static_cast<BYTE>(nEnd - nStart);
    *pValueLength = nValueLength;
    if (nValueLength != 0) {
        BYTE* pValue = new BYTE[nValueLength];
        *ppValue = pValue;
        if (pValue == NULL) {
            *pValueLength = 0;
            return FALSE;
        }
        memcpy(pValue, pData + nStart, nValueLength);
    }

    return TRUE;
}

// Builds and appends the `\validate\` answer: the six-byte `secure` challenge
// encoded with the game code (RC4, then eight 6-bit characters), then flushes
// the reply.
//
// 0x4D0F40
BOOLEAN CGameSpy::sub_4D0F40(unsigned long nAddress, unsigned short nPort, const BYTE* pSecure)
{
    if (pSecure != NULL) {
        CString sCode;
        g_pChitin->GetGameSpyCode(sCode);

        BYTE key[7];
        BYTE data[6];
        for (INT i = 0; i < 6; i++) {
            data[i] = pSecure[i];
            // The original reads six bytes from the code's buffer whatever its
            // length; only the first strlen(key) bytes are used and the engine's
            // code is six bytes, so pad rather than read past the CString.
            key[i] = i < sCode.GetLength()
                ? static_cast<BYTE>(static_cast<const char*>(sCode)[i])
                : 0;
        }
        // The original writes the NUL right after the six key bytes
        // (0x4D0FDC / 0x4D12B9), so strlen() stops there even though the game
        // code itself has no terminator in the six copied bytes.
        key[6] = 0;

        BYTE state[0x102];
        GameSpyRc4KeySchedule(key, static_cast<INT>(strlen(reinterpret_cast<const char*>(key))), state);
        GameSpyRc4Crypt(data, 6, state);

        char chars[8];
        for (INT nGroup = 0; nGroup < 2; nGroup++) {
            BYTE nValues[4];
            nValues[0] = static_cast<BYTE>(data[nGroup * 3] >> 2);
            nValues[1] = static_cast<BYTE>(((data[nGroup * 3] & 0x03) << 4) + (data[nGroup * 3 + 1] >> 4));
            nValues[2] = static_cast<BYTE>(((data[nGroup * 3 + 1] & 0x0F) << 2) + (data[nGroup * 3 + 2] >> 6));
            nValues[3] = static_cast<BYTE>(data[nGroup * 3 + 2] & 0x3F);
            for (INT i = 0; i < 4; i++) {
                chars[nGroup * 4 + i] = static_cast<char>(GameSpyEncode6Bits(nValues[i]));
            }
        }

        CString sValidate("\\validate\\");
        INT nLength = sValidate.GetLength() + 8;
        BYTE* pBuffer = new BYTE[nLength];
        if (pBuffer == NULL) {
            delete[] m_pResponse;
            return FALSE;
        }
        memcpy(pBuffer, sValidate.GetBuffer(), sValidate.GetLength());
        memcpy(pBuffer + sValidate.GetLength(), chars, 8);

        if (m_pResponse != NULL) {
            if (m_nResponseLength + nLength >= 0x546) {
                sub_4D0C70(nAddress, nPort);
            }
            memcpy(m_pResponse + m_nResponseLength, pBuffer, nLength);
            m_nResponseLength += nLength;
        }
        delete[] pBuffer;
    }

    sub_4D0C70(nAddress, nPort);
    return TRUE;
}

// The same `\validate\` answer, then `\final\`, flushed once.
//
// 0x4D1220
BOOLEAN CGameSpy::sub_4D1220(unsigned long nAddress, unsigned short nPort, const BYTE* pSecure)
{
    if (pSecure != NULL) {
        CString sCode;
        g_pChitin->GetGameSpyCode(sCode);

        BYTE key[7];
        BYTE data[6];
        for (INT i = 0; i < 6; i++) {
            data[i] = pSecure[i];
            key[i] = i < sCode.GetLength()
                ? static_cast<BYTE>(static_cast<const char*>(sCode)[i])
                : 0;
        }
        // The original writes the NUL right after the six key bytes
        // (0x4D0FDC / 0x4D12B9), so strlen() stops there even though the game
        // code itself has no terminator in the six copied bytes.
        key[6] = 0;

        BYTE state[0x102];
        GameSpyRc4KeySchedule(key, static_cast<INT>(strlen(reinterpret_cast<const char*>(key))), state);
        GameSpyRc4Crypt(data, 6, state);

        char chars[8];
        for (INT nGroup = 0; nGroup < 2; nGroup++) {
            BYTE nIn[3];
            nIn[0] = data[nGroup * 3];
            nIn[1] = data[nGroup * 3 + 1];
            nIn[2] = data[nGroup * 3 + 2];
            BYTE nOut[4];
            GameSpyDecode6Bits(nIn, nOut);
            for (INT i = 0; i < 4; i++) {
                chars[nGroup * 4 + i] = static_cast<char>(GameSpyEncode6Bits(nOut[i]));
            }
        }

        CString sValidate("\\validate\\");
        INT nLength = sValidate.GetLength() + 8;
        BYTE* pBuffer = new BYTE[nLength];
        if (pBuffer == NULL) {
            delete[] m_pResponse;
            return FALSE;
        }
        memcpy(pBuffer, sValidate.GetBuffer(), sValidate.GetLength());
        memcpy(pBuffer + sValidate.GetLength(), chars, 8);

        if (m_pResponse != NULL) {
            if (m_nResponseLength + nLength >= 0x546) {
                sub_4D0C70(nAddress, nPort);
            }
            memcpy(m_pResponse + m_nResponseLength, pBuffer, nLength);
            m_nResponseLength += nLength;
        }
        delete[] pBuffer;
    }

    CString sFinal("\\final\\");
    INT nLength = sFinal.GetLength();
    BYTE* pBuffer = new BYTE[nLength];
    if (pBuffer == NULL) {
        delete[] m_pResponse;
        return FALSE;
    }
    memcpy(pBuffer, sFinal.GetBuffer(), nLength);

    if (m_pResponse != NULL) {
        if (m_nResponseLength + nLength >= 0x546) {
            sub_4D0C70(nAddress, nPort);
        }
        memcpy(m_pResponse + m_nResponseLength, pBuffer, nLength);
        m_nResponseLength += nLength;
    }
    delete[] pBuffer;

    sub_4D0C70(nAddress, nPort);
    return TRUE;
}

// The reply handler: runs the query's keys in the protocol's order - basic,
// info, rules, players, status (which answers with all four response types),
// and echo - appending each answer to the reply buffer and flushing it with
// the query id. The `secure` challenge is copied out first and rides along on
// every validate.
//
// 0x4D15D0
BOOLEAN CGameSpy::sub_4D15D0(unsigned long nAddress, unsigned short nPort, const BYTE* pData, DWORD nLength)
{
    m_nQueryId++;
    if (m_nQueryId > 99) {
        m_nQueryId = 0;
    }
    m_nQueryCounter = 0;

    m_pResponse = new BYTE[0x578];
    if (m_pResponse == NULL) {
        return FALSE;
    }
    m_nResponseLength = 0;

    BYTE* pSecure = NULL;
    BYTE* pValue = NULL;
    DWORD nValueLength = 0;

    {
        CString sKey("secure");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            if (pValue != NULL) {
                pSecure = new BYTE[6];
                if (pSecure == NULL) {
                    delete[] m_pResponse;
                    return FALSE;
                }
                memcpy(pSecure, pValue, 6);
                if (pValue != NULL && nValueLength != 0) {
                    delete[] pValue;
                }
            }
        }
    }

    {
        CString sKey("basic");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(0, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (pValue != NULL && nValueLength != 0) {
                delete[] pValue;
            }
        }
    }

    {
        CString sKey("info");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(1, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (pValue != NULL && nValueLength != 0) {
                delete[] pValue;
            }
        }
    }

    {
        CString sKey("rules");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(2, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (pValue != NULL && nValueLength != 0) {
                delete[] pValue;
            }
        }
    }

    {
        CString sKey("players");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(3, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (pValue != NULL && nValueLength != 0) {
                delete[] pValue;
            }
        }
    }

    {
        CString sKey("status");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(0, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(1, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(2, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }
            {
                BYTE* pResponse = NULL;
                DWORD nResponseLength = 0;
                if (g_pChitin->GetGameSpyResponse(3, &pResponse, &nResponseLength)) {
                    if (m_pResponse != NULL) {
                        if (m_nResponseLength + static_cast<INT>(nResponseLength) >= 0x546) {
                            sub_4D0C70(nAddress, nPort);
                        }
                        memcpy(m_pResponse + m_nResponseLength, pResponse, nResponseLength);
                        m_nResponseLength += static_cast<INT>(nResponseLength);
                    }
                    if (nResponseLength != 0) {
                        delete[] pResponse;
                    }
                }
            }
            if (pValue != NULL && nValueLength != 0) {
                delete[] pValue;
            }
        }
    }

    {
        CString sKey("echo");
        if (sub_4D0DA0(reinterpret_cast<const char*>(pData), nLength, sKey, &pValue, &nValueLength)) {
            if (m_nResponseLength != 0) {
                sub_4D0F40(nAddress, nPort, pSecure);
            }

            CString sEcho("\\echo\\");
            if (nValueLength < 0x546) {
                INT nEchoLength = sEcho.GetLength() + static_cast<INT>(nValueLength);
                BYTE* pBuffer = new BYTE[nEchoLength];
                if (pBuffer == NULL) {
                    delete[] m_pResponse;
                    return FALSE;
                }
                memcpy(pBuffer, sEcho.GetBuffer(), sEcho.GetLength());
                if (nValueLength != 0) {
                    memcpy(pBuffer + sEcho.GetLength(), pValue, nValueLength);
                }
                if (m_pResponse != NULL) {
                    if (m_nResponseLength + nEchoLength >= 0x546) {
                        sub_4D0C70(nAddress, nPort);
                    }
                    memcpy(m_pResponse + m_nResponseLength, pBuffer, nEchoLength);
                    m_nResponseLength += nEchoLength;
                }
                delete[] pBuffer;
                if (pValue != NULL && nValueLength != 0) {
                    delete[] pValue;
                }
            } else {
                delete[] m_pResponse;
                return FALSE;
            }
        }
    }

    if (m_nResponseLength != 0) {
        sub_4D1220(nAddress, nPort, pSecure);
    }

    if (pSecure != NULL) {
        delete[] pSecure;
    }
    delete[] m_pResponse;
    return TRUE;
}

// Sends the master server one heartbeat: the query string is
// `\heartbeat\<port>\gamename\<name>\` and, when a1 is 1, also the
// `statechanged\` key, each ending in a newline. The message goes to the
// master address at +0x06, port htons(0x6CFC), from the send socket at +0x0A.
//
// NOTE: The original returns from its two allocation failures without leaving
// the reply section (#design-flaw); the port leaves it, for the reason in
// sub_4D0980's NOTE (the path is a heap failure, not the normal one).
//
// 0x4D1EC0
BOOLEAN CGameSpy::sub_4D1EC0(BOOL a1)
{
    CString sGameSpyName;

    if (m_bHeartbeatActive == FALSE || g_pChitin->cNetwork.m_bIsHost == FALSE) {
        return FALSE;
    }

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    char* pQuery = new char[0x50];
    if (pQuery == NULL) {
        LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
        return FALSE;
    }

    g_pChitin->GetGameSpyGameName(sGameSpyName);
    if (a1 == 1) {
        sprintf(pQuery, "\\heartbeat\\%d\\gamename\\%s\\statechanged\\\n", m_nPort,
            static_cast<const char*>(sGameSpyName));
    } else {
        sprintf(pQuery, "\\heartbeat\\%d\\gamename\\%s\\\n", m_nPort,
            static_cast<const char*>(sGameSpyName));
    }

    // The original allocates the exact string length and copies that many
    // bytes: the datagram has no terminator and sendto gets the terminator's
    // index.
    INT nLength = static_cast<INT>(strlen(pQuery));
    char* pMessage = new char[nLength];
    if (pMessage == NULL) {
        delete[] pQuery;
        LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
        return FALSE;
    }
    memcpy(pMessage, pQuery, nLength);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0x6CFC);
    addr.sin_addr.s_addr = m_nMasterAddress;
    sendto(m_hSendSocket, pMessage, nLength, 0, &addr, sizeof(addr));

    delete[] pMessage;
    delete[] pQuery;

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return TRUE;
}

// Opens the heartbeat: resolves the master (sub_4D0980), then makes the send
// socket (+0x0A) and the query socket (+0x0E) non-blocking and binds the
// latter to the GameSpy port from the chitin (0x5BE when the INI has none).
// Any failure closes what was opened and leaves +0x01 clear. A successful
// open sets +0x12 to the port, +0x28 (the retry counter) to zero, and on
// failure calls WSACleanup.
//
// 0x4D2060
BOOLEAN CGameSpy::sub_4D2060()
{
    if (sub_4D0980() == FALSE) {
        m_bHeartbeatActive = FALSE;
        return FALSE;
    }

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    // The first heartbeat is 30 s later.
    m_dwLastHeartbeat = GetTickCount() - 30000;

    WORD nPort = g_pChitin->GetMultiplayerGameSpyPort();
    if (nPort == 0) {
        nPort = 0x5BE;
    }

    m_bHeartbeatActive = TRUE;
    if (m_bWSAStarted == FALSE || g_pChitin->cNetwork.m_bIsHost == FALSE) {
        m_bHeartbeatActive = FALSE;
    }

    if (m_bHeartbeatActive == TRUE) {
        BOOLEAN bOpened = FALSE;
        if (m_bWSAStarted != FALSE) {
            SOCKET hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (hSocket != INVALID_SOCKET) {
                unsigned long bNonBlocking = 1;
                if (ioctlsocket(hSocket, FIONBIO, &bNonBlocking) == 0) {
                    m_hSendSocket = hSocket;
                    bOpened = TRUE;
                } else {
                    closesocket(hSocket);
                }
            }
        }
        if (bOpened == FALSE) {
            m_bHeartbeatActive = FALSE;
        }
    }

    if (m_bHeartbeatActive == TRUE) {
        BOOLEAN bOpened = FALSE;
        if (m_bWSAStarted != FALSE) {
            SOCKET hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (hSocket != INVALID_SOCKET) {
                unsigned long bNonBlocking = 1;
                if (ioctlsocket(hSocket, FIONBIO, &bNonBlocking) == 0) {
                    m_hQuerySocket = hSocket;
                    bOpened = TRUE;
                } else {
                    closesocket(hSocket);
                }
            }
        }
        if (bOpened == FALSE) {
            m_bHeartbeatActive = FALSE;
        }
    }

    if (m_bHeartbeatActive == TRUE) {
        BOOLEAN bBound = FALSE;
        if (m_bWSAStarted != FALSE) {
            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(nPort);
            addr.sin_addr.s_addr = 0;
            if (bind(m_hQuerySocket, &addr, sizeof(addr)) == 0) {
                bBound = TRUE;
            } else {
                closesocket(m_hQuerySocket);
            }
        }
        if (bBound == FALSE) {
            closesocket(m_hSendSocket);
            m_bHeartbeatActive = FALSE;
        }
    }

    m_nPort = nPort;
    m_nRetries = 0;

    if (m_bHeartbeatActive == FALSE) {
        // #design-flaw: the original sets +0x00 to 1 around WSACleanup even
        // though winsock never started; kept as the bytes have it.
        m_bWSAStarted = TRUE;
        WSACleanup();
        m_bWSAStarted = FALSE;
    }

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return m_bHeartbeatActive;
}

// The periodic pump: while the heartbeat is on and this is the host, resends
// the heartbeat when the retry counter has not run out (3,750 ms) and again
// every 30 s, then takes one datagram off the query socket and hands it to the
// reply handler under the reply section.
//
// 0x4D2210
BOOLEAN CGameSpy::sub_4D2210()
{
    if (m_bHeartbeatActive == FALSE || g_pChitin->cNetwork.m_bIsHost == FALSE) {
        return FALSE;
    }

    if (m_nRetries <= 5) {
        if (GetTickCount() - m_dwLastHeartbeat > 0xEA6) {
            m_nRetries++;
            m_dwLastHeartbeat = GetTickCount();
            sub_4D1EC0(FALSE);
        }
    }

    if (GetTickCount() - m_dwLastHeartbeat > 0x7530) {
        m_dwLastHeartbeat = GetTickCount();
        sub_4D1EC0(FALSE);
    }

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    unsigned long nAddress = 0;
    unsigned short nPort = 0;
    BYTE* pData = NULL;
    DWORD nLength = 0;
    if (sub_4D0B80(m_hQuerySocket, &nAddress, &nPort, &pData, &nLength)) {
        sub_4D15D0(nAddress, nPort, pData, nLength);
        if (nLength != 0) {
            delete[] pData;
        }
    }

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return TRUE;
}

// Closes the heartbeat sockets and winsock when a heartbeat was running;
// returns FALSE when none was.
//
// 0x4D2310
BOOLEAN CGameSpy::sub_4D2310()
{
    if (m_bHeartbeatActive == FALSE) {
        return FALSE;
    }

    closesocket(m_hSendSocket);
    closesocket(m_hQuerySocket);

    if (m_bWSAStarted == TRUE) {
        WSACleanup();
        m_bWSAStarted = FALSE;
    }

    return TRUE;
}
