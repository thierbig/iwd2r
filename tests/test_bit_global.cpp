#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameAIBase.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CVariableHash.h"
#include "engine_world.h"

// CGameAIBase::BitGlobal (0x4688A0), dispatched as script action 247:
// combines an integer mask into a variable with a bitwise op. String1 names
// the variable as "SCOPE:NAME" (uppercased); m_specificID is the mask and
// m_specificID2 picks the op: 1 AND, 2 OR, 3 XOR, anything else fails
// without writing. GLOBAL writes the game variables, LOCALS the sprite's
// own variables (sprite objects only; any other object ends the action
// silently), anything else an area named by the scope -- MYAREA meaning the
// acting area's own name. A missing variable is created from 0.

namespace {

using engine_world::DataDir;
using engine_world::FinishedCharacter;
using engine_world::FramePump;
using engine_world::NewGameInStartArea;
using engine_world::StartEngine;

// The party member and its game, made once per process: CInfGame::NewGame
// asserts when a game is already standing, so a second NewGameInStartArea
// in one process aborts (the binary runs all cases together when invoked
// directly). The tests use distinct variable names instead of cleaning up.
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

// Arms m_curAction as a real action 247 and runs it through the sprite's
// ExecuteAction: the same path the script engine takes (the sprite switch
// hands the id down to CGameAIBase::ExecuteAction's case 247).
SHORT RunAction247(CGameSprite* pSprite, const char* sDest, LONG nMask, LONG nOp)
{
    pSprite->m_curAction.m_actionID = 247;
    pSprite->m_curAction.SetString1(sDest);
    pSprite->m_curAction.m_specificID = nMask;
    pSprite->m_curAction.m_specificID2 = nOp;
    return pSprite->ExecuteAction();
}

// Adds nValue under sName and returns the stored copy.
CVariable* AddVariable(CVariableHash* pHash, const char* sName, LONG nValue)
{
    CVariable var;
    var.SetName(sName);
    var.m_intValue = nValue;
    pHash->AddKey(var);
    return pHash->FindKey(sName);
}

} // namespace

TEST_CASE("bit global: GLOBAL AND, OR and XOR, the create arm and the bad op", "[ai][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CVariableHash* pGlobals = g_pBaldurChitin->GetObjectGame()->GetVariables();

    REQUIRE(AddVariable(pGlobals, "IWD2RBITDST", 0x0F0F) != NULL);

    // The three ops on an existing variable.
    CHECK(RunAction247(pSprite, "GLOBAL:IWD2RBITDST", 0x00FF, 2)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RBITDST")->m_intValue == 0x0FFF);

    pGlobals->FindKey("IWD2RBITDST")->m_intValue = 0x0F0F;
    CHECK(RunAction247(pSprite, "GLOBAL:IWD2RBITDST", 0x00FF, 3)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RBITDST")->m_intValue == 0x0FF0);

    pGlobals->FindKey("IWD2RBITDST")->m_intValue = 0x0F0F;
    CHECK(RunAction247(pSprite, "GLOBAL:IWD2RBITDST", 0x00FF, 1)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RBITDST")->m_intValue == 0x000F);

    // A missing variable is created from 0 and then combined.
    CHECK(RunAction247(pSprite, "GLOBAL:IWD2RBITNEW", 0x00FF, 2)
        == CGameAIBase::ACTION_DONE);
    CVariable* pNew = pGlobals->FindKey("IWD2RBITNEW");
    REQUIRE(pNew != NULL);
    CHECK(pNew->m_intValue == 0x00FF);

    // An unknown op fails without writing: the variable keeps its value and
    // a missing variable is not created.
    CHECK(RunAction247(pSprite, "GLOBAL:IWD2RBITDST", 0x00FF, 7)
        == CGameAIBase::ACTION_ERROR);
    CHECK(pGlobals->FindKey("IWD2RBITDST")->m_intValue == 0x000F);
    CHECK(RunAction247(pSprite, "GLOBAL:IWD2RBITBAD", 0x00FF, 7)
        == CGameAIBase::ACTION_ERROR);
    CHECK(pGlobals->FindKey("IWD2RBITBAD") == NULL);
}

TEST_CASE("bit global: LOCALS writes the sprite's own variables", "[ai][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CVariableHash* pLocals = pSprite->GetLocalVariables();
    REQUIRE(pLocals != NULL);

    REQUIRE(AddVariable(pLocals, "IWD2RBITLDST", 0x0FF0) != NULL);

    CHECK(RunAction247(pSprite, "LOCALS:IWD2RBITLDST", 0x00F0, 1)
        == CGameAIBase::ACTION_DONE);
    CHECK(pLocals->FindKey("IWD2RBITLDST")->m_intValue == 0x00F0);

    // A missing variable is created from 0 and then combined.
    CHECK(RunAction247(pSprite, "LOCALS:IWD2RBITLNEW", 0x00F0, 3)
        == CGameAIBase::ACTION_DONE);
    CVariable* pNew = pLocals->FindKey("IWD2RBITLNEW");
    REQUIRE(pNew != NULL);
    CHECK(pNew->m_intValue == 0x00F0);

    // LOCALS never touches the game variables.
    CHECK(g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey("IWD2RBITLDST") == NULL);
}

TEST_CASE("bit global: MYAREA and a named area write that area's variables", "[ai][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CGameArea* pArea = pSprite->GetArea();
    CVariableHash* pAreaVars = pArea->GetVariables();
    CVariableHash* pGlobals = g_pBaldurChitin->GetObjectGame()->GetVariables();

    // MYAREA resolves to the acting area's own name.
    REQUIRE(AddVariable(pAreaVars, "IWD2RBITADST", 0x0F0F) != NULL);
    CHECK(RunAction247(pSprite, "MYAREA:IWD2RBITADST", 0x0FF0, 2)
        == CGameAIBase::ACTION_DONE);
    CHECK(pAreaVars->FindKey("IWD2RBITADST")->m_intValue == 0x0FFF);

    // A scope naming an area directly works the same, and a missing
    // variable is created there from 0 (so the combine yields the mask
    // under XOR; AND would leave the 0).
    const char* sAreaName = reinterpret_cast<const char*>(pArea->m_header.m_areaName);
    REQUIRE(sAreaName[0] != '\0');
    std::string sDirect = std::string(sAreaName) + ":IWD2RBITANEW";
    CHECK(RunAction247(pSprite, sDirect.c_str(), 0x0FF0, 3)
        == CGameAIBase::ACTION_DONE);
    CVariable* pNew = pAreaVars->FindKey("IWD2RBITANEW");
    REQUIRE(pNew != NULL);
    CHECK(pNew->m_intValue == 0x0FF0);

    // A scope naming no area writes nothing anywhere.
    CHECK(RunAction247(pSprite, "ELSEWHERE:IWD2RBITNOWHERE", 0x0FF0, 2)
        == CGameAIBase::ACTION_DONE);
    CHECK(pAreaVars->FindKey("IWD2RBITNOWHERE") == NULL);
    CHECK(pGlobals->FindKey("IWD2RBITNOWHERE") == NULL);

    // An unknown op on the area arm fails without writing.
    CHECK(RunAction247(pSprite, "MYAREA:IWD2RBITADST", 0x0FF0, 7)
        == CGameAIBase::ACTION_ERROR);
    CHECK(pAreaVars->FindKey("IWD2RBITADST")->m_intValue == 0x0FFF);
}
