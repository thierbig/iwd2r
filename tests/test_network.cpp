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

// CBaldurMessage::OnResourceDemanded (0x429A90): the host-only RESOURCE 82/68
// handler. The resource loading needs the game files, so the unit test pins
// the guards and the one branch that never loads a resource: an unknown type
// falls through to the default (nSize = nType) and answers with the echoed
// packet {BYTE nameLen, name, DWORD type, DWORD size, data}. The reply goes
// through SendSpecificMessage, so with a non-null provider its wire bytes are
// in the static buffer: [0]=0xFF flag, [1]=82 type, [2]=82 reply subtype.
TEST_CASE("CBaldurMessage::OnResourceDemanded answers only the host and echoes an unknown type", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    BYTE packet[3 + 4 + 4];
    memset(packet, 0, sizeof(packet));
    DWORD nType = 1;
    memcpy(packet + 3, &nType, sizeof(nType));
    memcpy(packet + 7, "TEST", 4);

    // No session: the first guard answers FALSE and the buffer is untouched.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = TRUE;
    memset(pBuffer, 0xCD, 4);
    CHECK(message.OnResourceDemanded(0, packet, sizeof(packet)) == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A client does not answer the demand.
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;
    CHECK(message.OnResourceDemanded(0, packet, sizeof(packet)) == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // The host: an unknown type (1) takes the default case, nSize = nType = 1,
    // so the reply payload is 4 + 1 + 9 = 14 bytes. A non-null provider makes
    // SendSpecificMessage marshal it; the send itself fails (DirectPlay is
    // gone) and is ignored.
    network.m_bIsHost = TRUE;
    network.m_psPlayerName[0] = "sender";
    network.m_pPlayerID[0] = 1;
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    memset(pBuffer, 0xCD, 3 + 14 + 1);
    CHECK(message.OnResourceDemanded(0, packet, sizeof(packet)) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == 82);
    CHECK(pBuffer[2] == 82);
    const BYTE* pPayload = pBuffer + 3;
    CHECK(pPayload[0] == 4);
    CHECK(memcmp(pPayload + 1, "TEST", 4) == 0);
    DWORD nEchoType;
    memcpy(&nEchoType, pPayload + 5, sizeof(nEchoType));
    DWORD nEchoSize;
    memcpy(&nEchoSize, pPayload + 9, sizeof(nEchoSize));
    CHECK(nEchoType == 1);
    CHECK(nEchoSize == 1);
    CHECK(pPayload[13] == 0); // the one zeroed data byte
    CHECK(pPayload[14] == 0xCD);

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}
