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
