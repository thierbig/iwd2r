#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CBlood.h"
#include "CButtonData.h"
#include "CDerivedStats.h"
#include "CGameArea.h"
#include "CGameButtonList.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfButtonArray.h"
#include "CInfGame.h"
#include "CParticle.h"
#include "CRuleTables.h"
#include "CScreenCreateChar.h"
#include "CScreenJournal.h"
#include "CScreenWorld.h"
#include "CSoundProperties.h"
#include "CUIControlBase.h"
#include "CUIControlEdit.h"
#include "CUIControlEditMultiLine.h"
#include "CUIControlFactory.h"
#include "CUIControlTextDisplay.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CWarp.h"
#include "CWeather.h"
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

// The ui-misc stream's controls and visual systems: the action bar
// (CInfButtonArray), CBlood's particles, CWeather's wind and the text
// display's items, on the engine in this process and a private view of the
// install without saves (tests/data_view.h).

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

// The party member of a new one-character game, selected, or NULL. The game
// is made once per process (CInfGame::NewGame wants no characters).
CGameSprite* SelectedPartyMember()
{
    static bool made = false;
    if (!made) {
        std::vector<BYTE> cre = FinishedCharacter();
        if (cre.empty())
            return NULL;
        NewGameWithOneCharacter(cre, "Probe");
        made = true;
    }
    CGameSprite* pSprite = PartySprite();
    if (pSprite == NULL)
        return NULL;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0)
        pGame->SelectCharacter(pSprite->GetId(), FALSE);
    return pSprite;
}

CButtonData* LevelEntry(BYTE nLevel)
{
    CButtonData* pButtonData = new CButtonData();
    pButtonData->m_abilityId.field_1D = nLevel;
    return pButtonData;
}

size_t Particles(USHORT nType, SHORT nDirection)
{
    CBlood blood(NULL, 400, 300, 20, nDirection, nType, 50);
    return static_cast<size_t>(blood.m_particleList.GetCount());
}

}

// CParticle's copy constructor (0x7C35B0) copies everything but the tag,
// the tail length included (CBlood copies a template particle for each one
// it makes); an infinite life span is 0x7FFFFFFF (0x85EB74).
TEST_CASE("particle: the copy keeps the tail length", "[blood]")
{
    CHECK(CParticle::LIFESPAN_INFINITE == 0x7FFFFFFF);

    CParticle particle(1, 2, 3, 4, 5, 6, RGB(7, 8, 9), 4, 512, CParticle::DOTS);
    particle.m_nTimeStamp = 11;
    particle.m_nRenderTime = 40;
    particle.m_nLifeSpan = 15;
    particle.m_bTag = TRUE;

    CParticle copy(particle);
    CHECK(copy.m_pos.x == 1);
    CHECK(copy.m_pos.y == 2);
    CHECK(copy.m_pos.z == 3);
    CHECK(copy.m_vel.x == 4);
    CHECK(copy.m_vel.y == 5);
    CHECK(copy.m_vel.z == 6);
    CHECK(copy.m_rgbColor == RGB(7, 8, 9));
    CHECK(copy.m_nTailLength == 4);
    CHECK(copy.m_nGravity == 512);
    CHECK(copy.m_wType == CParticle::DOTS);
    CHECK(copy.m_nTimeStamp == 11);
    CHECK(copy.m_nRenderTime == 40);
    CHECK(copy.m_nLifeSpan == 15);
    CHECK_FALSE(copy.m_bTag);

    CParticle blank;
    CHECK(blank.m_nLifeSpan == CParticle::LIFESPAN_INFINITE);
}

// GenerateParticles (0x51A500): a stream type's particle count is fixed (the
// streams shrink by streams / length each step; the high squirt adds a left
// and a right particle per step).
TEST_CASE("blood: each stream type makes its fixed number of particles", "[blood]")
{
    CHECK(Particles(0, 0) == 70); // SQUIRT_HIGH: 20+16+12+8+4 and 2 per step
    CHECK(Particles(1, 4) == 30); // SQUIRT_MEDIUM: 10+8+6+4+2
    CHECK(Particles(2, 8) == 15); // SQUIRT_LOW
    CHECK(Particles(3, 12) == 60); // GUSH_HIGH
    CHECK(Particles(4, 2) == 30); // GUSH_MEDIUM
    CHECK(Particles(5, 6) == 15); // GUSH_LOW
    CHECK(Particles(6, 0) == 21); // PULSATING_ARTERY: 6+5+4+3+2+1
}

// GenerateExplodingBlood (0x51A2A0): bursts until a running count of
// rand() % 5 reaches 30; each burst is one particle and rand() % 20 more.
// CalcBoundingRect (0x519C40) centres its rectangle on the point with the
// video mode's FX size, so this one needs the engine. The test engine never
// activates its video mode, so it has no surfaces for CVidInf::GetFXSize to
// check; the 3D flag makes it answer with the FX size instead.
TEST_CASE("blood: exploding blood makes blobs", "[blood][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    const BOOL bIs3dAccelerated = g_pChitin->cVideo.m_bIs3dAccelerated;
    g_pChitin->cVideo.m_bIs3dAccelerated = TRUE;
    CBlood blood(NULL, 400, 300, 0, 0, 7, 50);
    CSize size;
    const BOOL bHaveSize = g_pChitin->pActiveEngine->pVidMode->GetFXSize(size);
    g_pChitin->cVideo.m_bIs3dAccelerated = bIs3dAccelerated;

    CHECK(blood.m_particleType == CParticle::BLOB);
    CHECK(blood.m_particleList.GetCount() > 0);
    CHECK(blood.BloodLeft());

    REQUIRE(bHaveSize);
    size.cx--;
    size.cy--;
    CHECK(blood.m_refPoint == CPoint(400 - (size.cx >> 1), 300 - (size.cy >> 1)));
    CHECK(blood.m_rBounding.right == 400 + (size.cx >> 1));
    CHECK(blood.m_rBounding.bottom == 300 + (size.cy >> 1));
    CHECK(blood.m_rBounding.left == 400 - (size.cx >> 1));
    CHECK(blood.m_rBounding.top == 300 - (size.cy >> 1));
}

// AsynchronousUpdate (0x519740): one time stamp per call; a particle whose
// render time is still ahead stops the walk; the blood is done (FALSE) once
// its particles and their splashes have died.
TEST_CASE("blood: the update runs the particles out", "[blood]")
{
    for (USHORT nType = 0; nType <= 6; nType++) {
        INFO(nType);
        CBlood blood(NULL, 400, 300, 20, 0, nType, 50);
        REQUIRE(blood.BloodLeft());
        int nCalls = 0;
        BOOLEAN bLeft = TRUE;
        while (bLeft && nCalls < 1000) {
            bLeft = blood.AsynchronousUpdate();
            nCalls++;
            CHECK(blood.m_nTimeStamp == nCalls);
            CHECK(static_cast<bool>(bLeft) == static_cast<bool>(blood.BloodLeft()));
        }
        CHECK_FALSE(bLeft);
        CHECK(nCalls < 1000);
        CHECK(blood.m_particleList.IsEmpty());
        CHECK(blood.m_lstSplashParticles.IsEmpty());
    }
}

// CalcBoundingRect (0x519C40): the artery's rectangle by direction.
TEST_CASE("blood: the pulsating artery's bounding rectangle by direction", "[blood]")
{
    CBlood south(NULL, 400, 300, 20, 0, 6, 50);
    CHECK(south.m_rBounding == CRect(380, 240, 420, 320));
    CHECK(south.m_refPoint == CPoint(380, 240));

    CBlood north(NULL, 400, 300, 20, 8, 6, 50);
    CHECK(north.m_rBounding == CRect(380, 230, 420, 300));
    CHECK(north.m_refPoint == CPoint(380, 230));

    CBlood west(NULL, 400, 300, 20, 4, 6, 50);
    CHECK(west.m_rBounding == CRect(335, 240, 415, 320));
    CHECK(west.m_refPoint == CPoint(335, 240));

    // The left and top edges stop at 0; the reference point does not.
    CBlood corner(NULL, 5, 10, 20, 0, 6, 50);
    CHECK(corner.m_rBounding == CRect(0, 0, 25, 30));
    CHECK(corner.m_refPoint == CPoint(-15, -50));
}

// CalcBoundingRect (0x519C40): a squirt's or gush's rectangle by direction.
TEST_CASE("blood: a stream's bounding rectangle by direction", "[blood]")
{
    CBlood south(NULL, 400, 300, 20, 0, 0, 50);
    CHECK(south.m_rBounding == CRect(355, 270, 445, 360));
    CHECK(south.m_refPoint == CPoint(355, 270));

    CBlood north(NULL, 400, 300, 20, 8, 3, 50);
    CHECK(north.m_rBounding == CRect(355, 250, 445, 330));
    CHECK(north.m_refPoint == CPoint(355, 250));
}

// GetWind (0x5564F0): 100 with the wind off; in a light wind the share left
// of the first fifth of the transition, compared signed.
TEST_CASE("weather: the wind's ambient share", "[weather][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CWeather& weather = g_pBaldurChitin->m_pEngineWorld->m_weather;
    const BOOLEAN bWindOn = weather.m_bWindOn;
    const USHORT nWindLevel = weather.m_nWindLevel;
    const ULONG nDurationCounter = weather.m_nDurationCounter;

    weather.m_bWindOn = FALSE;
    CHECK(weather.GetWind() == 100);

    weather.m_bWindOn = TRUE;
    weather.m_nWindLevel = 32;
    weather.m_nDurationCounter = 0;
    CHECK(weather.GetWind() == 0);

    weather.m_nWindLevel = 16;
    const ULONG nFifth = CWeather::WEATHER_TRANSITION_TIME / 5;
    weather.m_nDurationCounter = 0;
    CHECK(weather.GetWind() == 100);
    weather.m_nDurationCounter = nFifth / 2;
    CHECK(weather.GetWind() == 100 - 500 * (nFifth / 2) / CWeather::WEATHER_TRANSITION_TIME);
    weather.m_nDurationCounter = nFifth;
    CHECK(weather.GetWind() == 0);
    weather.m_nDurationCounter = nFifth + 1;
    CHECK(weather.GetWind() == 0);
    // The compare is signed: a counter past 0x7FFFFFFF passes it, and the
    // share wraps as the original's 32-bit unsigned product and byte do:
    // 500 * 0xFFFFFFFF is 0xFFFFFE0C, / 150 is 28633111, low byte 23.
    REQUIRE(CWeather::WEATHER_TRANSITION_TIME == 150);
    weather.m_nDurationCounter = 0xFFFFFFFF;
    CHECK(weather.GetWind() == 100 - 23);

    weather.m_bWindOn = bWindOn;
    weather.m_nWindLevel = nWindLevel;
    weather.m_nDurationCounter = nDurationCounter;
}

// The constructor (0x5879E0): state 0, button 100 selected, the default
// customizable buttons.
TEST_CASE("action bar: the constructor's defaults", "[actionbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::unique_ptr<CInfButtonArray> pBar = std::make_unique<CInfButtonArray>();
    CHECK(pBar->m_nState == 0);
    CHECK(pBar->m_nSelectedButton == 100);
    CHECK(pBar->field_16E0 == -1);
    CHECK(pBar->field_19B2 == 0);
    CHECK(pBar->m_stateStack.IsEmpty());
    CHECK(pBar->m_nSelectedWeaponSet == 0);
    const INT customizable[9] = { 5, 3, 70, 71, 80, 81, 90, 91, 10 };
    for (int n = 0; n < 9; n++) {
        INFO(n);
        CHECK(pBar->m_customizableButtonTypes[n] == customizable[n]);
    }
    for (int n = 0; n < 12; n++) {
        INFO(n);
        CHECK(pBar->m_buttonArray[n].field_1D4 == 0);
    }
}

// The destructor (0x59E980): the one explicit statement tears the global
// button list down; the member destruction after it is compiler-generated.
TEST_CASE("action bar: the destructor tears the global button list down", "[actionbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    // The constructor's SetState(0, 0) also runs sub_587BD0, so populate the
    // global only after the bar exists.
    std::unique_ptr<CInfButtonArray> pBar = std::make_unique<CInfButtonArray>();
    CGameButtonList* pList = new CGameButtonList();
    pList->AddTail(LevelEntry(1));
    pList->AddTail(LevelEntry(2));
    CInfButtonArray::dword_8E6820 = pList;

    pBar.reset();
    CHECK(CInfButtonArray::dword_8E6820 == NULL);
}

// SetState (0x589110) pushes the state it leaves, ResetState (0x588FF0) and
// sub_589FF0 pop it; field_19B2 is the stack's size.
TEST_CASE("action bar: the state stack", "[actionbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    if (SelectedPartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfButtonArray& bar = g_pBaldurChitin->GetObjectGame()->m_cButtonArray;
    bar.m_stateStack.RemoveAll();
    bar.field_19B2 = 0;
    bar.m_nState = 0;

    // Nothing is pushed while the state is 0.
    bar.SetState(110, TRUE);
    CHECK(bar.m_nState == 110);
    CHECK(bar.field_19B2 == 0);
    const INT state110[12] = { 7, 8, 15, 16, 17, 18, 19, 20, 100, 100, 100, 100 };
    for (int n = 0; n < 12; n++) {
        INFO(n);
        CHECK(bar.m_buttonTypes[n] == state110[n]);
    }

    bar.SetState(108, TRUE);
    CHECK(bar.m_nState == 108);
    CHECK(bar.field_19B2 == 1);
    CHECK(bar.m_buttonTypes[0] == 21);
    CHECK(bar.m_buttonTypes[11] == 32);

    // The same state again pushes nothing.
    bar.SetState(108, TRUE);
    CHECK(bar.field_19B2 == 1);

    bar.SetState(109, TRUE);
    CHECK(bar.field_19B2 == 2);
    CHECK(static_cast<int>(bar.m_stateStack.GetCount()) == bar.field_19B2);

    // One pop goes back to the last state.
    CHECK(bar.ResetState());
    CHECK(bar.m_nState == 108);
    CHECK(bar.field_19B2 == 1);

    bar.SetState(109, TRUE);
    CHECK(bar.field_19B2 == 2);
    // Popping everything goes back to the first state pushed.
    bar.sub_589FF0(FALSE, TRUE);
    CHECK(bar.m_nState == 110);
    CHECK(bar.field_19B2 == 0);
    CHECK(bar.m_buttonTypes[0] == 7);

    // An empty stack keeps the state.
    CHECK(bar.ResetState());
    CHECK(bar.m_nState == 110);
}

// UpdateButtons (0x58A340), through SetState (0x589110): state 110 is guard,
// attack, stop, the five quick formations and four empty buttons. Each sets the frames of GUIBTACT it shows, its
// tooltip and, for a formation, its FORMn icon.
TEST_CASE("action bar: the buttons of state 110", "[actionbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    if (SelectedPartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CUIPanel* pPanel = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(1);
    REQUIRE(pPanel != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CInfButtonArray& bar = pGame->m_cButtonArray;
    bar.m_stateStack.RemoveAll();
    bar.field_19B2 = 0;
    bar.m_nState = 0;
    bar.m_nSelectedButton = -1;
    REQUIRE(bar.SetState(110, FALSE));

    // Guard: frames 0 and 2, tooltip 15925.
    CHECK(bar.m_buttonArray[0].field_0 == 1);
    CHECK(bar.m_buttonArray[0].field_8 == 1);
    CHECK(bar.m_buttonArray[0].field_C == 0);
    CHECK(bar.m_buttonArray[0].field_10 == 2);
    CHECK(pPanel->GetControl(6)->m_nToolTipStrRef == 15925);
    // Attack: frames 12 and 14, tooltip 4666.
    CHECK(bar.m_buttonArray[1].field_C == 12);
    CHECK(bar.m_buttonArray[1].field_10 == 14);
    CHECK(pPanel->GetControl(7)->m_nToolTipStrRef == 4666);
    // Stop: frame 44, tooltip 15924.
    CHECK(bar.m_buttonArray[2].field_C == 44);
    CHECK(bar.m_buttonArray[2].field_10 == 44);
    CHECK(pPanel->GetControl(8)->m_nToolTipStrRef == 15924);

    // The quick formations show their icons, the current one selected.
    for (int n = 0; n < 5; n++) {
        INFO(n);
        const CInfButtonSettings& settings = bar.m_buttonArray[3 + n];
        CHECK(settings.field_0 == 1);
        CHECK(settings.field_4 == 1);
        CHECK(settings.field_8 == 0);
        const SHORT nFormation = pGame->m_gameSave.m_quickFormations[n];
        CString sIcon;
        if (nFormation == -1) {
            sIcon = "STONFORM";
        } else if (nFormation < 10) {
            sIcon.Format("FORM%d", nFormation);
        } else {
            sIcon.Format("FORM%c", static_cast<char>(nFormation) + '7');
        }
        const CResRef& icon = static_cast<const CResHelper<CResCell, 1000>&>(settings.field_14).cResRef;
        CHECK(icon == CResRef(sIcon));
        CHECK(settings.field_1CC == (nFormation == pGame->m_gameSave.m_curFormation ? 1 : 0));
        CHECK(pPanel->GetControl(9 + n)->m_nToolTipStrRef == 4935);
    }

    // The last four are empty and greyed out.
    for (int n = 8; n < 12; n++) {
        INFO(n);
        CHECK(bar.m_buttonArray[n].field_0 == 0);
        CHECK(bar.m_buttonArray[n].m_bGreyOut);
        CHECK(pPanel->GetControl(6 + n)->m_nToolTipStrRef == static_cast<STRREF>(-1));
    }
}

// sub_595FB0 (0x595FB0) and sub_596040 (0x596040) page a spell list by
// level: forward to the first entry of the next level (0 at the last
// level), back to the first entry of the previous level (0 past the top).
TEST_CASE("action bar: paging a spell list by level", "[actionbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CInfButtonArray& bar = g_pBaldurChitin->GetObjectGame()->m_cButtonArray;
    CInfButtonArray::sub_587BD0();
    CGameButtonList* pList = new CGameButtonList();
    pList->AddTail(LevelEntry(1));
    pList->AddTail(LevelEntry(1));
    pList->AddTail(LevelEntry(2));
    pList->AddTail(LevelEntry(2));
    pList->AddTail(LevelEntry(3));
    CInfButtonArray::dword_8E6820 = pList;

    bar.field_197A = 0;
    CHECK(bar.sub_595FB0(pList) == 2);
    bar.field_197A = 2;
    CHECK(bar.sub_595FB0(pList) == 4);
    bar.field_197A = 4;
    CHECK(bar.sub_595FB0(pList) == 0);

    bar.field_197A = 4;
    CHECK(bar.sub_596040(pList) == 2);
    bar.field_197A = 2;
    CHECK(bar.sub_596040(pList) == 0);

    bar.field_197A = 0;
    CInfButtonArray::sub_587BD0();
    CHECK(CInfButtonArray::dword_8E6820 == NULL);
}

// CUIControlTextDisplay::GetItem (0x4E3E70): an item that wraps comes back
// as its first line and the next one joined by a space, the last item
// included (CScreenWorld copies every item of the dialog display into the
// new one when the dialog panel changes). Only an item's first line has a
// line count, and the loop reads the count of the line it appended, so the
// original stops after the second line.
TEST_CASE("text display: an item's first two lines come back joined", "[textdisplay][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    // The world screen's panels load with a game.
    FramePump pump;
    if (SelectedPartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    // The world screen's message panel's text display, as its activation
    // takes it (CScreenWorld 0x686DE0).
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CUIPanel* pPanel = pWorld->GetManager()->GetPanel(pWorld->GetPanel_22_0());
    REQUIRE(pPanel != NULL);
    CUIControlTextDisplay* pDisplay = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(1));
    REQUIRE(pDisplay != NULL);
    REQUIRE(pDisplay->m_plstStrings != NULL);
    pDisplay->RemoveAll();

    const CString sShort("A short line.");
    CString sLong;
    for (int n = 0; n < 40; n++) {
        CString sWord;
        sWord.Format("word%d ", n);
        sLong += sWord;
    }
    sLong.TrimRight();
    pDisplay->DisplayString(CString("Probe: "), sShort, RGB(255, 255, 0), RGB(255, 255, 255), -1, FALSE, TRUE);
    POSITION posLong = pDisplay->DisplayString(CString("Probe: "), sLong, RGB(255, 255, 0), RGB(255, 255, 255), -1, FALSE, TRUE);
    REQUIRE(posLong != NULL);
    REQUIRE(pDisplay->field_A6C == 2);

    // The long item wraps over three lines or more; only its first line has
    // the count.
    const INT nLines = pDisplay->m_plstStrings->GetAt(posLong)->GetNumLines();
    REQUIRE(nLines > 2);
    CString sJoined;
    POSITION pos = posLong;
    for (INT n = 0; n < nLines; n++) {
        REQUIRE(pos != NULL);
        CDisplayString* pLine = pDisplay->m_plstStrings->GetNext(pos);
        if (n > 0) {
            CHECK(pLine->GetNumLines() == 0);
        }
        if (n == 1) {
            sJoined += " ";
        }
        if (n < 2) {
            sJoined += pLine->GetString();
        }
    }
    CHECK(pos == NULL);

    CString sLabel;
    CString sString;
    REQUIRE(pDisplay->GetItem(0, sLabel, sString));
    CHECK(sString == sShort);
    CHECK(sLabel.Find("Probe") == 0);
    REQUIRE(pDisplay->GetItem(1, sLabel, sString));
    CHECK(sString == sJoined);
    CHECK(sString.Find("word0 ") == 0);
    CHECK(sString.Find("word39") == -1);
    CHECK_FALSE(pDisplay->GetItem(2, sLabel, sString));
    CHECK(sString == "");

    pDisplay->RemoveAll();
}

// OnPageDown (0x4E28B0), OnPageUp (0x4E2A00) and OnScroll (0x4E2790): the top
// line moves by the page less one line, within the strings past the visible
// lines; the step's compare with the visible lines is unsigned, so with no
// visible lines a page down moves up one line.
TEST_CASE("text display: paging and scrolling", "[textdisplay][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    if (SelectedPartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CUIPanel* pPanel = pWorld->GetManager()->GetPanel(pWorld->GetPanel_22_0());
    REQUIRE(pPanel != NULL);
    CUIControlTextDisplay* pDisplay = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(1));
    REQUIRE(pDisplay != NULL);
    pDisplay->RemoveAll();
    for (int n = 0; n < 10; n++) {
        CString sLine;
        sLine.Format("line %d", n);
        pDisplay->DisplayString(CString(""), sLine, RGB(255, 255, 0), RGB(255, 255, 255), -1, FALSE, TRUE);
    }
    REQUIRE(pDisplay->m_plstStrings->GetCount() == 10);
    const short nVisible = pDisplay->field_A6A;
    pDisplay->field_A6A = 4;
    pDisplay->field_5A = 0;

    pDisplay->OnPageDown(10);
    CHECK(pDisplay->field_5A == 3);
    pDisplay->OnPageDown(10);
    CHECK(pDisplay->field_5A == 6);
    pDisplay->OnPageDown(1);
    CHECK(pDisplay->field_5A == 6);
    pDisplay->OnPageUp(2);
    CHECK(pDisplay->field_5A == 4);
    pDisplay->OnPageUp(100);
    CHECK(pDisplay->field_5A == 1);
    pDisplay->OnPageUp(100);
    CHECK(pDisplay->field_5A == 0);

    pDisplay->OnScroll(1, 2);
    CHECK(pDisplay->field_5A == 3);
    pDisplay->OnScroll(2, 2);
    CHECK(pDisplay->field_5A == 6);

    pDisplay->field_A6A = 0;
    pDisplay->OnPageDown(5);
    CHECK(pDisplay->field_5A == 5);
    pDisplay->OnPageUp(5);
    CHECK(pDisplay->field_5A == 6);

    pDisplay->field_A6A = nVisible;
    pDisplay->RemoveAll();
}

// CRuleTables::GetSoundEnvironment (0x545890) wraps the reverb index by the
// REVERB table's rows (m_nSizeY), not its columns.
TEST_CASE("rules: the sound environment wraps by the reverb table's rows", "[rules][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    const CRuleTables& rules = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    const INT nRows = rules.m_tReverb.GetHeight();
    const INT nColumns = rules.m_tReverb.GetWidth();
    INFO(nRows << " rows, " << nColumns << " columns");
    REQUIRE(nRows > 2);
    REQUIRE(nRows != nColumns);

    for (INT nReverb : { 0, 1, nRows - 1, nRows, nRows + 2 }) {
        INFO(nReverb);
        EAXPRESET preset;
        memset(&preset, 0, sizeof(preset));
        const INT nPreset = rules.GetSoundEnvironment(preset, nReverb);
        CHECK(nPreset == nReverb % nRows);
        CHECK(preset.fVolume == static_cast<float>(atof(rules.m_tReverb.GetAt(CPoint(1, nPreset)))));
        CHECK(preset.fDecayTime == static_cast<float>(atof(rules.m_tReverb.GetAt(CPoint(2, nPreset)))));
    }
}

// CRuleTables::GetLayOnHandsAmount (0x544930): the object type's class mask
// decides, the paladin level times (charisma - 10) / 2 (charisma less 1
// below 10), the level alone below 1.
TEST_CASE("rules: lay on hands follows the object type's class", "[rules][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    const CRuleTables& rules = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    std::unique_ptr<CDerivedStats> pStats = std::make_unique<CDerivedStats>();
    pStats->m_nClassLevels[CAIOBJECTTYPE_C_PALADIN - 1] = 6;
    pStats->m_classMask = 0;
    CAIObjectType typeAI;
    typeAI.m_nClassMask = CLASSMASK_PALADIN;

    pStats->m_nCHR = 16;
    CHECK(rules.GetLayOnHandsAmount(typeAI, *pStats) == 18);
    pStats->m_nCHR = 17;
    CHECK(rules.GetLayOnHandsAmount(typeAI, *pStats) == 18);
    pStats->m_nCHR = 11;
    CHECK(rules.GetLayOnHandsAmount(typeAI, *pStats) == 6);
    pStats->m_nCHR = 8;
    CHECK(rules.GetLayOnHandsAmount(typeAI, *pStats) == 6);

    // The derived stats' mask does not count.
    typeAI.m_nClassMask = 0;
    pStats->m_classMask = CLASSMASK_PALADIN;
    pStats->m_nCHR = 16;
    CHECK(rules.GetLayOnHandsAmount(typeAI, *pStats) == 0);
}

// CUIControlEdit::OnKeyDown (0x4D7130): Enter puts a changed text at the head
// of the history and moves the rest down, the oldest dropped; up shows the
// entry at the history index and moves it on, down the one two back. The
// world screen's console edit (CUIControlEditLua, 0x77BD00) keeps ten.
TEST_CASE("edit: the input history", "[edit][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    if (SelectedPartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CUIPanel* pPanel = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(6);
    REQUIRE(pPanel != NULL);
    CUIControlEditLua* pEdit = static_cast<CUIControlEditLua*>(pPanel->GetControl(0));
    REQUIRE(pEdit != NULL);
    REQUIRE(pEdit->field_873 == 10);
    for (int n = 0; n < 10; n++) {
        pEdit->field_874[n] = "";
    }
    pEdit->field_872 = 1;

    auto enter = [&](const char* text) {
        pEdit->m_sText = text;
        pEdit->m_nCursorIndex = static_cast<SHORT>(pEdit->m_sText.GetLength());
        pEdit->OnKeyDown(VK_RETURN);
    };
    enter("first");
    enter("second");
    enter("third");
    CHECK(pEdit->field_874[0] == "third");
    CHECK(pEdit->field_874[1] == "second");
    CHECK(pEdit->field_874[2] == "first");
    CHECK(pEdit->field_874[3] == "");
    CHECK(pEdit->field_872 == 1);

    // The same text again changes nothing.
    enter("third");
    CHECK(pEdit->field_874[1] == "second");

    pEdit->OnKeyDown(VK_UP);
    CHECK(pEdit->m_sText == "second");
    CHECK(pEdit->field_872 == 2);
    pEdit->OnKeyDown(VK_UP);
    CHECK(pEdit->m_sText == "first");
    CHECK(pEdit->field_872 == 3);
    pEdit->OnKeyDown(VK_UP);
    CHECK(pEdit->m_sText == "first");
    CHECK(pEdit->field_872 == 3);

    pEdit->OnKeyDown(VK_DOWN);
    CHECK(pEdit->m_sText == "second");
    CHECK(pEdit->field_872 == 2);
    pEdit->OnKeyDown(VK_DOWN);
    CHECK(pEdit->m_sText == "third");
    CHECK(pEdit->field_872 == 1);
    pEdit->OnKeyDown(VK_DOWN);
    CHECK(pEdit->m_sText == "third");
    CHECK(pEdit->field_872 == 1);

    // Ten entries at most: the oldest goes.
    for (int n = 0; n < 10; n++) {
        CString sText;
        sText.Format("entry %d", n);
        enter(sText);
    }
    CHECK(pEdit->field_874[0] == "entry 9");
    CHECK(pEdit->field_874[9] == "entry 0");

    for (int n = 0; n < 10; n++) {
        pEdit->field_874[n] = "";
    }
    pEdit->m_sText = "";
}

namespace {

// Character generation's biography edit (panel 51, control 4), empty and
// focused, with its scroll bar (control 3).
CUIControlEditMultiLine* BiographyEdit()
{
    CUIPanel* pPanel = g_pBaldurChitin->m_pEngineCreateChar->GetManager()->GetPanel(51);
    if (pPanel == NULL) {
        return NULL;
    }
    CUIControlEditMultiLine* pEdit = static_cast<CUIControlEditMultiLine*>(pPanel->GetControl(4));
    if (pEdit == NULL) {
        return NULL;
    }
    pEdit->m_bActive = TRUE;
    pEdit->SetText(CString(""));
    pEdit->m_sText = "";
    pEdit->field_86A = 0;
    pEdit->field_86E = 0;
    pEdit->field_87A = 0;
    pEdit->field_88A = -1;
    pEdit->field_88E = -1;
    pEdit->m_bFocused = TRUE;
    return pEdit;
}

// Types TEXT (letters, digits and spaces) through OnKeyDown, with no Shift.
void TypeText(CUIControlEditMultiLine* pEdit, const char* text)
{
    for (const char* p = text; *p != '\0'; p++) {
        char ch = *p;
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
        pEdit->OnKeyDown(static_cast<SHORT>(ch == ' ' ? VK_SPACE : ch));
    }
}

// The pixel widths of the edit's lines, split at its line breaks, without
// the space a soft break keeps at the line's end.
std::vector<LONG> LineWidths(CUIControlEditMultiLine* pEdit)
{
    std::vector<LONG> widths;
    CString sText = pEdit->m_sText;
    INT nStart = 0;
    while (nStart <= sText.GetLength()) {
        INT nEnd = CUtil::FindOneOf(sText.Mid(nStart), CString("\n\r"), 0);
        if (nEnd == -1) {
            nEnd = sText.GetLength() - nStart;
        }
        CString sLine = sText.Mid(nStart, nEnd);
        if (sLine.Right(1) == " ") {
            sLine = sLine.Left(sLine.GetLength() - 1);
        }
        widths.push_back(pEdit->m_cVidFont.GetStringLength(sLine, FALSE));
        nStart += nEnd + 1;
    }
    return widths;
}

}

// ReConstitute (0x4D9830), through SetText (0x4E17B0) and CalculateNewLines
// (0x4D9D00) mode 3: the text wraps at spaces into lines narrower than the
// control less 10 pixels, with soft breaks (\r) GetText takes out again; the
// hard breaks stay.
TEST_CASE("multi-line edit: the text wraps into the control", "[editml][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CUIControlEditMultiLine* pEdit = BiographyEdit();
    REQUIRE(pEdit != NULL);

    CString sText;
    for (int n = 0; n < 60; n++) {
        CString sWord;
        sWord.Format("word%d ", n);
        sText += sWord;
    }
    sText += "\nlast line";
    pEdit->SetText(sText);

    CHECK(pEdit->GetText() == sText);
    CHECK(pEdit->m_sText.Find('\r') != -1);
    CHECK(pEdit->field_87A > 2);

    // Every line fits; the line count is the breaks plus one.
    std::vector<LONG> widths = LineWidths(pEdit);
    CHECK(static_cast<INT>(widths.size()) == pEdit->field_87A);
    for (size_t n = 0; n < widths.size(); n++) {
        INFO(n);
        CHECK(widths[n] < pEdit->m_size.cx - 10);
    }

    // A soft break follows a space; the hard break stays where it was.
    INT nBreak = pEdit->m_sText.Find('\r');
    CHECK(pEdit->m_sText[nBreak - 1] == ' ');
    CHECK(pEdit->m_sText.Right(10) == "\nlast line");

    // Reflowing again changes nothing.
    CString sWrapped = pEdit->m_sText;
    pEdit->ReConstitute();
    CHECK(pEdit->m_sText == sWrapped);
}

// OnKeyDown (0x4DB160) and CalculateNewLines (0x4D9D00) modes 1 and 2:
// typing at the end, Backspace, the arrows, Home, End, Enter and Ctrl+Left.
TEST_CASE("multi-line edit: typing and moving the cursor", "[editml][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CUIControlEditMultiLine* pEdit = BiographyEdit();
    REQUIRE(pEdit != NULL);
    // No Shift, Ctrl or Caps Lock is down in the test engine.
    REQUIRE_FALSE(pEdit->m_pPanel->m_pManager->m_pWarp->GetShiftKey());
    REQUIRE_FALSE(pEdit->m_pPanel->m_pManager->m_pWarp->GetCtrlKey());

    TypeText(pEdit, "hello world");
    CHECK(pEdit->GetText() == "hello world");
    CHECK(pEdit->field_86A == 11);

    pEdit->OnKeyDown(VK_BACK);
    CHECK(pEdit->GetText() == "hello worl");
    CHECK(pEdit->field_86A == 10);

    pEdit->OnKeyDown(VK_HOME);
    CHECK(pEdit->field_86A == 0);
    pEdit->OnKeyDown(VK_RIGHT);
    pEdit->OnKeyDown(VK_RIGHT);
    CHECK(pEdit->field_86A == 2);
    TypeText(pEdit, "x");
    CHECK(pEdit->GetText() == "hexllo worl");
    CHECK(pEdit->field_86A == 3);
    pEdit->OnKeyDown(VK_LEFT);
    CHECK(pEdit->field_86A == 2);
    pEdit->OnKeyDown(VK_DELETE);
    CHECK(pEdit->GetText() == "hello worl");
    CHECK(pEdit->field_86A == 2);

    pEdit->OnKeyDown(VK_END);
    CHECK(pEdit->field_86A == 10);
    pEdit->OnKeyDown(VK_RETURN);
    CHECK(pEdit->GetText() == "hello worl\n");
    CHECK(pEdit->field_86A == 11);
    CHECK(pEdit->field_87A == 2);
    TypeText(pEdit, "2nd");
    CHECK(pEdit->GetText() == "hello worl\n2nd");

    // Up goes to the same column of the line above, as near as it can.
    pEdit->OnKeyDown(VK_UP);
    CHECK(pEdit->field_86A <= 3);
    pEdit->OnKeyDown(VK_DOWN);
    CHECK(pEdit->field_86A >= 11);

    // Escape puts back the text the edit had when it took the focus.
    pEdit->field_866 = "kept";
    pEdit->OnKeyDown(VK_ESCAPE);
    CHECK(pEdit->GetText() == "kept");
}

// Typing past the line's end breaks it (CalculateNewLines mode 1), and
// deleting pulls the next line's words back up (mode 2).
TEST_CASE("multi-line edit: typing and deleting reflow the lines", "[editml][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CUIControlEditMultiLine* pEdit = BiographyEdit();
    REQUIRE(pEdit != NULL);

    std::string typed;
    for (int n = 0; n < 40; n++) {
        typed += "abc ";
    }
    TypeText(pEdit, typed.c_str());
    CHECK(pEdit->GetText() == CString(typed.c_str()));
    CHECK(pEdit->field_87A > 1);
    for (LONG width : LineWidths(pEdit)) {
        CHECK(width < pEdit->m_size.cx - 10);
    }

    INT nLines = pEdit->field_87A;
    for (int n = 0; n < 20; n++) {
        pEdit->OnKeyDown(VK_BACK);
    }
    CHECK(pEdit->GetText() == CString(typed.substr(0, typed.size() - 20).c_str()));
    pEdit->ReConstitute();
    CHECK(pEdit->field_87A <= nLines);
}

// OnKeyDown (0x4DB160) with Shift and Ctrl: Shift+End selects to the line's
// end and a letter replaces the selection; Ctrl+Left and Ctrl+Right jump by
// words; Ctrl+Home and Ctrl+End to the text's ends. Character generation
// never reports Ctrl (its vtable's GetCtrlKey is CWarp's, 0x78E6E0, in the
// original too), so the biography edit's manager answers to the journal
// screen, which does, while the test runs.
TEST_CASE("multi-line edit: selecting and word jumps", "[editml][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CUIControlEditMultiLine* pEdit = BiographyEdit();
    REQUIRE(pEdit != NULL);
    CUIManager* pManager = pEdit->m_pPanel->m_pManager;
    CWarp* pCreateChar = pManager->m_pWarp;
    CWarp* pWarp = g_pBaldurChitin->m_pEngineJournal;
    pManager->m_pWarp = pWarp;
    pWarp->SetSystemKeyCtrl(FALSE);
    pWarp->SetSystemKeyShift(FALSE);
    pWarp->SetSystemKeyCapsLock(FALSE);

    TypeText(pEdit, "one two three");
    pEdit->OnKeyDown(VK_HOME);
    CHECK(pEdit->field_86A == 0);

    pWarp->SetSystemKeyCtrl(TRUE);
    pEdit->OnKeyDown(VK_RIGHT);
    CHECK(pEdit->field_86A == 4);
    pEdit->OnKeyDown(VK_RIGHT);
    CHECK(pEdit->field_86A == 8);
    pEdit->OnKeyDown(VK_LEFT);
    CHECK(pEdit->field_86A == 4);
    pEdit->OnKeyDown(VK_END);
    CHECK(pEdit->field_86A == 13);
    pEdit->OnKeyDown(VK_HOME);
    CHECK(pEdit->field_86A == 0);
    CHECK(pEdit->field_86E == 0);
    pWarp->SetSystemKeyCtrl(FALSE);

    // Shift+Right twice, then Shift+End: the selection runs from 0 to 13.
    pWarp->SetSystemKeyShift(TRUE);
    pEdit->OnKeyDown(VK_RIGHT);
    pEdit->OnKeyDown(VK_RIGHT);
    CHECK(pEdit->field_88A == 0);
    CHECK(pEdit->field_88E == 2);
    pEdit->OnKeyDown(VK_END);
    CHECK(pEdit->field_88A == 0);
    CHECK(pEdit->field_88E == 13);
    pWarp->SetSystemKeyShift(FALSE);

    // A letter replaces the selection made forwards; the cursor stays.
    pEdit->OnKeyDown('Z');
    CHECK(pEdit->GetText() == "z");
    CHECK(pEdit->field_88A == -1);
    CHECK(pEdit->field_88E == -1);

    // Without Shift an arrow drops the selection.
    pEdit->SetText(CString("abcdef"));
    pEdit->field_86A = 3;
    pWarp->SetSystemKeyShift(TRUE);
    pEdit->OnKeyDown(VK_LEFT);
    pWarp->SetSystemKeyShift(FALSE);
    CHECK(pEdit->field_88A == 3);
    CHECK(pEdit->field_88E == 2);
    pEdit->OnKeyDown(VK_LEFT);
    CHECK(pEdit->field_88A == -1);
    CHECK(pEdit->field_88E == -1);
    CHECK(pEdit->field_86A == 1);

    // Delete takes a selection out and the cursor goes to its start.
    pEdit->field_88A = 1;
    pEdit->field_88E = 4;
    pEdit->OnKeyDown(VK_DELETE);
    CHECK(pEdit->GetText() == "aef");
    CHECK(pEdit->field_86A == 1);

    pManager->m_pWarp = pCreateChar;
}

// OnLButtonDown (0x4DABD0) puts the cursor at the character clicked (the end
// of the line right of its text) and starts a drag; OnMouseMove (0x4E05B0)
// moves the selection's end with it.
TEST_CASE("multi-line edit: clicking and dragging", "[editml][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CUIControlEditMultiLine* pEdit = BiographyEdit();
    REQUIRE(pEdit != NULL);
    pEdit->SetText(CString("first line\nsecond"));

    const CPoint ptText = pEdit->m_ptOrigin + pEdit->m_ptText;
    const SHORT nFontHeight = pEdit->m_cVidFont.GetFontHeight(FALSE);

    CHECK(pEdit->OnLButtonDown(ptText + CPoint(1, 1)));
    CHECK(pEdit->field_86A == 0);
    CHECK(pEdit->field_888 == 1);
    CHECK(pEdit->field_88A == 0);
    CHECK(pEdit->field_88E == 0);

    // Right of the first line's text: its end, before the break.
    CHECK(pEdit->OnLButtonDown(ptText + CPoint(pEdit->m_size.cx - 2, 1)));
    CHECK(pEdit->field_86A == 10);

    // The second line, dragged to from the first line's start.
    CHECK(pEdit->OnLButtonDown(ptText + CPoint(1, 1)));
    pEdit->OnMouseMove(ptText + CPoint(pEdit->m_size.cx - 2, nFontHeight + 1));
    CHECK(pEdit->field_86A == 17);
    CHECK(pEdit->field_88A == 0);
    CHECK(pEdit->field_88E == 17);
    pEdit->OnLButtonUp(ptText + CPoint(pEdit->m_size.cx - 2, nFontHeight + 1));
    CHECK(pEdit->field_888 == 0);
}

// The IME's composition (Double Byte Character Support): with the IME on
// (CImm::field_128), a key puts the composed string (CImm::sub_4D89E0, which
// hands it over and clears it) in at the cursor instead of the key's
// character. CUIControlEdit::OnKeyDown (0x4D7130) needs the option on too;
// CUIControlEditMultiLine::OnKeyDown (0x4DB160) only the IME. Off in a normal
// run; Linux has no IME, so the composition is set by hand.
TEST_CASE("edit: the IME's composition goes in at the cursor", "[edit][editml][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    if (SelectedPartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    const int nDoubleByte = g_pBaldurChitin->field_1A0;
    g_pBaldurChitin->field_1A0 = 1;
    g_pBaldurChitin->cImm.field_128 = 1;

    CUIPanel* pPanel = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(6);
    REQUIRE(pPanel != NULL);
    CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(0));
    REQUIRE(pEdit != NULL);
    pEdit->m_sText = "abcd";
    pEdit->m_nCursorIndex = 2;
    g_pBaldurChitin->cImm.field_130 = "XY";
    pEdit->OnKeyDown('Q');
    CHECK(pEdit->m_sText == "abXYcd");
    CHECK(pEdit->m_nCursorIndex == 4);
    CHECK(g_pBaldurChitin->cImm.field_130 == "");

    // With no double-byte characters Backspace and Left still go one
    // character.
    g_pBaldurChitin->cImm.field_128 = 0;
    pEdit->OnKeyDown(VK_BACK);
    CHECK(pEdit->m_sText == "abXcd");
    CHECK(pEdit->m_nCursorIndex == 3);
    pEdit->OnKeyDown(VK_LEFT);
    CHECK(pEdit->m_nCursorIndex == 2);
    pEdit->m_sText = "";

    CUIControlEditMultiLine* pMulti = BiographyEdit();
    REQUIRE(pMulti != NULL);
    pMulti->SetText(CString("hello"));
    pMulti->field_86A = 5;
    g_pBaldurChitin->cImm.field_128 = 1;
    g_pBaldurChitin->cImm.field_130 = " there";
    pMulti->OnKeyDown('Q');
    CHECK(pMulti->GetText() == "hello there");
    CHECK(pMulti->field_86A == 11);

    g_pBaldurChitin->cImm.field_128 = 0;
    g_pBaldurChitin->cImm.field_130 = "";
    g_pBaldurChitin->field_1A0 = nDoubleByte;
}
