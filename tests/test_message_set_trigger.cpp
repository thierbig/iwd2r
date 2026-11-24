// The CMessageSetTrigger codec (0x509C40/0x50A090, survey 121 rows
// 100-101): the receive side of the trigger message a controlled sprite
// gets. The class, its constructor, getters and Run are already ported; this
// adds the Marshal/Unmarshal pair. The payload carries one CAITrigger: its
// id and specifics, its cause as a CAIObjectType block, three fixed LONGs
// and two length-prefixed strings.
//
// The fixture is the zeroed CBaldurChitin/CInfGame with the two arrays the
// message touches, as tests/test_message_chapter_done.cpp builds it.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIObjectType.h"
#include "CAITrigger.h"
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

void SetCause(CAIObjectType& cause, const char* name, BYTE first, LONG instance)
{
    cause.m_sName = name;
    cause.m_nEnemyAlly = first;
    cause.m_nGeneral = static_cast<BYTE>(first + 1);
    cause.m_nRace = static_cast<BYTE>(first + 2);
    cause.m_nSubRace = static_cast<BYTE>(first + 3);
    cause.m_nClass = static_cast<BYTE>(first + 4);
    cause.m_nSpecific = static_cast<BYTE>(first + 5);
    cause.m_nGender = static_cast<BYTE>(first + 6);
    cause.m_nAlignment = static_cast<BYTE>(first + 7);
    cause.m_nAvClass = static_cast<BYTE>(first + 8);
    cause.m_nClassMask = 0x11223344;
    cause.m_nInstance = instance;
    for (INT index = 0; index < 5; index++) {
        cause.m_SpecialCase[index] = static_cast<BYTE>(first + 10 + index);
    }
}

CAITrigger MakeTrigger(LONG causeInstance)
{
    CAIObjectType cause;
    SetCause(cause, "TRIG1", 2, causeInstance);
    CAITrigger trigger(907, cause, -5);
    trigger.m_flags = 0x1234;
    trigger.m_specific2 = 77;
    trigger.m_specific3 = -88;
    trigger.m_string1 = "ONE";
    trigger.m_string2 = "TWO";
    return trigger;
}

void CheckCause(const CAIObjectType& got, const CAIObjectType& want)
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

} // namespace

TEST_CASE("msg: CMessageSetTrigger::MarshalMessage writes the trigger", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pTarget = AddObject(pGame, 7, 0x1001);
    CGameObject* pCause = AddObject(pGame, 8, 0x1002);

    CAITrigger trigger = MakeTrigger(pCause->m_id);
    CMessageSetTrigger message(trigger, 10, pTarget->m_id);

    BYTE packet[HEADER_LENGTH + 0x100];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    // 0xE + (5 + 2) + 0x26 + (3 + 2) + (3 + 2): the cause name is TRIG1 and
    // the two strings are ONE and TWO.
    REQUIRE(dwSize == 0xE + 7 + 0x26 + 5 + 5);
    PLAYER_ID player = 0;
    LONG remoteObject = 0;
    SHORT triggerID = 0;
    LONG specificID = 0;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    std::memcpy(&triggerID, packet + HEADER_LENGTH + 8, sizeof(triggerID));
    std::memcpy(&specificID, packet + HEADER_LENGTH + 0xA, sizeof(specificID));
    CHECK(player == 7);
    CHECK(remoteObject == 0x1001);
    CHECK(triggerID == 907);
    CHECK(specificID == -5);
    SHORT nameLength = 0;
    std::memcpy(&nameLength, packet + HEADER_LENGTH + 0xE, sizeof(nameLength));
    CHECK(nameLength == 5);
    CHECK(std::memcmp(packet + HEADER_LENGTH + 0x10, "TRIG1", 5) == 0);

    // The cause block: five selectors at +0x10+len, four at +0x15+len, the
    // class mask at +0x19+len, the instance pair at +0x1D+len and the
    // special-case bytes at +0x25+len.
    const DWORD block = HEADER_LENGTH + 0x10 + 5;
    CHECK(packet[block] == 2);
    CHECK(packet[block + 1] == 3);
    CHECK(packet[block + 2] == 4);
    CHECK(packet[block + 3] == 5);
    CHECK(packet[block + 4] == 6);
    CHECK(packet[block + 5] == 7);
    CHECK(packet[block + 6] == 8);
    CHECK(packet[block + 7] == 9);
    CHECK(packet[block + 8] == 10);
    DWORD classMask = 0;
    std::memcpy(&classMask, packet + block + 9, sizeof(classMask));
    CHECK(classMask == 0x11223344);
    PLAYER_ID instancePlayer = 0;
    LONG instanceRemote = 0;
    std::memcpy(&instancePlayer, packet + block + 0xD, sizeof(instancePlayer));
    std::memcpy(&instanceRemote, packet + block + 0x11, sizeof(instanceRemote));
    CHECK(instancePlayer == 8);
    CHECK(instanceRemote == 0x1002);
    CHECK(packet[block + 0x15] == 12);
}

TEST_CASE("msg: CMessageSetTrigger::MarshalMessage zeroes the size for an unknown target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CAITrigger trigger = MakeTrigger(CGameObjectArray::INVALID_INDEX);
    CMessageSetTrigger message(trigger, 10, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + 0x100];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0x10;
    message.MarshalMessage(&pPacket, &dwSize);

    CHECK(dwSize == 0);
    for (BYTE nByte : packet) {
        CHECK(nByte == 0xCD);
    }
}

TEST_CASE("msg: CMessageSetTrigger codec round-trips the CAITrigger", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pTarget = AddObject(pGame, 7, 0x1001);
    CGameObject* pCause = AddObject(pGame, 8, 0x1002);
    AddRemote(pGame, 7, 0x1001, pTarget->m_id);
    AddRemote(pGame, 8, 0x1002, pCause->m_id);

    CAITrigger trigger = MakeTrigger(pCause->m_id);
    CMessageSetTrigger sent(trigger, 10, pTarget->m_id);

    BYTE packet[HEADER_LENGTH + 0x100];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);

    CMessageSetTrigger received(trigger, 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(received.GetTarget() == pTarget->m_id);
    CHECK(received.m_trigger.m_triggerID == trigger.m_triggerID);
    CHECK(received.m_trigger.m_specificID == trigger.m_specificID);
    CheckCause(received.m_trigger.m_triggerCause, trigger.m_triggerCause);
    CHECK(received.m_trigger.m_flags == trigger.m_flags);
    CHECK(received.m_trigger.m_specific2 == trigger.m_specific2);
    CHECK(received.m_trigger.m_specific3 == trigger.m_specific3);
    CHECK(received.m_trigger.m_string1 == trigger.m_string1);
    CHECK(received.m_trigger.m_string2 == trigger.m_string2);
}

TEST_CASE("msg: CMessageSetTrigger::UnmarshalMessage rejects unresolved pairs", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pTarget = AddObject(pGame, 7, 0x1001);
    CGameObject* pCause = AddObject(pGame, 8, 0x1002);
    AddRemote(pGame, 7, 0x1001, pTarget->m_id);
    AddRemote(pGame, 8, 0x1002, pCause->m_id);

    CAITrigger trigger = MakeTrigger(pCause->m_id);
    CMessageSetTrigger sent(trigger, 10, pTarget->m_id);
    BYTE packet[HEADER_LENGTH + 0x100];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);

    SECTION("an unknown target pair rejects the message")
    {
        PLAYER_ID player = 9;
        LONG remoteObject = 0x1999;
        std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
        std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));

        CMessageSetTrigger received(trigger, 0, 44);
        CHECK(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == FALSE);
        CHECK(received.GetTarget() == 44);
    }

    SECTION("an unknown cause-instance pair rejects the message")
    {
        // The cause block starts at payload +0x10 with the five-byte name
        // length prefix: its instance pair is at payload +0x1D+nameLength.
        const DWORD causeBlock = HEADER_LENGTH + 0x10 + 5;
        PLAYER_ID player = 9;
        LONG remoteObject = 0x1999;
        std::memcpy(packet + causeBlock + 0xD, &player, sizeof(player));
        std::memcpy(packet + causeBlock + 0x11, &remoteObject, sizeof(remoteObject));

        CMessageSetTrigger received(trigger, 0, pTarget->m_id);
        CHECK(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == FALSE);
    }

    SECTION("an absent cause-instance pair decodes as INVALID_INDEX")
    {
        const DWORD causeBlock = HEADER_LENGTH + 0x10 + 5;
        PLAYER_ID player = 0;
        LONG remoteObject = -1;
        std::memcpy(packet + causeBlock + 0xD, &player, sizeof(player));
        std::memcpy(packet + causeBlock + 0x11, &remoteObject, sizeof(remoteObject));

        CMessageSetTrigger received(trigger, 0, pTarget->m_id);
        CHECK(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
        CHECK(received.m_trigger.m_triggerCause.m_nInstance
            == static_cast<LONG>(CGameObjectArray::INVALID_INDEX));

        // The vtable getters: SEND and subtype 52.
        CHECK(received.GetCommType() == CMessage::SEND);
        CHECK(received.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_SET_TRIGGER);
    }
}
