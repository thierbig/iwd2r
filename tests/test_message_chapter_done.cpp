// CMessageChapterDone (vtable 0x84D3B4, subtype 98): the chapter screen's
// Done button pressed on the machines that do not own it.
//
// The identity tests pin the 0xC-byte layout the inline constructor at
// 0x5D4190 writes (vptr + target + source, no added members) and the three
// vtable getters (0x4536E0 comm type 5, 0x40A0E0 message type, 0x4F6CB0
// subtype 98). The codec tests pin the 8-byte wire body (one
// PLAYER_ID/LONG pair for the target sprite's remote ids) and the
// resolve-or-reject rule of UnmarshalMessage. Run only forwards to
// CScreenChapter::OnDoneButtonClick when the chapter screen is the active
// engine; observing that needs a live screen, so Run is not covered here.
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

extern CChitin* g_pChitin;

namespace {

// CNetwork::SPEC_MSG_HEADER_LENGTH is a runtime constant; mirror its verified
// value (src/CNetwork.cpp) so the packet arrays have a real bound.
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

// A zeroed CBaldurChitin/CInfGame with only the two arrays the message
// touches constructed, as tests/test_message_link_95.cpp does.
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

LONG AddObject(CInfGame* pGame, PLAYER_ID remotePlayerID, LONG remoteObjectID)
{
    CGameObject* pObject = new CGameObject();
    pObject->m_objectType = CGameObject::TYPE_SPRITE;

    LONG id;
    REQUIRE(pGame->m_cObjectArray.Add(&id, pObject, INFINITE) == CGameObjectArray::SUCCESS);

    pObject->m_id = id;
    pObject->m_remotePlayerID = remotePlayerID;
    pObject->m_remoteObjectID = remoteObjectID;
    return id;
}

// CGameRemoteObjectArray::Add leaves an empty bucket null (its else branch
// stores NULL), so insert the entry ourselves into the array's public
// bucket. The array's destructor owns and deletes it.
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

void PackPacket(BYTE* packet, PLAYER_ID player, LONG object)
{
    std::memset(packet, 0, HEADER_LENGTH + 8);
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &object, sizeof(object));
}

} // namespace

TEST_CASE("msg: CMessageChapterDone identity and field layout (vtable 0x84D3B4)", "[msg][network]")
{
    // The original allocates 0xC bytes (vptr + target + source); the class
    // adds no member of its own. The host is 64-bit, so only the absence of
    // added members is fixed here.
    static_assert(sizeof(CMessageChapterDone) == sizeof(CMessage),
        "the class adds no members over CMessage");

    // 0x4F6C90: arg1 -> +0x8 (source), arg2 -> +0x4 (target). The
    // constructor site 0x5D4190 passes the same party slot id twice.
    CMessageChapterDone message(33, 44);

    CHECK(message.GetSource() == 33);
    CHECK(message.GetTarget() == 44);

    CMessageChapterDone hostCopy(9, 9);
    CHECK(hostCopy.GetSource() == 9);
    CHECK(hostCopy.GetTarget() == 9);

    // The vtable 0x84D3B4 getters: 0x4536E0 (5 = BROADCAST_FORCED_OTHERS),
    // 0x40A0E0 (67 = MSG_TYPE_CMESSAGE) and 0x4F6CB0 (98).
    CHECK(message.GetCommType() == CMessage::BROADCAST_FORCED_OTHERS);
    CHECK(message.GetCommType() == 5);
    CHECK(message.GetMsgType() == CBaldurMessage::MSG_TYPE_CMESSAGE);
    CHECK(message.GetMsgSubType() == 98);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_CHAPTER_DONE);
}

TEST_CASE("msg: CMessageChapterDone::MarshalMessage writes the target's remote ids or zeroes the size", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    const LONG id = AddObject(pGame, 7, 0x1001);

    CMessageChapterDone message(id, id);

    SECTION("the share succeeds: 8 bytes, the pair in wire order")
    {
        BYTE packet[HEADER_LENGTH + 8];
        std::memset(packet, 0xCD, sizeof(packet));
        BYTE* pPacket = packet;
        DWORD dwSize = 0;

        message.MarshalMessage(&pPacket, &dwSize);

        REQUIRE(dwSize == 8);
        PLAYER_ID player = 0;
        LONG object = 0;
        std::memcpy(&player, packet, sizeof(player));
        std::memcpy(&object, packet + 4, sizeof(object));
        CHECK(player == 7);
        CHECK(object == 0x1001);
    }

    SECTION("the share fails: the size is zero and the buffer is untouched")
    {
        message.m_targetId = CGameObjectArray::INVALID_INDEX;

        BYTE packet[HEADER_LENGTH + 8];
        std::memset(packet, 0xCD, sizeof(packet));
        BYTE* pPacket = packet;
        DWORD dwSize = 16;

        message.MarshalMessage(&pPacket, &dwSize);

        CHECK(dwSize == 0);
        for (BYTE nByte : packet) {
            CHECK(nByte == 0xCD);
        }
    }
}

TEST_CASE("msg: CMessageChapterDone::UnmarshalMessage retargets on a resolved remote id and rejects an unknown one", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    AddRemote(pGame, 7, 0x1001, 0x2001);

    BYTE packet[HEADER_LENGTH + 8];

    SECTION("a resolved remote id retargets the message and keeps the source")
    {
        PackPacket(packet, 7, 0x1001);
        CMessageChapterDone message(33, 44);

        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
        CHECK(message.GetTarget() == 0x2001);
        CHECK(message.GetSource() == 33);
    }

    SECTION("an unknown remote id returns FALSE and leaves the target alone")
    {
        PackPacket(packet, 99, 0x1999);
        CMessageChapterDone message(33, 44);

        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == FALSE);
        CHECK(message.GetTarget() == 44);
        CHECK(message.GetSource() == 33);
    }
}

TEST_CASE("msg: CMessageChapterDone codec round-trips through the remote object array", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    const LONG id = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, id);

    // The host passes the same party slot id twice (0x5D4190).
    CMessageChapterDone sent(id, id);
    BYTE packet[HEADER_LENGTH + 8];
    std::memset(packet, 0, sizeof(packet));
    // MarshalMessage writes the body; UnmarshalMessage reads it after the
    // 3-byte network header CNetwork prepends when it sends the packet.
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);
    REQUIRE(dwSize == 8);
    CHECK(dwSize == sizeof(packet) - HEADER_LENGTH);

    CMessageChapterDone received(0, 0);
    CHECK(received.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
    CHECK(received.GetTarget() == id);
}
