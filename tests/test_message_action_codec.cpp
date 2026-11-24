// The action-payload messages' codec: CMessageInsertAction (subtype 27,
// survey 121 rows 81-82) shares the CAIAction wire body with the already
// ported CMessageAddAction (upstream #131). The insert codec is pinned here
// against the add one byte for byte, and both unmarshals are checked to
// rebuild the same action.
//
// NOTE: the exe's CMessageInsertAction::MarshalMessage size computation adds
// one 0x1A-byte object block too many (0x26 where the body writes 0xC + 2 +
// 2 + 8), so its closing UTIL_ASSERT would fire; the port reports the count
// it writes, which makes the body equal to CMessageAddAction's. The message
// is only marshalled from the multiplayer Send branch
// (docs/adr/0002-single-player-only.md).
//
// The fixture is the zeroed CBaldurChitin/CInfGame with the two arrays the
// messages touch, as tests/test_message_chapter_done.cpp builds it.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAIObjectType.h"
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

void SetObjectType(CAIObjectType& type, const char* name, BYTE first, LONG instance)
{
    type.m_sName = name;
    type.m_nEnemyAlly = first;
    type.m_nGeneral = static_cast<BYTE>(first + 1);
    type.m_nRace = static_cast<BYTE>(first + 2);
    type.m_nSubRace = static_cast<BYTE>(first + 3);
    type.m_nClass = static_cast<BYTE>(first + 4);
    type.m_nSpecific = static_cast<BYTE>(first + 5);
    type.m_nGender = static_cast<BYTE>(first + 6);
    type.m_nAlignment = static_cast<BYTE>(first + 7);
    type.m_nAvClass = static_cast<BYTE>(first + 8);
    type.m_nClassMask = 0x11223344;
    type.m_nInstance = instance;
    for (INT index = 0; index < 5; index++) {
        type.m_SpecialCase[index] = static_cast<BYTE>(first + 10 + index);
    }
}

// One action with every wire field distinct, its three object types pointing
// at three objects of the fixture game.
CAIAction MakeAction(LONG actorId, LONG acteeId, LONG actee2Id)
{
    CAIAction action;
    action.m_actionID = 3;
    SetObjectType(action.m_actorID, "ACTOR", 2, actorId);
    SetObjectType(action.m_acteeID, "ACTEE", 21, acteeId);
    SetObjectType(action.m_acteeID2, "ACTEE2", 40, actee2Id);
    action.m_specificID = -11;
    action.m_specificID2 = 22;
    action.m_specificID3 = 33;
    action.SetString1("FIRST");
    action.SetString2("SECOND");
    action.m_dest = CPoint(400, 500);
    return action;
}

void CheckObjectType(const CAIObjectType& got, const CAIObjectType& want)
{
    CHECK(got.m_sName == want.m_sName);
    CHECK(got.m_nEnemyAlly == want.m_nEnemyAlly);
    CHECK(got.m_nGeneral == want.m_nGeneral);
    CHECK(got.m_nRace == want.m_nRace);
    CHECK(got.m_nSubRace == want.m_nSubRace);
    CHECK(got.m_nClass == want.m_nClass);
    CHECK(got.m_nSpecific == want.m_nSpecific);
    CHECK(got.m_nGender == want.m_nGender);
    CHECK(got.m_nAlignment == want.m_nAlignment);
    CHECK(got.m_nAvClass == want.m_nAvClass);
    CHECK(got.m_nClassMask == want.m_nClassMask);
    CHECK(got.m_nInstance == want.m_nInstance);
    for (INT index = 0; index < 5; index++) {
        CHECK(got.m_SpecialCase[index] == want.m_SpecialCase[index]);
    }
}

void CheckAction(const CAIAction& got, const CAIAction& want)
{
    CHECK(got.m_actionID == want.m_actionID);
    CheckObjectType(got.m_actorID, want.m_actorID);
    CheckObjectType(got.m_acteeID, want.m_acteeID);
    CheckObjectType(got.m_acteeID2, want.m_acteeID2);
    CHECK(got.m_specificID == want.m_specificID);
    CHECK(got.m_specificID2 == want.m_specificID2);
    CHECK(got.m_specificID3 == want.m_specificID3);
    CHECK(got.GetString1() == want.GetString1());
    CHECK(got.GetString2() == want.GetString2());
    CHECK(got.m_dest.x == want.m_dest.x);
    CHECK(got.m_dest.y == want.m_dest.y);
}

} // namespace

TEST_CASE("msg: CMessageInsertAction marshals the same body as CMessageAddAction", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pTarget = AddObject(pGame, 7, 0x1001);
    CGameObject* pActor = AddObject(pGame, 8, 0x1002);
    CGameObject* pActee = AddObject(pGame, 9, 0x1003);
    CGameObject* pActee2 = AddObject(pGame, 10, 0x1004);

    CAIAction action = MakeAction(pActor->m_id, pActee->m_id, pActee2->m_id);

    CMessageAddAction add(action, 10, pTarget->m_id);
    CMessageInsertAction insert(action, 10, pTarget->m_id);

    BYTE addPacket[HEADER_LENGTH + 0x200];
    BYTE insertPacket[HEADER_LENGTH + 0x200];
    std::memset(addPacket, 0xCD, sizeof(addPacket));
    std::memset(insertPacket, 0xCD, sizeof(insertPacket));
    BYTE* pAdd = addPacket + HEADER_LENGTH;
    BYTE* pInsert = insertPacket + HEADER_LENGTH;
    DWORD addSize = 0;
    DWORD insertSize = 0;
    add.MarshalMessage(&pAdd, &addSize);
    insert.MarshalMessage(&pInsert, &insertSize);

    REQUIRE(addSize > 0);
    REQUIRE(insertSize == addSize);
    CHECK(std::memcmp(addPacket + HEADER_LENGTH, insertPacket + HEADER_LENGTH, insertSize) == 0);

    // The insert message is SEND, subtype 27.
    CHECK(insert.GetCommType() == CMessage::SEND);
    CHECK(insert.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_INSERT_ACTION);
}

TEST_CASE("msg: CMessageInsertAction codec round-trips the CAIAction", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pTarget = AddObject(pGame, 7, 0x1001);
    CGameObject* pActor = AddObject(pGame, 8, 0x1002);
    CGameObject* pActee = AddObject(pGame, 9, 0x1003);
    CGameObject* pActee2 = AddObject(pGame, 10, 0x1004);
    AddRemote(pGame, 7, 0x1001, pTarget->m_id);
    AddRemote(pGame, 8, 0x1002, pActor->m_id);
    AddRemote(pGame, 9, 0x1003, pActee->m_id);
    AddRemote(pGame, 10, 0x1004, pActee2->m_id);

    CAIAction action = MakeAction(pActor->m_id, pActee->m_id, pActee2->m_id);
    CMessageInsertAction sent(action, 10, pTarget->m_id);

    BYTE packet[HEADER_LENGTH + 0x200];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);
    REQUIRE(dwSize > 0);

    CAIAction empty;
    CMessageInsertAction received(empty, 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(received.GetTarget() == pTarget->m_id);
    CheckAction(received.m_action, action);

    // The shared codec: the add message decodes the very same bytes into the
    // same action.
    CMessageAddAction addReceived(empty, 0, 0);
    REQUIRE(addReceived.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(addReceived.GetTarget() == pTarget->m_id);
    CheckAction(addReceived.m_action, action);
}

TEST_CASE("msg: CMessageInsertAction rejects an unknown target pair", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CAIAction action;
    CMessageInsertAction message(action, 10, 44);

    BYTE packet[HEADER_LENGTH + 0x10];
    std::memset(packet, 0, sizeof(packet));
    PLAYER_ID player = 9;
    LONG remoteObject = 0x1999;
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));

    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == FALSE);
    CHECK(message.GetTarget() == 44);
}

TEST_CASE("msg: CMessageInsertAction::MarshalMessage zeroes the size for an unknown target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CAIAction action;
    CMessageInsertAction message(action, 10, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + 0x10];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0x10;
    message.MarshalMessage(&pPacket, &dwSize);

    CHECK(dwSize == 0);
    for (BYTE nByte : packet) {
        CHECK(nByte == 0xCD);
    }
}
