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
#include "CMessage.h"
#include "CRuleTables.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenStore.h"
#include "CScreenWorld.h"
#include "CStore.h"
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

// Resting: CInfGame::CanRestParty (0x5C1E30), RestParty (0x5C1160) and
// RestPartyPrivate (0x5C1350) on a new game's party in AR1000. Stores:
// CStore::SetResRef (0x54B2A0) and Marshal (0x54B9A0).

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

// Sets CInfGame::m_bHealPartyOnRest for a test and puts it back however the
// test ends.
class HealOnRestGuard {
public:
    explicit HealOnRestGuard(BOOL value)
        : m_saved(CInfGame::m_bHealPartyOnRest)
    {
        CInfGame::m_bHealPartyOnRest = value;
    }
    ~HealOnRestGuard()
    {
        CInfGame::m_bHealPartyOnRest = m_saved;
    }
    HealOnRestGuard(const HealOnRestGuard&) = delete;
    HealOnRestGuard& operator=(const HealOnRestGuard&) = delete;

private:
    BOOL m_saved;
};

// How many objects the area's lists hold, the waiting-to-be-added ones
// included (as tests/test_gam.cpp counts them).
int ObjectsInArea(CGameArea* pArea)
{
    int count = 0;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight,
        &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        count += static_cast<int>(list->GetCount());
    }
    return count;
}

// Drops the messages waiting in the global handler, so a test can count the
// ones its own call adds (the pattern tests/test_ai.cpp uses).
void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
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

}

TEST_CASE("rest: CanRestParty asks to rest a gathered party and refuses a scattered one or a no-rest area", "[rest][data]")
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
    CGameArea* pArea = party[0]->GetArea();
    REQUIRE(pArea != NULL);
    REQUIRE(party[1]->GetArea() == pArea);

    STRREF strError = static_cast<STRREF>(-1);
    // AR1000's header forbids resting (flag 0x2): "You may not rest at
    // this time."
    const DWORD flags = pArea->m_header.m_flags;
    REQUIRE((flags & 0x2) != 0);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    pArea->m_header.m_flags = flags & ~0x2;

    // Both at their start points: the game asks "Are you sure you wish to
    // rest?".
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);
    CHECK(strError == 15358);

    // The second member out of range (the 0x40000 of the squared distance,
    // the vertical part weighted 16/9): "You may not rest when your party
    // is scattered."
    const CPoint home = party[1]->m_pos;
    party[1]->m_pos.x = party[0]->m_pos.x + 600;
    party[1]->m_pos.y = party[0]->m_pos.y;
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 16457);

    // Just inside: 511^2 + 0 <= 0x40000.
    party[1]->m_pos.x = party[0]->m_pos.x + 511;
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);

    // Vertically 384 is in range (384^2 * 16 / 9 == 0x40000), 385 is not.
    party[1]->m_pos.x = party[0]->m_pos.x;
    party[1]->m_pos.y = party[0]->m_pos.y + 384;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);
    party[1]->m_pos.y = party[0]->m_pos.y + 385;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 16457);

    // The third argument answers TRUE for any gathered party, before the
    // save checks; a scattered one goes on to them.
    party[1]->m_pos = home;
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, TRUE, FALSE) == TRUE);
    CHECK(strError == 10309);

    // The no-rest flag is ignored when the party rents a room (the second
    // argument) or with the fourth argument.
    pArea->m_header.m_flags |= 0x2;
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    CHECK(pGame->CanRestParty(strError, TRUE, FALSE, FALSE) == TRUE);
    CHECK(strError == 15358);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, TRUE) == TRUE);
    pArea->m_header.m_flags = flags & ~0x2;

    // Without the leader permission: the multiplayer refusal.
    pGame->m_singlePlayerPermissions.m_bPermissions[CGamePermission::LEADER] = FALSE;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 17198);
    pGame->m_singlePlayerPermissions.m_bPermissions[CGamePermission::LEADER] = TRUE;

    // A party with nobody alive: nothing but the default refusal.
    const DWORD state0 = party[0]->m_derivedStats.m_generalState;
    const DWORD state1 = party[1]->m_derivedStats.m_generalState;
    party[0]->m_derivedStats.m_generalState |= STATE_DEAD;
    party[1]->m_derivedStats.m_generalState |= STATE_DEAD;
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    party[0]->m_derivedStats.m_generalState = state0;
    party[1]->m_derivedStats.m_generalState = state1;
    pArea->m_header.m_flags = flags;
}

TEST_CASE("rest: CanRestParty refuses while a party member has Mold Touch or Shroud of Flame", "[rest][data]")
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
    CGameArea* pArea = party[0]->GetArea();
    REQUIRE(pArea != NULL);

    // AR1000's header forbids resting; take that out so that only the spell
    // states can refuse it.
    const DWORD flags = pArea->m_header.m_flags;
    pArea->m_header.m_flags = flags & ~0x2;

    STRREF strError = static_cast<STRREF>(-1);
    REQUIRE(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);
    REQUIRE(strError == 15358);

    // CGameObjectArray 0x5C9300: SPLSTATE_MOLD_TOUCH on any sprite refuses
    // the rest: "You may not rest at this time."
    party[0]->m_derivedStats.m_spellStates.set(SPLSTATE_MOLD_TOUCH, true);
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    party[0]->m_derivedStats.m_spellStates.set(SPLSTATE_MOLD_TOUCH, false);

    // The same for SPLSTATE_SHROUD_OF_FLAME.
    party[0]->m_derivedStats.m_spellStates.set(SPLSTATE_SHROUD_OF_FLAME, true);
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    party[0]->m_derivedStats.m_spellStates.set(SPLSTATE_SHROUD_OF_FLAME, false);

    // Cleared: the normal answer.
    strError = static_cast<STRREF>(-1);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);
    CHECK(strError == 15358);

    pArea->m_header.m_flags = flags;
}

TEST_CASE("rest: RestParty passes the rest time and queues the heal, the Rest action and the rest trigger", "[rest][data]")
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
    g_pBaldurChitin->pActiveEngine = g_pBaldurChitin->m_pEngineWorld;

    // AR1000's own rest encounter (activated, a 10% chance an hour) would
    // interrupt the rest this test measures.
    party[0]->GetArea()->m_headerRestEncounter.m_activated = 0;

    HealOnRestGuard guard(FALSE);

    for (CGameSprite* pSprite : party) {
        pSprite->m_queuedActions.RemoveAll();
        pSprite->m_pendingTriggers.RemoveAll();
        pSprite->m_baseStats.m_hitPoints = 1;
    }

    const ULONG before = pGame->m_worldTime.m_gameTime;
    CHECK(pGame->RestParty(7, FALSE) == FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    // Eight hours, whatever the wounds.
    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING);

    for (CGameSprite* pSprite : party) {
        // The heal (effect 17, applied as it arrives) gives the points
        // asked for.
        CHECK(pSprite->m_baseStats.m_hitPoints == std::min(8, static_cast<int>(pSprite->m_baseStats.m_maxHitPointsBase)));
        CHECK(HasQueuedAction(pSprite, 96));

        BOOL trigger = FALSE;
        POSITION pos = pSprite->m_pendingTriggers.GetHeadPosition();
        while (pos != NULL) {
            if (pSprite->m_pendingTriggers.GetNext(pos)->m_triggerID == 0x93) {
                trigger = TRUE;
            }
        }
        CHECK(trigger);
    }

    // Renting a room returns TRUE.
    CHECK(pGame->RestParty(7, TRUE) == TRUE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
}

TEST_CASE("rest: with healing on rest the clock runs by the party's wounds and the heal is full", "[rest][data]")
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
    g_pBaldurChitin->pActiveEngine = g_pBaldurChitin->m_pEngineWorld;

    // AR1000's own rest encounter (activated, a 10% chance an hour) would
    // interrupt the rest this test measures.
    party[0]->GetArea()->m_headerRestEncounter.m_activated = 0;

    HealOnRestGuard guard(TRUE);

    // The hourly rate: the given points plus, per member, 19 per cleric or
    // druid level plus 5 and, from level 10, 19 per paladin or ranger level
    // less 147.
    INT nRate = 3;
    for (CGameSprite* pSprite : party) {
        INT nLevel = pSprite->m_derivedStats.GetClassMaskLevel(CLASSMASK_CLERIC | CLASSMASK_DRUID);
        if (nLevel > 0) {
            nRate += nLevel * 19 + 5;
        }
        nLevel = pSprite->m_derivedStats.GetClassMaskLevel(CLASSMASK_PALADIN | CLASSMASK_RANGER);
        if (nLevel > 9) {
            nRate += nLevel * 19 - 147;
        }
    }

    // Nobody hurt by more than a point: one rest period.
    for (CGameSprite* pSprite : party) {
        pSprite->m_baseStats.m_hitPoints = pSprite->m_baseStats.m_maxHitPointsBase - 1;
    }
    ULONG before = pGame->m_worldTime.m_gameTime;
    pGame->RestParty(3, FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING);

    // Wounds within an hour's healing: two rest periods.
    party[0]->m_baseStats.m_hitPoints = party[0]->m_baseStats.m_maxHitPointsBase - 2;
    party[1]->m_baseStats.m_hitPoints = party[1]->m_baseStats.m_maxHitPointsBase;
    before = pGame->m_worldTime.m_gameTime;
    pGame->RestParty(3, FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING * 2);

    // Deep wounds: the hours of healing, rounded up, in rest periods, plus
    // one; everybody is healed in full (1000 points). A first CHR with cleric
    // or druid levels raises the hourly rate above a small premade's pool, so
    // widen the leader's pool to keep the wounds above one rate.
    if (party[0]->m_baseStats.m_maxHitPointsBase < static_cast<SHORT>(nRate + 4)) {
        party[0]->m_baseStats.m_maxHitPointsBase = static_cast<SHORT>(nRate + 4);
        party[0]->m_derivedStats.m_nMaxHitPoints = party[0]->m_baseStats.m_maxHitPointsBase;
    }
    party[0]->m_baseStats.m_hitPoints = 1;
    party[1]->m_baseStats.m_hitPoints = 2;
    const INT nWounds = party[0]->m_baseStats.m_maxHitPointsBase - 1 + party[1]->m_baseStats.m_maxHitPointsBase - 2;
    const INT nHealed = (nWounds / nRate + (nWounds % nRate != 0 ? 1 : 0)) * nRate;
    REQUIRE(nHealed > nRate);
    before = pGame->m_worldTime.m_gameTime;
    pGame->RestParty(3, FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    const DWORD expected = CRuleTables::TIME_RESTING * static_cast<DWORD>(nHealed) * 100
            / static_cast<DWORD>(nRate * 100)
        + CRuleTables::TIME_RESTING;
    CHECK(pGame->m_worldTime.m_gameTime - before == expected);
    for (CGameSprite* pSprite : party) {
        CHECK(pSprite->m_baseStats.m_hitPoints == pSprite->m_baseStats.m_maxHitPointsBase);
    }
}

namespace {

// The bytes of a file in the view's temp folder; empty when there is none.
std::vector<char> TempFile(const char* name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(g_view, ec)) {
        if (Lower(entry.path().filename().string()) != "temp") {
            continue;
        }
        for (const auto& file : fs::directory_iterator(entry.path(), ec)) {
            if (Lower(file.path().filename().string()) == Lower(name)) {
                std::ifstream in(file.path(), std::ios::binary);
                return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            }
        }
    }
    return std::vector<char>();
}

uint32_t Dword(const std::vector<char>& d, size_t at)
{
    uint32_t value;
    std::memcpy(&value, d.data() + at, sizeof(value));
    return value;
}

}

// Reputation: CInfGame::ReputationAdjustment (0x5BDC00). REPUTATI.2DA gives
// the delta for the reputation the test sets and HAPPY.2DA the happiness of
// the new one, so the numbers below are the shipped tables' cells.
TEST_CASE("reputation: ChangeReputation moves the party and posts one happiness and one 0x6C effect per member", "[reputation][data]")
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
    REQUIRE(party.size() == 2);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CMessageHandler* pHandler = g_pBaldurChitin->GetMessageHandler();
    pHandler->AsynchronousUpdate();

    // HAPPY.2DA's columns: neutral 1, evil 2, anything else good 0.
    party[0]->m_typeAI.m_nAlignment = 2;
    party[1]->m_typeAI.m_nAlignment = 3;

    // Two different starting reputations, so the effect's SET is visible.
    party[0]->GetBaseStats()->m_reputation = 100;
    party[1]->GetBaseStats()->m_reputation = 30;

    // REPUTATI.2DA row 10 (reputation 100): KILL_INNOCENT is -40.
    pGame->m_nReputation = 100;
    pGame->ChangeReputation(0);
    CHECK(pGame->m_nReputation == 60);

    int nHappiness = 0;
    int nEffect = 0;
    for (POSITION pos = pHandler->m_messageList.GetHeadPosition(); pos != NULL;) {
        CMessage* pMessage = pHandler->m_messageList.GetNext(pos);
        if (pMessage->GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_SET_HAPPINESS) {
            nHappiness++;
        } else if (pMessage->GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ADD_EFFECT) {
            nEffect++;
        }
    }
    CHECK(nHappiness == 2);
    CHECK(nEffect == 2);

    pHandler->AsynchronousUpdate();

    // HAPPY.2DA row 6 (reputation 60): neutral 0, evil 80.
    CHECK(party[0]->m_nHappiness == 0);
    CHECK(party[1]->m_nHappiness == 80);

    // The effect is opcode 0x6C, amount the new reputation, duration type 1.
    // The 0x6C effect is applied immediately (dwFlags 1, duration type 1):
    // it SETS the member's base reputation to the party's, so a member at
    // 100 and one at 30 both end at 60 and neither was added to.
    CHECK(party[0]->GetBaseStats()->m_reputation == 60);
    CHECK(party[1]->GetBaseStats()->m_reputation == 60);

    // Clamped to reputation 1 (10 stored) and "set to 100" travels as a delta.
    pGame->ReputationAdjustment(-200, FALSE);
    CHECK(pGame->m_nReputation == 10);
    pGame->ReputationAdjustment(100, TRUE);
    CHECK(pGame->m_nReputation == 100);
    pHandler->AsynchronousUpdate();

    // A member another player controls gets neither message.
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    const PLAYER_ID nLocalPlayer = g_pChitin->cNetwork.m_idLocalPlayer;
    g_pChitin->cNetwork.m_nServiceProvider = 1;
    g_pChitin->cNetwork.m_idLocalPlayer = 1;
    party[0]->m_remotePlayerID = 2;
    party[1]->m_remotePlayerID = 1;
    pGame->m_nReputation = 100;
    pGame->ChangeReputation(0);
    int nLocalMessages = 0;
    for (POSITION pos = pHandler->m_messageList.GetHeadPosition(); pos != NULL;) {
        CMessage* pMessage = pHandler->m_messageList.GetNext(pos);
        if (pMessage->GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_SET_HAPPINESS
            || pMessage->GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ADD_EFFECT) {
            nLocalMessages++;
        }
    }
    CHECK(g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL);
    CHECK(nLocalMessages == 2); // one of each, for the member this player controls
    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    g_pChitin->cNetwork.m_idLocalPlayer = nLocalPlayer;
    party[0]->m_remotePlayerID = 0;
    party[1]->m_remotePlayerID = 0;
    pHandler->AsynchronousUpdate();
}

TEST_CASE("store: Marshal writes the store SetResRef read, without its sold-out items", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    CStore store(CResRef("11OSWALD"));
    CHECK(store.m_resRef == CResRef("11OSWALD"));
    REQUIRE(memcmp(store.m_pVersion, "STORV9.0", 8) == 0);
    REQUIRE(store.m_lInventory.GetCount() > 0);
    // The count read down to the items is left wrapped.
    CHECK(store.m_header.m_nInventoryCount == 0xFFFFFFFF);
    CHECK(store.m_bLocalCopy == FALSE);

    // One item sold out: Marshal drops it.
    CResRef soldOut(store.m_lInventory.GetHead()->m_itemId);
    const DWORD stock = store.m_lInventory.GetHead()->m_nInStock;
    store.m_lInventory.GetHead()->m_nInStock = 0;
    const INT nItems = store.m_lInventory.GetCount();

    store.Marshal(pGame->m_sTempDir);
    CHECK(store.m_lInventory.GetCount() == nItems - 1);

    std::vector<char> bytes = TempFile("11OSWALD.STO");
    REQUIRE(bytes.size() == 8 + sizeof(CStoreFileHeader) + sizeof(CStoreFileItem) * (nItems - 1) + sizeof(DWORD) * store.m_nBuyTypes + sizeof(CStoreFileDrinks) * store.m_nDrinks + sizeof(CStoreFileSpell) * store.m_nSpells);
    CHECK(std::memcmp(bytes.data(), "STORV9.0", 8) == 0);
    CHECK(Dword(bytes, 8 + 0x2C) == 0xF0);
    CHECK(Dword(bytes, 8 + 0x30) == static_cast<uint32_t>(nItems - 1));
    CHECK(Dword(bytes, 8 + 0x24) == 0xF0 + sizeof(CStoreFileItem) * (nItems - 1));
    CHECK(Dword(bytes, 8 + 0x28) == store.m_nBuyTypes);
    CHECK(Dword(bytes, 8 + 0x44) == Dword(bytes, 8 + 0x24) + sizeof(DWORD) * store.m_nBuyTypes);
    CHECK(Dword(bytes, 8 + 0x48) == store.m_nDrinks);
    CHECK(Dword(bytes, 8 + 0x68) == Dword(bytes, 8 + 0x44) + sizeof(CStoreFileDrinks) * store.m_nDrinks);
    CHECK(Dword(bytes, 8 + 0x6C) == store.m_nSpells);

    // The resource now lives in the temp folder: a store read from it is
    // the one written, a local copy, and writes the same bytes again.
    CStore reread(CResRef("11OSWALD"));
    CHECK(reread.m_bLocalCopy == TRUE);
    REQUIRE(reread.m_lInventory.GetCount() == nItems - 1);
    POSITION pos = reread.m_lInventory.GetHeadPosition();
    POSITION posOld = store.m_lInventory.GetHeadPosition();
    while (pos != NULL) {
        CStoreFileItem* pItem = reread.m_lInventory.GetNext(pos);
        CStoreFileItem* pOld = store.m_lInventory.GetNext(posOld);
        CHECK(std::memcmp(pItem, pOld, sizeof(CStoreFileItem)) == 0);
        CHECK(CResRef(pItem->m_itemId) != soldOut);
    }
    CHECK(reread.m_nBuyTypes == store.m_nBuyTypes);
    CHECK(reread.m_nDrinks == store.m_nDrinks);
    CHECK(reread.m_nSpells == store.m_nSpells);
    if (store.m_nDrinks != 0) {
        CHECK(std::memcmp(reread.m_pDrinks, store.m_pDrinks, sizeof(CStoreFileDrinks) * store.m_nDrinks) == 0);
    }
    if (store.m_nSpells != 0) {
        CHECK(std::memcmp(reread.m_pSpells, store.m_pSpells, sizeof(CStoreFileSpell) * store.m_nSpells) == 0);
    }

    reread.Marshal(pGame->m_sTempDir);
    CHECK(TempFile("11OSWALD.STO") == bytes);
    (void)stock;
}

TEST_CASE("journal: Unmarshal keeps each record's character and Marshal writes the records back", "[journal]")
{
    CSavedGameJournalEntry saved[3];
    saved[0].m_strEntry = 1000;
    saved[0].m_time = 50;
    saved[0].m_chapter = 2;
    saved[0].m_type = 4;
    saved[0].m_character = 3;
    saved[1].m_strEntry = 1001;
    saved[1].m_time = 60;
    saved[1].m_chapter = 0;
    saved[1].m_type = 1;
    saved[1].m_character = 0;
    saved[2].m_strEntry = 1002;
    saved[2].m_time = 70;
    saved[2].m_chapter = 2;
    saved[2].m_type = 2;
    saved[2].m_character = 0xFF;

    CGameJournal journal;
    journal.Unmarshal(saved, 3);
    REQUIRE(journal.CountEntries() == 3);

    CSavedGameJournalEntry out[3];
    for (CSavedGameJournalEntry& entry : out) {
        entry.m_read = 0xCD;
    }
    journal.Marshal(out);

    // Chapter order: chapter 0's record first, then chapter 2's in order.
    CHECK(out[0].m_strEntry == 1001);
    CHECK(out[0].m_chapter == 0);
    CHECK(out[0].m_read == 0);
    CHECK(out[0].m_type == 1);
    // A character of 0 loads as 1.
    CHECK(out[0].m_character == 1);
    CHECK(out[1].m_strEntry == 1000);
    CHECK(out[1].m_time == 50);
    CHECK(out[1].m_chapter == 2);
    CHECK(out[1].m_type == 4);
    CHECK(out[1].m_character == 3);
    CHECK(out[2].m_strEntry == 1002);
    CHECK(out[2].m_character == 0xFF);
}

namespace {

// The personal slots (18 to 41) of a sprite, emptied.
void ClearPersonalSlots(CGameSprite* pSprite)
{
    for (int n = 18; n < 18 + CScreenInventory::PERSONAL_INVENTORY_SIZE; n++) {
        pSprite->m_equipment.m_items[n] = NULL;
    }
}

}

TEST_CASE("inventory: SwapItemPersonalInventory places, takes, splits, stacks and refuses", "[inventory][data]")
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
    ClearPersonalSlots(pSprite);

    STRREF strError = 0;

    // A slot outside the personal inventory: "Bad Slot Number".
    CItem* pItem = new CItem(CResRef("00POTN01"), 0, 0, 0, 0, 0);
    CHECK(pGame->SwapItemPersonalInventory(0, pItem, 5, strError, 1, FALSE, FALSE) == FALSE);
    CHECK(strError == 9374);
    CHECK(pItem != NULL);

    // No slot given: the first empty one.
    pSprite->m_equipment.m_items[18] = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    CHECK(pGame->SwapItemPersonalInventory(0, pItem, 0x7FFF, strError, 1, FALSE, FALSE) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
    CHECK(pItem == NULL);
    REQUIRE(pSprite->m_equipment.m_items[19] != NULL);
    CHECK(pSprite->m_equipment.m_items[19]->GetResRef() == CResRef("00POTN01"));

    // Taking an item out of a slot: the hand gets it and the slot empties.
    CHECK(pGame->SwapItemPersonalInventory(0, pItem, 19, strError, 1, FALSE, FALSE) == TRUE);
    REQUIRE(pItem != NULL);
    CHECK(pItem->GetResRef() == CResRef("00POTN01"));
    CHECK(pSprite->m_equipment.m_items[19] == NULL);

    CItem* pGems = pSprite->m_equipment.m_items[18];
    const WORD nMax = pGems->GetMaxStackable();
    if (nMax > 3) {
        // Taking two of a stack of three splits it.
        pGems->SetUsageCount(0, 3);
        CItem* pHand = NULL;
        CHECK(pGame->SwapItemPersonalInventory(0, pHand, 18, strError, 2, FALSE, FALSE) == TRUE);
        REQUIRE(pHand != NULL);
        CHECK(pHand->GetResRef() == CResRef("00GEM01"));
        CHECK(pHand->GetUsageCount(0) == 2);
        CHECK(pGems->GetUsageCount(0) == 1);

        // Putting them back stacks them and empties the hand.
        CHECK(pGame->SwapItemPersonalInventory(0, pHand, 18, strError, 2, FALSE, FALSE) == TRUE);
        CHECK(pHand == NULL);
        CHECK(pGems->GetUsageCount(0) == 3);

        // Auto-stacking finds the stack for a gem given no slot.
        pHand = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
        CHECK(pGame->SwapItemPersonalInventory(0, pHand, 0x7FFF, strError, 1, FALSE, TRUE) == TRUE);
        CHECK(pHand == NULL);
        CHECK(pGems->GetUsageCount(0) == 4);
    }

    // A full inventory: "Inventory Is Full".
    for (int n = 18; n < 18 + CScreenInventory::PERSONAL_INVENTORY_SIZE; n++) {
        if (pSprite->m_equipment.m_items[n] == NULL) {
            pSprite->m_equipment.m_items[n] = new CItem(CResRef("00POTN02"), 1, 0, 0, 0, 0);
        }
    }
    CHECK(pGame->SwapItemPersonalInventory(0, pItem, 0x7FFF, strError, 1, FALSE, FALSE) == FALSE);
    CHECK(strError == 17999);
    CHECK(pItem != NULL);
    delete pItem;
    ClearPersonalSlots(pSprite);
}

TEST_CASE("inventory: an item dropped on a bag goes into the bag's store", "[inventory][store][data]")
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
    ClearPersonalSlots(pSprite);

    CStore bag(CResRef("00GBGA01"));
    REQUIRE(bag.GetType() == 4);
    const INT nBefore = bag.GetNumItems();

    pSprite->m_equipment.m_items[20] = new CItem(CResRef("00GBGA01"), 0, 0, 0, 0, 0);
    CItem* pGem = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    REQUIRE(bag.IsItemAcceptable(pGem));

    STRREF strError = 0;
    CHECK(pGame->SwapItemPersonalInventory(0, pGem, 20, strError, 1, FALSE, FALSE) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
    CHECK(pGem == NULL);
    // The bag itself stays in its slot.
    REQUIRE(pSprite->m_equipment.m_items[20] != NULL);
    CHECK(pSprite->m_equipment.m_items[20]->GetResRef() == CResRef("00GBGA01"));

    // The bag's store was written to the temp folder with the gem in it.
    CStore after(CResRef("00GBGA01"));
    CHECK(after.GetNumItems() == nBefore + 1);
    BOOL found = FALSE;
    POSITION pos = after.m_lInventory.GetHeadPosition();
    while (pos != NULL) {
        if (CResRef(after.m_lInventory.GetNext(pos)->m_itemId) == CResRef("00GEM01")) {
            found = TRUE;
        }
    }
    CHECK(found);

    // A bag in a bag: "Wrong Item Type", and the item goes nowhere.
    CItem* pOtherBag = new CItem(CResRef("00GBGA02"), 0, 0, 0, 0, 0);
    CHECK(pGame->SwapItemPersonalInventory(0, pOtherBag, 20, strError, 1, FALSE, FALSE) == FALSE);
    CHECK(strError == 26285);
    CHECK(pOtherBag != NULL);
    CHECK(pSprite->m_equipment.m_items[20]->GetResRef() == CResRef("00GBGA01"));
    delete pOtherBag;
    ClearPersonalSlots(pSprite);
}

TEST_CASE("store: AddItemExt puts a sold item on its line or a new one", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CStore store(CResRef("11OSWALD"));
    REQUIRE(store.GetType() != 4);

    // An item without a resref is refused.
    CItem empty;
    CHECK(store.AddItemExt(empty, 1) == -1);

    // A single item the store already lists (and does not have in infinite
    // supply) raises its stock.
    INT nIndex = 0;
    CStoreFileItem* pLine = NULL;
    POSITION pos = store.m_lInventory.GetHeadPosition();
    while (pos != NULL) {
        CStoreFileItem* pStoreItem = store.m_lInventory.GetNext(pos);
        CItem probe(CResRef(pStoreItem->m_itemId), 0, 0, 0, 0, 0);
        if ((pStoreItem->m_nStoreFlags & 0x1) == 0 && probe.GetMaxStackable() <= 1
            && probe.GetResRef() == CResRef(pStoreItem->m_itemId)) {
            pLine = pStoreItem;
            break;
        }
        nIndex++;
    }
    if (pLine != NULL) {
        const DWORD stock = pLine->m_nInStock;
        CItem same(CResRef(pLine->m_itemId), 0, 0, 0, 0, 0);
        CHECK(store.AddItemExt(same, 1) == nIndex);
        CHECK(pLine->m_nInStock == stock + 1);
    }

    // A gem it does not list goes on a new line at the end.
    const char* gems[] = { "00GEM01", "00GEM02", "00GEM03", "00GEM04", "00GEM05", "00GEM06", "00GEM07", "00GEM08" };
    const char* unlisted = NULL;
    for (const char* name : gems) {
        if (store.GetItemIndex(CResRef(name)) == INT_MAX) {
            unlisted = name;
            break;
        }
    }
    REQUIRE(unlisted != NULL);
    const INT nLines = store.m_lInventory.GetCount();
    CItem gem(CResRef(unlisted), 3, 0, 0, 0, 0);
    CHECK(store.AddItemExt(gem, 1) == nLines);
    REQUIRE(store.m_lInventory.GetCount() == nLines + 1);
    CStoreFileItem* pNew = store.m_lInventory.GetTail();
    CHECK(CResRef(pNew->m_itemId) == CResRef(unlisted));
    // Gems (type 34) are kept as single units in stock.
    CHECK(pNew->m_usageCount[0] == 1);
    CHECK(pNew->m_nInStock == 3);
    CHECK(gem.GetUsageCount(0) == 3);
}

TEST_CASE("import: ImportCharacter builds the sprite of a CHR with its name, biography and quick slots", "[import][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    // The premade character AKCHAR1 (CHR and its biography RES).
    fs::path chr;
    std::error_code ec;
    for (const auto& folder : fs::directory_iterator(g_view, ec)) {
        if (Lower(folder.path().filename().string()) != "characters") {
            continue;
        }
        for (const auto& file : fs::directory_iterator(folder.path(), ec)) {
            if (Lower(file.path().filename().string()) == "akchar1.chr") {
                chr = file.path();
            }
        }
    }
    if (chr.empty()) {
        SKIP("[iwd2r-skip] no AKCHAR1.CHR in the Characters folder");
    }
    std::ifstream in(chr, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(bytes.compare(0, 8, "CHR V2.2") == 0);
    const std::string name(bytes.c_str() + 8, strnlen(bytes.c_str() + 8, 32));

    const DWORD gold = pGame->m_gameSave.m_nPartyGold;
    // NOTE: The result is the sprite's id, which may be 0 (the first object
    // of a new game) like the failure's.
    const LONG nId = pGame->ImportCharacter(CString("AKCHAR1"), 2);

    CGameSprite* pSprite = NULL;
    REQUIRE(pGame->GetObjectArray()->GetShare(nId, CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE)
        == CGameObjectArray::SUCCESS);
    CHECK(std::string(static_cast<LPCSTR>(pSprite->m_sName)) == name);
    // The biography goes into the user strings, at 0xF240 plus the index.
    CHECK(pSprite->m_baseStats.m_biography == 0xF242u);
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(0xF242, strRes);
    CHECK(strRes.szText.GetLength() > 0);
    CHECK(pSprite->m_bGlobal == TRUE);
    CHECK(pSprite->m_nLastSpellbookClassIndex == static_cast<BYTE>(bytes[8 + 0x15A]));
    // The party gets the character's starting gold (rolled with rand() by
    // CRuleTables::GetStartingGold).
    CHECK(pGame->m_gameSave.m_nPartyGold > gold);
    pGame->GetObjectArray()->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    // A file that is not there: 0.
    CHECK(pGame->ImportCharacter(CString("NOSUCHCH"), 0) == 0);
}

// CScreenCreateChar::ImportCharacter (0x613B20) wraps the one above for the
// create char screen. On the single-player full-import path (the settings'
// bitfield forced to 7 when no network provider) the exe ends in
// m_nCurrentStep = 6 (0x613cdb), m_nFirstStep = m_nCurrentStep (0x613ceb)
// and field_1628 = 1 (0x613d13, the character is not re-exported on
// Finish). Step 6 is what the main panel's UpdateMainPanel repaints from:
// without it the panel stays in the fresh-character state after the import
// popup closes.
TEST_CASE("import: the create char screen enters the imported state after a CHR import", "[import][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    fs::path chr;
    std::error_code ec;
    for (const auto& folder : fs::directory_iterator(g_view, ec)) {
        if (Lower(folder.path().filename().string()) != "characters") {
            continue;
        }
        for (const auto& file : fs::directory_iterator(folder.path(), ec)) {
            if (Lower(file.path().filename().string()) == "akchar1.chr") {
                chr = file.path();
            }
        }
    }
    if (chr.empty()) {
        SKIP("[iwd2r-skip] no AKCHAR1.CHR in the Characters folder");
    }

    CScreenCreateChar* pCreateChar = g_pBaldurChitin->m_pEngineCreateChar;
    REQUIRE(pCreateChar != NULL);
    // The fresh-character panel state an import is made from.
    pCreateChar->m_nCurrentStep = 0;
    pCreateChar->m_nFirstStep = 0;
    pCreateChar->field_1628 = 0;
    pCreateChar->ImportCharacter(CString("AKCHAR1"));

    CHECK(pCreateChar->m_nCurrentStep == 6);
    CHECK(pCreateChar->m_nFirstStep == 6);
    CHECK(pCreateChar->field_1628 == 1);
    CHECK(pCreateChar->GetSpriteId() != CGameObjectArray::INVALID_INDEX);
}

namespace {

// The stock of an item's first line in a store, 0 when it has none.
DWORD StockOf(CStore& store, const CResRef& item)
{
    POSITION pos = store.m_lInventory.GetHeadPosition();
    while (pos != NULL) {
        CStoreFileItem* pLine = store.m_lInventory.GetNext(pos);
        if (CResRef(pLine->m_itemId) == item) {
            return pLine->m_nInStock;
        }
    }
    return 0;
}

}

TEST_CASE("store: TakeItemFromStore takes the item out of the store for good", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // An identified line with stock and no infinite supply.
    CResRef item;
    DWORD stock = 0;
    {
        CStore store(CResRef("11OSWALD"));
        POSITION pos = store.m_lInventory.GetHeadPosition();
        while (pos != NULL) {
            CStoreFileItem* pLine = store.m_lInventory.GetNext(pos);
            if ((pLine->m_nStoreFlags & 0x1) == 0 && (pLine->m_dynamicFlags & 0x1) != 0 && pLine->m_nInStock > 0
                && StockOf(store, CResRef(pLine->m_itemId)) == pLine->m_nInStock) {
                item = CResRef(pLine->m_itemId);
                stock = pLine->m_nInStock;
                break;
            }
        }
    }
    REQUIRE(stock > 0);

    CHECK(pGame->TakeItemFromStore(CResRef("11OSWALD"), item, 1) == 1);

    CStore after(CResRef("11OSWALD"));
    CHECK(StockOf(after, item) == stock - 1);

    // RemoveItemExt on a line it cannot find answers 0.
    CHECK(after.RemoveItemExt(CResRef("NOSUCHIT"), 0, -1, 1, NULL) == 0);
    CHECK(after.RemoveItemExt(CResRef(""), 0, -1, 1, NULL) == 0);
}

TEST_CASE("store: TakeItemBags takes a gem back out of a bag", "[store][inventory][data]")
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
    ClearPersonalSlots(pSprite);

    pSprite->m_equipment.m_items[20] = new CItem(CResRef("00GBGA01"), 0, 0, 0, 0, 0);
    CItem* pGem = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    STRREF strError = 0;
    REQUIRE(pGame->SwapItemPersonalInventory(0, pGem, 20, strError, 1, FALSE, FALSE) == TRUE);
    {
        CStore bag(CResRef("00GBGA01"));
        REQUIRE(bag.GetItemIndex(CResRef("00GEM01")) != INT_MAX);
    }

    CHECK(pSprite->TakeItemBags(CString("00GEM01"), 1, 20) == 1);

    CStore bag(CResRef("00GBGA01"));
    CHECK(bag.GetItemIndex(CResRef("00GEM01")) == INT_MAX);
    ClearPersonalSlots(pSprite);
}

TEST_CASE("store: the store screen's moves from a bag to the store remove the item from the bag without asserting", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CScreenStore* pScreen = g_pBaldurChitin->m_pEngineStore;
    REQUIRE(pScreen != NULL);
    // The buy and sell panel and the single-player button bar, as the
    // screen's activation picks them.
    if (pScreen->m_cUIManager.GetPanel(2) == NULL) {
        pScreen->EngineGameInit();
    }
    CUIPanel* pOldMain = pScreen->m_pMainPanel;
    CUIPanel* pOldBar = pScreen->m_pButtonBar;
    pScreen->m_pMainPanel = pScreen->m_cUIManager.GetPanel(2);
    pScreen->m_pButtonBar = pScreen->m_cUIManager.GetPanel(3);
    REQUIRE(pScreen->m_pMainPanel != NULL);
    REQUIRE(pScreen->m_pButtonBar != NULL);

    for (int path = 0; path < 2; path++) {
        INFO("path " << path);
        CStore* pStore = new CStore(CResRef("11OSWALD"));
        CStore* pBag = new CStore(CResRef("00GBGA01"));
        while (pBag->m_lInventory.GetCount() > 0) {
            delete pBag->m_lInventory.RemoveHead();
        }
        CItem gem(CResRef("00GEM02"), 1, 0, 0, 0, 0);
        REQUIRE(pBag->AddItemExt(gem, 1) == 0);
        const DWORD stock = StockOf(*pStore, CResRef("00GEM02"));

        CStore* pOldStore = pScreen->m_pStore;
        CStore* pOldBag = pScreen->m_pBag;
        pScreen->m_pStore = pStore;
        pScreen->m_pBag = pBag;
        CScreenStoreItem* pEntry = new CScreenStoreItem();
        pEntry->m_bSelected = TRUE;
        pScreen->m_lGroupItems.AddTail(pEntry);

        if (path == 0) {
            pScreen->sub_67A5B0();
        } else {
            pScreen->sub_67B030();
        }

        CHECK(pBag->m_lInventory.GetCount() == 0);
        CHECK(StockOf(*pStore, CResRef("00GEM02")) == stock + 1);

        // The moves rebuild the group list (UpdateGroupItems 0x676780 frees
        // the unselected lines, the test's among them); free what is left.
        pScreen->DestroyGroupItems(FALSE);
        pScreen->m_pStore = pOldStore;
        pScreen->m_pBag = pOldBag;
        delete pStore;
        delete pBag;
    }
    pScreen->m_pMainPanel = pOldMain;
    pScreen->m_pButtonBar = pOldBar;
}

TEST_CASE("store: InvalidateStore cuts the temp copy of a store to an empty STOO header", "[store][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // A store that is not in the temp folder is left alone.
    CStore::InvalidateStore(CResRef("11DEIRD"));
    {
        CStore store(CResRef("11DEIRD"));
        CHECK(std::memcmp(store.m_pVersion, "STORV9.0", 8) == 0);
        CHECK(store.m_lInventory.GetCount() > 0);
        store.Marshal(pGame->m_sTempDir);
    }

    // Its temp copy becomes "STOOV9.0" alone, which no longer loads as a
    // store.
    CStore::InvalidateStore(CResRef("11DEIRD"));
    CHECK(TempFile("11DEIRD.STO") == std::vector<char>({ 'S', 'T', 'O', 'O', 'V', '9', '.', '0' }));
    CStore after(CResRef("11DEIRD"));
    CHECK(std::memcmp(after.m_pVersion, "STOOV9.0", 8) == 0);
    CHECK(after.m_lInventory.GetCount() == 0);
}

// 0x46FEA0: the area's rest encounter. One chance roll (a miss returns FALSE;
// the binary's retry counter never takes its back edge), then the rest's random
// delay; then a rarity roll, the creature entry and the count jitter. The delay
// is drawn before the square search, so the clock's jump pins the first two
// draws against the same libc; the search's tie-breaks and the spawn placement
// come after and cannot move them.
//
// NOT asserted here (implemented from the disassembly only): the rarity 0 path
// (GetNearestOpenSquare + rand() % 2 per creature instead of the formation
// offsets), the search's rand() % 7 tie-breaks, and AddCurrentTime's <= 900
// branch (the seeds chosen always give a delay above it).
TEST_CASE("rest: a rest encounter interrupts the rest with its rolled delay and creatures", "[rest][data]")
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
    CGameArea* pArea = party[0]->GetArea();
    REQUIRE(pArea != NULL);

    // An encounter of our own: a goblin, one at a time, always.
    CAreaFileRestEncounter& rest = pArea->m_headerRestEncounter;
    INFO("AR1000's own encounter: activated " << rest.m_activated
                                              << ", chance " << rest.m_probDay << "/" << rest.m_probNight);
    rest = CAreaFileRestEncounter();
    rest.m_activated = 1;
    rest.m_probDay = 100;
    rest.m_probNight = 100;
    rest.m_randomCreatureNum = 1;
    rest.m_maxTypeNum = 1;
    std::memcpy(rest.m_randomCreature[0], "00GOB", 6);
    rest.m_randomCreatureString[0] = 1000;

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;

    // A chance of 100 always hits (the compare is <=), so the seed only has
    // to give a delay the clock acts on: AddCurrentTime ignores <= 900.
    int nHitSeed = 0;
    ULONG nDelay = 0;
    for (int n = 1; n < 1000 && nHitSeed == 0; n++) {
        srand(n);
        rand();
        const ULONG nDrawn = rand() % CTimerWorld::TIMESCALE_MSEC_PER_HOUR;
        if (nDrawn > 900) {
            nHitSeed = n;
            nDelay = nDrawn;
        }
    }
    REQUIRE(nHitSeed != 0);

    srand(nHitSeed);
    ClearMessages();
    const ULONG nBefore = pGame->m_worldTime.m_gameTime;
    const int nObjects = ObjectsInArea(pArea);

    // The count is maxTypeNum plus the jitter (rand() % 5 - 2), clamped at
    // one; the draws before it are the chance, the delay, the rarity
    // (rand() % 10) and the entry (rand() % 1).
    srand(nHitSeed);
    rand();
    rand();
    rand();
    rand();
    const int nExpected = std::max(1, 1 + rand() % 5 - 2);

    srand(nHitSeed);
    CHECK(pArea->CheckRestEncounter() == TRUE);
    CHECK(pGame->m_worldTime.m_gameTime == nBefore + nDelay);
    CHECK(ObjectsInArea(pArea) == nObjects + nExpected);

    // The encounter's text, last after the creatures' own messages: the
    // area's string ref, no name, the last party member as both caller and
    // target (the original's quirk).
    CMessage* pLast = handler.m_messageList.IsEmpty() ? NULL : handler.m_messageList.GetTail();
    auto* pText = dynamic_cast<CMessageDisplayTextRef*>(pLast);
    REQUIRE(pText != nullptr);
    CHECK(pText->m_name == static_cast<STRREF>(-1));
    CHECK(pText->m_text == 1000);
    CHECK(pText->m_sourceId == pGame->GetCharacterId(1));
    CHECK(pText->m_targetId == pGame->GetCharacterId(1));

    // The same seed draws the chance and then the delay: the order the two
    // draws are in.
    srand(nHitSeed);
    CHECK(rand() % 100 <= 100);
    CHECK(static_cast<ULONG>(rand() % CTimerWorld::TIMESCALE_MSEC_PER_HOUR) == nDelay);

    // No chance: the one roll happens and misses (a 0 chance still hits on a
    // 0 draw, so the seed is chosen to draw non-zero), so nothing changes.
    rest.m_probDay = 0;
    rest.m_probNight = 0;

    int nMissSeed = 0;
    for (int n = 1; n < 1000 && nMissSeed == 0; n++) {
        srand(n);
        if (rand() % 100 != 0) {
            nMissSeed = n;
        }
    }
    REQUIRE(nMissSeed != 0);

    srand(nMissSeed);
    ClearMessages();
    const ULONG nBeforeMiss = pGame->m_worldTime.m_gameTime;
    const int nObjectsMiss = ObjectsInArea(pArea);

    CHECK(pArea->CheckRestEncounter() == FALSE);
    CHECK(pGame->m_worldTime.m_gameTime == nBeforeMiss);
    CHECK(ObjectsInArea(pArea) == nObjectsMiss);
    CHECK(handler.m_messageList.IsEmpty());

    // The count: maxTypeNum plus the jitter (rand() % 5 - 2), clamped at one.
    // The draws before it are the chance, the delay, the rarity
    // (rand() % 10) and the entry (rand() % 1).
    rest.m_probDay = 100;
    rest.m_maxTypeNum = 3;

    srand(nHitSeed);
    rand();
    rand();
    rand();
    rand();
    const int nExpectedCount = std::max(1, 3 + rand() % 5 - 2);

    srand(nHitSeed);
    ClearMessages();
    const int nObjectsCount = ObjectsInArea(pArea);
    CHECK(pArea->CheckRestEncounter() == TRUE);
    CHECK(ObjectsInArea(pArea) == nObjectsCount + nExpectedCount);

    // Through RestPartyPrivate (the hook): the encounter ends the rest before
    // the heal, the Rest action and the rest trigger.
    rest.m_maxTypeNum = 1;
    srand(nHitSeed);
    for (CGameSprite* pSprite : party) {
        pSprite->m_queuedActions.RemoveAll();
        pSprite->m_pendingTriggers.RemoveAll();
        pSprite->m_baseStats.m_hitPoints = 1;
    }
    g_pBaldurChitin->pActiveEngine = g_pBaldurChitin->m_pEngineWorld;
    HealOnRestGuard guard(FALSE);

    CHECK(pGame->RestParty(7, FALSE) == FALSE);
    for (CGameSprite* pSprite : party) {
        CHECK(HasQueuedAction(pSprite, 96) == FALSE);
        CHECK(pSprite->m_baseStats.m_hitPoints == 1);
    }

    // Without the encounter the same rest heals and queues the Rest action.
    rest.m_probDay = 0;
    rest.m_probNight = 0;
    srand(nMissSeed);
    for (CGameSprite* pSprite : party) {
        pSprite->m_queuedActions.RemoveAll();
        pSprite->m_pendingTriggers.RemoveAll();
        pSprite->m_baseStats.m_hitPoints = 1;
    }

    CHECK(pGame->RestParty(7, FALSE) == FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(HasQueuedAction(party[0], 96));
    CHECK(party[0]->m_baseStats.m_hitPoints > 1);

    // The dawn tick is day: the binary's band is TIME_DAWN <= t < TIME_NIGHT
    // (`div` then `cmp [0x84EC34] 99000; jae night` / `cmp [0x84EC1C] 27000;
    // jae day` at 0x46FEE7-0x46FEFB). At exactly 27000 with the day chance 100
    // and the night chance 0 the day roll is the one taken; the night roll of
    // the old `>` port missed on every seed but a 1-in-100.
    pGame->m_worldTime.m_gameTime = CTimerWorld::TIME_DAWN;
    rest.m_probDay = 100;
    rest.m_probNight = 0;

    // The seed's first draw (the chance) must not be 0 - the night roll would
    // then hit on it - and its second (the delay) must be above 900, or the
    // clock's own minimum swallows the jump.
    int nEdgeSeed = 0;
    for (int n = 1; n < 1000 && nEdgeSeed == 0; n++) {
        srand(n);
        if (rand() % 100 == 0) {
            continue;
        }
        if (rand() % CTimerWorld::TIMESCALE_MSEC_PER_HOUR > 900) {
            nEdgeSeed = n;
        }
    }
    REQUIRE(nEdgeSeed != 0);

    srand(nEdgeSeed);
    ClearMessages();
    const ULONG nBeforeEdge = pGame->m_worldTime.m_gameTime;
    CHECK(pArea->CheckRestEncounter() == TRUE);
    CHECK(pGame->m_worldTime.m_gameTime > nBeforeEdge);
}
