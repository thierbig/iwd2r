#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CMultiplayerSettings.h"
#include "CNetwork.h"
#include "CScreenSinglePlayer.h"
#include "CScreenWorld.h"
#include "CUIControlTextDisplay.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "engine_world.h"

#include <cstring>
#include <string>
#include <vector>

// CMessageEnterDialogMode::Run (0x4FE5B0) moves the world screen into
// conversation mode on every dialog start. StartDialog queues the message
// through AddMessage(pMessage, FALSE), and its GetCommType() is
// BROADCAST_FORCED_OTHERS (0x4536E0): Broadcast (0x4F7620) with
// bSendMessageToSelf == FALSE deletes it locally, so single player never
// runs it. It runs only on the "other" machines of a multiplayer session,
// where the service provider is real and GetPanel_21_0() (0x696200)
// resolves to dialog panel 21 instead of the single-player stand-in 0.
// This test hosts the null-provider session of test_sp_host.cpp, brings
// the world up with one character, then emulates that other machine: the
// provider is poked to TCP/IP and the sprite is marked as the local
// player's so the in-control branch is taken (0x502, not 0x182).

extern CChitin* g_pChitin;
extern CBaldurChitin* g_pBaldurChitin;

namespace {

// The null-provider host session of a single-player game, closed again
// however the test ends so later tests see no session (test_sp_host.cpp).
class HostSession {
public:
    HostSession()
    {
        CNetwork& network = g_pChitin->cNetwork;
        REQUIRE(network.EnumerateServiceProviders() == TRUE);
        REQUIRE(network.SelectServiceProvider(CNetwork::SERV_PROV_NULL) == TRUE);
        REQUIRE(network.InitializeConnectionToServiceProvider(TRUE) == TRUE);

        network.m_sSessionNameToMake = "s";
        network.m_bSessionNameToMake = TRUE;
        network.m_sLocalPlayerName = "p";
        network.m_bPlayerNameToMake = TRUE;

        REQUIRE(network.HostNewSession() == TRUE);

        INT err = -1;
        REQUIRE(network.CreatePlayer(err) == TRUE);
        REQUIRE(err == CNetwork::ERROR_NONE);

        REQUIRE(network.GetSessionOpen() == TRUE);
        REQUIRE(network.GetSessionHosting() == TRUE);
    }

    ~HostSession() { g_pChitin->cNetwork.CloseSession(TRUE); }

    HostSession(const HostSession&) = delete;
    HostSession& operator=(const HostSession&) = delete;
};

// engine_world::NewGameInStartArea as a single-player game runs it: a party
// slot becomes a character when creation completes, and only a slot marked
// CHARSTATUS_CHARACTER survives CInfGame::UpdateCharacterSlots once a
// session is open (test_sp_host.cpp).
CGameSprite* NewGameInStartAreaHost(std::vector<BYTE>& bytes)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    for (INT nSlot = 0; nSlot < CINFGAME_MAXCHARACTERS; nSlot++) {
        if (pGame->GetCharacterSlot(nSlot) != -1) {
            pGame->ClearCharacterSlot(nSlot);
        }
    }

    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    pSprite->m_sName = "Probe";
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->GetMultiplayerSettings()->SignalCharacterStatus(0,
        CMultiplayerSettings::CHARSTATUS_SIGNAL_CREATION_COMPLETE,
        TRUE,
        TRUE);
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->EngineGameInit();
    g_pBaldurChitin->pActiveEngine = pWorld;
    pGame->m_bGameLoaded = TRUE;
    return pSprite;
}

// Clear the slot the way the engine clears a leaving character, so a tag
// filter running later cases in this process does not trip NewGame's
// empty-game assert (test_sp_host.cpp).
void ClearPartySlot(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const INT nSlot = pGame->GetCharacterSlotFromId(pSprite->GetId());
    if (nSlot != -1) {
        pGame->ClearCharacterSlot(nSlot);
    }
}

} // namespace

TEST_CASE("enter dialog mode: Run moves the world screen into dialog", "[dialog][message][data]")
{
    if (engine_world::DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(engine_world::StartEngine(error));
    INFO(error);
    engine_world::FramePump pump;
    std::vector<BYTE> bytes = engine_world::FinishedCharacter();
    if (bytes.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    // The retail launcher's WinMain runs CChitin::InitializeServices before
    // any engine does, and CSoundMixer::Initialize creates the channel
    // objects (InitializeChannels, 0x7AB6B0) before it even touches
    // DirectSound — so every machine reaching Run(), with or without a sound
    // device, has them. The harness starts at CBaldurChitin::Init and skips
    // WinMain, leaving the channel array empty; Run()'s SelectEngine reaches
    // CGameArea::OnActivation, whose ApplyVolumeSliders tail derefs the
    // channels without a guard. Create them as WinMain would. Initialize
    // (0x7AB340) first resets m_nMaxChannels to -1 so InitializeChannels'
    // deletion loop runs zero times on the fresh mixer; the ctor leaves 0,
    // whose GetAt(0) would read the empty array.
    g_pChitin->cSoundMixer.m_nMaxChannels = -1;
    g_pChitin->cSoundMixer.InitializeChannels(g_pBaldurChitin->GetNumberSoundChannels());

    HostSession session;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    g_pBaldurChitin->pActiveEngine = pWorld;

    CGameSprite* pSprite = NewGameInStartAreaHost(bytes);
    REQUIRE(pSprite != NULL);

    // Emulate the multiplayer "other" machine Run() executes on: a real
    // provider makes the panel accessors resolve the multiplayer panels,
    // and the sprite belongs to the local player so the in-control branch
    // (game mode 0x502) runs. The provider is restored before teardown so
    // CloseSession does not take a network path under the fake provider.
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    pSprite->m_remotePlayerID = g_pChitin->cNetwork.m_idLocalPlayer;

    CUIPanel* pDialogPanel = pWorld->GetManager()->GetPanel(pWorld->GetPanel_21_0());
    REQUIRE(pDialogPanel != NULL);
    REQUIRE(pDialogPanel->m_nID == 21);
    // EngineGameInit already pointed the dialog display at the panel's
    // text display; Run copies it into the dialog panel's control.
    REQUIRE(pWorld->m_pActiveDialogDisplay != NULL);
    REQUIRE(pDialogPanel->GetControl(1) != NULL);
    REQUIRE(pDialogPanel->GetControl(3) != NULL);
    REQUIRE(pDialogPanel->GetControl(11) != NULL);
    REQUIRE(pDialogPanel->GetControl(0x10000009) != NULL);

    CMessageEnterDialogMode* pMessage = new CMessageEnterDialogMode(1234, pSprite->GetId(), pSprite->GetId());
    pMessage->Run();
    delete pMessage;

    CHECK(pWorld->m_bInControlOfDialog == TRUE);
    CHECK(pWorld->m_bPaused == TRUE);
    CHECK(pGame->GetGameSave()->m_mode == 0x502);
    CHECK(pWorld->m_pActiveDialogDisplay == pDialogPanel->GetControl(1));
    CHECK(pWorld->m_pActiveChatDisplay == pDialogPanel->GetControl(3));
    CHECK(pGame->m_tempCursor == 4);
    CHECK(pGame->GetVisibleArea()->m_nScrollState == 0);
    CHECK(pWorld->m_internalLoadedDialog.field_54 == TRUE);

    // The host branch permits the dialog for the speaker's player.
    CHECK(pGame->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE);
    CHECK(pGame->GetMultiplayerSettings()->m_idHostPermittedDialog == g_pChitin->cNetwork.m_idLocalPlayer);

    // The portrait button is disabled and shows the speaker's small
    // portrait (SetPortrait, 0x64F930).
    CUIControlButtonSinglePlayerPortrait* pPortrait = static_cast<CUIControlButtonSinglePlayerPortrait*>(pDialogPanel->GetControl(11));
    CHECK(pPortrait->m_bEnabled == FALSE);
    CHECK(std::memcmp(pPortrait->m_portraitResRef.GetResRef(),
              pSprite->GetBaseStats()->m_portraitSmall,
              RESREF_SIZE)
        == 0);

    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    pSprite->m_remotePlayerID = 0;

    ClearPartySlot(pSprite);
}
