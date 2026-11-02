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

// The movie pair: CBaldurMessage::SendMovieRequestToServer (0x436690) is the
// client's request, addressed to the host by name, and
// CBaldurMessage::MovieAnnounceStatus (0x4369E0) the host's announce to every
// player; both carry {BYTE length; char resref[length]}. A non-null provider
// makes SendSpecificMessage write the packet into CNetwork::STATIC_MESSAGE_BUFFER
// (SendMessage_ then reports failure, but the wire bytes are the original's).
TEST_CASE("msg: the movie request and announce carry the length-prefixed resref", "[network][msg]")
{
    ChitinScope scope;
    REQUIRE(g_pChitin != nullptr);

    CNetwork& network = g_pChitin->cNetwork;
    const INT nProvider = network.m_nServiceProvider;
    const BOOLEAN bConnection = network.m_bConnectionEstablished;
    const BOOLEAN bIsHost = network.m_bIsHost;
    const int nHostPlayer = network.m_nHostPlayer;
    const CString sHostName = network.m_psPlayerName[0];

    CBaldurMessage message;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_psPlayerName[0] = "Host";

    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // Guards: both need the session, the request is the client's (a host
    // answers FALSE) and the announce the host's (a client answers FALSE);
    // nothing is written in either case.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMovieRequestToServer(CResRef("MOVTEST")) == FALSE);
    CHECK(message.MovieAnnounceStatus(CResRef("MOVTEST")) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = TRUE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMovieRequestToServer(CResRef("MOVTEST")) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    network.m_bIsHost = FALSE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.MovieAnnounceStatus(CResRef("MOVTEST")) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    // The client's request: MOVIE/82 with {BYTE 7; "MOVTEST"}, addressed to
    // the host. Without a host name it fails instead of sending to nobody.
    network.m_nHostPlayer = 0;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMovieRequestToServer(CResRef("MOVTEST")) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == CBaldurMessage::MSG_TYPE_MOVIE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_SUBTYPE] == CBaldurMessage::MSG_SUBTYPE_MOVIE_REQUEST);
    CHECK(pBuffer[CNetwork::SPEC_MSG_HEADER_LENGTH] == 7);
    CHECK(std::memcmp(pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + 1, "MOVTEST", 7) == 0);

    network.m_nHostPlayer = -1;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMovieRequestToServer(CResRef("MOVTEST")) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    // The host's announce: MOVIE/65, to all players, the same payload; a
    // full eight-character resref travels whole.
    network.m_bIsHost = TRUE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.MovieAnnounceStatus(CResRef("MOVIETST")) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == CBaldurMessage::MSG_TYPE_MOVIE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_SUBTYPE] == CBaldurMessage::MSG_SUBTYPE_MOVIE_ANNOUNCE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_HEADER_LENGTH] == 8);
    CHECK(std::memcmp(pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + 1, "MOVIETST", 8) == 0);

    // An empty resref is sent as length 0 (the original does not test it).
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.MovieAnnounceStatus(CResRef("")) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_HEADER_LENGTH] == 0);

    network.m_nServiceProvider = nProvider;
    network.m_bConnectionEstablished = bConnection;
    network.m_bIsHost = bIsHost;
    network.m_nHostPlayer = nHostPlayer;
    network.m_psPlayerName[0] = sHostName;
}
