// The null-provider session sequence of CNetwork, linked against the engine.
//
// Fixture: `CNetwork n;` constructs without the chitin object, but the
// session sequence does not: HostNewSession, AddPlayerToList (called by
// CreatePlayer) and OnCloseSession call the g_pChitin->OnMultiplayer* hooks,
// and the incoming-queue code locks g_pChitin->cNetwork.field_F52. A bare
// CChitin subclass supplies both: its hooks are the base class's empty ones,
// and constructing it initializes its own cNetwork. The four thread bodies are
// pure virtual in CChitin and never run here.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CMessage.h"
#include "CNetwork.h"

#include <cstring>

extern CChitin* g_pChitin;

namespace {

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }
};

// Installs a TestChitin as g_pChitin for the scope of a test.
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

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

}

TEST_CASE("null service provider hosts a single-player session", "[network]")
{
    ChitinScope scope;
    REQUIRE(g_pChitin != nullptr);

    CNetwork n;

    REQUIRE(n.EnumerateServiceProviders() == TRUE);
    REQUIRE(n.m_nTotalServiceProviders == 1);
    REQUIRE(n.SelectServiceProvider(CNetwork::SERV_PROV_NULL) == TRUE);
    REQUIRE(n.InitializeConnectionToServiceProvider(TRUE) == TRUE);

    n.m_sSessionNameToMake = "s";
    n.m_bSessionNameToMake = TRUE;
    n.m_sLocalPlayerName = "p";
    n.m_bPlayerNameToMake = TRUE;

    REQUIRE(n.HostNewSession() == TRUE);

    INT err = -1;
    REQUIRE(n.CreatePlayer(err) == TRUE);
    CHECK(err == CNetwork::ERROR_NONE);

    CHECK(n.GetSessionOpen() == TRUE);
    CHECK(n.GetSessionHosting() == TRUE);
    CHECK(n.m_idLocalPlayer == 1);

    CString s;
    n.GetHostPlayerName(s);
    CHECK(s == "p");

    CHECK(n.FindPlayerLocationByID(1, FALSE) == 0);

    BYTE data[4] = { 1, 2, 3, 4 };
    CHECK(n.SendSpecificMessage("p", 0, 1, 1, data, 4) == TRUE);
    CHECK(n.PeekSpecificMessage("p", 1, 1) == FALSE);

    n.CloseSession(TRUE);
    CHECK(n.GetSessionOpen() == FALSE);
    CHECK(n.m_idLocalPlayer == 0);
}

// CBaldurMessage::SendMapWorldCancelRequestToServer (0x435360): the client's
// empty MAPWORLD cancel request. It stops at three guards (no session, a host,
// no host name) and otherwise sends type/subtype 109/99 to the host with no
// payload. CNetwork::SendSpecificMessage writes the packet into the public
// static buffer before the (DirectPlay-less) network layer fails the actual
// send, so the test can read the wire bytes: [0]=0xFF flag, [1]=type,
// [2]=subtype, [3..]=payload (none here).
TEST_CASE("CBaldurMessage::SendMapWorldCancelRequestToServer sends the MapWorld cancel packet", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    network.m_nHostPlayer = 0;
    network.m_psPlayerName[0] = "host";
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_pPlayerID[0] = 1;

    // No session: the first guard answers FALSE and the buffer is untouched.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    memset(pBuffer, 0xCD, 4);
    CHECK(message.SendMapWorldCancelRequestToServer() == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A host does not send the client's request.
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = TRUE;
    CHECK(message.SendMapWorldCancelRequestToServer() == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A client with no host name stops at the name guard.
    network.m_bIsHost = FALSE;
    network.m_nHostPlayer = -1;
    CHECK(message.SendMapWorldCancelRequestToServer() == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A client with the host named sends the packet. A non-null provider is
    // what makes SendSpecificMessage marshal it (the null provider returns
    // before the buffer is touched); the send itself fails because DirectPlay
    // is gone, which the handler does not propagate.
    network.m_nHostPlayer = 0;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    memset(pBuffer, 0xCD, 4);
    CHECK(message.SendMapWorldCancelRequestToServer() == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == 109);
    CHECK(pBuffer[2] == 99);
    CHECK(pBuffer[3] == 0xCD);
    CHECK(CBaldurMessage::MSG_TYPE_MAPWORLD == 109);
    CHECK(CBaldurMessage::MSG_SUBTYPE_MAPWORLD_CANCEL_REQUEST == 99);

    // Leave the embedded CNetwork closed: its destructor calls OnCloseSession,
    // which needs g_pChitin, but the ChitinScope restores g_pChitin before the
    // chitin's members are destroyed.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}
