#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameEffectList.h"
#include "CGameSprite.h"
#include "CImmunities.h"
#include "CInfGame.h"
#include "CMemINI.h"
#include "CScreenCreateChar.h"
#include "CSpawn.h"
#include "IcewindCGameEffects.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The effects' ApplyEffect on a sprite made from CHARBASE.CRE, the way
// tests/test_cre.cpp makes one: the engine in this process up to the
// Character Generation screen, built once and leaked.

namespace {

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
    const char* install = std::getenv("IWD2_DATA");
    if (install == nullptr || *install == '\0') {
        startError = "IWD2_DATA unset";
        return false;
    }
    const std::string original = install;
    view = std::make_unique<DataView>(original);
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
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

std::vector<BYTE> LoadCharBase()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

// A fresh sprite from CHARBASE.CRE (StartCreateChar's arguments, 0x610540).
CGameSprite* MakeSprite()
{
    static std::vector<BYTE> bytes = LoadCharBase();
    std::vector<BYTE> copy = bytes;
    return new CGameSprite(copy.data(),
        static_cast<LONG>(copy.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
}

#define REQUIRE_ENGINE()                            \
    do {                                            \
        const char* env = std::getenv("IWD2_DATA"); \
        if (env == nullptr || *env == '\0') {       \
            SKIP("[iwd2r-skip] IWD2_DATA unset");   \
        }                                           \
        std::string error;                          \
        INFO(error);                                \
        REQUIRE(StartEngine(error));                \
    } while (0)

// An effect through the original's factory (DecodeEffect 0x49E0E0).
CGameEffect* Make(WORD effectID, DWORD dwFlags, LONG amount, WORD durationType, DWORD duration = 0)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    effect.effectID = effectID;
    effect.dwFlags = dwFlags;
    effect.effectAmount = amount;
    effect.durationType = durationType;
    effect.duration = duration;
    effect.probabilityUpper = 100;
    return CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
}

// Applies one effect and returns its m_done.
BOOL Apply(CGameSprite* pSprite, WORD effectID, DWORD dwFlags, LONG amount, WORD durationType, BOOL secondary = FALSE)
{
    CGameEffect* pEffect = Make(effectID, dwFlags, amount, durationType);
    pEffect->m_secondaryType = secondary;
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    BOOL done = pEffect->m_done;
    delete pEffect;
    return done;
}

}

TEST_CASE("stat modifier effects follow the original's modes", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SECTION("number of attacks (1) clamps to 5 and keeps at least 1 for a percentage")
    {
        pSprite->m_derivedStats.m_nNumberOfAttacks = 2;
        CHECK(Apply(pSprite, CGAMEEFFECT_ATTACKNO, 0, 7, 0) == FALSE);
        CHECK(pSprite->m_derivedStats.m_nNumberOfAttacks == 5);

        pSprite->m_baseStats.m_numberOfAttacksBase = 3;
        CHECK(Apply(pSprite, CGAMEEFFECT_ATTACKNO, 0, 1, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_numberOfAttacksBase == 4);

        pSprite->m_derivedStats.m_nNumberOfAttacks = 2;
        pSprite->m_baseStats.m_numberOfAttacksBase = 1;
        Apply(pSprite, CGAMEEFFECT_ATTACKNO, 2, 50, 0);
        CHECK(pSprite->m_derivedStats.m_nNumberOfAttacks == 1);
    }

    SECTION("non-cumulative luck (133) sign-extends the amount's low byte")
    {
        // 0x4BAFC0: movsx from the amount's low byte, added to the word.
        pSprite->GetDerivedStats()->m_nLuck = 0;
        Apply(pSprite, CGAMEEFFECT_NON_CUMULATIVELUCK, 0, -2, 0);
        CHECK(pSprite->GetDerivedStats()->m_nLuck == -2);
        Apply(pSprite, CGAMEEFFECT_NON_CUMULATIVELUCK, 0, 3, 0);
        CHECK(pSprite->GetDerivedStats()->m_nLuck == 1);
    }

    SECTION("charisma (6) counts a temporary bonus once, by its excess")
    {
        pSprite->m_derivedStats.field_13C = 0;
        SHORT bonus = pSprite->m_bonusStats.m_nCHR;
        Apply(pSprite, CGAMEEFFECT_CHR, 0, 2, 0);
        Apply(pSprite, CGAMEEFFECT_CHR, 0, 3, 0);
        Apply(pSprite, CGAMEEFFECT_CHR, 0, 1, 0);
        CHECK(pSprite->m_bonusStats.m_nCHR == bonus + 3);
        CHECK(pSprite->m_derivedStats.field_13C == 3);

        CHECK(Apply(pSprite, CGAMEEFFECT_CHR, 1, 55, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_CHRBase == 40);
        Apply(pSprite, CGAMEEFFECT_CHR, 1, -4, 0);
        CHECK(pSprite->m_derivedStats.m_nCHR == 1);
    }

    SECTION("luck (22) clamps to -20..20; mode 1 reads the amount unsigned")
    {
        Apply(pSprite, CGAMEEFFECT_LUCK, 1, -3, 0);
        CHECK(pSprite->m_derivedStats.m_nLuck == 20);
        Apply(pSprite, CGAMEEFFECT_LUCK, 1, -30, 1);
        CHECK(pSprite->m_baseStats.m_luckBase == -20);
        SHORT bonus = pSprite->m_bonusStats.m_nLuck;
        Apply(pSprite, CGAMEEFFECT_LUCK, 0, 2, 0);
        CHECK(pSprite->m_bonusStats.m_nLuck == bonus + 2);
    }

    SECTION("base attack bonus (54) keeps 1..40")
    {
        pSprite->m_baseStats.m_attackBase = 5;
        Apply(pSprite, CGAMEEFFECT_BASEATTACKBONUS, 0, 3, 1);
        CHECK(pSprite->m_baseStats.m_attackBase == 8);
        Apply(pSprite, CGAMEEFFECT_BASEATTACKBONUS, 1, 0, 0);
        CHECK(pSprite->m_derivedStats.m_nTHAC0 == 1);
        Apply(pSprite, CGAMEEFFECT_BASEATTACKBONUS, 2, 50, 0);
        CHECK(pSprite->m_derivedStats.m_nTHAC0 == 4);
    }

    SECTION("lore (21) and alchemy (61) work on skills 8 and 0, kept in 0..100")
    {
        pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 10;
        Apply(pSprite, CGAMEEFFECT_LORE, 2, 50, 0);
        CHECK(pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] == 5);
        Apply(pSprite, CGAMEEFFECT_LORE, 1, -5, 1);
        CHECK(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] == 0);

        signed char bonus = pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY];
        Apply(pSprite, CGAMEEFFECT_SKILLALCHEMY, 0, 4, 0);
        CHECK(pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] == bonus + 4);
        Apply(pSprite, CGAMEEFFECT_SKILLALCHEMY, 0, 120, 1);
        CHECK(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] == 100);
    }

    delete pSprite;
}

TEST_CASE("wizard spell slots (42) add known spells for the flagged level", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // The effect indexes the game's spell table by the memorised entry's id.
    REQUIRE(pGame->m_spells.m_nCount >= 2);

    // One memorised spell at level 0 with a slot (field_4 = 1), and one
    // without any slot (field_4 = 0), which the original skips.
    CGameSpriteGroupedSpellList* pSpells = pSprite->m_derivedStats.sub_447B00(CAIObjectType::C_BARD);
    pSpells->Add(0, 0, 1, 1, 0);
    pSpells->Add(1, 0, 0, 0, 0);
    REQUIRE(pSpells->m_nHighestLevel == 1);

    // The class walks test the AI type's class mask (one bit per class id).
    new (&pSprite->m_typeAI) CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1 << (CAIObjectType::C_WIZARD - 1));

    CGameSpriteSpellList* pKnown = pSprite->m_spells.m_spellsByClass[pGame->GetSpellcasterIndex(CAIObjectType::C_WIZARD)].GetSpellsAtLevel(0);

    // Nothing flagged: nothing is added, and the original does not set m_done.
    CHECK(Apply(pSprite, CGAMEEFFECT_SPELLMEMORIZATIONMAGE, 0, 2, 0) == FALSE);
    CHECK(pKnown->m_List.size() == 0);

    // Level 0 flagged: two extra slots per memorised spell, skipping the
    // slotless entry.
    CHECK(Apply(pSprite, CGAMEEFFECT_SPELLMEMORIZATIONMAGE, 1, 2, 0) == FALSE);

    CHECK(pKnown->m_List.size() == 1);
    CHECK(pKnown->m_List[0].m_nID == 0);
    CHECK(pKnown->m_List[0].field_4 == 2);
    CHECK(pKnown->m_List[0].field_8 == 2);
    CHECK(pKnown->field_10 == 2);

    // The other classes of the group are gated by the same mask.
    CHECK(pSprite->m_spells.m_spellsByClass[pGame->GetSpellcasterIndex(CAIObjectType::C_SORCERER)].m_lists[0].m_List.size() == 0);
}

TEST_CASE("priest spell slots (62) add known spells for the flagged level", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    REQUIRE(pGame->m_spells.m_nCount >= 2);

    // The priest effect reads the cleric's list (the group's lowest class).
    CGameSpriteGroupedSpellList* pSpells = pSprite->m_derivedStats.sub_447B00(CAIObjectType::C_CLERIC);
    pSpells->Add(0, 0, 1, 1, 0);
    pSpells->Add(1, 0, 0, 0, 0);
    REQUIRE(pSpells->m_nHighestLevel == 1);

    new (&pSprite->m_typeAI) CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1 << (CAIObjectType::C_CLERIC - 1));

    CGameSpriteSpellList* pKnown = pSprite->m_spells.m_spellsByClass[pGame->GetSpellcasterIndex(CAIObjectType::C_CLERIC)].GetSpellsAtLevel(0);

    CHECK(Apply(pSprite, CGAMEEFFECT_SPELLMEMOIZATIONCLERIC, 0, 2, 0) == FALSE);
    CHECK(pKnown->m_List.size() == 0);

    CHECK(Apply(pSprite, CGAMEEFFECT_SPELLMEMOIZATIONCLERIC, 1, 2, 0) == FALSE);

    CHECK(pKnown->m_List.size() == 1);
    CHECK(pKnown->m_List[0].m_nID == 0);
    CHECK(pKnown->m_List[0].field_4 == 2);
    CHECK(pKnown->m_List[0].field_8 == 2);
    CHECK(pKnown->field_10 == 2);

    CHECK(pSprite->m_spells.m_spellsByClass[pGame->GetSpellcasterIndex(CAIObjectType::C_DRUID)].m_lists[0].m_List.size() == 0);
}

TEST_CASE("find familiar (192) refuses non-protagonists and a second familiar", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // A caster that is not the protagonist is refused after the strref 0x215a
    // message, with m_done set and no familiar created (0x4b9eb0).
    pSprite->m_id = pGame->GetProtagonist() + 1;
    int nFamiliars = pGame->m_familiars.GetCount();
    CHECK(Apply(pSprite, CGAMEEFFECT_FINDFAMILIAR, 0, 0, 0) == TRUE);
    CHECK(pGame->m_familiars.GetCount() == nFamiliars);

    // With the game-wide familiar flag (field_4204) set, even the protagonist
    // is refused (strref 0x2159). The flag is restored afterwards: the summon
    // path itself needs an area to place the familiar in and is not reachable
    // from this fixture.
    pSprite->m_id = pGame->GetProtagonist();
    pGame->field_4204 = 1;
    CHECK(Apply(pSprite, CGAMEEFFECT_FINDFAMILIAR, 0, 0, 0) == TRUE);
    CHECK(pGame->m_familiars.GetCount() == nFamiliars);
    pGame->field_4204 = 0;
}

TEST_CASE("maximum hit point effects (18)", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_baseStats.m_hitPoints = 10;
    pSprite->m_baseStats.m_maxHitPointsBase = 10;
    pSprite->m_derivedStats.m_nMaxHitPoints = 10;
    pSprite->m_tempStats.m_generalState = 0;

    SECTION("mode 0, permanent, raises current and maximum")
    {
        CHECK(Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 15);
        CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 15);
        CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    }

    SECTION("mode 0, temporary, raises current hit points only with the secondary flag")
    {
        Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 0);
        CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 15);
        CHECK(pSprite->m_baseStats.m_hitPoints == 10);
        Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 0, TRUE);
        CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    }

    SECTION("mode 2 heals the gain over the previous maximum")
    {
        pSprite->m_tempStats.m_nMaxHitPoints = 10;
        Apply(pSprite, CGAMEEFFECT_HITPOINTS, 2, 150, 0);
        CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 15);
        CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    }

    SECTION("a dead sprite is left alone")
    {
        pSprite->m_baseStats.m_hitPoints = 0;
        CHECK(Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 10);
    }

    delete pSprite;
}

TEST_CASE("condition effects set the original's state bits", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SECTION("invisibility (20)")
    {
        Apply(pSprite, CGAMEEFFECT_INVISIBLE, 0, 0, 0);
        CHECK((pSprite->m_derivedStats.m_generalState & STATE_INVISIBLE) != 0);
        CHECK(pSprite->m_derivedStats.m_visualEffects[IWD_VFX_INVISIBILITY]);
        CHECK(pSprite->m_portraitIcons.Find(reinterpret_cast<int*>(110)) != NULL);

        Apply(pSprite, CGAMEEFFECT_INVISIBLE, 1, 0, 1);
        CHECK((pSprite->m_baseStats.m_generalState & (STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY)) == (STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY));
    }

    SECTION("berserk (3) and blood rage")
    {
        Apply(pSprite, CGAMEEFFECT_BERSERK, 1, 0, 0);
        CHECK((pSprite->m_derivedStats.m_generalState & STATE_BERSERK) != 0);
        CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_BERSERK_ALWAYS_ACTIVE));

        pSprite->m_bonusStats.field_128 = 0;
        pSprite->m_derivedStats.field_128 = 0;
        SHORT str = pSprite->m_bonusStats.m_nSTR;
        Apply(pSprite, CGAMEEFFECT_BERSERK, 2, 0, 0);
        CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_BLOOD_RAGE));
        CHECK(pSprite->m_derivedStats.m_naturalImmunities.count(400) == 1);
        CHECK(pSprite->m_bonusStats.m_nSTR == str + 6);
        CHECK(pSprite->m_derivedStats.field_128 == 6);
    }

    delete pSprite;
}

TEST_CASE("regeneration (98) queues a persistent heal", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    INT count = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount();

    CGameEffect* pEffect = Make(CGAMEEFFECT_REGENERATION, 2, 3, 0, 60);
    pEffect->m_sourceRes = CResRef("SPWI999");
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);

    REQUIRE(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount() == count + 1);
    CPersistantEffect84C494* pRegen = static_cast<CPersistantEffect84C494*>(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetTail());
    CHECK(pRegen->field_34 == 3);
    CHECK(pRegen->field_24 == 15);
    CHECK(pRegen->field_1C == 60);
    CHECK(strncmp(reinterpret_cast<const char*>(pRegen->field_29), "SPWI999", 7) == 0);

    delete pSprite;
}

TEST_CASE("solid colour effects (51, 52) add one colour effect per range", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    // Range 255 walks the original's 17-entry table (0x8A8EE0, 0x8A8F28)
    // up to its -1 terminator: 17 entries, then stop.
    for (WORD effectID : { static_cast<WORD>(CGAMEEFFECT_COLORTINTSOLID), static_cast<WORD>(CGAMEEFFECT_COLORLIGHTSOLID) }) {
        INT before = pSprite->m_derivedStats.m_appliedColorEffects.GetCount();
        Apply(pSprite, effectID, 255, 0x00FF0000, 0);
        CHECK(pSprite->m_derivedStats.m_appliedColorEffects.GetCount() == before + 17);

        before = pSprite->m_derivedStats.m_appliedColorEffects.GetCount();
        Apply(pSprite, effectID, 2, 0x00FF0000, 0);
        CHECK(pSprite->m_derivedStats.m_appliedColorEffects.GetCount() == before + 1);
    }

    delete pSprite;
}

TEST_CASE("spell immunity (206, 290) stays on while it has a resource", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    CGameEffect* pEffect = Make(CGAMEEFFECT_IMMUNITYSPELL, 0, 0, 0);
    pEffect->m_res = CResRef("SPWI112");
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == FALSE);
    delete pEffect;

    CGameEffect* pEmpty = Make(CGAMEEFFECT_IMMUNITYSPELL, 0, 0, 0);
    REQUIRE(pEmpty->ApplyEffect(pSprite) == TRUE);
    CHECK(pEmpty->m_done == TRUE);
    delete pEmpty;

    // Mode 1 ends the effect on a sprite that is not undead (Evaluate
    // 0x4BF140 holds).
    CGameEffect* pLiving = Make(CGAMEEFFECT_IMMUNITYSPELL, 1, 0, 0);
    pLiving->m_res = CResRef("SPWI112");
    REQUIRE(pLiving->ApplyEffect(pSprite) == TRUE);
    CHECK(pLiving->m_done == TRUE);
    delete pLiving;

    delete pSprite;
}

TEST_CASE("damage (12) takes resistances and leaves the living awake", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_baseStats.m_hitPoints = 30;
    pSprite->m_baseStats.m_maxHitPointsBase = 30;
    pSprite->m_tempStats.m_nMaxHitPoints = 30;
    pSprite->m_baseStats.m_generalState = 0;

    // Magic and poison damage start no sprite effect (blood and the
    // elemental effects need an effect palette this fixture does not set
    // up), so they carry the checks.
    SECTION("magic, mode 0, less the magic damage resistance")
    {
        pSprite->m_tempStats.m_nMagicDamageResistance = 2;
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400000, 7, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 25);
        CHECK(pSprite->field_5582 == 5);
    }

    SECTION("poison less the poison resistance; a resistance above the amount does nothing")
    {
        pSprite->m_tempStats.m_nResistPoison = 3;
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x200000, 9, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 24);
        pSprite->m_tempStats.m_nResistPoison = 20;
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x200000, 9, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 24);
    }

    SECTION("mode 3 halves, mode 2 is a percentage of the current hit points")
    {
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400003, 9, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 26);
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400002, 50, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 13);
    }

    SECTION("a cut scene stops it")
    {
        g_pBaldurChitin->GetObjectGame()->m_gameSave.m_cutScene = TRUE;
        CHECK(Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400000, 7, 0) == TRUE);
        g_pBaldurChitin->GetObjectGame()->m_gameSave.m_cutScene = FALSE;
        CHECK(pSprite->m_baseStats.m_hitPoints == 30);
    }

    delete pSprite;
}

TEST_CASE("RemoveAllOfType removes the matching effects but the one to leave", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    CGameEffectList list;

    CGameEffect* pFirst = Make(CGAMEEFFECT_LUCK, 0, 1, 0);
    CGameEffect* pKeep = Make(CGAMEEFFECT_LUCK, 0, 2, 0);
    CGameEffect* pOther = Make(CGAMEEFFECT_CHR, 0, 1, 0);
    CGameEffect* pLast = Make(CGAMEEFFECT_LUCK, 0, 2, 0);
    list.AddTail(pFirst);
    POSITION posKeep = list.AddTail(pKeep);
    list.AddTail(pOther);
    list.AddTail(pLast);

    SECTION("any amount")
    {
        list.RemoveAllOfType(pSprite, CGAMEEFFECT_LUCK, posKeep, -1);
        REQUIRE(list.GetCount() == 2);
        CHECK(list.GetHead() == pKeep);
        CHECK(list.GetTail() == pOther);

        // The node before the one to leave was visited: m_posNext is the
        // node after it (0x4C097C).
        POSITION posNext = posKeep;
        list.GetNext(posNext);
        CHECK(list.m_posNext == posNext);
    }

    SECTION("a given amount")
    {
        list.RemoveAllOfType(pSprite, CGAMEEFFECT_LUCK, posKeep, 2);
        REQUIRE(list.GetCount() == 3);
        CHECK(list.GetHead() == pFirst);
        CHECK(list.GetTail() == pOther);

        POSITION posNext = posKeep;
        list.GetNext(posNext);
        CHECK(list.m_posNext == posNext);
    }

    SECTION("no node to leave")
    {
        list.RemoveAllOfType(pSprite, CGAMEEFFECT_LUCK, NULL, -1);
        REQUIRE(list.GetCount() == 1);
        CHECK(list.GetHead() == pOther);
    }

    // The list's destructor deletes the surviving effects.

    delete pSprite;
}

TEST_CASE("visible (47) ends invisibility; the folded no-op effects return 1", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    pSprite->m_tempStats.m_generalState = 0;
    pSprite->m_baseStats.m_generalState |= STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY;
    pSprite->m_derivedStats.m_generalState |= STATE_INVISIBLE;
    CHECK(Apply(pSprite, CGAMEEFFECT_VISIBLE, 0, 0, 0) == TRUE);
    CHECK((pSprite->m_baseStats.m_generalState & (STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY)) == 0);
    CHECK((pSprite->m_derivedStats.m_generalState & STATE_INVISIBLE) == 0);

    CHECK(Apply(pSprite, CGAMEEFFECT_IDENTIFY, 0, 0, 0) == TRUE);
    CHECK(Apply(pSprite, CGAMEEFFECT_BOUNCESPELL, 0, 0, 0) == FALSE);
    CHECK(Apply(pSprite, CGAMEEFFECT_LEVELDRAIN, 0, 0, 0) == FALSE);

    delete pSprite;
}

TEST_CASE("poison (25) queues a timed poison damage", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    INT count = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount();

    CHECK(Apply(pSprite, CGAMEEFFECT_POISON, 2, 1, 0) == FALSE);
    CHECK((pSprite->m_derivedStats.m_generalState & STATE_POISONED) != 0);
    REQUIRE(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount() == count + 1);
    CPersistantEffect84C420* pPoison = static_cast<CPersistantEffect84C420*>(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetTail());
    CHECK(pPoison->field_34 == 0x200000);
    CHECK(pPoison->field_38 == 1);
    CHECK(pPoison->field_24 == 15);

    // Mode 6 also holds the sprite, then poisons as mode 4 (every 105).
    Apply(pSprite, CGAMEEFFECT_POISON, 6, 2, 0);
    CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_HELD));
    CHECK((pSprite->m_derivedStats.m_generalState & STATE_HELPLESS) != 0);
    pPoison = static_cast<CPersistantEffect84C420*>(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetTail());
    CHECK(pPoison->field_24 == 105);

    delete pSprite;
}

TEST_CASE("item effects create and destroy items", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    // A creature (no portrait) gets the item in the first free slot from 15.
    for (int n = CGameSpriteEquipment::SLOT_MISC; n < 43; n++) {
        pSprite->m_equipment.m_items[n] = NULL;
    }
    CGameEffect* pCreate = Make(CGAMEEFFECT_CREATEITEM, 0, 1, 1);
    pCreate->m_res = CResRef("00POTN03");
    REQUIRE(pCreate->ApplyEffect(pSprite) == TRUE);
    CHECK(pCreate->m_done == TRUE);
    REQUIRE(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC] != NULL);
    CHECK(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC]->cResRef == CResRef("00POTN03"));

    CGameEffect* pDestroy = Make(CGAMEEFFECT_DESTROYITEM, 0, 0, 1);
    pDestroy->m_res = CResRef("00POTN03");
    REQUIRE(pDestroy->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC] == NULL);
    CHECK(pSprite->field_562C == 1);

    delete pSprite;
}

TEST_CASE("learn spell (147) finds a wizard spell and ends", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_tempStats.m_nINT = 40;
    pSprite->m_tempStats.m_nLuck = 0;
    pSprite->m_baseStats.m_specialization = 0x4000;

    CGameEffect* pEffect = Make(CGAMEEFFECT_LEARNSPELL, 0, 0, 1);
    pEffect->m_res = CResRef("SPWI112");
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);

    delete pSprite;
}

TEST_CASE("stoneskins (218), slay (55) and stone to flesh (43)", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SECTION("iron skins add 10 to the physical resistances and keep the larger count")
    {
        SHORT nSlashing = pSprite->m_derivedStats.m_nResistSlashing;
        pSprite->m_derivedStats.m_nStoneSkins = 2;
        CHECK(Apply(pSprite, CGAMEEFFECT_STONESKINS, 1, 5, 0) == FALSE);
        CHECK(pSprite->m_derivedStats.m_nResistSlashing == nSlashing + 10);
        CHECK(pSprite->m_derivedStats.m_nStoneSkins == 5);
        CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_IRON_SKINS));
        CHECK(pSprite->m_derivedStats.m_naturalImmunities.count(CGAMEEFFECT_STONESKINS) == 1);
    }

    SECTION("slay leaves a sprite of another race alone")
    {
        SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
        BYTE nRace = pSprite->GetAIType().m_nRace;
        CHECK(Apply(pSprite, CGAMEEFFECT_SLAY, 4, nRace + 1, 0) == TRUE);
        CHECK(pSprite->m_baseStats.m_hitPoints == nHitPoints);
        CHECK(pSprite->m_timedEffectList.GetCount() == 0);
    }

    SECTION("stone to flesh does nothing to a living sprite")
    {
        pSprite->m_derivedStats.m_generalState &= ~STATE_STONE_DEATH;
        SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
        CHECK(Apply(pSprite, CGAMEEFFECT_STONETOFLESH, 0, 0, 0) == TRUE);
        CHECK(pSprite->m_baseStats.m_hitPoints == nHitPoints);
    }

    delete pSprite;
}

TEST_CASE("apply effect (177) and the equip triggers (182, 183)", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SECTION("177 with a type that does not match does nothing")
    {
        BYTE nRace = pSprite->GetAIType().m_nRace;
        CGameEffect* pEffect = Make(CGAMEEFFECT_APPLYEFFECT, 4, nRace + 1, 0);
        pEffect->m_res = CResRef("NOFILE");
        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == FALSE);
        delete pEffect;
    }

    SECTION("177 with a matching type and no such EFF file only looks it up")
    {
        BYTE nRace = pSprite->GetAIType().m_nRace;
        CGameEffect* pEffect = Make(CGAMEEFFECT_APPLYEFFECT, 4, nRace, 0);
        pEffect->m_res = CResRef("NOFILE");
        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == FALSE);
        delete pEffect;
    }

    SECTION("182 records the item and its EFF (none in the vanilla data)")
    {
        INT count = pSprite->m_derivedStats.field_290.GetCount();
        CGameEffect* pEffect = Make(CGAMEEFFECT_APPLYEFFECTEQUIPITEM, 0, 0, 0);
        pEffect->m_res = CResRef("00SWDL01");
        pEffect->m_res2 = CResRef("NOFILE");
        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->m_derivedStats.field_290.GetCount() == count + 1);
        CImmunitiesItemEquip* pEntry = pSprite->m_derivedStats.field_290.GetTail();
        CHECK(pEntry->m_res == CResRef("00SWDL01"));
        CHECK(pEntry->m_error == static_cast<STRREF>(-1));
        CHECK(pEntry->m_pEffect == NULL);
        delete pEffect;
    }

    SECTION("183 records the item type and the error string")
    {
        INT count = pSprite->m_derivedStats.field_2AC.GetCount();
        CGameEffect* pEffect = Make(CGAMEEFFECT_APPLYEFFECTEQUIPITEMTYPE, 0x14, 1234, 0);
        pEffect->m_res = CResRef("NOFILE");
        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->m_derivedStats.field_2AC.GetCount() == count + 1);
        CImmunitiesItemTypeEquip* pEntry = pSprite->m_derivedStats.field_2AC.GetTail();
        CHECK(pEntry->m_type == 0x14);
        CHECK(pEntry->m_error == 1234);
        delete pEffect;
    }

    delete pSprite;
}

TEST_CASE("fire resistance (30) adds a negative temporary bonus as a signed amount", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SHORT nBonus = pSprite->m_bonusStats.m_nResistFire;
    Apply(pSprite, CGAMEEFFECT_RESISTFIRE, 0, -10, 0);
    CHECK(pSprite->m_bonusStats.m_nResistFire == nBonus - 10);

    pSprite->m_baseStats.m_resistFireBase = 40;
    Apply(pSprite, CGAMEEFFECT_RESISTFIRE, 2, 50, 1);
    CHECK(pSprite->m_baseStats.m_resistFireBase == 20);

    delete pSprite;
}

TEST_CASE("berserk (3) and barbarian rage (440) removal add the fatigue spell's effects", "[effects][data]")
{
    REQUIRE_ENGINE();

    SECTION("berserk: only blood rage (2) fetches EffBR")
    {
        CGameSprite* pSprite = MakeSprite();
        CGameEffect* pEffect = Make(CGAMEEFFECT_BERSERK, 0, 0, 0);
        pEffect->m_casterLevel = 1;
        pEffect->OnRemove(pSprite);
        CHECK(pSprite->m_timedEffectList.GetCount() == 0);
        delete pEffect;

        pEffect = Make(CGAMEEFFECT_BERSERK, 2, 0, 0);
        pEffect->m_casterLevel = 1;
        pEffect->m_sourceRes = CResRef("SPCL321");
        pEffect->OnRemove(pSprite);
        CHECK(pSprite->m_timedEffectList.GetCount() > 0);
        // EffBR's fatigue of 16.
        CHECK(pSprite->m_baseStats.m_fatigue == 16);
        POSITION pos = pSprite->m_timedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pAdded = pSprite->m_timedEffectList.GetNext(pos);
            CHECK(pAdded->m_sourceRes == CResRef("SPCL321"));
            CHECK(pAdded->m_casterLevel == 1);
        }
        delete pEffect;
        delete pSprite;
    }

    SECTION("barbarian rage: EffBBR unless immune to fatigue")
    {
        CGameSprite* pSprite = MakeSprite();
        REQUIRE(pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_BARBARIAN) < 20);
        CGameEffect* pEffect = Make(ICEWIND_CGAMEEFFECT_BARBARIANRAGE, 0, 0, 0);
        pEffect->m_casterLevel = 1;
        pSprite->m_derivedStats.m_naturalImmunities.insert(CGAMEEFFECT_FATIGUE);
        pEffect->OnRemove(pSprite);
        CHECK(pSprite->m_timedEffectList.GetCount() == 0);
        pSprite->m_derivedStats.m_naturalImmunities.erase(CGAMEEFFECT_FATIGUE);
        pEffect->OnRemove(pSprite);
        CHECK(pSprite->m_timedEffectList.GetCount() > 0);
        delete pEffect;
        delete pSprite;
    }
}

TEST_CASE("damage (12) goes through the damage reductions", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_baseStats.m_hitPoints = 30;
    pSprite->m_baseStats.m_maxHitPointsBase = 30;
    pSprite->m_tempStats.m_nMaxHitPoints = 30;
    pSprite->m_baseStats.m_generalState = 0;
    CDamageReduction reduction(3, 5);
    reduction.AddDamageType(0x400000);
    pSprite->m_derivedStats.m_damageReductions.push_back(reduction);

    // A +0 source (m_special 0): 7 - 3.
    CGameEffect* pEffect = Make(CGAMEEFFECT_DAMAGE, 0x400000, 7, 0);
    pEffect->m_special = 0;
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 26);
    delete pEffect;

    // m_special -1: no reduction.
    pEffect = Make(CGAMEEFFECT_DAMAGE, 0x400000, 7, 0);
    pEffect->m_special = static_cast<DWORD>(-1);
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 19);
    delete pEffect;

    // A +5 source passes the 3/+5.
    pEffect = Make(CGAMEEFFECT_DAMAGE, 0x400000, 7, 0);
    pEffect->m_special = 5;
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 12);
    delete pEffect;

    delete pSprite;
}

TEST_CASE("spell immunity (206) and stoneskin (218) fill the derived stats containers", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    CGameEffect* pEffect = Make(CGAMEEFFECT_IMMUNITYSPELL, 0, 0, 0);
    pEffect->m_res = CResRef("SPWI112");
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->m_derivedStats.m_resourceImmunities.count("SPWI112") == 1);
    CHECK(pSprite->m_derivedStats.m_naturalImmunities.count(0) == 0);
    delete pEffect;

    pSprite->m_derivedStats.m_damageReductions.clear();
    // A secondary-type stoneskin sets its pool to 10 per caster level.
    pEffect = Make(CGAMEEFFECT_STONESKINS, 0, 0, 0);
    pEffect->m_secondaryType = TRUE;
    pEffect->m_casterLevel = 4;
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(pSprite->m_derivedStats.m_damageReductions.size() == 1);
    CDamageReduction& entry = pSprite->m_derivedStats.m_damageReductions[0];
    CHECK(entry.m_nAmount == 10);
    CHECK(entry.m_nEnchantment == 5);
    CHECK(entry.IsGeneric() == TRUE);
    CHECK(entry.m_pPool == &pEffect->m_effectAmount2);
    CHECK(entry.m_sourceCreaturePtr == pSprite);
    CHECK(entry.m_sourceEffectPtr == pEffect);
    CHECK(pEffect->m_effectAmount2 == 40);
    CHECK(entry.GetEffectiveAmount() == 10);
    // The pool absorbs 10 per hit until it runs out.
    CHECK(pSprite->m_derivedStats.m_damageReductions.Apply(15, 0, 0) == 5);
    CHECK(pEffect->m_effectAmount2 == 30);
    pSprite->m_derivedStats.m_damageReductions.clear();
    delete pEffect;

    delete pSprite;
}

TEST_CASE("check add rejects a resource immunity and adds the source's save adjustment", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    BYTE roll1 = 10;
    BYTE roll2 = 10;
    BYTE roll3 = 10;
    BYTE roll4 = 10;
    BYTE probability = 50;

    CGameEffect* pEffect = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pEffect->m_sourceRes = CResRef("SPWI999");
    pSprite->m_derivedStats.m_resourceImmunities.insert("SPWI999");
    CHECK(pEffect->sub_4A3310(pSprite, &roll1, &roll2, &roll3, &roll4, &probability) == FALSE);
    pSprite->m_derivedStats.m_resourceImmunities.clear();
    delete pEffect;

    // A wizard source (class 11 in bits 8..15) with INT 18: +4.
    pSprite->m_derivedStats.m_nINT = 18;
    pEffect = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pEffect->m_sourceID = pSprite->GetId();
    pEffect->m_savingThrow = 0x4;
    pEffect->m_saveMod = 1;
    pEffect->m_casterLevel = (CAIOBJECTTYPE_C_WIZARD << 8) | 5;
    pEffect->sub_4A3310(pSprite, &roll1, &roll2, &roll3, &roll4, &probability);
    CHECK(pEffect->m_saveMod == 5);
    delete pEffect;

    // No class: nothing added.
    pEffect = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pEffect->m_sourceID = pSprite->GetId();
    pEffect->m_savingThrow = 0x4;
    pEffect->m_saveMod = 1;
    pEffect->m_casterLevel = 5;
    pEffect->sub_4A3310(pSprite, &roll1, &roll2, &roll3, &roll4, &probability);
    CHECK(pEffect->m_saveMod == 1);
    delete pEffect;

    delete pSprite;
}

TEST_CASE("check add gives SPIN108 the source's animal empathy save modifier", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    BYTE roll1 = 10;
    BYTE roll2 = 10;
    BYTE roll3 = 10;
    BYTE roll4 = 10;
    BYTE probability = 50;

    // 0x547620: animal empathy (skill 1) less the spell level less 5, in
    // place of the modifier the effect had.
    pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ANIMAL_EMPATHY] = 12;
    CGameEffect* pEffect = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pEffect->m_sourceID = pSprite->GetId();
    pEffect->m_sourceRes = CResRef("SPIN108");
    pEffect->m_savingThrow = 0x4;
    pEffect->m_saveMod = 3;
    pEffect->m_spellLevel = 2;
    pEffect->sub_4A3310(pSprite, &roll1, &roll2, &roll3, &roll4, &probability);
    CHECK(pEffect->m_saveMod == 5);
    delete pEffect;

    // Another source resref keeps its modifier.
    pEffect = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pEffect->m_sourceID = pSprite->GetId();
    pEffect->m_sourceRes = CResRef("SPIN109");
    pEffect->m_savingThrow = 0x4;
    pEffect->m_saveMod = 3;
    pEffect->m_spellLevel = 2;
    pEffect->sub_4A3310(pSprite, &roll1, &roll2, &roll3, &roll4, &probability);
    CHECK(pEffect->m_saveMod == 3);
    delete pEffect;

    // Without a saving throw nothing is adjusted.
    pEffect = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pEffect->m_sourceID = pSprite->GetId();
    pEffect->m_sourceRes = CResRef("SPIN108");
    pEffect->m_saveMod = 3;
    pEffect->sub_4A3310(pSprite, &roll1, &roll2, &roll3, &roll4, &probability);
    CHECK(pEffect->m_saveMod == 3);
    delete pEffect;

    delete pSprite;
}

TEST_CASE("stoneskin's exhausted pool removes only its effect and later hits take full damage", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_derivedStats.m_damageReductions.clear();

    // An unrelated effect and the stoneskin, both on the timed list.
    CGameEffect* pOther = Make(CGAMEEFFECT_STR, 0, 1, 0);
    pSprite->m_timedEffectList.AddTail(pOther);
    CGameEffect* pStoneSkin = Make(CGAMEEFFECT_STONESKINS, 0, 0, 0);
    pStoneSkin->m_secondaryType = TRUE;
    pStoneSkin->m_casterLevel = 4;
    pSprite->m_timedEffectList.AddTail(pStoneSkin);
    REQUIRE(pStoneSkin->ApplyEffect(pSprite) == TRUE);
    REQUIRE(pSprite->m_derivedStats.m_damageReductions.size() == 1);
    REQUIRE(pStoneSkin->m_effectAmount2 == 40);

    // One hit larger than the pool: the whole pool comes off (60 - 40), the
    // stoneskin is removed and the entry is neutralized (ruling M3-10).
    CHECK(pSprite->m_derivedStats.m_damageReductions.Apply(60, 0, 0) == 20);
    REQUIRE(pSprite->m_timedEffectList.GetCount() == 1);
    CHECK(pSprite->m_timedEffectList.GetHead() == pOther);
    CHECK(pSprite->m_equipedEffectList.GetCount() == 0);
    const CDamageReduction& entry = pSprite->m_derivedStats.m_damageReductions[0];
    CHECK(entry.m_nAmount == 0);
    CHECK(entry.m_pPool == nullptr);
    CHECK(entry.m_sourceEffectPtr == nullptr);

    // A second hit before the rebuild takes full damage and deletes nothing.
    CHECK(pSprite->m_derivedStats.m_damageReductions.Apply(15, 0, 0) == 15);
    CHECK(pSprite->m_timedEffectList.GetCount() == 1);
    CHECK(pSprite->m_timedEffectList.GetHead() == pOther);
    CHECK(pOther->m_effectID == CGAMEEFFECT_STR);

    delete pSprite;
}

namespace {

// The bytes of a creature file, as LoadCharBase reads CHARBASE.
std::vector<BYTE> LoadCreature(const char* resRef)
{
    CCreatureFile file;
    file.SetResRef(CResRef(resRef), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

// The persistent effect's source resref: the original's strcpy target at
// +0x29, ten bytes there (the copy at 0x4C29EE moves 0x29..0x32 as one
// member).
std::string SourceOf(CPersistantEffect84C4A4* pEffect)
{
    const char* source = reinterpret_cast<const char*>(pEffect->field_29);
    return std::string(source, strnlen(source, sizeof(pEffect->field_29)));
}

}

// AR3000's trolls (30TROLL) wear 00TROLL1, whose equip effect is a
// regeneration (98, mode 3, amount 2): its source resref has eight
// characters, so the strcpy into the persistent effect writes nine bytes.
// Loading AR3000 aborted on it (_FORTIFY_SOURCE) while field_29 was
// declared as eight bytes. The poison (25) and disease (78) sites copy the
// same way.
TEST_CASE("persistent effects keep an eight-character source resref", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    CTypedPtrList<CPtrList, CPersistantEffect*>& list = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList;

    struct Site {
        WORD effectID;
        DWORD dwFlags;
        LONG amount;
    };
    const Site sites[] = {
        { CGAMEEFFECT_REGENERATION, 3, 2 }, // 0x4B6620, as 00TROLL1
        { CGAMEEFFECT_POISON, 2, 1 }, // 0x4AF610
        { CGAMEEFFECT_DISEASE, 2, 1 }, // 0x4B5790
        { CGAMEEFFECT_DISEASE, 11, 1 }, // 0x4B5E50 (mold touch)
    };
    for (const Site& site : sites) {
        INFO("effect " << site.effectID << " mode " << site.dwFlags);
        INT count = list.GetCount();
        CGameEffect* pEffect = Make(site.effectID, site.dwFlags, site.amount, 0, 60);
        pEffect->m_sourceRes = CResRef("00TROLL1");
        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        delete pEffect;

        REQUIRE(list.GetCount() == count + 1);
        CPersistantEffect84C4A4* pPersistant = static_cast<CPersistantEffect84C4A4*>(list.GetTail());
        CHECK(SourceOf(pPersistant) == "00TROLL1");
        CHECK(pPersistant->field_29[8] == '\0');

        // The copy keeps the ten bytes.
        CPersistantEffect84C4A4* pCopy = static_cast<CPersistantEffect84C4A4*>(pPersistant->Copy());
        CHECK(SourceOf(pCopy) == "00TROLL1");
        delete pCopy;
    }

    delete pSprite;
}

// The same through the creature: 30TROLL equips 00TROLL1 and its effect list
// applies the regeneration.
TEST_CASE("AR3000's troll applies 00TROLL1's regeneration", "[effects][data]")
{
    REQUIRE_ENGINE();
    std::vector<BYTE> bytes = LoadCreature("30TROLL");
    REQUIRE(!bytes.empty());
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
    pSprite->ProcessEffectList();

    bool found = false;
    CTypedPtrList<CPtrList, CPersistantEffect*>& list = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList;
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        CPersistantEffect84C4A4* pEffect = static_cast<CPersistantEffect84C4A4*>(list.GetNext(pos));
        if (SourceOf(pEffect) == "00TROLL1") {
            found = true;
            CHECK(static_cast<CPersistantEffect84C494*>(pEffect)->field_34 == 1);
            CHECK(pEffect->field_24 == 2 * 15);
        }
    }
    CHECK(found);

    delete pSprite;
}

// AR3000 itself: a new game, then the area loaded the way a travel loads it
// (CInfGame::LoadArea 0x5A24D0). Its ice trolls (30ICETRL) come in wearing
// 00CIWWLF and 00CWABI2, both with a regeneration (98) equip effect and an
// eight-character resref.
TEST_CASE("AR3000 loads with its ice trolls regenerating", "[effects][data][ar3000]")
{
    REQUIRE_ENGINE();
    // The main loop's drawn frames: the loads wait for m_bDisplayStale to
    // clear (tests/test_gam.cpp's FramePump).
    std::atomic<bool> running(true);
    std::thread pump([&running] {
        while (running) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct Join {
        std::atomic<bool>& running;
        std::thread& pump;
        ~Join()
        {
            running = false;
            pump.join();
        }
    } join { running, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameArea* pArea = pGame->LoadArea(CString("AR3000"), 0, FALSE, FALSE);
    REQUIRE(pArea != NULL);
    // The added objects onto the sorted lists (as a frame's AIUpdate does).
    pArea->SortLists();

    int iceTrolls = 0;
    int regenerating = 0;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* objects : lists) {
        POSITION pos = objects->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(objects->GetNext(pos)));
            CGameObject* pObject;
            if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
                continue;
            }
            pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
                continue;
            }
            CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
            if (pSprite->m_resRef != CResRef("30ICETRL")) {
                continue;
            }
            iceTrolls++;
            pSprite->ProcessEffectList();
            std::vector<std::string> sources;
            CTypedPtrList<CPtrList, CPersistantEffect*>& list = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList;
            POSITION effectPos = list.GetHeadPosition();
            while (effectPos != NULL) {
                sources.push_back(SourceOf(static_cast<CPersistantEffect84C4A4*>(list.GetNext(effectPos))));
            }
            if (std::find(sources.begin(), sources.end(), "00CIWWLF") != sources.end()
                && std::find(sources.begin(), sources.end(), "00CWABI2") != sources.end()) {
                regenerating++;
            }
        }
    }
    CHECK(iceTrolls > 0);
    CHECK(regenerating == iceTrolls);
}

// The spawn lists' ParseData (CSpawnFile 0x768840, CSpawnList 0x768D50)
// count the last item, which has no separator after it, as the originals do
// (inc ebp at 0x768926 and 0x768E48): CSpawnList's constructor asserts the
// count equals the array's size (AR3000.INI's critters= list).
TEST_CASE("spawn lists count the last item of a comma list", "[spawn]")
{
    CMemINI ini;
    CSpawnFile file(&ini);
    CSpawnList list(&ini, CString("none"));

    CStringArray fileSections;
    CHECK(file.ParseData(CString("DRUM_SUMMON_A,DRUM_SUMMON_B,DRUM_SUMMON_C,DRUM_SUMMON_D"), fileSections) == 4);
    REQUIRE(fileSections.GetSize() == 4);
    CHECK(fileSections[3] == "DRUM_SUMMON_D");

    CStringArray listSections;
    CHECK(list.ParseData(CString(" A, B.C "), listSections) == 3);
    REQUIRE(listSections.GetSize() == 3);
    CHECK(listSections[0] == "A");
    CHECK(listSections[1] == "B");
    CHECK(listSections[2] == "C");

    // A single item, and a trailing separator (nothing after it to add).
    CStringArray single;
    CHECK(list.ParseData(CString("1_SECOND"), single) == 1);
    CHECK(single.GetSize() == 1);
    CStringArray trailing;
    CHECK(list.ParseData(CString("A,B,"), trailing) == 2);
    CHECK(trailing.GetSize() == 2);
}
