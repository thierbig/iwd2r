// The null-provider session sequence of CNetwork, linked against the engine.
//
// Fixture: `CNetwork n;` constructs without the chitin object, but the
// session sequence does not: HostNewSession, AddPlayerToList (called by
// CreatePlayer) and OnCloseSession call the g_pChitin->OnMultiplayer* hooks,
// and the incoming-queue code locks g_pChitin->cNetwork.field_F52. A bare
// CChitin subclass supplies both: its hooks are the base class's empty ones,
// and constructing it initializes its own cNetwork. The four thread bodies are
// pure virtual in CChitin and never run here.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CMessage.h"
#include "CNetwork.h"

extern CChitin* g_pChitin;

namespace {

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }
};

// Installs a TestChitin as g_pChitin for the scope of a test.
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

}

TEST_CASE("null service provider hosts a single-player session", "[network]")
{
    ChitinScope scope;
    REQUIRE(g_pChitin != nullptr);

    CNetwork n;

    REQUIRE(n.EnumerateServiceProviders() == TRUE);
    REQUIRE(n.m_nTotalServiceProviders == 1);
    REQUIRE(n.SelectServiceProvider(CNetwork::SERV_PROV_NULL) == TRUE);
    REQUIRE(n.InitializeConnectionToServiceProvider(TRUE) == TRUE);

    n.m_sSessionNameToMake = "s";
    n.m_bSessionNameToMake = TRUE;
    n.m_sLocalPlayerName = "p";
    n.m_bPlayerNameToMake = TRUE;

    REQUIRE(n.HostNewSession() == TRUE);

    INT err = -1;
    REQUIRE(n.CreatePlayer(err) == TRUE);
    CHECK(err == CNetwork::ERROR_NONE);

    CHECK(n.GetSessionOpen() == TRUE);
    CHECK(n.GetSessionHosting() == TRUE);
    CHECK(n.m_idLocalPlayer == 1);

    CString s;
    n.GetHostPlayerName(s);
    CHECK(s == "p");

    CHECK(n.FindPlayerLocationByID(1, FALSE) == 0);

    BYTE data[4] = { 1, 2, 3, 4 };
    CHECK(n.SendSpecificMessage("p", 0, 1, 1, data, 4) == TRUE);
    CHECK(n.PeekSpecificMessage("p", 1, 1) == FALSE);

    n.CloseSession(TRUE);
    CHECK(n.GetSessionOpen() == FALSE);
    CHECK(n.m_idLocalPlayer == 0);
}

// CBaldurMessage::SendChatMessage (0x432C50): the chat formatting and the
// chat-buffer path. A plain message becomes "[<local player>]:  <text>"; a
// "name: text" whose name is a player in the session becomes
// "[<local>->name]:  <text>" (a whisper, sent to that player); an unknown
// name stays in the text; with no session the message is the joke answer
// "You talking to me?". The caller's string and the local chat buffer both
// receive the formatted text. The network send itself needs a live
// DirectPlay session, so the test only observes the formatting and the
// buffer (the unit-test route from report-T122.md row 8).
TEST_CASE("CBaldurMessage::SendChatMessage formats the local, target and disconnected chat", "[msg][network]")
{
    ChitinScope scope;
    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage message;

    network.m_sLocalPlayerName = "sender";
    network.m_psPlayerName[0] = "target";
    network.m_pPlayerID[0] = 1;
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = FALSE;

    // A plain message is announced with the local player's name.
    CString sText("hello");
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "hello"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "[sender]:  hello");

    // "target: text" to a player in the session is a whisper.
    message.m_cChatBuffer.ClearMessages();
    sText = "target: whisper";
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "target: whisper"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "[sender->target]:   whisper");

    // A name that is not a player stays part of the text.
    message.m_cChatBuffer.ClearMessages();
    sText = "nobody: hello";
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "nobody: hello"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "[sender]:  nobody: hello");

    // No session: the joke answer, still added to the local buffer.
    network.m_bConnectionEstablished = FALSE;
    message.m_cChatBuffer.ClearMessages();
    sText = "hello";
    CHECK(message.SendChatMessage(sText) == TRUE);
    CHECK(sText == "hello"); // the caller's CString is not rewritten
    REQUIRE(message.m_cChatBuffer.m_lMessages.GetCount() == 1);
    CHECK(*message.m_cChatBuffer.m_lMessages.GetTail() == "You talking to me?");

    // Leave the embedded CNetwork closed before the ChitinScope restores
    // g_pChitin (its destructor would otherwise call OnCloseSession).
    network.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
}
