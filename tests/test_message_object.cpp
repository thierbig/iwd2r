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

// Places an object at array index 1. GetShare/GetDeny only check
// m_nMaxArrayIndex, m_nNextObjectId and the entry's m_objectId, so no
// CGameObjectArray::Add (which would touch the remote array and the sprite AI
// type) is needed. The array's destructor owns the object.
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

// CGameRemoteObjectArray::Add leaves an empty bucket null (its else branch
// stores NULL), so insert the entry ourselves into the array's public bucket.
// The array's destructor owns and deletes it.
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

// tests/test_network.cpp's connected-provider setup: player location 0 is the
// session host, so OnObjectControl accepts nMsgFrom 0, and a send lands in
// CNetwork::STATIC_MESSAGE_BUFFER.
class SessionScope {
public:
    explicit SessionScope(BOOLEAN bHost)
        : m_network(g_pChitin->cNetwork)
    {
        m_network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
        m_network.m_bConnectionEstablished = TRUE;
        m_network.m_bIsHost = bHost;
        m_network.m_idLocalPlayer = 1;
        m_network.m_nHostPlayer = 0;
        m_network.m_psPlayerName[0] = "remote";
        m_network.m_pPlayerID[0] = 2;
        m_network.m_pbPlayerVisible[0] = TRUE;
        std::memset(CNetwork::STATIC_MESSAGE_BUFFER, 0xCD, STATIC_MESSAGE_SIZE);
    }

    ~SessionScope()
    {
        m_network.m_bConnectionEstablished = FALSE;
        m_network.m_bIsHost = FALSE;
        m_network.m_nHostPlayer = -1;
        m_network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    }

    SessionScope(const SessionScope&) = delete;
    SessionScope& operator=(const SessionScope&) = delete;

    CNetwork& Network() { return m_network; }

private:
    CNetwork& m_network;
};

void WritePair(BYTE* at, PLAYER_ID remotePlayerID, LONG remoteObjectID)
{
    std::memcpy(at, &remotePlayerID, sizeof(remotePlayerID));
    std::memcpy(at + sizeof(remotePlayerID), &remoteObjectID, sizeof(remoteObjectID));
}

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

TEST_CASE("remote object array: UnmarshalControlChanges starts after the spec header", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session(FALSE);
    CGameRemoteObjectArray* pRemote = game.Game()->GetRemoteObjectArray();

    CGameObject* pObject = new CGameObject();
    pObject->m_remotePlayerID = 3;
    pObject->m_remoteObjectID = 4;
    const LONG nObject = PutObject(game.Game(), pObject);
    AddRemote(game.Game(), 3, 4, nObject);

    // {header[3] poison; SHORT 0 areas changed; SHORT 1 change;
    //  {PLAYER_ID oldPlayer 3; LONG oldObject 4; PLAYER_ID newPlayer 9;
    //   LONG newObject 10; BOOLEAN localControl 0}}
    const DWORD payloadSize = sizeof(SHORT) + sizeof(SHORT)
        + sizeof(PLAYER_ID) + sizeof(LONG) + sizeof(PLAYER_ID) + sizeof(LONG)
        + sizeof(BOOLEAN);
    BYTE packet[HEADER_LENGTH + payloadSize];
    std::memset(packet, 0xCD, sizeof(packet));
    DWORD cnt = HEADER_LENGTH;
    const SHORT nAreas = 0;
    std::memcpy(packet + cnt, &nAreas, sizeof(nAreas));
    cnt += sizeof(nAreas);
    const SHORT nChanges = 1;
    std::memcpy(packet + cnt, &nChanges, sizeof(nChanges));
    cnt += sizeof(nChanges);
    WritePair(packet + cnt, 3, 4);
    cnt += PAIR_SIZE;
    WritePair(packet + cnt, 9, 10);
    cnt += PAIR_SIZE;
    packet[cnt++] = FALSE;
    REQUIRE(cnt == sizeof(packet));

    // Reading from offset 0 would take the 0xCDCD header as the area count
    // and then trip the trailing `dwSize == cnt`; the payload starts at +3.
    CHECK(pRemote->UnmarshalControlChanges(packet, sizeof(packet)) == TRUE);
    LONG nLocal = 0;
    CHECK(pRemote->Find(9, 10, nLocal) == TRUE);
    CHECK(nLocal == nObject);
}

TEST_CASE("object control: OnObjectControlSuggest asks for the mapped object", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session(TRUE);

    // The suggestion carries the object's own remote ids; the local copy
    // mirrors them in m_remotePlayerID/m_remoteObjectID.
    CGameObject* pObject = new CGameObject();
    pObject->m_remotePlayerID = 7;
    pObject->m_remoteObjectID = 8;
    const LONG nObject = PutObject(game.Game(), pObject);
    AddRemote(game.Game(), 7, 8, nObject);

    CBaldurMessage message;

    BYTE packet[HEADER_LENGTH + PAIR_SIZE] = {};
    WritePair(packet + HEADER_LENGTH, 7, 8);

    // A closed session refuses before the lookup.
    session.Network().m_bConnectionEstablished = FALSE;
    CHECK(message.OnObjectControlSuggest(0, packet, sizeof(packet)) == FALSE);
    session.Network().m_bConnectionEstablished = TRUE;

    // An unknown {player; object} pair is refused.
    BYTE unknown[HEADER_LENGTH + PAIR_SIZE] = {};
    WritePair(unknown + HEADER_LENGTH, 9, 9);
    CHECK(message.OnObjectControlSuggest(0, unknown, sizeof(unknown)) == FALSE);

    // A known pair is handed to ObjectControlRequest (0x430980): this host
    // moves the control to itself and queues the change for the next
    // OBJECT/67 broadcast.
    CHECK(message.OnObjectControlSuggest(0, packet, sizeof(packet)) == TRUE);
    CGameRemoteObjectArray* pRemote = game.Game()->GetRemoteObjectArray();
    LONG nLocal = 0;
    CHECK(pRemote->Find(1, nObject, nLocal) == TRUE);
    CHECK(nLocal == nObject);
    CHECK(pRemote->m_nControlsChanged == 1);
    REQUIRE(pRemote->m_pControlsChanged != NULL);
    CHECK(pRemote->m_pControlsChanged->oldPlayerID == 7);
    CHECK(pRemote->m_pControlsChanged->oldObjectID == 8);
    CHECK(pRemote->m_pControlsChanged->newPlayerID == 1);
    CHECK(pRemote->m_pControlsChanged->newObjectID == nObject);
    CHECK(pRemote->m_pControlsChanged->localControl == FALSE);
}

TEST_CASE("object control: OnObjectControlRequest applies the host's control change", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session(TRUE);

    CGameObject* pObject = new CGameObject();
    pObject->m_remotePlayerID = 3;
    pObject->m_remoteObjectID = 4;
    const LONG nObject = PutObject(game.Game(), pObject);
    AddRemote(game.Game(), 3, 4, nObject);

    CBaldurMessage message;

    BYTE packet[HEADER_LENGTH + 4 * sizeof(DWORD)] = {};
    WritePair(packet + HEADER_LENGTH, 3, 4);
    WritePair(packet + HEADER_LENGTH + PAIR_SIZE, 9, 10);

    // Only the host applies a request.
    session.Network().m_bIsHost = FALSE;
    CHECK(message.OnObjectControlRequest(0, packet, sizeof(packet)) == FALSE);
    session.Network().m_bIsHost = TRUE;

    CHECK(message.OnObjectControlRequest(0, packet, sizeof(packet)) == TRUE);
    CGameRemoteObjectArray* pRemote = game.Game()->GetRemoteObjectArray();
    LONG nLocal = 0;
    CHECK(pRemote->Find(9, 10, nLocal) == TRUE);
    CHECK(nLocal == nObject);
    CHECK(pRemote->m_nControlsChanged == 1);
    REQUIRE(pRemote->m_pControlsChanged != NULL);
    CHECK(pRemote->m_pControlsChanged->oldPlayerID == 3);
    CHECK(pRemote->m_pControlsChanged->oldObjectID == 4);
    CHECK(pRemote->m_pControlsChanged->newPlayerID == 9);
    CHECK(pRemote->m_pControlsChanged->newObjectID == 10);
    CHECK(pRemote->m_pControlsChanged->localControl == FALSE);
}

TEST_CASE("object control: OnObjectControl unmarshals the host's broadcast", "[msg][network]")
{
    ChitinScope chitin;
    GameScope game;
    SessionScope session(FALSE);

    CGameObject* pObject = new CGameObject();
    pObject->m_remotePlayerID = 3;
    pObject->m_remoteObjectID = 4;
    const LONG nObject = PutObject(game.Game(), pObject);
    AddRemote(game.Game(), 3, 4, nObject);

    CBaldurMessage message;

    // {header; SHORT 0 areas changed; SHORT 1 control change;
    //  {PLAYER_ID oldPlayer 3; LONG oldObject 4; PLAYER_ID newPlayer 9;
    //   LONG newObject 10; BOOLEAN localControl 0}}
    const DWORD payloadSize = sizeof(SHORT) + sizeof(SHORT)
        + sizeof(PLAYER_ID) + sizeof(LONG) + sizeof(PLAYER_ID) + sizeof(LONG)
        + sizeof(BOOLEAN);
    BYTE packet[HEADER_LENGTH + payloadSize] = {};
    DWORD cnt = HEADER_LENGTH;
    const SHORT nAreas = 0;
    std::memcpy(packet + cnt, &nAreas, sizeof(nAreas));
    cnt += sizeof(nAreas);
    const SHORT nChanges = 1;
    std::memcpy(packet + cnt, &nChanges, sizeof(nChanges));
    cnt += sizeof(nChanges);
    WritePair(packet + cnt, 3, 4);
    cnt += PAIR_SIZE;
    WritePair(packet + cnt, 9, 10);
    cnt += PAIR_SIZE;
    packet[cnt++] = FALSE;
    REQUIRE(cnt == sizeof(packet));

    // A client applies exactly one broadcast: the host's.
    session.Network().m_bIsHost = TRUE;
    CHECK(message.OnObjectControl(0, packet, sizeof(packet)) == FALSE);
    session.Network().m_bIsHost = FALSE;

    // Player location 1 is not the host's id.
    CHECK(message.OnObjectControl(1, packet, sizeof(packet)) == FALSE);

    // A closed session refuses.
    session.Network().m_bConnectionEstablished = FALSE;
    CHECK(message.OnObjectControl(0, packet, sizeof(packet)) == FALSE);
    session.Network().m_bConnectionEstablished = TRUE;

    CHECK(message.OnObjectControl(0, packet, sizeof(packet)) == TRUE);
    CGameRemoteObjectArray* pRemote = game.Game()->GetRemoteObjectArray();
    LONG nLocal = 0;
    CHECK(pRemote->Find(9, 10, nLocal) == TRUE);
    CHECK(nLocal == nObject);
}
