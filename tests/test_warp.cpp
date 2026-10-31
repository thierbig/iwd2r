#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CWarp.h"

// CWarp::TimerUpdate (0x7B5550): the pump over CWarp::lTimers. The timer class
// itself is unrecovered (see CWarpTimer in CWarp.h), so the tests build a
// test-only subclass and a synthetic list: nothing in the game ever adds an
// entry, which is why a unit test is the whole proof.

extern CChitin* g_pChitin;

namespace {

// A failed UTIL_ASSERT, thrown by TestChitin::ShutDown (as tests/test_vid.cpp).
struct FailedAssert {
    int nLine;
};

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }

    void ShutDown(int nLineNumber, const char* szFileName, const char* text) override
    {
        if (nLineNumber != -1) {
            throw FailedAssert { nLineNumber };
        }
        CChitin::ShutDown(nLineNumber, szFileName, text);
    }
};

class ChitinScope {
public:
    ChitinScope()
        : m_previous(g_pChitin)
    {
        g_pChitin = &m_chitin;
    }
    ~ChitinScope() { g_pChitin = m_previous; }

    ChitinScope(const ChitinScope&) = delete;
    ChitinScope& operator=(const ChitinScope&) = delete;

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

// TIMER_UPDATES_PER_SECOND is a mutable static the game reads from the INI;
// the pump divides 10000 by it, so the tests pin it and put it back.
class UpdatesPerSecondScope {
public:
    explicit UpdatesPerSecondScope(DWORD nPerSecond)
        : m_previous(CChitin::TIMER_UPDATES_PER_SECOND)
    {
        CChitin::TIMER_UPDATES_PER_SECOND = nPerSecond;
    }
    ~UpdatesPerSecondScope() { CChitin::TIMER_UPDATES_PER_SECOND = m_previous; }

    UpdatesPerSecondScope(const UpdatesPerSecondScope&) = delete;
    UpdatesPerSecondScope& operator=(const UpdatesPerSecondScope&) = delete;

private:
    DWORD m_previous;
};

class TestTimer : public CWarpTimer {
public:
    int nUpdates = 0;

    void Update() override { nUpdates++; }
};

// The class has no constructor of its own (nothing in the original constructs
// one), so every field the pump reads is set explicitly.
void Init(CWarpTimer& timer, BOOL bEveryTick, LONG nPeriod, LONG nAccumulator)
{
    timer.field_4 = bEveryTick;
    timer.field_8 = 0;
    timer.field_C = 0;
    timer.field_10 = NULL;
    timer.field_14 = NULL;
    timer.field_18 = nPeriod;
    timer.field_1C = nAccumulator;
}

}

TEST_CASE("warp: TimerUpdate fires every-tick timers on every call", "[warp]")
{
    ChitinScope scope;
    CWarp warp;
    TestTimer timer;
    Init(timer, TRUE, 0, 0);
    warp.lTimers.AddTail(&timer);

    warp.TimerUpdate();
    warp.TimerUpdate();

    CHECK(timer.nUpdates == 2);
    // The every-tick branch never touches the accumulator.
    CHECK(timer.field_1C == 0);
    CHECK(warp.lTimers.GetCount() == 1);
}

TEST_CASE("warp: TimerUpdate accumulates 10000 / updates per second and keeps the remainder", "[warp]")
{
    ChitinScope scope;
    UpdatesPerSecondScope perSecond(10); // one update = 10000 / 10 = 1000
    CWarp warp;
    TestTimer timer;
    Init(timer, FALSE, 2500, 0);
    warp.lTimers.AddTail(&timer);

    warp.TimerUpdate(); // 1000 < 2500
    CHECK(timer.nUpdates == 0);
    CHECK(timer.field_1C == 1000);

    warp.TimerUpdate(); // 2000 < 2500
    CHECK(timer.nUpdates == 0);
    CHECK(timer.field_1C == 2000);

    warp.TimerUpdate(); // 3000 >= 2500: fires, keeps 500
    CHECK(timer.nUpdates == 1);
    CHECK(timer.field_1C == 500);

    warp.TimerUpdate(); // 1500 < 2500
    CHECK(timer.nUpdates == 1);

    warp.TimerUpdate(); // 2500 >= 2500: fires, keeps 0
    CHECK(timer.nUpdates == 2);
    CHECK(timer.field_1C == 0);
}

TEST_CASE("warp: TimerUpdate lets a timer remove itself from the list", "[warp]")
{
    ChitinScope scope;
    CWarp warp;
    TestTimer timer;
    Init(timer, TRUE, 0, 0);
    POSITION pos = warp.lTimers.AddTail(&timer);
    // All four fields the removal needs: the flag, the owner, the linked mark
    // and the timer's own POSITION.
    timer.field_8 = 1;
    timer.field_10 = &warp;
    timer.field_C = 1;
    timer.field_14 = pos;

    warp.TimerUpdate();

    CHECK(timer.nUpdates == 1);
    CHECK(timer.field_C == 0);
    CHECK(timer.field_14 == NULL);
    CHECK(warp.lTimers.IsEmpty());
}

TEST_CASE("warp: TimerUpdate also lets a periodic timer remove itself after it fires", "[warp]")
{
    ChitinScope scope;
    UpdatesPerSecondScope perSecond(10);
    CWarp warp;
    TestTimer timer;
    Init(timer, FALSE, 1000, 0);
    POSITION pos = warp.lTimers.AddTail(&timer);
    timer.field_8 = 1;
    timer.field_10 = &warp;
    timer.field_C = 1;
    timer.field_14 = pos;

    warp.TimerUpdate(); // reaches the period: fires and removes itself
    CHECK(timer.nUpdates == 1);
    CHECK(timer.field_1C == 0);
    CHECK(warp.lTimers.IsEmpty());
}

TEST_CASE("warp: TimerUpdate keeps walking when a timer removes itself mid-list", "[warp]")
{
    ChitinScope scope;
    CWarp warp;
    TestTimer first;
    TestTimer second;
    TestTimer third;
    Init(first, TRUE, 0, 0);
    Init(second, TRUE, 0, 0);
    Init(third, TRUE, 0, 0);

    POSITION posFirst = warp.lTimers.AddTail(&first);
    warp.lTimers.AddTail(&second);
    warp.lTimers.AddTail(&third);

    first.field_8 = 1;
    first.field_10 = &warp;
    first.field_C = 1;
    first.field_14 = posFirst;

    warp.TimerUpdate();

    // All three were updated; only the first left.
    CHECK(first.nUpdates == 1);
    CHECK(second.nUpdates == 1);
    CHECK(third.nUpdates == 1);
    CHECK(warp.lTimers.GetCount() == 2);
    CHECK(warp.lTimers.GetHead() == &second);
}

TEST_CASE("warp: TimerUpdate does not remove a timer that lacks one of the four fields", "[warp]")
{
    ChitinScope scope;
    CWarp warp;
    TestTimer timer;
    Init(timer, TRUE, 0, 0);
    POSITION pos = warp.lTimers.AddTail(&timer);
    timer.field_8 = 1;
    timer.field_10 = &warp;
    timer.field_C = 1;
    timer.field_14 = pos;

    // Each of the four has to be non-zero; clear them one at a time.
    timer.field_8 = 0;
    warp.TimerUpdate();
    CHECK(timer.nUpdates == 1);
    CHECK(warp.lTimers.GetCount() == 1);
    timer.field_8 = 1;

    timer.field_10 = NULL;
    warp.TimerUpdate();
    CHECK(warp.lTimers.GetCount() == 1);
    timer.field_10 = &warp;

    timer.field_C = 0;
    warp.TimerUpdate();
    CHECK(warp.lTimers.GetCount() == 1);
    timer.field_C = 1;

    timer.field_14 = NULL;
    warp.TimerUpdate();
    CHECK(warp.lTimers.GetCount() == 1);

    timer.field_14 = pos;
    warp.TimerUpdate();
    CHECK(warp.lTimers.IsEmpty());
}

TEST_CASE("warp: TimerUpdate skips a null entry in the list", "[warp]")
{
    ChitinScope scope;
    CWarp warp;
    TestTimer timer;
    Init(timer, TRUE, 0, 0);
    warp.lTimers.AddTail(static_cast<CObject*>(NULL));
    warp.lTimers.AddTail(&timer);

    warp.TimerUpdate();

    CHECK(timer.nUpdates == 1);
    CHECK(warp.lTimers.GetCount() == 2);
}
