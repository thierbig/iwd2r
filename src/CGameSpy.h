#ifndef CGAMESPY_H_
#define CGAMESPY_H_

#include "mfc.h"

// The GameSpy master-server heartbeat. The layout is pinned by the four row
// functions (0x4D1EC0, 0x4D2060, 0x4D2210, 0x4D2310), the three helpers this
// class holds (0x4D0980, 0x4D0B80 and the reply handler 0x4D15D0) and the
// window procedure's 0x402 arm: the heartbeat's sockets at +0x0A/+0x0E, the
// master address at +0x06, and the reply buffer at +0x22 with its length at
// +0x16. Nothing here is a guess; the offsets and types are the ones the
// instructions read and store.
//
// The sockets are the platform's inert ones by default (platform/win32_api.h),
// so on this engine the class never gets past WSAStartup and the whole
// heartbeat stays off - the single-player decision (ADR 0002) with the row's
// bodies whole.
class CGameSpy {
public:
    CGameSpy();
    ~CGameSpy();
    BOOLEAN sub_4D0980();
    BOOLEAN sub_4D0B80(SOCKET hSocket, unsigned long* pAddress, unsigned short* pPort, BYTE** ppData, DWORD* pLength);
    BOOLEAN sub_4D0C70(unsigned long nAddress, unsigned short nPort);
    BOOLEAN sub_4D0DA0(const char* pData, DWORD nLength, const CString& sKey, BYTE** ppValue, DWORD* pValueLength);
    BOOLEAN sub_4D0F40(unsigned long nAddress, unsigned short nPort, const BYTE* pSecure);
    BOOLEAN sub_4D1220(unsigned long nAddress, unsigned short nPort, const BYTE* pSecure);
    BOOLEAN sub_4D15D0(unsigned long nAddress, unsigned short nPort, const BYTE* pData, DWORD nLength);
    BOOLEAN sub_4D1EC0(BOOL a1);
    BOOLEAN sub_4D2060();
    BOOLEAN sub_4D2210();
    BOOLEAN sub_4D2310();

    /* 0000 */ BOOLEAN m_bWSAStarted;
    /* 0001 */ BOOLEAN m_bHeartbeatActive;
    /* 0002 */ DWORD m_dwLastHeartbeat;
    /* 0006 */ unsigned long m_nMasterAddress;
    /* 000A */ SOCKET m_hSendSocket;
    /* 000E */ SOCKET m_hQuerySocket;
    /* 0012 */ int m_nPort;
    /* 0016 */ int m_nResponseLength;
    /* 001A */ int m_nQueryId;
    /* 001E */ int m_nQueryCounter;
    /* 0022 */ BYTE* m_pResponse;
    /* 0026 */ BOOLEAN m_bHostResolved;
    /* 0028 */ int m_nRetries;
};

#endif /* CGAMESPY_H_ */
