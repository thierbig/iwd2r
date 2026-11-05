#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSound.h"
#include "CGameSpawning.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMemINI.h"
#include "CMessage.h"
#include "CMoveList.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenJournal.h"
#include "CScreenMap.h"
#include "CScreenOptions.h"
#include "CScreenSpellbook.h"
#include "CScreenStore.h"
#include "CScreenWorld.h"
#include "CSpawn.h"
#include "CUIControlBase.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVisibility.h"
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
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef IWD2R_EXE
#error IWD2R_EXE must be defined by tests/CMakeLists.txt
#endif
#ifndef IWD2R_SOURCE_DIR
#error IWD2R_SOURCE_DIR must be defined by tests/CMakeLists.txt
#endif

// The game file (GAM) and the character export (CHR), on the engine in this
// process (NewGame, a sprite from CHARBASE.CRE, the game's own save and load
// functions) and end to end (scenarios/world_entry.txt through
// Party Formation's Done). Every engine works on a private view of the
// install (tests/data_view.h).

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

uint16_t Le16(const std::string& d, size_t at)
{
    return static_cast<uint16_t>(uint16_t(static_cast<unsigned char>(d[at])) | (uint16_t(static_cast<unsigned char>(d[at + 1])) << 8));
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

// ---- The engine in this process ----------------------------------------

// The engine as iwd2r_main builds it, on a private view of the install that
// lives until the process ends (as tests/test_cre.cpp does). Built once and
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
    // SaveGame copies into mpsave/default, which an install made without
    // saves may lack.
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
    // CChitin::InitInstance (0x790080) without its threads: every engine
    // loads its panels, which UpdatePortraitToolTip and the save read.
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
// a drawn frame (CInfGame::sub_59FA00 spins until m_bDisplayStale clears)
// return as soon as the flag is raised.
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

// The CRE of a finished character: the `nIndex`-th "CHR V2.2" file in the
// view's Characters folder, in file order (the install ships 30 premade
// characters). Empty when there is none at that index.
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

// A party sprite from a finished character's CRE, the way character creation's
// Accept leaves it (a global sprite, not yet in a slot).
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

// A new game with one party member made from a finished character, the way
// character creation's Accept leaves it before Party Formation's Done.
CGameSprite* NewGameWithOneCharacter(std::vector<BYTE>& bytes, const char* name)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = MakePartySprite(bytes, name);
    pGame->SetCharacterSlot(0, pSprite->GetId());
    return pSprite;
}

int LoadedAreas(CInfGame* pGame)
{
    int count = 0;
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            count++;
    }
    return count;
}

// Drops every message waiting in the global handler, so a test can count the
// ones its own call adds.
void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The tooltip strref of the party portrait of slot `nPortrait` on a screen
// that carries the portraits (CInfGame::UpdatePortraitToolTip 0x5AFE90 writes
// panel 1's control nPortrait of each).
STRREF PortraitToolTip(CBaldurEngine* pEngine, DWORD nPortrait)
{
    CUIPanel* pPanel = pEngine->GetManager()->GetPanel(1);
    REQUIRE(pPanel != nullptr);
    CUIControlBase* pControl = pPanel->GetControl(nPortrait);
    REQUIRE(pControl != nullptr);
    return pControl->GetToolTipStrRef();
}

// How many times `id` is on the area's object lists, counting the lists of
// objects still waiting to be added (AddToArea queues them until the area's
// next update).
int TimesInArea(CGameArea* pArea, LONG id)
{
    int count = 0;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight,
        &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            if (static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos))) == id)
                count++;
        }
    }
    return count;
}

std::string SavedGame()
{
    return ReadFile(Child(Child(g_view, "mpsave"), "default") / "ICEWIND2.GAM");
}

std::string Upper(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

// The files packed into a "SAV V1.0" archive (CDimm::CompressDirectory,
// 0x784A50): per file its name length with the NUL, the name, the
// uncompressed and the compressed size, then the zlib data. Keyed by
// upper-case name; an entry that does not unpack is left out.
std::map<std::string, std::string> UnpackSav(const std::string& sav)
{
    std::map<std::string, std::string> files;
    if (sav.size() < 8 || sav.compare(0, 8, "SAV V1.0") != 0)
        return files;
    size_t at = 8;
    while (at + 4 <= sav.size()) {
        const uint32_t nameLength = Le32(sav, at);
        at += 4;
        if (nameLength == 0 || at + nameLength + 8 > sav.size())
            break;
        const std::string name = sav.substr(at, nameLength - 1);
        at += nameLength;
        const uint32_t size = Le32(sav, at);
        const uint32_t packed = Le32(sav, at + 4);
        at += 8;
        if (at + packed > sav.size())
            break;
        std::string bytes(size, '\0');
        DWORD length = size;
        if (CUtil::Uncompress(reinterpret_cast<BYTE*>(bytes.data()), &length,
                reinterpret_cast<const BYTE*>(sav.data() + at), packed)
                == 0
            && length == size) {
            files[Upper(name)] = bytes;
        }
        at += packed;
    }
    return files;
}

// Every file SaveGame left in mpsave/default, by upper-case name, with the
// files packed in ICEWIND2.SAV listed beside the archive itself.
std::map<std::string, std::string> SavedFiles()
{
    std::map<std::string, std::string> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(Child(g_view, "mpsave"), "default"), ec)) {
        files[Upper(entry.path().filename().string())] = ReadFile(entry.path());
    }
    if (files.count("ICEWIND2.SAV") == 1) {
        for (const auto& [name, bytes] : UnpackSav(files.at("ICEWIND2.SAV")))
            files[name] = bytes;
    }
    return files;
}

// The object id of the sprite in `pArea` whose script name is `name`, or -1.
LONG SpriteId(CGameArea* pArea, const char* name)
{
    LONG found = -1;
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (SHORT n = 0; n <= pArray->m_nMaxArrayIndex && found == -1; n++) {
        const LONG id = (static_cast<LONG>(n) << 16) | static_cast<USHORT>(pArray->m_pArray[n].m_objectId);
        CGameObject* pObject = nullptr;
        if (pArray->m_pArray[n].m_objectId == -1
            || pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS)
            continue;
        if (pObject->GetArea() == pArea && pObject->GetObjectType() == CGameObject::TYPE_SPRITE
            && CString(static_cast<CGameSprite*>(pObject)->GetScriptName()).CompareNoCase(name) == 0)
            found = id;
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }
    return found;
}

// How many objects of each type (CGameObject::TYPE_*) the object array holds
// in `pArea`.
std::map<int, int> ObjectTypesIn(CGameArea* pArea)
{
    std::map<int, int> types;
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (SHORT n = 0; n <= pArray->m_nMaxArrayIndex; n++) {
        const LONG id = (static_cast<LONG>(n) << 16) | static_cast<USHORT>(pArray->m_pArray[n].m_objectId);
        CGameObject* pObject = nullptr;
        if (pArray->m_pArray[n].m_objectId == -1
            || pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS)
            continue;
        if (pObject->GetArea() == pArea)
            types[pObject->GetObjectType()]++;
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }
    return types;
}

// Checks an AREAV9.1 file's creature table (offset 0x64, count 0x68, 0x110
// bytes each): every creature carries its CRE, so a producer whose 64-bit
// handle did not resolve (compat/marshal_ptr.h) shows up here instead of as
// a silent zero block. Returns the creature count.
int CheckAreaCreatures(const std::string& are)
{
    REQUIRE(are.size() >= 0x6A);
    CHECK(are.compare(0, 8, "AREAV9.1") == 0);
    const uint32_t table = Le32(are, 0x64);
    const uint16_t count = Le16(are, 0x68);
    REQUIRE(table + 0x110 * count <= are.size());
    for (uint16_t n = 0; n < count; n++) {
        const uint32_t at = table + 0x110 * n;
        const uint32_t offset = Le32(are, at + 0x88);
        const uint32_t size = Le32(are, at + 0x8C);
        INFO("creature " << n << " " << are.substr(at, 32).c_str());
        CHECK(size > 0);
        REQUIRE(offset + size <= are.size());
        CHECK(are.compare(offset, 8, "CRE V2.2") == 0);
    }
    return count;
}

// Checks the GAMEV2.2 layout CInfGame::Marshal writes.
void CheckGameFile(const std::string& gam)
{
    REQUIRE(gam.size() >= 0xB4);
    CHECK(gam.compare(0, 8, "GAMEV2.2") == 0);
    const uint32_t partyOffset = Le32(gam, 0x20);
    const uint32_t partyCount = Le32(gam, 0x24);
    CHECK(partyOffset == 0xB4);
    REQUIRE(partyOffset + 0x340 * partyCount <= gam.size());
    uint32_t end = partyOffset + 0x340 * partyCount;
    for (uint32_t n = 0; n < partyCount; n++) {
        const uint32_t size = Le32(gam, partyOffset + 0x340 * n + 8);
        CHECK(size > 0);
        CHECK(Le32(gam, partyOffset + 0x340 * n + 4) == end);
        // The member's CRE data, copied from the marshal (a zero block when
        // its handle was unknown, compat/marshal_ptr.h).
        CHECK(gam.compare(end, 8, "CRE V2.2") == 0);
        end += size;
    }
    CHECK(Le32(gam, 0x30) == end);
    CHECK(Le32(gam, 0x34) == 0);
}

// ---- The engine as a child process -------------------------------------

#ifndef _WIN32
struct Run {
    int status;
    std::string log;
    bool world;
};

Run RunScenario(const std::string& view, const fs::path& script)
{
    const fs::path shots = fs::path(view).parent_path() / "shots";
    fs::create_directories(shots);
    const fs::path out = fs::path(view).parent_path() / "log.txt";
    // IWD2R_RS=null: without the resource-server thread, LoadArea's wait
    // loop (0x5A24D0) cannot grant m_nActionProgress by the thread's
    // completion timing, so the progress arithmetic in this scenario is the
    // same every run (docs/followups.md, "Intermittent
    // CProgressBar::AddActionTarget"; the race is the original's too).
    const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_AUDIO=null IWD2R_RS=null IWD2R_SCRIPT=\""
        + script.string()
        + "\" IWD2R_SHOT_DIR=\"" + shots.string() + "\" IWD2_DATA=\"" + view + "\" timeout 150 \"" IWD2R_EXE "\" > \""
        + out.string() + "\" 2>&1";
    const int raw = std::system(full.c_str());
    Run run;
    run.status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
    run.log = ReadFile(out);
    run.world = fs::exists(shots / "world.png");
    return run;
}
#endif

}

TEST_CASE("gam: SaveGame's game file loads back and saves again byte for byte", "[gam][data]")
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
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    REQUIRE(pSprite->GetArea() == pGame->m_pGameAreaMaster);
    // As Party Formation's Done does before it saves: SortLists moves the
    // objects the area load queued onto the lists CGameArea::Marshal walks.
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }

    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const std::map<std::string, std::string> firstFiles = SavedFiles();
    const std::string first = SavedGame();
    CheckGameFile(first);
    REQUIRE(firstFiles.count("AR1000.ARE") == 1);
    // AR1000 (the Targos docks) has creatures; the area-side producers and
    // the eight UnmarshalPtr consumers in CGameArea::Marshal carry them.
    CHECK(CheckAreaCreatures(firstFiles.at("AR1000.ARE")) > 0);
    CHECK(Le32(first, 0x24) == 1);
    CHECK(first.compare(0x58, 6, std::string("AR1000", 6)) == 0);
    CHECK(first.compare(0xB4 + 0x18, 6, std::string("AR1000", 6)) == 0);

    // A second save of the same game writes the same bytes.
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const std::map<std::string, std::string> secondFiles = SavedFiles();
    CHECK(secondFiles.size() == firstFiles.size());
    for (const auto& [name, bytes] : firstFiles) {
        INFO(name);
        REQUIRE(secondFiles.count(name) == 1);
        CHECK(secondFiles.at(name) == bytes);
    }

    // Unmarshal of the written file (through LoadGame, which reads it from
    // the save folder) gives the same master area and party count.
    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CHECK(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    CHECK(pGame->m_nCharacters == 1);
    CHECK(LoadedAreas(pGame) == 1);
}

// Party Formation's Done saves and loads the game back (CScreenSinglePlayer::
// OnMainDoneButtonClick, 0x6629E0); the world starts on the reloaded area.
// SaveGame packs the area files into ICEWIND2.SAV (0x784A50), LoadGame
// unpacks them, and the area comes back from its CGameArea::Marshal
// (0x471630): every door, trigger, container, sound and static, and the
// creatures in Marshal's order (the order that fixes their object ids).
TEST_CASE("gam: SaveGame packs AR1000 into ICEWIND2.SAV and LoadGame brings back every object", "[gam][data]")
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
    NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }
    const std::map<int, int> before = ObjectTypesIn(pGame->m_pGameAreaMaster);
    // AR1000 (difficulty Normal): 35 creatures and the party member, 6
    // doors, 17 triggers, 25 containers, 14 sounds, 9 statics.
    CHECK(before.at(CGameObject::TYPE_DOOR) == 6);
    CHECK(before.at(CGameObject::TYPE_TRIGGER) == 17);

    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const fs::path dir = Child(Child(g_view, "mpsave"), "default");
    CHECK(fs::exists(Child(dir, "ICEWIND2.SAV")));
    CHECK_FALSE(fs::exists(Child(dir, "AR1000.ARE")));
    const std::map<std::string, std::string> packed = UnpackSav(ReadFile(Child(dir, "ICEWIND2.SAV")));
    REQUIRE(packed.count("AR1000.ARE") == 1);
    const std::string are = packed.at("AR1000.ARE");
    REQUIRE(are.size() >= 0xB8);
    CHECK(Le16(are, 0x6A) == 17); // triggers
    CHECK(Le32(are, 0xB4) == 6); // doors
    CHECK(CheckAreaCreatures(are) == 35);

    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CHECK(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    // The reload took the saved area (its header's save time is set).
    CHECK(pGame->m_pGameAreaMaster->m_header.m_lastSaved != 0);
    // The object ids follow from the reload's creation order: the game's
    // AI object, the area's object marker, then the creatures as Marshal
    // wrote them. Hedron (the opening dialog's speaker) is the fifteenth:
    // the original's world_entry recording has him as 0x100010.
    CHECK(SpriteId(pGame->m_pGameAreaMaster, "Hedron") == 0x100010);
    const std::map<int, int> after = ObjectTypesIn(pGame->m_pGameAreaMaster);
    for (const auto& [type, count] : before) {
        INFO("object type " << type);
        CHECK(after.count(type) == 1);
        if (after.count(type) == 1)
            CHECK(after.at(type) == count);
    }
}

// CGameArea::Marshal (0x471630): for the master area the original empties
// field_AAA and then fills it again from the game's move and limbo lists, so
// a save carries a creature that is between areas, in no area's list.
TEST_CASE("gam: SaveGame carries a creature that is in the move list", "[gam][data]")
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
    NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }
    const std::map<int, int> before = ObjectTypesIn(pGame->m_pGameAreaMaster);

    // A creature of no area, as one in transit: it is in the object array and
    // a pending move names it, but no area list holds it.
    CCreatureFile creatureData;
    creatureData.SetResRef(CResRef("10GOB"), TRUE, TRUE);
    REQUIRE(creatureData.GetDataSize() > 0);
    CGameSprite* pTraveller = new CGameSprite(creatureData.GetData(),
        creatureData.GetDataSize(),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    creatureData.ReleaseData();
    REQUIRE_FALSE(pTraveller->m_bGlobal);
    CMoveListEntry* pEntry = new CMoveListEntry();
    pEntry->m_nSpriteId = pTraveller->GetId();
    pEntry->m_areaResRef = "AR1001";
    pEntry->m_ptDestination = CPoint(1000, 1000);
    pGame->m_cMoveList.AddTail(pEntry);

    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const fs::path dir = Child(Child(g_view, "mpsave"), "default");
    const std::map<std::string, std::string> packed = UnpackSav(ReadFile(Child(dir, "ICEWIND2.SAV")));
    REQUIRE(packed.count("AR1000.ARE") == 1);
    // AR1000's own 35 creatures and the one the move list hands the area.
    CHECK(CheckAreaCreatures(packed.at("AR1000.ARE")) == 36);

    // The reloaded area holds it, so it came back with the save. The pending
    // move is dropped first, so nothing in it outlives the game it names.
    pGame->m_cMoveList.RemoveAll();
    delete pEntry;
    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    const std::map<int, int> after = ObjectTypesIn(pGame->m_pGameAreaMaster);
    CHECK(after.at(CGameObject::TYPE_SPRITE) == before.at(CGameObject::TYPE_SPRITE) + 1);
}

// The ids on the area's front list, head to tail.
static std::vector<LONG> FrontIds(CGameArea* pArea)
{
    std::vector<LONG> ids;
    POSITION pos = pArea->m_lVertSort.GetHeadPosition();
    while (pos != NULL)
        ids.push_back(static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSort.GetNext(pos))));
    return ids;
}

// The y of object `id`.
static LONG YOf(LONG id)
{
    CGameObject* pObject = nullptr;
    REQUIRE(g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
    const LONG y = pObject->GetPos().y;
    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    return y;
}

// CGameArea::SortLists (0x4794E0) merges the objects AddToArea queued into
// the y-sorted front list: each queued object goes on once, every front
// object stays, and the list is in y order. (Ours once merged the front part
// from the queued part's start, which put a queued object on twice and
// dropped the last front object, AR1000_Door4, from the saved area.)
TEST_CASE("gam: SortLists merges a queued object into the front list once and keeps every front object", "[gam][data]")
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
    CGameArea* pArea = pGame->m_pGameAreaMaster;
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    REQUIRE(pArea->m_lVertSortAdd.IsEmpty());

    // Queue the party member again as AddToArea does: off the front list,
    // onto the queue.
    REQUIRE(TimesInArea(pArea, pSprite->GetId()) == 1);
    pArea->m_lVertSort.RemoveAt(pSprite->GetVertListPos());
    pArea->m_lVertSortAdd.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->GetId())));
    std::vector<LONG> front;
    POSITION pos = pArea->m_lVertSort.GetHeadPosition();
    while (pos != NULL)
        front.push_back(static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSort.GetNext(pos))));
    REQUIRE(front.size() > 2);

    // The queued sprite takes the y of a front object in the middle: on a
    // tie the queued object goes first (the merge takes it while its y is
    // less than or equal to the front object's). The front part, already in
    // y order, keeps its order through the bubble sort.
    const LONG tieId = front[front.size() / 2];
    const LONG tieY = YOf(tieId);
    pSprite->m_pos.y = tieY;
    std::vector<LONG> expected;
    bool placed = false;
    for (LONG id : front) {
        if (!placed && YOf(id) >= tieY) {
            expected.push_back(pSprite->GetId());
            placed = true;
        }
        expected.push_back(id);
    }
    if (!placed)
        expected.push_back(pSprite->GetId());

    pArea->SortLists();
    CHECK(pArea->m_lVertSortAdd.IsEmpty());
    CHECK(FrontIds(pArea) == expected);
    CHECK(TimesInArea(pArea, pSprite->GetId()) == 1);
    for (LONG id : front) {
        INFO("object 0x" << std::hex << id);
        CHECK(TimesInArea(pArea, id) == 1);
    }
    pos = pArea->m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        POSITION here = pos;
        const LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSort.GetNext(pos)));
        CGameObject* pObject = nullptr;
        REQUIRE(pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
        CHECK(pObject->GetVertListPos() == here);
        pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }
}

// A queued object deleted before SortLists (0x4794E0) runs is dropped and
// counts as one fewer queued object (`local_40 = iVar6 - 1`): the front list
// comes out as it was, with no slot of the sort array left unread.
TEST_CASE("gam: SortLists drops a queued object deleted before the sort", "[gam][data]")
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
    NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    CGameArea* pArea = pGame->m_pGameAreaMaster;
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    const std::vector<LONG> front = FrontIds(pArea);
    REQUIRE(front.size() > 2);

    // An object queued for the front list, then deleted from the array.
    CGameObject* pObject = new CGameObject();
    LONG id = 0;
    REQUIRE(pGame->GetObjectArray()->Add(&id, pObject, INFINITE) == CGameObjectArray::SUCCESS);
    pArea->m_lVertSortAdd.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(id)));
    REQUIRE(pGame->GetObjectArray()->Delete(id, CGameObjectArray::THREAD_ASYNCH, NULL, INFINITE) == CGameObjectArray::SUCCESS);
    delete pObject;

    pArea->SortLists();
    CHECK(pArea->m_lVertSortAdd.IsEmpty());
    CHECK(FrontIds(pArea) == front);
}

// M1 Review Focus 2, the two-area save: AR1001 loaded beside AR1000 (its
// creatures carry effect 261, IcewindCGameEffectImmunityToEffectAndResource,
// whose ApplyEffect is ported). AR1001 is in MASTAREA.2DA, so loading it
// makes it the master area (CGameArea 0x472DE0). SaveGame (0x5AC430)
// marshals every loaded area whose header flag 0x1 is clear, so
// ICEWIND2.SAV packs both; the game file names AR1000, where the party
// stands, as the current world area (file offset 0x40, after the
// signature) and AR1001 as the master area (0x58). LoadGame brings both
// back from the save (each header's save time is set), AR1001 with the
// same objects. (Which of the two is the master after the load depends on
// the load order and is not pinned here.)
TEST_CASE("gam: SaveGame writes both loaded areas and LoadGame brings both back from the save", "[gam][data]")
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
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    REQUIRE(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    CGameArea* pFirst = pGame->m_pGameAreaMaster;

    CGameArea* pSecond = pGame->LoadArea(CString("AR1001"), 255, FALSE, FALSE);
    REQUIRE(pSecond != NULL);
    CHECK(pSecond->m_resRef == "AR1001");
    CHECK(pGame->m_pGameAreaMaster == pSecond);
    CHECK(pSprite->GetArea() == pFirst);
    CHECK(LoadedAreas(pGame) == 2);
    CHECK(pSecond->m_header.m_lastSaved == 0);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }
    const std::map<int, int> before = ObjectTypesIn(pSecond);
    REQUIRE(before.count(CGameObject::TYPE_SPRITE) == 1);

    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const std::string gam = SavedGame();
    CheckGameFile(gam);
    CHECK(gam.compare(0x40, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0x58, 6, std::string("AR1001", 6)) == 0);
    const fs::path dir = Child(Child(g_view, "mpsave"), "default");
    const std::map<std::string, std::string> packed = UnpackSav(ReadFile(Child(dir, "ICEWIND2.SAV")));
    REQUIRE(packed.count("AR1000.ARE") == 1);
    REQUIRE(packed.count("AR1001.ARE") == 1);
    CHECK(CheckAreaCreatures(packed.at("AR1000.ARE")) == 35);
    CHECK(CheckAreaCreatures(packed.at("AR1001.ARE")) == before.at(CGameObject::TYPE_SPRITE));

    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    CHECK(LoadedAreas(pGame) == 2);
    CGameArea* pReloaded = pGame->GetArea(CString("AR1001"));
    REQUIRE(pReloaded != NULL);
    CHECK(pReloaded->m_header.m_lastSaved != 0);
    CGameArea* pParty = pGame->GetArea(CString("AR1000"));
    REQUIRE(pParty != NULL);
    CHECK(pParty->m_header.m_lastSaved != 0);
    CHECK(pGame->m_nCharacters == 1);
    CHECK(ObjectTypesIn(pReloaded) == before);
}

// A load-robustness check: a game file whose current area names an area the
// save has no ARE for.
TEST_CASE("gam: loading a game file that names an unsaved second area does not assert", "[gam][data]")
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

    // Review Focus 2: the game file also names AR1001 (as the current world
    // area, offset 0x40), an area the save has no ARE for. CInfGame::
    // Unmarshal loads only the master area AR1000 (the party member stands
    // there), and nothing asserts. A party member standing in AR1001 would
    // load it from the game data instead (CGameSprite::Unmarshal).
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));

    const fs::path file = Child(Child(g_view, "mpsave"), "default") / "ICEWIND2.GAM";
    std::string gam = ReadFile(file);
    CheckGameFile(gam);
    CHECK(gam.compare(0x40, 6, std::string("AR1000", 6)) == 0);
    gam.replace(0x40, 8, std::string("AR1001\0\0", 8));
    std::ofstream(file, std::ios::binary | std::ios::trunc) << gam;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(file.parent_path(), ec)) {
        CHECK(Lower(entry.path().filename().string()) != "ar1001.are");
    }

    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CHECK(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    CHECK(LoadedAreas(pGame) == 1);
    CHECK(pGame->GetArea(CString("AR1001")) == NULL);
}

// CGameJournal::AddEntry (0x4C63B0) builds the entry from its text, time
// and type (the inlined constructor) before it looks for the text in the
// journal. Ours built it with the default constructor, so the text it
// compared and stored was whatever the heap held: an entry was refused at
// random, and with it walk_and_talk's "Your journal has been updated." line
// that CGameDialogReply::Pick pins to the top of the dialog panel, so the
// next reply sat three lines higher in some Windows Sandbox runs.
TEST_CASE("journal: AddEntry stores its text and refuses the same text again", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    // Without the feedback line: the entry is what is under test.
    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD textLevel = pOptions->m_nEffectTextLevel;
    pOptions->m_nEffectTextLevel = textLevel & ~0x10u;

    CGameJournal journal;
    CHECK(journal.AddEntry(1000, 2, 50, 4) == TRUE);
    CHECK(journal.AddEntry(1001, 2, 60, 1) == TRUE);
    // The same text again, in any chapter: refused.
    CHECK(journal.AddEntry(1000, 3, 70, 4) == FALSE);
    REQUIRE(journal.CountEntries() == 2);

    CSavedGameJournalEntry out[2];
    journal.Marshal(out);
    CHECK(out[0].m_strEntry == 1000);
    CHECK(out[0].m_time == 50);
    CHECK(out[0].m_chapter == 2);
    CHECK(out[0].m_type == 4);
    CHECK(out[0].m_character == 0xFF);
    CHECK(out[1].m_strEntry == 1001);
    CHECK(out[1].m_time == 60);
    CHECK(out[1].m_type == 1);

    pOptions->m_nEffectTextLevel = textLevel;
}

TEST_CASE("gam: SetupCharacters twice adds the party once", "[gam][data]")
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

    // Review Focus 4: Party Formation's Done clicked twice runs
    // UpdateCharacterSlots and SetupCharacters twice.
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    const LONG id = pSprite->GetId();

    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pSprite->GetArea() != NULL);
    const CPoint placed = pSprite->GetPos();
    // The first call placed the sprite: it is no longer at (-1, -1).
    CHECK(placed.x != -1);
    CHECK(TimesInArea(pSprite->GetArea(), id) == 1);

    // SetupCharacters zeroes every member's gold unconditionally, so gold
    // set between the calls proves the second call's body ran.
    pSprite->m_baseStats.m_gold = 123;
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    CHECK(pSprite->m_baseStats.m_gold == 0);
    CHECK(pGame->m_nCharacters == 1);
    CHECK(pGame->m_characters[0] == id);
    CHECK(pGame->m_characters[1] == CGameObjectArray::INVALID_INDEX);
    CHECK(LoadedAreas(pGame) == 1);
    CHECK(TimesInArea(pSprite->GetArea(), id) == 1);
    CHECK(pSprite->GetPos().x == placed.x);
    CHECK(pSprite->GetPos().y == placed.y);
}

// 0x5AF160, reached by the world screen's portrait drag
// (CUIControlPortraitWorld::OnLButtonUp 0x77B160): the two slots' ids of
// m_characterPortraits change places, the tooltip of the slot each character
// moved into is rewritten on every screen that shows the portraits, and when
// one of the two slots is 0 the new leader says its "selected" line (sound 2,
// CMessagePlaySound 0x43E170's vtable).
TEST_CASE("gam: SwapCharacters exchanges two portrait slots and the new leader speaks", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    // Two different premade characters, so the tooltips can tell the slots apart.
    std::vector<BYTE> first = FinishedCharacter(0);
    std::vector<BYTE> second = FinishedCharacter(1);
    if (first.empty() || second.empty()) {
        SKIP("[iwd2r-skip] fewer than two CHR V2.2 files in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pFirst = NewGameWithOneCharacter(first, "Probe");
    pGame->UpdateCharacterSlots();
    CGameSprite* pSecond = MakePartySprite(second, "Probe2");
    pGame->SetCharacterSlot(1, pSecond->GetId());
    pGame->UpdateCharacterSlots();
    REQUIRE(pGame->GetNumCharacters() == 2);

    // The premade characters leave m_name at -1, so GetNameRef would answer
    // the slot-derived -2 - slot for both and the tooltip would read the same
    // before and after a swap. Real strrefs make the two characters tell
    // apart in the tooltips, which is what the swapped slots carry.
    pFirst->m_baseStats.m_name = 16484;
    pSecond->m_baseStats.m_name = 16485;
    pGame->UpdateCharacterSlots();

    const LONG nFirstId = pFirst->GetId();
    const LONG nSecondId = pSecond->GetId();
    REQUIRE(pGame->GetCharacterId(0) == nFirstId);
    REQUIRE(pGame->GetCharacterId(1) == nSecondId);
    const STRREF nFirstName = pFirst->GetNameRef();
    const STRREF nSecondName = pSecond->GetNameRef();
    REQUIRE(nFirstName != nSecondName);

    // The eight screens whose panel 1 carries the party portraits.
    CBaldurEngine* const engines[] = {
        g_pBaldurChitin->m_pEngineCharacter,
        g_pBaldurChitin->m_pEngineInventory,
        g_pBaldurChitin->m_pEngineJournal,
        g_pBaldurChitin->m_pEngineMap,
        g_pBaldurChitin->m_pEngineOptions,
        g_pBaldurChitin->m_pEngineSpellbook,
        g_pBaldurChitin->m_pEngineStore,
        g_pBaldurChitin->m_pEngineWorld,
    };

    ClearMessages();

    // Before the swap the tooltips follow the slots.
    REQUIRE(PortraitToolTip(engines[0], 0) == nFirstName);
    REQUIRE(PortraitToolTip(engines[0], 1) == nSecondName);

    pGame->SwapCharacters(0, 1);

    CHECK(pGame->GetCharacterId(0) == nSecondId);
    CHECK(pGame->GetCharacterId(1) == nFirstId);
    CHECK(pGame->GetNumCharacters() == 2);
    for (CBaldurEngine* pEngine : engines) {
        CHECK(PortraitToolTip(pEngine, 0) == nSecondName);
        CHECK(PortraitToolTip(pEngine, 1) == nFirstName);
    }

    // The new leader's selection voice: sound 2, its text, no circle, and the
    // leader's id on both ends (the original sets source and target to it).
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    auto* pMessage = dynamic_cast<CMessagePlaySound*>(handler.m_messageList.GetHead());
    REQUIRE(pMessage != nullptr);
    CHECK(pMessage->m_soundId == 2);
    CHECK(pMessage->m_showText == TRUE);
    CHECK(pMessage->m_showCircle == FALSE);
    CHECK(pMessage->m_sourceId == nSecondId);
    CHECK(pMessage->m_targetId == nSecondId);

    // Swapping back moves both again and has the other leader speak.
    ClearMessages();
    pGame->SwapCharacters(1, 0);

    CHECK(pGame->GetCharacterId(0) == nFirstId);
    CHECK(pGame->GetCharacterId(1) == nSecondId);
    CHECK(PortraitToolTip(engines[0], 0) == nFirstName);
    CHECK(PortraitToolTip(engines[0], 1) == nSecondName);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    pMessage = dynamic_cast<CMessagePlaySound*>(handler.m_messageList.GetHead());
    REQUIRE(pMessage != nullptr);
    CHECK(pMessage->m_sourceId == nFirstId);
    CHECK(pMessage->m_targetId == nFirstId);

    // A third member, and a swap that leaves slot 0 alone: the two ids change
    // places and nothing is sent.
    CGameSprite* pThird = MakePartySprite(first, "Probe3");
    pGame->SetCharacterSlot(2, pThird->GetId());
    pGame->UpdateCharacterSlots();
    REQUIRE(pGame->GetNumCharacters() == 3);
    const LONG nThirdId = pThird->GetId();
    REQUIRE(pGame->GetCharacterId(2) == nThirdId);

    ClearMessages();
    pGame->SwapCharacters(1, 2);

    CHECK(pGame->GetCharacterId(1) == nThirdId);
    CHECK(pGame->GetCharacterId(2) == nSecondId);
    CHECK(pGame->GetCharacterId(0) == nFirstId);
    CHECK(handler.m_messageList.IsEmpty());

    ClearMessages();
}

TEST_CASE("gam: character export overwrites a junk PROBE.CHR and numbers past a valid one", "[gam][data]")
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

    // Review Focus 5. CScreenCreateChar::ExportCharacter (0x614AE0) numbers
    // the name (PROBE00, PROBE01...) only when it is on the export list, and
    // CInfGame::GetImportCharacters (0x5C3240) lists only files that start
    // with "CHR V2.2" (the compare at 0x5C341A). So the original exports over
    // a junk PROBE.CHR (CInfGame::CharacterExport writes through the
    // resource and overwrites it), and a second export, with a valid
    // PROBE.CHR now there, goes to PROBE00.CHR and leaves PROBE.CHR alone.
    const fs::path characters = Child(g_view, "Characters");
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(characters, ec)) {
        if (Lower(entry.path().stem().string()).rfind("probe", 0) == 0)
            fs::remove(entry.path());
    }
    std::ofstream(characters / "PROBE.CHR", std::ios::binary) << "junk";

    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    // Non-zero values where a premade has zeros, so that each header offset
    // below is pinned by a value and not by the zeroed buffer.
    pSprite->m_quickWeapons[1].m_abilityId.m_itemNum = 11;
    pSprite->m_quickWeapons[1].m_abilityId.m_abilityNum = 1;
    pSprite->m_quickItems[0].m_abilityId.m_itemNum = 15;
    pSprite->m_quickItems[0].m_abilityId.m_abilityNum = 2;
    pSprite->m_quickSpells[0].m_abilityId.field_1D = 7;
    pSprite->m_quickSpells[0].m_abilityId.field_1E = 9;
    pSprite->m_quickSpells[8].m_abilityId.field_1D = 3;
    pSprite->m_quickSpells[8].m_abilityId.field_1E = 4;
    pSprite->m_secondarySounds = CResRef("PROBESND");
    memset(pSprite->field_725A, 0, sizeof(pSprite->field_725A));
    memcpy(pSprite->field_725A, "PROBESET", 8);
    REQUIRE(g_pBaldurChitin->m_pEngineCreateChar->ExportCharacter(pSprite));

    const std::string chr = ReadFile(characters / "PROBE.CHR");
    REQUIRE(chr.size() > 0x224);
    CHECK(!fs::exists(Child(characters, "PROBE00.CHR")));

    // The CHR V2.2 header (IESDP), field by field against the sprite.
    CHECK(chr.compare(0, 8, "CHR V2.2") == 0);
    CHECK(chr.compare(0x08, 6, std::string("Probe\0", 6)) == 0);
    CHECK(Le32(chr, 0x28) == 0x224);
    CHECK(Le32(chr, 0x2C) + 0x224 == chr.size());
    CHECK(chr.compare(0x224, 8, "CRE V2.2") == 0);
    for (int n = 0; n < 8; n++) {
        CHECK(static_cast<SHORT>(Le16(chr, 0x30 + 2 * n)) == pSprite->m_quickWeapons[n].m_abilityId.m_itemNum);
        CHECK(static_cast<SHORT>(Le16(chr, 0x40 + 2 * n)) == pSprite->m_quickWeapons[n].m_abilityId.m_abilityNum);
        CHECK(static_cast<BYTE>(chr[0x1A4 + n]) == pSprite->field_3D3A[n]);
    }
    for (int n = 0; n < 9; n++) {
        BYTE resref[RESREF_SIZE];
        pSprite->m_quickSpells[n].m_abilityId.m_res.GetResRef(resref);
        CHECK(memcmp(chr.data() + 0x50 + RESREF_SIZE * n, resref, RESREF_SIZE) == 0);
        CHECK(static_cast<BYTE>(chr[0x98 + n]) == pSprite->m_quickSpells[n].m_abilityId.m_nClass);
        pSprite->m_quickInnates[n].m_abilityId.m_res.GetResRef(resref);
        CHECK(memcmp(chr.data() + 0xAE + RESREF_SIZE * n, resref, RESREF_SIZE) == 0);
        pSprite->m_quickSongs[n].m_abilityId.m_res.GetResRef(resref);
        CHECK(memcmp(chr.data() + 0xF6 + RESREF_SIZE * n, resref, RESREF_SIZE) == 0);
        CHECK(static_cast<int>(Le32(chr, 0x13E + 4 * n)) == pSprite->field_3D14[n]);
        CHECK(static_cast<BYTE>(chr[0x164 + n]) == pSprite->m_quickSpells[n].m_abilityId.field_1D);
        CHECK(static_cast<BYTE>(chr[0x16D + n]) == static_cast<BYTE>(pSprite->m_quickSpells[n].m_abilityId.field_1E));
    }
    CHECK(static_cast<BYTE>(chr[0x164]) == 7);
    CHECK(static_cast<BYTE>(chr[0x16D]) == 9);
    CHECK(static_cast<BYTE>(chr[0x164 + 8]) == 3);
    CHECK(static_cast<BYTE>(chr[0x16D + 8]) == 4);
    CHECK(static_cast<SHORT>(Le16(chr, 0x30 + 2 * 1)) == 11);
    CHECK(static_cast<SHORT>(Le16(chr, 0x40 + 2 * 1)) == 1);
    CHECK(static_cast<SHORT>(Le16(chr, 0xA2)) == 15);
    CHECK(static_cast<SHORT>(Le16(chr, 0xA8)) == 2);
    // The secondary sounds resref (0x17C) and the sound set name (0x184).
    CHECK(chr.compare(0x17C, 8, "PROBESND") == 0);
    CHECK(chr.compare(0x184, 9, std::string("PROBESET\0", 9)) == 0);
    for (int n = 0; n < 3; n++) {
        CHECK(static_cast<SHORT>(Le16(chr, 0xA2 + 2 * n)) == pSprite->m_quickItems[n].m_abilityId.m_itemNum);
        CHECK(static_cast<SHORT>(Le16(chr, 0xA8 + 2 * n)) == pSprite->m_quickItems[n].m_abilityId.m_abilityNum);
    }
    CHECK(static_cast<BYTE>(chr[0x162]) == pSprite->m_nLastSpellbookClassIndex);
    CHECK(static_cast<BYTE>(chr[0x163]) == pSprite->m_nLastSpellbookSpellLevel);
    CHECK(static_cast<BYTE>(chr[0x178]) == pSprite->m_nLastSong);

    REQUIRE(g_pBaldurChitin->m_pEngineCreateChar->ExportCharacter(pSprite));
    CHECK(ReadFile(characters / "PROBE.CHR") == chr);
    const std::string numbered = ReadFile(Child(characters, "PROBE00.CHR"));
    CHECK(numbered.compare(0, 8, "CHR V2.2") == 0);
    CHECK(numbered.size() == chr.size());
}

// The visibility map (CVisibility.cpp): the party member SetupCharacters
// places in AR1000 is added to the area's map (CGameSprite::AddToArea ->
// AddCharacter), whose sweeps run MarkTileLine to its vision ellipse; a move
// (UpDate) clears its bit around the old tile and sweeps around the new one.
// The tile values themselves are compared with the original by the vis/
// oracle cases and the replay's world.explored_count.
TEST_CASE("vis: the party member in AR1000 explores its vision ellipse and moves it", "[vis][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    CVisibilityMap& vis = pArea->m_visibility;
    REQUIRE(vis.m_pMap != NULL);

    auto explored = [&vis]() {
        LONG count = 0;
        for (LONG n = 0; n < vis.m_nMapSize; n++) {
            if (vis.IsTileExplored(n))
                count++;
        }
        return count;
    };

    // One slot holds the member, with bit 0.
    int slots = 0;
    for (LONG id : vis.m_aCharacterIds) {
        if (id == pSprite->GetId())
            slots++;
    }
    REQUIRE(slots == 1);
    CHECK(vis.m_aCharacterIds[0] == pSprite->GetId());

    const CPoint start = pSprite->GetPos();
    const LONG startTile = vis.PointToTile(start);
    CHECK(vis.IsTileExplored(startTile));
    CHECK((vis.m_pMap[startTile] & 0x01) != 0);
    const LONG first = explored();
    // Within the 29 x 21 tiles of the 14 x 10 ellipse.
    CHECK(first > 1);
    CHECK(first <= 29 * 21);

    // Adding the member again keeps its slot and its bit (the original's
    // AddCharacter stops at the slot holding it).
    REQUIRE(vis.AddCharacter(start, pSprite->GetId(), pSprite->GetVisibleTerrainTable()));
    slots = 0;
    for (LONG id : vis.m_aCharacterIds) {
        if (id == pSprite->GetId())
            slots++;
    }
    CHECK(slots == 1);
    CHECK(explored() == first);

    // A move out of the old ellipse: the old tile loses the member's bit but
    // stays explored; the new one is explored and visible.
    const CPoint moved(start.x - 30 * 32, start.y);
    const LONG movedTile = vis.PointToTile(moved);
    vis.UpDate(start, moved, pSprite->GetId(), pSprite->GetVisibleTerrainTable());
    CHECK(vis.IsTileExplored(startTile));
    CHECK((vis.m_pMap[startTile] & 0x01) == 0);
    CHECK(vis.IsTileExplored(movedTile));
    CHECK((vis.m_pMap[movedTile] & 0x01) != 0);
    CHECK(explored() > first);

    // An id that is not on the map changes nothing.
    std::vector<BYTE> before(vis.m_pMap, vis.m_pMap + vis.m_nMapSize);
    vis.UpDate(moved, start, 0x7FFF, pSprite->GetVisibleTerrainTable());
    CHECK(std::equal(before.begin(), before.end(), vis.m_pMap));
}

// INI spawns (CSpawn.cpp): CSpawn::ExecuteSpawn on AR1000 with a section
// built in memory. The area's difficulty band comes from the party level
// (header byte 0x4E) against the two bounds (0x4C, 0x4D); a spawn whose
// area_diff_N key turns its band off spawns nothing and asserts nothing;
// with the band on it creates create_qty creatures at the spawn point.
// (Review Focus 5, CGameSpawning::Spawn's per-total-level budget, has its
// own tests below.)
namespace {

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

}

TEST_CASE("spawn: an INI spawn whose difficulty band is off spawns nothing; on, it spawns its quantity", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    // Band 1: the party level below the first bound.
    pArea->m_header.field_4C = 4;
    pArea->m_header.field_4D = 8;
    pArea->m_header.field_4E = 1;

    // A point well away from the party (2414,897), out of its sight.
    CMemINISection section(CString("probe"));
    section.Set(CString("cre_file"), CString("10JON"));
    section.Set(CString("create_qty"), CString("2"));
    section.Set(CString("spawn_point"), CString("[1100.700:4]"));
    section.Set(CString("area_diff_1"), CString("0"));

    const int before = ObjectsInArea(pArea);
    {
        CSpawn spawn(&section);
        REQUIRE(spawn.DataIsValid);
        CHECK(spawn.ExecuteSpawn(pArea) == 0);
        CHECK(ObjectsInArea(pArea) == before);

        // Bands 2 and 3 stay on (their keys are absent).
        pArea->m_header.field_4E = 6;
        CHECK(spawn.ExecuteSpawn(pArea) == 2);
        CHECK(ObjectsInArea(pArea) == before + 2);
    }

    section.Set(CString("area_diff_3"), CString("0"));
    pArea->m_header.field_4E = 30;
    CSpawn spawn(&section);
    REQUIRE(spawn.DataIsValid);
    CHECK(spawn.ExecuteSpawn(pArea) == 0);
    CHECK(ObjectsInArea(pArea) == before + 2);
}

// CSpawn::Read's spec key and ExecuteSpawn's search count: a bracketed
// object type or a script name both search (GetCountInPoly over the whole
// area, which OfType matches by type and name); a count that reaches spec_qty
// spawns nothing, one below it spawns create_qty; an unclosed bracket leaves
// the spawn invalid.
TEST_CASE("spawn: the spec key's object type gates an INI spawn on the area's count", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    // The area's objects onto the list GetCountInPoly walks.
    pArea->SortLists();
    REQUIRE(pArea->m_lVertSort.GetCount() > 1);

    auto section = [](const char* spec, const char* qty) {
        auto* p = new CMemINISection(CString("probe"));
        p->Set(CString("cre_file"), CString("10JON"));
        p->Set(CString("create_qty"), CString("2"));
        p->Set(CString("spawn_point"), CString("[1100.700:4]"));
        p->Set(CString("spec"), CString(spec));
        p->Set(CString("spec_qty"), CString(qty));
        return p;
    };

    {
        std::unique_ptr<CMemINISection> p(section("[3.4.5.6.7.8.9.10]", "1"));
        CSpawn spawn(p.get());
        REQUIRE(spawn.DataIsValid);
        CHECK(spawn.field_13E == TRUE);
        CHECK(spawn.mSearchType.m_nEnemyAlly == 3);
        CHECK(spawn.mSearchType.m_nGeneral == 4);
        CHECK(spawn.mSearchType.m_nRace == 5);
        CHECK(spawn.mSearchType.m_nSubRace == 6);
        CHECK(spawn.mSearchType.m_nClass == 7);
        CHECK(spawn.mSearchType.m_nSpecific == 8);
        CHECK(spawn.mSearchType.m_nGender == 9);
        CHECK(spawn.mSearchType.m_nAlignment == 10);
    }

    const int before = ObjectsInArea(pArea);
    {
        std::unique_ptr<CMemINISection> p(section("[0.0.0.0.0.0.0.0]", "1"));
        CSpawn spawn(p.get());
        REQUIRE(spawn.DataIsValid);
        CHECK(spawn.ExecuteSpawn(pArea) == 0);
        CHECK(ObjectsInArea(pArea) == before);
    }
    {
        // A name no object in the area carries: the count stays 0.
        std::unique_ptr<CMemINISection> p(section("  NoSuchProbe  ", "1"));
        CSpawn spawn(p.get());
        REQUIRE(spawn.DataIsValid);
        CHECK(spawn.field_13E == TRUE);
        CHECK(spawn.mSearchType.m_sName == "NoSuchProbe");
        CHECK(spawn.ExecuteSpawn(pArea) == 2);
        CHECK(ObjectsInArea(pArea) == before + 2);
    }
    {
        std::unique_ptr<CMemINISection> p(section("[0.0.0.0.0.0.0.0]", "1000"));
        CSpawn spawn(p.get());
        REQUIRE(spawn.DataIsValid);
        CHECK(spawn.ExecuteSpawn(pArea) == 2);
        CHECK(ObjectsInArea(pArea) == before + 4);
    }
    {
        std::unique_ptr<CMemINISection> p(section("[1.2", "1"));
        CSpawn spawn(p.get());
        CHECK_FALSE(spawn.DataIsValid);
    }
}

// ARE spawn points (CGameSpawning::Spawn, 0x4C9CE0) on AR1000. There is no
// per-level table: the budget is m_difficulty * total level / 100 against
// the running sum of the creatures' m_nXP (the CRE's current-XP field, the
// creature's power). The first creature always spawns; more of the picked
// kind spawn while the sum stays within the budget, up to m_maxTypeNum. So
// a total level whose budget covers no creature spawns exactly one and
// asserts nothing (Review Focus 5). The gates (activated, the hour's bit,
// the point's own tile visible) stop it before anything spawns.
namespace {

std::vector<LONG> SpritesInArea(CGameArea* pArea)
{
    std::vector<LONG> ids;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight,
        &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            ids.push_back(static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos))));
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// The resrefs of the sprites in `after` but not in `before`.
std::vector<std::string> NewSprites(const std::vector<LONG>& before, const std::vector<LONG>& after)
{
    std::vector<LONG> added;
    std::set_difference(after.begin(), after.end(), before.begin(), before.end(), std::back_inserter(added));
    std::vector<std::string> names;
    for (LONG id : added) {
        CGameObject* pObject = nullptr;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }
        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            CString name;
            static_cast<CGameSprite*>(pObject)->m_resRef.CopyToString(name);
            names.push_back(std::string(name));
        }
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }
    return names;
}

// A spawn point at (1100, 700), out of the party's sight (2414, 897),
// active at every hour, not spawn-once.
CAreaFileRandomMonsterSpawningPoint SpawnPoint(std::initializer_list<const char*> creatures, WORD difficulty, WORD maxTypeNum)
{
    CAreaFileRandomMonsterSpawningPoint point;
    strncpy(reinterpret_cast<char*>(point.m_scriptName), "ProbeSpawn", SCRIPTNAME_SIZE);
    point.m_posX = 1100;
    point.m_posY = 700;
    WORD n = 0;
    for (const char* creature : creatures) {
        memcpy(point.m_randomCreature[n++], CResRef(creature).GetResRef(), RESREF_SIZE);
    }
    point.m_randomCreatureNum = n;
    point.m_difficulty = difficulty;
    point.m_maxTypeNum = maxTypeNum;
    point.m_activated = 1;
    point.m_timeOfDayVisible = 0xFFFFFF;
    return point;
}

}

TEST_CASE("spawn: a spawn point's budget is difficulty times the total level over the creatures' power", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    REQUIRE_FALSE(pArea->m_visibility.IsTileVisible(pArea->m_visibility.PointToTile(CPoint(1100, 700))));

    // The budget against the creatures' power.
    {
        // 10GOB's power (CRE current XP) is 1, 12GOBSH1's 3.
        struct Row {
            const char* creature;
            WORD difficulty;
            BYTE level;
            WORD maxTypeNum;
            size_t spawned;
        };
        const Row rows[] = {
            // Level 0 with no party summed in is impossible here (0 means "sum
            // the party"), so the empty budget comes from difficulty 0.
            { "10GOB", 0, 20, 10, 1 }, // budget 0: the first creature only
            { "10GOB", 100, 1, 10, 1 }, // budget 1
            { "10GOB", 100, 3, 10, 3 }, // budget 3
            { "10GOB", 50, 7, 10, 3 }, // budget 3 (350 / 100)
            { "10GOB", 100, 9, 10, 9 }, // budget 9
            { "10GOB", 100, 20, 10, 10 }, // capped by m_maxTypeNum
            { "10GOB", 100, 20, 0, 0 }, // m_maxTypeNum 0: nothing
            { "12GOBSH1", 100, 2, 10, 1 }, // budget 2 < 3: the first only
            { "12GOBSH1", 100, 6, 10, 2 }, // budget 6 = 2 * 3: stops on equal
            { "12GOBSH1", 100, 8, 10, 2 }, // budget 8: a third (9) is over
        };
        for (const Row& row : rows) {
            INFO(row.creature << " difficulty " << row.difficulty << " level " << int(row.level) << " max "
                              << row.maxTypeNum);
            CAreaFileRandomMonsterSpawningPoint point = SpawnPoint({ row.creature }, row.difficulty, row.maxTypeNum);
            CGameSpawning* pSpawning = new CGameSpawning(pArea, &point);
            const std::vector<LONG> before = SpritesInArea(pArea);
            pSpawning->Spawn(CPoint(2414, 897), row.level);
            const std::vector<std::string> added = NewSprites(before, SpritesInArea(pArea));
            CHECK(added.size() == row.spawned);
            for (const std::string& name : added) {
                CHECK(name == row.creature);
            }
            // Set whenever it ran to the end.
            CHECK(pSpawning->field_136 == 1);
        }
    }

    // Level 0 sums the party's levels.
    {
        const BYTE party = pSprite->m_derivedStats.GetLevel();
        REQUIRE(party >= 1);
        CAreaFileRandomMonsterSpawningPoint point = SpawnPoint({ "10GOB" }, 100, 40);
        CGameSpawning* pSpawning = new CGameSpawning(pArea, &point);
        const std::vector<LONG> before = SpritesInArea(pArea);
        pSpawning->Spawn(CPoint(2414, 897), 0);
        CHECK(NewSprites(before, SpritesInArea(pArea)).size() == party);
    }

    // A rejected pick falls back to the first creature.
    {
        // Budget 0: if rand picks 12GOBSH1 it is over budget at the first
        // place and the loop restarts with 10GOB, which always spawns; if it
        // picks 10GOB that spawns and a second is over. Either way, one 10GOB.
        for (int n = 0; n < 8; n++) {
            CAreaFileRandomMonsterSpawningPoint point = SpawnPoint({ "10GOB", "12GOBSH1" }, 0, 10);
            CGameSpawning* pSpawning = new CGameSpawning(pArea, &point);
            const std::vector<LONG> before = SpritesInArea(pArea);
            pSpawning->Spawn(CPoint(2414, 897), 20);
            const std::vector<std::string> added = NewSprites(before, SpritesInArea(pArea));
            REQUIRE(added.size() == 1);
            CHECK(added[0] == "10GOB");
        }
    }

    // The gates spawn nothing and leave the point unspent.
    {
        CTimerWorld& timer = pGame->m_worldTime;
        const ULONG saved = timer.m_gameTime;
        // Hour 13 of the next day.
        timer.m_gameTime = (timer.m_gameTime / CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) * CTimerWorld::TIMESCALE_MSEC_PER_DAY
            + 13 * CTimerWorld::TIMESCALE_MSEC_PER_HOUR + 10;
        REQUIRE(timer.GetCurrentHour() == 13);

        CAreaFileRandomMonsterSpawningPoint point = SpawnPoint({ "10GOB" }, 100, 3);
        point.m_timeOfDayVisible = 0xFFFFFF & ~(1u << 13);
        CGameSpawning* pSpawning = new CGameSpawning(pArea, &point);
        std::vector<LONG> before = SpritesInArea(pArea);
        pSpawning->Spawn(CPoint(2414, 897), 3);
        CHECK(NewSprites(before, SpritesInArea(pArea)).empty());
        CHECK(pSpawning->field_136 == 0);

        // The hour's bit on: it spawns.
        pSpawning->m_spawningObject.m_timeOfDayVisible = 1u << 13;
        pSpawning->Spawn(CPoint(2414, 897), 3);
        CHECK(NewSprites(before, SpritesInArea(pArea)).size() == 3);
        CHECK(pSpawning->field_136 == 1);

        // Spawn-once (m_wFlags bit 0) after it spawned: nothing more.
        pSpawning->m_spawningObject.m_wFlags = 1;
        before = SpritesInArea(pArea);
        pSpawning->Spawn(CPoint(2414, 897), 3);
        CHECK(NewSprites(before, SpritesInArea(pArea)).empty());

        // Not activated.
        point.m_timeOfDayVisible = 0xFFFFFF;
        point.m_activated = 0;
        CGameSpawning* pInactive = new CGameSpawning(pArea, &point);
        before = SpritesInArea(pArea);
        pInactive->Spawn(CPoint(2414, 897), 3);
        CHECK(NewSprites(before, SpritesInArea(pArea)).empty());
        CHECK(pInactive->field_136 == 0);

        timer.m_gameTime = saved;
    }

    // A point in the party's sight spawns nothing.
    {
        const CPoint start = pSprite->GetPos();
        REQUIRE(pArea->m_visibility.IsTileVisible(pArea->m_visibility.PointToTile(start)));
        CAreaFileRandomMonsterSpawningPoint point = SpawnPoint({ "10GOB" }, 100, 3);
        point.m_posX = static_cast<WORD>(start.x);
        point.m_posY = static_cast<WORD>(start.y);
        CGameSpawning* pSpawning = new CGameSpawning(pArea, &point);
        const std::vector<LONG> before = SpritesInArea(pArea);
        pSpawning->Spawn(start, 3);
        CHECK(NewSprites(before, SpritesInArea(pArea)).empty());
        CHECK(pSpawning->field_136 == 0);
    }
}

TEST_CASE("spawn: a spawn point lets the party save (0x47C830)", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    // CGameSpawning's vtable (0x84C5B0) has 0x47C830 in slot 0x28:
    // strError -1, TRUE, so the area does not refuse saving for it.
    CAreaFileRandomMonsterSpawningPoint point = SpawnPoint({ "10GOB" }, 100, 1);
    CGameSpawning* pSpawning = new CGameSpawning(pArea, &point);
    STRREF strError = 0;
    CHECK(pSpawning->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));

    strError = 0;
    CHECK(pArea->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
}

#ifndef _WIN32
TEST_CASE("gam: Party Formation's Done saves a game file the engine loads back", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }

    DataView view(DataDir());
    // As StartEngine: SaveGame copies into mpsave/default.
    std::error_code ec;
    if (!fs::exists(Child(view.Path(), "mpsave"), ec)) {
        fs::create_directories(fs::path(view.Path()) / "mpsave", ec);
    }
    const Run run = RunScenario(view.Path(), fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "world_entry.txt");
    const std::string gam = ReadFile(Child(Child(view.Path(), "mpsave"), "default") / "ICEWIND2.GAM");

    INFO("output:\n"
        << run.log);
    CHECK(run.status == 0);
    // LoadGame read the file back and reached the world screen.
    CHECK(run.world);

    CheckGameFile(gam);
    // One party member in the start area where the original puts it
    // (world_entry's recording: AR1000, 2414,897).
    CHECK(Le32(gam, 0x24) == 1);
    CHECK(gam.compare(0x58, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0x40, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0xB4 + 0x18, 6, std::string("AR1000", 6)) == 0);
    CHECK(Le16(gam, 0xB4 + 0x20) == 2414);
    CHECK(Le16(gam, 0xB4 + 0x22) == 897);
}
#endif

// CGameObjectArray::Delete (0x59A530) on an array built by its constructor
// (0x599870): the deleted entry's id becomes -1, so the old index reads as
// DELETED; its slot heads the deleted list and the next Add (0x59A0F0)
// reuses it with the next object id; Delete hands back the object.
TEST_CASE("objects: Delete clears the entry and Add reuses its slot with a new id", "[objects][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    // Delete also drops the index from the game's remote object array
    // (CGameRemoteObjectArray::Delete); without a session that array holds
    // nothing for these indices, so the call changes nothing here.
    CGameObjectArray array(4, 2);
    CGameObject* objects[3] = { new CGameObject(), new CGameObject(), new CGameObject() };
    LONG ids[3];
    for (int n = 0; n < 3; n++)
        REQUIRE(array.Add(&ids[n], objects[n], INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(ids[0] == 0x00000000);
    CHECK(ids[1] == 0x00010001);
    CHECK(ids[2] == 0x00020002);

    CGameObject* pObject = NULL;
    REQUIRE(array.Delete(ids[1], CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(pObject == objects[1]);
    CHECK(array.m_pArray[1].m_objectId == -1);
    CHECK(array.m_deletedListHead == 1);
    CHECK(array.m_pArray[1].m_nextDeletedArrayIndex == -1);

    // The old index is gone; the others are untouched.
    CHECK(array.GetShare(ids[1], CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::DELETED);
    CHECK(array.Delete(ids[1], CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::DELETED);
    CHECK(pObject == NULL);
    REQUIRE(array.GetShare(ids[2], CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(pObject == objects[2]);

    // A share on another thread keeps the entry.
    REQUIRE(array.GetShare(ids[2], CGameObjectArray::THREAD_1, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(array.Delete(ids[2], CGameObjectArray::THREAD_ASYNCH, NULL, INFINITE) == CGameObjectArray::SHARED);
    REQUIRE(array.ReleaseShare(ids[2], CGameObjectArray::THREAD_1, INFINITE) == CGameObjectArray::SUCCESS);

    // Deleting a second entry chains the list: slot 2, then slot 1.
    REQUIRE(array.Delete(ids[2], CGameObjectArray::THREAD_ASYNCH, NULL, INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(array.m_deletedListHead == 2);
    CHECK(array.m_pArray[2].m_nextDeletedArrayIndex == 1);

    LONG reused;
    CGameObject* pNew = new CGameObject();
    REQUIRE(array.Add(&reused, pNew, INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(reused == 0x00020003);
    CHECK(array.m_deletedListHead == 1);
    REQUIRE(array.GetShare(reused, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
    CHECK(pObject == pNew);

    delete objects[1];
    delete objects[2];
}

TEST_CASE("party: AddExperienceParty shares the experience among the living members", "[spawn][data]")
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
    REQUIRE(pGame->GetNumCharacters() == 1);

    // One living member: it gets everything, less its multiclass penalty.
    const INT nPenalty = pSprite->GetMulticlassingPenalty();
    const LONG nExpected = nPenalty > 0 ? (100 - nPenalty) * 2578 / 100 : 2578;
    CHECK(pGame->AddExperienceParty(2578, FALSE, FALSE) == nExpected);

    // The share goes out as an experience effect (104) message to the member.
    const DWORD nBefore = pSprite->m_baseStats.m_xp;
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    pSprite->ProcessEffectList();
    CHECK(pSprite->m_baseStats.m_xp == nBefore + static_cast<DWORD>(nExpected));

    // A dead party counts one member and gives nothing.
    pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
    CHECK(pGame->AddExperienceParty(100, FALSE, FALSE) == 0);
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;
}

TEST_CASE("party: AddXPVar gives the exptable row's experience (case 238)", "[spawn][data]")
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
    REQUIRE(pGame->GetNumCharacters() == 1);

    // EXPTABLE.2DA: Level_1_Easy is 300. No strref (0): no text.
    const INT nPenalty = pSprite->GetMulticlassingPenalty();
    const DWORD nExpected = nPenalty > 0 ? (100 - nPenalty) * 300 / 100 : 300;
    const DWORD nBefore = pSprite->m_baseStats.m_xp;
    pSprite->m_curAction = CAIAction(238, CString("Level_1_Easy"), CPoint(-1, -1), 0, 0);
    CHECK(pSprite->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    pSprite->ProcessEffectList();
    CHECK(pSprite->m_baseStats.m_xp == nBefore + nExpected);

    // A row the table lacks is an error and gives nothing.
    pSprite->m_curAction = CAIAction(238, CString("No_Such_Row"), CPoint(-1, -1), 0, 0);
    CHECK(pSprite->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_ERROR);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    pSprite->ProcessEffectList();
    CHECK(pSprite->m_baseStats.m_xp == nBefore + nExpected);
    pSprite->m_curAction = CAIAction();
}

TEST_CASE("sound: DoAIUpdate plays a periodic sound and draws its next period", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    // An ambient sound in turn (flag bits 0 and 2, not 3), every hour, a
    // period of 2 seconds without variance, two sounds.
    CAreaFileSoundObject object;
    // The class constructor clears it.
    strncpy(reinterpret_cast<char*>(object.m_scriptName), "ProbeSound", SCRIPTNAME_SIZE);
    object.m_posX = 1100;
    object.m_posY = 700;
    memcpy(object.m_soundObject[0], CResRef("AM1000A").GetResRef(), RESREF_SIZE);
    memcpy(object.m_soundObject[1], CResRef("AM1000B").GetResRef(), RESREF_SIZE);
    object.m_soundObjectNum = 2;
    object.m_period = 2;
    object.m_periodVariance = 0;
    object.m_timeOfDayActive = 0xFFFFFF;
    object.m_dwFlags = 0x1 | 0x4;
    CGameSound* pSound = new CGameSound(pArea, &object);
    const LONG counter = pSound->m_id;

    // The constructor (0x4C8B10) starts the count at 0, the period at
    // (2 - 0 + 0) * 15 and the turn at the number of sounds.
    REQUIRE(pSound->m_period == 30);
    REQUIRE(pSound->m_currentSound == 2);

    // Not active: nothing.
    CHECK(pSound->DoAIUpdate(FALSE, counter) == FALSE);
    CHECK(pSound->m_periodCount == 0);

    // 30 updates count up to the period ...
    for (int n = 0; n < 30; n++) {
        CHECK(pSound->DoAIUpdate(TRUE, counter) == FALSE);
    }
    CHECK(pSound->m_periodCount == 30);
    CHECK(pSound->m_currentSound == 2);

    // ... and the next plays the next sound in turn, (2 + 1) % 2, and draws
    // the next period.
    pSound->DoAIUpdate(TRUE, counter);
    CHECK(pSound->m_currentSound == 1);
    CHECK(pSound->m_periodCount == 0);
    CHECK(pSound->m_period == 30);
    pSound->DoAIUpdate(TRUE, counter);
    CHECK(pSound->m_periodCount == 1);

    // A disabled sound (bit 0 clear) does nothing.
    pSound->m_soundObject.m_dwFlags = 0x4;
    pSound->DoAIUpdate(TRUE, counter);
    CHECK(pSound->m_periodCount == 1);

    // RemoveFromArea (0x4C95B0) takes it out of the object array and deletes it.
    pSound->RemoveFromArea();
}

TEST_CASE("sound: DoAIUpdate picks a random sound before it draws the period variance", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    // A random ambient sound (bits 0, 2 and 3), a period of 4 seconds with a
    // variance of 2, two sounds.
    CAreaFileSoundObject object;
    strncpy(reinterpret_cast<char*>(object.m_scriptName), "ProbeRandom", SCRIPTNAME_SIZE);
    object.m_posX = 1100;
    object.m_posY = 700;
    memcpy(object.m_soundObject[0], CResRef("AM1000A").GetResRef(), RESREF_SIZE);
    memcpy(object.m_soundObject[1], CResRef("AM1000B").GetResRef(), RESREF_SIZE);
    object.m_soundObjectNum = 2;
    object.m_period = 4;
    object.m_periodVariance = 2;
    object.m_timeOfDayActive = 0xFFFFFF;
    object.m_dwFlags = 0x1 | 0x4 | 0x8;

    // The draws the constructor and the play make, in order: a seed whose
    // second and third draws give different periods, so the order shows.
    unsigned int seed = 1;
    int draws[3] = {};
    for (;; seed++) {
        srand(seed);
        for (int& draw : draws) {
            draw = rand();
        }
        if (draws[1] % 5 != draws[2] % 5) {
            break;
        }
    }

    srand(seed);
    CGameSound* pSound = new CGameSound(pArea, &object);
    const LONG counter = pSound->m_id;
    // The constructor (0x4C8B10): (4 - 2 + rand() % 5) * 15.
    REQUIRE(pSound->m_period == (4 - 2 + draws[0] % 5) * 15);
    const LONG period = pSound->m_period;
    for (LONG n = 0; n < period; n++) {
        REQUIRE(pSound->DoAIUpdate(TRUE, counter) == FALSE);
    }

    // The play: the sound first (rand() % 2), then the next period
    // (rand() % 5); the turn is left alone.
    pSound->DoAIUpdate(TRUE, counter);
    CHECK(pSound->m_currentSound == 2);
    CHECK(pSound->m_periodCount == 0);
    CHECK(pSound->m_period == (4 - 2 + draws[2] % 5) * 15);

    pSound->RemoveFromArea();
}

TEST_CASE("sound: DoAIUpdate starts a looping sound in its hours and stops it outside", "[spawn][data]")
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
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    // One looping ambient sound (bits 0, 1 and 2), every hour.
    CAreaFileSoundObject object;
    strncpy(reinterpret_cast<char*>(object.m_scriptName), "ProbeLoop", SCRIPTNAME_SIZE);
    object.m_posX = 1100;
    object.m_posY = 700;
    memcpy(object.m_soundObject[0], CResRef("AM1000A").GetResRef(), RESREF_SIZE);
    object.m_soundObjectNum = 1;
    object.m_period = 2;
    object.m_timeOfDayActive = 0xFFFFFF;
    object.m_dwFlags = 0x1 | 0x2 | 0x4;
    CGameSound* pSound = new CGameSound(pArea, &object);
    const LONG counter = pSound->m_id;
    REQUIRE((pSound->m_soundObject.m_dwFlags & 0x2) != 0);

    // In its hours it starts once and then leaves the loop playing; no
    // period is counted.
    CHECK(pSound->m_bLoopPlaying == FALSE);
    CHECK(pSound->DoAIUpdate(TRUE, counter) == FALSE);
    CHECK(pSound->m_bLoopPlaying == TRUE);
    CHECK(pSound->DoAIUpdate(TRUE, counter) == FALSE);
    CHECK(pSound->m_bLoopPlaying == TRUE);

    // Outside them (the constructor's adjusted copy of the hours) it stops.
    pSound->m_timeOfDayActive = 0;
    CHECK(pSound->DoAIUpdate(TRUE, counter) == FALSE);
    CHECK(pSound->m_bLoopPlaying == FALSE);
    CHECK(pSound->DoAIUpdate(TRUE, counter) == FALSE);
    CHECK(pSound->m_bLoopPlaying == FALSE);

    pSound->RemoveFromArea();
}

TEST_CASE("items: CheckItemUsable answers 1 for every item type but 35 and 11", "[spawn][data]")
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
    pSprite->m_derivedStats.m_classMask = CLASSMASK_FIGHTER;
    pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = 0;

    // 0x5BA094: a wand (35) goes on to the class tests. 00WAND02 is not
    // usable by a fighter (bit 0x10 of its flags), and without Use Magic
    // Device there is no second chance: 0.
    CItem wand(CResRef("00WAND02"), 0, 0, 0, 0, 0);
    REQUIRE(wand.GetItemType() == 35);
    CHECK(pGame->CheckItemUsable(pSprite, &wand) == 0);

    // A scroll (11) usable by every class: 1.
    CItem scroll(CResRef("00SCRL01"), 0, 0, 0, 0, 0);
    REQUIRE(scroll.GetItemType() == 11);
    CHECK(pGame->CheckItemUsable(pSprite, &scroll) == 1);

    // Any other type answers 1 at once.
    CItem dagger(CResRef("00DAGG01"), 0, 0, 0, 0, 0);
    REQUIRE(dagger.GetItemType() != 35);
    REQUIRE(dagger.GetItemType() != 11);
    CHECK(pGame->CheckItemUsable(pSprite, &dagger) == 1);
}

TEST_CASE("items: CMessageContainerAddItem keeps its compress flag", "[spawn][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    // The constructor inlined at 0x480844 stores the parameter.
    const CMessageContainerAddItem keep(CItem(), 3, TRUE, 1, 2);
    CHECK(keep.m_bCompressContainer == TRUE);
    CHECK(keep.m_slotNum == 3);
    const CMessageContainerAddItem drop(CItem(), 4, FALSE, 1, 2);
    CHECK(drop.m_bCompressContainer == FALSE);
}

// CSwitchCDStatus::Update (0x54D5A0): the overlay demands its border mosaics,
// the two message mosaics and the two fonts when it is shown, and releases
// them when it is dismissed. The draw branch needs a presentable back buffer,
// so only the two state branches run here.
TEST_CASE("gam: the CD switch overlay demands and releases its resources", "[gam][data]")
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

    NewGameWithOneCharacter(cre, "Probe");

    CSwitchCDStatus& overlay = g_pBaldurChitin->m_cSwitchingCDStatus;
    CString sDrive("D:");

    overlay.Update(TRUE, FALSE, 1, sDrive, TRUE, TRUE, TRUE);
    CHECK(overlay.field_4 != 0);
    CHECK(overlay.field_0 == 0);

    overlay.Update(FALSE, FALSE, 1, sDrive, TRUE, TRUE, TRUE);
    CHECK(overlay.field_4 == 0);
    CHECK(overlay.field_0 == 0);
}

// CGameSprite::CheckReputation (0x737990): a paladin below reputation 70 falls
// - the class switches to fighter in the three AI types, the state gains 0x200,
// the five innate spells are removed and the XP is rewritten to the next-level
// threshold. The derived class levels come back from the base header because
// the tail's Reload rebuilds them (the original's paladin branch does not write
// the base levels; report-T49), so they are asserted to equal the base's.
// A second call is a no-op (the paladin mask is gone) and reputation 80 falls
// nobody. A ranger at 60 moves its levels in the base header (which survives).
TEST_CASE("gam: CheckReputation drops a paladin below reputation 70 to fighter", "[gam][data]")
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
    const CRuleTables& ruleTables = pGame->GetRuleTables();

    // A consistent level-7 paladin: the base header says paladin 7/fighter 0,
    // the derived stats match, reputation 60, the 0x800 bit armed.
    pSprite->m_baseStats.m_characterLevel = 7;
    pSprite->m_baseStats.m_paladinLevel = 7;
    pSprite->m_baseStats.m_fighterLevel = 0;
    pSprite->m_derivedStats.SetClassLevel(CAIOBJECTTYPE_C_PALADIN, 7);
    pSprite->m_derivedStats.SetClassLevel(CAIOBJECTTYPE_C_FIGHTER, 0);
    pSprite->m_typeAI.m_nClassMask |= 0x40;
    pSprite->m_derivedStats.m_nReputation = 60;
    pSprite->m_baseStats.m_flags |= 0x800;
    // The five innate spells the fall removes, so their removal is visible.
    const CResRef* const spells[] = { &CGameSprite::SPIN109, &CGameSprite::SPIN120, &CGameSprite::SPIN121,
        &CGameSprite::SPIN125, &CGameSprite::SPIN152 };
    UINT spellIds[5] = {};
    for (int n = 0; n < 5; n++) {
        REQUIRE(pGame->GetInnateSpells()->Find(*spells[n], spellIds[n]) == TRUE);
        REQUIRE(pSprite->m_innateSpells.Add(spellIds[n], 1, 0, 0) == TRUE);
    }

    pSprite->CheckReputation();

    // The durable effects of the fall.
    CHECK((pSprite->m_baseStats.m_flags & 0x200) != 0);
    CHECK(pSprite->m_typeAI.m_nClass == CAIOBJECTTYPE_C_FIGHTER);
    CHECK(pSprite->m_liveTypeAI.m_nClass == CAIOBJECTTYPE_C_FIGHTER);
    CHECK(pSprite->m_startTypeAI.m_nClass == CAIOBJECTTYPE_C_FIGHTER);
    CHECK((pSprite->m_typeAI.m_nClassMask & 0x40) == 0);
    CHECK((pSprite->m_startTypeAI.m_nClassMask & 0x40) == 0);
    for (UINT nId : spellIds) {
        UINT nIndex = 0;
        CHECK(pSprite->m_innateSpells.Find(nId, nIndex) == FALSE);
    }
    const DWORD nNextLevelXP = ruleTables.GetNextLevelXP(7);
    CHECK(pSprite->m_baseStats.m_xp == nNextLevelXP);
    CHECK(pSprite->m_derivedStats.m_nXP == nNextLevelXP);
    // The tail's Reload rebuilds the derived levels from the base header, which
    // the paladin branch does not touch: they are the base's values again.
    CHECK(pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_PALADIN) == 7u);
    CHECK(pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_FIGHTER) == 0u);

    // A second call finds no paladin: nothing changes.
    const DWORD nFlagsAfter = pSprite->m_baseStats.m_flags;
    const DWORD nXpAfter = pSprite->m_baseStats.m_xp;
    const int nMaskAfter = pSprite->m_typeAI.m_nClassMask;
    pSprite->CheckReputation();
    CHECK(pSprite->m_baseStats.m_flags == nFlagsAfter);
    CHECK(pSprite->m_baseStats.m_xp == nXpAfter);
    CHECK(pSprite->m_typeAI.m_nClassMask == nMaskAfter);

    // A paladin at reputation 80 does not fall.
    pSprite->m_derivedStats.SetClassLevel(CAIOBJECTTYPE_C_PALADIN, 7);
    pSprite->m_derivedStats.SetClassLevel(CAIOBJECTTYPE_C_FIGHTER, 0);
    pSprite->m_typeAI.m_nClassMask |= 0x40;
    pSprite->m_derivedStats.m_nReputation = 80;
    pSprite->m_baseStats.m_flags &= ~0x200u;
    const DWORD nEightyFlags = pSprite->m_baseStats.m_flags;
    const DWORD nEightyXp = pSprite->m_baseStats.m_xp;
    pSprite->CheckReputation();
    CHECK((pSprite->m_baseStats.m_flags & 0x200) == 0);
    CHECK(pSprite->m_baseStats.m_flags == nEightyFlags);
    CHECK(pSprite->m_baseStats.m_xp == nEightyXp);

    // A ranger at 60: its levels move in the base header, where the tail's
    // Reload finds them.
    pSprite->m_baseStats.m_characterLevel = 7;
    pSprite->m_baseStats.m_rangerLevel = 7;
    pSprite->m_baseStats.m_fighterLevel = 0;
    pSprite->m_derivedStats.SetClassLevel(CAIOBJECTTYPE_C_RANGER, 7);
    pSprite->m_derivedStats.SetClassLevel(CAIOBJECTTYPE_C_FIGHTER, 0);
    pSprite->m_typeAI.m_nClassMask |= 0x80;
    pSprite->m_derivedStats.m_nReputation = 60;
    pSprite->m_baseStats.m_flags |= 0x800;
    pSprite->m_baseStats.m_flags &= ~0x400u;
    pSprite->CheckReputation();
    CHECK((pSprite->m_baseStats.m_flags & 0x400) != 0);
    CHECK(pSprite->m_baseStats.m_rangerLevel == 0);
    CHECK(pSprite->m_baseStats.m_fighterLevel == 7);
    CHECK(pSprite->m_baseStats.m_characterLevel == 7);
    CHECK(pSprite->m_typeAI.m_nClass == CAIOBJECTTYPE_C_FIGHTER);
    // NOTE: the typeAI class mask still has the ranger bit after the tail
    // (0x80 observed); the branch does clear it, so the tail's ResetAIType
    // path re-derives it. Not asserted until that path is understood (T49 §6's
    // CheckStatsChange follow-up is the likely place).
}
