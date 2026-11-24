// The container messages' codecs: CMessageContainerAddItem (subtype 12,
// survey 121 rows 69-70) and CMessageContainerItems (subtype 13, rows 71-72).
// Both messages are sent to the machine that owns the container; the marshal
// writes the target's remote id pair and the item body, and the unmarshal
// resolves the pair through the game's remote object array and rebuilds the
// CItem(s) from the wire.
//
// The items are real CItems (their constructor goes through the rule tables
// and the ITM resource), so these cases boot the engine as
// tests/test_message_sprite_update.cpp does and start a game with one party
// member, which is the message target.

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

namespace {

namespace fs = std::filesystem;

// CNetwork::SPEC_MSG_HEADER_LENGTH is a runtime constant; mirror its verified
// value (src/CNetwork.cpp) so the packet arrays have a real bound.
constexpr DWORD HEADER_LENGTH = 3;

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

// CGameRemoteObjectArray::Add leaves an empty bucket null (its else branch
// stores NULL), so insert the entry ourselves into the array's public bucket.
void AddRemote(CInfGame* pGame, PLAYER_ID remotePlayerID, LONG remoteObjectID, LONG localObjectID)
{
    const SHORT arrayIndex = static_cast<SHORT>((remoteObjectID >> 16) & 0x7FFF);
    CGameRemoteObjectListEntry* pEntry = new CGameRemoteObjectListEntry();
    pEntry->remotePlayerID = remotePlayerID;
    pEntry->remoteObjectID = remoteObjectID;
    pEntry->localObjectID = localObjectID;
    pEntry->pNext = pGame->m_cRemoteObjectArray.m_pArray[arrayIndex];
    pGame->m_cRemoteObjectArray.m_pArray[arrayIndex] = pEntry;
}

// The message target: the party's sprite, given a known remote pair and a
// remote-array entry pointing back at it, so a marshal/unmarshal round trip
// resolves to the same local id.
CGameSprite* MakeTarget(std::vector<BYTE>& cre, CInfGame* pGame)
{
    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    pSprite->m_remotePlayerID = 7;
    pSprite->m_remoteObjectID = 0x1001;
    AddRemote(pGame, 7, 0x1001, pSprite->GetId());
    return pSprite;
}

} // namespace

TEST_CASE("msg: CMessageContainerAddItem::MarshalMessage writes the item body", "[msg][network][data]")
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
    CGameSprite* pSprite = MakeTarget(cre, pGame);

    CItem item(CResRef("00AMUL01"), 1, 2, 3, 5, 0x40);
    CMessageContainerAddItem message(item, 3, FALSE, 10, pSprite->GetId());

    BYTE packet[HEADER_LENGTH + 0x40];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    // 0x18 + the eight-character resref.
    REQUIRE(dwSize == 0x18 + 8);
    PLAYER_ID player = 0;
    LONG remoteObject = 0;
    SHORT slot = 0;
    WORD uses[3] = { 0, 0, 0 };
    WORD wear = 0;
    DWORD flags = 0;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    std::memcpy(&slot, packet + HEADER_LENGTH + 8, sizeof(slot));
    std::memcpy(uses, packet + HEADER_LENGTH + 0xC + 8, sizeof(uses));
    std::memcpy(&wear, packet + HEADER_LENGTH + 0xC + 8 + 6, sizeof(wear));
    std::memcpy(&flags, packet + HEADER_LENGTH + 0xC + 8 + 6 + 2, sizeof(flags));
    CHECK(player == 7);
    CHECK(remoteObject == 0x1001);
    CHECK(slot == 3);
    CHECK(packet[HEADER_LENGTH + 10] == 0);
    CHECK(packet[HEADER_LENGTH + 0xB] == 8);
    CHECK(std::memcmp(packet + HEADER_LENGTH + 0xC, "00AMUL01", 8) == 0);
    // The wire carries what GetUsageCount reports per ability, not the raw
    // m_useCount members: an item with fewer abilities reports zero there.
    CHECK(uses[0] == item.GetUsageCount(0));
    CHECK(uses[1] == item.GetUsageCount(1));
    CHECK(uses[2] == item.GetUsageCount(2));
    CHECK(wear == item.m_wear);
    CHECK(flags == item.m_flags);
}

TEST_CASE("msg: CMessageContainerAddItem::MarshalMessage zeroes the size for an unknown target", "[msg][network][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    CItem item(CResRef("00AMUL01"), 1, 2, 3, 5, 0x40);
    CMessageContainerAddItem message(item, 3, FALSE, 10, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + 0x40];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0x10;
    message.MarshalMessage(&pPacket, &dwSize);

    CHECK(dwSize == 0);
    for (BYTE nByte : packet) {
        CHECK(nByte == 0xCD);
    }
}

TEST_CASE("msg: CMessageContainerAddItem codec round-trips", "[msg][network][data]")
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
    CGameSprite* pSprite = MakeTarget(cre, pGame);

    CItem item(CResRef("00AMUL01"), 1, 2, 3, 5, 0x40);
    CMessageContainerAddItem sent(item, 3, TRUE, 10, pSprite->GetId());

    BYTE packet[HEADER_LENGTH + 0x40];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);
    REQUIRE(dwSize == 0x18 + 8);

    CMessageContainerAddItem received(item, 0, FALSE, 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(received.GetTarget() == pSprite->GetId());
    CHECK(received.m_slotNum == 3);
    CHECK(received.m_bCompressContainer == TRUE);
    CHECK(received.m_item.cResRef == CResRef("00AMUL01"));
    CHECK(received.m_item.m_useCount1 == item.GetUsageCount(0));
    CHECK(received.m_item.m_useCount2 == item.GetUsageCount(1));
    CHECK(received.m_item.m_useCount3 == item.GetUsageCount(2));
    CHECK(received.m_item.m_wear == item.m_wear);
    CHECK(received.m_item.m_flags == item.m_flags);
}

TEST_CASE("msg: CMessageContainerAddItem::UnmarshalMessage rejects an unknown remote pair", "[msg][network][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    CItem item(CResRef("00AMUL01"), 1, 2, 3, 5, 0x40);
    CMessageContainerAddItem message(item, 3, FALSE, 10, 44);

    BYTE packet[HEADER_LENGTH + 0x20];
    std::memset(packet, 0, sizeof(packet));
    PLAYER_ID player = 9;
    LONG remoteObject = 0x1999;
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));

    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == FALSE);
    CHECK(message.GetTarget() == 44);
    CHECK(message.m_slotNum == 3);
    CHECK(message.m_item.cResRef == CResRef("00AMUL01"));
}
