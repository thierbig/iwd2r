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
