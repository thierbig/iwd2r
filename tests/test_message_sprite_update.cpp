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
#include <cstring>
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

// CMessageSpriteUpdate::sub_50D660 (0x50D660): the receive-side area gate.
// Run 0x50D6F0 captures the sprite's area as m_sAreaString, "NO_AREA" when it
// is in none, and both callers (multiplayer-only, so the body is dormant in
// the engine today) ask whether an update applies in the area a party
// transition enters. The sentinel is a case-sensitive strcmp; the area name
// itself is compared case-insensitively (CompareStrings 0x7E8182, lstrcmpi).
TEST_CASE("sprite update: sub_50D660 matches NO_AREA or the area name", "[msg]")
{
    CMessageSpriteUpdate message(NULL, 0, 0);

    SECTION("NO_AREA matches any area, the empty name included")
    {
        message.m_sAreaString = "NO_AREA";
        CHECK(message.sub_50D660(CString("AR1000")) == TRUE);
        CHECK(message.sub_50D660(CString("")) == TRUE);
        CHECK(message.sub_50D660(CString("no_area")) == TRUE);
    }

    SECTION("a real area matches its own name, in any case")
    {
        message.m_sAreaString = "AR1000";
        CHECK(message.sub_50D660(CString("AR1000")) == TRUE);
        CHECK(message.sub_50D660(CString("ar1000")) == TRUE);
    }

    SECTION("a different area, and the empty name, do not match")
    {
        message.m_sAreaString = "AR1000";
        CHECK(message.sub_50D660(CString("AR2000")) == FALSE);
        CHECK(message.sub_50D660(CString("")) == FALSE);
    }

    SECTION("an empty captured area matches only the empty name")
    {
        CHECK(message.sub_50D660(CString("")) == TRUE);
        CHECK(message.sub_50D660(CString("AR1000")) == FALSE);
    }

    SECTION("the sentinel is compared case-sensitively")
    {
        // Not the sentinel, so the body falls through to the name compare:
        // "NO_AREA" as the argument still matches, case-insensitively.
        message.m_sAreaString = "no_area";
        CHECK(message.sub_50D660(CString("NO_AREA")) == TRUE);
        CHECK(message.sub_50D660(CString("AR1000")) == FALSE);
    }
}

// ---------------------------------------------------------------------------
// CMessageSpriteEquipment (subtype 55): its wire record and its apply side.
// ---------------------------------------------------------------------------

namespace {

constexpr DWORD kSpecHeader = 3;

// kNumSlots is a runtime-defined static const BYTE.
constexpr SHORT kNumSlots = 51;

// The items a message or a sprite equipment carries, slot by slot.
struct EquipmentSnapshot {
    BOOL present[kNumSlots] = {};
    CString resRef[kNumSlots];
    WORD useCount1[kNumSlots] = {};
    WORD useCount2[kNumSlots] = {};
    WORD useCount3[kNumSlots] = {};
    WORD wear[kNumSlots] = {};
    DWORD flags[kNumSlots] = {};
};

EquipmentSnapshot SnapshotEquipment(CGameSpriteEquipment* pEquipment)
{
    EquipmentSnapshot snapshot;
    for (SHORT nSlot = 0; nSlot < kNumSlots; nSlot++) {
        CItem* pItem = pEquipment->m_items[nSlot];
        if (pItem == NULL) {
            continue;
        }
        snapshot.present[nSlot] = TRUE;
        snapshot.resRef[nSlot] = pItem->GetResRef().GetResRefStr();
        snapshot.useCount1[nSlot] = pItem->m_useCount1;
        snapshot.useCount2[nSlot] = pItem->m_useCount2;
        snapshot.useCount3[nSlot] = pItem->m_useCount3;
        snapshot.wear[nSlot] = pItem->m_wear;
        snapshot.flags[nSlot] = pItem->m_flags;
    }
    return snapshot;
}

void RequireSameEquipment(const EquipmentSnapshot& expected, CGameSpriteEquipment* pEquipment)
{
    for (SHORT nSlot = 0; nSlot < kNumSlots; nSlot++) {
        INFO("slot " << nSlot);
        CItem* pItem = pEquipment->m_items[nSlot];
        CHECK((pItem != NULL) == (expected.present[nSlot] != FALSE));
        if (pItem == NULL || !expected.present[nSlot]) {
            continue;
        }
        CHECK(pItem->GetResRef() == expected.resRef[nSlot]);
        CHECK(pItem->m_useCount1 == expected.useCount1[nSlot]);
        CHECK(pItem->m_useCount2 == expected.useCount2[nSlot]);
        CHECK(pItem->m_useCount3 == expected.useCount3[nSlot]);
        CHECK(pItem->m_wear == expected.wear[nSlot]);
        CHECK(pItem->m_flags == expected.flags[nSlot]);
    }
}

} // namespace

// CMessageSpriteEquipment::MarshalMessage (0x50B680): the 14-byte record
// (target's remote id pair, item count, selected weapon and ability, weapon
// set) then one block per equipped slot: slot, resref length and bytes, the
// three usage counts, wear and file flags. UnmarshalMessage (0x50B9A0)
// reverses it and resolves the target through the remote object array.
TEST_CASE("sprite equipment: MarshalMessage and UnmarshalMessage round-trip the record", "[msg][data]")
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

    // The record carries the target's remote id pair, which the receive side
    // resolves back through the remote object array.
    pSprite->m_remotePlayerID = 0x1234;
    pSprite->m_remoteObjectID = 0x5678;
    REQUIRE(pGame->GetRemoteObjectArray()->Add(0x1234, 0x5678, pSprite->GetId(), FALSE) == TRUE);

    CMessageSpriteEquipment sent(pSprite, 0, pSprite->GetId());
    const EquipmentSnapshot expected = SnapshotEquipment(&sent.m_equipment);

    BYTE packet[kSpecHeader + 0x800];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPayload = packet + kSpecHeader;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPayload, &dwSize);

    // The payload is written after the header and the size is payload-only.
    REQUIRE(dwSize > 0xE);
    CHECK(packet[0] == 0xCD);
    CHECK(packet[1] == 0xCD);
    CHECK(packet[2] == 0xCD);

    CMessageSpriteEquipment received(NULL, 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, dwSize + kSpecHeader) == TRUE);

    CHECK(received.m_targetId == pSprite->GetId());
    CHECK(received.m_equipment.m_selectedWeapon == sent.m_equipment.m_selectedWeapon);
    CHECK(received.m_equipment.m_selectedWeaponAbility == sent.m_equipment.m_selectedWeaponAbility);
    CHECK(received.m_nWeaponSet == sent.m_nWeaponSet);
    RequireSameEquipment(expected, &received.m_equipment);

    // UnmarshalMessage created the items; the transfer side would consume
    // them, so free them here.
    for (SHORT nSlot = 0; nSlot < kNumSlots; nSlot++) {
        delete received.m_equipment.m_items[nSlot];
        received.m_equipment.m_items[nSlot] = NULL;
    }
}

// CMessageSpriteEquipment::Run (0x50BBC0): take the target sprite, keep the
// launcher the current weapon needs, then hand every message slot over to
// the sprite: replace a different item (unequipping the old one when it is
// the selected weapon or the launcher, and disposing of it), equip the
// equipment slots and select the ability of a replaced selected weapon.
TEST_CASE("sprite equipment: Run equips the message's items on the target", "[msg][data]")
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
    CGameSpriteEquipment* pEquipment = pSprite->GetEquipment();

    // A created character's quick weapon buttons are filled in by character
    // generation; this fixture makes the sprite from the CRE alone, so seed
    // the buttons the tail's sub_726810/GetQuickWeapon rebuild reads for the
    // empty weapon sets (itemNum 10 = the fist).
    for (INT nButton = 0; nButton < 8; nButton++) {
        pSprite->m_quickWeapons[nButton].m_abilityId.m_itemNum = 10;
    }

    // The premade's first weapon set holds 00AX1H01 and its second
    // 00BWLX01, so these two differ and take the replace path.
    CMessageSpriteEquipment message(NULL, 0, pSprite->GetId());
    message.m_nWeaponSet = 0;
    message.m_equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_FIST;
    message.m_equipment.m_selectedWeaponAbility = 0;
    message.m_equipment.m_items[CGameSpriteEquipment::SLOT_WEAPON] = new CItem(CResRef("00SWDL01"), 0, 0, 0, 0, 0);
    message.m_equipment.m_items[CGameSpriteEquipment::SLOT_WEAPON + 2] = new CItem(CResRef("00AX1H01"), 0, 0, 0, 0, 0);

    message.Run();

    // The message's items moved into the equipment and its slots emptied.
    CItem* pWeapon = pEquipment->m_items[CGameSpriteEquipment::SLOT_WEAPON];
    REQUIRE(pWeapon != NULL);
    CHECK(pWeapon->GetResRef() == "00SWDL01");

    CItem* pSecondWeapon = pEquipment->m_items[CGameSpriteEquipment::SLOT_WEAPON + 2];
    REQUIRE(pSecondWeapon != NULL);
    CHECK(pSecondWeapon->GetResRef() == "00AX1H01");

    for (SHORT nSlot = 0; nSlot < kNumSlots; nSlot++) {
        INFO("slot " << nSlot);
        CHECK(message.m_equipment.m_items[nSlot] == NULL);
    }
}

// ---------------------------------------------------------------------------
// CMessageSpriteUpdate::UnmarshalMessage (0x50D290): the decode half of the
// subtype 57 wire record that Run test above applies.
// ---------------------------------------------------------------------------

// CMessageSpriteUpdate::UnmarshalMessage (0x50D290): the 14-byte record
// (target's remote id pair, field-group mask) then only the groups the mask
// bit marks present, in the exe's order.
TEST_CASE("sprite update: UnmarshalMessage decodes the field groups it marks present", "[msg][data]")
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

    pSprite->m_remotePlayerID = 0x999;
    pSprite->m_remoteObjectID = 0x888;
    REQUIRE(pGame->GetRemoteObjectArray()->Add(0x999, 0x888, pSprite->GetId(), FALSE) == TRUE);

    const DWORD nGroups = 0x400 | 0x100 | 2 | 4 | 8 | 0x800 | 0x10 | 0x40 | 0x20 | 1;

    BYTE packet[0x300];
    std::memset(packet, 0, sizeof(packet));

    DWORD cnt = kSpecHeader;
    *reinterpret_cast<PLAYER_ID*>(packet + cnt) = 0x999;
    cnt += sizeof(PLAYER_ID);
    *reinterpret_cast<LONG*>(packet + cnt) = 0x888;
    cnt += sizeof(LONG);
    *reinterpret_cast<DWORD*>(packet + cnt) = nGroups;
    cnt = 0xF;

    // Area name (0x400): length byte then the bytes.
    const char* sArea = "AR1000";
    const BYTE nAreaLength = static_cast<BYTE>(std::strlen(sArea));
    packet[cnt] = nAreaLength;
    cnt++;
    std::memcpy(packet + cnt, sArea, nAreaLength);
    cnt = nAreaLength + 0x10;

    // Sequence (0x100).
    *reinterpret_cast<SHORT*>(packet + cnt) = 7;
    cnt += sizeof(SHORT);

    // Position (0x2).
    *reinterpret_cast<LONG*>(packet + cnt) = 1234;
    *reinterpret_cast<LONG*>(packet + cnt + 4) = -5678;
    cnt += 8;

    // General state (0x4).
    *reinterpret_cast<DWORD*>(packet + cnt) = 0x40;
    cnt += sizeof(DWORD);

    // Hit points (0x8).
    *reinterpret_cast<SHORT*>(packet + cnt) = 37;
    *reinterpret_cast<SHORT*>(packet + cnt + 2) = 40;
    cnt += 4;

    // Base-stat flags (0x800).
    *reinterpret_cast<DWORD*>(packet + cnt) = 0x1234;
    cnt += sizeof(DWORD);

    // AC and field_64..field_6A (0x10): eight shorts.
    for (INT nField = 0; nField < 8; nField++) {
        *reinterpret_cast<SHORT*>(packet + cnt) = static_cast<SHORT>(100 + nField);
        cnt += sizeof(SHORT);
    }

    // Enemy/ally and morale (0x40).
    packet[cnt + 0] = 200;
    packet[cnt + 1] = 201;
    packet[cnt + 2] = 202;
    packet[cnt + 3] = 1;
    cnt += 4;

    // Portrait icons (0x20): count then the ids.
    *reinterpret_cast<SHORT*>(packet + cnt) = 2;
    cnt += sizeof(SHORT);
    *reinterpret_cast<LONG*>(packet + cnt) = 101;
    *reinterpret_cast<LONG*>(packet + cnt + 4) = 102;
    cnt += 8;

    // Path, current path point, destination and move scale (0x1).
    *reinterpret_cast<SHORT*>(packet + cnt) = 3;
    cnt += sizeof(SHORT);
    *reinterpret_cast<LONG*>(packet + cnt + 0) = 10;
    *reinterpret_cast<LONG*>(packet + cnt + 4) = 20;
    *reinterpret_cast<LONG*>(packet + cnt + 8) = 30;
    cnt += 12;
    *reinterpret_cast<SHORT*>(packet + cnt) = 1;
    cnt += sizeof(SHORT);
    *reinterpret_cast<LONG*>(packet + cnt) = 4444;
    *reinterpret_cast<LONG*>(packet + cnt + 4) = -5555;
    cnt += 8;
    packet[cnt] = 5;
    cnt++;
    packet[cnt] = 1;
    cnt++;

    CMessageSpriteUpdate message(NULL, 0, 0);
    REQUIRE(message.UnmarshalMessage(packet, cnt) == TRUE);

    CHECK(message.m_targetId == pSprite->GetId());
    CHECK(message.field_108 == nGroups);
    CHECK(message.m_sAreaString == "AR1000");
    CHECK(message.m_nSequence == 7);
    CHECK(message.m_ptPosition.x == 1234);
    CHECK(message.m_ptPosition.y == -5678);
    CHECK(message.m_dwState == 0x40);
    CHECK(message.m_nHitPoints == 37);
    CHECK(message.m_nMaxHitPoints == 40);
    CHECK(message.field_60 == 0x1234);
    CHECK(message.field_64 == 100);
    CHECK(message.field_66 == 101);
    CHECK(message.field_68 == 102);
    CHECK(message.field_6A == 103);
    CHECK(message.m_nACCrushingMod == 104);
    CHECK(message.m_nACMissileMod == 105);
    CHECK(message.m_nACPiercingMod == 106);
    CHECK(message.m_nACSlashingMod == 107);
    CHECK(message.m_nEnemyAlly == 200);
    CHECK(message.m_nEnemyAllyLive == 201);
    CHECK(message.m_nEnemyAllyStart == 202);
    CHECK(message.m_bMoralFailure == TRUE);
    CHECK(message.field_74 == 2);
    REQUIRE(message.field_76 != NULL);
    CHECK(static_cast<LONG*>(message.field_76)[0] == 101);
    CHECK(static_cast<LONG*>(message.field_76)[1] == 102);
    CHECK(message.m_nPath == 3);
    REQUIRE(message.m_pPath != NULL);
    CHECK(message.m_pPath[0] == 10);
    CHECK(message.m_pPath[1] == 20);
    CHECK(message.m_pPath[2] == 30);
    CHECK(message.m_currPath == 1);
    CHECK(message.m_ptDest.x == 4444);
    CHECK(message.m_ptDest.y == -5555);
    CHECK(message.m_nMoveScale == 5);
    CHECK(message.field_85 == 1);

    // An unknown remote object is refused and leaves the message alone.
    CMessageSpriteUpdate unknown(NULL, 0, 0);
    packet[kSpecHeader + 1] = 0xAA;
    CHECK(unknown.UnmarshalMessage(packet, cnt) == FALSE);
}
