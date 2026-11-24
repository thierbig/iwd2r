// The dialog-set messages' codecs: CMessageSetDialogResRef (subtype 68,
// survey 121 rows 86-87) and CMessageSetDialogWait (subtype 40, rows 88-89).
// Both are receive-only: the resref message is broadcast so the other
// machines point the same sprite at the same dialog, and the wait message is
// sent to the machine that controls the waiting sprite; single player never
// sends or receives them (docs/adr/0002-single-player-only.md).
//
// The fixture is the zeroed CBaldurChitin/CInfGame with the two arrays the
// messages touch, as tests/test_message_chapter_done.cpp builds it; neither
// payload needs game data.
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

TEST_CASE("msg: CMessageSetDialogResRef::MarshalMessage writes the resref", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    CMessageSetDialogResRef message(CResRef("DLG001"), 10, pObject->m_id);

    BYTE packet[HEADER_LENGTH + 0x20];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    // 9 + the six-character resref.
    REQUIRE(dwSize == 9 + 6);
    PLAYER_ID player = 0;
    LONG remoteObject = 0;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    CHECK(player == 7);
    CHECK(remoteObject == 0x1001);
    CHECK(packet[HEADER_LENGTH + 8] == 6);
    CHECK(std::memcmp(packet + HEADER_LENGTH + 9, "DLG001", 6) == 0);
}

TEST_CASE("msg: CMessageSetDialogResRef::MarshalMessage keeps the body for an unknown target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CMessageSetDialogResRef message(CResRef("DLG001"), 10, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + 0x20];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    // NOTE: A failed target share does not zero the size here: the pair is
    // written as (0, 0xFFFFFFFF), as the original does.
    CHECK(dwSize == 9 + 6);
    PLAYER_ID player = 99;
    LONG remoteObject = 99;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    CHECK(player == 0);
    CHECK(remoteObject == -1);
}

TEST_CASE("msg: CMessageSetDialogResRef codec round-trips", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);

    CMessageSetDialogResRef sent(CResRef("DLG001"), 10, pObject->m_id);
    BYTE packet[HEADER_LENGTH + 0x20];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);

    CMessageSetDialogResRef received(CResRef(""), 0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);
    CHECK(received.GetTarget() == pObject->m_id);
    CHECK(received.m_cResRefDialog == CResRef("DLG001"));

    // The vtable getters: BROADCAST_OTHERS and subtype 68.
    CHECK(received.GetCommType() == CMessage::BROADCAST_OTHERS);
    CHECK(received.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_SET_DIALOG_RESREF);
}

TEST_CASE("msg: CMessageSetDialogResRef::UnmarshalMessage keeps an unresolved target as INVALID", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    // Wire: unknown remote pair, then the length-prefixed resref.
    BYTE packet[HEADER_LENGTH + 0x0F];
    std::memset(packet, 0, sizeof(packet));
    PLAYER_ID player = 9;
    LONG remoteObject = 0x1999;
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));
    packet[HEADER_LENGTH + 8] = 6;
    std::memcpy(packet + HEADER_LENGTH + 9, "DLG001", 6);

    CMessageSetDialogResRef message(CResRef(""), 0, 44);
    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
    CHECK(message.GetTarget() == static_cast<LONG>(CGameObjectArray::INVALID_INDEX));
    CHECK(message.m_cResRefDialog == CResRef("DLG001"));
}
