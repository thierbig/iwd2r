#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameContainer.h"
#include "CGameSprite.h"
#include "CGameTimer.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CRuleTables.h"
#include "CStore.h"
#include "CTimerWorld.h"
#include "engine_world.h"

#include <initializer_list>
#include <utility>

// CGameSprite::CompressTime (0x6FC260): the time compression every sprite
// runs when the clock jumps (world-map travel). Below 901 ms
// (0x385) only CheckTimers advances; a long delta decays intoxication,
// restores morale towards 10, expires timers, reactivates the sprite on
// the search map and (a party member whose wounds show) queues the heal.
//
// CGameContainer::CompressTime (0x47D850): the same virtual on the
// container. The polygon-draw timer always ages; a delta of a full day or
// more on a ground pile (type 4) then drops every unflagged item, ages a
// bag item's contents through its store and empties the pile into
// m_bDeleteMe — unless a critical (flags bit 0) item survives in it.

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
struct ProbeState {
    CGameSprite* pSprite = NULL;
    // The start area and the sprite's place in it, read while the sprite
    // stands there: the expired-sprite case below evicts the sprite, and
    // the container cases after it still build on the (still loaded)
    // area.
    CGameArea* pArea = NULL;
    CPoint ptHome;
    bool emptyChr = false;

    ProbeState()
    {
        std::vector<BYTE> bytes = FinishedCharacter();
        if (bytes.empty()) {
            emptyChr = true;
            return;
        }
        pSprite = NewGameInStartArea(bytes);
        if (pSprite != NULL && pSprite->GetArea() != NULL) {
            pArea = pSprite->GetArea();
            ptHome = pSprite->GetPos();
        }
    }
};

ProbeState& TheProbe()
{
    static ProbeState state;
    return state;
}

CGameSprite* Probe(bool& emptyChr)
{
    emptyChr = TheProbe().emptyChr;
    return TheProbe().pSprite;
}

CGameArea* ProbeArea(bool& emptyChr)
{
    emptyChr = TheProbe().emptyChr;
    return TheProbe().pArea;
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
    bool emptyArea = false;
    REQUIRE(ProbeArea(emptyArea) != NULL);
    return pSprite;
}

// Drops the messages waiting in the global handler, so a test can count
// the ones its own call adds (the pattern tests/test_rest_store.cpp uses).
void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// A heap ground pile (type 4) beside the probe sprite's place in the
// start area, the state the game leaves when items land on the ground.
// The caller retires it with RemoveFromArea(), which takes it out of the
// object array and deletes it (the destructor frees whatever is still in
// m_lstItems).
CGameContainer* NewPile()
{
    bool emptyChr = false;
    CGameArea* pArea = ProbeArea(emptyChr);
    const CPoint& pt = TheProbe().ptHome;
    return new CGameContainer(pArea, CRect(pt.x - 8, pt.y - 6, pt.x + 9, pt.y + 7));
}

// Writes a gem bag's store into the game's temp folder with one line per
// item (and that item's stack count) — the state a player leaves behind by
// dropping things into the bag. A fresh CStore on the same resref then
// reads this copy, which is how CompressTime finds the bag's contents.
void SeedBag(const char* sBagRes, std::initializer_list<std::pair<const char*, WORD>> items)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CResRef cResBag(sBagRes);
    CStore bag(cResBag);
    REQUIRE(bag.GetType() == 4);
    // The retail gem bags ship empty.
    REQUIRE(bag.GetNumItems() == 0);
    for (const auto& [sItemRes, nUseCount] : items) {
        CItem seed(CResRef(sItemRes), nUseCount, 0, 0, 0, 0);
        REQUIRE(bag.AddItemExt(seed, 1) >= 0);
    }
    bag.Marshal(pGame->m_sTempDir);
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

TEST_CASE("compress: a container ages its polygon-draw timer while the day and type gates hold the pile", "[compress][data]")
{
    ProbeOrSkip();
    CGameContainer* pPile = NewPile();
    CItem* pGem = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    pPile->m_lstItems.AddTail(pGem);

    // The timer ages through an unsigned compare of the sign-extended
    // value: a timer older than the delta decays, a younger one clamps.
    pPile->m_drawPoly = 5000;
    CHECK(pPile->CompressTime(1000) == TRUE);
    CHECK(pPile->m_drawPoly == 4000);

    pPile->m_drawPoly = 500;
    CHECK(pPile->CompressTime(1000) == TRUE);
    CHECK(pPile->m_drawPoly == 0);

    // A negative timer counts as a huge unsigned value, so it decays too.
    pPile->m_drawPoly = -100;
    CHECK(pPile->CompressTime(1000) == TRUE);
    CHECK(pPile->m_drawPoly == -1100);

    // A delta of less than a full day leaves the pile itself alone, even
    // on a ground pile whose items are all droppable.
    pPile->m_drawPoly = 5000;
    CHECK(pPile->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY) == TRUE);
    CHECK(pPile->m_lstItems.GetCount() == 1);
    CHECK(pPile->m_lstItems.GetHead() == pGem);
    CHECK(pPile->m_bDeleteMe == FALSE);

    pPile->RemoveFromArea();

    // A chest (any container that is not a ground pile) never decays, no
    // matter the delta.
    CGameContainer* pChest = NewPile();
    pChest->m_containerType = 3;
    pChest->m_lstItems.AddTail(new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0));
    pChest->m_lstItems.AddTail(new CItem(CResRef("11KEYWW2"), 1, 0, 0, 0, 0));
    pChest->m_drawPoly = 5000;

    CHECK(pChest->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) == TRUE);
    CHECK(pChest->m_drawPoly == 0);
    CHECK(pChest->m_lstItems.GetCount() == 2);
    CHECK(pChest->m_bDeleteMe == FALSE);

    pChest->RemoveFromArea();
}

TEST_CASE("compress: a ground pile drops its unflagged items and empties itself", "[compress][data]")
{
    ProbeOrSkip();
    ClearMessages();
    CGameContainer* pPile = NewPile();
    pPile->m_lstItems.AddTail(new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0));
    // A NULL node (a marshalled empty slot) is dropped without a delete.
    pPile->m_lstItems.AddTail(static_cast<CItem*>(NULL));
    pPile->m_lstItems.AddTail(new CItem(CResRef("00GEM02"), 1, 0, 0, 0, 0));

    // Gems carry no critical flag (file flags 0x2C), so the whole pile
    // goes: the call reports the removal and the next update deletes it.
    CHECK(pPile->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) == FALSE);
    CHECK(pPile->m_lstItems.IsEmpty());
    CHECK(pPile->m_bDeleteMe == TRUE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == 0);

    pPile->RemoveFromArea();
}

TEST_CASE("compress: a critical item keeps its ground pile alive", "[compress][data]")
{
    ProbeOrSkip();
    ClearMessages();
    CGameContainer* pPile = NewPile();
    pPile->m_lstItems.AddTail(new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0));
    // 11KEYWW2 (the Wandering Village key) carries the critical flag
    // (file flags 0x82D, bit 0).
    pPile->m_lstItems.AddTail(new CItem(CResRef("11KEYWW2"), 1, 0, 0, 0, 0));
    pPile->m_lstItems.AddTail(new CItem(CResRef("00GEM02"), 1, 0, 0, 0, 0));

    // The gems are dropped, the key stays and keeps the pile standing.
    CHECK(pPile->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) == TRUE);
    REQUIRE(pPile->m_lstItems.GetCount() == 1);
    CHECK(pPile->m_lstItems.GetHead()->GetResRef() == CResRef("11KEYWW2"));
    CHECK(pPile->m_bDeleteMe == FALSE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == 0);

    pPile->RemoveFromArea();
}

TEST_CASE("compress: a bag in a pile ages through its store, rescues critical contents and drops itself", "[compress][data]")
{
    ProbeOrSkip();

    // A pile holding a gem bag whose store (marshaled to the temp folder)
    // carries two plain gems.
    SeedBag("00GBGA01", { { "00GEM01", 1 }, { "00GEM02", 1 } });
    ClearMessages();
    CGameContainer* pPile = NewPile();
    pPile->m_lstItems.AddTail(new CItem(CResRef("00GBGA01"), 1, 0, 0, 0, 0));

    // The bag's contents age: every loop pass removes one stack while the
    // index advances, so the walk visits only every other line — of
    // [00GEM01, 00GEM02] the first is removed and the second survives,
    // and the marshaled store keeps it. Nothing is critical, so the bag
    // itself is dropped from the pile and the emptied pile retires.
    CHECK(pPile->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) == FALSE);
    CHECK(pPile->m_lstItems.IsEmpty());
    CHECK(pPile->m_bDeleteMe == TRUE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == 0);

    CStore after(CResRef("00GBGA01"));
    CHECK(after.GetNumItems() == 1);
    CHECK(after.GetItemId(0) == CResRef("00GEM02"));

    pPile->RemoveFromArea();

    // The same walk over [00GEM01, 00GEM02, DECASTA]: pass 0 removes
    // 00GEM01, pass 1 lands on the critical sword (file flags 0x6B, bit
    // 0) — its identity goes out as a CMessageContainerAddItem and it
    // leaves the store — and pass 2 stops. 00GEM02 survives in the store,
    // the bag is dropped from the pile, but the rescued sword keeps the
    // call from reporting the pile's removal. DECASTA (not the plot keys,
    // which carry no ability) is chosen so the stack count the message
    // copies survives GetUsageCount's ability gate.
    SeedBag("00GBGA02", { { "00GEM01", 1 }, { "00GEM02", 1 }, { "DECASTA", 3 } });
    ClearMessages();
    CGameContainer* pPile2 = NewPile();
    pPile2->m_lstItems.AddTail(new CItem(CResRef("00GBGA02"), 1, 0, 0, 0, 0));

    CHECK(pPile2->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) == TRUE);
    CHECK(pPile2->m_lstItems.IsEmpty());
    CHECK(pPile2->m_bDeleteMe == FALSE);

    CStore after2(CResRef("00GBGA02"));
    CHECK(after2.GetNumItems() == 1);
    CHECK(after2.GetItemId(0) == CResRef("00GEM02"));

    // The rescue message carries only the item's identity, stack counts
    // and flags — the partial copy of the local path. The count came back
    // from the store line (3), not the CItem default (1).
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    CHECK(handler.m_messageList.GetCount() == 1);
    auto* pMessage = dynamic_cast<CMessageContainerAddItem*>(handler.m_messageList.GetHead());
    REQUIRE(pMessage != nullptr);
    CHECK(pMessage->m_item.GetResRef() == CResRef("DECASTA"));
    CHECK(pMessage->m_item.GetUsageCount(0) == 3);
    CHECK(pMessage->m_item.m_flags == 1);

    pPile2->RemoveFromArea();
    ClearMessages();
}

TEST_CASE("compress: a bag walk counts store lines, not stock", "[compress][data]")
{
    ProbeOrSkip();

    // One line of five gems whose stock reads 3. The original bounds the
    // walk on the line count ([store+0xFC] at 0x47DDBA), so a single pass
    // takes one gem off the line. Bounding on GetNumItems() (the summed
    // stock, 3) would run three passes, the last two re-using the stale
    // item after GetItem(1) fails, and leave two gems.
    SeedBag("00GBGA03", { { "00GEM01", 5 } });
    {
        CStore bag(CResRef("00GBGA03"));
        REQUIRE(bag.m_lInventory.GetCount() == 1);
        REQUIRE(bag.m_lInventory.GetHead()->m_usageCount[0] == 5);
        bag.m_lInventory.GetHead()->m_nInStock = 3;
        bag.Marshal(g_pBaldurChitin->GetObjectGame()->m_sTempDir);
    }
    ClearMessages();
    CGameContainer* pPile = NewPile();
    pPile->m_lstItems.AddTail(new CItem(CResRef("00GBGA03"), 1, 0, 0, 0, 0));

    CHECK(pPile->CompressTime(CTimerWorld::TIMESCALE_MSEC_PER_DAY + 1) == FALSE);
    CHECK(pPile->m_lstItems.IsEmpty());

    CStore after(CResRef("00GBGA03"));
    REQUIRE(after.m_lInventory.GetCount() == 1);
    CHECK(after.m_lInventory.GetHead()->m_usageCount[0] == 4);

    pPile->RemoveFromArea();
    ClearMessages();
}
