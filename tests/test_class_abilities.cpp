// CGameSprite::sub_71E760 (0x71E760): the class-ability reapply pass. With
// every class level zeroed the eleven ApplyClassAbilities arms skip, so the
// observable part is the tail: drow (elf subrace 1) and svirfneblin (gnome
// subrace 1) gain one point of base magic resistance per level gained,
// nothing for other races or subraces, and nothing without a level gain.

#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CDerivedStats.h"
#include "CGameSprite.h"
#include "engine_world.h"

namespace {

using engine_world::DataDir;
using engine_world::FinishedCharacter;
using engine_world::FramePump;
using engine_world::NewGameInStartArea;
using engine_world::StartEngine;

// The standard prologue: one party member per process (CInfGame::NewGame
// asserts when a game is already standing) or a skip when the install or a
// finished character is missing.
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
    return once.pSprite;
}

// Zero every class level so the eleven arms skip ApplyClassAbilities (which
// reads the class-ability 2DAs), then level the sprite from nOldLevel to
// nNewLevel under the given race/subrace and return the magic-resistance
// delta of one sub_71E760 call.
int MagicResistDelta(BYTE nRace, BYTE nSubRace, BYTE nOldLevel, BYTE nNewLevel)
{
    CGameSprite* pSprite = ProbeOrSkip();
    for (INT i = 0; i < CAIOBJECT_CLASS_MAX; i++) {
        pSprite->m_derivedStats.m_nClassLevels[i] = 0;
    }

    CDerivedStats oldStats;
    oldStats.m_nLevel = nOldLevel;
    pSprite->m_derivedStats.m_nLevel = nNewLevel;
    pSprite->m_typeAI.m_nRace = nRace;
    pSprite->m_typeAI.m_nSubRace = nSubRace;

    BYTE nBefore = pSprite->m_baseStats.m_resistMagicBase;
    pSprite->sub_71E760(oldStats, TRUE);
    return pSprite->m_baseStats.m_resistMagicBase - nBefore;
}

} // namespace

TEST_CASE("class abilities: drow gains magic resistance per level", "[sprite]")
{
    REQUIRE(MagicResistDelta(CAIOBJECTTYPE_R_ELF, 1, 10, 13) == 3);
}

TEST_CASE("class abilities: svirfneblin gains magic resistance per level", "[sprite]")
{
    REQUIRE(MagicResistDelta(CAIOBJECTTYPE_R_GNOME, 1, 4, 6) == 2);
}

TEST_CASE("class abilities: human gains none", "[sprite]")
{
    REQUIRE(MagicResistDelta(CAIOBJECTTYPE_R_HUMAN, 0, 10, 15) == 0);
}

TEST_CASE("class abilities: surface elf gains none", "[sprite]")
{
    REQUIRE(MagicResistDelta(CAIOBJECTTYPE_R_ELF, 2, 10, 15) == 0);
}

TEST_CASE("class abilities: no level gained gains none", "[sprite]")
{
    REQUIRE(MagicResistDelta(CAIOBJECTTYPE_R_ELF, 1, 10, 10) == 0);
    // A lost level subtracts nothing too (the original only loops a
    // positive diff).
    REQUIRE(MagicResistDelta(CAIOBJECTTYPE_R_ELF, 1, 10, 9) == 0);
}
