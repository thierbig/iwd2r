#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAICondition.h"
#include "CAIScriptFile.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CButtonData.h"
#include "CChitin.h"
#include "CDerivedStats.h"
#include "CGameArea.h"
#include "CGameAreaNotes.h"
#include "CGameContainer.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CImm.h"
#include "CInfButtonArray.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CItem.h"
#include "CMessage.h"
#include "CParticle.h"
#include "CPathSearch.h"
#include "CResCell.h"
#include "CResMosaic.h"
#include "CRuleTables.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenJournal.h"
#include "CScreenMap.h"
#include "CScreenStore.h"
#include "CScreenWorld.h"
#include "CScreenWorldMap.h"
#include "CTimerWorld.h"
#include "CUIControlScrollBar.h"
#include "CUIControlTextDisplay.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVidFont.h"
#include "CVidMosaic.h"
#include "CWarp.h"
#include "CWeather.h"
#include "Icewind586B70.h"
#include "ScreenLayout.h"
#include "compat/msvc_rand.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

// The ui-misc stream's second round: the character screen's combat page
// (CScreenCharacter::UpdateEquipmentInformation and the sprite's dexterity
// bonus), the quick lists (CGameSprite::CheckQuickLists), the world
// screen's keys, the toolbar and CUtil::FindOneOf, on the engine in this
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

// The party member in slot 0.
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

// The party member of a new one-character game, or NULL. The game is made
// once per process (CInfGame::NewGame wants no characters).
CGameSprite* PartyMember()
{
    static bool made = false;
    if (!made) {
        std::vector<BYTE> cre = FinishedCharacter();
        if (cre.empty())
            return NULL;
        NewGameWithOneCharacter(cre, "Probe");
        made = true;
    }
    return PartySprite();
}

// Every item of a text display, as GetItem joins its lines.
std::vector<std::string> DisplayItems(CUIControlTextDisplay* pText)
{
    std::vector<std::string> items;
    CString sLabel;
    CString sString;
    for (INT n = 0; pText->GetItem(n, sLabel, sString); n++) {
        items.push_back(std::string(static_cast<LPCSTR>(sString)));
    }
    return items;
}

bool Has(const std::vector<std::string>& items, const std::string& item)
{
    return std::find(items.begin(), items.end(), item) != items.end();
}

// The first item that starts with `prefix`, or "".
std::string Starting(const std::vector<std::string>& items, const std::string& prefix)
{
    for (const std::string& item : items) {
        if (item.compare(0, prefix.size(), prefix) == 0)
            return item;
    }
    return std::string();
}

std::string Format(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

std::string Str(STRREF strRef)
{
    return std::string(static_cast<LPCSTR>(CBaldurEngine::FetchString(strRef)));
}

// The character screen's information text (panel 2, control 45) with the
// combat page (tab 61) shown for the party member in slot 0.
std::vector<std::string> CombatPage()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;
    pCharacter->m_nSelectedCharacter = 0;
    pCharacter->CheckMultiPlayerViewableModifyable();
    pCharacter->field_1844 = 1;
    pCharacter->UpdateMainPanel(TRUE);
    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pCharacter->GetManager()->GetPanel(2)->GetControl(45));
    return DisplayItems(pText);
}

}

// 0x723EA0: GetAC's dexterity part on its own, capped by the armour's type
// and zero while blind without Blind Fight (a penalty stays).
TEST_CASE("sprite: the dexterity bonus to the armor class", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CDerivedStats* pStats = pSprite->GetDerivedStats();
    CItem* pArmor = pSprite->m_equipment.m_items[1];
    pSprite->m_equipment.m_items[1] = NULL;
    const SHORT nDex = pStats->m_nDEX;
    const DWORD nState = pStats->m_generalState;
    auto rest = [&]() {
        return pStats->field_C + pStats->field_A + pStats->field_8 + pStats->field_6 + pSprite->sub_723F60() + 10;
    };

    pStats->m_nDEX = 18;
    CHECK(pSprite->sub_723EA0() == 4);
    CHECK(pSprite->GetAC() == rest() + 4);

    // Chain mail (62) caps it at 2; plate (64) at 0.
    CItem chain(CResRef("00CHAN01"), 0, 0, 0, 0, 0);
    REQUIRE(chain.GetItemType() == 62);
    pSprite->m_equipment.m_items[1] = &chain;
    CHECK(pSprite->sub_723EA0() == 2);
    CHECK(pSprite->GetAC() == rest() + 2);
    CItem plate(CResRef("00PLAT01"), 0, 0, 0, 0, 0);
    REQUIRE(plate.GetItemType() == 64);
    pSprite->m_equipment.m_items[1] = &plate;
    CHECK(pSprite->sub_723EA0() == 0);
    pSprite->m_equipment.m_items[1] = NULL;

    // Blind: the bonus goes unless the creature has Blind Fight; a penalty
    // stays.
    pStats->m_generalState |= STATE_BLIND;
    const DWORD nFeats = pSprite->m_baseStats.m_feats[0];
    pSprite->m_baseStats.m_feats[0] &= ~(1u << CGAMESPRITE_FEAT_BLIND_FIGHT);
    CHECK(pSprite->sub_723EA0() == 0);
    pSprite->m_baseStats.m_feats[0] |= 1u << CGAMESPRITE_FEAT_BLIND_FIGHT;
    CHECK(pSprite->sub_723EA0() == 4);
    pSprite->m_baseStats.m_feats[0] &= ~(1u << CGAMESPRITE_FEAT_BLIND_FIGHT);
    pStats->m_nDEX = 6;
    CHECK(pSprite->sub_723EA0() == -2);
    CHECK(pSprite->GetAC() == rest() - 2);

    pSprite->m_baseStats.m_feats[0] = nFeats;
    pStats->m_generalState = nState;
    pStats->m_nDEX = nDex;
    pSprite->m_equipment.m_items[1] = pArmor;
}

// UpdateEquipmentInformation (0x5DE2C0) and its weapon statistics (0x5DF910):
// a long sword in the first weapon slot gives the attack line, the armour
// class with its dexterity part, the number of attacks and the sword's
// damage and critical range.
TEST_CASE("character screen: the combat page of a character with a long sword", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CItem* pOld = pSprite->m_equipment.m_items[43];
    const BYTE nOldWeapon = pSprite->m_equipment.m_selectedWeapon;
    const BYTE nOldAbility = static_cast<BYTE>(pSprite->m_equipment.m_selectedWeaponAbility);
    CItem* pSword = new CItem(CResRef("00SWDL01"), 0, 0, 0, 0, 0x1);
    pSprite->m_equipment.m_items[43] = pSword;
    pSprite->SelectWeaponAbility(43, 0, FALSE, TRUE);
    REQUIRE(pSprite->m_equipment.m_selectedWeapon == 43);

    std::vector<std::string> items = CombatPage();
    INFO([&] {
        std::string all;
        for (const std::string& item : items)
            all += "[" + item + "]\n";
        return all;
    }());

    CDerivedStats* pStats = pSprite->GetDerivedStats();
    CHECK(Has(items, Str(9457))); // "Attack Roll Modifiers"
    CHECK(Has(items, Format("%s: %d", Str(33553).c_str(), pSprite->GetAC())));
    CHECK(Has(items, Format("  %s: %d", Str(31353).c_str(), 10)));
    CHECK(Has(items, Format("%s: %d", Str(9458).c_str(), pStats->m_nNumberOfAttacks)));
    if (pSprite->sub_723EA0() != 0) {
        CHECK(Has(items, Format("  %s: %+d", Str(1151).c_str(), pSprite->sub_723EA0())));
    } else {
        CHECK(Starting(items, Format("  %s: ", Str(1151).c_str())).empty());
    }

    // The main hand's first attack: the base attack bonus and every
    // modifier the page lists.
    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    int nBase = 0;
    int nAttacks = 0;
    int nStep = 0;
    rule.sub_546B60(pSprite, nBase, nAttacks, nStep, TRUE);
    pSword->Demand();
    const ITEM_ABILITY* pAbility = pSword->GetAbility(0);
    REQUIRE(pAbility != NULL);
    const int nFirst = static_cast<SHORT>(rule.GetIterativeAttackBonus(nBase, nStep, 0)
        + pSprite->sub_724170() + pSprite->sub_7240A0()
        + pSprite->sub_73C7E0() + static_cast<WORD>(pSprite->sub_73D420()) + pSprite->sub_73CAD0()
        + pSprite->sub_73CB10(pSword, pAbility)
        + pSprite->sub_73CA20(pSword, pAbility) + pSprite->sub_73CF10(pSword, pAbility) + pSprite->sub_73D080(FALSE)
        + pSprite->sub_73CAE0(pSword, pAbility) + pSprite->sub_73CC40(pSword, pAbility));
    const std::string sMain = Starting(items, Str(734) + ": "); // "Main Hand: "
    REQUIRE_FALSE(sMain.empty());
    const std::string sFirst = Format("%s: %+d", Str(734).c_str(), nFirst);
    CHECK(sMain.compare(0, sFirst.size(), sFirst) == 0);
    CHECK((sMain.size() == sFirst.size() || sMain[sFirst.size()] == '/'));
    CHECK(Starting(items, Format("  %s: %+d", Str(31353).c_str(), rule.GetIterativeAttackBonus(nBase, nStep, 0))).size() > 0);

    // The weapon statistics: "Main Hand - <name>", the dice, the threat.
    CHECK(Has(items, Str(41119))); // "Weapon Statistics"
    // UpdateText trims the line ("Longsword " has a trailing space).
    std::string sHand = Str(734) + " - " + Str(pSword->GetGenericName());
    sHand.erase(sHand.find_last_not_of(' ') + 1);
    CHECK(Has(items, sHand));
    CHECK(Has(items, Format("  %s: %dd%d", Str(39518).c_str(), pAbility->damageDiceCount, pAbility->damageDice)));
    const int nThreat = static_cast<SHORT>(pSword->sub_4E9840(pAbility) + pSprite->GetCriticalHitBonus());
    if (nThreat != 0) {
        CHECK(Has(items, Format("  %s: %d-20 / x%d", Str(41122).c_str(), std::max(1, 20 - nThreat), static_cast<int>(pSword->GetCriticalHitMultiplier()))));
    } else {
        CHECK(Has(items, Format("  %s: 20 / x%d", Str(41122).c_str(), static_cast<int>(pSword->GetCriticalHitMultiplier()))));
    }
    pSword->Release();

    // The saving throws close the page.
    CHECK_FALSE(items.empty());

    // Back to the character's own weapon for the later cases.
    pSprite->SelectWeaponAbility(CGameSpriteEquipment::SLOT_FIST, 0, FALSE, TRUE);
    pSprite->m_equipment.m_items[43] = pOld;
    delete pSword;
    pSprite->SelectWeaponAbility(nOldWeapon, nOldAbility, FALSE, TRUE);
}

namespace {

// The rand() draws from the state `before` to `after`, or -1.
int Draws(unsigned int before, unsigned int after, int limit = 1000)
{
    unsigned int state = before;
    for (int n = 0; n <= limit; n++) {
        if (state == after) {
            return n;
        }
        state = state * 214013u + 2531011u;
    }
    return -1;
}

CAbilityId QuickItemAbility(SHORT nSlot)
{
    // As the inventory screen builds it for a scroll (0x62B0B0).
    CAbilityId abilityId;
    abilityId.m_res = "";
    abilityId.m_itemType = 2;
    abilityId.m_itemNum = nSlot;
    abilityId.m_abilityNum = 0;
    return abilityId;
}

CButtonData Button(const CAbilityId& abilityId, SHORT nCount)
{
    CButtonData buttonData;
    buttonData.m_icon = "PROBE";
    buttonData.m_name = 1;
    buttonData.m_abilityId = abilityId;
    buttonData.m_count = nCount;
    return buttonData;
}

bool Cleared(const CButtonData& buttonData)
{
    return buttonData.m_count == 0
        && buttonData.m_abilityId.m_itemType == -1
        && buttonData.m_icon == ""
        && buttonData.m_name == static_cast<STRREF>(-1)
        && buttonData.m_bDisplayCount == TRUE;
}

}

// CheckQuickLists (0x71ABA0): the matching buttons of every quick list take
// the change; an emptied weapon, item or song is cleared, an emptied spell
// or innate greys out unless removeSpellIfZero; `remove` clears every match;
// only the first quick item and the first six songs count. It draws no rand.
TEST_CASE("sprite: the quick lists follow a spent ability", "[uimisc][quicklists][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(pSprite->m_bGlobal);

    CAbilityId spell;
    spell.m_res = "SPWI112";
    spell.m_itemType = 1;
    spell.m_nClass = 11;
    spell.field_1D = 1;
    CAbilityId other = spell;
    other.m_res = "SPWI104";

    pSprite->m_quickSpells[0] = Button(spell, 2);
    pSprite->m_quickSpells[1] = Button(other, 2);
    pSprite->m_quickSpells[2] = Button(spell, 2);
    pSprite->m_quickInnates[0] = Button(spell, 1);
    pSprite->m_quickWeapons[3] = Button(spell, 1);
    pSprite->field_3D3A[3] = 7;
    pSprite->m_quickItems[0] = Button(spell, 3);
    pSprite->m_quickItems[1] = Button(spell, 3);
    pSprite->m_quickSongs[5] = Button(spell, 1);
    pSprite->m_quickSongs[6] = Button(spell, 1);

    unsigned int before = iwd2r_rand_state();
    pSprite->CheckQuickLists(&spell, -1, FALSE, FALSE);
    CHECK(Draws(before, iwd2r_rand_state()) == 0);

    CHECK(pSprite->m_quickSpells[0].m_count == 1);
    CHECK_FALSE(pSprite->m_quickSpells[0].m_bDisabled);
    CHECK(pSprite->m_quickSpells[1].m_count == 2);
    CHECK(pSprite->m_quickSpells[2].m_count == 1);
    // The innate is spent: greyed out, still there.
    CHECK(pSprite->m_quickInnates[0].m_count == 0);
    CHECK(pSprite->m_quickInnates[0].m_bDisabled);
    CHECK(pSprite->m_quickInnates[0].m_abilityId.m_res == spell.m_res);
    // The weapon button is cleared with its flag.
    CHECK(Cleared(pSprite->m_quickWeapons[3]));
    CHECK(pSprite->field_3D3A[3] == 0);
    // Only the first quick item.
    CHECK(pSprite->m_quickItems[0].m_count == 2);
    CHECK(pSprite->m_quickItems[1].m_count == 3);
    // Songs: the sixth is spent (and kept: songs go only with
    // removeSpellIfZero), the seventh is not checked.
    CHECK(pSprite->m_quickSongs[5].m_count == 0);
    CHECK(pSprite->m_quickSongs[5].m_abilityId.m_res == spell.m_res);
    CHECK(pSprite->m_quickSongs[6].m_count == 1);

    // Spent to zero with removeSpellIfZero: the spells go.
    pSprite->CheckQuickLists(&spell, -5, FALSE, TRUE);
    CHECK(Cleared(pSprite->m_quickSpells[0]));
    CHECK(Cleared(pSprite->m_quickSpells[2]));
    CHECK(pSprite->m_quickSpells[1].m_count == 2);
    CHECK(Cleared(pSprite->m_quickItems[0]));

    // A gain re-enables a greyed spell; `remove` clears it whatever its
    // count.
    pSprite->m_quickSpells[0] = Button(spell, 0);
    pSprite->m_quickSpells[0].m_bDisabled = TRUE;
    pSprite->CheckQuickLists(&spell, 1, FALSE, FALSE);
    CHECK(pSprite->m_quickSpells[0].m_count == 1);
    CHECK_FALSE(pSprite->m_quickSpells[0].m_bDisabled);
    pSprite->CheckQuickLists(&spell, 1, TRUE, FALSE);
    CHECK(Cleared(pSprite->m_quickSpells[0]));
    CHECK(Cleared(pSprite->m_quickItems[1]));

    // A creature outside the party and its allies is left alone.
    pSprite->m_bGlobal = FALSE;
    pSprite->m_quickSpells[0] = Button(spell, 2);
    pSprite->CheckQuickLists(&spell, -1, FALSE, FALSE);
    CHECK(pSprite->m_quickSpells[0].m_count == 2);
    pSprite->m_bGlobal = TRUE;

    for (int n = 0; n < 9; n++) {
        pSprite->m_quickSpells[n] = CButtonData();
        pSprite->m_quickInnates[n] = CButtonData();
        pSprite->m_quickSongs[n] = CButtonData();
    }
    pSprite->m_quickItems[0] = CButtonData();
    pSprite->m_quickItems[1] = CButtonData();
}

// CopySpell (0x62B0B0): writing the last charge of a scroll into the
// spellbook uses it up (CheckIfUsedUp 0x75D450: the scroll vanishes) and
// clears its quick item (CheckQuickLists, removeSpellIfZero).
TEST_CASE("inventory: copying a scroll's last charge removes it and its quick item", "[uimisc][inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    const SHORT nSlot = CGameSpriteEquipment::SLOT_MISC;
    CItem* pOld = pSprite->m_equipment.m_items[nSlot];
    CItem* pScroll = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
    pScroll->Demand();
    const ITEM_ABILITY* pLearn = pScroll->GetAbility(1);
    REQUIRE(pLearn != NULL);
    pScroll->Release();
    pSprite->m_equipment.m_items[nSlot] = pScroll;
    const CAbilityId item = QuickItemAbility(nSlot);
    pSprite->m_quickItems[0] = Button(item, 1);

    CScreenInventory* pInventory = g_pBaldurChitin->m_pEngineInventory;
    pInventory->m_nSelectedCharacter = 0;
    pInventory->m_nRequesterButtonId = 5; // slot 15
    REQUIRE(pInventory->MapButtonIdToInventoryId(5) == nSlot);

    // CheckIfUsedUp on its own: a spent scroll that stacks (5) vanishes
    // without a sound (0x75D942: Unequip, the item deleted, the weapon set
    // reselected through 0x726810). It draws no rand of its own: as many as
    // that sequence run without it. (Those come from the effect list and
    // CheckAdd, other streams' code, and from the character.)
    {
        const SHORT nSpentSlot = static_cast<SHORT>(nSlot + 1);
        CItem* pReference = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
        pReference->SetUsageCount(0, 0);
        pSprite->m_equipment.m_items[nSpentSlot] = pReference;
        unsigned int before = iwd2r_rand_state();
        pSprite->Unequip(nSpentSlot);
        pReference->ReleaseAll();
        delete pReference;
        pSprite->m_equipment.m_items[nSpentSlot] = NULL;
        pSprite->sub_726810(pSprite->m_nWeaponSet);
        const int nSequence = Draws(before, iwd2r_rand_state());
        REQUIRE(nSequence >= 0);

        CItem* pSpent = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
        pSpent->SetUsageCount(0, 0);
        pSprite->m_equipment.m_items[nSpentSlot] = pSpent;
        before = iwd2r_rand_state();
        CHECK(pSprite->CheckIfUsedUp(nSpentSlot, 0));
        CHECK(Draws(before, iwd2r_rand_state()) == nSequence);
        CHECK(pSprite->m_equipment.m_items[nSpentSlot] == NULL);
    }

    pInventory->CopySpell();

    CHECK(pSprite->m_equipment.m_items[nSlot] == NULL);
    CHECK(Cleared(pSprite->m_quickItems[0]));

    pSprite->m_equipment.m_items[nSlot] = pOld;
}

// IdentifyWithScroll (0x62AE70): the identify scroll's last charge goes
// (CheckIfUsedUp 0x75D450) and the requested item is identified.
TEST_CASE("inventory: identifying with the last scroll uses it up", "[uimisc][inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    const SHORT nItemSlot = 16; // button 6
    const SHORT nScrollSlot = 20;
    CItem* pOldItem = pSprite->m_equipment.m_items[nItemSlot];
    CItem* pOldScroll = pSprite->m_equipment.m_items[nScrollSlot];
    CItem* pSword = new CItem(CResRef("00SWDL01"), 0, 0, 0, 0, 0);
    pSprite->m_equipment.m_items[nItemSlot] = pSword;
    pSprite->m_equipment.m_items[nScrollSlot] = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
    REQUIRE(pSprite->FindItemPersonal(CString("SPWI110Z"), 0, FALSE) == nScrollSlot);

    CScreenInventory* pInventory = g_pBaldurChitin->m_pEngineInventory;
    pInventory->m_nSelectedCharacter = 0;
    pInventory->m_nRequesterButtonId = 6;
    REQUIRE(pInventory->MapButtonIdToInventoryId(6) == nItemSlot);

    pInventory->IdentifyWithScroll();
    CHECK(pSprite->m_equipment.m_items[nScrollSlot] == NULL);
    CHECK((pSword->m_flags & 0x1) != 0);

    pSprite->m_equipment.m_items[nItemSlot] = pOldItem;
    pSprite->m_equipment.m_items[nScrollSlot] = pOldScroll;
    delete pSword;
}

// CUtil::FindOneOf (0x781180): the first character of the searched string,
// from `start`, that is one of the separators. With Double Byte Character
// Support on (g_pChitin->field_1A0), a lead byte of the searched string
// skips its trail byte, and the separators lose their lead bytes. It used
// to test the separators' byte at the searched string's index (reading
// past their end), so a trail byte that equals a separator matched.
TEST_CASE("util: FindOneOf skips a double-byte character of the searched string", "[uimisc][util]")
{
    // FindOneOf reads only field_1A0 of the chitin. When another case has
    // started the engine, its threads read g_pChitin: the case changes only
    // that flag on the engine's chitin and puts it back (no main loop runs
    // in the tests, so nothing lays out text meanwhile). Without an engine
    // no thread reads g_pChitin, and a zeroed chitin stands in.
    // IsDBCSLeadByte is the shim's on every platform
    // (platform/win32_stubs.cpp).
    CChitin* pSaved = g_pChitin;
    std::unique_ptr<unsigned char[]> storage;
    if (g_pChitin == NULL) {
        storage.reset(new unsigned char[sizeof(CChitin)]());
        g_pChitin = reinterpret_cast<CChitin*>(storage.get());
    }
    struct Restore {
        CChitin* pChitin;
        int nFlag;
        ~Restore()
        {
            iwd2r_SetTestLeadBytes(nullptr);
            g_pChitin->field_1A0 = nFlag;
            g_pChitin = pChitin;
        }
    } restore { pSaved, g_pChitin->field_1A0 };

    // A single-byte code page.
    g_pChitin->field_1A0 = 0;
    CHECK(CUtil::FindOneOf(CString("a,b.c"), CString(",."), 0) == 1);
    CHECK(CUtil::FindOneOf(CString("a,b.c"), CString(",."), 2) == 3);
    CHECK(CUtil::FindOneOf(CString("abc"), CString(",."), 0) == -1);
    CHECK(CUtil::FindOneOf(CString("a,b"), CString(""), 0) == -1);
    CHECK(CUtil::FindOneOf(CString("a,b"), CString(","), 5) == -1);
    // A long string and one separator: the separator's string is never
    // indexed by the searched string's position.
    CHECK(CUtil::FindOneOf(CString("abcdefghijklmnopqrstuvwxyz;"), CString(";"), 0) == 26);

    // Shift-JIS's lead bytes (0x81-0x9F, 0xE0-0xFC), Double Byte Character
    // Support on.
    static bool leadBytes[256] = {};
    for (int c = 0x81; c <= 0x9F; c++)
        leadBytes[c] = true;
    for (int c = 0xE0; c <= 0xFC; c++)
        leadBytes[c] = true;
    iwd2r_SetTestLeadBytes(leadBytes);
    g_pChitin->field_1A0 = 1;

    // The ',' after the lead byte is its trail byte: the next ',' matches.
    CHECK(CUtil::FindOneOf(CString("\x81,a,b"), CString(","), 0) == 3);
    // Starting on the trail byte, it is a plain character.
    CHECK(CUtil::FindOneOf(CString("\x81,a,b"), CString(","), 1) == 1);
    // The separators lose their lead bytes.
    CHECK(CUtil::FindOneOf(CString("x\x81y,"), CString("\x81,"), 0) == 3);
    CHECK(CUtil::FindOneOf(CString("x,"), CString("\x81"), 0) == -1);
    CHECK(CUtil::FindOneOf(CString("abcdefghijklmnopqrstuvwxyz;"), CString(";"), 0) == 26);
}

// SelectToolbar (0x5ADAE0): one selected party member gives the action bar
// its customizable button types (0x595E70) and its weapon set (0x595F70)
// before state 114.
TEST_CASE("toolbar: a selected character's buttons and weapon set reach the action bar", "[uimisc][toolbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        pGame->SelectCharacter(pSprite->GetId(), FALSE);
    }
    REQUIRE(pGame->GetGroup()->GetCount() == 1);

    const int saved[9] = { pSprite->field_3D14[0], pSprite->field_3D14[1], pSprite->field_3D14[2],
        pSprite->field_3D14[3], pSprite->field_3D14[4], pSprite->field_3D14[5], pSprite->field_3D14[6],
        pSprite->field_3D14[7], pSprite->field_3D14[8] };
    const BYTE nSavedSet = pSprite->m_nWeaponSet;
    for (int n = 0; n < 9; n++) {
        pSprite->field_3D14[n] = 30 + n;
    }
    pSprite->m_nWeaponSet = 2;

    CInfButtonArray* pButtonArray = pGame->GetButtonArray();
    pButtonArray->m_nSelectedWeaponSet = 0;
    pGame->SelectToolbar();

    for (int n = 0; n < 9; n++) {
        CHECK(pButtonArray->m_customizableButtonTypes[n] == 30 + n);
    }
    CHECK(pButtonArray->m_nSelectedWeaponSet == 2);
    CHECK(pButtonArray->m_nState == 114);

    for (int n = 0; n < 9; n++) {
        pSprite->field_3D14[n] = saved[n];
    }
    pSprite->m_nWeaponSet = nSavedSet;
    pGame->SelectToolbar();
}

// UpdateButtons (0x58A340), the quick weapons (types 60-67): the button of
// the selected weapon gets the selected frame (field_1CC) only while a
// weapon button is the selected button (0x58D2C0 tests m_nSelectedButton).
// With none selected it keeps an unselected button's look (render round 5
// found ours testing the button's own type: the red frame where the
// original draws the green highlight).
TEST_CASE("toolbar: the selected weapon's button is framed only while a weapon button is selected", "[uimisc][toolbar][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        pGame->SelectCharacter(pSprite->GetId(), FALSE);
    }
    pGame->SelectToolbar();
    CInfButtonArray* pButtonArray = pGame->GetButtonArray();
    INT savedTypes[12];
    memcpy(savedTypes, pButtonArray->m_buttonTypes, sizeof(savedTypes));
    const INT nSavedSelected = pButtonArray->m_nSelectedButton;

    // Button 0: the current weapon set's weapon, the selected weapon.
    const INT nType = 60 + 2 * pSprite->m_nWeaponSet;
    pButtonArray->m_buttonTypes[0] = nType;

    pButtonArray->m_nSelectedButton = nType;
    pButtonArray->UpdateButtons();
    REQUIRE(pButtonArray->m_buttonArray[0].field_1CC == 1);

    // Another weapon button selected: still framed.
    pButtonArray->m_nSelectedButton = nType == 67 ? 60 : 67;
    pButtonArray->UpdateButtons();
    CHECK(pButtonArray->m_buttonArray[0].field_1CC == 1);

    // No button selected (100, as Escape leaves it) or another action's.
    pButtonArray->m_nSelectedButton = 100;
    pButtonArray->UpdateButtons();
    CHECK(pButtonArray->m_buttonArray[0].field_1CC == 0);
    pButtonArray->m_nSelectedButton = 2;
    pButtonArray->UpdateButtons();
    CHECK(pButtonArray->m_buttonArray[0].field_1CC == 0);

    memcpy(pButtonArray->m_buttonTypes, savedTypes, sizeof(savedTypes));
    pButtonArray->m_nSelectedButton = nSavedSelected;
    pGame->SelectToolbar();
}

namespace {

// The world screen with game input on (CGameSave::m_mode bit 0), the
// party member selected and its toolbar (state 114) shown.
CScreenWorld* WorldWithInput(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetGroup()->GetCount() == 0) {
        pGame->SelectCharacter(pSprite->GetId(), FALSE);
    }
    pGame->SelectToolbar();
    pGame->GetGameSave()->m_mode = 0x1;
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->m_bCtrlKeyDown = FALSE;
    pWorld->m_bShiftKeyDown = FALSE;
    pWorld->field_EA4 = -1;
    return pWorld;
}

void PressKey(CScreenWorld* pWorld, BYTE nKey)
{
    pWorld->m_pVirtualKeysFlags[0] = nKey;
    pWorld->OnKeyDown(1);
}

}

// OnKeyDown (0x6872D0), the keymap's customize keys (0x688D2A, 0x688E17,
// 0x6890E2, 0x689146, 0x6891B9): when the toolbar has no button of the
// key's type, the key opens that type's list through
// CInfButtonArray::sub_594170 (keymap 19: type 10, state 106; keymap 13:
// type 5, state 115). With the button there, the key presses it.
TEST_CASE("world keys: a customize key opens its list when the bar lacks the button", "[uimisc][worldkeys][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const DWORD nMode = pGame->GetGameSave()->m_mode;
    CScreenWorld* pWorld = WorldWithInput(pSprite);
    CInfButtonArray* pButtonArray = pGame->GetButtonArray();
    REQUIRE(pButtonArray->m_nState == 114);

    // VK_F13 is in no raw key case: the keymap decides. Keymap 19 (type
    // 10) and 13 (type 5) get it in turn; no lower entry has it.
    const BYTE nKey = 0x7C;
    BYTE savedKeys[CINFGAME_KEYMAP_SIZE];
    BOOLEAN savedFlags[CINFGAME_KEYMAP_SIZE];
    memcpy(savedKeys, pGame->m_pKeymap, sizeof(savedKeys));
    memcpy(savedFlags, pGame->m_pKeymapFlags, sizeof(savedFlags));
    for (int n = 0; n < CINFGAME_KEYMAP_SIZE; n++) {
        if (pGame->m_pKeymap[n] == nKey)
            pGame->m_pKeymap[n] = 0;
    }

    INT savedTypes[12];
    memcpy(savedTypes, pButtonArray->m_buttonTypes, sizeof(savedTypes));
    for (int n = 0; n < 12; n++) {
        pButtonArray->m_buttonTypes[n] = 1;
    }
    REQUIRE(pButtonArray->GetButtonId(10) == 0xFF);

    pGame->m_pKeymap[19] = nKey;
    pGame->m_pKeymapFlags[19] = FALSE;
    PressKey(pWorld, nKey);
    CHECK(pButtonArray->m_nState == 106);

    // Holding Ctrl, the entry does not match.
    pGame->SelectToolbar();
    for (int n = 0; n < 12; n++) {
        pButtonArray->m_buttonTypes[n] = 1;
    }
    pWorld->m_bCtrlKeyDown = TRUE;
    PressKey(pWorld, nKey);
    CHECK(pButtonArray->m_nState == 114);
    pWorld->m_bCtrlKeyDown = FALSE;

    pGame->m_pKeymap[19] = 0;
    pGame->m_pKeymap[13] = nKey;
    pGame->m_pKeymapFlags[13] = FALSE;
    PressKey(pWorld, nKey);
    CHECK(pButtonArray->m_nState == 115);

    memcpy(pGame->m_pKeymap, savedKeys, sizeof(savedKeys));
    memcpy(pGame->m_pKeymapFlags, savedFlags, sizeof(savedFlags));
    memcpy(pButtonArray->m_buttonTypes, savedTypes, sizeof(savedTypes));
    pGame->GetGameSave()->m_mode = nMode;
    pGame->SelectToolbar();
}

// Escape ends a targeting state (the action bar's reset, the cursor back),
// the arrows set the visible area's key scroll, and without game input the
// keys reach none of these.
TEST_CASE("world keys: Escape and the arrows act with game input only", "[uimisc][worldkeys][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const DWORD nMode = pGame->GetGameSave()->m_mode;
    CScreenWorld* pWorld = WorldWithInput(pSprite);
    CGameArea* pArea = pGame->GetVisibleArea();
    REQUIRE(pArea != NULL);

    pGame->m_nState = 2;
    pGame->m_cButtonArray.m_nSelectedButton = 5;
    pGame->m_tempCursor = 0;
    PressKey(pWorld, VK_ESCAPE);
    CHECK(pGame->m_nState == 0);
    CHECK(pGame->m_cButtonArray.m_nSelectedButton == 100);
    CHECK(pGame->m_tempCursor == 4);

    const LONG nScroll = pArea->m_nKeyScrollState;
    PressKey(pWorld, VK_LEFT);
    CHECK(pArea->m_nKeyScrollState == 7);
    PressKey(pWorld, VK_NUMPAD9);
    CHECK(pArea->m_nKeyScrollState == 2);
    PressKey(pWorld, VK_DOWN);
    CHECK(pArea->m_nKeyScrollState == 5);

    // Without game input (a dialog, a cut scene) the arrows do nothing.
    pGame->GetGameSave()->m_mode = 0;
    PressKey(pWorld, VK_UP);
    CHECK(pArea->m_nKeyScrollState == 5);
    pGame->m_nState = 2;
    PressKey(pWorld, VK_ESCAPE);
    CHECK(pGame->m_nState == 2);

    pGame->m_nState = 0;
    pArea->m_nKeyScrollState = nScroll;
    pGame->GetGameSave()->m_mode = nMode;
}

// Space (0x6872D0, VK_SPACE) toggles the pause through TogglePauseGame
// (0x68DFD0) as a visual pause: paused, the world's time stops and the
// screen dithers; again, the time runs. Without game input it does nothing.
TEST_CASE("world keys: Space pauses and unpauses the game", "[uimisc][worldkeys][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const DWORD nMode = pGame->GetGameSave()->m_mode;
    CScreenWorld* pWorld = WorldWithInput(pSprite);
    CTimerWorld* pTimer = pGame->GetWorldTimer();
    const BOOLEAN bTimeActive = pTimer->m_active;
    const ULONG nGameTime = pTimer->m_gameTime;
    const BYTE nLastPercentage = pTimer->m_nLastPercentage;
    const BOOLEAN bForceDither = pGame->m_bForceDither;
    const unsigned char nF44 = pWorld->field_F44;
    REQUIRE_FALSE(pWorld->m_bPaused);
    pTimer->m_active = TRUE;
    pWorld->field_F44 = 0;

    PressKey(pWorld, VK_SPACE);
    CHECK(pWorld->m_bPaused == TRUE);
    CHECK(pWorld->field_14A == TRUE);
    CHECK_FALSE(pTimer->m_active);
    CHECK(pGame->m_bForceDither == TRUE);

    PressKey(pWorld, VK_SPACE);
    CHECK(pWorld->m_bPaused == FALSE);
    CHECK(pWorld->field_14A == FALSE);
    CHECK(pTimer->m_active);
    CHECK(pGame->m_bForceDither == FALSE);

    // Without game input (a dialog, a cut scene) Space does not pause.
    pGame->GetGameSave()->m_mode = 0;
    PressKey(pWorld, VK_SPACE);
    CHECK(pWorld->m_bPaused == FALSE);
    CHECK(pTimer->m_active);

    pTimer->m_active = bTimeActive;
    pTimer->m_gameTime = nGameTime;
    pTimer->m_nLastPercentage = nLastPercentage;
    pGame->m_bForceDither = bForceDither;
    pWorld->field_F44 = nF44;
    pGame->GetGameSave()->m_mode = nMode;
}

// The keymap's first entry (I by default) opens the inventory
// (OnLeftPanelButtonClick 5 on the active engine).
TEST_CASE("world keys: the inventory key opens the inventory", "[uimisc][worldkeys][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const DWORD nMode = pGame->GetGameSave()->m_mode;
    CScreenWorld* pWorld = WorldWithInput(pSprite);

    REQUIRE(pGame->GetKeymap(0) == 'I');
    REQUIRE(pGame->GetKeymapFlag(0) == 0);
    CWarp* pPrevious = g_pBaldurChitin->pActiveEngine;
    const BOOLEAN bTimeActive = pGame->GetWorldTimer()->m_active;
    g_pBaldurChitin->pActiveEngine = pWorld;
    PressKey(pWorld, 'I');
    CHECK(g_pBaldurChitin->pActiveEngine == g_pBaldurChitin->m_pEngineInventory);

    // The inventory started the world's time; the later cases want it as it
    // was (AddEffect runs the effect list at once only while it is stopped).
    g_pBaldurChitin->pActiveEngine = pPrevious;
    pGame->GetWorldTimer()->m_active = bTimeActive;
    pGame->GetGameSave()->m_mode = nMode;
}

// CAIScriptFile::ParseTrigger (0x410B40), an object and two numbers
// (TRIGGER.IDS 0x40E7 CheckSkillGT(O:Object*,I:Value*,I:SkillNum*Skills)):
// the last one is read without the closing bracket (0x41212A-0x412151), so
// an IDS name resolves. Brogan's Intimidate reply (10BROGAN state 7) asks
// for it; it used to keep the bracket and read 0.
TEST_CASE("dialog script: a trigger's last number is found by its IDS name", "[uimisc][script][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CAIScriptFile scriptFile;
    scriptFile.ParseConditionalString(CString("CheckSkillGT(Protagonist, 0, Intimidate)\n"));
    INFO(static_cast<const char*>(scriptFile.m_errors));
    REQUIRE(scriptFile.m_curCondition->m_triggerList.GetCount() == 1);
    CAITrigger* pTrigger = scriptFile.m_curCondition->m_triggerList.GetHead();
    CHECK(pTrigger->m_triggerID == 0x40E7);
    CHECK(pTrigger->m_specificID == 0);
    CHECK(pTrigger->m_specific2 == 7); // SKILLS.IDS: 7 Intimidate

    // A number as before.
    scriptFile.ParseConditionalString(CString("CheckSkillLT(Protagonist, 1, 4)\n"));
    REQUIRE(scriptFile.m_curCondition->m_triggerList.GetCount() == 1);
    pTrigger = scriptFile.m_curCondition->m_triggerList.GetHead();
    CHECK(pTrigger->m_triggerID == 0x40E8);
    CHECK(pTrigger->m_specificID == 1);
    CHECK(pTrigger->m_specific2 == 4);
}

// CGameContainer::MarshalMessage (0x480C00) for a new ground pile, as
// CInfGame::GetGroundPile (0x5B75C0) makes one at the inventory's opening:
// no polygon, no items. The item count starts at 0 (0x480D09); it used to
// be read uninitialised and fail the size assert inside the constructor's
// CGameObjectArray::Add.
TEST_CASE("container: a new ground pile marshals no items", "[uimisc][container][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    const CPoint& pt = pSprite->GetPos();
    CGameContainer* pPile = new CGameContainer(pSprite->GetArea(), CRect(pt.x - 8, pt.y - 6, pt.x + 9, pt.y + 7));
    REQUIRE(pPile->m_lstItems.GetCount() == 0);

    BYTE* pData = NULL;
    DWORD dwSize = 0;
    pPile->MarshalMessage(&pData, &dwSize);
    CString sArea;
    pSprite->GetArea()->m_resRef.CopyToString(sArea);
    // The record, the area name's length and name, the polygon's count and
    // the item count; no points and no items.
    CHECK(dwSize == sizeof(CAreaFileContainer) + sizeof(BYTE) + 2 * sizeof(WORD) + static_cast<DWORD>(sArea.GetLength()));
}

// The text character sheet's lines (0x5EF760): the label over a row of
// dots, the value; indented lines behind three spaces with a shorter row.
// An item's line (0x5EFCC0): the uses past one, NOT IDENTIFIED, a star when
// equipped.
TEST_CASE("character screen: the text character sheet's lines", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    // "L", 28 dots and a space under the label; "L", 25 dots and a space
    // indented.
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString("Name"), CString("Probe"), FALSE))
        == "Name" + std::string(25, '.') + " Probe\n");
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString("Strength"), CString("13"), TRUE))
        == "   Strength" + std::string(18, '.') + " 13\n");

    // Bounded (ruling P4-10): a label longer than the row ends where it
    // ends, the value follows; the line keeps to the 1000-byte buffer with
    // its new line, the value cut first, then the label.
    const std::string sLong(40, 'x');
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString(sLong.c_str()), CString("7"), FALSE)) == sLong + "7\n");
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString(sLong.c_str()), CString("7"), TRUE)) == "   " + sLong + "7\n");
    const std::string sHuge(2000, 'v');
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString("Name"), CString(sHuge.c_str()), FALSE))
        == "Name" + std::string(25, '.') + " " + std::string(998 - 30, 'v') + "\n");
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString(sHuge.c_str()), CString("7"), FALSE)) == std::string(998, 'v') + "\n");
    CHECK(std::string(CScreenCharacter::sub_5EF760(CString("Name"), CString(sHuge.c_str()), TRUE))
        == "   Name" + std::string(22, '.') + " " + std::string(995 - 27, 'v') + "\n");

    FILE* file = tmpfile();
    REQUIRE(file != nullptr);
    CItem sword(CResRef("00SWDL01"), 0, 0, 0, 0, 0x1);
    CItem unknown(CResRef("00SWDL01"), 0, 0, 0, 0, 0);
    unknown.m_flags &= ~0x1;
    CItem bolts(CResRef("00BOLT01"), 10, 0, 0, 0, 0x1);
    CScreenCharacter::sub_5EFCC0(file, &sword, TRUE, FALSE);
    CScreenCharacter::sub_5EFCC0(file, &unknown, FALSE, FALSE);
    CScreenCharacter::sub_5EFCC0(file, &bolts, FALSE, FALSE);
    std::string text(4096, '\0');
    rewind(file);
    text.resize(fread(&text[0], 1, text.size(), file));
    fclose(file);

    std::string sSword = Str(sword.GetGenericName());
    std::string sUnknown = Str(unknown.GetGenericName());
    std::string sBolts = Str(bolts.GetGenericName());
    std::string expected = Format("-- %s *\n", sSword.c_str());
    expected += Format("-- %s - %s\n", sUnknown.c_str(), Str(17108).c_str());
    expected += Format("-- %s (%d)\n", sBolts.c_str(), static_cast<int>(bolts.GetUsageCount(0)));
    CHECK(text == expected);

    // A description (0x5EFB60) in lines of at most 70 characters, cut at a
    // new line or the last space; nothing else is lost.
    file = tmpfile();
    REQUIRE(file != nullptr);
    sword.Demand();
    const STRREF strDescription = sword.GetDescription();
    sword.Release();
    CScreenCharacter::sub_5EFB60(file, strDescription);
    text.assign(65536, '\0');
    rewind(file);
    text.resize(fread(&text[0], 1, text.size(), file));
    fclose(file);

    const std::string sDescription = "     " + Str(strDescription);
    REQUIRE(sDescription.size() > 140);
    std::string sRejoined;
    size_t nStart = 0;
    while (nStart < text.size()) {
        size_t nEnd = text.find('\n', nStart);
        if (nEnd == std::string::npos)
            nEnd = text.size();
        CHECK(nEnd - nStart <= 70);
        sRejoined += text.substr(nStart, nEnd - nStart);
        nStart = nEnd + 1;
    }
    std::string sExpected;
    for (char ch : sDescription) {
        if (ch != '\n' && ch != ' ')
            sExpected += ch;
    }
    sRejoined.erase(std::remove(sRejoined.begin(), sRejoined.end(), ' '), sRejoined.end());
    CHECK(sRejoined == sExpected);
}

// sub_5E4560 (0x5E4560) runs the text character sheet export (0x5EFFD0) and
// says how it went in the world screen's messages. Without the save root's
// default folder the file cannot be opened.
TEST_CASE("character screen: the text character sheet without its folder", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    std::error_code ec;
    fs::remove_all(Child(g_view, "mpsave") / "default", ec);

    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;
    pCharacter->m_nSelectedCharacter = 0;
    BYTE bSuccess = TRUE;
    CString sFileName = pCharacter->sub_5EFFD0(bSuccess);
    CHECK_FALSE(bSuccess);
    CHECK(std::string(static_cast<LPCSTR>(sFileName)) == "...\\default\\Probe.txt ");

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CUIControlTextDisplay* pDisplay = static_cast<CUIControlTextDisplay*>(pWorld->GetManager()->GetPanel(pWorld->GetPanel_22_0())->GetControl(1));
    REQUIRE(pDisplay != NULL);
    pDisplay->RemoveAll();
    pCharacter->sub_5E4560();
    std::vector<std::string> items = DisplayItems(pDisplay);
    REQUIRE(items.size() == 1);
    CHECK(items[0].find(Str(16802).substr(0, 20)) != std::string::npos); // "Error writing text character sheet."
    pDisplay->RemoveAll();
}

#ifndef _WIN32

namespace {

void FlushAndLeave(int)
{
    fflush(NULL);
    _exit(42);
}

}

// The export writes the sheet up to the spell lists, where the original
// asks GetSpells for "class" 0 (the list index) and its assert ends the game
// (0x5CA6A4). A child process runs it; its SIGABRT handler flushes the file.
TEST_CASE("character screen: the text character sheet up to the original's assert", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CGameSprite* pSprite;
    {
        // The new game waits for drawn frames; the child needs none.
        FramePump pump;
        pSprite = PartyMember();
    }
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    std::error_code ec;
    fs::create_directories(Child(g_view, "mpsave") / "default", ec);
    const fs::path sheet = Child(g_view, "mpsave") / "default" / "Probe.txt";
    fs::remove(sheet, ec);
    const int nAC = pSprite->GetAC();

    fflush(NULL);
    pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        signal(SIGABRT, FlushAndLeave);
        g_pBaldurChitin->m_pEngineCharacter->m_nSelectedCharacter = 0;
        BYTE bSuccess = FALSE;
        g_pBaldurChitin->m_pEngineCharacter->sub_5EFFD0(bSuccess);
        _exit(0);
    }
    // The child of a process with threads has only this one: should it
    // block on a lock another thread held at the fork, it is killed after a
    // minute (the whole case takes under two seconds).
    int status = 0;
    pid_t waited = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while ((waited = waitpid(pid, &status, WNOHANG)) == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (waited == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        FAIL("the export's child process did not end within a minute");
    }
    REQUIRE(waited == pid);
    REQUIRE(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 42);

    REQUIRE(fs::exists(sheet));
    const std::string text = ReadFile(sheet);
    INFO(text);
    CHECK(text.find("Name" + std::string(25, '.') + " Probe\n") == 0);
    CHECK(text.find(Format("%s:\n", Str(17088).c_str())) != std::string::npos); // "Abilities:"
    CHECK(text.find(Format("%s", Str(10339).c_str())) != std::string::npos); // "Armor Class"
    CHECK(text.find(Format(". %d\n", nAC)) != std::string::npos);
    CHECK(text.find(Format("%s:\n", Str(9457).c_str())) != std::string::npos); // "Attack Roll Modifiers:"
    CHECK(text.find(Format("%s:\n", Str(17379).c_str())) != std::string::npos); // "Saving Throws:"
    CHECK(text.find(Format("%s:\n", Str(15544).c_str())) != std::string::npos); // "Resistances:"
    // Nothing past the spell lists.
    CHECK(text.find(Format("--  %s:\n", Str(11292).c_str())) == std::string::npos);

    fs::remove(sheet, ec);
}

#endif

// The keymap loops of four screens' OnKeyDown (the journal, the store, the
// area map and the world map): a key at no keymap index 0 walks the whole
// keymap and returns. Ours never moved its index, so such a key hung the
// game (the original increments it).
namespace {

// Runs `press` on a thread of its own and says whether it returned within
// five seconds. A hung press cannot be joined without hanging the case too,
// so its thread is detached and left to the process's end: the case fails,
// and ctest runs one case per process.
bool ReturnsInTime(std::function<void()> press)
{
    auto done = std::make_shared<std::atomic<bool>>(false);
    std::thread worker([press, done] {
        press();
        done->store(true);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done->load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (done->load()) {
        worker.join();
        return true;
    }
    worker.detach();
    return false;
}

// VK_F13, in no raw key case of the four screens, taken out of the keymap:
// every keymap entry is tested and none matches.
const BYTE KEY_UNBOUND = 0x7C;

void UnbindKey(CInfGame* pGame, BYTE nKey)
{
    for (int n = 0; n < CINFGAME_KEYMAP_SIZE; n++) {
        if (pGame->m_pKeymap[n] == nKey) {
            pGame->m_pKeymap[n] = 0;
        }
    }
}

// Presses the unbound key on `pScreen` with the key flags saved and restored:
// the pressed-key flag and Ctrl's state are the screen's, and a later case in
// the same process would see whatever the press left.
template <typename TScreen>
void PressUnboundKey(TScreen* pScreen)
{
    const BYTE savedPressed = pScreen->m_pVirtualKeysFlags[0];
    const BOOL savedCtrl = pScreen->m_bCtrlKeyDown;
    pScreen->m_bCtrlKeyDown = FALSE;
    pScreen->m_pVirtualKeysFlags[0] = KEY_UNBOUND;
    CHECK(ReturnsInTime([pScreen] { pScreen->OnKeyDown(1); }));
    pScreen->m_pVirtualKeysFlags[0] = savedPressed;
    pScreen->m_bCtrlKeyDown = static_cast<BOOLEAN>(savedCtrl);
}

} // namespace

TEST_CASE("keys: an unbound key returns from the journal and the store and both maps", "[uimisc][keys][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    BYTE savedKeys[CINFGAME_KEYMAP_SIZE];
    memcpy(savedKeys, pGame->m_pKeymap, sizeof(savedKeys));
    UnbindKey(pGame, KEY_UNBOUND);

    SECTION("journal")
    {
        PressUnboundKey(g_pBaldurChitin->m_pEngineJournal);
    }

    SECTION("store")
    {
        PressUnboundKey(g_pBaldurChitin->m_pEngineStore);
    }

    SECTION("area map")
    {
        PressUnboundKey(g_pBaldurChitin->m_pEngineMap);
    }

    SECTION("world map")
    {
        PressUnboundKey(g_pBaldurChitin->m_pEngineWorldMap);
    }

    memcpy(pGame->m_pKeymap, savedKeys, sizeof(savedKeys));
}

// Area travel (ui-misc round 4): a click on a travel region or at an area's
// edge, the actions it queues, and the two travel actions (LeaveAreaName
// 0x74CDB0 through a region, LeaveArea 0x74C0D0 to the world map), on a new
// game's party member in AR1000.
namespace {

// The region of `pArea` whose script name is `name`, or NULL.
CGameTrigger* AreaRegion(CGameArea* pArea, const char* name)
{
    CGameObjectArray* pObjects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (pObjects->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pObjects->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                if (pObject->GetObjectType() == CGameObject::TYPE_TRIGGER) {
                    CGameTrigger* pTrigger = static_cast<CGameTrigger*>(pObject);
                    if (_strnicmp(pTrigger->m_scriptName, name, SCRIPTNAME_SIZE) == 0) {
                        return pTrigger;
                    }
                }
            }
        }
    }
    return NULL;
}

// A single-player game as the game runs one: a host session of the null
// service provider with its one local player (CScreenConnection's new game
// calls HostNewSession and CreatePlayer, which set the connection and host
// flags and the player id). The test engine starts with no provider (-1)
// and no session; the session is closed when the case ends.
class HostSession {
public:
    HostSession()
        : m_nServiceProvider(g_pChitin->cNetwork.m_nServiceProvider)
        , m_bPlayerShutdown(g_pBaldurChitin->m_cBaldurMessage.m_bPlayerShutdown)
        , m_bOpened(false)
    {
        CNetwork& network = g_pChitin->cNetwork;
        if (!network.m_bConnectionEstablished) {
            if (!network.m_bServiceProviderEnumerated) {
                network.EnumerateServiceProviders();
            }
            network.SelectServiceProvider(CNetwork::SERV_PROV_NULL);
            network.InitializeConnectionToServiceProvider(TRUE);
            network.m_sSessionNameToMake = "Session";
            network.m_bSessionNameToMake = TRUE;
            network.m_sLocalPlayerName = "Player";
            network.m_bPlayerNameToMake = TRUE;
            INT nError = -1;
            m_bOpened = network.HostNewSession() == TRUE && network.CreatePlayer(nError) == TRUE;
        }
        network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    }
    ~HostSession()
    {
        if (m_bOpened) {
            g_pChitin->cNetwork.CloseSession(TRUE);
            g_pBaldurChitin->m_cBaldurMessage.m_bPlayerShutdown = m_bPlayerShutdown;
        }
        g_pChitin->cNetwork.m_nServiceProvider = m_nServiceProvider;
    }
    HostSession(const HostSession&) = delete;
    HostSession& operator=(const HostSession&) = delete;

    bool Open() const
    {
        return g_pChitin->cNetwork.m_bConnectionEstablished == TRUE && g_pChitin->cNetwork.m_bIsHost == TRUE;
    }

private:
    INT m_nServiceProvider;
    BOOLEAN m_bPlayerShutdown;
    bool m_bOpened;
};

// The party member selected alone, its action queue empty, in a single
// player game: the null service provider, which the connection screen
// selects (the test engine starts with none, -1, which the travel actions
// take for a multiplayer game).
void SelectAlone(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    pGame->UnselectAll();
    pGame->SelectCharacter(pSprite->GetId(), FALSE);
    pSprite->ClearActions(FALSE);
    pSprite->m_queuedActions.RemoveAll();
}

// The sprite moved to `pt` in its area (out of the area and back in).
void PutAt(CGameSprite* pSprite, const CPoint& pt)
{
    CGameArea* pArea = pSprite->GetArea();
    pSprite->RemoveFromArea();
    pSprite->m_pos = pt;
    pSprite->AddToArea(pArea, pt, 0, CGameObject::LIST_FRONT);
}

int CountActions(CGameSprite* pSprite, SHORT actionID)
{
    int count = 0;
    POSITION pos = pSprite->m_queuedActions.GetHeadPosition();
    while (pos != NULL) {
        if (pSprite->m_queuedActions.GetNext(pos)->m_actionID == actionID) {
            count++;
        }
    }
    return count;
}

// LeaveAreaName (93) or LeaveArea (91) run as the AI update runs an action:
// with the sprite denied.
SHORT RunTravelAction(CGameSprite* pSprite, const CAIAction& action)
{
    CGameObjectArray* pObjects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CGameObject* pObject;
    REQUIRE(pObjects->GetDeny(pSprite->GetId(), CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
    pSprite->m_curAction = action;
    SHORT result = action.m_actionID == CAIAction::LEAVEAREA ? pSprite->LeaveArea() : pSprite->LeaveAreaName();
    pObjects->ReleaseDeny(pSprite->GetId(), CGameObjectArray::THREAD_ASYNCH, INFINITE);
    return result;
}

const SHORT LEAVEAREANAME = 93;

} // namespace

// CGameArea::Unmarshal (0x472DE0) keeps each entry point of the area file in
// m_entryPoints (CPtrList::AddTail at 0x473F9B), where GetEntryPoint finds
// the travel's destination.
TEST_CASE("travel: a loaded area's entry points are found by name", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pStart = pSprite->GetArea();
    REQUIRE(pStart != NULL);

    CPoint pt;
    SHORT facing = -1;
    // AR1000's way back from the house behind Door5.
    REQUIRE(pStart->GetEntryPoint(CString("FR1005"), pt, facing));
    CHECK(pt == CPoint(809, 1053));
    CHECK(facing == 0);
    CHECK_FALSE(pStart->GetEntryPoint(CString("NoSuchEntry"), pt, facing));

    // The house's entrance from AR1000 (AR1005.ARE: 165,224, facing 8).
    CGameArea* pHouse = pGame->LoadArea(CString("AR1005"), 0, FALSE, FALSE);
    REQUIRE(pHouse != NULL);
    REQUIRE(pHouse->GetEntryPoint(CString("FR1000"), pt, facing));
    CHECK(pt == CPoint(165, 224));
    CHECK(facing == 8);
    // Names are compared without regard to case.
    CHECK(pHouse->GetEntryPoint(CString("fr1000"), pt, facing));
}

// CGameArea::OnActionButtonClickTravel (0x475870): a click at the edge gives
// the group LeaveArea (91) on the point with the edge's direction and area,
// clearing their actions, then MoveToPoint (23) to it without clearing
// (0x475C69); AddAction puts each at the head and GetNextAction takes the
// head, so the walk runs first and one LeaveArea follows it.
TEST_CASE("travel: a click at the area's edge queues the walk before one LeaveArea", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    SelectAlone(pSprite);

    // The west edge: the first search squares.
    const CPoint pt(CPathSearch::GRID_SQUARE_SIZEX / 2, 600);
    pArea->OnActionButtonClickTravel(pt);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CHECK(pGame->m_lastClick == CPoint(-1, -1));
    REQUIRE(pSprite->m_queuedActions.GetCount() == 2);
    CHECK(CountActions(pSprite, CAIAction::LEAVEAREA) == 1);

    const CAIAction* pMove = pSprite->m_queuedActions.GetHead();
    CHECK(pMove->m_actionID == CAIAction::MOVETOPOINT);
    CHECK(pMove->m_dest == pt);
    CHECK(pMove->m_specificID == 0);
    CHECK(pMove->m_specificID2 == -1);

    const CAIAction* pLeave = pSprite->m_queuedActions.GetTail();
    CHECK(pLeave->m_actionID == CAIAction::LEAVEAREA);
    CHECK(pLeave->m_dest == pt);
    CHECK(pLeave->m_specificID == static_cast<LONG>(4 | ((pArea->m_header.m_flagsEdgeWest & 0x1) << 16)));
    CHECK(std::string(static_cast<LPCSTR>(pLeave->m_string1))
        == std::string(static_cast<LPCSTR>(CString(pArea->m_header.m_areaEdgeWest))));

    pSprite->ClearActions(FALSE);
    pSprite->m_queuedActions.RemoveAll();
}

// CGameTrigger::OnActionButton (0x4CED10): a click on a travel region tells
// the region it was clicked (CLICKED, 112) and gives the group LeaveAreaName
// (93) on the region, clearing their actions, then MoveToPoint (23) to its
// position (its alternate point with the flag 0x200, as To1005 has), and
// the region's id as the group's trigger id.
TEST_CASE("travel: a click on a travel region queues the walk before LeaveAreaName", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    CGameTrigger* pRegion = AreaRegion(pArea, "To1005");
    REQUIRE(pRegion != NULL);
    REQUIRE(pRegion->m_triggerType == 2);
    REQUIRE((pRegion->m_dwFlags & 0x200) != 0);
    SelectAlone(pSprite);
    pRegion->m_pendingTriggers.RemoveAll();
    pGame->m_nState = 0;

    pRegion->OnActionButton(pRegion->m_pos);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CHECK(pSprite->m_triggerId == pRegion->GetId());
    REQUIRE(pSprite->m_queuedActions.GetCount() == 2);
    CHECK(CountActions(pSprite, LEAVEAREANAME) == 1);

    const CAIAction* pMove = pSprite->m_queuedActions.GetHead();
    CHECK(pMove->m_actionID == CAIAction::MOVETOPOINT);
    CHECK(pMove->m_dest == pRegion->field_628);
    CHECK(pMove->m_specificID2 == -1);

    const CAIAction* pLeave = pSprite->m_queuedActions.GetTail();
    CHECK(pLeave->m_actionID == LEAVEAREANAME);
    CHECK(pLeave->m_specificID == pRegion->GetId());

    bool bClicked = false;
    POSITION pos = pRegion->m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        if (pRegion->m_pendingTriggers.GetNext(pos)->m_triggerID == CAITRIGGER_CLICKED) {
            bClicked = true;
        }
    }
    CHECK(bClicked);

    pSprite->ClearActions(FALSE);
    pSprite->m_queuedActions.RemoveAll();
    pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;
}

// The first region of `pArea` of the type `nType` that `accept` takes.
template <typename F>
CGameTrigger* AreaRegionOfType(CGameArea* pArea, WORD nType, F accept)
{
    CGameObjectArray* pObjects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (pObjects->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pObjects->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                if (pObject->GetObjectType() == CGameObject::TYPE_TRIGGER) {
                    CGameTrigger* pTrigger = static_cast<CGameTrigger*>(pObject);
                    if (pTrigger->m_triggerType == nType && accept(pTrigger)) {
                        return pTrigger;
                    }
                }
            }
        }
    }
    return NULL;
}

// CGameTrigger::OnActionButton (0x4CED10), an info region (type 1): the
// click floats the region's text over it (FloatText 10, 5: a text object in
// field_1C) and queues nothing.
TEST_CASE("travel: a click on an info region floats its text", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    REQUIRE(pGame->GetVisibleArea() == pArea);
    CGameTrigger* pInfo = AreaRegionOfType(pArea, 1, [](CGameTrigger* p) {
        return p->m_description != static_cast<STRREF>(-1) && p->m_description != 0;
    });
    REQUIRE(pInfo != NULL);
    SelectAlone(pSprite);
    pGame->m_nState = 0;
    pInfo->field_1C = 0;

    // The floating text sizes itself by the current video mode's FX size
    // (character generation's, which StartEngine sets up, whichever engine
    // another case left active); the test engine never activates its video
    // mode, so it has no FX surface, and the 3D flag makes GetFXSize answer
    // with the size instead.
    CWarp* pActive = g_pChitin->pActiveEngine;
    g_pChitin->pActiveEngine = g_pBaldurChitin->m_pEngineCreateChar;
    const BOOL bIs3dAccelerated = g_pChitin->cVideo.m_bIs3dAccelerated;
    g_pChitin->cVideo.m_bIs3dAccelerated = TRUE;
    pInfo->OnActionButton(pInfo->m_pos);
    g_pChitin->cVideo.m_bIs3dAccelerated = bIs3dAccelerated;
    g_pChitin->pActiveEngine = pActive;
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CHECK(pInfo->field_1C != 0);
    CHECK(pSprite->m_queuedActions.GetCount() == 0);
    CHECK(pGame->m_lastClick == CPoint(-1, -1));
}

// CGameTrigger::OnActionButton (0x4CED10), state 2 with the disarm icon
// (0x24) on an active, detected trap: the group is sent to RemoveTraps (28)
// on it and the state and the selected button go back (0, 100), when the
// selected character has ranks in Disable Device (the base skill 5); without
// them nothing happens.
TEST_CASE("travel: the disarm cursor on a detected trap sends the group to remove it", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    CGameTrigger* pTrap = AreaRegionOfType(pArea, 0, [](CGameTrigger*) {
        return true;
    });
    REQUIRE(pTrap != NULL);
    SelectAlone(pSprite);

    const WORD nActivated = pTrap->m_trapActivated;
    const WORD nDetected = pTrap->m_trapDetected;
    const BYTE nSkill = pSprite->m_baseStats.m_skills[5];
    pTrap->m_trapActivated = 1;
    pTrap->m_trapDetected = 1;

    // No Disable Device: nothing.
    pSprite->m_baseStats.m_skills[5] = 0;
    pGame->m_nState = 2;
    pGame->m_iconIndex = 0x24;
    pTrap->OnActionButton(pTrap->m_pos);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pSprite->m_queuedActions.GetCount() == 0);
    CHECK(pGame->m_nState == 2);

    // With ranks: RemoveTraps on the trap.
    pSprite->m_baseStats.m_skills[5] = 4;
    pTrap->OnActionButton(pTrap->m_pos);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    REQUIRE(pSprite->m_queuedActions.GetCount() == 1);
    CHECK(pSprite->m_queuedActions.GetHead()->m_actionID == 28);
    CHECK(pGame->m_nState == 0);
    CHECK(pGame->m_cButtonArray.m_nSelectedButton == 100);

    pSprite->m_baseStats.m_skills[5] = nSkill;
    pTrap->m_trapActivated = nActivated;
    pTrap->m_trapDetected = nDetected;
    pGame->m_iconIndex = 0xFF;
    pGame->m_nState = 0;
    pSprite->ClearActions(FALSE);
    pSprite->m_queuedActions.RemoveAll();
}

// LeaveAreaName (0x74CDB0) stops when the member is not by the region: done
// beyond the travel distance (0x10000 by dy*dy*16/9 + dx*dx), an error
// within it but off the region (RANGE_TRIGGER), an error for a region that
// is gone; nothing is loaded.
TEST_CASE("travel: LeaveAreaName waits until the member stands on the region", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    CGameTrigger* pRegion = AreaRegion(pArea, "To1005");
    REQUIRE(pRegion != NULL);
    const CPoint ptRegion = pRegion->GetPos();
    const CPoint ptStart = pSprite->GetPos();
    SelectAlone(pSprite);

    int nAreas = 0;
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        nAreas += pGame->m_gameAreas[n] != NULL;
    }

    const CAIAction leave(LEAVEAREANAME, CAIObjectType(), pRegion->GetId(), 0, 0);

    // 300 pixels away: 90000 > 0x10000.
    PutAt(pSprite, CPoint(ptRegion.x + 300, ptRegion.y));
    CHECK(RunTravelAction(pSprite, leave) == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetArea() == pArea);

    // 100 pixels away: 10000 is within 0x10000 but past RANGE_TRIGGER (4096).
    PutAt(pSprite, CPoint(ptRegion.x + 100, ptRegion.y));
    CHECK(RunTravelAction(pSprite, leave) == CGameAIBase::ACTION_ERROR);
    CHECK(pSprite->GetArea() == pArea);

    // No such object.
    const CAIAction nowhere(LEAVEAREANAME, CAIObjectType(), CGameObjectArray::INVALID_INDEX, 0, 0);
    CHECK(RunTravelAction(pSprite, nowhere) == CGameAIBase::ACTION_ERROR);

    int nAreasAfter = 0;
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        nAreasAfter += pGame->m_gameAreas[n] != NULL;
    }
    CHECK(nAreasAfter == nAreas);

    PutAt(pSprite, ptStart);
}

// LeaveAreaName (0x74CDB0) with the member on the region: the region's area
// is loaded and the member put at its entry point (in search squares, the
// formation's first place, the nearest open square), in the new area and
// selected; the action is done.
TEST_CASE("travel: LeaveAreaName on the region takes the member to the entry point", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    CGameTrigger* pRegion = AreaRegion(pArea, "To1005");
    REQUIRE(pRegion != NULL);
    const CPoint ptStart = pSprite->GetPos();
    SelectAlone(pSprite);
    PutAt(pSprite, pRegion->GetPos());
    // The click's SetGroupTriggerId: the members that go are those with
    // the region's id (To1005 does not need the whole party).
    REQUIRE((pRegion->m_dwFlags & 0x4) == 0);
    pSprite->m_triggerId = pRegion->GetId();
    // Selecting the member in the new area activates it, which sets the
    // channel volumes: the mixer CChitin::InitializeServices (0x790240)
    // starts with the video.
    g_pChitin->cSoundMixer.Initialize(&g_pChitin->cWnd, 16, g_pChitin->GetNumberSoundChannels());
    // In a session the travel brings the world screen up (SelectEngine);
    // the engine that was active comes back at the end without an
    // activation.
    CWarp* pEngine = g_pChitin->pActiveEngine;

    const CAIAction leave(LEAVEAREANAME, CAIObjectType(), pRegion->GetId(), 0, 0);
    CHECK(RunTravelAction(pSprite, leave) == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CGameArea* pHouse = pGame->GetArea(CString("AR1005"));
    REQUIRE(pHouse != NULL);
    CHECK(pSprite->GetArea() == pHouse);
    // FR1000 is 165,224 (search square 10,18); the sprite stands within a
    // few squares of it.
    const CPoint pos = pSprite->GetPos();
    INFO("pos " << pos.x << "," << pos.y);
    CHECK(std::abs(pos.x - 165) <= 4 * CPathSearch::GRID_SQUARE_SIZEX);
    CHECK(std::abs(pos.y - 224) <= 4 * CPathSearch::GRID_SQUARE_SIZEY);
    CHECK(pSprite->m_triggerId == CGameObjectArray::INVALID_INDEX);
    CHECK(pGame->GetGroup()->GetCount() == 1);
    CHECK(pArea->field_436 == 1);

    // Back to AR1000 for the other cases (one process may run them all).
    pSprite->RemoveFromArea();
    pSprite->m_pos = ptStart;
    pSprite->AddToArea(pArea, ptStart, 0, CGameObject::LIST_FRONT);
    pGame->UnselectAll();
    pGame->SelectCharacter(pSprite->GetId(), FALSE);
    CHECK(pGame->GetVisibleArea() == pArea);
    g_pChitin->pActiveEngine = pEngine;
    g_pChitin->cSoundMixer.CleanUp();
}

// LeaveAreaName (0x74CDB0) on a party required region (To1100, flag 0x4) in
// a single-player game, a host session: CBaldurMessage 0x438710 answers 0
// while the game is paused (an error, nothing loaded), else the host makes
// the party's transition (CInfGame 0x5C4AD0) and the action is done: the
// auto-save, AR1100 loaded and the member put by its FR1000 (249,1376) in
// the 2 by 3 formation, the area left behind marshalled, the progress
// screen down.
TEST_CASE("travel: a party required region takes the party through the host's transition", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    CGameTrigger* pRegion = AreaRegion(pArea, "To1100");
    REQUIRE(pRegion != NULL);
    REQUIRE((pRegion->m_dwFlags & 0x4) != 0);
    const CPoint ptStart = pSprite->GetPos();
    SelectAlone(pSprite);
    pGame->m_singlePlayerPermissions.SetSinglePermission(CGamePermission::AREA_TRANSITION, TRUE);
    PutAt(pSprite, pRegion->GetPos());
    pSprite->m_triggerId = pRegion->GetId();
    g_pChitin->cSoundMixer.Initialize(&g_pChitin->cWnd, 16, g_pChitin->GetNumberSoundChannels());
    // In a session the travel brings the world screen up (SelectEngine);
    // the engine that was active comes back at the end without an
    // activation.
    CWarp* pEngine = g_pChitin->pActiveEngine;

    const CAIAction leave(LEAVEAREANAME, CAIObjectType(), pRegion->GetId(), 0, 0);

    // Paused: refused.
    const BOOL bPaused = pWorld->m_bPaused;
    pWorld->m_bPaused = TRUE;
    CHECK(RunTravelAction(pSprite, leave) == CGameAIBase::ACTION_ERROR);
    pWorld->m_bPaused = bPaused;
    CHECK(pGame->GetArea(CString("AR1100")) == NULL);
    CHECK(pSprite->GetArea() == pArea);

    pWorld->m_bPaused = FALSE;
    // The save names come back at the end (the character sheet cases write
    // into the current save's folder).
    const CString sSaveGame = pGame->m_sSaveGame;
    const CString sCharacterSaveName = CScreenCharacter::SAVE_NAME;
    const BOOL bScreenGrabbed = pGame->field_50D8;
    pGame->m_sSaveGame = "";
    // The auto-save's screen grab (SynchronousUpdate) is taken once
    // field_50D8 is clear; the test engine draws no frame, so it counts as
    // taken.
    pGame->field_50D8 = TRUE;
    CHECK(RunTravelAction(pSprite, leave) == CGameAIBase::ACTION_DONE);
    pWorld->m_bPaused = bPaused;
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CGameArea* pTargos = pGame->GetArea(CString("AR1100"));
    REQUIRE(pTargos != NULL);
    CHECK(pSprite->GetArea() == pTargos);
    const CPoint pos = pSprite->GetPos();
    INFO("pos " << pos.x << "," << pos.y);
    CHECK(std::abs(pos.x - 249) <= 6 * CPathSearch::GRID_SQUARE_SIZEX);
    CHECK(std::abs(pos.y - 1376) <= 6 * CPathSearch::GRID_SQUARE_SIZEY);
    CHECK(std::string(static_cast<LPCSTR>(pGame->m_sSaveGame)) == std::string(static_cast<LPCSTR>(CInfGame::AUTO_SAVE_NAME)));
    CHECK(pGame->m_nTravelScreenImageToUse < 5);
    CHECK(pArea->field_436 == 1);
    CHECK(pGame->m_bInAreaTransition == FALSE);
    CHECK(pSprite->m_triggerId == CGameObjectArray::INVALID_INDEX);

    // Back to AR1000 for the other cases.
    pSprite->RemoveFromArea();
    pSprite->m_pos = ptStart;
    pSprite->AddToArea(pArea, ptStart, 0, CGameObject::LIST_FRONT);
    pGame->UnselectAll();
    pGame->SelectCharacter(pSprite->GetId(), FALSE);
    CHECK(pGame->GetVisibleArea() == pArea);
    g_pChitin->pActiveEngine = pEngine;
    g_pChitin->cSoundMixer.CleanUp();
    pGame->m_sSaveGame = sSaveGame;
    CScreenCharacter::SAVE_NAME = sCharacterSaveName;
    pGame->field_50D8 = bScreenGrabbed;
}

// LeaveArea (0x74C0D0): a member by the clicked edge point (RANGE_EDGE) with
// the party near opens the world map for travel (state 1) in the edge's
// direction; one further away is told to gather (an error, the walk goes
// on); one beyond the travel distance, or without the area transition
// permission, is done.
TEST_CASE("travel: LeaveArea opens the world map for travel by the edge", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    HostSession session;
    REQUIRE(session.Open());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorldMap* pWorldMap = g_pBaldurChitin->m_pEngineWorldMap;
    const CPoint ptStart = pSprite->GetPos();
    SelectAlone(pSprite);

    // Without the area transition permission (InitGame gives it in a single
    // player game): done, nothing opens.
    pGame->m_singlePlayerPermissions.SetSinglePermission(CGamePermission::AREA_TRANSITION, FALSE);
    CAIAction leaveDenied(CAIAction::LEAVEAREA, CString(""), ptStart, 4);
    CHECK(RunTravelAction(pSprite, leaveDenied) == CGameAIBase::ACTION_DONE);
    CHECK(g_pChitin->pActiveEngine != pWorldMap);
    pGame->m_singlePlayerPermissions.SetSinglePermission(CGamePermission::AREA_TRANSITION, TRUE);

    // 300 pixels away: done, nothing opens.
    CAIAction leaveFar(CAIAction::LEAVEAREA, CString(""), CPoint(ptStart.x + 300, ptStart.y), 4);
    CHECK(RunTravelAction(pSprite, leaveFar) == CGameAIBase::ACTION_DONE);
    CHECK(g_pChitin->pActiveEngine != pWorldMap);

    // 200 pixels away: within 0x10000 (40000) but past RANGE_EDGE (36864):
    // an error, the world map stays shut.
    CAIAction leaveNear(CAIAction::LEAVEAREA, CString(""), CPoint(ptStart.x + 200, ptStart.y), 4);
    CHECK(RunTravelAction(pSprite, leaveNear) == CGameAIBase::ACTION_ERROR);
    CHECK(g_pChitin->pActiveEngine != pWorldMap);

    // At the point: the world map, for travel, westwards.
    CWarp* pPrevious = g_pChitin->pActiveEngine;
    CAIAction leaveAt(CAIAction::LEAVEAREA, CString(""), ptStart, 4);
    CHECK(RunTravelAction(pSprite, leaveAt) == CGameAIBase::ACTION_DONE);
    CHECK(g_pChitin->pActiveEngine == pWorldMap);
    CHECK(pWorldMap->m_nEngineState == 1);
    CHECK(pGame->GetGroup()->GetCount() == 0);

    g_pChitin->SelectEngine(pPrevious);
    pGame->SelectCharacter(pSprite->GetId(), FALSE);
}

// CScreenWorldMap::MoveToArea (0x69D0F0) in a host session (a single-player
// game is one): the party is placed in the fixed order of
// m_characters[0..5], the formation offset indexed by the slot itself
// (0x69D7E8-0x69D87D, 0x69DAE5-0x69DB07), an empty slot skipped without
// consuming a place; not in portrait order with a running offset. Here the
// sole member sits in fixed-order slot 1 with slot 0 empty and portrait 0
// still naming him, so the connected path stands him at the formation's
// second place, two squares east of the entry (0x8C6C08, turned by the entry
// point's direction), where the portrait order would stand him at the entry
// itself.
TEST_CASE("travel: a host session's MoveToArea places the party in the fixed order", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    const CPoint ptStart = pSprite->GetPos();

    // The member in fixed-order slot 1, slot 0 empty; the portrait order
    // (what the unconnected path reads) still names him at 0.
    const LONG nMember = pGame->m_characters[0];
    REQUIRE(pGame->m_characterPortraits[0] == nMember);
    pGame->m_characters[0] = CGameObjectArray::INVALID_INDEX;
    pGame->m_characters[1] = nMember;

    HostSession session;
    REQUIRE(session.Open());

    // Selecting the member in the new area activates it, which sets the
    // channel volumes: the mixer CChitin::InitializeServices (0x790240)
    // starts with the video.
    g_pChitin->cSoundMixer.Initialize(&g_pChitin->cWnd, 16, g_pChitin->GetNumberSoundChannels());
    CWarp* pEngine = g_pChitin->pActiveEngine;

    // AR1005's FR1000 entry point (165,224, facing 8): the small house's
    // floor is open around it, where AR1001's north edge band is walled and
    // the nearest-open search lands far from the point.
    const CResRef cResDest("AR1005");
    g_pBaldurChitin->m_pEngineWorldMap->MoveToArea(cResDest, CString("FR1000"), 8, FALSE, 0);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CGameArea* pHouse = pSprite->GetArea();
    REQUIRE(pHouse != NULL);
    REQUIRE(pHouse->m_resRef == cResDest);

    // FR1000 in search squares (0x69D13D-0x69D15E divides the point by the
    // grid).
    const INT nEntryX = 165 / CPathSearch::GRID_SQUARE_SIZEX;
    const INT nEntryY = 224 / CPathSearch::GRID_SQUARE_SIZEY;
    const CPoint sq(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
        pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY);
    INFO("member " << sq.x << "," << sq.y << " entry " << nEntryX << "," << nEntryY);
    // Slot 1: the formation's second place, the entry and two squares east.
    // The nearest open square shifts the entry's own blocked place two west
    // (136,222 against FR1000's 165,224, as the region travel's single
    // member lands).
    CHECK(std::abs(sq.x - (nEntryX + 2)) <= 1);
    CHECK(std::abs(sq.y - nEntryY) <= 1);
    // The portrait order's running offset would stand him at the entry's own
    // place (square 8, west of the entry).
    CHECK(sq.x > nEntryX);

    // Back to AR1000 and the slots for the other cases (one process may run
    // them all).
    pSprite->RemoveFromArea();
    pSprite->m_pos = ptStart;
    pSprite->AddToArea(pArea, ptStart, 0, CGameObject::LIST_FRONT);
    pGame->m_characters[0] = nMember;
    pGame->m_characters[1] = CGameObjectArray::INVALID_INDEX;
    pGame->UnselectAll();
    pGame->SelectCharacter(pSprite->GetId(), FALSE);
    CHECK(pGame->GetVisibleArea() == pArea);
    g_pChitin->pActiveEngine = pEngine;
    g_pChitin->cSoundMixer.CleanUp();
}

// Icewind586B70::sub_5872C0 (0x5872C0) and sub_586DC0 (0x586DC0): a member
// leaving unsummons what it summoned (the entries naming it as summoner):
// the entry is cleared and the creature marked for removal. Entries of
// others stay.
TEST_CASE("travel: a member leaving unsummons its summons", "[uimisc][travel][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    std::vector<BYTE> cre = FinishedCharacter();
    REQUIRE(!cre.empty());
    CGameSprite* pSummoned = new CGameSprite(cre.data(),
        static_cast<LONG>(cre.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    const CPoint ptSummoned(pSprite->GetPos().x + 3 * CPathSearch::GRID_SQUARE_SIZEX, pSprite->GetPos().y);
    pSummoned->m_pos = ptSummoned;
    pSummoned->AddToArea(pArea, ptSummoned, 0, CGameObject::LIST_FRONT);

    // The summon table's work runs in a host session; the creature is made
    // before it opens (a new sprite in a session registers with the remote
    // object array).
    HostSession session;
    REQUIRE(session.Open());

    Icewind586B70* pTable = Icewind586B70::Instance();
    pTable->Clear();
    pTable->mEntries[0] = Icewind586B70::Entry(pSummoned->GetId(), pSprite->GetId());
    // Another summoner's entry.
    pTable->mEntries[1] = Icewind586B70::Entry(12345, 67890);

    // Nothing for a sprite that summoned nothing.
    pTable->sub_5872C0(pSummoned);
    CHECK(pTable->mEntries[0].field_0 == pSummoned->GetId());
    CHECK_FALSE(pSummoned->m_removeFromArea);

    pTable->sub_5872C0(pSprite);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pTable->mEntries[0].field_0 == 0);
    CHECK(pTable->mEntries[0].field_4 == 0);
    CHECK(pSummoned->m_removeFromArea);
    CHECK(pTable->mEntries[1].field_0 == 12345);
    CHECK(pTable->mEntries[1].field_4 == 67890);

    pTable->Clear();
}

// Weather particles (ui-misc round 4): the original builds a new particle's
// position with the height rolled before the width (its arguments are
// evaluated right to left), in the generation (z, y, x) and in the respawn
// of a melted flake or a fallen drop (y, x).
namespace {

// The VC6 generator, as compat/msvc_rand rolls it for one thread.
class Vc6Rand {
public:
    explicit Vc6Rand(unsigned int seed)
        : m_state(seed)
    {
    }
    int operator()()
    {
        m_state = m_state * 214013u + 2531011u;
        return static_cast<int>((m_state >> 16) & 0x7FFF);
    }

private:
    unsigned int m_state;
};

} // namespace

// CSnowStorm::GenerateFlakes (0x5581B0): the 250 flakes' constructors roll
// their drift (two each), then each flake rolls z, y (0x5582A3) and x
// (0x5582BD).
TEST_CASE("weather: a new snow flake rolls its height before its width", "[uimisc][weather]")
{
    iwd2r_srand(7);
    const CPoint pt(320, 240);
    const CRect bounds(0, 0, 800, 433);
    CSnowStorm storm;
    storm.GenerateFlakes(pt, bounds);
    REQUIRE(storm.m_pSnowFlakes != NULL);

    Vc6Rand roll(7);
    for (int n = 0; n < 250; n++) {
        const bool bRight = (roll() & 1) != 0;
        INFO("flake " << n);
        CHECK(storm.m_pSnowFlakes[n].m_vel.x == (bRight ? 1 : -1) * CSnowFlake::DRIFT_VELOCITY);
        CHECK(storm.m_pSnowFlakes[n].m_nDriftWidth == 5 + roll() % 5);
    }
    for (int n = 0; n < 250; n++) {
        INFO("flake " << n);
        const int z = roll() % 100;
        const int y = 4 * pt.y / 3 + z + roll() % (4 * bounds.Height() / 3);
        const int x = pt.x + roll() % bounds.Width();
        CHECK(storm.m_pSnowFlakes[n].m_pos.z == z << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pSnowFlakes[n].m_pos.y == y << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pSnowFlakes[n].m_pos.x == x << CParticle::RESOLUTION_INC);
    }
}

// CRainStorm::GenerateDrops (0x557C30): the drops' constructors roll
// nothing; each drop rolls z, y (0x557D23) and x (0x557D3D).
TEST_CASE("weather: a new rain drop rolls its height before its width", "[uimisc][weather]")
{
    iwd2r_srand(11);
    const CPoint pt(64, 96);
    const CRect bounds(0, 0, 640, 400);
    CRainStorm storm;
    storm.GenerateDrops(pt, bounds);
    REQUIRE(storm.m_pRainDrops != NULL);

    Vc6Rand roll(11);
    for (int n = 0; n < 250; n++) {
        INFO("drop " << n);
        const int z = roll() % 100;
        const int y = 4 * pt.y / 3 + z + roll() % (4 * bounds.Height() / 3);
        const int x = pt.x + roll() % bounds.Width();
        CHECK(storm.m_pRainDrops[n].m_pos.z == z << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pRainDrops[n].m_pos.y == y << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pRainDrops[n].m_pos.x == x << CParticle::RESOLUTION_INC);
    }
}

// CSnowStorm::AsynchronousUpdate (0x557FC0): a melted flake comes back at
// the top of the view, rolling y (0x558120) before x (0x55813A).
TEST_CASE("weather: a melted snow flake comes back rolling its height first", "[uimisc][weather][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = pGame->GetVisibleArea();
    REQUIRE(pArea != NULL);
    // AR1000 is outdoors (area type 0x4), where the weather runs.
    REQUIRE((pArea->m_header.m_areaType & 0x4) != 0);
    CInfinity* pInfinity = pArea->GetInfinity();
    const CRect rView(pInfinity->nNewX,
        pInfinity->nNewY,
        pInfinity->nNewX + pInfinity->rViewPort.Width(),
        pInfinity->nNewY + pInfinity->rViewPort.Height());

    CSnowStorm storm;
    storm.GenerateFlakes(rView.TopLeft(), CRect(0, 0, rView.Width(), rView.Height()));
    REQUIRE(storm.m_pSnowFlakes != NULL);
    // Every flake on the ground with one update of melting left.
    for (int n = 0; n < 250; n++) {
        storm.m_pSnowFlakes[n].m_pos.z = 0;
        storm.m_pSnowFlakes[n].m_nMeltTime = 1;
    }

    const BOOLEAN bActive = pGame->GetWorldTimer()->m_active;
    pGame->GetWorldTimer()->m_active = TRUE;
    iwd2r_srand(5);
    storm.AsynchronousUpdate();
    pGame->GetWorldTimer()->m_active = bActive;

    Vc6Rand roll(5);
    for (int n = 0; n < 250; n++) {
        INFO("flake " << n);
        const int y = roll() % (4 * rView.bottom / 3 - 4 * rView.top / 3) + 4 * rView.top / 3 + 100;
        const int x = rView.left + roll() % rView.Width();
        CHECK(storm.m_pSnowFlakes[n].m_pos.y == y << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pSnowFlakes[n].m_pos.x == x << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pSnowFlakes[n].m_pos.z == 100 << CParticle::RESOLUTION_INC);
        CHECK(storm.m_pSnowFlakes[n].m_nMeltTime == 30);
    }
}

// CUIControlTextDisplay::DisplayString (0x4E20F0): a new string clears the
// scroll still to run for the last one (field_A65, at 0x4E2171) before it
// decides the top line.
TEST_CASE("text display: a new string ends the scroll still to run", "[uimisc][textdisplay][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    // A game, whose message log the display shows.
    if (PartyMember() == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CUIControlTextDisplay* pDisplay = static_cast<CUIControlTextDisplay*>(pWorld->GetManager()->GetPanel(pWorld->GetPanel_22_0())->GetControl(1));
    REQUIRE(pDisplay != NULL);
    REQUIRE(pDisplay->m_plstStrings != NULL);
    pDisplay->RemoveAll();

    pDisplay->DisplayString(CString(""), CString("first"), RGB(255, 255, 255), RGB(255, 255, 255), -1, FALSE, TRUE);
    pDisplay->field_A65 = 4;
    pDisplay->DisplayString(CString(""), CString("second"), RGB(255, 255, 255), RGB(255, 255, 255), -1, FALSE, TRUE);
    CHECK(pDisplay->field_A65 == 0);
    // The display does not overflow: the top line stays.
    CHECK(pDisplay->field_5A == 0);

    pDisplay->field_A65 = 4;
    pDisplay->DisplayString(CString(""), CString("third"), RGB(255, 255, 255), RGB(255, 255, 255), -1, TRUE, TRUE);
    CHECK(pDisplay->field_A65 == 0);
    // Moved to the top: the new string's index.
    CHECK(pDisplay->field_5A == 2);

    pDisplay->RemoveAll();
}

// CGameAreaNotes::DeleteANote (0x47BBD0): the list half the harness can reach
// honestly, without the map screen. An empty list and a missing id answer
// FALSE and leave the list alone; a connected client takes the message branch
// and returns before the UI, deferring the removal to the host's announce; the
// removal DeleteANote performs (delete the note, RemoveAt its node) is the same
// one sub_47B150 takes on an empty text, reached here through that helper.
// The host branch and the local removal need the live map panel (GetPanel(2),
// RemoveControl, RemoveNote) and the note's TLK override, which no honest
// harness here provides.
TEST_CASE("notes: DeleteANote's list walk and the removal helper", "[uimisc][notes]")
{
    // sub_47B150 constructs a STR_RES (a CSound), whose constructor takes the
    // chitin's mixer; a zeroed chitin stands in when no engine is running
    // (the same pattern as the FindOneOf case above).
    CChitin* pSaved = g_pChitin;
    std::unique_ptr<unsigned char[]> chitin;
    if (g_pChitin == NULL) {
        chitin.reset(new unsigned char[sizeof(CChitin)]());
        g_pChitin = reinterpret_cast<CChitin*>(chitin.get());
    }
    struct Restore {
        CChitin* pChitin;
        ~Restore() { g_pChitin = pChitin; }
    } restore { pSaved };

    // Raw storage: the destructor walks the map screen's controls.
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameAreaNotes)));
    std::memset(storage, 0, sizeof(CGameAreaNotes));
    auto* notes = reinterpret_cast<CGameAreaNotes*>(storage);
    notes->m_bInitialized = TRUE;

    CHECK(notes->DeleteANote(7) == FALSE); // empty list

    CAreaUserNote first = {};
    first.m_startX = 3;
    first.m_startY = 5;
    first.m_note = 100;
    first.m_id = 11;
    notes->AddANote(first);

    CAreaUserNote second = {};
    second.m_startX = 8;
    second.m_startY = 2;
    second.m_note = 200;
    second.m_id = 22;
    notes->AddANote(second);

    // Not in the list: FALSE and both notes stay, in order.
    CHECK(notes->DeleteANote(33) == FALSE);
    CHECK(notes->GetNoteButtonText(11) == 100);
    CHECK(notes->GetNoteButtonText(22) == 200);

    // A connected client with no announce in progress finds the note, takes
    // the message branch and returns TRUE without touching the list: the
    // host's announce handler owns the removal. A zeroed chitin has no
    // session, so SendAreaMapNoteToServer itself stops at its first guard
    // (and its Boolean is ignored here, as in the original).
    {
        CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
        std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
        g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
        g_pChitin->cNetwork.m_bConnectionEstablished = TRUE;
        g_pChitin->cNetwork.m_bIsHost = FALSE;
        g_pChitin->cNetwork.m_nHostPlayer = -1; // no host name, so the send stops at its name guard
        g_pBaldurChitin->m_cBaldurMessage.m_bInOnAreaMapNoteAnnounce = FALSE;

        CHECK(notes->DeleteANote(11) == TRUE);
        CHECK(notes->GetNoteButtonText(11) == 100); // still listed

        g_pChitin->cNetwork.m_bConnectionEstablished = FALSE;
        g_pBaldurChitin = pSavedBaldur;
    }

    // The removal primitive: sub_47B150 with no text deletes the note whose
    // square matches and takes its node out of the list.
    CHECK(notes->sub_47B150(8, 2, CString(""), 0, -1) == TRUE);
    CHECK(notes->GetNoteButtonText(22) == static_cast<STRREF>(-1));
    CHECK(notes->GetNoteButtonText(11) == 100);

    // A second call for the same square finds nothing left to remove.
    CHECK(notes->sub_47B150(8, 2, CString(""), 0, -1) == FALSE);

    // The list's ownership, without the destructor's map-screen path.
    POSITION pos = notes->m_areaNoteList.GetHeadPosition();
    while (pos != NULL) {
        delete notes->m_areaNoteList.GetNext(pos);
    }
    notes->m_areaNoteList.RemoveAll();
    ::operator delete(storage);
}

// CGameAreaNotes::Add (0x47B330, the six-argument overload behind the
// 0x47B090 wrapper): the list half the harness can reach honestly. With the
// cell uninitialised the note is added, updated in place on the same square
// (sub_47B150's non-empty path) or deleted there by an empty text; initialised
// but off the map screen the same list update runs and field_8E is set; a
// connected client sends and returns before touching the list. The map-screen
// tail (AddButton, the grid, GetControl/SequenceSet, the TLK entry) needs the
// live panel and is not faked.
TEST_CASE("notes: Add writes and updates the list without the map screen", "[uimisc][notes]")
{
    CChitin* pSavedChitin = g_pChitin;
    CBaldurChitin* pSavedBaldur = g_pBaldurChitin;
    std::unique_ptr<unsigned char[]> chitinStorage(new unsigned char[sizeof(CBaldurChitin)]());
    g_pChitin = reinterpret_cast<CChitin*>(chitinStorage.get());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(chitinStorage.get());
    struct Restore {
        CChitin* pChitin;
        CBaldurChitin* pBaldur;
        ~Restore()
        {
            g_pChitin = pChitin;
            g_pBaldurChitin = pBaldur;
        }
    } restore { pSavedChitin, pSavedBaldur };

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameAreaNotes)));
    std::memset(storage, 0, sizeof(CGameAreaNotes));
    auto* notes = reinterpret_cast<CGameAreaNotes*>(storage);

    // Uninitialised, no session: a new note in the list with the given text
    // reference (a non--1 reference skips the TLK Add, whose files are shut).
    notes->m_bInitialized = FALSE;
    notes->Add(CString("AR1000"), 4, 6, CString("first"), 0x00020000, 42);
    REQUIRE(notes->m_areaNoteList.GetCount() == 1);
    {
        CAreaUserNote* pNote = notes->m_areaNoteList.GetHead();
        CHECK(pNote->m_startX == 4);
        CHECK(pNote->m_startY == 6);
        CHECK(pNote->m_note == 42);
        CHECK(pNote->m_dwflags == 0x00020000);
    }
    // The same square again updates it in place: one note, the new flags and
    // reference, the text replaced through sub_47B150.
    notes->Add(CString("AR1000"), 4, 6, CString("second"), 0x00030000, 43);
    CHECK(notes->m_areaNoteList.GetCount() == 1);
    {
        CAreaUserNote* pNote = notes->m_areaNoteList.GetHead();
        CHECK(pNote->m_dwflags == 0x00030000);
        CHECK(pNote->m_note == 43);
    }

    // An empty text on that square is the helper's delete: the note goes.
    notes->Add(CString("AR1000"), 4, 6, CString(), 0, 44);
    CHECK(notes->m_areaNoteList.GetCount() == 0);

    // Initialised, off the map screen (the active engine is not m_pEngineMap,
    // which stays null): the note is added and field_8E marks the map stale.
    notes->m_bInitialized = TRUE;
    notes->m_ptCellSize.x = 1;
    notes->m_ptCellSize.y = 1;
    notes->m_ptButtonSize.x = 1;
    notes->m_ptButtonSize.y = 1;
    notes->m_areaNoteGrid = new DWORD*[1];
    notes->m_areaNoteGrid[0] = new DWORD[1];
    notes->m_areaNoteGrid[0][0] = -1;
    static char sActiveEngine;
    g_pChitin->cNetwork.m_bConnectionEstablished = TRUE;
    g_pChitin->cNetwork.m_bIsHost = FALSE;
    g_pChitin->pActiveEngine = reinterpret_cast<CWarp*>(&sActiveEngine);

    // A client inside an announce: no send, so the list logic runs.
    g_pBaldurChitin->m_cBaldurMessage.m_bInOnAreaMapNoteAnnounce = TRUE;

    notes->Add(CString("AR1000"), 0, 0, CString("map off"), 0x00010000, 50);
    CHECK(notes->m_areaNoteList.GetCount() == 1);
    CHECK(notes->field_8E == 1);
    CHECK(notes->GetNoteButtonText(0) == 50);

    // The same square now matches the helper first, so nothing is added (the
    // grid stays -1: AddButton never ran) and field_8E is set again.
    notes->Add(CString("AR1000"), 0, 0, CString("again"), 0x00010000, 51);
    CHECK(notes->m_areaNoteList.GetCount() == 1);

    // A connected client with no announce in progress returns after the send,
    // before the list: a new square adds nothing. (No host name in the zeroed
    // session, so the send itself stops at its first name guard.)
    g_pBaldurChitin->m_cBaldurMessage.m_bInOnAreaMapNoteAnnounce = FALSE;
    g_pChitin->cNetwork.m_nHostPlayer = -1;
    notes->Add(CString("AR1000"), 9, 9, CString("client"), 0x00010000, 52);
    CHECK(notes->m_areaNoteList.GetCount() == 1);

    // Ownership, without the destructor's map-screen path.
    POSITION pos = notes->m_areaNoteList.GetHeadPosition();
    while (pos != NULL) {
        delete notes->m_areaNoteList.GetNext(pos);
    }
    notes->m_areaNoteList.RemoveAll();
    delete[] notes->m_areaNoteGrid[0];
    delete[] notes->m_areaNoteGrid;
    ::operator delete(storage);
}

// CImm::sub_7C3020 (0x7C3020), the composition window's rect (report-T80.md
// §2.10; WSO calls it GetCompositionWindowRect): the mosaic's size from a
// synthetic MOS resource, the height three lines of the edit's font plus the
// padding either side clamped to the mosaic's height, and the placement
// against the panel and the edit. The mosaic's GetSize Demands and Releases
// its resource, so the synthetic CResMosaic is flagged loaded (0x4|0x10) and
// carries a high m_nID: CDimm hands back m_pData without touching a file
// (InternalDemand's shortcut), on a zeroed chitin or the engine's alike.
TEST_CASE("ime: the composition window's rect follows the font, the mosaic and the panel", "[uimisc][ime]")
{
    CChitin* pSaved = g_pChitin;
    std::unique_ptr<unsigned char[]> chitinStorage;
    if (g_pChitin == NULL) {
        chitinStorage.reset(new unsigned char[sizeof(CChitin)]());
        g_pChitin = reinterpret_cast<CChitin*>(chitinStorage.get());
    }
    struct Restore {
        CChitin* pChitin;
        ~Restore() { g_pChitin = pChitin; }
    } restore { pSaved };

    // A 0x40-byte MOS blob: the magic and the version Parse checks (0x7805D0),
    // then the header's sizes. Parse only computes pointers from the rest.
    std::vector<BYTE> mos(0x40, 0);
    std::memcpy(&mos[0], "MOS ", 4);
    std::memcpy(&mos[4], "V1  ", 4);
    const WORD nXTiles = 1;
    const WORD nYTiles = 1;
    const DWORD nPaletteOffset = 0x18;
    std::memcpy(&mos[0xC], &nXTiles, 2);
    std::memcpy(&mos[0xE], &nYTiles, 2);
    std::memcpy(&mos[0x14], &nPaletteOffset, 4);

    auto* resStorage = new unsigned char[sizeof(CResMosaic)]();
    auto* res = reinterpret_cast<CResMosaic*>(resStorage);
    res->dwFlags = CRes::RES_FLAG_0x04 | CRes::RES_FLAG_0x10;
    res->m_pData = mos.data();
    res->nSize = static_cast<DWORD>(mos.size());
    res->field_18 = 0;
    res->field_44 = 1;
    res->m_nID = 0xFF000000;

    auto* mosaicStorage = new unsigned char[sizeof(CVidMosaic)]();
    auto* mosaic = reinterpret_cast<CVidMosaic*>(mosaicStorage);
    mosaic->pRes = res;
    mosaic->m_bDoubleSize = FALSE;

    auto* immStorage = new unsigned char[sizeof(CImm)]();
    auto* imm = reinterpret_cast<CImm*>(immStorage);
    imm->field_13C = mosaic;

    const auto setMosaic = [&mos](WORD cx, WORD cy) {
        std::memcpy(&mos[8], &cx, 2);
        std::memcpy(&mos[0xA], &cy, 2);
    };
    const auto rect = [imm](CPoint ptPanel, CSize panelSize, CPoint ptEdit, CSize editSize, SHORT nFontHeight) {
        return imm->sub_7C3020(ptPanel, panelSize, ptEdit, editSize, nFontHeight);
    };

    // No mosaic: an empty rect.
    imm->field_13C = NULL;
    CHECK(rect(CPoint(10, 20), CSize(200, 150), CPoint(30, 5), CSize(60, 10), 10) == CRect(0, 0, 0, 0));
    imm->field_13C = mosaic;

    // The font asks 10*3 + 3*2 = 36, the mosaic is 40x12: the height clamps
    // to 12; the edit leaves room below the panel's top, so the rect sits
    // under it (top 20 + 10 + 5); the edit is wide enough (60 - 40 + 30 >= 0),
    // so the left is the panel's plus the edit's size and offset
    // ((10 - 40) + 60 + 30).
    setMosaic(40, 12);
    CHECK(rect(CPoint(10, 20), CSize(200, 150), CPoint(30, 5), CSize(60, 10), 10) == CRect(60, 35, 100, 47));

    // The mosaic fits the panel's height (12 against 10): the rect starts at
    // the panel's corner (no offset down); left as above.
    CHECK(rect(CPoint(1, 7), CSize(200, 10), CPoint(30, 5), CSize(60, 10), 2) == CRect(51, 7, 91, 19));

    // The edit is tall enough to reach past the panel with the rect (100 <
    // 60 + 50 + 12) and starts below its top (50 - 12 >= 0): the rect goes
    // above the edit's top (0 - 12 + 50); left ((0 - 40) + 60 + 0).
    CHECK(rect(CPoint(0, 0), CSize(200, 100), CPoint(0, 50), CSize(60, 60), 2) == CRect(20, 38, 60, 50));

    // The edit is so tall that the rect would end past the panel (100 <
    // 90 + 2 + 12) and starts near its top (2 - 12 < 0): the rect centres in
    // the panel's height ((100 - 12) / 2 + 5).
    CHECK(rect(CPoint(5, 5), CSize(200, 100), CPoint(0, 2), CSize(60, 90), 2) == CRect(25, 49, 65, 61));

    // The mosaic is wider than the panel (60 against 50): the left is the
    // panel's own; top as the room-below case (4 + 20 + 0).
    setMosaic(60, 12);
    CHECK(rect(CPoint(3, 4), CSize(50, 100), CPoint(0, 0), CSize(60, 20), 2) == CRect(3, 24, 63, 36));

    // The edit is narrower than the mosaic (10 - 40 + 0 < 0) and the panel
    // reaches past it (50 < 0 + 40 is false): left + the edit's x (3 + 0).
    setMosaic(40, 12);
    CHECK(rect(CPoint(3, 4), CSize(50, 100), CPoint(0, 0), CSize(10, 20), 2) == CRect(3, 24, 43, 36));

    // The same, but the edit starts before the mosaic's end (50 < 20 + 40)
    // and is too narrow to reach it (10 - 40 + 20 < 0): the mosaic centres
    // in the panel's width ((50 - 40) / 2 + 3).
    CHECK(rect(CPoint(3, 4), CSize(50, 100), CPoint(20, 0), CSize(10, 20), 2) == CRect(8, 24, 48, 36));

    // A mosaic shorter than the font asks (8 against 12): the height clamps
    // down to the mosaic's own.
    setMosaic(40, 8);
    CHECK(rect(CPoint(0, 0), CSize(200, 50), CPoint(0, 0), CSize(60, 10), 2) == CRect(20, 10, 60, 18));
}

// CUtil::SplitString (0x780CF0): the line loop's Double Byte Character
// Support. With g_pChitin->field_1A0 on, a lead byte takes its trail byte with
// it (the pair is measured as one unit, and the trail byte is never tested for
// a break), and the line budget loses 10 px. The font fixture is a BAM with
// 256 frames: frame 0 (what every single byte uses) is 5 px, frame 0x40 (a
// lead byte whose trail is 'A') is 100 px, so a pair can overrun the budget
// while the string left of it does not.
TEST_CASE("util: SplitString keeps a double-byte pair together and budgets 10 px less", "[uimisc][util]")
{
    // A chitin with a constructed m_sFontName: SplitString asks the font's
    // GetStringLength, which reads the chitin's font name to choose the BAM
    // path (the table lookup needs the engine's threads, which do not run
    // here, so a bare chitin like FindOneOf's cannot hold its CString).
    class SplitStringChitin : public CChitin {
    public:
        void MessageThreadMain(void*) override { }
        void RSThreadMain(void*) override { }
        void MainAIThread(void*) override { }
        void MusicThreadMain(void*) override { }
    };
    SplitStringChitin chitin;
    CChitin* pSaved = g_pChitin;
    g_pChitin = &chitin;
    struct Restore {
        CChitin* pChitin;
        ~Restore()
        {
            iwd2r_SetTestLeadBytes(nullptr);
            g_pChitin = pChitin;
        }
    } restore { pSaved };
    chitin.m_sFontName = "";
    chitin.field_1A0 = 0;

    const int kFrames = 256;
    const DWORD nTable = 0x18;
    const DWORD nSequences = nTable + kFrames * sizeof(FRAMEENTRY);
    const DWORD nFrameList = nSequences + sizeof(SEQUENCEENTRY);
    const DWORD nPalette = nFrameList + kFrames * sizeof(WORD);
    std::vector<BYTE> bytes(nPalette + 1024, 0);
    BAMHEADER header = {};
    std::memcpy(&header.nFileType, "BAM ", 4);
    std::memcpy(&header.nFileVersion, "V1  ", 4);
    header.nFrames = kFrames;
    header.nSequences = 1;
    header.nTableOffset = nTable;
    header.nPaletteOffset = nPalette;
    header.nFrameListOffset = nFrameList;
    std::memcpy(bytes.data(), &header, sizeof(header));
    for (int i = 0; i < kFrames; i++) {
        FRAMEENTRY frame = {};
        frame.nWidth = i == 0x40 ? 100 : 5;
        frame.nHeight = 7;
        std::memcpy(bytes.data() + nTable + i * sizeof(FRAMEENTRY), &frame, sizeof(frame));
        const WORD nFrame = static_cast<WORD>(i);
        std::memcpy(bytes.data() + nFrameList + i * sizeof(WORD), &nFrame, sizeof(nFrame));
    }
    SEQUENCEENTRY sequence = { static_cast<SHORT>(kFrames), 0 };
    std::memcpy(bytes.data() + nSequences, &sequence, sizeof(sequence));

    CResCell res;
    res.m_bCacheHeader = FALSE;
    REQUIRE(res.Parse(bytes.data()));
    CVidFont font;
    static_cast<CResHelper<CResCell, 1000>&>(font).pRes = &res;

    // Shift-JIS's lead bytes (0x81-0x9F, 0xE0-0xFC), for the pair below.
    static bool leadBytes[256] = {};
    for (int c = 0x81; c <= 0x9F; c++)
        leadBytes[c] = true;
    for (int c = 0xE0; c <= 0xFC; c++)
        leadBytes[c] = true;

    auto split = [&](const char* text, WORD lineLength, BYTE maxStrings, BOOL divideWords, BOOL checkRemainder,
                     WORD suggested) {
        CString out[8];
        const BYTE n = CUtil::SplitString(&font, CString(text), lineLength, maxStrings, out, divideWords, TRUE,
            checkRemainder, suggested);
        std::vector<std::string> lines;
        for (BYTE i = 0; i < n; i++) {
            lines.push_back(static_cast<const char*>(out[i]));
        }
        return lines;
    };

    // The single-byte arm: 5 px per character.
    iwd2r_SetTestLeadBytes(nullptr);
    CHECK(split("abcdef", 10, 3, TRUE, FALSE, 0) == std::vector<std::string> { "ab", "cd", "ef" });
    CHECK(split("ab\ncd", 100, 3, TRUE, FALSE, 0) == std::vector<std::string> { "ab", "cd" });

    // Double Byte Character Support reserves 10 px: the same string fits two
    // characters per 15 px instead of three.
    chitin.field_1A0 = 0;
    CHECK(split("abcdef", 15, 2, TRUE, FALSE, 0) == std::vector<std::string> { "abc", "def" });
    chitin.field_1A0 = 1;
    CHECK(split("abcdef", 15, 2, TRUE, FALSE, 0) == std::vector<std::string> { "a", "bcdef" });

    // Shift-JIS lead byte 0x81 with trail 'A': as a pair it is 100 px, so
    // with the 5 px budget left it moves to the next line whole. Without the
    // support the loop would stop on 'A' (the pair measures 100 px there) and
    // break it into "\x81" and "A"; with the flag off the two bytes are just
    // 5 px each and both fit.
    iwd2r_SetTestLeadBytes(leadBytes);
    chitin.field_1A0 = 1;
    CHECK(split("\x81"
                "A",
              15, 2, TRUE, FALSE, 0)
        == std::vector<std::string> { "", "\x81"
                                          "A" });
    chitin.field_1A0 = 0;
    CHECK(split("\x81"
                "A",
              15, 2, TRUE, FALSE, 0)
        == std::vector<std::string> { "\x81"
                                      "A" });

    static_cast<CResHelper<CResCell, 1000>&>(font).pRes = NULL;
}

// CBaldurEngine::OnMouseWheel (0x4289C0): the tail after the scroll-bar loop.
// With no current scroll bar the two remembered bars (0xFE/0x102) take the
// wheel when both exist - the forward one for a forward tick, the backward one
// otherwise - and only when the chosen bar's panel and the bar itself are
// active and nTicks is positive; each tick then calls the bar's +0x78 slot
// twice. The object's vtable is a synthetic array whose entries all point at
// the recorder below (the oracle case does the same on the original's side):
// the slot index is the class's own, so this also pins that the port calls
// OnScrollButtonUp's slot and passes the repaired direction 0. The guards are
// pinned by poisoned pointers that a wrong branch would dereference.
namespace {

int sBarCalls;
const void* sCalledBar;
int sCalledDirection;

#if defined(_MSC_VER)
void __fastcall OnScrollButtonUpProbe(void* self, void*, unsigned char nDirection)
#else
void OnScrollButtonUpProbe(void* self, unsigned char nDirection)
#endif
{
    sBarCalls++;
    sCalledBar = self;
    sCalledDirection = nDirection;
}

} // namespace

TEST_CASE("engine: OnMouseWheel's remembered scroll bars", "[uimisc][engine]")
{
    std::vector<unsigned char> engineStorage(sizeof(CBaldurEngine), 0);
    auto* engine = reinterpret_cast<CBaldurEngine*>(engineStorage.data());
    std::vector<unsigned char> panelStorage(sizeof(CUIPanel), 0);
    auto* panel = reinterpret_cast<CUIPanel*>(panelStorage.data());
    std::vector<unsigned char> barStorage(sizeof(CUIControlScrollBar), 0);
    auto* bar = reinterpret_cast<CUIControlScrollBar*>(barStorage.data());
    auto* barBase = reinterpret_cast<CUIControlBase*>(bar);
    barBase->m_pPanel = panel;

    void* vtable[33] = {};
    for (void*& entry : vtable) {
        entry = reinterpret_cast<void*>(&OnScrollButtonUpProbe);
    }
    *reinterpret_cast<void***>(barStorage.data()) = vtable;

    auto* poison = reinterpret_cast<CUIControlScrollBar*>(static_cast<uintptr_t>(1));

    // nLines == 0 returns before anything is read.
    engine->m_pCurrentScrollBar = poison;
    engine->field_FE = poison;
    engine->field_102 = poison;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 0, 0);
    CHECK(sBarCalls == 0);

    // A current scroll bar returns after its own loop even when it is inactive:
    // the tail must not look at the poisoned 0xFE/0x102.
    engine->m_pCurrentScrollBar = bar;
    panel->m_bActive = FALSE;
    barBase->m_bActive = TRUE;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 1, 0);
    CHECK(sBarCalls == 0);

    // The tail needs both remembered bars.
    engine->m_pCurrentScrollBar = NULL;
    engine->field_FE = bar;
    engine->field_102 = NULL;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 1, 0);
    CHECK(sBarCalls == 0);
    engine->field_FE = NULL;
    engine->field_102 = bar;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 1, 0);
    CHECK(sBarCalls == 0);

    // A forward tick consults field_FE, a backward one field_102: the other is
    // poisoned and must not be dereferenced.
    panel->m_bActive = FALSE;
    barBase->m_bActive = TRUE;
    engine->field_FE = bar;
    engine->field_102 = poison;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 1, 0);
    engine->field_FE = poison;
    engine->field_102 = bar;
    engine->CBaldurEngine::OnMouseWheel(FALSE, 2, 1, 0);
    CHECK(sBarCalls == 0);

    // The call path: an active panel and bar with a positive tick count calls
    // the slot twice a tick, on the bar the direction selects, with the
    // direction the port passes (0: the exe pushes nothing there).
    engine->field_FE = bar;
    engine->field_102 = poison;
    panel->m_bActive = TRUE;
    barBase->m_bActive = TRUE;
    sBarCalls = 0;
    sCalledBar = NULL;
    sCalledDirection = -1;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 3, 2, 0);
    CHECK(sBarCalls == 6);
    CHECK(sCalledBar == bar);
    CHECK(sCalledDirection == 0);

    engine->field_FE = poison;
    engine->field_102 = bar;
    sBarCalls = 0;
    engine->CBaldurEngine::OnMouseWheel(FALSE, 2, 2, 0);
    CHECK(sBarCalls == 4);
    CHECK(sCalledBar == bar);

    // Neither an inactive panel, nor an inactive bar, nor a non-positive tick
    // count reaches the calls.
    sBarCalls = 0;
    engine->field_FE = bar;
    engine->field_102 = bar;
    panel->m_bActive = FALSE;
    barBase->m_bActive = TRUE;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 1, 0);
    panel->m_bActive = TRUE;
    barBase->m_bActive = FALSE;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 2, 1, 0);
    barBase->m_bActive = TRUE;
    engine->CBaldurEngine::OnMouseWheel(TRUE, 0, 1, 0);
    engine->CBaldurEngine::OnMouseWheel(TRUE, -3, 1, 0);
    CHECK(sBarCalls == 0);

    // The fields and the bar are left as they were.
    CHECK(engine->field_FE == bar);
    CHECK(engine->field_102 == bar);
    CHECK(barBase->m_bActive == TRUE);
}
