#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CMultiplayerSettings.h"
#include "CNetwork.h"
#include "CScreenInventory.h"
#include "CScreenWorld.h"
#include "CStore.h"
#include "engine_world.h"
#include "msvc_rand.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The single-player host session: a real single-player game runs as a
// connected host under the null provider (wave2-common.md), so resting,
// bags and area unloading go through the host branches that were dropped
// as "multiplayer only". Every test here opens the session exactly as
// CScreenConnection::OnNewGameButtonClick does.

extern CChitin* g_pChitin;
extern CBaldurChitin* g_pBaldurChitin;

namespace {

// The null-provider host session of a single-player game, closed again
// however the test ends so later tests see no session.
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
// slot becomes a character when creation completes
// (CScreenCreateChar::StopCreateChar signals CREATION_COMPLETE), and only a
// slot marked CHARSTATUS_CHARACTER survives CInfGame::UpdateCharacterSlots
// once a session is open. Without the signal a host session sees an empty
// party: no rest messages, no area for the sprite and no living member to
// vote.
CGameSprite* NewGameInStartAreaHost(std::vector<BYTE>& bytes)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NewGame asserts an empty game (InfGame.cpp:8769). ctest runs every
    // case as its own process, but a tag filter runs them in one, and a
    // case that stopped halfway leaves its character behind.
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

// ctest runs every case as its own process, but a tag filter runs them in
// one, and CInfGame::NewGame asserts the game holds no characters
// (InfGame.cpp:8769). Clear the slot the way the engine clears a leaving
// character (CInfGame::ClearCharacterSlot, 0x5BD530: it compacts
// m_characters and decrements m_nCharacters).
void ClearPartySlot(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const INT nSlot = pGame->GetCharacterSlotFromId(pSprite->GetId());
    if (nSlot != -1) {
        pGame->ClearCharacterSlot(nSlot);
    }
}

BOOL HasQueuedAction(CGameSprite* pSprite, SHORT actionID)
{
    if (pSprite->m_curAction.m_actionID == actionID) {
        return TRUE;
    }
    POSITION pos = pSprite->m_queuedActions.GetHeadPosition();
    while (pos != NULL) {
        if (pSprite->m_queuedActions.GetNext(pos)->m_actionID == actionID) {
            return TRUE;
        }
    }
    return FALSE;
}

void ClearPersonalSlots(CGameSprite* pSprite)
{
    for (INT nSlot = 0; nSlot < CScreenInventory::PERSONAL_INVENTORY_SIZE + 16; nSlot++) {
        CItem* pItem = pSprite->m_equipment.m_items[nSlot];
        if (pItem != NULL) {
            delete pItem;
            pSprite->m_equipment.m_items[nSlot] = NULL;
        }
    }
}

}

TEST_CASE("sp-host: RestParty rests the party and returns TRUE", "[sp-host][rest][data]")
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

    HostSession session;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    g_pBaldurChitin->pActiveEngine = pWorld;

    CGameSprite* pSprite = NewGameInStartAreaHost(bytes);
    REQUIRE(pSprite != NULL);

    const BOOL bListenToJoin = pGame->GetMultiplayerSettings()->GetListenToJoinOption();

    pSprite->m_queuedActions.RemoveAll();
    pSprite->m_pendingTriggers.RemoveAll();
    pSprite->m_baseStats.m_hitPoints = 1;

    CInfGame::m_bHealPartyOnRest = FALSE;

    // The host branch: TRUE, and the rest happens now (the listen-to-join
    // option is untouched under the null provider; the dropped code left it
    // cleared).
    const ULONG before = pGame->m_worldTime.m_gameTime;
    CHECK(pGame->RestParty(7, FALSE) == TRUE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING);
    CHECK(HasQueuedAction(pSprite, 96));

    BOOL bRestTrigger = FALSE;
    POSITION pos = pSprite->m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        if (pSprite->m_pendingTriggers.GetNext(pos)->m_triggerID == 0x93) {
            bRestTrigger = TRUE;
        }
    }
    CHECK(bRestTrigger);
    CHECK(pWorld->m_bRestPending == FALSE);
    CHECK(pGame->GetMultiplayerSettings()->GetListenToJoinOption() == bListenToJoin);

    // A hard pause defers the rest: TRUE, pending, and the clock stands.
    pSprite->m_queuedActions.RemoveAll();
    pWorld->m_bHardPaused = TRUE;
    const ULONG pendingBefore = pGame->m_worldTime.m_gameTime;
    CHECK(pGame->RestParty(7, FALSE) == TRUE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pWorld->m_bRestPending == TRUE);
    CHECK(pWorld->m_nRestHP == 7);
    CHECK(pWorld->m_bRestRenting == FALSE);
    CHECK(pWorld->m_bRestMovie == FALSE);
    CHECK(pGame->m_worldTime.m_gameTime == pendingBefore);
    pWorld->m_bHardPaused = FALSE;
    pWorld->m_bRestPending = FALSE;

    ClearPartySlot(pSprite);
}

TEST_CASE("sp-host: an item dropped on a bag fills the host's server store", "[sp-host][inventory][store][data]")
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

    HostSession session;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameInStartAreaHost(bytes);
    REQUIRE(pSprite != NULL);
    ClearPersonalSlots(pSprite);

    CStore bag(CResRef("00GBGA01"));
    REQUIRE(bag.GetType() == 4);

    // A stack of gems: the session path counts stacks against the
    // capacity, so a stack that does not fit whole is cut to the number of
    // fitting stacks, and the item keeps its original count.
    const WORD nMax = CItem(CResRef("00GEM01"), 0, 0, 0, 0, 0).GetMaxStackable();
    const INT nCapacity = static_cast<WORD>(bag.m_header.field_94);
    const INT nBefore = bag.GetNumItems();
    if (nMax < 2 || nCapacity == 0 || nCapacity <= nBefore) {
        SKIP("[iwd2r-skip] the bag or the gem does not exercise the stack rule");
    }

    CItem* pGem = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    const WORD nUsage = static_cast<WORD>((nCapacity - nBefore + 1) * nMax);
    pGem->SetUsageCount(0, nUsage);

    STRREF strError = 0;
    const INT nCount = pGame->AddItemToBag(CResRef("00GBGA01"), pGem, strError);

    // One more stack than the bag takes: the count is cut to the remaining
    // stack slots (the disconnected path counts items and cuts elsewhere).
    CHECK(strError == static_cast<STRREF>(-1));
    CHECK(nCount == nCapacity - nBefore);
    CHECK(pGem->GetUsageCount(0) == nUsage);
    delete pGem;

    // The server store was released (which marshals it to the temp
    // folder), and a fresh copy of the bag carries the gem.
    for (INT nSlot = 0; nSlot < 12; nSlot++) {
        if (pGame->m_aServerStore[nSlot] != NULL) {
            CHECK(pGame->m_aServerStore[nSlot]->m_resRef != CResRef("00GBGA01"));
        }
    }

    CStore after(CResRef("00GBGA01"));
    CHECK(after.GetNumItems() == nBefore + 1);

    // Dropping the bag itself is still "Wrong Item Type".
    CItem* pOtherBag = new CItem(CResRef("00GBGA02"), 0, 0, 0, 0, 0);
    strError = 0;
    CHECK(pGame->AddItemToBag(CResRef("00GBGA01"), pOtherBag, strError) == 0);
    CHECK(strError == 26285);
    delete pOtherBag;

    ClearPersonalSlots(pSprite);
    ClearPartySlot(pSprite);
}

TEST_CASE("sp-host: RemoveItemFromBag removes one item from the server store", "[sp-host][inventory][store][data]")
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

    HostSession session;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameInStartAreaHost(bytes);
    REQUIRE(pSprite != NULL);

    // Start with a one-line bag so both the returned item and the persisted
    // post-removal store are unambiguous.
    {
        CStore bag(CResRef("00GBGA01"));
        REQUIRE(bag.GetType() == 4);
        while (bag.m_lInventory.GetCount() > 0) {
            delete bag.m_lInventory.RemoveHead();
        }
        CItem gem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
        REQUIRE(bag.AddItemExt(gem, 1) == 0);
        bag.Marshal(pGame->m_sTempDir);
    }

    CItem* pItem = pGame->RemoveItemFromBag(CResRef("00GBGA01"), CResRef("00GEM01"));
    REQUIRE(pItem != NULL);
    CHECK(pItem->GetResRef() == CResRef("00GEM01"));
    CHECK(pItem->GetUsageCount(0) == 1);
    delete pItem;

    {
        CStore bag(CResRef("00GBGA01"));
        CHECK(bag.GetNumItems() == 0);
        CHECK(bag.GetItemIndex(CResRef("00GEM01")) == INT_MAX);
    }
    for (INT nSlot = 0; nSlot < 12; nSlot++) {
        if (pGame->m_aServerStore[nSlot] != NULL) {
            CHECK(pGame->m_aServerStore[nSlot]->m_resRef != CResRef("00GBGA01"));
        }
    }

    // The no-match tail also releases the demanded server store.
    CHECK(pGame->RemoveItemFromBag(CResRef("00GBGA01"), CResRef("NOSUCHIT")) == NULL);
    for (INT nSlot = 0; nSlot < 12; nSlot++) {
        if (pGame->m_aServerStore[nSlot] != NULL) {
            CHECK(pGame->m_aServerStore[nSlot]->m_resRef != CResRef("00GBGA01"));
        }
    }
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    ClearPartySlot(pSprite);
}

TEST_CASE("sp-host: the delete-area poll starts on one rand and deletes the empty area", "[sp-host][data]")
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

    HostSession session;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();

    CGameSprite* pSprite = NewGameInStartAreaHost(bytes);
    REQUIRE(pSprite != NULL);
    CGameArea* pFirst = pSprite->GetArea();
    REQUIRE(pFirst != NULL);
    REQUIRE(pFirst->m_nCharacters != 0);

    // A second area the party has left: the count is 0 once the last party
    // member leaves it (0xFF while the area loads means never visited), and
    // only a left area is a deletion candidate. The master stays with the
    // party.
    CGameArea* pSecond = pGame->LoadArea(CString("AR1001"), 255, FALSE, FALSE);
    REQUIRE(pSecond != NULL);
    pSecond->m_nCharacters = 0;
    pGame->m_pGameAreaMaster = pFirst;

    // The single player casts every vote at once: the frame that starts the
    // poll resolves it too (the original falls through from the start half to
    // the resolve half in the same call, 0x693a4c-0x693a5e), deletes the
    // area and draws exactly one rand, the slot roll.
    iwd2r_srand(1);
    pWorld->DeleteAreaMultiplayer();
    // Exactly one draw: the state after it is 214013*1 + 2531011.
    CHECK(iwd2r_rand_state() == 2745024u);

    CHECK(pSecond->field_AE6 == 0);
    CHECK(pMessage->m_bDeleteAreaPolling == FALSE);
    CHECK(pMessage->GetDeleteAreaString() == CString(""));
    CHECK(pMessage->m_dwDeleteAreaTimeout == 0);
    CHECK(pGame->GetArea(CString("AR1001")) == NULL);
    CHECK(pFirst->m_nCharacters != 0);
    CHECK(pGame->m_pGameAreaMaster == pFirst);

    // With the second area gone there is no candidate: one roll per idle
    // call, and no poll starts. The state has not moved since the pinned
    // draw above, and the idle call advances it by exactly one more draw
    // (2745024*214013 + 2531011).
    CHECK(iwd2r_rand_state() == 2745024u);
    pWorld->DeleteAreaMultiplayer();
    CHECK(iwd2r_rand_state() == 3357800067u);
    CHECK(pMessage->m_bDeleteAreaPolling == FALSE);

    ClearPartySlot(pSprite);
}

TEST_CASE("sp-host: the poll guards and the ballot run on the host", "[sp-host][data]")
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

    HostSession session;

    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();

    CGameSprite* pSprite = NewGameInStartAreaHost(bytes);
    REQUIRE(pSprite != NULL);

    // A poll the host starts itself resolves: the local player's empty
    // vote is the whole ballot, and a living party member is enough.
    REQUIRE(pMessage->StartDeleteAreaPoll(CString("AR1000"), FALSE) == TRUE);
    CHECK(pMessage->GetDeleteAreaString() == CString("AR1000"));
    CHECK(pMessage->CanResolveDeleteAreaPoll() == TRUE);
    CHECK(pMessage->DeleteAreaBallotResult() == TRUE);

    // A dead party cannot resolve the poll.
    const DWORD nState = pSprite->m_baseStats.m_generalState;
    pSprite->m_baseStats.m_generalState |= STATE_DEAD;
    CHECK(pMessage->CanResolveDeleteAreaPoll() == FALSE);
    pSprite->m_baseStats.m_generalState = nState;

    // Cancelling resets the poll and runs on the host.
    REQUIRE(pMessage->CancelDeleteAreaPoll() == TRUE);
    CHECK(pMessage->GetDeleteAreaString() == CString(""));
    CHECK(pMessage->m_bDeleteAreaPolling == FALSE);
    CHECK(pMessage->m_dwDeleteAreaTimeout == 0);

    // A resend keeps the votes cast.
    pMessage->m_pnDeleteAreaVoters[1] = 7;
    pMessage->m_pnDeleteAreaVotes[1] = CBaldurMessage::DELETEAREA_PENDING_VOTE;
    REQUIRE(pMessage->StartDeleteAreaPoll(CString("AR1000"), TRUE) == TRUE);
    CHECK(pMessage->m_pnDeleteAreaVoters[1] == 7);
    CHECK(pMessage->m_pnDeleteAreaVotes[1] == CBaldurMessage::DELETEAREA_PENDING_VOTE);

    // A voter that no longer matches a seat (a player that left) resolves
    // the poll, but its ballot fails, so the area is kept and flagged.
    CHECK(pMessage->CanResolveDeleteAreaPoll() == TRUE);
    CHECK(pMessage->DeleteAreaBallotResult() == FALSE);

    // Resolving resets the ballot.
    REQUIRE(pMessage->ResolveDeleteAreaPoll(CString("AR1000"), TRUE) == TRUE);
    CHECK(pMessage->m_pnDeleteAreaVoters[1] == 0);
    CHECK(pMessage->m_pnDeleteAreaVotes[1] == CBaldurMessage::DELETEAREA_EMPTY_VOTE);
    CHECK(pMessage->m_bDeleteAreaPolling == FALSE);

    ClearPartySlot(pSprite);
}

// CBaldurMessage::ObjectControlRequest (0x430980): the client's "may I control
// this object" request. It needs an established connection; then it shares the
// object, compares its current remote owner with the local player, and either
// changes the control in the host's remote-object array (host) or sends
// {DWORD oldRemotePlayerID; DWORD oldRemoteObjectID; DWORD localPlayerID;
//  DWORD localObjectID} to the host by name (client).
TEST_CASE("sp-host: ObjectControlRequest asks the host for a remote object", "[sp-host][msg][data]")
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

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();
    CNetwork& network = g_pChitin->cNetwork;
    BYTE* pBuffer = CNetwork::STATIC_MESSAGE_BUFFER;

    // The object whose control is requested: not a party member, owned by
    // remote player 2.
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
    pSprite->m_remotePlayerID = 2;
    pSprite->m_remoteObjectID = 0x00012345;
    const LONG nObjectID = pSprite->GetId();

    const BOOLEAN bConnection = network.m_bConnectionEstablished;
    const BOOLEAN bHost = network.m_bIsHost;
    const INT nHostPlayer = network.m_nHostPlayer;
    const DWORD nLocalPlayer = network.m_idLocalPlayer;
    const INT nProvider = network.m_nServiceProvider;

    // No established connection: nothing to request.
    network.m_bConnectionEstablished = FALSE;
    memset(pBuffer, 0xCD, 3 + 4 * sizeof(DWORD));
    CHECK(pMessage->ObjectControlRequest(nObjectID) == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // An object the array cannot share: FALSE.
    network.m_bConnectionEstablished = TRUE;
    CHECK(pMessage->ObjectControlRequest(99999) == FALSE);

    // Already controlled by the local player: FALSE and nothing queued.
    network.m_idLocalPlayer = 7;
    pSprite->m_remotePlayerID = 7;
    CHECK(pMessage->ObjectControlRequest(nObjectID) == FALSE);
    CHECK(pGame->GetRemoteObjectArray()->m_nControlsChanged == 0);

    // The host moves the control itself and answers TRUE.
    network.m_bIsHost = TRUE;
    pSprite->m_remotePlayerID = 2;
    CHECK(pMessage->ObjectControlRequest(nObjectID) == TRUE);

    // A client with no host name stops at the name guard.
    network.m_bIsHost = FALSE;
    network.m_nHostPlayer = -1;
    memset(pBuffer, 0xCD, 3 + 4 * sizeof(DWORD));
    CHECK(pMessage->ObjectControlRequest(nObjectID) == FALSE);
    CHECK(pBuffer[0] == 0xCD);

    // A client with the host named sends the four-DWORD request. A non-null
    // provider is what makes SendSpecificMessage marshal it (the null provider
    // returns before the buffer is touched).
    network.m_nHostPlayer = 0;
    network.m_psPlayerName[0] = "host";
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    memset(pBuffer, 0xCD, 3 + 4 * sizeof(DWORD));
    CHECK(pMessage->ObjectControlRequest(nObjectID) == TRUE);
    CHECK(pBuffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(pBuffer[1] == 0x4F);
    CHECK(pBuffer[2] == 99);
    CHECK(*reinterpret_cast<DWORD*>(pBuffer + 3) == 2);
    CHECK(*reinterpret_cast<DWORD*>(pBuffer + 7) == 0x00012345);
    CHECK(*reinterpret_cast<DWORD*>(pBuffer + 11) == 7);
    CHECK(*reinterpret_cast<DWORD*>(pBuffer + 15) == static_cast<DWORD>(nObjectID));
    CHECK(CBaldurMessage::MSG_TYPE_OBJECT == 0x4F);
    CHECK(CBaldurMessage::MSG_SUBTYPE_OBJECT_CONTROL_REQUEST == 99);

    network.m_bConnectionEstablished = bConnection;
    network.m_bIsHost = bHost;
    network.m_nHostPlayer = nHostPlayer;
    network.m_idLocalPlayer = nLocalPlayer;
    network.m_nServiceProvider = nProvider;
}
