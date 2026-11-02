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

// CBaldurMessage::SendReformPartyAnnounceStatus (0x436DF0): the host's
// one-byte reform-party announcement to all players (REFORMPARTY 113/115).
// It stops at the session/hosting guards and otherwise marshals {BYTE 1}
// and sends it with SEND_ALL_PLAYERS | SEND_GUARANTEED. With a non-null
// provider CNetwork::SendSpecificMessage writes the packet into the public
// static buffer before the DirectPlay-less network layer fails the actual
// send, so the test can read the wire bytes: [0]=0xFF flag, [1]=113 type,
// [2]=115 subtype, [3]=1 payload.
TEST_CASE("CBaldurMessage::SendReformPartyAnnounceStatus broadcasts the reform-party packet", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // No session: the first guard answers FALSE and the buffer is untouched.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = TRUE;
    memset(pBuffer, 0xCD, 5);
    CHECK(message.SendReformPartyAnnounceStatus() == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A client does not announce the reform.
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;
    CHECK(message.SendReformPartyAnnounceStatus() == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // The host's packet: a non-null provider makes SendSpecificMessage
    // marshal it before the send itself fails (DirectPlay is gone); the
    // handler returns TRUE anyway, as the original ignores the send result.
    network.m_bIsHost = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    memset(pBuffer, 0xCD, 5);
    CHECK(message.SendReformPartyAnnounceStatus() == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == 113);
    CHECK(pBuffer[2] == 115);
    CHECK(pBuffer[3] == 1);
    CHECK(pBuffer[4] == 0xCD);
    CHECK(CBaldurMessage::MSG_TYPE_REFORMPARTY == 113);
    CHECK(CBaldurMessage::MSG_SUBTYPE_REFORMPARTY_ANNOUNCE_STATUS == 115);

    // Leave the embedded CNetwork closed: its destructor calls OnCloseSession,
    // which needs g_pChitin, but the ChitinScope restores g_pChitin before the
    // chitin's members are destroyed.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}
