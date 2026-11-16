#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CResRef.h"
#include "engine_world.h"

#include <cstring>

// CInfGame::TakeFamiliarResRef (0x5AF540): the Find Familiar effect's pick.
// Clamps the level to 1..9, folds the caster's alignment byte onto one of
// the nine familiar pools through the grid formula's byte wrap (0x11..0x33
// onto 0..8; everything else misses), pops a random stored resref from that
// pool's level list - posting the removal message - or names the pool's
// chapter default when the list is empty. Alignment 0 names nothing.

namespace {

using engine_world::DataDir;
using engine_world::FinishedCharacter;
using engine_world::FramePump;
using engine_world::NewGameInStartArea;
using engine_world::StartEngine;

// The game the pools live in, made once per process (CInfGame::NewGame
// asserts when a game is already standing).
CInfGame* GameOrSkip()
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    static FramePump pump;
    struct Once {
        CInfGame* pGame = NULL;
        bool emptyChr = false;
        Once()
        {
            std::vector<BYTE> bytes = FinishedCharacter();
            if (bytes.empty()) {
                emptyChr = true;
                return;
            }
            NewGameInStartArea(bytes);
            pGame = g_pBaldurChitin->GetObjectGame();
        }
    };
    static Once once;
    if (once.emptyChr) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(once.pGame != NULL);
    return once.pGame;
}

// The pool default for alignment 0x11 (lawful good), spelled as a resref.
void SetDefaultFamiliar(CInfGame* pGame, const char* name)
{
    RESREF resRef = { 0 };
    std::memcpy(resRef, name, std::strlen(name));
    std::memcpy(pGame->m_defaultFamiliarResRefs[0], resRef, sizeof(RESREF));
}

} // namespace

TEST_CASE("TakeFamiliarResRef names the pool default when the level list is empty", "[familiar]")
{
    CInfGame* pGame = GameOrSkip();

    SetDefaultFamiliar(pGame, "MYFAMLR");
    REQUIRE(pGame->m_familiarResRefs[0][6].GetCount() == 0);

    CString sFamiliar;
    pGame->TakeFamiliarResRef(7, 0x11, sFamiliar);
    CHECK(sFamiliar == "MYFAMLR");
}

TEST_CASE("TakeFamiliarResRef pops a stored familiar and empties the slot", "[familiar]")
{
    CInfGame* pGame = GameOrSkip();

    SetDefaultFamiliar(pGame, "DFAMLR");
    pGame->m_familiarResRefs[0][6].RemoveAll();
    pGame->m_familiarResRefs[0][6].AddTail(new CResRef("STORED1"));
    REQUIRE(pGame->m_familiarResRefs[0][6].GetCount() == 1);

    // One stored entry: rand() % 1 is always 0, so the pick is the entry.
    CString sFamiliar;
    pGame->TakeFamiliarResRef(7, 0x11, sFamiliar);
    CHECK(sFamiliar == "STORED1");
    CHECK(pGame->m_familiarResRefs[0][6].GetCount() == 0);
}

TEST_CASE("TakeFamiliarResRef clamps the level into the pool's nine slots", "[familiar]")
{
    CInfGame* pGame = GameOrSkip();

    pGame->m_familiarResRefs[0][0].RemoveAll();
    pGame->m_familiarResRefs[0][0].AddTail(new CResRef("STORED0"));
    REQUIRE(pGame->m_familiarResRefs[0][0].GetCount() == 1);

    // Level 0 clamps to 1, which reads the first slot.
    CString sFamiliar;
    pGame->TakeFamiliarResRef(0, 0x11, sFamiliar);
    CHECK(sFamiliar == "STORED0");
    CHECK(pGame->m_familiarResRefs[0][0].GetCount() == 0);
}

TEST_CASE("TakeFamiliarResRef names nothing for an alignment off the grid", "[familiar]")
{
    CInfGame* pGame = GameOrSkip();

    // 0x10 folds to index 255 ((1 + 0x54) * 3 + 0 over the byte), which is
    // off the nine pools.
    CString sFamiliar = "LEFTOVER";
    pGame->TakeFamiliarResRef(7, 0x10, sFamiliar);
    CHECK(sFamiliar == "");
}

TEST_CASE("TakeFamiliarResRef names nothing for alignment 0", "[familiar]")
{
    CInfGame* pGame = GameOrSkip();

    SetDefaultFamiliar(pGame, "MYFAMLR");

    CString sFamiliar = "LEFTOVER";
    pGame->TakeFamiliarResRef(7, 0, sFamiliar);
    CHECK(sFamiliar == "");
}
