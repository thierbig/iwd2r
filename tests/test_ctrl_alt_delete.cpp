// CtrlAltDelete_EnableCheatKeys (0x4EC350): the console handler that
// flips the world screen's +0x156 cheat-key flag.
//
// The other handlers in src/CtrlAltDelete.cpp forward into a live game
// (party gold/XP, area move, creature spawn) and are pinned by their
// evidence listings instead; this store is the one effect observable
// without game data.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CScreenWorld.h"
#include "CtrlAltDelete.h"

#include <memory>

TEST_CASE("CtrlAltDelete_EnableCheatKeys stores the argument into the world screen's cheat-key flag", "[cheats]")
{
    CBaldurChitin* previousBaldur = g_pBaldurChitin;

    // Zeroed CBaldurChitin with a zeroed CScreenWorld behind +0x1C88
    // (m_pEngineWorld, what GetScreenWorld returns): the flag starts at
    // 0 and the handler is a plain store, so no other field is touched.
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> screen(new unsigned char[sizeof(CScreenWorld)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pEngineWorld = reinterpret_cast<CScreenWorld*>(screen.get());

    CScreenWorld* pScreen = g_pBaldurChitin->GetScreenWorld();
    REQUIRE(pScreen == reinterpret_cast<CScreenWorld*>(screen.get()));
    REQUIRE(pScreen->m_bCheatKeysEnabled == 0);

    CtrlAltDelete_EnableCheatKeys(1);
    CHECK(pScreen->m_bCheatKeysEnabled == 1);

    CtrlAltDelete_EnableCheatKeys(0);
    CHECK(pScreen->m_bCheatKeysEnabled == 0);

    g_pBaldurChitin = previousBaldur;
}
