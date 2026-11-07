#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "CScreenCreateChar.h"
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
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// CMessageSpriteUpdate::Run (0x50D6F0): the receive side of the sprite
// update a networked machine applies to its copy of a sprite. A message
// built by the in-tree ctor (which captures every field group) runs as a
// no-op round trip, and a message that marks field groups present
// (field_108) overwrites the sprite with its own values.

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

// The engine as iwd2r_main builds it, on a private view of the install that
// lives until the process ends (as tests/test_gam.cpp does).
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

// Stands in for the main loop the tests do not run (as tests/test_gam.cpp).
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

// The CRE of a finished character: the first "CHR V2.2" file in the view's
// Characters folder, in file order.
std::vector<BYTE> FinishedCharacter(size_t nIndex = 0)
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    size_t seen = 0;
    for (const fs::path& file : files) {
        const std::string chr = ReadFile(file);
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0) {
                if (seen++ == nIndex)
                    return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
            }
        }
    }
    return std::vector<BYTE>();
}

CGameSprite* MakePartySprite(std::vector<BYTE>& bytes, const char* name)
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
    pSprite->m_sName = name;
    pSprite->m_bGlobal = TRUE;
    return pSprite;
}

// A new game with one party member in AR1000 (as tests/test_gam.cpp).
CGameSprite* NewGameWithOneCharacter(std::vector<BYTE>& bytes, const char* name)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = MakePartySprite(bytes, name);
    pGame->SetCharacterSlot(0, pSprite->GetId());
    return pSprite;
}

// Run only applies an update that arrives from another machine: stand up a
// connected provider whose local player is not the sprite's, and put both
// back on the way out.
class NetworkGuard {
public:
    explicit NetworkGuard(CGameSprite* pSprite)
        : m_network(g_pChitin->cNetwork)
        , m_savedProvider(m_network.m_nServiceProvider)
        , m_savedPlayer(m_network.m_idLocalPlayer)
    {
        m_network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
        m_network.m_idLocalPlayer = 0x11111111;
        REQUIRE(m_network.m_idLocalPlayer != pSprite->m_remotePlayerID);
    }

    ~NetworkGuard()
    {
        m_network.m_nServiceProvider = m_savedProvider;
        m_network.m_idLocalPlayer = m_savedPlayer;
    }

    NetworkGuard(const NetworkGuard&) = delete;
    NetworkGuard& operator=(const NetworkGuard&) = delete;

private:
    CNetwork& m_network;
    const INT m_savedProvider;
    const PLAYER_ID m_savedPlayer;
};

std::vector<LONG> PortraitIcons(const CGameSprite* pSprite)
{
    std::vector<LONG> icons;
    POSITION pos = pSprite->m_portraitIcons.GetHeadPosition();
    while (pos != NULL) {
        icons.push_back(static_cast<LONG>(reinterpret_cast<INT_PTR>(pSprite->m_portraitIcons.GetNext(pos))));
    }
    return icons;
}

}

TEST_CASE("sprite update: Run round-trips a message the ctor captured", "[msg][data]")
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

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pSprite->GetArea() != NULL);

    const SHORT nHitPoints = pSprite->GetBaseStats()->m_hitPoints;
    const LONG nMaxHitPoints = pSprite->GetDerivedStats()->m_nMaxHitPoints;
    const DWORD dwFlags = pSprite->GetBaseStats()->m_flags;
    const DWORD dwGeneralState = pSprite->GetDerivedStats()->m_generalState;
    const CPoint ptPos = pSprite->GetPos();

    {
        NetworkGuard guard(pSprite);
        CMessageSpriteUpdate update(pSprite, 0, pSprite->GetId());
        update.Run();
    }

    // The ctor captured every field group from the sprite and left
    // field_108 zero, so Run re-captures the same values and applies no
    // update: the sprite comes back unchanged.
    CHECK(pSprite->GetBaseStats()->m_hitPoints == nHitPoints);
    CHECK(pSprite->GetDerivedStats()->m_nMaxHitPoints == nMaxHitPoints);
    CHECK(pSprite->GetBaseStats()->m_flags == dwFlags);
    CHECK(pSprite->GetDerivedStats()->m_generalState == dwGeneralState);
    CHECK(pSprite->GetPos().x == ptPos.x);
    CHECK(pSprite->GetPos().y == ptPos.y);
    CHECK(pSprite->GetArea() != NULL);
}

TEST_CASE("sprite update: Run applies the field groups field_108 marks present", "[msg][data]")
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

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pSprite->GetArea() != NULL);

    const DWORD dwFlagsBefore = pSprite->GetBaseStats()->m_flags;

    {
        NetworkGuard guard(pSprite);
        CMessageSpriteUpdate update(pSprite, 0, pSprite->GetId());

        // Hit points (0x8), flags (0x800), portraits (0x20) and the AI
        // type (0x40) arrive with the message; everything else is
        // re-captured from the sprite.
        update.field_108 = 0x8 | 0x800 | 0x20 | 0x40;
        update.m_nHitPoints = 37;
        update.m_nMaxHitPoints = 40;
        update.field_60 = dwFlagsBefore | 0x40;
        update.m_nEnemyAlly = 200;
        update.m_bMoralFailure = TRUE;

        LONG iconIds[2] = { 101, 102 };
        update.field_74 = 2;
        update.field_76 = iconIds;

        update.Run();

        // field_76 stays owned by the test; the message must not free it.
        update.field_76 = NULL;
    }

    CHECK(pSprite->GetBaseStats()->m_hitPoints == 37);
    CHECK(pSprite->GetDerivedStats()->m_nMaxHitPoints == 40);
    CHECK(pSprite->GetBaseStats()->m_flags == (dwFlagsBefore | 0x40));
    CHECK(pSprite->GetAIType().GetEnemyAlly() == 200);
    CHECK(pSprite->m_moraleFailure == TRUE);
    CHECK(PortraitIcons(pSprite) == std::vector<LONG> { 101, 102 });
}
