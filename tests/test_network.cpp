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
