// The dialog/store mode messages' codecs: CMessageEnterDialogMode (subtype
// 22, survey 121 rows 74-75) and CMessageEnterStoreMode (subtype 23, rows
// 76-77). Both are receive-only: the class is broadcast so that the other
// machines enter the same dialog/store screen; single player never sends or
// receives them (docs/adr/0002-single-player-only.md).
//
// The fixture is the zeroed CBaldurChitin/CInfGame with the two arrays the
// messages touch, as tests/test_message_chapter_done.cpp builds it. The
// store message's payload carries two CAIObjectType blocks, whose fields are
// plain data, so no game data is needed either.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

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

// CGameRemoteObjectArray::Add leaves an empty bucket null (its else branch
// stores NULL), so insert the entry ourselves into the array's public bucket.
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

} // namespace

TEST_CASE("msg: CMessageEnterDialogMode::MarshalMessage writes the gold", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    CMessageEnterDialogMode message(12345, 10, pObject->m_id);

    BYTE packet[HEADER_LENGTH + 0xC];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    REQUIRE(dwSize == 0xC);
    PLAYER_ID player = 0;
    LONG remoteObject = 0;
    DWORD gold = 0;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    std::memcpy(&gold, packet + HEADER_LENGTH + 8, sizeof(gold));
    CHECK(player == 7);
    CHECK(remoteObject == 0x1001);
    CHECK(gold == 12345);
}

TEST_CASE("msg: CMessageEnterDialogMode::MarshalMessage zeroes the size for an unknown target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CMessageEnterDialogMode message(12345, 10, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + 0xC];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0x10;
    message.MarshalMessage(&pPacket, &dwSize);

    CHECK(dwSize == 0);
    for (BYTE nByte : packet) {
        CHECK(nByte == 0xCD);
    }
}

TEST_CASE("msg: CMessageEnterDialogMode codec round-trips", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);

    CMessageEnterDialogMode sent(12345, 10, pObject->m_id);
    BYTE packet[HEADER_LENGTH + 0xC];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);

    CMessageEnterDialogMode received(0, 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(received.GetTarget() == pObject->m_id);
    CHECK(received.m_nPartyGold == 12345);
}

TEST_CASE("msg: CMessageEnterDialogMode::UnmarshalMessage keeps an unresolved target as INVALID", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    // Wire: unknown remote pair, then the party gold.
    BYTE packet[HEADER_LENGTH + 0xC];
    std::memset(packet, 0, sizeof(packet));
    PLAYER_ID player = 9;
    LONG remoteObject = 0x1999;
    DWORD gold = 777;
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));
    std::memcpy(packet + HEADER_LENGTH + 8, &gold, sizeof(gold));

    CMessageEnterDialogMode message(0, 0, 44);
    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
    CHECK(message.GetTarget() == static_cast<LONG>(CGameObjectArray::INVALID_INDEX));
    CHECK(message.m_nPartyGold == 777);

    // The vtable getters: BROADCAST_FORCED_OTHERS and subtype 22.
    CHECK(message.GetCommType() == CMessage::BROADCAST_FORCED_OTHERS);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_ENTER_DIALOG_MODE);
}

// The store message's two CAIObjectType blocks: SHORT name length and name,
// five bytes (enemy/ally, general, race, subrace, class), four bytes
// (specific, gender, alignment, avClass), the class mask, the block's
// instance id as a remote pair (or (0, 0xFFFFFFFF)) and the five special-case
// bytes; 0x1A fixed bytes after the name.
namespace {
constexpr DWORD STORE_BLOCK_BASE = 0x1A;

void SetTypeFields(CAIObjectType& type, BYTE first, const char* name)
{
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
    type.m_sName = name;
    for (INT index = 0; index < 5; index++) {
        type.m_SpecialCase[index] = static_cast<BYTE>(first + 10 + index);
    }
}
} // namespace

TEST_CASE("msg: CMessageEnterStoreMode::MarshalMessage writes the two object blocks", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    CGameObject* pInstance = AddObject(pGame, 8, 0x1002);

    CAIObjectType proprietor;
    SetTypeFields(proprietor, 2, "PROP");
    proprietor.m_nInstance = pInstance->m_id;
    CAIObjectType customer;
    SetTypeFields(customer, 21, "CUST");
    customer.m_nInstance = CGameObjectArray::INVALID_INDEX;

    CMessageEnterStoreMode message(proprietor, customer, CResRef("STOR01"), 10, pObject->m_id);

    BYTE packet[HEADER_LENGTH + 0x80];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    // (6 + 9) + two blocks of (2 + 4 + 0x1A).
    REQUIRE(dwSize == 6 + 9 + 2 * (2 + 4 + STORE_BLOCK_BASE));
    PLAYER_ID player = 0;
    LONG remoteObject = 0;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    CHECK(player == 7);
    CHECK(remoteObject == 0x1001);
    CHECK(packet[HEADER_LENGTH + 8] == 6);
    CHECK(std::memcmp(packet + HEADER_LENGTH + 9, "STOR01", 6) == 0);

    const DWORD first = HEADER_LENGTH + 6 + 9;
    SHORT nameLength = 0;
    std::memcpy(&nameLength, packet + first, sizeof(nameLength));
    CHECK(nameLength == 4);
    CHECK(std::memcmp(packet + first + 2, "PROP", 4) == 0);
    const DWORD firstFields = first + 2 + 4;
    CHECK(packet[firstFields] == 2);
    CHECK(packet[firstFields + 1] == 3);
    CHECK(packet[firstFields + 2] == 4);
    CHECK(packet[firstFields + 3] == 5);
    CHECK(packet[firstFields + 4] == 6);
    CHECK(packet[firstFields + 5] == 7);
    CHECK(packet[firstFields + 6] == 8);
    CHECK(packet[firstFields + 7] == 9);
    CHECK(packet[firstFields + 8] == 10);
    DWORD classMask = 0;
    std::memcpy(&classMask, packet + firstFields + 9, sizeof(classMask));
    CHECK(classMask == 0x11223344);
    PLAYER_ID instancePlayer = 0;
    LONG instanceRemote = 0;
    std::memcpy(&instancePlayer, packet + firstFields + 0xD, sizeof(instancePlayer));
    std::memcpy(&instanceRemote, packet + firstFields + 0x11, sizeof(instanceRemote));
    CHECK(instancePlayer == 8);
    CHECK(instanceRemote == 0x1002);
    CHECK(packet[firstFields + 0x15] == 12);

    const DWORD second = first + 2 + 4 + STORE_BLOCK_BASE;
    std::memcpy(&nameLength, packet + second, sizeof(nameLength));
    CHECK(nameLength == 4);
    CHECK(std::memcmp(packet + second + 2, "CUST", 4) == 0);
    std::memcpy(&instancePlayer, packet + second + 2 + 4 + 0xD, sizeof(instancePlayer));
    std::memcpy(&instanceRemote, packet + second + 2 + 4 + 0x11, sizeof(instanceRemote));
    CHECK(instancePlayer == 0);
    CHECK(instanceRemote == -1);
}

TEST_CASE("msg: CMessageEnterStoreMode::MarshalMessage keeps the full body for an unknown target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CAIObjectType proprietor;
    SetTypeFields(proprietor, 2, "PROP");
    proprietor.m_nInstance = CGameObjectArray::INVALID_INDEX;
    CAIObjectType customer;
    SetTypeFields(customer, 21, "CUST");
    customer.m_nInstance = CGameObjectArray::INVALID_INDEX;

    CMessageEnterStoreMode message(proprietor, customer, CResRef("STOR01"), 10, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + 0x80];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    // NOTE: Unlike the other marshals, a failed target share does not zero
    // the size: the pair is written as (0, 0xFFFFFFFF).
    CHECK(dwSize == 6 + 9 + 2 * (2 + 4 + STORE_BLOCK_BASE));
    PLAYER_ID player = 123;
    LONG remoteObject = 456;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    CHECK(player == 0);
    CHECK(remoteObject == -1);
}

TEST_CASE("msg: CMessageEnterStoreMode codec round-trips", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    CGameObject* pProprietor = AddObject(pGame, 8, 0x1002);
    CGameObject* pCustomer = AddObject(pGame, 9, 0x1003);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);
    AddRemote(pGame, 8, 0x1002, pProprietor->m_id);
    AddRemote(pGame, 9, 0x1003, pCustomer->m_id);

    CAIObjectType proprietor;
    SetTypeFields(proprietor, 2, "PROP");
    proprietor.m_nInstance = pProprietor->m_id;
    CAIObjectType customer;
    SetTypeFields(customer, 21, "CUST");
    customer.m_nInstance = pCustomer->m_id;

    CMessageEnterStoreMode sent(proprietor, customer, CResRef("STOR01"), 10, pObject->m_id);
    BYTE packet[HEADER_LENGTH + 0x80];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);

    CMessageEnterStoreMode received(proprietor, customer, CResRef(""), 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(received.GetTarget() == pObject->m_id);
    CHECK(received.m_cResStore == CResRef("STOR01"));

    const CAIObjectType& gotProprietor = received.m_cAIProprietor;
    CHECK(gotProprietor.m_sName == "PROP");
    CHECK(gotProprietor.m_nEnemyAlly == proprietor.m_nEnemyAlly);
    CHECK(gotProprietor.m_nGeneral == proprietor.m_nGeneral);
    CHECK(gotProprietor.m_nRace == proprietor.m_nRace);
    CHECK(gotProprietor.m_nSubRace == proprietor.m_nSubRace);
    CHECK(gotProprietor.m_nClass == proprietor.m_nClass);
    CHECK(gotProprietor.m_nSpecific == proprietor.m_nSpecific);
    CHECK(gotProprietor.m_nGender == proprietor.m_nGender);
    CHECK(gotProprietor.m_nAlignment == proprietor.m_nAlignment);
    CHECK(gotProprietor.m_nAvClass == proprietor.m_nAvClass);
    CHECK(gotProprietor.m_nClassMask == proprietor.m_nClassMask);
    CHECK(gotProprietor.m_nInstance == pProprietor->m_id);
    for (INT index = 0; index < 5; index++) {
        CHECK(gotProprietor.m_SpecialCase[index] == proprietor.m_SpecialCase[index]);
    }

    const CAIObjectType& gotCustomer = received.m_cAICustomer;
    CHECK(gotCustomer.m_sName == "CUST");
    CHECK(gotCustomer.m_nEnemyAlly == customer.m_nEnemyAlly);
    CHECK(gotCustomer.m_nInstance == pCustomer->m_id);
    for (INT index = 0; index < 5; index++) {
        CHECK(gotCustomer.m_SpecialCase[index] == customer.m_SpecialCase[index]);
    }
}

TEST_CASE("msg: CMessageEnterStoreMode::UnmarshalMessage rejects an unknown instance pair", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);

    CAIObjectType proprietor;
    SetTypeFields(proprietor, 2, "PROP");
    proprietor.m_nInstance = CGameObjectArray::INVALID_INDEX;
    CAIObjectType customer;
    SetTypeFields(customer, 21, "CUST");
    customer.m_nInstance = CGameObjectArray::INVALID_INDEX;

    CMessageEnterStoreMode sent(proprietor, customer, CResRef("STOR01"), 10, pObject->m_id);
    BYTE packet[HEADER_LENGTH + 0x80];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);

    // Overwrite the first block's instance pair with an unresolved one: the
    // decoder rejects the whole message (a resolved pair or (0, 0xFFFFFFFF)
    // are the only accepted values).
    const DWORD first = HEADER_LENGTH + 6 + 9;
    const DWORD instancePair = first + 2 + 4 + 0xD;
    PLAYER_ID player = 9;
    LONG remoteObject = 0x1999;
    std::memcpy(packet + instancePair, &player, sizeof(player));
    std::memcpy(packet + instancePair + 4, &remoteObject, sizeof(remoteObject));

    CMessageEnterStoreMode received(proprietor, customer, CResRef(""), 0, 0);
    CHECK(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == FALSE);
}
