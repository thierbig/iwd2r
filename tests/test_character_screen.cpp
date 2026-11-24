#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CUIControlButton.h"
#include "CUIPanel.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The character screen's level-up (CScreenCharacter), on the engine in this
// process and a private view of the install without saves
// (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

// The entry of `dir` whose name matches `name` without regard to case.
fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

// The engine as iwd2r_main builds it, on a private view of the install (as
// tests/test_gam.cpp does), every engine's panels loaded. Built once and
// leaked: the engine's globals live until the process ends.
fs::path g_view;

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
    view = std::make_unique<DataView>(original, DataView::Saves::None);
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
    std::error_code ec;
    if (!fs::exists(Child(g_view, "mpsave"), ec)) {
        fs::create_directories(g_view / "mpsave", ec);
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

// Stands in for the main loop: the engine's waits for a drawn frame return
// as soon as they begin.
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

// The CRE of the first finished character in the view's Characters folder.
std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        const std::string chr = ReadFile(file);
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

// A new game in AR1000 with one party member, as Party Formation's Done
// leaves it before it saves.
void NewGameWithOneCharacter(std::vector<BYTE>& bytes, const char* name)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
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
    pSprite->m_sName = name;
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }
}

// The party member in slot 0, denied to this thread while the test reads it.
CGameSprite* PartySprite()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NULL;
    LONG nId = pGame->GetCharacterId(0);
    if (pGame->GetObjectArray()->GetShare(nId, CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE) != CGameObjectArray::SUCCESS)
        return NULL;
    pGame->GetObjectArray()->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    return pSprite;
}

DWORD TopPopup(CScreenCharacter* pCharacter)
{
    CUIPanel* pPanel = pCharacter->GetTopPopup();
    return pPanel != NULL ? pPanel->m_nID : 0;
}

// A level 1 fighter (the CHR files the probes leave in Characters) with the
// experience for level 2, the character screen pointed at it.
CScreenCharacter* FighterReadyToLevel(std::vector<BYTE>& cre)
{
    NewGameWithOneCharacter(cre, "Probe");
    CGameSprite* pSprite = PartySprite();
    if (pSprite == NULL)
        return NULL;
    pSprite->m_baseStats.m_xp = 1000;
    pSprite->m_derivedStats.m_nXP = 1000;
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;
    pCharacter->m_nSelectedCharacter = 0;
    pCharacter->CheckMultiPlayerViewableModifyable();
    return pCharacter;
}

}

// OnLevelUpButtonClick (0x5E5330) keeps the creature's stats and opens the
// class popup (54). Its Done (OnDoneButtonClick 0x5E2210, case 54) starts
// the level (LevelUpStartAbilities 0x5D8EA0) and SummonNextPopup (0x5E1910)
// walks 54, 6, 7, 55, 56, 8, 16 asking CheckSummonPopup (0x5E0FD0): a
// fighter going to level 2 gets no specialization, no ability point and no
// spells, but skill points and a feat. The last Done ends in LevelUp
// (0x5E56F0), which frees the kept stats and closes every popup.
TEST_CASE("character screen: a fighter levels up through the class, skills and feats popups", "[character][levelup][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CScreenCharacter* pCharacter = FighterReadyToLevel(cre);
    REQUIRE(pCharacter != NULL);
    CGameSprite* pSprite = PartySprite();
    REQUIRE(pSprite != NULL);
    if (pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_FIGHTER) != 1 || pSprite->m_derivedStats.m_nLevel != 1) {
        SKIP("[iwd2r-skip] the first CHR is not a level 1 fighter");
    }
    REQUIRE(pCharacter->m_bMultiPlayerModifyable);
    REQUIRE(pCharacter->CanLevelUp(pSprite));

    const SHORT nMaxHitPointsBefore = pSprite->m_baseStats.m_maxHitPointsBase;

    pCharacter->OnLevelUpButtonClick();
    REQUIRE(TopPopup(pCharacter) == 54);
    REQUIRE(pCharacter->m_pTempBaseStats != NULL);
    REQUIRE(pCharacter->m_pTempDerivedStats != NULL);
    REQUIRE(pCharacter->m_pBeforeLevelUpDerivedStats != NULL);
    // Done needs a class.
    CHECK_FALSE(pCharacter->IsDoneButtonClickable(pSprite));

    pCharacter->m_nClass = CAIOBJECTTYPE_C_FIGHTER;
    REQUIRE(pCharacter->IsDoneButtonClickable(pSprite));
    pCharacter->OnDoneButtonClick();

    // The new level is in the creature already; the skills popup is up with
    // the level's points.
    CHECK(pSprite->m_baseStats.m_fighterLevel == 2);
    CHECK(pSprite->m_baseStats.m_characterLevel == 2);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase > nMaxHitPointsBefore);
    CHECK(pCharacter->field_1840 == pSprite->GetId());
    REQUIRE(TopPopup(pCharacter) == 55);
    CHECK(pCharacter->m_nLevelUpSkillPoints > 0);
    CHECK(pCharacter->m_nExtraSkillPoints == pCharacter->m_nLevelUpSkillPoints);

    // Spend the points (Done allows at most one left).
    pCharacter->m_nExtraSkillPoints = 0;
    pCharacter->OnDoneButtonClick();
    REQUIRE(TopPopup(pCharacter) == 56);
    CHECK(pCharacter->m_nLevelUpFeats > 0);
    CHECK(pCharacter->m_nExtraFeats == pCharacter->m_nLevelUpFeats);
    CHECK_FALSE(pCharacter->IsDoneButtonClickable(pSprite));

    pCharacter->m_nExtraFeats = 0;
    pCharacter->OnDoneButtonClick();

    CHECK(TopPopup(pCharacter) == 0);
    CHECK(pCharacter->m_pTempBaseStats == NULL);
    CHECK(pCharacter->m_pTempDerivedStats == NULL);
    CHECK(pCharacter->m_pBeforeLevelUpDerivedStats == NULL);
    CHECK(pSprite->m_baseStats.m_fighterLevel == 2);
    CHECK(pSprite->m_derivedStats.m_nLevel == 2);
    CHECK_FALSE(pCharacter->CanLevelUp(pSprite));
}

// Cancel walks back (SummonPreviousPopup 0x5E1B20): from the skills popup
// it skips the popups the level did not need and, passing 6, restores the
// creature as the level-up found it before reopening the class popup; Cancel
// there (OnCancelButtonClick 0x5E3B80, case 54) frees the kept stats and
// closes the screen's popups.
TEST_CASE("character screen: cancelling a level-up restores the creature", "[character][levelup][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CScreenCharacter* pCharacter = FighterReadyToLevel(cre);
    REQUIRE(pCharacter != NULL);
    CGameSprite* pSprite = PartySprite();
    REQUIRE(pSprite != NULL);
    if (pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_FIGHTER) != 1 || pSprite->m_derivedStats.m_nLevel != 1) {
        SKIP("[iwd2r-skip] the first CHR is not a level 1 fighter");
    }

    const SHORT nMaxHitPoints = pSprite->m_baseStats.m_maxHitPointsBase;
    const BYTE nAttackBase = pSprite->m_baseStats.m_attackBase;
    const BYTE nFortitude = pSprite->m_baseStats.m_saveVSFortitudeBase;

    pCharacter->OnLevelUpButtonClick();
    REQUIRE(TopPopup(pCharacter) == 54);
    pCharacter->m_nClass = CAIOBJECTTYPE_C_FIGHTER;
    pCharacter->OnDoneButtonClick();
    REQUIRE(TopPopup(pCharacter) == 55);
    REQUIRE(pSprite->m_baseStats.m_fighterLevel == 2);

    CHECK(pCharacter->OnCancelButtonClick());
    REQUIRE(TopPopup(pCharacter) == 54);
    CHECK(pSprite->m_baseStats.m_fighterLevel == 1);
    CHECK(pSprite->m_baseStats.m_characterLevel == 1);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase == nMaxHitPoints);
    CHECK(pSprite->m_baseStats.m_attackBase == nAttackBase);
    CHECK(pSprite->m_baseStats.m_saveVSFortitudeBase == nFortitude);
    CHECK(pSprite->m_derivedStats.m_nLevel == 1);

    CHECK(pCharacter->OnCancelButtonClick());
    CHECK(TopPopup(pCharacter) == 0);
    CHECK(pCharacter->m_pTempBaseStats == NULL);
    CHECK(pCharacter->m_pTempDerivedStats == NULL);
    CHECK(pCharacter->m_pBeforeLevelUpDerivedStats == NULL);
    CHECK(pSprite->m_baseStats.m_fighterLevel == 1);
    CHECK(pCharacter->CanLevelUp(pSprite));
}

// UpdateMainPanel (0x5DBDE0) enables the Level Up button (37) when
// CanLevelUp (0x5E4E80) allows it and the creature is not dead.
TEST_CASE("character screen: the main panel enables Level Up with the experience for it", "[character][levelup][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CScreenCharacter* pCharacter = FighterReadyToLevel(cre);
    REQUIRE(pCharacter != NULL);
    CGameSprite* pSprite = PartySprite();
    REQUIRE(pSprite != NULL);
    if (pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_FIGHTER) != 1 || pSprite->m_derivedStats.m_nLevel != 1) {
        SKIP("[iwd2r-skip] the first CHR is not a level 1 fighter");
    }

    CUIControlButton* pLevelUp = static_cast<CUIControlButton*>(pCharacter->GetManager()->GetPanel(2)->GetControl(37));
    REQUIRE(pLevelUp != NULL);

    pCharacter->UpdateMainPanel(TRUE);
    CHECK(pLevelUp->m_bEnabled);

    pSprite->m_baseStats.m_xp = 0;
    pSprite->m_derivedStats.m_nXP = 0;
    pCharacter->UpdateMainPanel(TRUE);
    CHECK_FALSE(pLevelUp->m_bEnabled);
}
