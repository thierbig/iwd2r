// The object-control message family (rows 3-10 of #121) and the
// CGameRemoteObjectArray helpers it drives. The array helpers are tested
// directly here because the receive handlers reach them through
// ChangeControl/MarshalControlChanges, and those paths are what the handlers'
// tests below lean on.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameObject.h"
#include "CGameRemoteObjectArray.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"

#include <cstddef>
#include <cstring>
#include <memory>

extern CChitin* g_pChitin;

namespace {

// CNetwork::SPEC_MSG_HEADER_LENGTH is a runtime constant; mirror its verified
// value (src/CNetwork.cpp) so the packet arrays have a real bound.
constexpr DWORD HEADER_LENGTH = 3;
constexpr DWORD PAIR_SIZE = sizeof(PLAYER_ID) + sizeof(LONG);

class TestChitin : public CChitin {
public:
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

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

// A zeroed CBaldurChitin/CInfGame with only the two arrays the messages touch
// constructed (tests/test_network.cpp's ObjectControlSuggestion fixture).
class GameScope {
public:
    GameScope()
        : m_previous(g_pBaldurChitin)
    {
        m_baldur.reset(new unsigned char[sizeof(CBaldurChitin)]());
        m_game.reset(new unsigned char[sizeof(CInfGame)]());
        g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(m_baldur.get());
        g_pBaldurChitin->m_pObjectGame = reinterpret_cast<CInfGame*>(m_game.get());
        new (&Game()->m_cObjectArray) CGameObjectArray(10, 10);
        new (&Game()->m_cRemoteObjectArray) CGameRemoteObjectArray(10);
    }

    ~GameScope()
    {
        Game()->m_cObjectArray.~CGameObjectArray();
        Game()->m_cRemoteObjectArray.~CGameRemoteObjectArray();
        g_pBaldurChitin = m_previous;
    }

    GameScope(const GameScope&) = delete;
    GameScope& operator=(const GameScope&) = delete;

    CInfGame* Game() const { return g_pBaldurChitin->GetObjectGame(); }

private:
    CBaldurChitin* m_previous;
    std::unique_ptr<unsigned char[]> m_baldur;
    std::unique_ptr<unsigned char[]> m_game;
};

} // namespace

TEST_CASE("remote object array: Add stores an entry in an empty bucket", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    CGameRemoteObjectArray* pRemote = game.Game()->GetRemoteObjectArray();

    const LONG nObject = 0x00010001;

    // Two pairs in different buckets: 4 lands at index 0 and 0x10002 at
    // index 1, both empty buckets whose first entry the NULL store dropped.
    LONG nLocal = 0;
    CHECK(pRemote->Add(3, 4, nObject, FALSE) == TRUE);
    CHECK(pRemote->Find(3, 4, nLocal) == TRUE);
    CHECK(nLocal == nObject);

    CHECK(pRemote->Add(3, 0x00010002, nObject, FALSE) == TRUE);
    CHECK(pRemote->Find(3, 0x00010002, nLocal) == TRUE);
    CHECK(nLocal == nObject);

    // A third pair chains in front of the first at index 0, covering the
    // non-empty branch.
    CHECK(pRemote->Add(3, 5, nObject, FALSE) == TRUE);
    CHECK(pRemote->Find(3, 5, nLocal) == TRUE);
    CHECK(nLocal == nObject);
    CHECK(pRemote->Find(3, 4, nLocal) == TRUE);

    // A duplicate pair is refused.
    CHECK(pRemote->Add(3, 4, nObject, FALSE) == FALSE);
}
