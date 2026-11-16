#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CDerivedStats.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CRuleTables.h"
#include "CScreenCreateChar.h"
#include "CSpell.h"
#include "IcewindMisc.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <climits>
#include <cstdlib>
#include <list>
#include <memory>
#include <string>
#include <vector>

// CDerivedStats' resource immunities (+0x470), when-struck list (+0x490)
// and damage reductions (+0x49C, 0x447B60-0x448250), and the spell effect
// decoder (CSpell 0x54A510, IcewindMisc 0x586220, CRuleTables 0x547040).

// The decoded effects are not deleted: CGameEffect's destructor is not
// virtual (as in the original) and DecodeEffect returns derived classes.
// Each test case runs in its own process.

namespace {

const INT kPhysicalTypes[] = { 0, 0x100000, 0x1000000, 0x2000000, 0x4000000, 0x8000000 };

CDamageReduction MakeGeneric(LONG nAmount, LONG nEnchantment)
{
    CDamageReduction reduction(nAmount, nEnchantment);
    for (INT nType : kPhysicalTypes) {
        reduction.AddDamageType(nType);
    }
    return reduction;
}

// The engine in this process, as iwd2r_main builds it, without a game
// loaded: the game object, its variables, object array and message handler
// exist. Built once and leaked: the engine's globals live until the process
// ends. The engine writes into its data folder: it starts on a private view.
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
    if (install != nullptr && *install != '\0') {
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
    } else if (!platform::ResolveDataDir()) {
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
    // The sprites' palettes read the video mode (CVidPalette::SetType), as
    // tests/test_cre.cpp gives it.
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
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

// A live sprite from CHARBASE.CRE, made the way StartCreateChar makes it
// (0x610540); its constructor adds it to the object array.
CGameSprite* MakeSprite()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    BYTE* data = file.GetData();
    REQUIRE(data != NULL);
    std::vector<BYTE> bytes(data, data + file.GetDataSize());
    file.Release();
    return new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
}

} // namespace

TEST_CASE("damage reduction entry", "[derived]")
{
    CDamageReduction reduction(10, 5);
    CHECK(reduction.m_nAmount == 10);
    CHECK(reduction.m_nEnchantment == 5);
    CHECK(reduction.m_damageTypes.empty());
    CHECK(reduction.m_pPool == nullptr);
    CHECK(reduction.m_sourceCreaturePtr == nullptr);
    CHECK(reduction.m_sourceEffectPtr == nullptr);

    reduction.AddDamageType(0x100000);
    reduction.AddDamageType(0x100000);
    reduction.AddDamageType(0);
    CHECK(reduction.m_damageTypes.size() == 2);

    LONG pool = 30;
    CGameSprite* pSprite = reinterpret_cast<CGameSprite*>(0x1000);
    CGameEffect* pEffect = reinterpret_cast<CGameEffect*>(0x2000);
    reduction.SetSource(&pool, pSprite, pEffect);
    CHECK(reduction.m_pPool == &pool);
    CHECK(reduction.m_sourceCreaturePtr == pSprite);
    CHECK(reduction.m_sourceEffectPtr == pEffect);
}

TEST_CASE("damage reduction apply", "[derived]")
{
    SECTION("no pool: the amount comes off, never below 0")
    {
        CDamageReduction reduction(5, 1);
        CHECK(reduction.Apply(12) == 7);
        CHECK(reduction.Apply(5) == 0);
        CHECK(reduction.Apply(3) == 0);
    }

    SECTION("a pool that holds the damage: min(damage, amount) off both")
    {
        CDamageReduction reduction(10, 5);
        LONG pool = 40;
        reduction.SetSource(&pool, nullptr, nullptr);
        CHECK(reduction.Apply(25) == 15);
        CHECK(pool == 30);
        CHECK(reduction.Apply(4) == 0);
        CHECK(pool == 26);
        CHECK(reduction.Apply(26) == 16);
        CHECK(pool == 16);
    }

    SECTION("the list picks the best entry that applies")
    {
        CDamageReductionList list;
        // 5/+1 against everything, 10/+3 against slashing only, 20/+1
        // against fire only.
        CDamageReduction any(5, 1);
        list.push_back(any);
        CDamageReduction slashing(10, 3);
        slashing.AddDamageType(0x1000000);
        list.push_back(slashing);
        CDamageReduction fire(20, 1);
        fire.AddDamageType(0x80000);
        list.push_back(fire);

        // A +0 slashing weapon: 10/+3 is the best.
        CHECK(list.Apply(15, 0, 0x1000000) == 5);
        // A +2 slashing weapon: still under +3.
        CHECK(list.Apply(15, 2, 0x1000000) == 5);
        // A +3 slashing weapon passes 10/+3; 5/+1 is passed too.
        CHECK(list.Apply(15, 3, 0x1000000) == 15);
        // A +0 crushing weapon: only 5/+1 (empty set) applies.
        CHECK(list.Apply(15, 0, 0) == 10);
        // Fire: 20/+1.
        CHECK(list.Apply(15, 0, 0x80000) == 0);
        // Nothing applies: the damage is unchanged, even negative.
        CHECK(list.Apply(-3, 5, 0) == -3);
    }

    SECTION("a pool smaller than the amount makes the entry weaker")
    {
        CDamageReductionList list;
        CDamageReduction pooled(10, 5);
        LONG pool = 3;
        pooled.SetSource(&pool, nullptr, nullptr);
        list.push_back(pooled);
        list.push_back(CDamageReduction(4, 5));
        // Effective amounts: min(3, 10) = 3 against 4: the plain 4/+5 wins.
        CHECK(list.Apply(10, 0, 0) == 6);
        CHECK(pool == 3);
        // A tie keeps the first (strictly larger replaces).
        pool = 4;
        CHECK(list.Apply(3, 0, 0) == 0);
        CHECK(pool == 1);
    }
}

TEST_CASE("damage reduction lookup", "[derived]")
{
    CDamageReduction plain(12, 1);
    CHECK(plain.GetEffectiveAmount() == 12);
    LONG pool = 7;
    plain.SetSource(&pool, nullptr, nullptr);
    CHECK(plain.GetEffectiveAmount() == 7);
    pool = 12;
    CHECK(plain.GetEffectiveAmount() == 12);
    pool = 40;
    CHECK(plain.GetEffectiveAmount() == 12);

    CHECK(CDamageReduction(5, 1).IsGeneric() == TRUE);
    CHECK(MakeGeneric(5, 1).IsGeneric() == TRUE);
    CDamageReduction partial(5, 1);
    for (INT nType : kPhysicalTypes) {
        if (nType != 0x4000000) {
            partial.AddDamageType(nType);
        }
    }
    CHECK(partial.IsGeneric() == FALSE);
    partial.AddDamageType(0x4000000);
    partial.AddDamageType(0x80000);
    CHECK(partial.IsGeneric() == TRUE);

    CDamageReductionList list;
    CHECK(list.GetBestGeneric(5) == nullptr);
    list.push_back(MakeGeneric(5, 1));
    CDamageReduction slashing(10, 3);
    slashing.AddDamageType(0x1000000);
    list.push_back(slashing);
    list.push_back(MakeGeneric(15, 1));
    list.push_back(CDamageReduction(30, 5));

    // The empty-set 30/+5 is generic too and the largest.
    REQUIRE(list.GetBestGeneric(5) != nullptr);
    CHECK(list.GetBestGeneric(5)->m_nAmount == 30);

    // GetBest skips the generic ones.
    REQUIRE(list.GetBest(0, 0x1000000) != nullptr);
    CHECK(list.GetBest(0, 0x1000000)->m_nAmount == 10);
    CHECK(list.GetBest(3, 0x1000000) == nullptr);
    CHECK(list.GetBest(0, 0) == nullptr);
}

TEST_CASE("derived stats containers copy and clear", "[derived]")
{
    CDerivedStats stats;
    CHECK(stats.m_resourceImmunities.empty());
    CHECK(stats.m_whenStruckList.empty());
    CHECK(stats.m_damageReductions.empty());

    stats.m_resourceImmunities.insert("SPWI112");
    CWhenStruckEntry entry;
    entry.m_sourceRes = CResRef("SPIN100");
    entry.m_res = CResRef("SPIN101");
    entry.m_casterLevel = 7;
    entry.m_target = CPoint(3, 4);
    stats.m_whenStruckList.push_back(entry);
    LONG pool = 50;
    CDamageReduction reduction = MakeGeneric(10, 5);
    reduction.SetSource(&pool, nullptr, nullptr);
    stats.m_damageReductions.push_back(reduction);

    CDerivedStats copy;
    copy.m_resourceImmunities.insert("OLD");
    copy.m_damageReductions.push_back(CDamageReduction(1, 1));
    copy.m_damageReductions.push_back(CDamageReduction(2, 2));
    copy = stats;
    CHECK(copy.m_resourceImmunities.size() == 1);
    CHECK(copy.m_resourceImmunities.count("SPWI112") == 1);
    REQUIRE(copy.m_whenStruckList.size() == 1);
    CHECK(copy.m_whenStruckList.front().m_sourceRes == CResRef("SPIN100"));
    CHECK(copy.m_whenStruckList.front().m_res == CResRef("SPIN101"));
    CHECK(copy.m_whenStruckList.front().m_casterLevel == 7);
    CHECK(copy.m_whenStruckList.front().m_target == CPoint(3, 4));
    REQUIRE(copy.m_damageReductions.size() == 1);
    CHECK(copy.m_damageReductions[0].m_nAmount == 10);
    CHECK(copy.m_damageReductions[0].m_damageTypes.size() == 6);
    // The pool is shared, as the original copies the pointer.
    CHECK(copy.m_damageReductions[0].m_pPool == &pool);

    copy.BonusInit();
    CHECK(copy.m_resourceImmunities.empty());
    CHECK(copy.m_whenStruckList.empty());
    CHECK(copy.m_damageReductions.empty());
    CHECK(stats.m_damageReductions.size() == 1);
}

TEST_CASE("derived stats copy duplicates the resref lists", "[derived]")
{
    CDerivedStats stats;
    stats.field_4AC.AddTail(new CResRef("SPWI112"));
    stats.field_4AC.AddTail(new CResRef("SPIN100"));
    stats.field_4C8.AddTail(new CResRef("SPWM123"));

    CDerivedStats copy;
    copy = stats;

    REQUIRE(copy.field_4AC.GetCount() == 2);
    REQUIRE(copy.field_4C8.GetCount() == 1);

    // Each entry is a fresh CResRef with the source's value, not the
    // source's pointer.
    POSITION srcPos = stats.field_4AC.GetHeadPosition();
    CResRef* srcFirst = static_cast<CResRef*>(stats.field_4AC.GetNext(srcPos));

    POSITION pos = copy.field_4AC.GetHeadPosition();
    CResRef* first = static_cast<CResRef*>(copy.field_4AC.GetNext(pos));
    CResRef* second = static_cast<CResRef*>(copy.field_4AC.GetNext(pos));
    CHECK(first != srcFirst);
    CHECK(*first == "SPWI112");
    CHECK(*second == "SPIN100");

    POSITION posC8 = copy.field_4C8.GetHeadPosition();
    CHECK(*static_cast<CResRef*>(copy.field_4C8.GetNext(posC8)) == "SPWM123");

    // The lists hold raw pointers; nothing in the class frees them.
    pos = copy.field_4AC.GetHeadPosition();
    while (pos != NULL) {
        delete static_cast<CResRef*>(copy.field_4AC.GetNext(pos));
    }
    posC8 = copy.field_4C8.GetHeadPosition();
    while (posC8 != NULL) {
        delete static_cast<CResRef*>(copy.field_4C8.GetNext(posC8));
    }
    srcPos = stats.field_4AC.GetHeadPosition();
    while (srcPos != NULL) {
        delete static_cast<CResRef*>(stats.field_4AC.GetNext(srcPos));
    }
}

TEST_CASE("derived stats reload adds the creature's damage reduction", "[derived]")
{
    CCreatureFileHeader creature;
    CGameSpriteGroupedSpellList domainSpells;
    CDerivedStats stats;
    stats.m_resourceImmunities.insert("SPWI112");
    stats.m_damageReductions.push_back(CDamageReduction(99, 9));
    CWhenStruckEntry entry;
    stats.m_whenStruckList.push_back(entry);

    SECTION("none")
    {
        creature.field_1B4 = 0;
        stats.Reload(nullptr, &creature, nullptr, &domainSpells);
        CHECK(stats.m_damageReductions.empty());
        CHECK(stats.m_resourceImmunities.empty());
        CHECK(stats.m_whenStruckList.empty());
    }

    SECTION("2: 10/+2 against the six physical types")
    {
        creature.field_1B4 = 2;
        stats.Reload(nullptr, &creature, nullptr, &domainSpells);
        REQUIRE(stats.m_damageReductions.size() == 1);
        CHECK(stats.m_damageReductions[0].m_nAmount == 10);
        CHECK(stats.m_damageReductions[0].m_nEnchantment == 2);
        CHECK(stats.m_damageReductions[0].IsGeneric() == TRUE);
        CHECK(stats.m_damageReductions[0].m_damageTypes.size() == 6);
        CHECK(stats.m_damageReductions[0].m_pPool == nullptr);
    }

    SECTION("6: 15/+1")
    {
        creature.field_1B4 = 6;
        stats.Reload(nullptr, &creature, nullptr, &domainSpells);
        REQUIRE(stats.m_damageReductions.size() == 1);
        CHECK(stats.m_damageReductions[0].m_nAmount == 15);
        CHECK(stats.m_damageReductions[0].m_nEnchantment == 1);
    }
}

TEST_CASE("save adjustment for source sprite", "[spell][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    const CRuleTables& rules = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    CResRef res("SPIN126");

    pSprite->m_derivedStats.m_nINT = 18;
    pSprite->m_derivedStats.m_nWIS = 14;
    pSprite->m_derivedStats.m_nCHR = 9;
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0, CAIOBJECTTYPE_C_WIZARD, 0, 1) == 4);
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0, CAIOBJECTTYPE_C_DRUID, 0, 1) == 2);
    // 9: (9 - 1 - 10) / 2 = -1.
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0, CAIOBJECTTYPE_C_SORCERER, 0, 1) == -1);
    pSprite->m_derivedStats.m_nCHR = 6;
    // 6: -2.5, truncated to -2.
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0, CAIOBJECTTYPE_C_BARD, 0, 1) == -2);
    // No casting ability: (0 - 11) / 2, truncated to -5 (kept from the
    // original).
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0, CAIOBJECTTYPE_C_FIGHTER, 0, 1) == -5);
    // A cleric of Bane against a saving throw of exactly 0x10: one more.
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0x10, CAIOBJECTTYPE_C_CLERIC, SPECMASK_CLERIC_BANE, 1) == 3);
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0x18, CAIOBJECTTYPE_C_CLERIC, SPECMASK_CLERIC_BANE, 1) == 2);
    CHECK(rules.GetSaveAdjustmentForSourceSprite(pSprite, res, 0x10, CAIOBJECTTYPE_C_CLERIC, SPECMASK_CLERIC_HELM, 1) == 2);
}

TEST_CASE("spell ability effect", "[spell][data]")
{
    REQUIRE_ENGINE();
    CSpell spell(CResRef("SPWI112"));
    REQUIRE(spell.pRes != nullptr);
    REQUIRE(spell.GetAbilityCount() > 0);
    SPELL_ABILITY* pAbility = spell.GetAbility(0);
    REQUIRE(pAbility != nullptr);
    REQUIRE(pAbility->effectCount > 0);

    spell.pRes->Demand();
    const BYTE school = spell.pRes->m_pHeader->school;
    const BYTE secondaryType = spell.pRes->m_pHeader->secondaryType;
    const DWORD flags = spell.pRes->m_pHeader->itemFlags;
    const DWORD effectID = spell.pRes->GetAbilityEffect(0, 0, 10)->effectID;
    spell.pRes->Release();

    SECTION("no object")
    {
        CGameEffect* pEffect = spell.GetAbilityEffect(0, 0, nullptr, 0, 0, 0);
        REQUIRE(pEffect != nullptr);
        CHECK(pEffect->m_effectID == effectID);
        CHECK(pEffect->m_school == school);
        CHECK(pEffect->field_48 == secondaryType);
        CHECK(pEffect->m_sourceType == 1);
        CHECK(pEffect->m_sourceRes == CResRef("SPWI112"));
        CHECK(pEffect->m_sourceFlags == flags);
        CHECK(pEffect->m_sourceID == -1);
    }

    SECTION("past the ability's effects")
    {
        CHECK(spell.GetAbilityEffect(0, pAbility->effectCount, nullptr, 0, 0, 0) == nullptr);
    }

    SECTION("a sprite: the caster level and the class bits")
    {
        CGameSprite* pSprite = MakeSprite();
        CGameEffect* pEffect = spell.GetAbilityEffect(0, 0, pSprite, CAIOBJECTTYPE_C_CLERIC, SPECMASK_CLERIC_HELM, 7);
        REQUIRE(pEffect != nullptr);
        const BYTE index = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetSpecializationIndex(CAIOBJECTTYPE_C_CLERIC, SPECMASK_CLERIC_HELM);
        CHECK(pEffect->m_casterLevel == (7u | (CAIOBJECTTYPE_C_CLERIC << 8) | (static_cast<DWORD>(index) << 16)));
        CHECK(pEffect->m_spellPenetrationBonus == static_cast<DWORD>(pSprite->GetFeatValue(CGAMESPRITE_FEAT_SPELL_PENETRATION) << 1));

        pEffect = spell.GetAbilityEffect(0, 0, pSprite, CAIOBJECTTYPE_C_WIZARD, 0, 0);
        REQUIRE(pEffect != nullptr);
        const BYTE level = static_cast<BYTE>(pSprite->GetCasterLevel(&spell, CAIOBJECTTYPE_C_WIZARD, 0));
        CHECK(pEffect->m_casterLevel == (static_cast<DWORD>(level) | (CAIOBJECTTYPE_C_WIZARD << 8)));
    }
}

TEST_CASE("spell resref effects", "[spell][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    CSpell spell(CResRef("SPWI112"));
    const INT count = spell.GetAbilityCount();
    REQUIRE(count > 0);

    // The ability for a level: the last whose minimum level is not above it.
    const INT nLevel = 20;
    INT expected = -1;
    for (INT i = 0; i < count; i++) {
        if (spell.GetAbility(i)->minCasterLevel > nLevel) {
            break;
        }
        expected = i;
    }
    REQUIRE(expected >= 0);
    const INT effectCount = spell.GetAbility(expected)->effectCount;
    REQUIRE(effectCount > 0);

    const BYTE resRef[8] = { 'S', 'P', 'W', 'I', '1', '1', '2', 0 };
    std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite, resRef, nLevel);
    CHECK(static_cast<INT>(effects.size()) == effectCount);
    for (CGameEffect* pEffect : effects) {
        CHECK(pEffect->m_sourceID == pSprite->m_id);
        CHECK(pEffect->m_source == pSprite->GetPos());
        CHECK(pEffect->m_sourceRes == CResRef("SPWI112"));
    }

    // An eight-character buffer with no terminator.
    const BYTE full[8] = { 'S', 'P', 'W', 'I', '1', '1', '2', 'X' };
    CHECK(IcewindMisc::GetSpellResrefEffects(pSprite, full, nLevel).empty());

    // Below the first ability's level: no ability, no effects.
    if (spell.GetAbility(0)->minCasterLevel > 0) {
        CHECK(IcewindMisc::GetSpellResrefEffects(pSprite, resRef, spell.GetAbility(0)->minCasterLevel - 1).empty());
    }
}
