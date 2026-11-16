#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "CScreenWorld.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

namespace {

class TestChitin : public CChitin {
    // The base threads never run in this test.
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }
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

    TestChitin& chitin() { return m_chitin; }

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

// Zeroed engine objects the handler reads through g_pBaldurChitin. Only POD
// members are touched, so no constructor runs for the buffers; they leak on
// purpose, like the fixture in test_rules.cpp.
class BaldurScope {
public:
    BaldurScope()
        : m_previous(g_pBaldurChitin)
    {
        m_chitin = static_cast<CBaldurChitin*>(::operator new(sizeof(CBaldurChitin)));
        std::memset(static_cast<void*>(m_chitin), 0, sizeof(CBaldurChitin));
        m_game = static_cast<CInfGame*>(::operator new(sizeof(CInfGame)));
        std::memset(static_cast<void*>(m_game), 0, sizeof(CInfGame));
        m_world = static_cast<CScreenWorld*>(::operator new(sizeof(CScreenWorld)));
        std::memset(static_cast<void*>(m_world), 0, sizeof(CScreenWorld));

        m_chitin->m_pObjectGame = m_game;
        m_chitin->m_pEngineWorld = m_world;
        g_pBaldurChitin = m_chitin;
    }

    ~BaldurScope() { g_pBaldurChitin = m_previous; }

    BaldurScope(const BaldurScope&) = delete;
    BaldurScope& operator=(const BaldurScope&) = delete;

    CInfGame* game() { return m_game; }
    CScreenWorld* world() { return m_world; }

private:
    CBaldurChitin* m_previous;
    CBaldurChitin* m_chitin;
    CInfGame* m_game;
    CScreenWorld* m_world;
};

// A specific-message body whose only payload is the facing dword at the
// 3-byte specific-message header offset.
std::vector<BYTE> FacingMessage(DWORD facing)
{
    std::vector<BYTE> body(CNetwork::SPEC_MSG_HEADER_LENGTH + sizeof(DWORD), 0);
    DWORD* at = reinterpret_cast<DWORD*>(body.data() + CNetwork::SPEC_MSG_HEADER_LENGTH);
    *at = facing;
    return body;
}

} // namespace

TEST_CASE("map world start request is refused without a hosted session", "[message]")
{
    ChitinScope chitinScope;
    BaldurScope baldurScope;
    CBaldurMessage message;

    CHECK(message.OnMapWorldStartRequest(2, FacingMessage(0xBEEF).data(), 7) == FALSE);
    CHECK(baldurScope.world()->m_bPendingMapWorld == FALSE);
}

TEST_CASE("map world start request is refused when not hosting", "[message]")
{
    ChitinScope chitinScope;
    BaldurScope baldurScope;
    CBaldurMessage message;

    chitinScope.chitin().cNetwork.m_bConnectionEstablished = TRUE;

    CHECK(message.OnMapWorldStartRequest(2, FacingMessage(0xBEEF).data(), 7) == FALSE);
    CHECK(baldurScope.world()->m_bPendingMapWorld == FALSE);

    // ~CNetwork closes a live session when this flag is set; the test never
    // opens one, so clear it before the scopes unwind.
    chitinScope.chitin().cNetwork.m_bConnectionEstablished = FALSE;
}

TEST_CASE("map world start request lands pending when the host is not permitted",
    "[message]")
{
    ChitinScope chitinScope;
    BaldurScope baldurScope;
    CBaldurMessage message;

    chitinScope.chitin().cNetwork.m_bConnectionEstablished = TRUE;
    chitinScope.chitin().cNetwork.m_bIsHost = TRUE;
    chitinScope.chitin().cNetwork.m_pbPlayerVisible[2] = TRUE;
    chitinScope.chitin().cNetwork.m_pPlayerID[2] = 0x1234;
    baldurScope.game()->GetMultiplayerSettings()->m_bHostPermittedDialog = TRUE;

    CHECK(message.OnMapWorldStartRequest(2, FacingMessage(0xBEEF).data(), 7) == TRUE);

    CScreenWorld* world = baldurScope.world();
    CHECK(world->m_bPendingMapWorld == TRUE);
    CHECK(world->m_idPendingMapWorldController == 0x1234);
    // 0xBEEF truncated to the facing word: 0xBEEF - 0x10000 = -16657.
    CHECK(world->m_nPendingMapWorldDirection == static_cast<SHORT>(0xBEEF));

    // ~CNetwork closes a live session when this flag is set; the test never
    // opens one, so clear it before the scopes unwind.
    chitinScope.chitin().cNetwork.m_bConnectionEstablished = FALSE;
    chitinScope.chitin().cNetwork.m_bIsHost = FALSE;
}

TEST_CASE("map world start request lands pending while hard paused", "[message]")
{
    ChitinScope chitinScope;
    BaldurScope baldurScope;
    CBaldurMessage message;

    chitinScope.chitin().cNetwork.m_bConnectionEstablished = TRUE;
    chitinScope.chitin().cNetwork.m_bIsHost = TRUE;
    chitinScope.chitin().cNetwork.m_pbPlayerVisible[2] = TRUE;
    chitinScope.chitin().cNetwork.m_pPlayerID[2] = 0x5678;
    baldurScope.world()->m_bHardPaused = TRUE;

    CHECK(message.OnMapWorldStartRequest(2, FacingMessage(1).data(), 7) == TRUE);

    CScreenWorld* world = baldurScope.world();
    CHECK(world->m_bPendingMapWorld == TRUE);
    CHECK(world->m_idPendingMapWorldController == 0x5678);
    CHECK(world->m_nPendingMapWorldDirection == 1);

    // ~CNetwork closes a live session when this flag is set; the test never
    // opens one, so clear it before the scopes unwind.
    chitinScope.chitin().cNetwork.m_bConnectionEstablished = FALSE;
    chitinScope.chitin().cNetwork.m_bIsHost = FALSE;
}
