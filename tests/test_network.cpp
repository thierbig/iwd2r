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
    // advances the bar by 500000.
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    message.UpdateDemandCharacters(TRUE, 0, TRUE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == 6 * 500000);
    CHECK(g_pChitin->cProgressBar.m_nWaitingReason == 0);
    CHECK(g_pChitin->cProgressBar.m_bWaiting == FALSE);

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
