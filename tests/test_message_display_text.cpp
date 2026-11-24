// CMessageDisplayText::UnmarshalMessage (0x4FC390, survey 121 row 73): the
// receive side of the floating-text message. The class is broadcast so the
// other machines show the same text; single player never receives it
// (docs/adr/0002-single-player-only.md). Its MarshalMessage is not in this
// row set, so the case builds the wire body by hand from the decode below.
//
// The fixture is the zeroed CBaldurChitin/CInfGame with the two arrays the
// message touches, as tests/test_message_chapter_done.cpp builds it.
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
#include <string>
#include <vector>

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

// The body the decode at 0x4FC390 expects: remote pair, SHORT name length and
// name, SHORT text length and text, name/text colors, marker, move-to-top.
std::vector<BYTE> MakePacket(PLAYER_ID player, LONG remoteObjectID, const std::string& name, LONG marker, BOOLEAN moveToTop)
{
    std::vector<BYTE> packet(HEADER_LENGTH + 8 + 2 + name.size() + 2 + 3 + 4 + 4 + 4 + 1);
    BYTE* p = packet.data();
    std::memcpy(p + HEADER_LENGTH, &player, sizeof(player));
    std::memcpy(p + HEADER_LENGTH + 4, &remoteObjectID, sizeof(remoteObjectID));
    SHORT length = static_cast<SHORT>(name.size());
    std::memcpy(p + HEADER_LENGTH + 8, &length, sizeof(length));
    std::memcpy(p + HEADER_LENGTH + 0xA, name.data(), name.size());
    DWORD at = HEADER_LENGTH + 0xA + name.size();
    const char* text = "TXT";
    length = 3;
    std::memcpy(p + at, &length, sizeof(length));
    at += 2;
    std::memcpy(p + at, text, 3);
    at += 3;
    COLORREF nameColor = 0x11223344;
    COLORREF textColorValue = 0x55667788;
    std::memcpy(p + at, &nameColor, sizeof(nameColor));
    at += 4;
    std::memcpy(p + at, &textColorValue, sizeof(textColorValue));
    at += 4;
    std::memcpy(p + at, &marker, sizeof(marker));
    at += 4;
    p[at] = moveToTop;
    at += 1;
    packet.resize(at);
    return packet;
}

} // namespace

TEST_CASE("msg: CMessageDisplayText::UnmarshalMessage decodes the body", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;
    CInfGame* pGame = scope.Game();

    CGameObject* pObject = AddObject(pGame, 7, 0x1001);
    AddRemote(pGame, 7, 0x1001, pObject->m_id);

    std::vector<BYTE> packet = MakePacket(7, 0x1001, "NAME", 42, TRUE);

    CMessageDisplayText message(CString(""), CString(""), 0, 0, 0, 0, 0);
    REQUIRE(message.UnmarshalMessage(packet.data(), static_cast<DWORD>(packet.size())) == TRUE);
    CHECK(message.GetTarget() == pObject->m_id);
    CHECK(message.m_name == "NAME");
    CHECK(message.m_text == "TXT");
    CHECK(message.m_nameColor == 0x11223344);
    CHECK(message.m_textColor == 0x55667788);
    CHECK(message.m_marker == 42);
    CHECK(message.m_moveToTop == TRUE);
}

TEST_CASE("msg: CMessageDisplayText::UnmarshalMessage keeps an unresolved target as INVALID", "[msg][network]")
{
    ChitinScope chitin;
    GameScope scope;

    std::vector<BYTE> packet = MakePacket(9, 0x1999, "NAME", 42, FALSE);

    CMessageDisplayText message(CString(""), CString(""), 0, 0, 0, 0, 44);
    CHECK(message.UnmarshalMessage(packet.data(), static_cast<DWORD>(packet.size())) == TRUE);
    CHECK(message.GetTarget() == static_cast<LONG>(CGameObjectArray::INVALID_INDEX));
    CHECK(message.m_name == "NAME");
    CHECK(message.m_text == "TXT");
    CHECK(message.m_moveToTop == FALSE);

    // The vtable getters: BROADCAST_FORCED and the cut-scene status subtype.
    CHECK(message.GetCommType() == CMessage::BROADCAST_FORCED);
    CHECK(message.GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_CUT_SCENE_MODE_STATUS);
}
