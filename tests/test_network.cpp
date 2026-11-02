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
