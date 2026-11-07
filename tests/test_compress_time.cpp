#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CGameTimer.h"
#include "CInfGame.h"
#include "CRuleTables.h"
#include "CTimerWorld.h"
#include "engine_world.h"

// CGameSprite::CompressTime (0x6FC260): the time compression every sprite
// runs when the clock jumps (world-map travel). Below 901 ms
// (0x385) only CheckTimers advances; a long delta decays intoxication,
// restores morale towards 10, expires timers, reactivates the sprite on
// the search map and (a party member whose wounds show) queues the heal.

namespace {

using engine_world::DataDir;
using engine_world::FinishedCharacter;
using engine_world::FramePump;
using engine_world::NewGameInStartArea;
using engine_world::StartEngine;

// The party member and its game, made once per process: CInfGame::NewGame
// asserts when a game is already standing, so a second
// NewGameInStartArea in one process aborts. The tests re-arm the same
// sprite instead.
CGameSprite* Probe(bool& emptyChr)
{
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
    emptyChr = once.emptyChr;
    return once.pSprite;
}

// The shared probe set up for another compression: the leftover timers
// gone, a fresh 100-cycle timer standing, the morale/intoxication dials
// turned, and the three actives the search-bitmap gate reads cooled (a
// fresh party member starts hot).
void ArmProbe(CGameSprite* pSprite)
{
    POSITION pos = pSprite->m_timers.GetHeadPosition();
    while (pos != NULL) {
        POSITION oldPos = pos;
        CGameTimer* pTimer = pSprite->m_timers.GetNext(pos);
        pSprite->m_timers.RemoveAt(oldPos);
        delete pTimer;
    }
    CGameTimer* pTimer = new CGameTimer;
    pTimer->m_time = 100;
    pTimer->m_id = 7;
    pSprite->m_timers.AddTail(pTimer);
    pSprite->m_baseStats.m_morale = 3;
    pSprite->m_derivedStats.m_nMoraleRecoveryTime = 5;
    pSprite->m_baseStats.m_intoxication = 60;
    pSprite->m_active = FALSE;
    pSprite->m_activeAI = FALSE;
    pSprite->m_activeImprisonment = FALSE;
}

// The standard prologue of a compression test: returns the probe sprite
// or skips the test when the install or a finished character is missing.
CGameSprite* ProbeOrSkip()
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    static FramePump pump;
    bool emptyChr = false;
    CGameSprite* pSprite = Probe(emptyChr);
    if (emptyChr) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(pSprite != NULL);
    REQUIRE(pSprite->GetArea() != NULL);
    return pSprite;
}

} // namespace

TEST_CASE("compress: below 901 ms only the timers advance", "[compress][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    ArmProbe(pSprite);

    CHECK(pSprite->CompressTime(900) == TRUE);

    // CheckTimers ran (900 / TIMESCALE_MSEC_PER_SEC = 60 cycles) before the
    // early return; everything after it did not.
    CHECK(pSprite->m_timers.GetCount() == 1);
    CHECK(pSprite->m_timers.GetHead()->m_time == 40);
    CHECK(pSprite->m_baseStats.m_morale == 3);
    CHECK(pSprite->m_baseStats.m_intoxication == 60);
    CHECK(pSprite->m_active == FALSE);
}

TEST_CASE("compress: a long delta decays intoxication, restores morale and expires timers", "[compress][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    ArmProbe(pSprite);
    pSprite->m_baseStats.m_hitPoints = 5;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    INT nIntoxicationRate = 0;
    INT nRecoveryRate = 0;
    INT nIntoxicationCap = 0;
    pGame->GetRuleTables().GetIntoxicationInfo(static_cast<BYTE>(pSprite->m_derivedStats.m_nCON),
        nIntoxicationRate,
        nRecoveryRate,
        nIntoxicationCap);

    // The wound heal and the dirty flag live in the non-party branch
    // (m_bGlobal == 0); a party member instead walks the formation path,
    // whose ProcessEffectList consumes field_562C. Drive the non-party
    // branch with the same sprite.
    pSprite->m_bGlobal = FALSE;

    CHECK(pSprite->CompressTime(3600000) == TRUE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    pSprite->m_bGlobal = TRUE;

    // The timer burnt out (100 - 3600000/15 cycles) and was dropped.
    CHECK(pSprite->m_timers.IsEmpty());

    // Intoxication decays by the hours passed times the recovery rate,
    // floored at zero.
    const LONG nHours = 3600000 / CTimerWorld::TIMESCALE_MSEC_PER_HOUR;
    LONG nExpected = 60 - nHours * nRecoveryRate;
    if (nExpected < 0) {
        nExpected = 0;
    }
    CHECK(pSprite->m_baseStats.m_intoxication == nExpected);

    // Morale walks towards 10 by (deltaTime/2) / (rate *
    // TIMESCALE_MSEC_PER_SEC) steps; an hour of rest saturates it.
    CHECK(pSprite->m_baseStats.m_morale == 10);

    // The persistant-effect clock reset and the search-bitmap gate
    // reactivated the sprite (its hour bit is set).
    CHECK(pSprite->field_562C == 1);
    CHECK(pSprite->m_spriteEffectDuration == 0);
    CHECK(pSprite->m_active == TRUE);

    // The wounds showed, so the 10000-point heal (effect 17) was queued
    // and applied with the sprite updates.
    CHECK(pSprite->m_baseStats.m_hitPoints > 5);
}

TEST_CASE("compress: an expired sprite leaves the area", "[compress][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    ArmProbe(pSprite);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->m_worldTime.m_gameTime = 1000;
    pSprite->m_expirationTime = 1;

    // The expiration is checked after CheckTimers but before anything
    // else, so the timer ticks once and the call reports the removal.
    CHECK(pSprite->CompressTime(1000) == FALSE);
    CHECK(pSprite->m_timers.GetCount() == 1);
    CHECK(pSprite->m_timers.GetHead()->m_time == 100 - 1000 / CTimerWorld::TIMESCALE_MSEC_PER_SEC);
    CHECK(pSprite->GetArea() == NULL);
}
