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

#include "engine_world.h"

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

// tests/test_derived_spell.cpp's guard: the booted-engine tests need the
// install (IWD2_DATA); without it they skip rather than fail.
#define REQUIRE_ENGINE()                            \
    do {                                            \
        const char* env = std::getenv("IWD2_DATA"); \
        if (env == nullptr || *env == '\0') {       \
            SKIP("[iwd2r-skip] IWD2_DATA unset");   \
        }                                           \
        std::string error;                          \
        INFO(error);                                \
        REQUIRE(engine_world::StartEngine(error));  \
    } while (0)

// A minimal object array and remote-object map for the message codecs. The
// object array owns the object it is given; the remote map entry is
// installed directly because CGameRemoteObjectArray::Add's empty-bucket arm
// is not part of what is under test here.
class MessageGameScope {
public:
    MessageGameScope()
        : m_savedBaldur(g_pBaldurChitin)
    {
        m_baldur.reset(new unsigned char[sizeof(CBaldurChitin)]());
        m_game.reset(new unsigned char[sizeof(CInfGame)]());
        g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(m_baldur.get());
        g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(m_game.get());
        m_pGame = g_pBaldurChitin->GetObjectGame();
        new (&m_pGame->m_cObjectArray) CGameObjectArray(10, 10);
        new (&m_pGame->m_cRemoteObjectArray) CGameRemoteObjectArray(10);
    }

    ~MessageGameScope()
    {
        m_pGame->m_cRemoteObjectArray.~CGameRemoteObjectArray();
        m_pGame->m_cObjectArray.~CGameObjectArray();
        g_pBaldurChitin = m_savedBaldur;
    }

    LONG AddObject(PLAYER_ID player, LONG remote)
    {
        CGameObject* pObject = new CGameObject();
        LONG nObjectId;
        REQUIRE(m_pGame->m_cObjectArray.Add(&nObjectId, pObject, INFINITE) == CGameObjectArray::SUCCESS);
        pObject->m_remotePlayerID = player;
        pObject->m_remoteObjectID = remote;
        return nObjectId;
    }

    void MapRemote(PLAYER_ID player, LONG remote, LONG local)
    {
        CGameRemoteObjectListEntry* entry = new CGameRemoteObjectListEntry();
        entry->remotePlayerID = player;
        entry->remoteObjectID = remote;
        entry->localObjectID = local;
        const SHORT index = static_cast<SHORT>((remote >> 16) & 0x7FFF);
        entry->pNext = m_pGame->m_cRemoteObjectArray.m_pArray[index];
        m_pGame->m_cRemoteObjectArray.m_pArray[index] = entry;
    }

private:
    CBaldurChitin* m_savedBaldur;
    std::unique_ptr<unsigned char[]> m_baldur;
    std::unique_ptr<unsigned char[]> m_game;
    CInfGame* m_pGame;
};

// Every field the codec carries. The CAIObjectType location fields
// (m_ptCenter/m_nRadius/m_rect) and CAIAction::m_internalFlags are not on
// the wire, so the tests below keep them at their constructor defaults.
void CheckObjectTypeEqual(const CAIObjectType& expected, const CAIObjectType& actual)
{
    CHECK(actual.m_sName == expected.m_sName);
    CHECK(actual.m_nEnemyAlly == expected.m_nEnemyAlly);
    CHECK(actual.m_nGeneral == expected.m_nGeneral);
    CHECK(actual.m_nRace == expected.m_nRace);
    CHECK(actual.m_nSubRace == expected.m_nSubRace);
    CHECK(actual.m_nClass == expected.m_nClass);
    CHECK(actual.m_nSpecific == expected.m_nSpecific);
    CHECK(actual.m_nGender == expected.m_nGender);
    CHECK(actual.m_nAlignment == expected.m_nAlignment);
    CHECK(actual.m_nAvClass == expected.m_nAvClass);
    CHECK(actual.m_nClassMask == expected.m_nClassMask);
    CHECK(actual.m_nInstance == expected.m_nInstance);
    CHECK(actual.m_nLocationType == expected.m_nLocationType);
    for (int index = 0; index < 5; index++) {
        CHECK(actual.m_SpecialCase[index] == expected.m_SpecialCase[index]);
    }
}

void CheckActionEqual(const CAIAction& expected, const CAIAction& actual)
{
    CHECK(actual.m_actionID == expected.m_actionID);
    CheckObjectTypeEqual(expected.m_actorID, actual.m_actorID);
    CheckObjectTypeEqual(expected.m_acteeID, actual.m_acteeID);
    CheckObjectTypeEqual(expected.m_acteeID2, actual.m_acteeID2);
    CHECK(actual.m_specificID == expected.m_specificID);
    CHECK(actual.m_specificID2 == expected.m_specificID2);
    CHECK(actual.m_specificID3 == expected.m_specificID3);
    CHECK(actual.m_string1 == expected.m_string1);
    CHECK(actual.m_string2 == expected.m_string2);
    CHECK(actual.m_dest.x == expected.m_dest.x);
    CHECK(actual.m_dest.y == expected.m_dest.y);
    CHECK(actual.m_internalFlags == expected.m_internalFlags);
}
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

// CBaldurMessage::ChapterAnnounceStatus (0x434EB0): the host tells every
// player the chapter number and the chapter's resref as CHAPTER/97 - the
// payload is {BYTE nChapter; BYTE length; the resref chars}, with
// SEND_ALL_PLAYERS | SEND_GUARANTEED.
TEST_CASE("msg: the chapter announce carries the chapter and the resref", "[network][msg]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // No session: the first guard answers FALSE and the buffer is untouched.
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = TRUE;
    memset(pBuffer, 0xCD, 16);
    CHECK(message.ChapterAnnounceStatus(9, CResRef("AR1000")) == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A client does not announce the chapter.
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;
    CHECK(message.ChapterAnnounceStatus(9, CResRef("AR1000")) == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // The host's packet: the three-byte SPEC_MSG header, the chapter, the
    // length byte and the resref.
    network.m_bIsHost = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    memset(pBuffer, 0xCD, 16);
    CHECK(message.ChapterAnnounceStatus(9, CResRef("AR1000")) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == CBaldurMessage::MSG_TYPE_CHAPTER);
    CHECK(pBuffer[2] == CBaldurMessage::MSG_SUBTYPE_CHAPTER_ANNOUNCE_STATUS);
    CHECK(CBaldurMessage::MSG_TYPE_CHAPTER == 74);
    CHECK(CBaldurMessage::MSG_SUBTYPE_CHAPTER_ANNOUNCE_STATUS == 97);
    CHECK(pBuffer[3] == 9);
    CHECK(pBuffer[4] == 6);
    CHECK(memcmp(pBuffer + 5, "AR1000", 6) == 0);

    // An empty resref: a zero length byte, nothing after it.
    memset(pBuffer, 0xCD, 16);
    CHECK(message.ChapterAnnounceStatus(3, CResRef("")) == TRUE);
    CHECK(pBuffer[3] == 3);
    CHECK(pBuffer[4] == 0);
    CHECK(pBuffer[5] == 0xCD);

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

// The handler's response computation and the 1-byte LEAVEAREALUA 88/114
// reply on a zeroed game: the remote object always misses (so no feedback
// sprite), the area is never loaded and the settings are the only variable.
// Codes seen in the original: 0 deny, 3 busy/blocking, 4 paused. The allow
// code 1 lives in the booted-engine test below: the reply and the transition
// are one — the handler calls CInfGame::LeaveAreaLUA straight after
// marshalling reply 1 — and the transition's LoadArea resolves the
// destination through the key table (CResHelper::SetResRef,
// src/CRes.h -> CDimm::GetResObject -> CDimmKeyTable::Hash, whose modulo
// divides by the table's entry count, src/CDimmKeyTable.cpp), which a
// ChitinScope chitin never loads: m_nEntries is 0 and the hash divides by
// zero. The original divides just as unguarded — an area load always runs
// on a built engine — so this is the test's to route around, not the
// port's. Every other code stops the handler before the call, which is
// exactly why the zeroed game can take these arms.
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

    // A remote player in the inventory (field_AC) or the level-up screen
    // (field_B2) denies with 0. The reply is the LEAVEAREALUA 88/114 header
    // plus the one code byte.
    pSettings->field_AC[0] = 0;
    memset(pBuffer, 0xCD, 4 + 1);
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == CBaldurMessage::MSG_TYPE_LEAVEAREALUA);
    CHECK(pBuffer[2] == CBaldurMessage::MSG_SUBTYPE_LEAVEAREALUA_PERMIT_REPLY);
    CHECK(pBuffer[3] == 0);
    CHECK(pBuffer[4] == 0xCD);

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

// The allow arm — reply 1 — on the booted engine: the grant and the
// transition it inseparably performs. A new game stands in its start area;
// asking to travel to the unloaded AR1001 is granted and LeaveAreaLUA's
// LoadArea brings AR1001 in. The session is the zeroed test's fake host on
// the real cNetwork: a real HostSession would force the null provider under
// which SendSpecificMessage marshals nothing, while the TCP/IP provider
// keeps the static-message-buffer seam and its send failure is ignored.
// The in-play fixture is tests/test_combat.cpp's NewGameInStartArea: bare
// NewGame loads no area, so m_gameAreas[m_visibleArea] holds no live area,
// and the head's host SelectEngine(world) — which fires while the create-char
// screen is still active — runs WorldEngineActivated (0x5AF7F0), whose
// m_gameAreas[m_visibleArea]->OnActivation() the original equally never
// guards; it is the test's to set up, not the port's. Seating the party
// member creates AR1000 in slot 0 and pinning pActiveEngine to the world
// screen holds the steady in-play state both SelectEngine conditions skip.
// The remote object misses (no client ever joined), so the move half of the
// transition is skipped exactly as it is for a host alone: GetDeny of the
// invalid index answers BAD_INDEX, not SHARED, the retry loop falls through
// with no leader to move, and ReleaseDeny of the invalid index is a no-op.
TEST_CASE("CBaldurMessage::OnLeaveAreaLuaRequestToServer grants the permit and performs the transition", "[msg][network][data]")
{
    REQUIRE_ENGINE();

    // The main loop's drawn frames (tests/test_gam.cpp's FramePump): the
    // area load and the progress-bar bumps wait for m_bDisplayStale to
    // clear.
    engine_world::FramePump pump;

    std::vector<BYTE> cre = engine_world::FinishedCharacter();
    REQUIRE(!cre.empty());
    engine_world::NewGameInStartArea(cre);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    CNetwork& network = g_pChitin->cNetwork;
    const CString sSavedName = network.m_psPlayerName[0];
    const BOOL bSavedVisible = network.m_pbPlayerVisible[0];
    const auto nSavedPlayerID = network.m_pPlayerID[0];
    const auto nSavedProvider = network.m_nServiceProvider;
    const BOOL bSavedEstablished = network.m_bConnectionEstablished;
    const BOOL bSavedHost = network.m_bIsHost;
    const auto nSavedLocalPlayer = network.m_idLocalPlayer;
    const auto nSavedHostPlayer = network.m_nHostPlayer;

    network.m_psPlayerName[0] = "sender";
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_pPlayerID[0] = 1;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = TRUE;
    // The fake session is coherent: the host is the local player, seated at
    // player location 0. A real session hands every player a nonzero id and
    // marks the host's location; the defaults (local 0, host -1) are the
    // "no session" values, under which LoadArea's ChangeControlOnLoadGame
    // resolves the party member's controller to no player and asserts
    // (CGameRemoteObjectArray.cpp:742) — the original asserts identically,
    // it just never loads an area sessionless.
    network.m_idLocalPlayer = 1;
    network.m_nHostPlayer = 0;

    CBaldurMessage message;

    // {header 0xFF, LEAVEAREALUA 88, permit request 82;
    //  DWORD remotePlayerID 1; DWORD remoteObjectID 0;
    //  LONG x 10; LONG y 20; SHORT direction 3;
    //  BYTE 3 "abc"; BYTE 6 "AR1001"}
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
    memcpy(packet + nSize, "AR1001", 6);
    nSize += 6;

    // Granted: no player is in a character screen, the game is not paused,
    // not in a save/load mode, no dialog is pending and AR1001 is not
    // loaded. Only the grant reaches LeaveAreaLUA — the handler gates the
    // call on the reply — and only the transition loads the destination.
    CHECK(message.OnLeaveAreaLuaRequestToServer(0, packet, nSize) == TRUE);
    CHECK(pGame->GetArea(CString("AR1001")) != NULL);

    // The transition's own status sends (progress bar, the all-clear
    // signal) reuse the static buffer after the reply; the loaded area is
    // the stronger fact, this only notes that the seam stayed live.
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);

    // Leave the embedded CNetwork closed before the process tears the
    // engine down (its destructor would otherwise call OnCloseSession).
    network.m_psPlayerName[0] = sSavedName;
    network.m_pbPlayerVisible[0] = bSavedVisible;
    network.m_pPlayerID[0] = nSavedPlayerID;
    network.m_nServiceProvider = nSavedProvider;
    network.m_bConnectionEstablished = bSavedEstablished;
    network.m_bIsHost = bSavedHost;
    network.m_idLocalPlayer = nSavedLocalPlayer;
    network.m_nHostPlayer = nSavedHostPlayer;
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

    // The local player suggesting to itself takes the ObjectControlRequest
    // path (0x430980): this session is the host, so the control moves
    // through ChangeControl, nothing is sent, and the request answers TRUE.
    memset(pBuffer, 0xCD, 3 + 8 + 1);
    CHECK(message.ObjectControlSuggestion(1, nObjectID) == TRUE);
    CHECK(pBuffer[3] == 0xCD);

    pGame->m_cObjectArray.~CGameObjectArray();
    g_pBaldurChitin = pSavedBaldur;

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_bConnectionEstablished = FALSE;
    network.m_bIsHost = FALSE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}

// CMessageSetVariable::UnmarshalMessage (0x50A6C0) reads the wire in the
// order MarshalMessage (0x50A4D0) writes it: the name first (stored to +0x10
// at 0x50A75F), then the scope (+0xC at 0x50A79B). The fork import read the
// first string into the scope, so every received SetVariable swapped the two.
TEST_CASE("msg: CMessageSetVariable::UnmarshalMessage reads the name before the scope", "[network][msg]")
{
    // A zeroed CBaldurChitin/CInfGame: GetRemoteObjectArray()->Find misses.
    CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> game(new unsigned char[sizeof(CInfGame)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(game.get());

    // {header[3]; PLAYER_ID 1; LONG remote 0; SHORT 5 "KAPUT"; SHORT 6 "GLOBAL";
    //  LONG value 7; BYTE increment 1; DWORD flags 0x0F}
    BYTE packet[3 + 4 + 4 + 2 + 5 + 2 + 6 + 4 + 1 + 4];
    memset(packet, 0, sizeof(packet));
    DWORD cnt = 3;
    PLAYER_ID nPlayer = 1;
    memcpy(packet + cnt, &nPlayer, sizeof(nPlayer));
    cnt += sizeof(nPlayer) + sizeof(LONG);
    SHORT nLength = 5;
    memcpy(packet + cnt, &nLength, sizeof(nLength));
    cnt += sizeof(nLength);
    memcpy(packet + cnt, "KAPUT", 5);
    cnt += 5;
    nLength = 6;
    memcpy(packet + cnt, &nLength, sizeof(nLength));
    cnt += sizeof(nLength);
    memcpy(packet + cnt, "GLOBAL", 6);
    cnt += 6;
    LONG nValue = 7;
    memcpy(packet + cnt, &nValue, sizeof(nValue));
    cnt += sizeof(nValue);
    packet[cnt++] = 1;
    DWORD dwFlags = 0x0F;
    memcpy(packet + cnt, &dwFlags, sizeof(dwFlags));
    cnt += sizeof(dwFlags);
    REQUIRE(cnt == sizeof(packet));

    CMessageSetVariable message(CString(""), CString(""), 0, 0, 0);
    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
    CHECK(message.m_targetId == CGameObjectArray::INVALID_INDEX);
    CHECK(message.m_sName == "KAPUT");
    CHECK(message.m_sScope == "GLOBAL");
    CHECK(message.m_nValue == 7);
    CHECK(message.m_bIncrement == 1);
    CHECK(message.m_dwFlags == 0x0F);

    g_pBaldurChitin = pSavedBaldur;
}

// CMessageAddAction::MarshalMessage (0x4F7AE0) / UnmarshalMessage
// (0x4F80B0): an action is the target's remote ids, the action id, three
// CAIObjectTypes and the action tail (three specific ids, two strings and
// the destination). An empty action still sends its three objects: a zero
// length name, the nine one-byte selectors, the class mask, the (0, -1)
// instance fallback and five zero special-case bytes. The fixed size is
// 8 + 2 + 3 * (2 + 13 + 8 + 5) + 12 + 4 + 8 = 118.
TEST_CASE("msg: CMessageAddAction round-trips an empty action", "[network][msg]")
{
    ChitinScope chitin;
    MessageGameScope game;
    const LONG nTarget = game.AddObject(7, 0x00010002);
    game.MapRemote(7, 0x00010002, nTarget);

    CAIAction action;
    action.m_actionID = CAIAction::NO_ACTION;
    action.m_actorID = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    action.m_acteeID = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    action.m_acteeID2 = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    action.m_specificID = 0;
    action.m_specificID2 = 0;
    action.m_specificID3 = 0;
    action.m_string1 = "";
    action.m_string2 = "";
    action.m_dest = CPoint(0, 0);
    action.m_internalFlags = 0;

    CMessageAddAction message(action, 0, nTarget);
    CHECK(message.GetCommType() == CMessage::SEND);
    CHECK(message.GetMsgType() == CBaldurMessage::MSG_TYPE_CMESSAGE);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ADD_ACTION);

    BYTE packed[0x4000];
    memset(packed, 0xCD, sizeof(packed));
    BYTE* pPacked = packed;
    DWORD packedSize = 0;
    message.MarshalMessage(&pPacked, &packedSize);

    CHECK(pPacked == packed);
    CHECK(packedSize == 118);
    CHECK(packed[packedSize] == 0xCD);

    BYTE expected[0x4000];
    DWORD expectedSize = 0;
    auto put = [&](const void* p, DWORD n) {
        memcpy(expected + expectedSize, p, n);
        expectedSize += n;
    };

    const PLAYER_ID targetPlayer = 7;
    const LONG targetRemote = 0x00010002;
    const SHORT actionID = CAIAction::NO_ACTION;
    put(&targetPlayer, sizeof(targetPlayer));
    put(&targetRemote, sizeof(targetRemote));
    put(&actionID, sizeof(actionID));
    for (int index = 0; index < 3; index++) {
        const SHORT nameLength = 0;
        put(&nameLength, sizeof(nameLength));
        const BYTE selectors[9] = {};
        put(selectors, sizeof(selectors));
        const DWORD classMask = 0;
        put(&classMask, sizeof(classMask));
        const PLAYER_ID instancePlayer = 0;
        const LONG instanceRemote = CGameObjectArray::INVALID_INDEX;
        put(&instancePlayer, sizeof(instancePlayer));
        put(&instanceRemote, sizeof(instanceRemote));
        const BYTE specialCase[5] = {};
        put(specialCase, sizeof(specialCase));
    }
    const LONG zero = 0;
    put(&zero, sizeof(zero));
    put(&zero, sizeof(zero));
    put(&zero, sizeof(zero));
    const SHORT stringLength = 0;
    put(&stringLength, sizeof(stringLength));
    put(&stringLength, sizeof(stringLength));
    put(&zero, sizeof(zero));
    put(&zero, sizeof(zero));
    REQUIRE(expectedSize == packedSize);
    CHECK(memcmp(packed, expected, packedSize) == 0);

    BYTE packet[0x4000 + 3];
    packet[0] = CNetwork::SPEC_MSG_FLAG_ENABLED;
    packet[1] = CBaldurMessage::MSG_TYPE_CMESSAGE;
    packet[2] = CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ADD_ACTION;
    memcpy(packet + CNetwork::SPEC_MSG_HEADER_LENGTH, packed, packedSize);

    CMessageAddAction received(CAIAction::NULL_ACTION, 0, CGameObjectArray::INVALID_INDEX);
    CHECK(received.UnmarshalMessage(packet, CNetwork::SPEC_MSG_HEADER_LENGTH + packedSize) == TRUE);
    CHECK(received.m_targetId == nTarget);
    CheckActionEqual(action, received.m_action);
}

// The same codec with every carried field populated: names, the nine
// selectors, class masks, special cases, two different instances (one
// resolving through the remote map, one taking the (0, -1) fallback), the
// three specific ids, both strings and a negative destination. Names are 5,
// 5 and 6 bytes and the strings 10 and 11, so the payload is 118 + 16 + 21
// = 155 bytes.
TEST_CASE("msg: CMessageAddAction round-trips a populated action", "[network][msg]")
{
    ChitinScope chitin;
    MessageGameScope game;
    const LONG nTarget = game.AddObject(7, 0x00010002);
    game.MapRemote(7, 0x00010002, nTarget);

    CAIAction action;
    action.m_actionID = CAIAction::SPELL;

    action.m_actorID = CAIObjectType(2, 1, 1, 2, CAIObjectType::C_WIZARD, 3, 1, 2,
        nTarget, CAIObjectType::C_FIGHTER, CLASSMASK_WIZARD);
    action.m_actorID.m_sName = "ACTOR";
    const BYTE actorSpecialCase[5] = { 1, 2, 3, 4, 5 };
    action.m_actorID.SetSpecialCase(actorSpecialCase);

    action.m_acteeID = CAIObjectType(4, 2, 3, 4, CAIObjectType::C_CLERIC, 5, 2, 1,
        nTarget, CAIObjectType::C_CLERIC, CLASSMASK_CLERIC);
    action.m_acteeID.m_sName = "ACTEE";
    const BYTE acteeSpecialCase[5] = { 6, 7, 8, 9, 10 };
    action.m_acteeID.SetSpecialCase(acteeSpecialCase);

    action.m_acteeID2 = CAIObjectType(3, 3, 5, 6, CAIObjectType::C_ROGUE, 7, 1, 3,
        -1, CAIObjectType::C_ROGUE, CLASSMASK_ROGUE);
    action.m_acteeID2.m_sName = "ACTEE2";
    const BYTE actee2SpecialCase[5] = { 11, 12, 13, 14, 15 };
    action.m_acteeID2.SetSpecialCase(actee2SpecialCase);

    action.m_specificID = 11;
    action.m_specificID2 = 22;
    action.m_specificID3 = 33;
    action.m_string1 = "SPELL_NAME";
    action.m_string2 = "TARGET_NAME";
    action.m_dest = CPoint(1234, -567);
    action.m_internalFlags = 0;

    CMessageAddAction message(action, 0, nTarget);

    BYTE packed[0x4000];
    memset(packed, 0xCD, sizeof(packed));
    BYTE* pPacked = packed;
    DWORD packedSize = 0;
    message.MarshalMessage(&pPacked, &packedSize);

    CHECK(pPacked == packed);
    CHECK(packedSize == 155);
    CHECK(packed[packedSize] == 0xCD);

    BYTE expected[0x4000];
    DWORD expectedSize = 0;
    auto put = [&](const void* p, DWORD n) {
        memcpy(expected + expectedSize, p, n);
        expectedSize += n;
    };
    auto putObject = [&](const char* name,
                         BYTE enemyAlly,
                         BYTE general,
                         BYTE race,
                         BYTE subRace,
                         BYTE objectClass,
                         BYTE specific,
                         BYTE gender,
                         BYTE alignment,
                         BYTE avClass,
                         DWORD classMask,
                         PLAYER_ID instancePlayer,
                         LONG instanceRemote,
                         const BYTE specialCase[5]) {
        const SHORT nameLength = static_cast<SHORT>(strlen(name));
        put(&nameLength, sizeof(nameLength));
        put(name, nameLength);
        put(&enemyAlly, sizeof(enemyAlly));
        put(&general, sizeof(general));
        put(&race, sizeof(race));
        put(&subRace, sizeof(subRace));
        put(&objectClass, sizeof(objectClass));
        put(&specific, sizeof(specific));
        put(&gender, sizeof(gender));
        put(&alignment, sizeof(alignment));
        put(&avClass, sizeof(avClass));
        put(&classMask, sizeof(classMask));
        put(&instancePlayer, sizeof(instancePlayer));
        put(&instanceRemote, sizeof(instanceRemote));
        put(specialCase, 5);
    };

    const PLAYER_ID targetPlayer = 7;
    const LONG targetRemote = 0x00010002;
    const SHORT actionID = CAIAction::SPELL;
    put(&targetPlayer, sizeof(targetPlayer));
    put(&targetRemote, sizeof(targetRemote));
    put(&actionID, sizeof(actionID));
    putObject("ACTOR", 2, 1, 1, 2, CAIObjectType::C_WIZARD, 3, 1, 2, CAIObjectType::C_FIGHTER,
        CLASSMASK_WIZARD, 7, 0x00010002, actorSpecialCase);
    putObject("ACTEE", 4, 2, 3, 4, CAIObjectType::C_CLERIC, 5, 2, 1, CAIObjectType::C_CLERIC,
        CLASSMASK_CLERIC, 7, 0x00010002, acteeSpecialCase);
    putObject("ACTEE2", 3, 3, 5, 6, CAIObjectType::C_ROGUE, 7, 1, 3, CAIObjectType::C_ROGUE,
        CLASSMASK_ROGUE, 0, CGameObjectArray::INVALID_INDEX, actee2SpecialCase);
    const LONG specificID = 11;
    const LONG specificID2 = 22;
    const LONG specificID3 = 33;
    put(&specificID, sizeof(specificID));
    put(&specificID2, sizeof(specificID2));
    put(&specificID3, sizeof(specificID3));
    const SHORT string1Length = 10;
    put(&string1Length, sizeof(string1Length));
    put("SPELL_NAME", string1Length);
    const SHORT string2Length = 11;
    put(&string2Length, sizeof(string2Length));
    put("TARGET_NAME", string2Length);
    const LONG destX = 1234;
    const LONG destY = -567;
    put(&destX, sizeof(destX));
    put(&destY, sizeof(destY));
    REQUIRE(expectedSize == packedSize);
    CHECK(memcmp(packed, expected, packedSize) == 0);

    BYTE packet[0x4000 + 3];
    packet[0] = CNetwork::SPEC_MSG_FLAG_ENABLED;
    packet[1] = CBaldurMessage::MSG_TYPE_CMESSAGE;
    packet[2] = CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ADD_ACTION;
    memcpy(packet + CNetwork::SPEC_MSG_HEADER_LENGTH, packed, packedSize);

    CMessageAddAction received(CAIAction::NULL_ACTION, 0, CGameObjectArray::INVALID_INDEX);
    CHECK(received.UnmarshalMessage(packet, CNetwork::SPEC_MSG_HEADER_LENGTH + packedSize) == TRUE);
    CHECK(received.m_targetId == nTarget);
    CheckActionEqual(action, received.m_action);
}

// The failure values: a target the object array cannot share makes the
// marshaller write nothing and zero the size; a wire target the remote map
// cannot resolve leaves m_targetId alone; and an object instance whose
// remote ids resolve to neither a local id nor the (0, -1) fallback makes
// the unmarshaller return FALSE after the target was already accepted.
TEST_CASE("msg: CMessageAddAction marshal/unmarshal failure paths", "[network][msg]")
{
    ChitinScope chitin;
    MessageGameScope game;

    CAIAction action;
    action.m_actorID = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    action.m_acteeID = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    action.m_acteeID2 = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    action.m_dest = CPoint(0, 0);
    action.m_internalFlags = 0;

    CMessageAddAction missing(action, 0, CGameObjectArray::INVALID_INDEX);
    BYTE packed[0x4000];
    memset(packed, 0xCD, sizeof(packed));
    BYTE* pPacked = packed;
    DWORD packedSize = 0xDEADBEEF;
    missing.MarshalMessage(&pPacked, &packedSize);
    CHECK(packedSize == 0);
    CHECK(pPacked == packed);
    CHECK(packed[0] == 0xCD);

    // A target remote id the remote map does not know: Find misses, so the
    // message is rejected before m_targetId changes.
    BYTE targetMiss[0x4000];
    memset(targetMiss, 0, sizeof(targetMiss));
    const PLAYER_ID unknownPlayer = 99;
    const LONG targetRemote = 0x00010002;
    memcpy(targetMiss + CNetwork::SPEC_MSG_HEADER_LENGTH, &unknownPlayer, sizeof(unknownPlayer));
    memcpy(targetMiss + CNetwork::SPEC_MSG_HEADER_LENGTH + sizeof(PLAYER_ID), &targetRemote, sizeof(targetRemote));
    CMessageAddAction received(CAIAction::NULL_ACTION, 0, 12345);
    CHECK(received.UnmarshalMessage(targetMiss, CNetwork::SPEC_MSG_HEADER_LENGTH + 118) == FALSE);
    CHECK(received.m_targetId == 12345);

    // Accept the target, then fail on the first object's remote ids. The
    // object block starts at payload offset 10; with an empty name its
    // instance ids are at 25 (player) and 29 (object).
    const LONG nTarget = game.AddObject(7, 0x00010002);
    game.MapRemote(7, 0x00010002, nTarget);

    CMessageAddAction empty(action, 0, nTarget);
    BYTE payload[0x4000];
    memset(payload, 0xCD, sizeof(payload));
    BYTE* pPayload = payload;
    DWORD payloadSize = 0;
    empty.MarshalMessage(&pPayload, &payloadSize);
    REQUIRE(payloadSize == 118);

    BYTE objectMiss[0x4000];
    objectMiss[0] = CNetwork::SPEC_MSG_FLAG_ENABLED;
    objectMiss[1] = CBaldurMessage::MSG_TYPE_CMESSAGE;
    objectMiss[2] = CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ADD_ACTION;
    memcpy(objectMiss + CNetwork::SPEC_MSG_HEADER_LENGTH, payload, payloadSize);
    const PLAYER_ID badPlayer = 99;
    const LONG badRemote = 0x00020003;
    memcpy(objectMiss + CNetwork::SPEC_MSG_HEADER_LENGTH + 25, &badPlayer, sizeof(badPlayer));
    memcpy(objectMiss + CNetwork::SPEC_MSG_HEADER_LENGTH + 29, &badRemote, sizeof(badRemote));

    CMessageAddAction objectResult(CAIAction::NULL_ACTION, 0, CGameObjectArray::INVALID_INDEX);
    CHECK(objectResult.UnmarshalMessage(objectMiss, CNetwork::SPEC_MSG_HEADER_LENGTH + payloadSize) == FALSE);
    CHECK(objectResult.m_targetId == nTarget);
}
