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

#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameObject.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "CScreenWorld.h"

#include <cstring>
#include <memory>

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

// CBaldurMessage::WeatherBroadcast (0x436F50): the host tells every player
// the weather flags WORD as WEATHER/119 (SEND_ALL_PLAYERS | SEND_GUARANTEED).
// A non-null provider makes SendSpecificMessage write the packet into
// CNetwork::STATIC_MESSAGE_BUFFER; SendMessage_ then reports failure, but the
// wire bytes are the original's.
TEST_CASE("msg: the weather broadcast writes WEATHER/119 for the host only", "[network][msg]")
{
    ChitinScope scope;
    REQUIRE(g_pChitin != nullptr);

    CNetwork& network = g_pChitin->cNetwork;
    const INT nProvider = network.m_nServiceProvider;
    const BOOLEAN bConnection = network.m_bConnectionEstablished;
    const BOOLEAN bIsHost = network.m_bIsHost;

    CBaldurMessage message;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;

    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // A closed session, or a client, answers FALSE and leaves the buffer
    // untouched.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = TRUE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.WeatherBroadcast(0x1234) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.WeatherBroadcast(0x1234) == FALSE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == 0);

    // The host broadcasts WEATHER/119 to all players, guaranteed, with the
    // flags WORD alone (little endian).
    network.m_bIsHost = TRUE;
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.WeatherBroadcast(0x1234) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == CBaldurMessage::MSG_TYPE_WEATHER);
    CHECK(pBuffer[CNetwork::SPEC_MSG_SUBTYPE] == CBaldurMessage::MSG_SUBTYPE_WEATHER_BROADCAST);

    WORD wFlags = 0;
    std::memcpy(&wFlags, pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH, sizeof(wFlags));
    CHECK(wFlags == 0x1234);

    // Zero travels as zero.
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.WeatherBroadcast(0) == TRUE);
    wFlags = 1;
    std::memcpy(&wFlags, pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH, sizeof(wFlags));
    CHECK(wFlags == 0);

    network.m_nServiceProvider = nProvider;
    network.m_bConnectionEstablished = bConnection;
    network.m_bIsHost = bIsHost;
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

// CBaldurMessage::OnLeaveAreaLuaRequestToServer (0x437950): the host-side
// LEAVEAREALUA 88/82 permit handler. The two entry guards need no game state.
TEST_CASE("CBaldurMessage::OnLeaveAreaLuaRequestToServer refuses without a session or a host", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;

    BYTE packet[3 + 4 + 4 + 4 + 4 + 2 + 1 + 1] = { 0 };

    // A closed session is refused before anything is parsed.
    CHECK(network.m_bConnectionEstablished == FALSE);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, sizeof(packet)) == FALSE);

    // Only the host handles the server-bound request.
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, sizeof(packet)) == FALSE);

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
}

// The handler's response computation and the 1-byte LEAVEAREALUA 88/114 reply
// on a zeroed game: the remote object always misses (so no feedback sprite
// and no transition), the area is never loaded and the settings are the only
// variable. Codes seen in the original: 1 allow, 0 deny, 3 busy/blocking,
// 4 paused.
TEST_CASE("CBaldurMessage::OnLeaveAreaLuaRequestToServer answers the permit request", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;

    CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> game(new unsigned char[sizeof(CInfGame)]());
    std::unique_ptr<unsigned char[]> world(new unsigned char[sizeof(CScreenWorld)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(game.get());
    g_pBaldurChitin->m_pEngineWorld = reinterpret_cast<CScreenWorld*>(world.get());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CMultiplayerSettings* pSettings = pGame->GetMultiplayerSettings();
    for (INT i = 0; i < 6; i++) {
        pSettings->field_AC[i] = 0xFF;
        pSettings->field_B2[i] = FALSE;
    }

    network.m_psPlayerName[0] = "sender";
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_pPlayerID[0] = 1;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = TRUE;

    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // {header 0xFF, LEAVEAREALUA 88, permit request 82;
    //  DWORD remotePlayerID 1; DWORD remoteObjectID 0;
    //  LONG x 10; LONG y 20; SHORT direction 3;
    //  BYTE 3 "abc"; BYTE 6 "AR1000"}
    BYTE packet[64];
    DWORD nSize = 0;
    packet[nSize++] = CNetwork::SPEC_MSG_FLAG_ENABLED;
    packet[nSize++] = CBaldurMessage::MSG_TYPE_LEAVEAREALUA;
    packet[nSize++] = CBaldurMessage::MSG_SUBTYPE_LEAVEAREALUA_PERMIT_REQUEST;
    DWORD nRemotePlayerID = 1;
    DWORD nRemoteObjectID = 0;
    LONG nX = 10;
    LONG nY = 20;
    SHORT nDirection = 3;
    memcpy(packet + nSize, &nRemotePlayerID, sizeof(nRemotePlayerID));
    nSize += sizeof(nRemotePlayerID);
    memcpy(packet + nSize, &nRemoteObjectID, sizeof(nRemoteObjectID));
    nSize += sizeof(nRemoteObjectID);
    memcpy(packet + nSize, &nX, sizeof(nX));
    nSize += sizeof(nX);
    memcpy(packet + nSize, &nY, sizeof(nY));
    nSize += sizeof(nY);
    memcpy(packet + nSize, &nDirection, sizeof(nDirection));
    nSize += sizeof(nDirection);
    packet[nSize++] = 3;
    memcpy(packet + nSize, "abc", 3);
    nSize += 3;
    packet[nSize++] = 6;
    memcpy(packet + nSize, "AR1000", 6);
    nSize += 6;

    // Allow: no player is in a character screen, the game is not paused, not
    // in a save/load mode, no dialog is pending and the area is not loaded.
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == CBaldurMessage::MSG_TYPE_LEAVEAREALUA);
    CHECK(pBuffer[2] == CBaldurMessage::MSG_SUBTYPE_LEAVEAREALUA_PERMIT_REPLY);
    CHECK(pBuffer[3] == 1);
    CHECK(pBuffer[4] == 0xCD);

    // A remote player in the inventory (field_AC) or the level-up screen
    // (field_B2) denies with 0.
    pSettings->field_AC[0] = 0;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[3] == 0);

    pSettings->field_AC[0] = 0xFF;
    pSettings->field_B2[0] = TRUE;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[3] == 0);
    pSettings->field_B2[0] = FALSE;

    // A paused world overrides the answer with 4.
    g_pBaldurChitin->m_pEngineWorld->m_bPaused = TRUE;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[3] == 4);
    g_pBaldurChitin->m_pEngineWorld->m_bPaused = FALSE;

    // A save/load mode denies with 0.
    pGame->GetGameSave()->m_mode = 0x182;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[3] == 0);
    pGame->GetGameSave()->m_mode = 0;

    // A host-permitted dialog denies; inside the blocking-message loop the
    // answer is the busy code 3 instead.
    pSettings->m_bHostPermittedDialog = TRUE;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[3] == 0);

    message.m_bInHandleBlockingMessages = TRUE;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[3] == 3);
    message.m_bInHandleBlockingMessages = FALSE;
    pSettings->m_bHostPermittedDialog = FALSE;

    g_pBaldurChitin = pSavedBaldur;

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CBaldurMessage::SendChatMessage (0x432C50): the chat formatting and the
// chat-buffer path. A plain message becomes "[<local player>]:  <text>"; a
// "name: text" whose name is a player in the session becomes
// "[<local>->name]:  <text>" (a whisper, sent to that player); an unknown
// name stays in the text; with no session the message is the joke answer
// "You talking to me?". The caller's string and the local chat buffer both
// receive the formatted text. The network send itself needs a live
// DirectPlay session, so the test only observes the formatting and the
// buffer (the unit-test route from report-T122.md row 8).
TEST_CASE("CBaldurMessage::SendChatMessage formats the local, target and disconnected chat", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;

    network.m_sLocalPlayerName = "sender";
    network.m_psPlayerName[0] = "target";
    network.m_pPlayerID[0] = 1;
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;

    // A plain message is announced with the local player's name.
    CString sText("hello");
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "hello"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "[sender]:  hello");

    // "target: text" to a player in the session is a whisper.
    message.m_cChatBuffer.ClearMessages();
    sText = "target: whisper";
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "target: whisper"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "[sender->target]:   whisper");

    // A name that is not a player stays part of the text.
    message.m_cChatBuffer.ClearMessages();
    sText = "nobody: hello";
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "nobody: hello"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "[sender]:  nobody: hello");

    // No session: the joke answer, still added to the local buffer.
    network.m_bConnectionEstablished = FALSE;
    message.m_cChatBuffer.ClearMessages();
    sText = "hello";
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "hello"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "You talking to me?");

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CBaldurMessage::OnSwapItemRequest (0x4319C0): the SWAPITEM 73/82 host
// handler. The item/unmarshal paths need a real CGameSprite/CItem, so the
// unit test pins the guards that need no game state: a zeroed game's remote
// object array misses the request's remote id, and the handler still answers
// TRUE with the 5-byte reply {BYTE result, DWORD errorCode} sent as
// SWAPITEM 73/114 to the requester. The packet is in the static buffer:
// 0xFF, 73, 114, {0, -1}. The m_bInHandleBlockingMessages gate also refuses
// the swap and replies the same miss.
TEST_CASE("CBaldurMessage::OnSwapItemRequest replies a miss for an unknown remote object", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;

    // A zeroed CBaldurChitin/CInfGame: GetRemoteObjectArray()->Find always
    // misses (the array size is 0) and no swap is attempted.
    CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> game(new unsigned char[sizeof(CInfGame)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(game.get());

    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // {BYTE source 'G'; DWORD remotePlayerID 1; DWORD remoteObjectID 0;
    //  SHORT slot 0; SHORT count 0; BYTE hasItem 0}
    BYTE packet[3 + 1 + 4 + 4 + 2 + 2 + 1];
    memset(packet, 0, sizeof(packet));
    packet[3] = 0x47;
    DWORD nRemotePlayer = 1;
    memcpy(packet + 4, &nRemotePlayer, sizeof(nRemotePlayer));

    network.m_psPlayerName[0] = "sender";
    network.m_pPlayerID[0] = 1;
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;

    memset(pBuffer, 0xCD, 3 + 5 + 1);
    CHECK(message.OnSwapItemRequest(0, packet, sizeof(packet)) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == 73);
    CHECK(pBuffer[2] == 114);
    CHECK(pBuffer[3] == 0); // result: the Find missed
    // errorCode = -1
    CHECK(pBuffer[4] == 0xFF);
    CHECK(pBuffer[5] == 0xFF);
    CHECK(pBuffer[6] == 0xFF);
    CHECK(pBuffer[7] == 0xFF);
    CHECK(pBuffer[8] == 0xCD);

    // The blocking-message loop refuses the swap too; the miss packet is
    // the same.
    message.m_bInHandleBlockingMessages = TRUE;
    memset(pBuffer, 0xCD, 3 + 5 + 1);
    CHECK(message.OnSwapItemRequest(0, packet, sizeof(packet)) == TRUE);
    CHECK(pBuffer[3] == 0);
    message.m_bInHandleBlockingMessages = FALSE;

    g_pBaldurChitin = pSavedBaldur;

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CBaldurMessage::SwapItemReply (0x431CA0): a successful swap with no item to
// return is the 6-byte {result, hasItem = 0, errorCode} packet — the second
// byte is "has an item" (a local BOOLEAN in the dead nMsgFrom slot,
// 0x431D52-0x431D59), not the result again, and there is no item block to
// dereference.
TEST_CASE("CBaldurMessage::SwapItemReply answers a successful swap with no item", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;

    network.m_psPlayerName[0] = "sender";
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_pPlayerID[0] = 1;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;

    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    memset(pBuffer, 0xCD, 3 + 6 + 1);
    CHECK(message.SwapItemReply(0, TRUE, NULL, -1) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == 73);
    CHECK(pBuffer[2] == 114);
    CHECK(pBuffer[3] == 1); // result
    CHECK(pBuffer[4] == 0); // no item to return
    CHECK(pBuffer[5] == 0xFF); // errorCode = -1
    CHECK(pBuffer[6] == 0xFF);
    CHECK(pBuffer[7] == 0xFF);
    CHECK(pBuffer[8] == 0xFF);
    CHECK(pBuffer[9] == 0xCD);

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CBaldurMessage::DemandCharacterSlotUpdate (0x42DF50): the PLAYERCHAR 80/85
// {SHORT characterSlot} demand sent to one player. No game state is needed,
// so the unit test pins the session and empty-name guards and the wire bytes
// in CNetwork::STATIC_MESSAGE_BUFFER.
TEST_CASE("CBaldurMessage::DemandCharacterSlotUpdate sends the update demand to a player", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;

    network.m_psPlayerName[1] = "remote";
    network.m_pbPlayerVisible[1] = TRUE;
    network.m_pPlayerID[1] = 2;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;

    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    memset(pBuffer, 0xCD, 3 + sizeof(SHORT) + 1);
    CHECK(message.DemandCharacterSlotUpdate(4, 1) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == CBaldurMessage::MSG_TYPE_PLAYERCHAR);
    CHECK(pBuffer[2] == CBaldurMessage::MSG_SUBTYPE_PLAYERCHAR_UPDATE_DEMAND);
    CHECK(pBuffer[3] == 4); // the character slot, little-endian SHORT
    CHECK(pBuffer[4] == 0);
    CHECK(pBuffer[5] == 0xCD);

    // An empty player name refuses to send.
    network.m_pbPlayerVisible[1] = FALSE;
    memset(pBuffer, 0xCD, 3 + sizeof(SHORT) + 1);
    CHECK(message.DemandCharacterSlotUpdate(4, 1) == FALSE);
    CHECK(pBuffer[3] == 0xCD);

    // A closed session refuses to send.
    network.m_pbPlayerVisible[1] = TRUE;
    network.m_bConnectionEstablished = FALSE;
    memset(pBuffer, 0xCD, 3 + sizeof(SHORT) + 1);
    CHECK(message.DemandCharacterSlotUpdate(4, 1) == FALSE);
    CHECK(pBuffer[3] == 0xCD);

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CBaldurMessage::UpdateDemandCharacters (0x42D860): the slot walk and the
// progress bar. A zeroed CBaldurChitin/CInfGame with every m_characters slot
// at INVALID_INDEX makes GetDeny answer BAD_INDEX before it locks the array,
// so each slot is skipped the way an absent character is and the test can
// pin the connection guard, the 0..5 vs single-slot range, the 500000 step
// and the bar's show/clear pair without a game.
TEST_CASE("CBaldurMessage::UpdateDemandCharacters walks the slots and drives the progress bar", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;

    CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> game(new unsigned char[sizeof(CInfGame)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(game.get());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    for (INT i = 0; i < 6; i++) {
        pGame->m_characters[i] = CGameObjectArray::INVALID_INDEX;
    }

    CBaldurMessage message;

    // No connection: nothing is touched.
    CHECK(network.m_bConnectionEstablished == FALSE);
    message.UpdateDemandCharacters(TRUE, 0, TRUE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == 0);
    CHECK(g_pChitin->cProgressBar.m_bWaiting == FALSE);

    network.m_bConnectionEstablished = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    network.m_bIsHost = TRUE;

    // Update all: slots 0..5 are all absent, so each is skipped and still
    // advances the bar by 500000. The bar flag starts FALSE and no slot was
    // handled, so the cleanup must not clear a waiting bar (the old
    // initialiser copied bUpdateAllCharacters and always cleaned up).
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    g_pChitin->cProgressBar.m_bWaiting = TRUE;
    message.UpdateDemandCharacters(TRUE, 0, TRUE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == 6 * 500000);
    CHECK(g_pChitin->cProgressBar.m_nWaitingReason == 0);
    CHECK(g_pChitin->cProgressBar.m_bWaiting == TRUE);
    g_pChitin->cProgressBar.m_bWaiting = FALSE;

    // A single slot is one iteration; every character is absent, so the
    // GetDeny-success branch (which shows the bar with reason 19714 and
    // demands the character from its controller) is not reached here.
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    message.UpdateDemandCharacters(FALSE, 3, TRUE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == 500000);
    CHECK(g_pChitin->cProgressBar.m_nWaitingReason == 0);
    CHECK(g_pChitin->cProgressBar.m_bWaiting == FALSE);

    // A slot outside 0..5 uses INVALID_INDEX and still takes one step.
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    message.UpdateDemandCharacters(FALSE, 7, TRUE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == 500000);

    // bProgressBarInPlace == FALSE: the same walk, no step and no show.
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    g_pChitin->cProgressBar.m_nWaitingReason = 0;
    message.UpdateDemandCharacters(FALSE, 3, FALSE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == 0);
    CHECK(g_pChitin->cProgressBar.m_nWaitingReason == 0);

    g_pBaldurChitin = pSavedBaldur;

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CBaldurMessage::SendMapWorldAnnounceStatus (0x4354E0): the host broadcasts
// MAPWORLD/115 with the world map's active flag, controlling player and the
// edge the party is leaving through. SendSpecificMessage writes the packet
// into CNetwork::STATIC_MESSAGE_BUFFER whenever a non-null provider is
// selected; SendMessage_ then reports the send failed (DirectPlay is gone),
// but the wire bytes are the ones the original sends.
TEST_CASE("msg: MapWorld announce status writes MAPWORLD/115 for the host only", "[network][msg]")
{
    ChitinScope scope;
    REQUIRE(g_pChitin != nullptr);

    CNetwork& network = g_pChitin->cNetwork;
    const INT nProvider = network.m_nServiceProvider;
    const BOOLEAN bConnection = network.m_bConnectionEstablished;
    const BOOLEAN bIsHost = network.m_bIsHost;

    CBaldurMessage message;

    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;

    // A closed session, or a client, answers FALSE and leaves the buffer
    // untouched.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMapWorldAnnounceStatus(TRUE, 7, -3) == FALSE);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[CNetwork::SPEC_MSG_TYPE] == 0);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[CNetwork::SPEC_MSG_SUBTYPE] == 0);

    network.m_bConnectionEstablished = TRUE;
    CHECK(message.SendMapWorldAnnounceStatus(TRUE, 7, -3) == FALSE);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[CNetwork::SPEC_MSG_TYPE] == 0);

    // The host sends it to all players, guaranteed: the flag byte, the
    // MAPWORLD type, the announce-status subtype and
    // {BOOLEAN active; DWORD controller; LONG leaving edge}, 9 bytes.
    network.m_bIsHost = TRUE;
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMapWorldAnnounceStatus(TRUE, 7, -3) == TRUE);

    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;
    CHECK(pBuffer[CNetwork::SPEC_MSG_FLAG] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[CNetwork::SPEC_MSG_TYPE] == CBaldurMessage::MSG_TYPE_MAPWORLD);
    CHECK(pBuffer[CNetwork::SPEC_MSG_SUBTYPE] == CBaldurMessage::MSG_SUBTYPE_MAPWORLD_ANNOUNCE_STATUS);
    CHECK(pBuffer[CNetwork::SPEC_MSG_HEADER_LENGTH] == 1);

    DWORD nController = 0;
    std::memcpy(&nController, pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + 1, sizeof(nController));
    CHECK(nController == 7);

    LONG nLeavingEdge = 0;
    std::memcpy(&nLeavingEdge,
        pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + 1 + sizeof(nController),
        sizeof(nLeavingEdge));
    CHECK(nLeavingEdge == -3);

    // The values travel as they are, zero included.
    std::memset(pBuffer, 0, STATIC_MESSAGE_SIZE);
    CHECK(message.SendMapWorldAnnounceStatus(FALSE, 0, 0) == TRUE);
    CHECK(pBuffer[CNetwork::SPEC_MSG_HEADER_LENGTH] == 0);
    nController = 1;
    std::memcpy(&nController, pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + 1, sizeof(nController));
    CHECK(nController == 0);
    nLeavingEdge = 1;
    std::memcpy(&nLeavingEdge,
        pBuffer + CNetwork::SPEC_MSG_HEADER_LENGTH + 1 + sizeof(nController),
        sizeof(nLeavingEdge));
    CHECK(nLeavingEdge == 0);

    network.m_nServiceProvider = nProvider;
    network.m_bConnectionEstablished = bConnection;
    network.m_bIsHost = bIsHost;
}

// CBaldurMessage::ObjectControlSuggestion (0x430720): the host suggests a
// remote object to another player by sending the object's remote ids to that
// player (OBJECT 79/115). The object only needs its base CGameObject fields
// (m_remotePlayerID/m_remoteObjectID), so a minimal object array is enough.
TEST_CASE("CBaldurMessage::ObjectControlSuggestion sends the remote ids to the suggested player", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;

    CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> game(new unsigned char[sizeof(CInfGame)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(game.get());
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    new (&pGame->m_cObjectArray) CGameObjectArray(10, 10);
    CGameObject* pObject = new CGameObject();
    LONG nObjectID;
    REQUIRE(pGame->m_cObjectArray.Add(&nObjectID, pObject, INFINITE) == CGameObjectArray::SUCCESS);
    // Add resets the ids; the suggestion reads them from the object.
    pObject->m_remotePlayerID = 3;
    pObject->m_remoteObjectID = 4;

    network.m_idLocalPlayer = 1;
    network.m_psPlayerName[0] = "remote";
    network.m_pPlayerID[0] = 2;
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = TRUE;

    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // A closed session is refused before the object lookup.
    network.m_bConnectionEstablished = FALSE;
    memset(pBuffer, 0xCD, 3 + 8 + 1);
    CHECK(message.ObjectControlSuggestion(2, nObjectID) == FALSE);
    CHECK(pBuffer[3] == 0xCD);
    network.m_bConnectionEstablished = TRUE;

    // Only the host suggests.
    network.m_bIsHost = FALSE;
    memset(pBuffer, 0xCD, 3 + 8 + 1);
    CHECK(message.ObjectControlSuggestion(2, nObjectID) == FALSE);
    CHECK(pBuffer[3] == 0xCD);
    network.m_bIsHost = TRUE;

    // Player id 2 is at location 0: the packet carries the object's remote
    // ids {3, 4} as two DWORDs.
    memset(pBuffer, 0xCD, 3 + 8 + 1);
    CHECK(message.ObjectControlSuggestion(2, nObjectID) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == CBaldurMessage::MSG_TYPE_OBJECT);
    CHECK(pBuffer[2] == CBaldurMessage::MSG_SUBTYPE_OBJECT_CONTROL_SUGGEST);
    CHECK(pBuffer[3] == 3);
    CHECK(pBuffer[4] == 0);
    CHECK(pBuffer[5] == 0);
    CHECK(pBuffer[6] == 0);
    CHECK(pBuffer[7] == 4);
    CHECK(pBuffer[8] == 0);
    CHECK(pBuffer[9] == 0);
    CHECK(pBuffer[10] == 0);
    CHECK(pBuffer[11] == 0xCD);

    // The object's remote owner (3) is not a useful suggestion.
    memset(pBuffer, 0xCD, 3 + 8 + 1);
    CHECK(message.ObjectControlSuggestion(3, nObjectID) == FALSE);
    CHECK(pBuffer[3] == 0xCD);

    // A player not in the session is refused.
    memset(pBuffer, 0xCD, 3 + 8 + 1);
    CHECK(message.ObjectControlSuggestion(9, nObjectID) == FALSE);
    CHECK(pBuffer[3] == 0xCD);

    // A missing object is refused.
    CHECK(message.ObjectControlSuggestion(2, nObjectID + 100) == FALSE);

    // The object's owner asking for itself takes the ObjectControlRequest
    // path (still the TODO stub here, so FALSE).
    CHECK(message.ObjectControlSuggestion(1, nObjectID) == FALSE);

    pGame->m_cObjectArray.~CGameObjectArray();
    g_pBaldurChitin = pSavedBaldur;

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}
