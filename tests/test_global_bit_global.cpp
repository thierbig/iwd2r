#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameAIBase.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CVariableHash.h"
#include "engine_world.h"

// CGameAIBase::GlobalBitGlobal (0x469040), dispatched as script action 248:
// combines a source variable into a destination variable with a bitwise op.
// String1 names the destination, string2 the source, both "SCOPE:NAME"
// (uppercased); m_specificID picks the op: 1 AND, 2 OR, 3 XOR, anything else
// fails without writing. The source read: GLOBAL reads the game variables,
// LOCALS the sprite's own variables, anything else an area named by the
// scope -- except that a MYAREA destination redirects that fallback into the
// acting area (the original tests the destination scope there). The write
// goes to the storage the destination scope names, creating a missing
// variable from 0.

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

// Arms m_curAction as a real action 248 and runs it through the sprite's
// ExecuteAction: the same path the script engine takes (the sprite switch
// hands the id down to CGameAIBase::ExecuteAction's case 248).
SHORT RunAction248(CGameSprite* pSprite, const char* sDest, const char* sSrc, LONG nOp)
{
    pSprite->m_curAction.m_actionID = 248;
    pSprite->m_curAction.SetString1(sDest);
    pSprite->m_curAction.SetString2(sSrc);
    pSprite->m_curAction.m_specificID = nOp;
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

TEST_CASE("global bit global: GLOBAL destination AND, OR and XOR, the create arm and the bad op", "[ai][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CVariableHash* pGlobals = g_pBaldurChitin->GetObjectGame()->GetVariables();

    REQUIRE(AddVariable(pGlobals, "IWD2RTESTSRC", 0x00FF) != NULL);
    REQUIRE(AddVariable(pGlobals, "IWD2RTESTDST", 0x0F0F) != NULL);

    // The three ops on an existing destination.
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTDST", "GLOBAL:IWD2RTESTSRC", 2)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RTESTDST")->m_intValue == 0x0FFF);

    pGlobals->FindKey("IWD2RTESTDST")->m_intValue = 0x0F0F;
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTDST", "GLOBAL:IWD2RTESTSRC", 3)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RTESTDST")->m_intValue == 0x0FF0);

    pGlobals->FindKey("IWD2RTESTDST")->m_intValue = 0x0F0F;
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTDST", "GLOBAL:IWD2RTESTSRC", 1)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RTESTDST")->m_intValue == 0x000F);

    // A missing destination is created from 0 and then combined.
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTNEW", "GLOBAL:IWD2RTESTSRC", 2)
        == CGameAIBase::ACTION_DONE);
    CVariable* pNew = pGlobals->FindKey("IWD2RTESTNEW");
    REQUIRE(pNew != NULL);
    CHECK(pNew->m_intValue == 0x00FF);

    // An unknown op fails without writing: the destination keeps its value
    // and a missing destination is not created.
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTDST", "GLOBAL:IWD2RTESTSRC", 7)
        == CGameAIBase::ACTION_ERROR);
    CHECK(pGlobals->FindKey("IWD2RTESTDST")->m_intValue == 0x000F);
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTBAD", "GLOBAL:IWD2RTESTSRC", 7)
        == CGameAIBase::ACTION_ERROR);
    CHECK(pGlobals->FindKey("IWD2RTESTBAD") == NULL);

    // A missing source variable reads as 0.
    pGlobals->FindKey("IWD2RTESTDST")->m_intValue = 0x0F0F;
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTDST", "GLOBAL:IWD2RTESTMISSING", 3)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RTESTDST")->m_intValue == 0x0F0F);
}

TEST_CASE("global bit global: LOCALS reads and writes the sprite's own variables", "[ai][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CVariableHash* pLocals = pSprite->GetLocalVariables();
    REQUIRE(pLocals != NULL);

    REQUIRE(AddVariable(pLocals, "IWD2RTESTLSRC", 0x00F0) != NULL);
    REQUIRE(AddVariable(pLocals, "IWD2RTESTLDST", 0x0FF0) != NULL);

    CHECK(RunAction248(pSprite, "LOCALS:IWD2RTESTLDST", "LOCALS:IWD2RTESTLSRC", 1)
        == CGameAIBase::ACTION_DONE);
    CHECK(pLocals->FindKey("IWD2RTESTLDST")->m_intValue == 0x00F0);

    // A missing destination is created from 0 and then combined.
    CHECK(RunAction248(pSprite, "LOCALS:IWD2RTESTLNEW", "LOCALS:IWD2RTESTLSRC", 2)
        == CGameAIBase::ACTION_DONE);
    CVariable* pNew = pLocals->FindKey("IWD2RTESTLNEW");
    REQUIRE(pNew != NULL);
    CHECK(pNew->m_intValue == 0x00F0);

    // The source and destination scopes are independent.
    REQUIRE(AddVariable(g_pBaldurChitin->GetObjectGame()->GetVariables(), "IWD2RTESTGSRC", 0x0F00) != NULL);
    CHECK(RunAction248(pSprite, "LOCALS:IWD2RTESTLDST", "GLOBAL:IWD2RTESTGSRC", 2)
        == CGameAIBase::ACTION_DONE);
    CHECK(pLocals->FindKey("IWD2RTESTLDST")->m_intValue == 0x0FF0);
}

TEST_CASE("global bit global: a MYAREA destination writes the area and redirects the source lookup", "[ai][data]")
{
    CGameSprite* pSprite = ProbeOrSkip();
    CVariableHash* pAreaVars = pSprite->GetArea()->GetVariables();
    CVariableHash* pGlobals = g_pBaldurChitin->GetObjectGame()->GetVariables();

    // The source scope names no area, but a MYAREA destination redirects the
    // source lookup into the acting area (the original tests the
    // destination scope in the source fallback): the source variable is
    // found in the area and the destination is created there.
    REQUIRE(AddVariable(pAreaVars, "IWD2RTESTASRC", 0x0F0F) != NULL);
    CHECK(RunAction248(pSprite, "MYAREA:IWD2RTESTADST", "ELSEWHERE:IWD2RTESTASRC", 2)
        == CGameAIBase::ACTION_DONE);
    CVariable* pADst = pAreaVars->FindKey("IWD2RTESTADST");
    REQUIRE(pADst != NULL);
    CHECK(pADst->m_intValue == 0x0F0F);

    // Without MYAREA on the destination the same source lookup finds no
    // area and the source reads as 0: XOR leaves the destination as it was.
    REQUIRE(AddVariable(pGlobals, "IWD2RTESTGDST", 0x0F0F) != NULL);
    CHECK(RunAction248(pSprite, "GLOBAL:IWD2RTESTGDST", "ELSEWHERE:IWD2RTESTASRC", 3)
        == CGameAIBase::ACTION_DONE);
    CHECK(pGlobals->FindKey("IWD2RTESTGDST")->m_intValue == 0x0F0F);

    // A destination scope naming no area writes nothing anywhere.
    CHECK(RunAction248(pSprite, "ELSEWHERE:IWD2RTESTNOWHERE", "ELSEWHERE:IWD2RTESTASRC", 2)
        == CGameAIBase::ACTION_DONE);
    CHECK(pAreaVars->FindKey("IWD2RTESTNOWHERE") == NULL);
    CHECK(pGlobals->FindKey("IWD2RTESTNOWHERE") == NULL);
}
