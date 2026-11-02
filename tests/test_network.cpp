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

// CBaldurMessage::ResourceSuggestLoad (0x42AA10): with a real service provider
// and an open session it suggests a resource to every player as RESOURCE/83
// (SEND_ALL_PLAYERS | SEND_GUARANTEED) with
// {DWORD nType; DWORD dwFlags; char resref[len]} - no length prefix; the
// message's length carries the resref length. A non-null provider makes
// SendSpecificMessage write the packet into CNetwork::STATIC_MESSAGE_BUFFER.
TEST_CASE("msg: the resource suggest carries the type, flags and resref on RESOURCE/83", "[network][msg]")
{
    ChitinScope scope;
    REQUIRE(g_pChitin != nullptr);

    CNetwork& network = g_pChitin->cNetwork;
    const INT nProvider = network.m_nServiceProvider;
    const BOOLEAN bConnection = network.m_bConnectionEstablished;

    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // The null provider answers FALSE before the session test; whatever the
    // session says, nothing is written.
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    network.m_bConnectionEstablished = TRUE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.ResourceSuggestLoad(CString("ARE1000"), 1010, 0x0201) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    // With a provider, a closed session answers FALSE and writes nothing.
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = FALSE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.ResourceSuggestLoad(CString("ARE1000"), 1010, 0x0201) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    // The suggest: RESOURCE/83 to all players, guaranteed, then the type, the
    // flags and the raw resref bytes.
    network.m_bConnectionEstablished = TRUE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.ResourceSuggestLoad(CString("ARE1000"), 1010, 0x0201) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == CBaldurMessage::MSG_TYPE_RESOURCE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_SUBTYPE] == CBaldurMessage::MSG_SUBTYPE_RESOURCE_SUGGEST);

    UINT nType = 0;
    std::memcpy(&nType, pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH, sizeof(nType));
    CHECK(nType == 1010);

    DWORD dwFlags = 0;
    std::memcpy(&dwFlags,
        pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + sizeof(nType),
        sizeof(dwFlags));
    CHECK(dwFlags == 0x0201);

    CHECK(std::memcmp(pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + sizeof(nType) + sizeof(dwFlags),
              "ARE1000", 7)
        == 0);

    // An empty resref leaves the packet at the two dwords alone.
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.ResourceSuggestLoad(CString(""), 1, 2) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_HEADER_LENGTH + sizeof(nType) + sizeof(dwFlags)] == 0);

    network.m_nServiceProvider = nProvider;
    network.m_bConnectionEstablished = bConnection;
}
