// CBaldurMessage::OnDemandCharacterUpdate (0x43DC40), SendCharacterUpdate
// (0x43DD20) and OnCharacterUpdate (0x43DE50): the host's answer to the
// PLAYERCHAR/70 poll and its reverse. OnDemandCharacterUpdate maps the wire
// pair {PLAYER_ID remotePlayer; LONG remoteObject} at pData+3 (the engine's
// 3-byte spec header, 0x85E670) through the remote-object array and answers
// with SendCharacterUpdate; that one denies the local sprite, marshals its
// timed effect list at version 1 and sends PLAYERCHAR/102 with
// {pair; effects}. OnCharacterUpdate finds the same object and unmarshals the
// tail into its effect list (0xB = the 3-byte header plus the pair).
//
// The fixture is tests/test_message_link_95.cpp's zeroed CBaldurChitin /
// CInfGame with only the object and remote-object arrays constructed. The
// sprite is a zeroed CGameSprite block whose timed effect list is
// placement-constructed: the functions only touch the list plus
// CGameObject's m_remotePlayerID/m_remoteObjectID, never a sprite virtual.
// The session is tests/test_network.cpp's provider setup, so a send fills
// CNetwork::STATIC_MESSAGE_BUFFER, which is what the tests read.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameEffect.h"
#include "CGameEffectList.h"
#include "CGameObject.h"
#include "CGameRemoteObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>

extern CChitin* g_pChitin;

namespace {

// CNetwork::SPEC_MSG_HEADER_LENGTH is a runtime constant; mirror its
// verified value (src/CNetwork.cpp) so the packet arrays have a real bound.
constexpr DWORD HEADER_LENGTH = 3;
constexpr DWORD PAIR_SIZE = sizeof(DWORD) + sizeof(LONG);
constexpr BYTE POISON = 0xCD;

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
    }

    ~GameScope()
    {
        // The fake sprites belong to the test, not the array: the array's
        // destructor deletes every placed object through CGameObject's
        // virtual destructor, and a zeroed block has no vtable. Hiding the
        // placed entries keeps that cleanup off them; FakeSprite owns them.
        Game()->m_cObjectArray.m_nMaxArrayIndex = -1;
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

// A zeroed CGameSprite block with only its timed effect list constructed.
// The three functions read m_remotePlayerID/m_remoteObjectID and the list, so
// the block needs no sprite constructor (which needs a CRE and the engine).
class FakeSprite {
public:
    FakeSprite()
    {
        m_block.reset(new unsigned char[sizeof(CGameSprite)]());
        m_sprite = reinterpret_cast<CGameSprite*>(m_block.get());
        new (&m_sprite->m_timedEffectList) CGameEffectList();
    }

    ~FakeSprite()
    {
        m_sprite->m_timedEffectList.~CGameEffectList();
    }

    FakeSprite(const FakeSprite&) = delete;
    FakeSprite& operator=(const FakeSprite&) = delete;

    CGameSprite* Sprite() const { return m_sprite; }

private:
    std::unique_ptr<unsigned char[]> m_block;
    CGameSprite* m_sprite;
};

// Places a fake sprite at a chosen object-array index. GetDeny only checks
// m_nMaxArrayIndex, m_nNextObjectId and the entry's m_objectId, so no
// CGameObjectArray::Add (which dispatches CGameObject::SetAIType through the
// zeroed vtable) is needed.
constexpr LONG OBJECT_ID = 0x00010001;

LONG PutObject(CInfGame* pGame, CGameObject* pObject)
{
    CGameObjectArray* pArray = pGame->GetObjectArray();
    const SHORT arrayIndex = static_cast<SHORT>((OBJECT_ID >> 16) & 0x7FFF);
    pArray->m_pArray[arrayIndex].m_objectId = static_cast<SHORT>(OBJECT_ID);
    pArray->m_pArray[arrayIndex].m_objectPtr = pObject;
    pArray->m_nMaxArrayIndex = arrayIndex;
    pArray->m_nNextObjectId = static_cast<SHORT>(OBJECT_ID) + 1;
    return OBJECT_ID;
}

BYTE DenyCount(CInfGame* pGame, LONG nObject)
{
    const SHORT arrayIndex = static_cast<SHORT>((nObject >> 16) & 0x7FFF);
    return pGame->m_cObjectArray.m_pArray[arrayIndex].m_denyCounts[CGameObjectArray::THREAD_ASYNCH];
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

// tests/test_network.cpp's connected-provider setup: sends land in
// CNetwork::STATIC_MESSAGE_BUFFER, and player location 0 is the remote the
// character update answers.
class SessionScope {
public:
    SessionScope()
        : m_network(g_pChitin->cNetwork)
    {
        m_network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
        m_network.m_bConnectionEstablished = TRUE;
        m_network.m_bIsHost = TRUE;
        m_network.m_idLocalPlayer = 1;
        m_network.m_psPlayerName[0] = "remote";
        m_network.m_pPlayerID[0] = 2;
        m_network.m_pbPlayerVisible[0] = TRUE;
        std::memset(CNetwork::STATIC_MESSAGE_BUFFER, POISON, STATIC_MESSAGE_SIZE);
    }

    ~SessionScope()
    {
        m_network.m_bConnectionEstablished = FALSE;
        m_network.m_bIsHost = FALSE;
        m_network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    }

    SessionScope(const SessionScope&) = delete;
    SessionScope& operator=(const SessionScope&) = delete;

    CNetwork& Network() { return m_network; }

private:
    CNetwork& m_network;
};

void WritePair(BYTE* at, DWORD remotePlayerID, LONG remoteObjectID)
{
    std::memcpy(at, &remotePlayerID, sizeof(remotePlayerID));
    std::memcpy(at + sizeof(remotePlayerID), &remoteObjectID, sizeof(remoteObjectID));
}

DWORD ReadPairPlayer(const BYTE* at)
{
    DWORD value;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

LONG ReadPairObject(const BYTE* at)
{
    LONG value;
    std::memcpy(&value, at + sizeof(DWORD), sizeof(value));
    return value;
}

// The three-byte spec header SendSpecificMessage writes into the static
// buffer for a PLAYERCHAR/102 answer.
void CheckCharacterUpdateHeader(const BYTE* buffer)
{
    CHECK(buffer[0] == CNetwork::SPEC_MSG_FLAG_ENABLED);
    CHECK(buffer[1] == CBaldurMessage::MSG_TYPE_PLAYERCHAR);
    CHECK(buffer[2] == CBaldurMessage::MSG_SUBTYPE_PLAYERCHAR_102);
}

} // namespace

TEST_CASE("character update: OnDemandCharacterUpdate answers a PLAYERCHAR/70 poll", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session;
    FakeSprite sprite;

    sprite.Sprite()->m_remotePlayerID = 3;
    sprite.Sprite()->m_remoteObjectID = 4;
    const LONG nObject = PutObject(game.Game(), reinterpret_cast<CGameObject*>(sprite.Sprite()));
    AddRemote(game.Game(), 3, 4, nObject);

    BYTE packet[HEADER_LENGTH + PAIR_SIZE] = {};
    WritePair(packet + HEADER_LENGTH, 3, 4);

    CBaldurMessage message;

    // A closed session refuses before the remote-object lookup.
    session.Network().m_bConnectionEstablished = FALSE;
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, POISON, STATIC_MESSAGE_SIZE);
    CHECK(message.OnDemandCharacterUpdate(0, packet, sizeof(packet)) == FALSE);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[HEADER_LENGTH] == POISON);
    session.Network().m_bConnectionEstablished = TRUE;

    // An unknown {player; object} pair is refused too.
    BYTE unknown[HEADER_LENGTH + PAIR_SIZE] = {};
    WritePair(unknown + HEADER_LENGTH, 9, 9);
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, POISON, STATIC_MESSAGE_SIZE);
    CHECK(message.OnDemandCharacterUpdate(0, unknown, sizeof(unknown)) == FALSE);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[HEADER_LENGTH] == POISON);

    // A known pair is answered with PLAYERCHAR/102 carrying the same pair
    // and the (empty) effect list.
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, POISON, STATIC_MESSAGE_SIZE);
    CHECK(message.OnDemandCharacterUpdate(0, packet, sizeof(packet)) == TRUE);
    CheckCharacterUpdateHeader(CNetwork::STATIC_MESSAGE_BUFFER);
    CHECK(ReadPairPlayer(CNetwork::STATIC_MESSAGE_BUFFER + HEADER_LENGTH) == 3);
    CHECK(ReadPairObject(CNetwork::STATIC_MESSAGE_BUFFER + HEADER_LENGTH) == 4);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[HEADER_LENGTH + PAIR_SIZE] == POISON);
    CHECK(DenyCount(game.Game(), nObject) == 0);
}

TEST_CASE("character update: SendCharacterUpdate marshals the effect list into PLAYERCHAR/102", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session;
    FakeSprite sprite;

    sprite.Sprite()->m_remotePlayerID = 3;
    sprite.Sprite()->m_remoteObjectID = 4;
    const LONG nObject = PutObject(game.Game(), reinterpret_cast<CGameObject*>(sprite.Sprite()));

    CBaldurMessage message;

    // No object at the id: the guard fails without touching the buffer.
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, POISON, STATIC_MESSAGE_SIZE);
    CHECK(message.SendCharacterUpdate("remote", 0x00020002) == FALSE);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[0] == POISON);

    // An empty effect list sends only the pair, and every deny is released.
    std::memset(CNetwork::STATIC_MESSAGE_BUFFER, POISON, STATIC_MESSAGE_SIZE);
    CHECK(message.SendCharacterUpdate("remote", nObject) == TRUE);
    CheckCharacterUpdateHeader(CNetwork::STATIC_MESSAGE_BUFFER);
    CHECK(ReadPairPlayer(CNetwork::STATIC_MESSAGE_BUFFER + HEADER_LENGTH) == 3);
    CHECK(ReadPairObject(CNetwork::STATIC_MESSAGE_BUFFER + HEADER_LENGTH) == 4);
    CHECK(CNetwork::STATIC_MESSAGE_BUFFER[HEADER_LENGTH + PAIR_SIZE] == POISON);
    CHECK(DenyCount(game.Game(), nObject) == 0);
}

TEST_CASE("character update: OnCharacterUpdate unmarshals PLAYERCHAR/102 into the effect list", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session;
    FakeSprite sender;
    FakeSprite receiver;

    // The effect the sender's list carries: the base record Marshal writes
    // at version 1 is a CGameEffectBase copy, which is what the receiver's
    // Unmarshal decodes back into a CGameEffect.
    CGameEffectBase base;
    base.m_effectID = CGAMEEFFECT_AC;
    base.m_targetType = 1;
    base.m_spellLevel = 2;
    base.m_effectAmount = 37;
    base.m_durationType = 9;
    base.m_duration = 5;
    base.m_probabilityUpper = 100;
    base.m_savingThrow = 1;
    base.m_saveMod = -2;
    base.m_special = 3;
    base.m_flags = 0x10;

    ITEM_EFFECT item;
    std::memset(&item, 0, sizeof(item));
    item.effectID = CGAMEEFFECT_AC;
    CGameEffect* pSenderEffect = CGameEffect::DecodeEffect(&item, CPoint(-1, -1), -1, CPoint(-1, -1));
    pSenderEffect->CopyFromBase(&base);
    sender.Sprite()->m_timedEffectList.AddTail(pSenderEffect);

    BYTE* pEffects = NULL;
    const DWORD dwEffectsSize = sender.Sprite()->m_timedEffectList.Marshal(&pEffects, 1, FALSE);
    REQUIRE(dwEffectsSize == sizeof(CGameEffectBase));

    sender.Sprite()->m_remotePlayerID = 3;
    sender.Sprite()->m_remoteObjectID = 4;
    const LONG nReceiver = PutObject(game.Game(), reinterpret_cast<CGameObject*>(receiver.Sprite()));
    AddRemote(game.Game(), 3, 4, nReceiver);

    BYTE packet[HEADER_LENGTH + PAIR_SIZE + sizeof(CGameEffectBase)] = {};
    WritePair(packet + HEADER_LENGTH, 3, 4);
    std::memcpy(packet + HEADER_LENGTH + PAIR_SIZE, pEffects, dwEffectsSize);
    delete[] reinterpret_cast<CGameEffectBase*>(pEffects);

    CBaldurMessage message;

    // A closed session refuses before the lookup.
    session.Network().m_bConnectionEstablished = FALSE;
    CHECK(message.OnCharacterUpdate(0, packet, sizeof(packet)) == FALSE);
    session.Network().m_bConnectionEstablished = TRUE;

    // An unknown pair is refused.
    BYTE unknown[HEADER_LENGTH + PAIR_SIZE] = {};
    WritePair(unknown + HEADER_LENGTH, 9, 9);
    CHECK(message.OnCharacterUpdate(0, unknown, sizeof(unknown)) == FALSE);

    // A known pair decodes the tail into the receiver's effect list.
    REQUIRE(message.OnCharacterUpdate(0, packet, sizeof(packet)) == TRUE);
    REQUIRE(receiver.Sprite()->m_timedEffectList.GetCount() == 1);
    CGameEffect* pReceiverEffect = receiver.Sprite()->m_timedEffectList.GetHead();
    CHECK(pReceiverEffect->m_effectID == static_cast<WORD>(base.m_effectID));
    CHECK(pReceiverEffect->m_targetType == base.m_targetType);
    CHECK(pReceiverEffect->m_spellLevel == base.m_spellLevel);
    CHECK(pReceiverEffect->m_effectAmount == base.m_effectAmount);
    CHECK(pReceiverEffect->m_duration == base.m_duration);
    CHECK(pReceiverEffect->m_savingThrow == base.m_savingThrow);
    CHECK(pReceiverEffect->m_saveMod == base.m_saveMod);
    CHECK(pReceiverEffect->m_special == base.m_special);
    CHECK(DenyCount(game.Game(), nReceiver) == 0);

    // An empty tail unmarshals nothing.
    BYTE empty[HEADER_LENGTH + PAIR_SIZE] = {};
    WritePair(empty + HEADER_LENGTH, 3, 4);
    CHECK(message.OnCharacterUpdate(0, empty, sizeof(empty)) == TRUE);
    CHECK(receiver.Sprite()->m_timedEffectList.GetCount() == 1);
}
