// CMessageSetPath::UnmarshalMessage (0x508320, survey 121 row 94): the
// receive side of the path message a controlled sprite follows. The class,
// its constructor, getters and Run are already ported; the class's
// MarshalMessage (0x508120) is not in this row set, so the case builds the
// wire body from the decode below.
//
// The fixture is the zeroed CBaldurChitin/CInfGame with the two arrays the
// message touches, as tests/test_message_chapter_done.cpp builds it.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameObject.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"

#include <cstring>
#include <memory>
#include <vector>

extern CChitin* g_pChitin;

namespace {

constexpr DWORD HEADER_LENGTH = 3;

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

CGameObject* AddObject(CInfGame* pGame, PLAYER_ID remotePlayerID, LONG remoteObjectID)
{
    CGameObject* pObject = new CGameObject();

    LONG id;
    REQUIRE(pGame->m_cObjectArray.Add(&id, pObject, INFINITE) == CGameObjectArray::SUCCESS);

    pObject->m_id = id;
    pObject->m_remotePlayerID = remotePlayerID;
    pObject->m_remoteObjectID = remoteObjectID;
    return pObject;
}

void AddRemote(CInfGame* pGame, PLAYER_ID remotePlayerID, LONG remoteObjectID, LONG localObjectID)
{
    const SHORT arrayIndex = static_cast<SHORT>((remoteObjectID >> 16) & 0x7FFF);
    CGameRemoteObjectListEntry* pEntry = new CGameRemoteObjectListEntry();
    pEntry->remotePlayerID = remotePlayerID;
    pEntry->remoteObjectID = remoteObjectID;
    pEntry->localObjectID = localObjectID;
    pEntry->pNext = pGame->m_cRemoteObjectArray.m_pArray[arrayIndex];
    pGame->m_cRemoteObjectArray.m_pArray[arrayIndex] = pEntry;
}

// The body the decode at 0x508320 expects: remote pair, length-prefixed area
// name, position, path count, current path index, current destination, then
// the path points.
std::vector<BYTE> MakePacket(PLAYER_ID player, LONG remoteObjectID, const std::string& area,
    LONG position, SHORT nPath, SHORT currPath, CPoint currDest, const std::vector<LONG>& path)
{
    std::vector<BYTE> packet(HEADER_LENGTH + 8 + 1 + area.size() + 4 + 2 + 2 + 8 + 4 * path.size());
    std::memset(packet.data(), 0, packet.size());
    BYTE* p = packet.data();
    std::memcpy(p + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(p + HEADER_LENGTH + 4, &remoteObjectID, sizeof(remoteObjectID));
    p[HEADER_LENGTH + 8] = static_cast<BYTE>(area.size());
    std::memcpy(p + HEADER_LENGTH + 9, area.data(), area.size());
    DWORD at = HEADER_LENGTH + 9 + static_cast<DWORD>(area.size());
    std::memcpy(p + at, &position, sizeof(position));
    at += 4;
    std::memcpy(p + at, &nPath, sizeof(nPath));
    at += 2;
    std::memcpy(p + at, &currPath, sizeof(currPath));
    at += 2;
    std::memcpy(p + at, &currDest.x, sizeof(LONG));
    at += 4;
    std::memcpy(p + at, &currDest.y, sizeof(LONG));
    at += 4;
    for (LONG point : path) {
        std::memcpy(p + at, &point, sizeof(point));
        at += 4;
    }
    return packet;
}

} // namespace

TEST_CASE("msg: CMessageSetPath::UnmarshalMessage decodes the path", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);

    std::vector<BYTE> packet = MakePacket(7, 0x1001, "AR1000", 123, 3, 1, CPoint(10, 20), { 5, 6, 7 });

    CMessageSetPath message(0, NULL, 0, 0, CPoint(0, 0), 0, 0);
    REQUIRE(message.UnmarshalMessage(packet.data(), static_cast<DWORD>(packet.size())) == TRUE);
    CHECK(message.GetTarget() == pObject->m_id);
    CHECK(message.m_sAreaString == "AR1000");
    CHECK(message.m_position == 123);
    CHECK(message.m_nPath == 3);
    CHECK(message.m_currPath == 1);
    CHECK(message.m_currDest.x == 10);
    CHECK(message.m_currDest.y == 20);
    REQUIRE(message.m_pPath != NULL);
    CHECK(message.m_pPath[0] == 5);
    CHECK(message.m_pPath[1] == 6);
    CHECK(message.m_pPath[2] == 7);
}

TEST_CASE("msg: CMessageSetPath::UnmarshalMessage handles an empty path", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);

    std::vector<BYTE> packet = MakePacket(7, 0x1001, "AR1000", 0, 0, 0, CPoint(-1, -1), {});

    CMessageSetPath message(0, NULL, 0, 0, CPoint(0, 0), 0, 0);
    CHECK(message.UnmarshalMessage(packet.data(), static_cast<DWORD>(packet.size())) == TRUE);
    CHECK(message.m_nPath == 0);
    CHECK(message.m_pPath == NULL);
    CHECK(message.m_currDest.x == -1);
    CHECK(message.m_currDest.y == -1);
}

TEST_CASE("msg: CMessageSetPath::UnmarshalMessage rejects an unresolved target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    std::vector<BYTE> packet = MakePacket(9, 0x1999, "AR1000", 0, 0, 0, CPoint(0, 0), {});

    CMessageSetPath message(0, NULL, 0, 0, CPoint(0, 0), 0, 44);
    CHECK(message.UnmarshalMessage(packet.data(), static_cast<DWORD>(packet.size())) == FALSE);
    CHECK(message.GetTarget() == 44);
    CHECK(message.m_nPath == 0);

    // The vtable getters: BROADCAST and subtype 49.
    CHECK(message.GetCommType() == CMessage::BROADCAST);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_SET_PATH);
}
