// CGameSpy's heartbeat and socket lifecycle (0x4D0980, 0x4D1EC0, 0x4D2060,
// 0x4D2310), linked against the engine.
//
// The platform's winsock calls are inert by default (WSAStartup fails), so the
// tests install fakes through iwd2r_SetGameSpySockets: a startup that can
// succeed, handles that record their calls, a sendto that keeps the datagram,
// and a WSAAsyncGetHostByName whose completion the test's thread signals
// through the 0x26 flag (what CChitin::WndProc's 0x402 arm sets in the game).
//
// Fixture: a bare CChitin subclass supplies g_pChitin (the four thread bodies
// are pure virtual and never run) and its cNetwork (the reply section the
// functions lock, and the host flag). The class's own fields the constructor
// leaves alone (the two sockets and the master address) are set explicitly.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CGameSpy.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

extern CChitin* g_pChitin;

namespace {

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }

    void GetGameSpyGameName(CString& sGameSpyName) override { sGameSpyName = "IWDTEST"; }
    void GetGameSpyCode(CString& sGameSpyCode) override { sGameSpyCode = "IWDTST"; }
    WORD GetMultiplayerGameSpyPort() override { return m_nGameSpyPort; }

    // What the reply handler's GetGameSpyResponse call returns per type.
    BOOLEAN GetGameSpyResponse(BYTE type, BYTE** pData, DWORD* dwSize) override
    {
        m_nResponseTypes[m_nResponseCalls++] = type;
        if (type > 3) {
            return FALSE;
        }
        *dwSize = static_cast<DWORD>(m_sResponse[type].size());
        *pData = NULL;
        if (*dwSize != 0) {
            *pData = new BYTE[*dwSize];
            std::memcpy(*pData, m_sResponse[type].data(), *dwSize);
        }
        return TRUE;
    }

    WORD m_nGameSpyPort = 8000;
    std::string m_sResponse[4] = { "R0", "R1", "R2", "R3" };
    BYTE m_nResponseTypes[8] = {};
    int m_nResponseCalls = 0;
};

class ChitinScope {
public:
    ChitinScope()
        : m_previous(g_pChitin)
    {
        g_pChitin = &m_chitin;
    }
    ~ChitinScope() { g_pChitin = m_previous; }

    ChitinScope(const ChitinScope&) = delete;
    ChitinScope& operator=(const ChitinScope&) = delete;

    TestChitin& Chitin() { return m_chitin; }

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

// The fake winsock calls and what they recorded.
struct SocketLog {
    bool bStartupOk = true;
    bool bSocketFails = false;
    WORD nWSAVersion = 2;
    int nStartupCalls = 0;
    int nCleanupCalls = 0;
    int nSocketCalls = 0;
    SOCKET nNextSocket = 100;
    std::vector<SOCKET> opened;
    std::vector<SOCKET> closed;
    int nBoundPort = -1;
    SOCKET nBoundSocket = 0;
    std::string sSent;
    std::vector<std::string> sent;
    SOCKET nSentSocket = 0;
    int nSentFlags = -1;
    unsigned short nSentPort = 0;
    unsigned long nSentAddress = 0;
    // The datagrams the fake recvfrom serves, in order; the peek does not
    // consume one, the read does.
    struct Packet {
        std::string data;
        unsigned long nAddress = 0;
        unsigned short nPort = 0;
    };
    std::vector<Packet> recvPackets;
    size_t nRecvIndex = 0;
    int nRecvfromCalls = 0;
    int nRecvfromPeekFlags = -1;
    int nAsyncCalls = 0;
    int nAsyncCancelCalls = 0;
    // Filled by WSAAsyncGetHostByName; the resolved HOSTENT the port reads.
    unsigned long nResolvedAddress = 0x01020304;
    void* pAddrList[2] = { &nResolvedAddress, nullptr };
    CGameSpy* pSpyToResolve = nullptr;
};

SocketLog g_log;

int FakeStartup(WORD, WSADATA* data)
{
    g_log.nStartupCalls++;
    std::memset(data, 0, sizeof(*data));
    data->wVersion = g_log.bStartupOk ? g_log.nWSAVersion : 0;
    return g_log.bStartupOk ? 0 : SOCKET_ERROR;
}

int FakeCleanup()
{
    g_log.nCleanupCalls++;
    return 0;
}

SOCKET FakeSocket(int af, int type, int protocol)
{
    g_log.nSocketCalls++;
    if (g_log.bSocketFails || af != AF_INET || type != SOCK_DGRAM || protocol != IPPROTO_UDP) {
        return INVALID_SOCKET;
    }
    SOCKET handle = g_log.nNextSocket++;
    g_log.opened.push_back(handle);
    return handle;
}

int FakeIoctl(SOCKET, long command, unsigned long* argument)
{
    if (command != FIONBIO || argument == nullptr || *argument != 1) {
        return SOCKET_ERROR;
    }
    return 0;
}

int FakeBind(SOCKET handle, const struct sockaddr_in* address, int length)
{
    if (length != static_cast<int>(sizeof(*address)) || address->sin_family != AF_INET) {
        return SOCKET_ERROR;
    }
    g_log.nBoundSocket = handle;
    g_log.nBoundPort = address->sin_port;
    return 0;
}

int FakeSendto(SOCKET handle, const char* buffer, int length, int flags, const struct sockaddr_in* to,
    int toLength)
{
    if (length < 0 || to == nullptr || toLength != static_cast<int>(sizeof(*to))) {
        return SOCKET_ERROR;
    }
    g_log.sSent.assign(buffer, static_cast<size_t>(length));
    g_log.sent.push_back(g_log.sSent);
    g_log.nSentSocket = handle;
    g_log.nSentFlags = flags;
    g_log.nSentPort = to->sin_port;
    g_log.nSentAddress = to->sin_addr.s_addr;
    return length;
}

int FakeRecvfrom(SOCKET, char* buffer, int length, int flags, struct sockaddr_in* from,
    int* fromLength)
{
    g_log.nRecvfromCalls++;
    if (g_log.nRecvIndex >= g_log.recvPackets.size()) {
        return SOCKET_ERROR;
    }
    const SocketLog::Packet& packet = g_log.recvPackets[g_log.nRecvIndex];
    if (static_cast<int>(packet.data.size()) > length) {
        return SOCKET_ERROR;
    }
    if (flags == 2) {
        // The queued datagram is pending: report its size, consume nothing.
        g_log.nRecvfromPeekFlags = flags;
        return static_cast<int>(packet.data.size());
    }
    std::memcpy(buffer, packet.data.data(), packet.data.size());
    if (from != nullptr) {
        std::memset(from, 0, sizeof(*from));
        from->sin_family = AF_INET;
        from->sin_port = htons(packet.nPort);
        from->sin_addr.s_addr = packet.nAddress;
    }
    if (fromLength != nullptr) {
        *fromLength = static_cast<int>(sizeof(*from));
    }
    g_log.nRecvIndex++;
    return static_cast<int>(packet.data.size());
}

int FakeClosesocket(SOCKET handle)
{
    g_log.closed.push_back(handle);
    return 0;
}

HANDLE FakeAsyncGetHostByName(HWND, unsigned message, const char* name, char* buffer, int length)
{
    g_log.nAsyncCalls++;
    REQUIRE(message == 0x402);
    REQUIRE(name != nullptr);
    REQUIRE(std::string(name) == "master.gamespy.com");
    REQUIRE(buffer != nullptr);
    REQUIRE(length == 0x400);

    // A synthetic HOSTENT: h_addr_list at +0x0C.
    std::memset(buffer, 0, static_cast<size_t>(length));
    *reinterpret_cast<void**>(buffer + 0x0C) = g_log.pAddrList;

    // The window procedure sets the flag when the request completes; a thread
    // stands in for it, while the caller sleeps in SleepEx's 300 ms slice.
    CGameSpy* pSpy = g_log.pSpyToResolve;
    if (pSpy != nullptr) {
        std::thread([pSpy] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            pSpy->m_bHostResolved = TRUE;
        }).detach();
    }
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234));
}

int FakeCancelAsync(HANDLE)
{
    g_log.nAsyncCancelCalls++;
    return 0;
}

const IWD2R_GameSpySockets kFakeSockets = {
    FakeStartup,
    FakeCleanup,
    FakeSocket,
    FakeIoctl,
    FakeBind,
    FakeSendto,
    FakeRecvfrom,
    FakeClosesocket,
    FakeAsyncGetHostByName,
    FakeCancelAsync,
};

// Installs the fake table for the scope, and clears the log.
class SocketScope {
public:
    SocketScope()
    {
        g_log = SocketLog();
        g_log.opened.clear();
        g_log.closed.clear();
        // The temporary's pAddrList points at its own member; keep the live
        // one.
        g_log.pAddrList[0] = &g_log.nResolvedAddress;
        iwd2r_SetGameSpySockets(&kFakeSockets);
    }
    ~SocketScope() { iwd2r_SetGameSpySockets(nullptr); }

    SocketScope(const SocketScope&) = delete;
    SocketScope& operator=(const SocketScope&) = delete;
};

// A CGameSpy with the fields its constructor does not set (the two sockets
// and the master address) cleared, so a test never reads indeterminate state.
void InitSpy(CGameSpy& spy)
{
    spy.m_hSendSocket = 0;
    spy.m_hQuerySocket = 0;
    spy.m_nMasterAddress = 0;
}

}

TEST_CASE("gamespy: with the inert winsock the heartbeat never starts", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    CGameSpy spy;
    InitSpy(spy);

    CHECK(spy.sub_4D2060() == FALSE);
    CHECK(spy.m_bHeartbeatActive == FALSE);
    CHECK(spy.m_bWSAStarted == FALSE);
}

TEST_CASE("gamespy: the heartbeat opens both sockets, binds the query port and closes them", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    chitin.Chitin().m_nGameSpyPort = 0x1F40;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.pSpyToResolve = &spy;

    REQUIRE(spy.sub_4D2060() == TRUE);
    CHECK(spy.m_bHeartbeatActive == TRUE);
    CHECK(spy.m_bWSAStarted == TRUE);
    CHECK(spy.m_nMasterAddress == 0x01020304);
    CHECK(spy.m_nPort == 0x1F40);
    CHECK(spy.m_nRetries == 0);
    CHECK(spy.m_dwLastHeartbeat != 0);
    CHECK(g_log.nStartupCalls == 1);
    REQUIRE(g_log.opened.size() == 2);
    CHECK(spy.m_hSendSocket == g_log.opened[0]);
    CHECK(spy.m_hQuerySocket == g_log.opened[1]);
    CHECK(g_log.nBoundSocket == spy.m_hQuerySocket);
    CHECK(g_log.nBoundPort == htons(0x1F40));
    CHECK(g_log.nCleanupCalls == 0);

    CHECK(spy.sub_4D2310() == TRUE);
    REQUIRE(g_log.closed.size() == 2);
    CHECK(g_log.closed[0] == spy.m_hSendSocket);
    CHECK(g_log.closed[1] == spy.m_hQuerySocket);
    CHECK(g_log.nCleanupCalls == 1);
    CHECK(spy.m_bWSAStarted == FALSE);

    // The original clears +0x00 (winsock started) but not +0x01 (active), so
    // a second close closes the same stale handles again and still returns
    // TRUE; winsock is not cleaned up twice.
    g_log.closed.clear();
    CHECK(spy.sub_4D2310() == TRUE);
    CHECK(g_log.closed.size() == 2);
    CHECK(g_log.nCleanupCalls == 1);
    CHECK(spy.m_bWSAStarted == FALSE);
}

TEST_CASE("gamespy: a non-host does not start the heartbeat", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = FALSE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.pSpyToResolve = &spy;

    CHECK(spy.sub_4D2060() == FALSE);
    CHECK(spy.m_bHeartbeatActive == FALSE);
    CHECK(g_log.opened.empty());
}

TEST_CASE("gamespy: an unset GameSpy port falls back to 0x5BE", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    chitin.Chitin().m_nGameSpyPort = 0;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.pSpyToResolve = &spy;

    REQUIRE(spy.sub_4D2060() == TRUE);
    CHECK(spy.m_nPort == 0x5BE);
    CHECK(g_log.nBoundPort == htons(0x5BE));
}

TEST_CASE("gamespy: a failed host lookup leaves the heartbeat off", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    g_log.bStartupOk = false;
    CGameSpy spy;
    InitSpy(spy);

    CHECK(spy.sub_4D0980() == FALSE);
    CHECK(spy.m_bWSAStarted == FALSE);
    CHECK(g_log.nStartupCalls == 1);
    CHECK(g_log.nCleanupCalls == 1);
    CHECK(g_log.nAsyncCalls == 0);
}

TEST_CASE("gamespy: the heartbeat is the master protocol's query string", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_bHeartbeatActive = TRUE;
    spy.m_nPort = 8000;
    spy.m_nMasterAddress = 0x7F000001;

    REQUIRE(spy.sub_4D1EC0(FALSE) == TRUE);
    CHECK(g_log.sSent == "\\heartbeat\\8000\\gamename\\IWDTEST\\\n");
    CHECK(g_log.nSentFlags == 0);
    CHECK(g_log.nSentPort == htons(0x6CFC));
    CHECK(g_log.nSentAddress == 0x7F000001);

    REQUIRE(spy.sub_4D1EC0(TRUE) == TRUE);
    CHECK(g_log.sSent == "\\heartbeat\\8000\\gamename\\IWDTEST\\statechanged\\\n");

    // A1 is compared against 1 exactly: any other value takes the plain form.
    REQUIRE(spy.sub_4D1EC0(2) == TRUE);
    CHECK(g_log.sSent == "\\heartbeat\\8000\\gamename\\IWDTEST\\\n");
}

TEST_CASE("gamespy: a heartbeat that is off or not from the host is not sent", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_nPort = 8000;

    // Not active.
    CHECK(spy.sub_4D1EC0(FALSE) == FALSE);
    CHECK(g_log.sSent.empty());

    // Active, but this is not the host.
    spy.m_bHeartbeatActive = TRUE;
    chitin.Chitin().cNetwork.m_bIsHost = FALSE;
    CHECK(spy.sub_4D1EC0(FALSE) == FALSE);
    CHECK(g_log.sSent.empty());
}

TEST_CASE("gamespy: the query extractor finds a key's value between backslashes", "[gamespy]")
{
    ChitinScope chitin;
    CGameSpy spy;
    InitSpy(spy);

    const char* packet = "\\secure\\ABCDEF\\basic\\\\info\\1\\";
    DWORD nLength = static_cast<DWORD>(std::strlen(packet));
    BYTE* pValue = NULL;
    DWORD nValueLength = 0;

    REQUIRE(spy.sub_4D0DA0(packet, nLength, CString("secure"), &pValue, &nValueLength) == TRUE);
    CHECK(nValueLength == 6);
    REQUIRE(pValue != NULL);
    CHECK(std::memcmp(pValue, "ABCDEF", 6) == 0);
    delete[] pValue;

    // An empty value is found, with no buffer.
    REQUIRE(spy.sub_4D0DA0(packet, nLength, CString("basic"), &pValue, &nValueLength) == TRUE);
    CHECK(nValueLength == 0);
    CHECK(pValue == NULL);

    REQUIRE(spy.sub_4D0DA0(packet, nLength, CString("info"), &pValue, &nValueLength) == TRUE);
    CHECK(nValueLength == 1);
    REQUIRE(pValue != NULL);
    CHECK(pValue[0] == '1');
    delete[] pValue;

    // A missing key.
    CHECK(spy.sub_4D0DA0(packet, nLength, CString("rules"), &pValue, &nValueLength) == FALSE);
    CHECK(nValueLength == 0);
    CHECK(pValue == NULL);
}

TEST_CASE("gamespy: a flush appends the query id and sends the reply buffer", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_hQuerySocket = 42;
    spy.m_nQueryId = 7;
    spy.m_nQueryCounter = 0;
    spy.m_pResponse = new BYTE[0x578];
    std::memset(spy.m_pResponse, 0, 0x578);
    std::memcpy(spy.m_pResponse, "HELLO", 5);
    spy.m_nResponseLength = 5;

    REQUIRE(spy.sub_4D0C70(0x01020304, 0x1F40) == TRUE);
    CHECK(g_log.sSent == "HELLO\\queryid\\7.1\n");
    CHECK(g_log.nSentSocket == 42);
    CHECK(g_log.nSentPort == htons(0x1F40));
    CHECK(g_log.nSentAddress == 0x01020304);
    CHECK(g_log.nSentFlags == 0);
    CHECK(spy.m_nResponseLength == 0);
    CHECK(spy.m_pResponse[0] == 0); // zeroed for the next reply

    delete[] spy.m_pResponse;
    spy.m_pResponse = NULL;
}

TEST_CASE("gamespy: the validate answer encodes the secure challenge with the game code", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_hQuerySocket = 42;
    spy.m_nQueryId = 3;
    spy.m_nQueryCounter = 0;
    spy.m_pResponse = new BYTE[0x578];
    std::memset(spy.m_pResponse, 0, 0x578);
    spy.m_nResponseLength = 0;

    // The eight characters below are an independent model of the arm's
    // RC4 semantics (key "IWDTST", challenge 01..06): crypt bb 0d c9 13 ac 40,
    // which the 6-bit encoding writes as "uw3JE6xA".
    const BYTE secure[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    REQUIRE(spy.sub_4D0F40(0x01020304, 0x1F40, secure) == TRUE);
    CHECK(g_log.sSent == "\\validate\\uw3JE6xA\\queryid\\3.1\n");
    CHECK(g_log.nSentSocket == 42);
    CHECK(spy.m_nResponseLength == 0);

    delete[] spy.m_pResponse;
    spy.m_pResponse = NULL;
}

TEST_CASE("gamespy: the validate-and-final answer flushes once", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_hQuerySocket = 42;
    spy.m_nQueryId = 3;
    spy.m_nQueryCounter = 0;
    spy.m_pResponse = new BYTE[0x578];
    std::memset(spy.m_pResponse, 0, 0x578);
    spy.m_nResponseLength = 0;

    const BYTE secure[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    REQUIRE(spy.sub_4D1220(0x01020304, 0x1F40, secure) == TRUE);
    REQUIRE(g_log.sent.size() == 1);
    CHECK(g_log.sent[0] == "\\validate\\uw3JE6xA\\final\\\\queryid\\3.1\n");

    // Without a challenge the answer is just \final\.
    g_log.sent.clear();
    spy.m_nQueryId = 3;
    spy.m_nQueryCounter = 0;
    REQUIRE(spy.sub_4D1220(0x01020304, 0x1F40, NULL) == TRUE);
    REQUIRE(g_log.sent.size() == 1);
    CHECK(g_log.sent[0] == "\\final\\\\queryid\\3.1\n");

    delete[] spy.m_pResponse;
    spy.m_pResponse = NULL;
}

TEST_CASE("gamespy: the dequeue takes a datagram and its sender", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.recvPackets.push_back({ "\\basic\\", 0x0A0B0C0D, 0x1234 });

    unsigned long nAddress = 0;
    unsigned short nPort = 0;
    BYTE* pData = NULL;
    DWORD nLength = 0;
    REQUIRE(spy.sub_4D0B80(42, &nAddress, &nPort, &pData, &nLength) == TRUE);
    CHECK(g_log.nRecvfromCalls == 2); // the peek, then the read
    CHECK(g_log.nRecvfromPeekFlags == 2);
    CHECK(nLength == 7);
    REQUIRE(pData != NULL);
    CHECK(std::memcmp(pData, "\\basic\\", 7) == 0);
    CHECK(nAddress == 0x0A0B0C0D);
    CHECK(nPort == 0x1234);
    delete[] pData;

    // Nothing queued any more.
    CHECK(spy.sub_4D0B80(42, &nAddress, &nPort, &pData, &nLength) == FALSE);
}

TEST_CASE("gamespy: the reply handler answers a basic query", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_hQuerySocket = 42;

    std::string packet = "\\secure\\";
    packet.append("\x01\x02\x03\x04\x05\x06", 6);
    packet += "\\basic\\";
    REQUIRE(spy.sub_4D15D0(0x01020304, 0x1F40, reinterpret_cast<const BYTE*>(packet.data()),
                static_cast<DWORD>(packet.size()))
        == TRUE);

    // basic's answer, then the validate (for the secure challenge) and
    // \final\ before the flush.
    REQUIRE(g_log.sent.size() == 1);
    CHECK(g_log.sent[0] == "R0\\validate\\uw3JE6xA\\final\\\\queryid\\1.1\n");
    CHECK(chitin.Chitin().m_nResponseCalls == 1);
    CHECK(chitin.Chitin().m_nResponseTypes[0] == 0);
    CHECK(spy.m_pResponse != NULL); // the handler frees it, but the member keeps the pointer
    CHECK(spy.m_nQueryId == 1);
}

TEST_CASE("gamespy: the reply handler answers status with all four responses", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_hQuerySocket = 42;

    std::string packet = "\\secure\\";
    packet.append("\x01\x02\x03\x04\x05\x06", 6);
    packet += "\\status\\";
    REQUIRE(spy.sub_4D15D0(0x01020304, 0x1F40, reinterpret_cast<const BYTE*>(packet.data()),
                static_cast<DWORD>(packet.size()))
        == TRUE);

    // R0 goes out alone; each later response flushes behind its validate, and
    // the last one carries the final.
    REQUIRE(g_log.sent.size() == 4);
    CHECK(g_log.sent[0] == "R0\\validate\\uw3JE6xA\\queryid\\1.1\n");
    CHECK(g_log.sent[1] == "R1\\validate\\uw3JE6xA\\queryid\\1.2\n");
    CHECK(g_log.sent[2] == "R2\\validate\\uw3JE6xA\\queryid\\1.3\n");
    CHECK(g_log.sent[3] == "R3\\validate\\uw3JE6xA\\final\\\\queryid\\1.4\n");
    CHECK(chitin.Chitin().m_nResponseCalls == 4);
    CHECK(chitin.Chitin().m_nResponseTypes[0] == 0);
    CHECK(chitin.Chitin().m_nResponseTypes[1] == 1);
    CHECK(chitin.Chitin().m_nResponseTypes[2] == 2);
    CHECK(chitin.Chitin().m_nResponseTypes[3] == 3);
}

TEST_CASE("gamespy: the pump resends the heartbeat and dispatches a queued query", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_bHeartbeatActive = TRUE;
    spy.m_bWSAStarted = TRUE;
    spy.m_nPort = 8000;
    spy.m_nMasterAddress = 0x7F000001;
    spy.m_hSendSocket = 1;
    spy.m_hQuerySocket = 2;
    spy.m_dwLastHeartbeat = GetTickCount() - 0x7531;

    g_log.recvPackets.push_back({ "\\basic\\", 0x01020304, 0x1F40 });

    REQUIRE(spy.sub_4D2210() == TRUE);
    CHECK(spy.m_nRetries == 1);
    REQUIRE(g_log.sent.size() == 2);
    CHECK(g_log.sent[0] == "\\heartbeat\\8000\\gamename\\IWDTEST\\\n");
    CHECK(g_log.sent[1] == "R0\\final\\\\queryid\\1.1\n");
    CHECK(g_log.nSentSocket == 2); // the reply goes out on the query socket
}

TEST_CASE("gamespy: the pump does nothing when the heartbeat is off or not the host", "[gamespy]")
{
    ChitinScope chitin;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);

    // Off.
    CHECK(spy.sub_4D2210() == FALSE);

    // On, but not the host.
    spy.m_bHeartbeatActive = TRUE;
    chitin.Chitin().cNetwork.m_bIsHost = FALSE;
    CHECK(spy.sub_4D2210() == FALSE);
    CHECK(g_log.sent.empty());
    CHECK(g_log.nRecvfromCalls == 0);
}
