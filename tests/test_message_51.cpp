// CMessage51 (vtable 0x84D2B8, subtype 51; BG2 names the class
// CMessageSetTarget): the receive-only sprite move order, either to a point
// or as a search request the target's path search consumes.
//
// The identity tests pin the constructor (0x4F6170: arg1 -> source, arg2 ->
// target, m_pSearchRequest cleared, m_frontList = CSearchRequest::LIST_FRONT)
// and the three vtable getters (0x453510 comm type, 0x40A0E0 message type,
// 0x4F61A0 subtype 51). The codec tests pin both wire arms -- the 0x12-byte
// point arm and the search-request arm -- and UnmarshalMessage's
// resolve-or-reject rule. Run's SetTarget arms need a live CGameSprite, so
// only its deny/early-exit arm is covered here.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameArea.h"
#include "CGameObject.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "CSearchBitmap.h"
#include "CVidMode.h"
#include "CWarp.h"

#include <cstring>
#include <memory>

extern CChitin* g_pChitin;

namespace {

// CNetwork::SPEC_MSG_HEADER_LENGTH is a runtime constant; mirror its verified
// value (src/CNetwork.cpp) so the packet arrays have a real bound.
constexpr DWORD HEADER_LENGTH = 3;

// The point arm's payload: player, object, front list, search flag, point.
constexpr DWORD POINT_ARM_SIZE = 0x12;
// The search-request arm's fixed size: header through the area-name byte,
// then the 0x1A-byte object blocks and the trailing counts (0x55 + name).
constexpr DWORD SEARCH_ARM_BASE = 0x55;

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
// touches constructed, as tests/test_message_chapter_done.cpp does.
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

// CSearchRequest's constructor asks CInfGame::GetFrameRate, which reads
// g_pBaldurChitin->GetActiveEngine()->pVidMode->field_94 (0x5BB960). A zeroed
// CWarp and CVidMode cover that single read (field_94 = 0, the 0-fps arms),
// so the request can be built without the running game.
class EngineScope {
public:
    EngineScope()
    {
        m_engine.reset(new unsigned char[sizeof(CWarp)]());
        m_vidMode.reset(new unsigned char[sizeof(CVidMode)]());
        reinterpret_cast<CWarp*>(m_engine.get())->pVidMode = reinterpret_cast<CVidMode*>(m_vidMode.get());
        m_previous = g_pBaldurChitin->pActiveEngine;
        g_pBaldurChitin->pActiveEngine = reinterpret_cast<CWarp*>(m_engine.get());
    }
    ~EngineScope() { g_pBaldurChitin->pActiveEngine = m_previous; }

    EngineScope(const EngineScope&) = delete;
    EngineScope& operator=(const EngineScope&) = delete;

private:
    CWarp* m_previous;
    std::unique_ptr<unsigned char[]> m_engine;
    std::unique_ptr<unsigned char[]> m_vidMode;
};

// A zeroed CGameArea with only its resref set. Marshal's search arm reads the
// target sprite's area resref; Unmarshal's GetArea resolves it back through
// m_gameAreas and takes the address of its m_search bitmap, so no other field
// of the area is touched.
class AreaScope {
public:
    AreaScope()
        : m_storage(new unsigned char[sizeof(CGameArea)]())
    {
        Area()->m_resRef = CResRef("AR1000");
    }

    CGameArea* Area() const { return reinterpret_cast<CGameArea*>(m_storage.get()); }

private:
    std::unique_ptr<unsigned char[]> m_storage;
};

CGameObject* AddObject(CInfGame* pGame, PLAYER_ID remotePlayerID, LONG remoteObjectID)
{
    CGameObject* pObject = new CGameObject();
    pObject->m_objectType = CGameObject::TYPE_SPRITE;

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

// A CSearchRequest with one of everything, for the search-arm round trip.
CSearchRequest* MakeSearchRequest(LONG sourceId, LONG partyId, LONG targetId)
{
    CSearchRequest* pRequest = new CSearchRequest();
    pRequest->m_sourceSide = 2;
    pRequest->m_nPartyIds = 1;
    pRequest->m_nTargetIds = 1;
    pRequest->m_nTargetPoints = 1;
    pRequest->m_removeSelf = FALSE;
    pRequest->m_frontList = CSearchRequest::LIST_BACK_PROMOTE;
    for (BYTE index = 0; index < sizeof(pRequest->m_terrainTable); index++) {
        pRequest->m_terrainTable[index] = static_cast<BYTE>(index + 1);
    }
    pRequest->m_pathSmooth = TRUE;
    pRequest->m_collisionSearch = TRUE;
    pRequest->m_collisionDelay = 7;
    pRequest->m_exclusiveTargetPoints = TRUE;
    pRequest->m_sourceId = sourceId;
    pRequest->m_sourcePt.x = 100;
    pRequest->m_sourcePt.y = 200;
    pRequest->m_partyIds = new LONG[1];
    pRequest->m_partyIds[0] = partyId;
    pRequest->m_targetIds = new LONG[1];
    pRequest->m_targetIds[0] = targetId;
    pRequest->m_targetPoints = new POINT[1];
    pRequest->m_targetPoints[0].x = 300;
    pRequest->m_targetPoints[0].y = 400;
    pRequest->m_minNodes = 11;
    pRequest->m_maxNodes = 22;
    pRequest->m_minNodesBack = 33;
    pRequest->m_maxNodesBack = 44;
    pRequest->m_bBump = FALSE;
    return pRequest;
}

} // namespace

TEST_CASE("msg: CMessage51 identity and constructor (vtable 0x84D2B8)", "[msg][network]")
{
    // 0x4F6170: arg1 -> m_sourceId (+0x8), arg2 -> m_targetId (+0x4); the
    // constructor clears m_pSearchRequest and stores
    // CSearchRequest::LIST_FRONT in m_frontList.
    CMessage51 message(33, 44);

    CHECK(message.GetSource() == 33);
    CHECK(message.GetTarget() == 44);
    CHECK(message.m_pSearchRequest == nullptr);
    CHECK(message.m_frontList == CSearchRequest::LIST_FRONT);

    // 0x453510 (BROADCAST, the fold of every GetCommType returning it),
    // 0x40A0E0 (MSG_TYPE_CMESSAGE) and 0x4F61A0 (subtype 51).
    CHECK(message.GetCommType() == CMessage::BROADCAST);
    CHECK(message.GetMsgType() == CBaldurMessage::MSG_TYPE_CMESSAGE);
    CHECK(message.GetMsgSubType() == 51);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_51);
}

TEST_CASE("msg: CMessage51::MarshalMessage writes the point arm", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    const LONG id = pObject->m_id;

    CMessage51 message(id, id);
    message.m_ptTarget = CPoint(123, -456);
    message.m_frontList = CSearchRequest::LIST_BACK;

    BYTE packet[HEADER_LENGTH + POINT_ARM_SIZE];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    message.MarshalMessage(&pPacket, &dwSize);

    REQUIRE(dwSize == POINT_ARM_SIZE);
    PLAYER_ID player = 0;
    LONG remoteObject = 0;
    LONG x = 0;
    LONG y = 0;
    std::memcpy(&player, packet + HEADER_LENGTH, sizeof(player));
    std::memcpy(&remoteObject, packet + HEADER_LENGTH + 4, sizeof(remoteObject));
    std::memcpy(&x, packet + HEADER_LENGTH + 0xA, sizeof(x));
    std::memcpy(&y, packet + HEADER_LENGTH + 0xE, sizeof(y));
    CHECK(player == 7);
    CHECK(remoteObject == 0x1001);
    CHECK(packet[HEADER_LENGTH + 8] == CSearchRequest::LIST_BACK);
    CHECK(packet[HEADER_LENGTH + 9] == 0);
    CHECK(x == 123);
    CHECK(y == -456);
    // Only the payload is written; the size reported is payload-only.
    CHECK(packet[0] == 0xCD);
}

TEST_CASE("msg: CMessage51::MarshalMessage zeroes the size for an unknown target", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    CMessage51 message(CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX);

    BYTE packet[HEADER_LENGTH + POINT_ARM_SIZE];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0x10;
    message.MarshalMessage(&pPacket, &dwSize);

    CHECK(dwSize == 0);
    for (BYTE nByte : packet) {
        CHECK(nByte == 0xCD);
    }
}

TEST_CASE("msg: CMessage51::UnmarshalMessage resolves the point arm", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    const LONG id = pObject->m_id;
    AddRemote(pGame, 7, 0x1001, id);

    BYTE packet[HEADER_LENGTH + POINT_ARM_SIZE];
    std::memset(packet, 0, sizeof(packet));
    PLAYER_ID player = 7;
    LONG remoteObject = 0x1001;
    LONG x = 55;
    LONG y = -66;
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));
    packet[HEADER_LENGTH + 8] = CSearchRequest::LIST_BACK_PROMOTE;
    packet[HEADER_LENGTH + 9] = 0;
    std::memcpy(packet + HEADER_LENGTH + 0xA, &x, sizeof(x));
    std::memcpy(packet + HEADER_LENGTH + 0xE, &y, sizeof(y));

    CMessage51 message(1, 2);
    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == TRUE);
    CHECK(message.GetTarget() == id);
    CHECK(message.GetSource() == 1);
    CHECK(message.m_frontList == CSearchRequest::LIST_BACK_PROMOTE);
    CHECK(message.m_pSearchRequest == nullptr);
    CHECK(message.m_ptTarget.x == 55);
    CHECK(message.m_ptTarget.y == -66);
}

TEST_CASE("msg: CMessage51::UnmarshalMessage rejects an unknown remote pair", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    BYTE packet[HEADER_LENGTH + POINT_ARM_SIZE];
    std::memset(packet, 0, sizeof(packet));
    PLAYER_ID player = 9;
    LONG remoteObject = 0x1999;
    std::memcpy(packet + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(packet + HEADER_LENGTH + 4, &remoteObject, sizeof(remoteObject));

    CMessage51 message(1, 2);
    CHECK(message.UnmarshalMessage(packet, sizeof(packet)) == FALSE);
    CHECK(message.GetTarget() == 2);
    CHECK(message.m_ptTarget.x == 0);
    CHECK(message.m_ptTarget.y == 0);
}

TEST_CASE("msg: CMessage51 codec round-trips the search-request arm", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    EngineScope engine;
    AreaScope area;
    CInfGame* pGame = scope.Game();

    CGameObject* pTarget = AddObject(pGame, 7, 0x1001);
    const LONG targetId = pTarget->m_id;
    pTarget->m_pArea = area.Area();
    const LONG sourceId = AddObject(pGame, 8, 0x1002)->m_id;
    const LONG partyId = AddObject(pGame, 9, 0x1003)->m_id;
    const LONG targetObjectId = AddObject(pGame, 10, 0x1004)->m_id;
    AddRemote(pGame, 7, 0x1001, targetId);
    AddRemote(pGame, 8, 0x1002, sourceId);
    AddRemote(pGame, 9, 0x1003, partyId);
    AddRemote(pGame, 10, 0x1004, targetObjectId);
    pGame->m_gameAreas[0] = area.Area();

    CMessage51 sent(3, targetId);
    sent.m_pSearchRequest = MakeSearchRequest(sourceId, partyId, targetObjectId);
    sent.m_frontList = CSearchRequest::LIST_BACK;
    sent.m_ptTarget = CPoint(1, 2);

    // The fake area's resref is "AR1000", so the payload is
    // 0x55 + 6 + 8 * (1 party id + 1 target id + 1 target point).
    const DWORD expectedSize = SEARCH_ARM_BASE + 6 + 8 * 3;
    BYTE packet[HEADER_LENGTH + 0x100];
    std::memset(packet, 0xCD, sizeof(packet));
    BYTE* pPacket = packet + HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pPacket, &dwSize);
    REQUIRE(dwSize == expectedSize);

    CMessage51 received(0, 0);
    REQUIRE(received.UnmarshalMessage(packet, HEADER_LENGTH + dwSize) == TRUE);

    CHECK(received.GetTarget() == targetId);
    CHECK(received.m_frontList == CSearchRequest::LIST_BACK);
    REQUIRE(received.m_pSearchRequest != nullptr);

    CSearchRequest* pRequest = received.m_pSearchRequest;
    CHECK(pRequest->m_sourceSide == 2);
    CHECK(pRequest->m_nPartyIds == 1);
    CHECK(pRequest->m_nTargetIds == 1);
    CHECK(pRequest->m_nTargetPoints == 1);
    CHECK(pRequest->m_removeSelf == FALSE);
    CHECK(pRequest->m_frontList == CSearchRequest::LIST_BACK_PROMOTE);
    for (BYTE index = 0; index < sizeof(pRequest->m_terrainTable); index++) {
        CHECK(pRequest->m_terrainTable[index] == index + 1);
    }
    CHECK(pRequest->m_pathSmooth == TRUE);
    CHECK(pRequest->m_collisionSearch == TRUE);
    CHECK(pRequest->m_collisionDelay == 7);
    CHECK(pRequest->m_exclusiveTargetPoints == TRUE);
    CHECK(pRequest->m_sourceId == sourceId);
    CHECK(pRequest->m_sourcePt.x == 100);
    CHECK(pRequest->m_sourcePt.y == 200);
    CHECK(pRequest->m_partyIds[0] == partyId);
    CHECK(pRequest->m_targetIds[0] == targetObjectId);
    CHECK(pRequest->m_targetPoints[0].x == 300);
    CHECK(pRequest->m_targetPoints[0].y == 400);
    CHECK(pRequest->m_minNodes == 11);
    CHECK(pRequest->m_maxNodes == 22);
    CHECK(pRequest->m_minNodesBack == 33);
    CHECK(pRequest->m_maxNodesBack == 44);
    CHECK(pRequest->m_bBump == FALSE);
    CHECK(pRequest->m_searchBitmap == &area.Area()->m_search);
}

TEST_CASE("msg: CMessage51::Run leaves a non-sprite target alone", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    // A target that resolves but is not a sprite: Run must release the deny
    // without calling SetTarget. (The two SetTarget arms need a live
    // CGameSprite and are not covered here.)
    CGameObject* pObject = AddObject(pGame, 0, 0);
    pObject->m_objectType = CGameObject::TYPE_DOOR;
    const LONG id = pObject->m_id;

    CMessage51 message(id, id);
    message.m_ptTarget = CPoint(3, 4);
    message.Run();

    CHECK(pObject->m_pArea == nullptr);
    CHECK(pObject->m_id == id);
}
