#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameJournal.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMemINI.h"
#include "CMessage.h"
#include "CRuleTables.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenSpellbook.h"
#include "CScreenStore.h"
#include "CScreenWorld.h"
#include "CScreenWorldMap.h"
#include "CSpawn.h"
#include "CStore.h"
#include "CUIPanel.h"
#include "CWorldMap.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The store, inventory and world map screens: CWorldMap::FindPath
// (0x559B40), CScreenWorldMap::GetTravelTime (0x69B7F0) and MoveToArea
// (0x69D0F0), CScreenStore::CheckSpellUsable (0x67E560),
// IsBuyItemButtonClickable (0x677E60) and BuyStoreItems (0x678110) on a new
// game's party in AR1000.

namespace {

namespace fs = std::filesystem;

fs::path g_view;

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

// The engine as iwd2r_main builds it, on a private view of the install
// that lives until the process ends (as tests/test_cre.cpp does). Built
// once and leaked. CBaldurChitin's constructor starts the search thread.
bool StartEngine(std::string& error)
{
    static bool started = false;
    static std::string startError;
    static bool tried = false;
    if (tried) {
        error = startError;
        return started;
    }
    tried = true;

#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
    static std::unique_ptr<DataView> view;
    const std::string original = DataDir();
    view = std::make_unique<DataView>(original);
    g_view = view->Path();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", view->Path().c_str());
#else
    setenv("IWD2_DATA", view->Path().c_str(), 1);
#endif
    const bool found = platform::ResolveDataDir();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", original.c_str());
#else
    setenv("IWD2_DATA", original.c_str(), 1);
#endif
    if (!found) {
        startError = "data folder not found";
        return false;
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        return false;
    }
    chitin->Init(NULL);
    POSITION pos = chitin->lEngines.GetHeadPosition();
    while (pos != NULL) {
        CWarp* pEngine = static_cast<CWarp*>(chitin->lEngines.GetNext(pos));
        if (pEngine != NULL) {
            pEngine->EngineInitialized();
        }
    }
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

// Stands in for the main loop the tests do not run: the engine's waits for
// a drawn frame return as soon as the flag is raised.
class FramePump {
public:
    FramePump()
        : m_running(true)
        , m_thread([this] {
            while (m_running) {
                g_pChitin->m_bDisplayStale = FALSE;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        })
    {
    }
    ~FramePump()
    {
        m_running = false;
        m_thread.join();
    }
    FramePump(const FramePump&) = delete;
    FramePump& operator=(const FramePump&) = delete;

private:
    std::atomic<bool> m_running;
    std::thread m_thread;
};

std::string Lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

// The CRE of a finished character: the first "CHR V2.2" file in the view's
// Characters folder (the install ships premade characters), as
// tests/test_gam.cpp takes it. Empty when there is none.
std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    fs::path characters = g_view / "Characters";
    for (const auto& entry : fs::directory_iterator(g_view, ec)) {
        if (Lower(entry.path().filename().string()) == "characters")
            characters = entry.path();
    }
    for (const auto& entry : fs::directory_iterator(characters, ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

CGameSprite* MakeSprite(std::vector<BYTE>& bytes)
{
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
    pSprite->m_bGlobal = TRUE;
    return pSprite;
}

// A new game whose party (copies of a finished character) stands in the
// first area; empty when the install has no finished character.
std::vector<CGameSprite*> NewGameWithParty(int count)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    std::vector<BYTE> bytes = FinishedCharacter();
    std::vector<CGameSprite*> party;
    if (bytes.empty()) {
        return party;
    }
    for (int n = 0; n < count; n++) {
        CGameSprite* pSprite = MakeSprite(bytes);
        pGame->SetCharacterSlot(n, pSprite->GetId());
        party.push_back(pSprite);
    }
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    return party;
}

// The world map of the visible area and its map index, with every area
// made known and explorable for the test (the flags are put back by the
// guard).
class ExplorableGuard {
public:
    ExplorableGuard(CWorldMap* pWorldMap, DWORD nMap)
        : m_pWorldMap(pWorldMap)
        , m_nMap(nMap)
    {
        for (DWORD nArea = 0; nArea < pWorldMap->GetNumAreas(nMap); nArea++) {
            m_flags.push_back(pWorldMap->GetArea(nMap, nArea)->m_dwFlags);
            pWorldMap->GetArea(nMap, nArea)->m_dwFlags |= 0x1 | 0x4;
        }
    }
    ~ExplorableGuard()
    {
        for (DWORD nArea = 0; nArea < m_flags.size(); nArea++) {
            m_pWorldMap->GetArea(m_nMap, nArea)->m_dwFlags = m_flags[nArea];
        }
    }
    ExplorableGuard(const ExplorableGuard&) = delete;
    ExplorableGuard& operator=(const ExplorableGuard&) = delete;

private:
    CWorldMap* m_pWorldMap;
    DWORD m_nMap;
    std::vector<DWORD> m_flags;
};

CWorldMap* VisibleWorldMap(DWORD& nMap)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CString sArea;
    pGame->GetVisibleArea()->m_resRef.CopyToString(sArea);
    CWorldMap* pWorldMap = pGame->GetWorldMap(sArea);
    nMap = pWorldMap->sub_55A3A0();
    return pWorldMap;
}

// The cost of the cheapest chain of links between two areas, by plain
// relaxation over all links (independent of the engine's search).
DWORD CheapestDistance(CWorldMap* pWorldMap, DWORD nMap, DWORD nSrc, DWORD nDst)
{
    const DWORD nAreas = pWorldMap->GetNumAreas(nMap);
    std::vector<DWORD> best(nAreas, 0xFFFFFFFF);
    best[nSrc] = 0;
    for (DWORD nPass = 0; nPass < nAreas; nPass++) {
        for (DWORD nArea = 0; nArea < nAreas; nArea++) {
            if (best[nArea] == 0xFFFFFFFF) {
                continue;
            }
            CWorldMapList* pLinks = pWorldMap->GetAllLinks(nMap, nArea);
            POSITION pos = pLinks->GetHeadPosition();
            while (pos != NULL) {
                CWorldMapLinks* pLink = pWorldMap->GetLink(nMap, pLinks->GetNext(pos));
                if (best[nArea] + pLink->m_nDistanceScale < best[pLink->m_nArea]) {
                    best[pLink->m_nArea] = best[nArea] + pLink->m_nDistanceScale;
                }
            }
            delete pLinks;
        }
    }
    return best[nDst];
}

// The first area with a link out, and that link.
BOOL FirstLinkedArea(CWorldMap* pWorldMap, DWORD nMap, DWORD& nSrc, CWorldMapLinks*& pLink)
{
    for (nSrc = 0; nSrc < pWorldMap->GetNumAreas(nMap); nSrc++) {
        CWorldMapList* pLinks = pWorldMap->GetAllLinks(nMap, nSrc);
        if (pLinks->GetCount() > 0) {
            pLink = pWorldMap->GetLink(nMap, pLinks->GetHead());
            delete pLinks;
            return TRUE;
        }
        delete pLinks;
    }
    return FALSE;
}

}

TEST_CASE("worldmap: FindPath chains the links from the source to the destination at the least cost", "[worldmap][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    DWORD nMap;
    CWorldMap* pWorldMap = VisibleWorldMap(nMap);
    ExplorableGuard guard(pWorldMap, nMap);

    DWORD nSrc;
    CWorldMapLinks* pFirstLink;
    REQUIRE(FirstLinkedArea(pWorldMap, nMap, nSrc, pFirstLink));

    // Every reachable area: the links follow on from each other, start at
    // the source, end at the destination and cost the least.
    INT nChecked = 0;
    for (DWORD nDst = 0; nDst < pWorldMap->GetNumAreas(nMap); nDst++) {
        if (nDst == nSrc) {
            continue;
        }
        const DWORD nCheapest = CheapestDistance(pWorldMap, nMap, nSrc, nDst);
        CWorldMapList* pPath = pWorldMap->FindPath(nMap, nSrc, nDst);
        if (nCheapest == 0xFFFFFFFF) {
            CHECK(pPath == NULL);
            delete pPath;
            continue;
        }
        REQUIRE(pPath != NULL);
        REQUIRE(pPath->GetCount() > 0);

        DWORD nArea = nSrc;
        DWORD nDistance = 0;
        POSITION pos = pPath->GetHeadPosition();
        while (pos != NULL) {
            DWORD nLink = pPath->GetNext(pos);
            CHECK(pWorldMap->FindSourceAreaIndex(nMap, nLink) == nArea);
            CWorldMapLinks* pLink = pWorldMap->GetLink(nMap, nLink);
            nDistance += pLink->m_nDistanceScale;
            nArea = pLink->m_nArea;
        }
        CHECK(nArea == nDst);
        CHECK(nDistance == nCheapest);
        delete pPath;
        nChecked++;
    }
    CHECK(nChecked > 0);

    // To itself: an empty chain.
    CWorldMapList* pPath = pWorldMap->FindPath(nMap, nSrc, nSrc);
    REQUIRE(pPath != NULL);
    CHECK(pPath->GetCount() == 0);
    delete pPath;
}

TEST_CASE("worldmap: GetTravelTime is four hours per unit of distance along the path", "[worldmap][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    DWORD nMap;
    CWorldMap* pWorldMap = VisibleWorldMap(nMap);
    ExplorableGuard guard(pWorldMap, nMap);

    DWORD nSrc;
    CWorldMapLinks* pLink;
    REQUIRE(FirstLinkedArea(pWorldMap, nMap, nSrc, pLink));

    CScreenWorldMap* pScreen = g_pBaldurChitin->m_pEngineWorldMap;
    pScreen->m_cResCurrentArea = pWorldMap->GetArea(nMap, nSrc)->m_resCurrentArea;
    pScreen->m_nCurrentLink = static_cast<DWORD>(-1);

    const DWORD nDst = pLink->m_nArea;
    const DWORD nCheapest = CheapestDistance(pWorldMap, nMap, nSrc, nDst);
    CHECK(pScreen->GetTravelTime(nDst) == nCheapest * CRuleTables::TIME_TRAVELLING);
    CHECK(pScreen->m_pPath == NULL);

    // Standing at the area: no time.
    CHECK(pScreen->GetTravelTime(nSrc) == 0);
}

TEST_CASE("worldmap: MoveToArea takes the party into the area and passes the travel time with its fatigue", "[worldmap][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(2);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pOldArea = party[0]->GetArea();
    REQUIRE(pOldArea != NULL);

    DWORD nMap;
    CWorldMap* pWorldMap = VisibleWorldMap(nMap);
    DWORD nSrc;
    CWorldMapLinks* pLink;
    REQUIRE(FirstLinkedArea(pWorldMap, nMap, nSrc, pLink));
    (void)pLink;
    // AR1001, the Targos palisade next door (the first linked area, AR3000,
    // stops in CInfGame::LoadArea in the test build).
    const CResRef cResDest("AR1001");
    INFO(cResDest.GetResRefStr());
    REQUIRE(cResDest != pOldArea->m_resRef);

    // Selecting a member in the new area activates it, which sets the
    // channel volumes: the mixer CChitin::InitializeServices (0x790240)
    // starts with the video.
    g_pChitin->cSoundMixer.Initialize(&g_pChitin->cWnd, 16, g_pChitin->GetNumberSoundChannels());
    g_pBaldurChitin->pActiveEngine = g_pBaldurChitin->m_pEngineWorld;

    // One member rested long ago, the other just now.
    const DWORD nTravelTime = 2 * CTimerWorld::TIMESCALE_MSEC_PER_HOUR;
    const ULONG before = pGame->m_worldTime.m_gameTime;
    party[0]->field_722A = 0;
    party[1]->field_722A = before;
    const LONG nCap = CScreenWorldMap::FATIGUE_TIME_CAP;

    g_pBaldurChitin->m_pEngineWorldMap->MoveToArea(CString(""), cResDest, 0, FALSE, nTravelTime);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CGameArea* pNewArea = party[0]->GetArea();
    REQUIRE(pNewArea != NULL);
    CHECK(pNewArea->m_resRef == cResDest);
    CHECK(party[1]->GetArea() == pNewArea);
    CHECK(pGame->m_bInAreaTransition == FALSE);

    // The area left behind was written to the temp folder once.
    CHECK(pOldArea->field_436 == 1);

    // From the north edge: the leader stands near the top, in the middle.
    CHECK(party[0]->m_pos.y < pNewArea->GetInfinity()->nAreaY / 2);

    // The trip's time, less the tick the new area's activation takes off
    // when it stops the clock (CTimerWorld::StopTime 0x54F9F0).
    CHECK(pGame->m_worldTime.m_gameTime - before == nTravelTime - 1);

    // The fatigue clock moves by what the trip adds past a day without
    // rest, at most the trip.
    LONG nExpected0 = static_cast<LONG>(before - 0 - nCap + nTravelTime);
    nExpected0 = std::max<LONG>(0, std::min<LONG>(nExpected0, nTravelTime));
    CHECK(party[0]->field_722A == static_cast<ULONG>(nExpected0));
    CHECK(party[1]->field_722A == before);
}

TEST_CASE("store: CheckSpellUsable greys the temple spells by the customer's state", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CScreenStore* pStore = g_pBaldurChitin->m_pEngineStore;
    CGameSprite* pSprite = party[0];
    const DWORD state = pSprite->m_derivedStats.m_generalState;
    const BYTE race = pSprite->m_liveTypeAI.m_nRace;

    // Alive: healing yes, raising no, cure poison only when poisoned.
    pSprite->m_derivedStats.m_generalState = 0;
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR101")) == TRUE);
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR504")) == 0);
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR404")) == FALSE);
    pSprite->m_derivedStats.m_generalState = 0x4000;
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR404")) == TRUE);
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR212")) == TRUE);

    // Dead: the raises give back the dead bit itself (0x800), the rest no.
    pSprite->m_derivedStats.m_generalState = STATE_DEAD;
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR101")) == FALSE);
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR712")) == STATE_DEAD);
    pSprite->m_liveTypeAI.m_nRace = 1;
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR504")) == STATE_DEAD);

    // Raise Dead does not work on elves.
    pSprite->m_liveTypeAI.m_nRace = 2;
    CHECK(pStore->CheckSpellUsable(0, CResRef("SPPR504")) == 0);

    // Nobody in the slot.
    CHECK(pStore->CheckSpellUsable(5, CResRef("SPPR101")) == FALSE);

    pSprite->m_liveTypeAI.m_nRace = race;
    pSprite->m_derivedStats.m_generalState = state;
}

TEST_CASE("store: buying takes the gold and puts the item in a free slot", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(2);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenStore* pStore = g_pBaldurChitin->m_pEngineStore;

    pStore->StartStore(party[1]->GetAIType(), party[0]->GetAIType(), CResRef("11OSWALD"));
    REQUIRE(pStore->m_pStore != NULL);
    REQUIRE(pStore->m_pBag == NULL);
    pStore->SwitchMainPanel(2);
    pStore->UpdateStoreItems();
    REQUIRE(pStore->m_lStoreItems.GetCount() > 0);

    // Nothing selected: the button stays off.
    CHECK(pStore->IsBuyItemButtonClickable() == FALSE);

    // A click on the first line's button (control 5 of the buy and sell
    // panel) selects it and adds its price; a second click drops it.
    CUIControlButtonStoreStoreItem* pFirst = static_cast<CUIControlButtonStoreStoreItem*>(pStore->m_pMainPanel->GetControl(5));
    REQUIRE(pFirst != NULL);
    pStore->SetTopStoreItem(0);
    pFirst->OnLButtonClick(CPoint(0, 0));
    CHECK(pStore->m_lStoreItems.GetHead()->m_bSelected == TRUE);
    CHECK(pStore->m_nStoreCost == pStore->m_lStoreItems.GetHead()->m_nValue);
    pFirst->OnLButtonClick(CPoint(0, 0));
    CHECK(pStore->m_lStoreItems.GetHead()->m_bSelected == FALSE);
    CHECK(pStore->m_nStoreCost == 0);

    // The first item that is not a random one, bought once.
    CScreenStoreItem* pStoreItem = NULL;
    POSITION pos = pStore->m_lStoreItems.GetHeadPosition();
    while (pos != NULL) {
        CScreenStoreItem* pItem = pStore->m_lStoreItems.GetNext(pos);
        if (pItem->m_pItem != NULL && pItem->m_pItem->cResRef != "RNDTRES") {
            pStoreItem = pItem;
            break;
        }
    }
    REQUIRE(pStoreItem != NULL);
    const CResRef cResBought = pStoreItem->m_pItem->cResRef;

    pStoreItem->m_bSelected = TRUE;
    pStoreItem->m_nCount = 1;
    pStore->UpdateStoreCost();
    CHECK(pStore->IsBuyItemButtonClickable() == TRUE);

    // Not enough gold: the error popup, nothing bought.
    pGame->GetGameSave()->m_nPartyGold = 0;
    if (pStore->m_nStoreCost > 0) {
        pStore->BuyStoreItems();
        CHECK(pStore->m_nErrorState == 2);
        CHECK(pStore->m_strErrorText == 11047);
        CHECK(pGame->GetGameSave()->m_nPartyGold == 0);
        pStore->DismissPopup();
    }

    pGame->GetGameSave()->m_nPartyGold = 100000;
    const LONG nCost = pStore->m_nStoreCost;
    const UINT nUsedBefore = party[0]->m_equipment.GetUsedSlotsCount();

    pStore->BuyStoreItems();

    CHECK(pGame->GetGameSave()->m_nPartyGold == static_cast<DWORD>(100000 - nCost));
    CHECK(party[0]->m_equipment.GetUsedSlotsCount() == nUsedBefore + 1);

    BOOL bFound = FALSE;
    for (INT nSlot = 0; nSlot < CGameSpriteEquipment::NUM_SLOT; nSlot++) {
        CItem* pItem = party[0]->m_equipment.m_items[nSlot];
        if (pItem != NULL && pItem->cResRef == cResBought) {
            bFound = TRUE;
        }
    }
    CHECK(bFound);

    // The selection is cleared and the cost with it.
    CHECK(pStore->m_nStoreCost == 0);
    CHECK(pStore->IsBuyItemButtonClickable() == FALSE);

    pStore->DeleteStore();
}

namespace {

// The inventory screen's panels, loaded as a new game's engines load them.
CScreenInventory* InventoryWithPanels()
{
    CScreenInventory* pInventory = g_pBaldurChitin->m_pEngineInventory;
    if (pInventory->m_cUIManager.GetPanel(2) == NULL) {
        pInventory->EngineGameInit();
    }
    return pInventory;
}

// The inventory slot button with this id, on whichever panel holds it.
CUIControlButtonInventorySlot* InventorySlot(CScreenInventory* pInventory, DWORD nId)
{
    for (DWORD nPanel = 0; nPanel < 32; nPanel++) {
        CUIPanel* pPanel = pInventory->m_cUIManager.GetPanel(nPanel);
        if (pPanel != NULL) {
            CUIControlButtonInventorySlot* pSlot = dynamic_cast<CUIControlButtonInventorySlot*>(pPanel->GetControl(nId));
            if (pSlot != NULL) {
                return pSlot;
            }
        }
    }
    return NULL;
}

// Empties an equipment slot for a test, giving back what it held.
CItem* TakeSlot(CGameSprite* pSprite, INT nSlot)
{
    CItem* pItem = pSprite->m_equipment.m_items[nSlot];
    pSprite->m_equipment.m_items[nSlot] = NULL;
    return pItem;
}

}

TEST_CASE("inventory: OnKeyDown runs the key map and its entry 5 opens the spellbook (left panel button 4)", "[inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenInventory* pInventory = InventoryWithPanels();
    g_pChitin->cSoundMixer.Initialize(&g_pChitin->cWnd, 16, g_pChitin->GetNumberSoundChannels());
    g_pBaldurChitin->pActiveEngine = pInventory;

    // A key no entry uses: the loop over the key map ends (it never advanced
    // before) and nothing happens.
    for (SHORT index = 0; index < CINFGAME_KEYMAP_SIZE; index++) {
        if (pGame->m_pKeymap[index] == 'Q') {
            pGame->m_pKeymap[index] = 0;
        }
    }
    pInventory->m_bCtrlKeyDown = FALSE;
    pInventory->m_pVirtualKeysFlags[0] = 'Q';
    pInventory->OnKeyDown(1);
    CHECK(g_pBaldurChitin->pActiveEngine == pInventory);

    // Entry 5 bound to the key: left panel button 4, the spellbook.
    pGame->m_pKeymap[5] = 'Q';
    pGame->m_pKeymapFlags[5] = FALSE;
    pInventory->OnKeyDown(1);
    CHECK(g_pBaldurChitin->pActiveEngine == g_pBaldurChitin->m_pEngineSpellbook);
}

TEST_CASE("inventory: SwapItemPersonal fits helmets and rings and quiver items only in their slots", "[inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = party[0];
    STRREF strError;

    CItem* pHelm = new CItem(CResRef("00HELM01"), 0, 0, 0, 0, 0);
    CItem* pRing = new CItem(CResRef("00RING01"), 0, 0, 0, 0, 0);
    CItem* pBolts = new CItem(CResRef("00BOLT01"), 10, 0, 0, 0, 0);
    REQUIRE(pHelm->GetItemType() == 7);
    REQUIRE(pRing->GetItemType() == 10);
    REQUIRE(pBolts->GetItemType() == 31);

    CItem* pOldHelm = TakeSlot(pSprite, 6);
    CItem* pOldRing = TakeSlot(pSprite, 7);
    CItem* pOldQuiver = TakeSlot(pSprite, 11);

    // The wrong slot: "That item cannot be put there" (9375), the hand
    // keeps the item.
    CItem* pHand = pRing;
    CHECK(pGame->SwapItemPersonal(0, 6, pHand, strError, 1, FALSE) == FALSE);
    CHECK(strError == 9375);
    CHECK(pHand == pRing);
    pHand = pHelm;
    CHECK(pGame->SwapItemPersonal(0, 7, pHand, strError, 1, FALSE) == FALSE);
    CHECK(strError == 9375);
    pHand = pHelm;
    CHECK(pGame->SwapItemPersonal(0, 11, pHand, strError, 1, FALSE) == FALSE);
    CHECK(strError == 9375);
    pHand = pBolts;
    CHECK(pGame->SwapItemPersonal(0, 6, pHand, strError, 1, FALSE) == FALSE);

    // The right slot: the item goes in and the hand empties.
    pHand = pHelm;
    CHECK(pGame->SwapItemPersonal(0, 6, pHand, strError, 1, FALSE) == TRUE);
    CHECK(pHand == NULL);
    CHECK(pSprite->m_equipment.m_items[6] != NULL);
    pHand = pRing;
    CHECK(pGame->SwapItemPersonal(0, 7, pHand, strError, 1, FALSE) == TRUE);
    CHECK(pHand == NULL);
    pHand = pBolts;
    CHECK(pGame->SwapItemPersonal(0, 11, pHand, strError, 10, FALSE) == TRUE);
    CHECK(pSprite->m_equipment.m_items[11] != NULL);

    // The slot predicate the highlights use agrees.
    CItem* pProbe = new CItem(CResRef("00HELM01"), 0, 0, 0, 0, 0);
    CHECK(pGame->CanEquipInSlot(pSprite, 6, &pProbe, &strError) == TRUE);
    CHECK(pGame->CanEquipInSlot(pSprite, 7, &pProbe, &strError) == FALSE);
    CHECK(strError == 9375);
    CHECK(pGame->CanPortraitEquipInSlot(0, 11, &pProbe, &strError) == FALSE);
    CHECK(pGame->CanEquipInSlot(pSprite, 10, &pProbe, &strError) == FALSE);
    CHECK(pGame->CanEquipInSlot(pSprite, 51, &pProbe, &strError) == FALSE);
    CHECK(strError == 9374);
    delete pProbe;

    (void)pOldHelm;
    (void)pOldRing;
    (void)pOldQuiver;
}

TEST_CASE("inventory: FindEmptyItemPersonalInventory gives the first free pack slot or -1", "[inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = party[0];

    CItem* saved[24];
    for (INT n = 0; n < 24; n++) {
        saved[n] = TakeSlot(pSprite, 18 + n);
    }
    CHECK(pGame->FindEmptyItemPersonalInventory(0) == 18);

    CItem gem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    pSprite->m_equipment.m_items[18] = &gem;
    pSprite->m_equipment.m_items[19] = &gem;
    CHECK(pGame->FindEmptyItemPersonalInventory(0) == 20);

    for (INT n = 0; n < 24; n++) {
        pSprite->m_equipment.m_items[18 + n] = &gem;
    }
    CHECK(pGame->FindEmptyItemPersonalInventory(0) == -1);

    // No character in the slot.
    CHECK(pGame->FindEmptyItemPersonalInventory(5) == -1);

    for (INT n = 0; n < 24; n++) {
        pSprite->m_equipment.m_items[18 + n] = saved[n];
    }
}

TEST_CASE("inventory: a ring slot lights where the held ring can go and stays dark for a helmet", "[inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CScreenInventory* pInventory = InventoryWithPanels();
    pInventory->m_nSelectedCharacter = 0;

    // Button 22 is the first ring slot (equipment slot 7).
    CUIControlButtonInventorySlot* pSlot = InventorySlot(pInventory, 22);
    REQUIRE(pSlot != NULL);
    REQUIRE(pInventory->MapButtonIdToInventoryId(22) == 7);
    CItem* pOldRing = TakeSlot(party[0], 7);

    CItem* pRing = new CItem(CResRef("00RING01"), 0, 0, 0, 0, 0);
    CItem* pHelm = new CItem(CResRef("00HELM01"), 0, 0, 0, 0, 0);
    CItem* pOldTemp = pInventory->m_pTempItem;

    // Under the mouse: frame 2; elsewhere: frame 1.
    pInventory->m_pTempItem = pRing;
    pSlot->UpdateHighlight(pSlot->m_ptOrigin.x + 1, pSlot->m_ptOrigin.y + 1);
    CHECK(pSlot->m_cVidCell.m_nCurrentFrame == 2);
    pSlot->UpdateHighlight(pSlot->m_ptOrigin.x - 1, pSlot->m_ptOrigin.y - 1);
    CHECK(pSlot->m_cVidCell.m_nCurrentFrame == 1);

    // A helmet cannot go there: frame 0.
    pInventory->m_pTempItem = pHelm;
    pSlot->UpdateHighlight(pSlot->m_ptOrigin.x + 1, pSlot->m_ptOrigin.y + 1);
    CHECK(pSlot->m_cVidCell.m_nCurrentFrame == 0);

    // Nothing held: frame 0.
    pInventory->m_pTempItem = pRing;
    pSlot->UpdateHighlight(pSlot->m_ptOrigin.x + 1, pSlot->m_ptOrigin.y + 1);
    pInventory->m_pTempItem = NULL;
    pSlot->UpdateHighlight(pSlot->m_ptOrigin.x + 1, pSlot->m_ptOrigin.y + 1);
    CHECK(pSlot->m_cVidCell.m_nCurrentFrame == 0);

    pInventory->m_pTempItem = pOldTemp;
    party[0]->m_equipment.m_items[7] = pOldRing;
    delete pRing;
    delete pHelm;
}

TEST_CASE("spawn: ParseData counts every section including the last", "[spawn]")
{
    CMemINI ini;
    CStringArray sections;

    CSpawnFile file(&ini);
    CHECK(file.ParseData(CString("a,b,c"), sections) == 3);
    REQUIRE(sections.GetCount() == 3);
    CHECK(sections[0] == "a");
    CHECK(sections[2] == "c");

    CSpawnList list(&ini, CString("none"));
    sections.RemoveAll();
    CHECK(list.ParseData(CString(" a, b ,c "), sections) == 3);
    REQUIRE(sections.GetCount() == 3);
    CHECK(sections[1] == "b");
    CHECK(sections[2] == "c");

    // An empty piece between commas is skipped and not counted.
    sections.RemoveAll();
    CHECK(list.ParseData(CString("a,,b"), sections) == 2);
    CHECK(sections.GetCount() == 2);
}
