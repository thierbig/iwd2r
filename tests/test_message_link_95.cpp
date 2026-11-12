// CMessage95 (vtable 0x84D37C, subtype 95): the message
// IcewindCGameEffectDominate (opcode 263) queues at 0x56485D.
//
// The identity tests pin the two-id layout the inline constructor writes
// (+0x0C = first link, +0x10 = second link, +0x04/+0x08 = target/source).
// The codec tests pin the 16-byte wire order (two PLAYER_ID/LONG pairs) and
// the asymmetric unknown-id handling: an unknown first id becomes
// INVALID_INDEX and the message is accepted, an unknown second id is only
// accepted as the (0, INVALID_INDEX) placeholder. The Run tests pin both
// GetDeny guards, both object-type guards, the Icewind586B70 registry call
// and that every acquired deny is released.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameObject.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "Icewind586B70.h"

#include <cstddef>
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
// touches constructed, as tests/test_network.cpp's ObjectControlSuggestion
// test does.
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
        // Keep object ids non-zero: Icewind586B70 treats a zero id as a free
        // table entry, and the first id Add would hand out is 0.
        Game()->m_cObjectArray.m_nNextObjectId = 0x10;
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

LONG AddObject(CInfGame* pGame, BYTE type, PLAYER_ID remotePlayerID, LONG remoteObjectID)
{
    CGameObject* pObject = new CGameObject();
    pObject->m_objectType = type;

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

BYTE DenyCount(CInfGame* pGame, LONG id)
{
    const SHORT arrayIndex = static_cast<SHORT>((id >> 16) & 0x7FFF);
    return pGame->m_cObjectArray.m_pArray[arrayIndex].m_denyCounts[CGameObjectArray::THREAD_ASYNCH];
}

void PackPacket(BYTE* packet, PLAYER_ID player1, LONG object1, PLAYER_ID player2, LONG object2)
{
    std::memset(packet, 0, HEADER_LENGTH + 16);
    std::memcpy(packet + HEADER_LENGTH, &player1, sizeof(player1));
    std::memcpy(packet + HEADER_LENGTH + 4, &object1, sizeof(object1));
    std::memcpy(packet + HEADER_LENGTH + 8, &player2, sizeof(player2));
    std::memcpy(packet + HEADER_LENGTH + 12, &object2, sizeof(object2));
}

} // namespace

TEST_CASE("msg: CMessage95 identity and field layout (vtable 0x84D37C)", "[msg][network]")
{
    // The original allocates 0x14 bytes (vptr + target + source + two ids).
    // The host is 64-bit, so only the two-id tail is fixed here.
    static_assert(sizeof(CMessage95) == sizeof(CMessage) + 2 * sizeof(LONG),
        "the only members the original adds are the two link ids");

    CMessage95 message(11, 22, 33, 44);

    const BYTE* base = reinterpret_cast<const BYTE*>(&message);
    CHECK(reinterpret_cast<const BYTE*>(&message.m_link1Id) - base
        == static_cast<std::ptrdiff_t>(sizeof(CMessage)));
    CHECK(reinterpret_cast<const BYTE*>(&message.m_link2Id) - base
        == static_cast<std::ptrdiff_t>(sizeof(CMessage) + sizeof(LONG)));

    // 0x4F6B80: arg1 -> +0xC, arg2 -> +0x10, arg3 -> +0x8 (source),
    // arg4 -> +0x4 (target). 0x56485D writes the same four dwords inline:
    // target and source are the caller's id, the links are the two ids.
    CHECK(message.m_link1Id == 11);
    CHECK(message.m_link2Id == 22);
    CHECK(message.GetSource() == 33);
    CHECK(message.GetTarget() == 44);

    CMessage95 effectCopy(9, 5, 9, 9);
    CHECK(effectCopy.GetTarget() == 9);
    CHECK(effectCopy.GetSource() == 9);
    CHECK(effectCopy.m_link1Id == 9);
    CHECK(effectCopy.m_link2Id == 5);

    // The vtable 0x84D37C getters: 0x453510 (2 = BROADCAST), 0x40A0E0
    // (67 = MSG_TYPE_CMESSAGE) and 0x4F6BB0 (95).
    CHECK(message.GetCommType() == CMessage::BROADCAST);
    CHECK(message.GetMsgType() == CBaldurMessage::MSG_TYPE_CMESSAGE);
    CHECK(message.GetMsgSubType() == 95);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_95);
}

TEST_CASE("msg: CMessage95::MarshalMessage writes two remote-id pairs or zeroes the size", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    const LONG id1 = AddObject(pGame, CGameObject::TYPE_SPRITE, 7, 0x1001);
    const LONG id2 = AddObject(pGame, CGameObject::TYPE_SPRITE, 8, 0x1002);

    CMessage95 message(id1, id2, id1, id1);

    SECTION("both shares succeed: 16 bytes, first pair then second pair")
    {
        BYTE packet[HEADER_LENGTH + 16];
        std::memset(packet, 0xCD, sizeof(packet));
        BYTE* pPacket = packet;
        DWORD dwSize = 0;

        message.MarshalMessage(&pPacket, &dwSize);

        REQUIRE(dwSize == 16);
        PLAYER_ID player1 = 0;
        LONG object1 = 0;
        PLAYER_ID player2 = 0;
        LONG object2 = 0;
        std::memcpy(&player1, packet, sizeof(player1));
        std::memcpy(&object1, packet + 4, sizeof(object1));
        std::memcpy(&player2, packet + 8, sizeof(player2));
        std::memcpy(&object2, packet + 12, sizeof(object2));
        CHECK(player1 == 7);
        CHECK(object1 == 0x1001);
        CHECK(player2 == 8);
        CHECK(object2 == 0x1002);
        CHECK(packet[HEADER_LENGTH + 15] == 0xCD);
    }

    SECTION("the first share fails: the size is zero and the buffer is untouched")
    {
        message.m_link1Id = CGameObjectArray::INVALID_INDEX;

        BYTE packet[HEADER_LENGTH + 16];
        std::memset(packet, 0xCD, sizeof(packet));
        BYTE* pPacket = packet;
        DWORD dwSize = 16;

        message.MarshalMessage(&pPacket, &dwSize);

        CHECK(dwSize == 0);
        for (BYTE nByte : packet) {
            CHECK(nByte == 0xCD);
        }
    }

    SECTION("the second share fails: the first pair stays, the second is (0, INVALID_INDEX)")
    {
        message.m_link2Id = CGameObjectArray::INVALID_INDEX;

        BYTE packet[HEADER_LENGTH + 16];
        std::memset(packet, 0xCD, sizeof(packet));
        BYTE* pPacket = packet;
        DWORD dwSize = 0;

        message.MarshalMessage(&pPacket, &dwSize);

        REQUIRE(dwSize == 16);
        PLAYER_ID player1 = 0;
        LONG object1 = 0;
        PLAYER_ID player2 = 1;
        LONG object2 = 0x1234;
        std::memcpy(&player1, packet, sizeof(player1));
        std::memcpy(&object1, packet + 4, sizeof(object1));
        std::memcpy(&player2, packet + 8, sizeof(player2));
        std::memcpy(&object2, packet + 12, sizeof(object2));
        CHECK(player1 == 7);
        CHECK(object1 == 0x1001);
        CHECK(player2 == 0);
        CHECK(object2 == CGameObjectArray::INVALID_INDEX);
        CHECK(packet[HEADER_LENGTH + 15] == 0xCD);
    }
}

TEST_CASE("msg: CMessage95::UnmarshalMessage accepts an unknown first id but rejects an unknown second", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    AddRemote(pGame, 7, 0x1001, 0x2001);
    AddRemote(pGame, 8, 0x1002, 0x2002);

    BYTE packet[HEADER_LENGTH + 16];
    CMessage95 message(0xAAAA, 0xBBBB, 0, 0);

    SECTION("both remote ids resolve in wire order")
    {
        PackPacket(packet, 7, 0x1001, 8, 0x1002);
        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
        CHECK(message.m_link1Id == 0x2001);
        CHECK(message.m_link2Id == 0x2002);
    }

    SECTION("an unknown first remote id becomes INVALID_INDEX and does not reject")
    {
        PackPacket(packet, 99, 0x1999, 8, 0x1002);
        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
        CHECK(message.m_link1Id == CGameObjectArray::INVALID_INDEX);
        CHECK(message.m_link2Id == 0x2002);
    }

    SECTION("an unresolved second remote id is accepted only as (0, INVALID_INDEX)")
    {
        PackPacket(packet, 7, 0x1001, 0, CGameObjectArray::INVALID_INDEX);
        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
        CHECK(message.m_link1Id == 0x2001);
        CHECK(message.m_link2Id == CGameObjectArray::INVALID_INDEX);
    }

    SECTION("any other unresolved second remote id returns FALSE")
    {
        PackPacket(packet, 7, 0x1001, 9, 0x1999);
        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == FALSE);
        // The first id was already stored; the second is left alone.
        CHECK(message.m_link1Id == 0x2001);
        CHECK(message.m_link2Id == 0xBBBB);
    }

    SECTION("two unknown ids return FALSE")
    {
        PackPacket(packet, 99, 0x1999, 98, 0x1998);
        CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == FALSE);
        CHECK(message.m_link1Id == CGameObjectArray::INVALID_INDEX);
        CHECK(message.m_link2Id == 0xBBBB);
    }
}

TEST_CASE("msg: CMessage95 codec round-trips through the remote object array", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    const LONG id1 = AddObject(pGame, CGameObject::TYPE_SPRITE, 7, 0x1001);
    const LONG id2 = AddObject(pGame, CGameObject::TYPE_SPRITE, 8, 0x1002);
    AddRemote(pGame, 7, 0x1001, id1);
    AddRemote(pGame, 8, 0x1002, id2);

    CMessage95 sent(id1, id2, id1, id1);
    BYTE packet[HEADER_LENGTH + 16];
    std::memset(packet, 0, sizeof(packet));
    // MarshalMessage writes the body; UnmarshalMessage reads it after the
    // 3-byte network header CNetwork prepends when it sends the packet.
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);
    REQUIRE(dwSize == 16);
    CHECK(dwSize == sizeof(packet) - HEADER_LENGTH);

    CMessage95 received(0, 0, 0, 0);
    CHECK(received.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
    CHECK(received.m_link1Id == id1);
    CHECK(received.m_link2Id == id2);
}

TEST_CASE("msg: CMessage95::Run guards, object types, registry call and deny releases", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();
    CNetwork& network = g_pChitin->cNetwork;

    const LONG id1 = AddObject(pGame, CGameObject::TYPE_SPRITE, 2, 0x1001);
    const LONG id2 = AddObject(pGame, CGameObject::TYPE_SPRITE, 3, 0x1002);

    CGameObject* pObject1 = pGame->m_cObjectArray.m_pArray[(id1 >> 16) & 0x7FFF].m_objectPtr;
    CGameObject* pObject2 = pGame->m_cObjectArray.m_pArray[(id2 >> 16) & 0x7FFF].m_objectPtr;

    // sub_586F20 only records a pair when the first argument (the second
    // link, the controller) is a PC.
    pObject2->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_PC;

    Icewind586B70* pTable = Icewind586B70::Instance();
    REQUIRE(pTable != nullptr);
    pTable->Clear();

    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_idLocalPlayer = 1;

    CMessage95 message(id1, id2, id1, id1);

    SECTION("a success registers the pair and releases both denies")
    {
        message.Run();

        REQUIRE(pTable->GetCount() == 1);
        CHECK(pTable->mEntries[0].field_0 == id1);
        CHECK(pTable->mEntries[0].field_4 == id2);
        CHECK(pTable->mEntries[0].field_8 == true);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("a single player session takes no action but releases the first deny")
    {
        network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("the first object's local owner takes no action but releases the first deny")
    {
        network.m_idLocalPlayer = pObject1->m_remotePlayerID;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("a non-sprite first object takes no action but releases the first deny")
    {
        pObject1->m_objectType = CGameObject::TYPE_DOOR;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("a missing second object releases the first deny and records nothing")
    {
        message.m_link2Id = CGameObjectArray::INVALID_INDEX;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("the second object's local owner releases both denies")
    {
        network.m_idLocalPlayer = pObject2->m_remotePlayerID;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("a non-sprite second object releases both denies")
    {
        pObject2->m_objectType = CGameObject::TYPE_DOOR;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }

    SECTION("a missing first object acquires and releases no deny")
    {
        message.m_link1Id = CGameObjectArray::INVALID_INDEX;

        message.Run();

        CHECK(pTable->GetCount() == 0);
        CHECK(DenyCount(pGame, id1) == 0);
        CHECK(DenyCount(pGame, id2) == 0);
    }
}
