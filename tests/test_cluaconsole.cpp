// The CLUAConsole parse layer (Task 85c): the word encoder 0x5844E0, the
// word-to-id resolver 0x583E60, the argument-count check 0x584420, the
// argument parser 0x5840E0 with its token append 0x5842C0, and the shared
// report's non-printing tail 0x584320; and the dispatcher 0x5838A0 (Task
// 85d) driving that layer end to end. Every arm that prints (unknown
// word, bad parens, unclosed quote, wrong argument count, bad prefix)
// goes through CtrlAltDelete_DisplayText into a live world screen and is
// pinned by the evidence listings instead; the ENABLECHEATKEYS arm is the
// one dispatch effect observable without game data, over the zeroed
// chitin stub from tests/test_ctrl_alt_delete.cpp.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CLUAConsole.h"
#include "CScreenWorld.h"

#include <memory>

TEST_CASE("EncodeCommandWord doubles each byte after +8", "[clua]")
{
    CHECK(EncodeCommandWord("FIRSTAID") == "\x9c\xa2\xb4\xb6\xb8\x92\xa2\x98");
    CHECK(EncodeCommandWord("A") == "\x92");
    CHECK(EncodeCommandWord("") == "");
}

TEST_CASE("CLUAConsole.GetCommandWord returns the raw word", "[clua]")
{
    CLUAConsole c;
    c.field_4 = "SETGLOBAL";
    CHECK(c.GetCommandWord() == "SETGLOBAL");
}

TEST_CASE("CLUAConsole.ResolveCommand maps known words to ids", "[clua]")
{
    CLUAConsole c;
    c.ResolveCommand("SETGLOBAL");
    CHECK(c.field_0 == 0x1D);
    CHECK(c.GetCommandWord() == "SETGLOBAL");

    c.ResolveCommand("CREATEITEM");
    CHECK(c.field_0 == 0x15);

    c.ResolveCommand("JEFFKDEFENDS");
    CHECK(c.field_0 == 0x27);
}

TEST_CASE("CLUAConsole.CountArgs checks the per-command argument shape", "[clua]")
{
    CLUAConsole c;

    c.field_0 = 0x1D; // SETGLOBAL: two strings plus exactly one value
    c.field_8 = 1;
    c.field_C = 2;
    CHECK(c.CountArgs() == 3);
    c.field_8 = 2; // wrong value count
    CHECK(c.CountArgs() == 0);

    c.field_0 = 0x10; // ADDGOLD: exactly one value
    c.field_8 = 1;
    c.field_C = 0;
    CHECK(c.CountArgs() == 1);

    c.field_0 = 0x15; // CREATEITEM: one string plus any number of values
    c.field_8 = 3;
    c.field_C = 1;
    CHECK(c.CountArgs() == 4);

    c.field_0 = 0xC; // FIRSTAID: no arguments
    CHECK(c.CountArgs() == 0);

    c.field_0 = 0x12; // CHECKVAR: two strings
    c.field_8 = 0;
    c.field_C = 2;
    CHECK(c.CountArgs() == 2);

    c.field_0 = 0x1A; // MOVETOAREA: one string
    c.field_C = 1;
    CHECK(c.CountArgs() == 1);

    c.field_0 = 0x25; // SETCHAPTER: any number of values
    c.field_8 = 5;
    CHECK(c.CountArgs() == 5);
}

TEST_CASE("CLUAConsole.ParseArgs files numeric tokens into the value table", "[clua]")
{
    CLUAConsole c;
    c.field_0 = 0x1D;
    c.field_8 = 0;
    c.field_C = 0;
    c.ParseArgs("5,)");
    CHECK(c.field_8 == 1);
    REQUIRE(c.field_10.GetSize() == 1);
    CHECK(c.field_10[0] == 5);
    CHECK(c.field_C == 0);
}

TEST_CASE("CLUAConsole.ParseArgs files quoted tokens into the token store", "[clua]")
{
    CLUAConsole c;
    c.field_0 = 0x1D;
    c.field_8 = 0;
    c.field_C = 0;
    c.ParseArgs("\"name\",)");
    CHECK(c.field_C == 1);
    REQUIRE(c.field_24.GetSize() == 1);
    CHECK(c.field_24[0] == "name");
    CHECK(c.field_8 == 0);
}

TEST_CASE("CLUAConsole.ParseArgs splits at each comma or paren (FindOneOf 0x584178)", "[clua]")
{
    CLUAConsole c;
    c.field_0 = 0x1D;
    c.field_8 = 0;
    c.field_C = 0;
    c.ParseArgs("5, 6)");
    CHECK(c.field_8 == 2);
    REQUIRE(c.field_10.GetSize() == 2);
    CHECK(c.field_10[0] == 5);
    CHECK(c.field_10[1] == 6);

    CLUAConsole m;
    m.field_0 = 0x1D;
    m.field_8 = 0;
    m.field_C = 0;
    m.ParseArgs("\"potn08\", -3)");
    CHECK(m.field_C == 1);
    REQUIRE(m.field_24.GetSize() == 1);
    CHECK(m.field_24[0] == "potn08");
    CHECK(m.field_8 == 1);
    REQUIRE(m.field_10.GetSize() == 1);
    CHECK(static_cast<int>(m.field_10[0]) == -3);
}

TEST_CASE("CLUAConsole.ParseArgs stops on the bare paren tail", "[clua]")
{
    CLUAConsole c;
    c.field_8 = 0;
    c.field_C = 0;
    c.ParseArgs(")");
    CHECK(c.field_8 == 0);
    CHECK(c.field_C == 0);
}

TEST_CASE("CLUAConsole.AddTextArg appends and counts one token", "[clua]")
{
    CLUAConsole c;
    c.field_C = 0;
    c.AddTextArg("x");
    CHECK(c.field_C == 1);
    REQUIRE(c.field_24.GetSize() == 1);
    CHECK(c.field_24[0] == "x");
}

TEST_CASE("CLUAConsole.CommandNotFound leaves the id unknown without printing", "[clua]")
{
    CLUAConsole c;
    c.field_0 = 5; // not a report id: the default arm prints nothing
    c.CommandNotFound();
    CHECK(c.field_0 == 0xFFFFFFFF);
}

TEST_CASE("CLUAConsole.Execute returns silently on the empty line", "[clua]")
{
    CLUAConsole c;
    c.field_0 = 0x1234;
    c.Execute("");
    CHECK(c.field_0 == 0x1234); // untouched: nothing ran
}

TEST_CASE("CLUAConsole.Execute drives ENABLECHEATKEYS lines end to end", "[clua]")
{
    CBaldurChitin* previousBaldur = g_pBaldurChitin;

    // Zeroed CBaldurChitin with a zeroed CScreenWorld behind m_pEngineWorld
    // (what GetScreenWorld returns), as in tests/test_ctrl_alt_delete.cpp:
    // the dispatched handler is a plain store into that screen. Counts
    // accumulate across Execute calls on one console, so each line gets a
    // fresh one.
    std::unique_ptr<unsigned char[]> baldur(new unsigned char[sizeof(CBaldurChitin)]());
    std::unique_ptr<unsigned char[]> screen(new unsigned char[sizeof(CScreenWorld)]());
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(baldur.get());
    g_pBaldurChitin->m_pEngineWorld = reinterpret_cast<CScreenWorld*>(screen.get());
    CScreenWorld* pScreen = g_pBaldurChitin->GetScreenWorld();

    // Lowercase prefix and word exercise both MakeUpper passes. One value
    // passes through the count check and the table value is stored.
    pScreen->m_bCheatKeysEnabled = 1;
    CLUAConsole c0;
    c0.Execute("ctrlaltdelete:enablecheatkeys(0)");
    CHECK(pScreen->m_bCheatKeysEnabled == 0);
    CHECK(c0.field_0 == 0x18);

    CLUAConsole c1;
    c1.Execute("ctrlaltdelete:enablecheatkeys(1)");
    CHECK(pScreen->m_bCheatKeysEnabled == 1);

    // No argument: the zero-count arm forces the keys on.
    pScreen->m_bCheatKeysEnabled = 0;
    CLUAConsole cNone;
    cNone.Execute("ctrlaltdelete:enablecheatkeys()");
    CHECK(pScreen->m_bCheatKeysEnabled == 1);

    // Two values: CountArgs' shape table maps any count but one to zero,
    // so the zero-count arm fires here too.
    pScreen->m_bCheatKeysEnabled = 0;
    CLUAConsole cTwo;
    cTwo.Execute("ctrlaltdelete:enablecheatkeys(1,2)");
    CHECK(pScreen->m_bCheatKeysEnabled == 1);

    g_pBaldurChitin = previousBaldur;
}
