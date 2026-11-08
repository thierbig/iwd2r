#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CDerivedStats.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CRuleTables.h"
#include "engine_world.h"

// CGameSprite::CheckStatsChange (0x71C0C0): the derived-state recompute
// after a stats change. Refreshes each spell list's slot maxima and clamps
// the current slot count to them, applies the CON hit-point bonus delta (the
// decrease arm floors hit points at 1 and leaves a dead sprite's hit
// points untouched), adds the ability modifiers to the three saves
// (paladins add a positive charisma modifier on top), caps XP and sets the
// level-up flag from GetNextLevel.

namespace {

using engine_world::DataDir;
using engine_world::FinishedCharacter;
using engine_world::FramePump;
using engine_world::NewGameInStartArea;
using engine_world::StartEngine;

// The party member and its game, made once per process: CInfGame::NewGame
// asserts when a game is already standing, so a second NewGameInStartArea
// in one process aborts (the binary runs both cases together when invoked
// directly). The tests set up everything they read instead.
CGameSprite* ProbeOrSkip()
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    static FramePump pump;
    struct Once {
        CGameSprite* pSprite = NULL;
        bool emptyChr = false;
        Once()
        {
            std::vector<BYTE> bytes = FinishedCharacter();
            if (bytes.empty()) {
                emptyChr = true;
                return;
            }
            pSprite = NewGameInStartArea(bytes);
        }
    };
    static Once once;
    if (once.emptyChr) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(once.pSprite != NULL);
    REQUIRE(once.pSprite->GetArea() != NULL);
    return once.pSprite;
}

// The rule-table CON bonus of the probe's current stats: what
// CheckStatsChange stores into m_nHPCONBonusTotalOld.
INT ConBonus(CGameSprite* pSprite)
{
    return g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetHPCONBonusTotal(
        pSprite->m_typeAI, pSprite->m_derivedStats, static_cast<BYTE>(pSprite->m_derivedStats.m_nCON));
}

} // namespace

TEST_CASE("stats change: the CON bonus delta moves hit points with a floor of 1", "[stats][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    const INT nBonus = ConBonus(pSprite);

    // Calibrate: the first call stores the current bonus as the old one.
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_nHPCONBonusTotalOld == nBonus);
    CHECK(pSprite->m_bHPCONBonusTotalUpdate == FALSE);

    // Roomy hit-point fields, so the deltas below are observable on the
    // level-1 probe without the level floor or the final clamp masking
    // them, and the watermark starts empty.
    pSprite->m_derivedStats.m_nMaxHitPoints = 50;
    pSprite->m_baseStats.m_maxHitPointsBase = 40;
    pSprite->field_7110 = 0;

    // A decrease below the old bonus takes hit points down with it, but a
    // living sprite never drops below 1 hit point.
    pSprite->m_baseStats.m_hitPoints = 2;
    pSprite->m_nHPCONBonusTotalOld = nBonus + 3;
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_baseStats.m_hitPoints == 1);
    CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 47);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 37);
    CHECK(pSprite->m_nHPCONBonusTotalOld == nBonus);

    // The same decrease against a dead sprite leaves its hit points
    // untouched (unlike the load-time sibling, which zeroes them) while
    // the maxima still take the delta.
    pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
    pSprite->m_baseStats.m_hitPoints = 7;
    pSprite->m_nHPCONBonusTotalOld = nBonus + 3;
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_baseStats.m_hitPoints == 7);
    CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 44);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 34);
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;

    // An increase above the old bonus raises hit points and both maxima.
    pSprite->m_baseStats.m_hitPoints = 20;
    pSprite->m_nHPCONBonusTotalOld = nBonus - 3;
    const SHORT nMax3 = pSprite->m_derivedStats.m_nMaxHitPoints;
    const WORD nBase3 = pSprite->m_baseStats.m_maxHitPointsBase;
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == nMax3 + 3);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase == nBase3 + 3);
    INT nExpectedHp = 20 + 3;
    if (nExpectedHp > nMax3 + 3) {
        nExpectedHp = nMax3 + 3;
    }
    CHECK(pSprite->m_baseStats.m_hitPoints == nExpectedHp);

    // The bookkeeping the next call reads: the new bonus stored, the
    // update flag cleared, hit points within the maximum, and the max-ever
    // watermark (empty before) holding them.
    CHECK(pSprite->m_nHPCONBonusTotalOld == nBonus);
    CHECK(pSprite->m_bHPCONBonusTotalUpdate == FALSE);
    CHECK(pSprite->m_baseStats.m_hitPoints <= pSprite->m_derivedStats.m_nMaxHitPoints);
    CHECK(pSprite->field_7110 == pSprite->m_baseStats.m_hitPoints);
}

TEST_CASE("stats change: slot maxima clamp, saves, the XP cap and the level-up flag", "[stats][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // One call populates every list's slot maximum (GetMaxKnownSpells).
    pSprite->CheckStatsChange();

    // Overfilling the current counts above the maxima: the next call
    // clamps them back down (the sibling loop at the load-time site does
    // not -- the asm here does).
    UINT nBumped = 0;
    for (BYTE nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            CGameSpriteSpellList* pList = pSprite->m_spells.GetSpellsAtLevel(nClassIndex, nLevel);
            pList->field_18 = pList->field_14 + 7;
            nBumped++;
        }
    }
    pSprite->CheckStatsChange();
    UINT nClamped = 0;
    for (BYTE nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            CGameSpriteSpellList* pList = pSprite->m_spells.GetSpellsAtLevel(nClassIndex, nLevel);
            if (pList->field_18 == pList->field_14) {
                nClamped++;
            }
        }
    }
    CHECK(nClamped == nBumped);

    // The three saves take their ability modifier, plus divine grace for a
    // paladin with a positive charisma modifier.
    const CRuleTables& rules = pGame->GetRuleTables();
    const INT nModCon = rules.GetAbilityScoreModifier(pSprite->m_derivedStats.m_nCON);
    const INT nModDex = rules.GetAbilityScoreModifier(pSprite->m_derivedStats.m_nDEX);
    const INT nModWis = rules.GetAbilityScoreModifier(pSprite->m_derivedStats.m_nWIS);
    const INT nModChr = rules.GetAbilityScoreModifier(pSprite->m_derivedStats.m_nCHR);
    INT nGrace = 0;
    if (pSprite->m_typeAI.IsClassValid(CAIObjectType::C_PALADIN) && nModChr > 0) {
        nGrace = nModChr;
    }
    const SHORT nFort = pSprite->m_derivedStats.m_nSaveVSFortitude;
    const SHORT nRef = pSprite->m_derivedStats.m_nSaveVSReflex;
    const SHORT nWill = pSprite->m_derivedStats.m_nSaveVSWill;
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_derivedStats.m_nSaveVSFortitude == nFort + nModCon + nGrace);
    CHECK(pSprite->m_derivedStats.m_nSaveVSReflex == nRef + nModDex + nGrace);
    CHECK(pSprite->m_derivedStats.m_nSaveVSWill == nWill + nModWis + nGrace);

    // XP over the cap comes back down to it, and the level-up flag ends
    // the call holding what GetNextLevel says (both behind the portrait
    // gate).
    if (pGame->GetCharacterPortraitNum(pSprite->m_id) == -1) {
        SKIP("[iwd2r-skip] the probe has no portrait; the XP cap and level-up gate is closed");
    }
    const DWORD nCap = rules.GetXPCap();
    pSprite->m_derivedStats.m_nXP = nCap + 12345u;
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_derivedStats.m_nXP == nCap);

    CDerivedStats statsNext;
    statsNext = pSprite->m_derivedStats;
    rules.GetNextLevel(pSprite->m_startTypeAI.m_nClass, statsNext, pSprite);
    const BOOL bExpectedLevelUp = statsNext.m_nLevel != pSprite->m_derivedStats.m_nLevel;
    pSprite->CheckStatsChange();
    CHECK(pSprite->m_bLevelUp == bExpectedLevelUp);
}
