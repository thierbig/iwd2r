#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAIConditionResponse.h"
#include "CAIResponse.h"
#include "CAIScript.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CDerivedStats.h"
#include "CGameAIBase.h"
#include "CGameArea.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameEffect.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenJournal.h"
#include "CScreenWorld.h"
#include "CSpell.h"
#include "CStore.h"
#include "CVariableHash.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

// The private view of the install this process's engine runs on (set by
// StartEngine; as tests/test_rest_store.cpp does).
fs::path g_view;

// The engine in this process, as iwd2r_main builds it, without a game
// loaded: the game object, its variables, object array and message handler
// exist. Built once and leaked: the engine's globals live until the process
// ends. The engine writes into its data folder: it starts on a private view.
bool StartEngine(std::string& error)
{
    static bool started = false;
    static std::string startError;
    static bool tried = false;
    if (tried) {
        error = startError;
        return started;
    }
    tried = true;

#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
    static std::unique_ptr<DataView> view;
    const char* install = std::getenv("IWD2_DATA");
    if (install != nullptr && *install != '\0') {
        const std::string original = install;
        view = std::make_unique<DataView>(original);
        g_view = view->Path();
#ifdef _WIN32
        _putenv_s("IWD2_DATA", view->Path().c_str());
#else
        setenv("IWD2_DATA", view->Path().c_str(), 1);
#endif
        const bool found = platform::ResolveDataDir();
#ifdef _WIN32
        _putenv_s("IWD2_DATA", original.c_str());
#else
        setenv("IWD2_DATA", original.c_str(), 1);
#endif
        if (!found) {
            startError = "data folder not found";
            return false;
        }
    } else if (!platform::ResolveDataDir()) {
        startError = "data folder not found";
        return false;
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        return false;
    }
    chitin->Init(NULL);
    // The sprites' palettes read the video mode (CVidPalette::SetType), as
    // tests/test_cre.cpp gives it.
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

#define REQUIRE_ENGINE()                            \
    do {                                            \
        const char* env = std::getenv("IWD2_DATA"); \
        if (env == nullptr || *env == '\0') {       \
            SKIP("[iwd2r-skip] IWD2_DATA unset");   \
        }                                           \
        std::string error;                          \
        INFO(error);                                \
        REQUIRE(StartEngine(error));                \
    } while (0)

// An AI object in the game's object array, as an area's objects are.
CGameAIBase* AddAIObject()
{
    CGameAIBase* pObject = new CGameAIBase();
    LONG id = CGameObjectArray::INVALID_INDEX;
    REQUIRE(g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&id, pObject, INFINITE)
        == CGameObjectArray::SUCCESS);
    pObject->m_id = id;
    return pObject;
}

// A live sprite from CHARBASE.CRE, made the way StartCreateChar makes it
// (0x610540); its constructor adds it to the object array.
CGameSprite* MakeSprite()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    BYTE* data = file.GetData();
    REQUIRE(data != NULL);
    std::vector<BYTE> bytes(data, data + file.GetDataSize());
    file.Release();
    return new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
}

// A door: zeroed storage with its members that own memory constructed and
// its flags set. ToggleDoor reads only these when the door has no search
// polygons.
CGameDoor* MakeDoor(DWORD dwFlags)
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameDoor)));
    std::memset(storage, 0, sizeof(CGameDoor));
    CGameDoor* pDoor = reinterpret_cast<CGameDoor*>(storage);
    new (&pDoor->m_sound) CSound();
    new (&pDoor->m_pendingTriggers) CTypedPtrList<CPtrList, CAITrigger*>();
    new (&pDoor->m_curAction) CAIAction();
    new (&pDoor->m_typeAI) CAIObjectType();
    // The area: zeroed storage named AR1000, its search map empty (the door
    // has no search polygons).
    auto* area = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(area, 0, sizeof(CGameArea));
    pDoor->m_pArea = reinterpret_cast<CGameArea*>(area);
    std::memcpy(pDoor->m_pArea->m_header.m_areaName, "AR1000", 6);
    pDoor->m_dwFlags = dwFlags;
    pDoor->m_strNotPickable = static_cast<STRREF>(-1);
    pDoor->m_ptOpenDest = CPoint(10, 20);
    pDoor->m_ptClosedDest = CPoint(30, 40);
    // Not in the object array (that would need the door's constructor and an
    // area file): its status messages have no target and are dropped.
    pDoor->m_id = CGameObjectArray::INVALID_INDEX;
    return pDoor;
}

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The messages of type T waiting in the handler's queue, in queue order,
// without running them (tests/test_sprite_actions.cpp's helper).
template <typename T>
std::vector<T*> Queued()
{
    std::vector<T*> found;
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        CMessage* message = handler.m_messageList.GetNext(pos);
        if (T* typed = dynamic_cast<T*>(message)) {
            found.push_back(typed);
        }
    }
    return found;
}

std::string Lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// The bytes of a file in the view's temp folder; empty when there is none
// (as tests/test_rest_store.cpp's helper).
std::vector<char> TempFile(const char* name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(g_view, ec)) {
        if (Lower(entry.path().filename().string()) != "temp") {
            continue;
        }
        for (const auto& file : fs::directory_iterator(entry.path(), ec)) {
            if (Lower(file.path().filename().string()) == Lower(name)) {
                std::ifstream in(file.path(), std::ios::binary);
                return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            }
        }
    }
    return std::vector<char>();
}

uint32_t Dword(const std::vector<char>& d, size_t at)
{
    uint32_t value;
    std::memcpy(&value, d.data() + at, sizeof(value));
    return value;
}

// An effect the way CMessageAddEffect decodes one: the AddEffect arms own
// and delete it.
CGameEffect* MakeDoorEffect(WORD effectID)
{
    ITEM_EFFECT effect;
    std::memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    return CGameEffect::DecodeEffect(&effect,
        CPoint(-1, -1),
        CGameObjectArray::INVALID_INDEX,
        CPoint(-1, -1));
}

}

TEST_CASE("ai: ExecuteAction answers NoAction, an unknown action and SetGlobal", "[ai][data]")
{
    REQUIRE_ENGINE();
    CGameAIBase* pCaller = AddAIObject();

    pCaller->SetCurrAction(CAIAction::NULL_ACTION);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_NO_ACTION);
    CHECK(pCaller->m_nLastActionReturn == CGameAIBase::ACTION_NO_ACTION);

    // Attack (3) is a sprite's action: the base dispatcher reports it.
    CAIAction attack;
    attack.m_actionID = CAIAction::ATTACK;
    pCaller->SetCurrAction(attack);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // SetGlobal("ai_test_var","GLOBAL",5): the script compiler's string1 is
    // the scope followed by the name.
    CAIAction setGlobal;
    setGlobal.m_actionID = 30;
    setGlobal.m_string1 = "GLOBALai_test_var";
    setGlobal.m_specificID = 5;
    pCaller->SetCurrAction(setGlobal);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("AI_TEST_VAR"));
    REQUIRE(pVariable != NULL);
    CHECK(pVariable->m_intValue == 5);

    // IncrementGlobal by 3.
    CAIAction increment;
    increment.m_actionID = 109;
    increment.m_string1 = "GLOBALAI_TEST_VAR";
    increment.m_specificID = 3;
    pCaller->SetCurrAction(increment);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pVariable->m_intValue == 8);

    // The status triggers read it back: Global, GlobalGT, GlobalLT.
    CAITrigger global(0x400F, 8);
    global.m_string1 = "GLOBALAI_TEST_VAR";
    CHECK(pCaller->EvaluateStatusTrigger(global) == TRUE);
    CAITrigger globalGT(0x4034, 8);
    globalGT.m_string1 = "GLOBALAI_TEST_VAR";
    CHECK(pCaller->EvaluateStatusTrigger(globalGT) == FALSE);
    CAITrigger globalLT(0x4035, 9);
    globalLT.m_string1 = "GLOBALAI_TEST_VAR";
    CHECK(pCaller->EvaluateStatusTrigger(globalLT) == TRUE);
    // A variable nobody set reads as 0.
    CAITrigger missing(0x400F, 0);
    missing.m_string1 = "GLOBALAI_NEVER_SET";
    CHECK(pCaller->EvaluateStatusTrigger(missing) == TRUE);

    // AllowAreaResting(0) sets the area's no-rest flag (m_header.m_flags 2),
    // which AreaRestDisabled reads; AllowAreaResting(1) clears it.
    {
        auto* area = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
        std::memset(area, 0, sizeof(CGameArea));
        pCaller->m_pArea = reinterpret_cast<CGameArea*>(area);
        CAIAction allowResting;
        allowResting.m_actionID = 286;
        allowResting.m_specificID = 0;
        pCaller->SetCurrAction(allowResting);
        CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
        CAITrigger restDisabled(0x40D2, 0);
        CHECK(pCaller->EvaluateStatusTrigger(restDisabled) == TRUE);
        CHECK(pCaller->m_pArea->m_header.m_areaType == 0);
        allowResting.m_specificID = 1;
        pCaller->SetCurrAction(allowResting);
        CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
        CHECK(pCaller->EvaluateStatusTrigger(restDisabled) == FALSE);
        pCaller->m_pArea = NULL;
    }

    // Exists and Dead over an object of the array: an AI object that is not
    // a sprite exists and counts as dead; nobody does not exist.
    {
        CGameAIBase* pOther = AddAIObject();
        CAIObjectType other(0, 0, 0, 0, 0, 0, 0, 0, pOther->m_id, 0, 0);
        CAITrigger exists(0x400D, other, 0);
        CHECK(pCaller->EvaluateStatusTrigger(exists) == TRUE);
        CAITrigger dead(0x4051, other, 0);
        CHECK(pCaller->EvaluateStatusTrigger(dead) == TRUE);
        CAITrigger existsNobody(0x400D, CAIObjectType::NOONE, 0);
        CHECK(pCaller->EvaluateStatusTrigger(existsNobody) == FALSE);
        CAITrigger deadNobody(0x4051, CAIObjectType::NOONE, 0);
        CHECK(pCaller->EvaluateStatusTrigger(deadNobody) == TRUE);
    }

    // RandomNum(6, n): the object's random value of this update, 1 to 6.
    pCaller->m_randValue = 15;
    CAITrigger randomNum(0x4047, 6);
    randomNum.m_specific2 = 15 % 6 + 1;
    CHECK(pCaller->EvaluateStatusTrigger(randomNum) == TRUE);
    CAITrigger randomNumLT(0x4049, 6);
    randomNumLT.m_specific2 = 15 % 6 + 1;
    CHECK(pCaller->EvaluateStatusTrigger(randomNumLT) == FALSE);

    ClearMessages();
}

TEST_CASE("ai: Dead and IsActive answer for a live sprite", "[ai][data]")
{
    REQUIRE_ENGINE();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    REQUIRE(pSprite->m_id != CGameObjectArray::INVALID_INDEX);
    REQUIRE((pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0);
    REQUIRE((pSprite->m_baseStats.m_generalState & STATE_DEAD) == 0);
    CAIObjectType sprite(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // Alive in both states: not dead. Dead by its base state: dead.
    CAITrigger dead(0x4051, sprite, 0);
    CHECK(pCaller->EvaluateStatusTrigger(dead) == FALSE);
    pSprite->m_baseStats.m_generalState |= STATE_DEAD;
    CHECK(pCaller->EvaluateStatusTrigger(dead) == TRUE);
    pSprite->m_baseStats.m_generalState &= ~STATE_DEAD;
    pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
    CHECK(pCaller->EvaluateStatusTrigger(dead) == TRUE);
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;

    // IsActive reads the sprite's active flag.
    CAITrigger isActive(0x40D7, sprite, 0);
    pSprite->m_active = TRUE;
    CHECK(pCaller->EvaluateStatusTrigger(isActive) == TRUE);
    pSprite->m_active = FALSE;
    CHECK(pCaller->EvaluateStatusTrigger(isActive) == FALSE);
    // An AI object that is neither a sound, a sprite nor a trigger: FALSE.
    CAIObjectType other(0, 0, 0, 0, 0, 0, 0, 0, pCaller->m_id, 0, 0);
    CAITrigger isActiveOther(0x40D7, other, 0);
    CHECK(pCaller->EvaluateStatusTrigger(isActiveOther) == FALSE);
}

TEST_CASE("ai: a compiled script reads its triggers and weighted responses", "[ai][data]")
{
    REQUIRE_ENGINE();
    // AR1000.BCS's first block: OnCreation() and Global("HEALED_REIG",
    // "MYAREA",0), one response of weight 100: SetHP and Continue.
    CAIScript script(CResRef("AR1000"));
    REQUIRE(script.m_caList.GetCount() > 0);
    CAIConditionResponse* pFirst = script.m_caList.GetHead();
    REQUIRE(pFirst->m_condition.m_triggerList.GetCount() == 2);
    CHECK(pFirst->m_condition.m_triggerList.GetHead()->m_triggerID == CAITrigger::ONCREATION);
    CAITrigger* pGlobal = pFirst->m_condition.m_triggerList.GetTail();
    CHECK(pGlobal->m_triggerID == 0x400F);
    CHECK(pGlobal->m_string1 == "MYAREAHEALED_REIG");
    REQUIRE(pFirst->m_responseSet.m_responseList.GetCount() == 1);
    CHECK(pFirst->m_responseSet.m_weightTotal == 100);
    CAIResponse* pResponse = pFirst->m_responseSet.m_responseList.GetHead();
    CHECK(pResponse->m_weight == 100);
    REQUIRE(pResponse->m_actionList.GetCount() == 2);
    CHECK(pResponse->m_actionList.GetHead()->m_actionID == 254);
    CHECK(pResponse->m_actionList.GetTail()->m_actionID == CAIAction::CONTINUE);
}

TEST_CASE("ai: a locked door stays closed and says it is locked", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pClicker = AddAIObject();
    CGameDoor* pDoor = MakeDoor(0x2);

    CAIObjectType cause(0, 0, 0, 0, 0, 0, 0, 0, pClicker->m_id, 0, 0);
    pDoor->ToggleDoor(cause, FALSE);

    CHECK((pDoor->m_dwFlags & 0x1) == 0);
    CHECK((pDoor->m_dwFlags & 0x2) != 0);
    CHECK(pDoor->m_pos == CPoint(0, 0));

    // The feedback: "Locked" (15874) to the one who tried it, in the
    // original's colors.
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    auto* message = dynamic_cast<CMessageDisplayTextRefSend*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_text == 15874);
    CHECK(message->m_name == static_cast<STRREF>(-1));
    CHECK(message->m_textColor == RGB(215, 215, 190));
    CHECK(message->m_targetId == pClicker->m_id);
    CHECK(message->m_sourceId == pDoor->m_id);

    // A door with its own "locked" text says that instead.
    ClearMessages();
    pDoor->m_dwFlags |= 0x200;
    pDoor->m_strNotPickable = 1234;
    pDoor->ToggleDoor(cause, FALSE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    message = dynamic_cast<CMessageDisplayTextRefSend*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_text == 1234);
    CHECK((pDoor->m_dwFlags & 0x1) == 0);

    ClearMessages();
}

TEST_CASE("ai: a forced toggle opens a locked door, unlocks it, and closes it again", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameDoor* pDoor = MakeDoor(0x2);
    CAIObjectType cause(0, 0, 0, 0, 0, 0, 0, 0, CGameObjectArray::INVALID_INDEX, 0, 0);

    // A script's OpenDoor (143) toggles with bForce: the lock is ignored and
    // the door ends unlocked.
    pDoor->ToggleDoor(cause, TRUE);
    CHECK((pDoor->m_dwFlags & 0x1) != 0);
    CHECK((pDoor->m_dwFlags & 0x2) == 0);
    CHECK(pDoor->m_pos == CPoint(30, 40));
    CHECK(pDoor->m_tiledObject.m_wAIState == CTiledObject::STATE_PRIMARY_TILE);

    pDoor->ToggleDoor(cause, FALSE);
    CHECK((pDoor->m_dwFlags & 0x1) == 0);
    CHECK(pDoor->m_pos == CPoint(10, 20));
    CHECK(pDoor->m_tiledObject.m_wAIState == CTiledObject::STATE_SECONDARY_TILE);

    ClearMessages();
}

TEST_CASE("ai: knock unlocks a normally locked door and posts a door-open message", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameDoor* pDoor = MakeDoor(0x2);
    // The door-open message is SEND-routed, so the door needs a real id for
    // CMessageHandler::Send to find its target. The raw-storage door cannot
    // go into the object array itself (it has no vtable for later scans), so
    // it borrows a live AI object's id: Send reads only the target's
    // m_remotePlayerID from the slot. AddEffect is virtual, so the calls
    // below are qualified: the zeroed storage has no vfptr to dispatch
    // through, and the qualified name lands in the ported body directly.
    CGameAIBase* pProxy = AddAIObject();
    pDoor->m_id = pProxy->m_id;
    LONG id = pProxy->m_id;

    pDoor->CGameDoor::AddEffect(MakeDoorEffect(CGAMEEFFECT_KNOCK), 0, TRUE, TRUE);

    CHECK((pDoor->m_dwFlags & 0x2) == 0);

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    auto* message = dynamic_cast<CMessageDoorOpen*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_bOpen == TRUE);
    CHECK(message->m_targetId == id);
    CHECK(message->m_sourceId == id);

    ClearMessages();
}

TEST_CASE("ai: knock on a magically locked door says the spell failed", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameDoor* pDoor = MakeDoor(0x2);
    pDoor->m_lockDifficulty = 100;
    pDoor->CGameDoor::AddEffect(MakeDoorEffect(CGAMEEFFECT_KNOCK), 0, TRUE, TRUE);

    // Still locked.
    CHECK((pDoor->m_dwFlags & 0x2) != 0);

    // "The lock is held by magic and cannot be picked" (23169) in the
    // original's colors. Broadcast-forced, so it lands on the list even
    // though the door's id is INVALID_INDEX.
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    auto* message = dynamic_cast<CMessageDisplayTextRef*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_text == 0x5A81);
    CHECK(message->m_name == static_cast<STRREF>(-1));
    CHECK(message->m_nameColor == 0);
    CHECK(message->m_textColor == static_cast<COLORREF>(0xD7C8A0));
    CHECK(message->m_marker == -1);

    ClearMessages();
}

TEST_CASE("ai: detect traps marks a trapped door detected and redraws its outline", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameDoor* pDoor = MakeDoor(0x8);
    pDoor->m_trapActivated = 1;
    pDoor->m_trapDetectionDifficulty = 50;
    // The sprite-autopause lookup short-circuits on a NOONE type.
    pDoor->m_typeAI.Set(CAIObjectType::NOONE);
    // The set-trigger message is SEND-routed; borrow a live AI object's id
    // (the raw-storage door has no vtable to sit in the object array).
    CGameAIBase* pProxy = AddAIObject();
    pDoor->m_id = pProxy->m_id;

    pDoor->CGameDoor::AddEffect(MakeDoorEffect(CGAMEEFFECT_DETECTTRAPS), 0, TRUE, TRUE);

    CHECK(pDoor->m_trapDetected == 1);
    CHECK(pDoor->m_drawPoly == 400);

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 3);
    POSITION pos = handler.m_messageList.GetHeadPosition();
    auto* pSet = dynamic_cast<CMessageSetTrigger*>(handler.m_messageList.GetNext(pos));
    REQUIRE(pSet != nullptr);
    CHECK(pSet->m_trigger.m_triggerID == CAITRIGGER_DETECTED);
    auto* pStatus = dynamic_cast<CMessageTriggerStatus*>(handler.m_messageList.GetNext(pos));
    REQUIRE(pStatus != nullptr);
    CHECK(pStatus->m_dwFlags == 0x8);
    CHECK(pStatus->m_trapActivated == 1);
    CHECK(pStatus->m_trapDetected == 1);
    auto* pDraw = dynamic_cast<CMessageSetDrawPoly*>(handler.m_messageList.GetNext(pos));
    REQUIRE(pDraw != nullptr);
    CHECK(pDraw->m_time == 400);

    // Already detected: the second pass touches no message list.
    ClearMessages();
    pDoor->CGameDoor::AddEffect(MakeDoorEffect(CGAMEEFFECT_DETECTTRAPS), 0, TRUE, TRUE);
    CHECK(handler.m_messageList.GetCount() == 0);

    ClearMessages();
}

TEST_CASE("ai: a trigger hit by a cast-spell effect forces the spell on the effect's source", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    // The trigger: raw storage with the CGameAIBase base placement-constructed.
    // The CASTSPELL arm calls GetAIType() virtually (exe 0x4CD97C, vtable slot
    // 0x10); the base's vtable serves it, since neither CGameAIBase nor
    // CGameTrigger overrides GetAIType. AddEffect itself is called qualified,
    // as the door tests do (0x4CD650).
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameTrigger)));
    std::memset(storage, 0, sizeof(CGameTrigger));
    new (storage) CGameAIBase();
    CGameTrigger* pTrigger = reinterpret_cast<CGameTrigger*>(storage);
    // The CMessageAddAction is SEND-routed, so it needs a live target; the
    // trigger borrows a live AI object's id for its own, so the caller and
    // target ids in the message are distinct live values.
    CGameAIBase* pProxy = AddAIObject();
    pTrigger->m_id = pProxy->m_id;

    // The effect: a CASTSPELL from a second live object, the caster.
    CGameAIBase* pSource = AddAIObject();
    CGameEffect* pEffect = MakeDoorEffect(CGAMEEFFECT_CASTSPELL);
    pEffect->m_sourceID = pSource->m_id;
    pEffect->m_effectAmount = 7;
    pEffect->m_res = CResRef("SPWI123");

    pTrigger->CGameTrigger::AddEffect(pEffect, 0, TRUE, TRUE);

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    auto* message = dynamic_cast<CMessageAddAction*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_action.m_actionID == CAIAction::FORCESPELL);
    CHECK(message->m_action.m_acteeID.Equal(pTrigger->m_typeAI));
    CHECK(message->m_action.m_string1 == "SPWI123");
    CHECK(message->m_action.m_specificID == 7);
    // The trigger is the caller; the effect's source is the target (the exe's
    // stores at 0x4CDAE6-0x4CDAF8: m_id@0x5c -> +0x8, m_sourceID@0x10c -> +0x4).
    CHECK(message->m_sourceId == pProxy->m_id);
    CHECK(message->m_targetId == pSource->m_id);

    ClearMessages();
}

TEST_CASE("ai: Allegiance, Range, See and the HP triggers answer both ways", "[ai][data]")
{
    REQUIRE_ENGINE();
    auto* areaStorage = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(areaStorage, 0, sizeof(CGameArea));
    CGameArea* pArea = reinterpret_cast<CGameArea*>(areaStorage);

    CGameAIBase* pCaller = AddAIObject();
    pCaller->m_pArea = pArea;
    pCaller->m_pos = CPoint(400, 300);
    pCaller->m_typeAI.m_nEnemyAlly = 255;

    // Allegiance(Myself, n): the caller's own enemy-ally byte.
    CAIObjectType myself(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    myself.m_SpecialCase[0] = 1;
    CAITrigger enemy(0x400B, myself, 255);
    CHECK(pCaller->EvaluateStatusTrigger(enemy) == TRUE);
    CAITrigger neutral(0x400B, myself, 128);
    CHECK(pCaller->EvaluateStatusTrigger(neutral) == FALSE);

    // Range(object, 2, less than): within (2 + 1)^2 squared grid squares.
    CGameAIBase* pTarget = AddAIObject();
    pTarget->m_pArea = pArea;
    pTarget->m_pos = CPoint(400 + 2 * CPathSearch::GRID_SQUARE_SIZEX, 300);
    CAIObjectType target(0, 0, 0, 0, 0, 0, 0, 0, pTarget->m_id, 0, 0);
    CAITrigger rangeNear(0x4018, target, 2);
    rangeNear.m_specific2 = 2;
    CHECK(pCaller->EvaluateStatusTrigger(rangeNear) == TRUE);
    pTarget->m_pos = CPoint(400 + 5 * CPathSearch::GRID_SQUARE_SIZEX, 300);
    CHECK(pCaller->EvaluateStatusTrigger(rangeNear) == FALSE);
    CAITrigger rangeFar(0x4018, target, 2);
    rangeFar.m_specific2 = 3;
    CHECK(pCaller->EvaluateStatusTrigger(rangeFar) == TRUE);
    // An unknown mode answers FALSE.
    rangeFar.m_specific2 = 0;
    CHECK(pCaller->EvaluateStatusTrigger(rangeFar) == FALSE);

    // See: in the same area and on the caller's own point the line of
    // sight holds at once (CGameArea::CheckLOS's start == goal); another
    // area or beyond the visual range is not seen.
    CAITrigger see(0x401C, target, 0);
    pTarget->m_pos = pCaller->m_pos;
    CHECK(pCaller->EvaluateStatusTrigger(see) == TRUE);
    CHECK(pCaller->m_lSeen.GetInstance() == pTarget->m_id);
    pTarget->m_pos = CPoint(400 + 1000 * CPathSearch::GRID_SQUARE_SIZEX, 300);
    CHECK(pCaller->EvaluateStatusTrigger(see) == FALSE);
    pTarget->m_pos = pCaller->m_pos;
    pTarget->m_pArea = NULL;
    CHECK(pCaller->EvaluateStatusTrigger(see) == FALSE);

    // HP, HPGT, HPLT read a sprite's base hit points; not a sprite: FALSE.
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_baseStats.m_hitPoints = 10;
    CAIObjectType sprite(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    CAITrigger hpgt(0x4011, sprite, 5);
    CHECK(pCaller->EvaluateStatusTrigger(hpgt) == TRUE);
    CAITrigger hpgtHigh(0x4011, sprite, 10);
    CHECK(pCaller->EvaluateStatusTrigger(hpgtHigh) == FALSE);
    CAITrigger hp(0x4010, sprite, 10);
    CHECK(pCaller->EvaluateStatusTrigger(hp) == TRUE);
    CAITrigger hplt(0x4012, sprite, 10);
    CHECK(pCaller->EvaluateStatusTrigger(hplt) == FALSE);
    CAITrigger hpOther(0x4011, target, 0);
    CHECK(pCaller->EvaluateStatusTrigger(hpOther) == FALSE);

    pCaller->m_pArea = NULL;
}

TEST_CASE("ai: the sprite's dispatcher runs its own actions and leaves the rest to the base", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameSprite* pSprite = MakeSprite();

    // NoAction (0): done, and counted (m_noActionCount, 0x72901E).
    LONG nNoAction = pSprite->m_noActionCount;
    pSprite->SetCurrAction(CAIAction::NULL_ACTION);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_noActionCount == nNoAction + 1);
    CHECK(pSprite->m_nLastActionReturn == CGameAIBase::ACTION_DONE);

    // Continue (36) and CutSceneId (127): done.
    CAIAction action;
    action.m_actionID = 36;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    action.m_actionID = 127;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);

    // SetDialogueRange(300) (256): the range Dialogue reads (+0x7106).
    action.m_actionID = 256;
    action.m_specificID = 300;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->field_7106 == 300);

    // SetSavedLocationPoint (249) and MakeUnselectable (182).
    action.m_actionID = 249;
    action.m_specificID = 120;
    action.m_specificID2 = 240;
    action.m_specificID3 = 6;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetBaseStats()->field_2E4 == 120);
    CHECK(pSprite->GetBaseStats()->field_2E6 == 240);
    CHECK(pSprite->GetBaseStats()->field_2E8 == 6);
    action.m_actionID = 182;
    action.m_specificID = 2;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_nUnselectableCounter == 30);
    pSprite->m_nUnselectableCounter = 0;

    // Attack (3) is the sprite's but not ported yet: the case answers
    // ACTION_ERROR (its TODO), it does not reach the base's error case.
    action.m_actionID = CAIAction::ATTACK;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // SetGlobal (30) is the base's: the byte table sends it on.
    CAIAction setGlobal;
    setGlobal.m_actionID = 30;
    setGlobal.m_string1 = "GLOBALai_sprite_var";
    setGlobal.m_specificID = 7;
    pSprite->SetCurrAction(setGlobal);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("AI_SPRITE_VAR"));
    REQUIRE(pVariable != NULL);
    CHECK(pVariable->m_intValue == 7);

    // An id above the table's 0x142 goes to the base as well (its own
    // error case for an unknown id).
    action.m_actionID = 400;
    pSprite->SetCurrAction(action);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    ClearMessages();
}

TEST_CASE("ai: ExecuteAction changes allegiance and general, ChangeClass changes nothing", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // ChangeEnemyAlly(255) (153): LOBYTE(m_specificID) lands in m_nEnemyAlly
    // of all three types - GetAIType's m_typeAI, m_liveTypeAI and, unlike the
    // later arms, the start copy taken from m_startTypeAI (0x44EF89).
    CAIAction changeEnemyAlly;
    changeEnemyAlly.m_actionID = 153;
    changeEnemyAlly.m_acteeID = actee;
    changeEnemyAlly.m_specificID = 255;
    pCaller->SetCurrAction(changeEnemyAlly);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetAIType().GetEnemyAlly() == 255);
    CHECK(pSprite->m_liveTypeAI.GetEnemyAlly() == 255);
    CHECK(pSprite->m_startTypeAI.GetEnemyAlly() == 255);

    // The sprite update is BROADCAST_OTHERS-routed without force-passthrough,
    // so the single-player game (no other players to sync) drops it in
    // CMessageHandler::Broadcast (bSendMessageToSelf FALSE): the queue stays
    // empty. The EA change above ran inside the same conditional, so the
    // message path was taken.
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == 0);

    // ChangeGeneral(4) (154): same shape, m_nGeneral on all three; the start
    // copy comes from m_liveTypeAI here, so it carries the EA written above
    // and overwrites a race only the start type had (0x44F1CC).
    pSprite->m_startTypeAI.SetRace(77);
    REQUIRE(pSprite->m_liveTypeAI.GetRace() != 77);
    CAIAction changeGeneral;
    changeGeneral.m_actionID = 154;
    changeGeneral.m_acteeID = actee;
    changeGeneral.m_specificID = 4;
    pCaller->SetCurrAction(changeGeneral);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetAIType().GetGeneral() == 4);
    CHECK(pSprite->m_liveTypeAI.GetGeneral() == 4);
    CHECK(pSprite->m_startTypeAI.GetGeneral() == 4);
    CHECK(pSprite->m_startTypeAI.GetEnemyAlly() == 255);
    CHECK(pSprite->m_liveTypeAI.GetEnemyAlly() == 255);
    CHECK(pSprite->m_startTypeAI.GetRace() == pSprite->m_liveTypeAI.GetRace());

    // ChangeClass (156): the original builds three CAIObjectType locals and
    // discards them without touching the sprite (0x44F55E) - the general
    // stays as ChangeGeneral left it.
    CAIAction changeClass;
    changeClass.m_actionID = 156;
    changeClass.m_acteeID = actee;
    changeClass.m_specificID = 99;
    pCaller->SetCurrAction(changeClass);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetAIType().GetGeneral() == 4);
    CHECK(pSprite->GetAIType().GetEnemyAlly() == 255);

    // A second ChangeEnemyAlly (200) follows whatever value the action
    // carries, proving the 255s above were written by the action rather
    // than coinciding with the fixture's stored types.
    changeEnemyAlly.m_specificID = 200;
    pCaller->SetCurrAction(changeEnemyAlly);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetAIType().GetEnemyAlly() == 200);
    CHECK(pSprite->m_liveTypeAI.GetEnemyAlly() == 200);
    CHECK(pSprite->m_startTypeAI.GetEnemyAlly() == 200);

    ClearMessages();
}

TEST_CASE("ai: sprite internal, morale, TurnAMT and SetExtendedNight actions", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameSprite* pSprite = MakeSprite();
    CGameSprite* pTarget = MakeSprite();
    CAIObjectType target(0, 0, 0, 0, 0, 0, 0, 0, pTarget->m_id, 0, 0);

    // SetInternal/IncrementInternal (239/240): five signed words at CRE
    // offset 0x298, selected by specificID and set/incremented by specificID2.
    SHORT* pInternals = reinterpret_cast<SHORT*>(pTarget->m_baseStats.field_295 + 3);
    pInternals[2] = 100;
    pSprite->SetCurrAction(CAIAction(239, target, 2, -1234, 0));
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pInternals[2] == -1234);
    pSprite->SetCurrAction(CAIAction(240, target, 2, 123, 0));
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pInternals[2] == -1111);
    for (LONG nIndex : { -1, 5 }) {
        pSprite->SetCurrAction(CAIAction(240, target, nIndex, 5000, 0));
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
        CHECK(pInternals[2] == -1111);
    }
    pInternals[4] = SHRT_MAX;
    pSprite->SetCurrAction(CAIAction(240, target, 4, 1, 0));
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pInternals[4] == SHRT_MIN);

    // MoraleSet/MoraleInc/MoraleDec (102-104) queue a freshly decoded morale
    // effect. The acting sprite is the message source and the shared target is
    // the message target.
    const int moraleActions[] = { 102, 103, 104 };
    const int expectedAmounts[] = { 7, 7, -7 };
    const DWORD expectedFlags[] = { 1, 0, 0 };
    for (int n = 0; n < 3; n++) {
        ClearMessages();
        pSprite->SetCurrAction(CAIAction(moraleActions[n], target, 7, 0, 0));
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);

        CMessageAddEffect* pAdd = NULL;
        int nAdd = 0;
        CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
        POSITION pos = handler.m_messageList.GetHeadPosition();
        while (pos != NULL) {
            auto* pMessage = dynamic_cast<CMessageAddEffect*>(handler.m_messageList.GetNext(pos));
            if (pMessage != NULL) {
                pAdd = pMessage;
                nAdd++;
            }
        }
        REQUIRE(nAdd == 1);
        REQUIRE(pAdd != NULL);
        CHECK(pAdd->m_sourceId == pSprite->m_id);
        CHECK(pAdd->m_targetId == pTarget->m_id);
        REQUIRE(pAdd->m_effect != NULL);
        CHECK(dynamic_cast<CGameEffectMorale*>(pAdd->m_effect) != NULL);
        std::unique_ptr<ITEM_EFFECT> pEffect(pAdd->m_effect->GetItemEffect());
        REQUIRE(pEffect != NULL);
        CHECK(pEffect->effectID == CGAMEEFFECT_MORALE);
        CHECK(pEffect->effectAmount == expectedAmounts[n]);
        CHECK(pEffect->dwFlags == expectedFlags[n]);
        CHECK(pEffect->durationType == 1);
    }

    // TurnAMT (277) is a relative Face: the turn is based on the pending
    // direction (m_nNewDirection, +0x537E), not the drawn one, so 14 + 5
    // wraps to 3. It sends the sprite update only for a placed sprite; both
    // the unplaced and placed paths set the requested direction and finish
    // at once.
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    pSprite->m_pArea = NULL;
    pSprite->m_pos = CPoint(-1, -1);
    pSprite->m_nNewDirection = 14;
    pSprite->m_nDirection = 13;
    pSprite->m_walkBackwards = FALSE;
    CAIAction turn;
    turn.m_actionID = 277;
    turn.m_specificID = 5;
    pSprite->SetCurrAction(turn);
    ClearMessages();
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_nNewDirection == 3);
    CHECK(pSprite->m_deltaDirection == 1);

    pSprite->m_pos = CPoint(100, 100);
    pSprite->m_nNewDirection = 3;
    pSprite->m_nDirection = 12;
    turn.m_specificID = 2;
    pSprite->SetCurrAction(turn);
    ClearMessages();
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_nNewDirection == 5);
    // The placed path also constructs and broadcasts CMessageSpriteUpdate;
    // BROADCAST_OTHERS with force FALSE hands it to Broadcast, which deletes
    // it rather than queueing it for self, so there is nothing local to read.

    // SetExtendedNight (301) sets area-type bit 0x40. This sprite-specific
    // case clears the whole area type for FALSE (the base's version only
    // clears the bit), and reports its update through CMessageSetAreaType.
    auto* const area = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(area, 0, sizeof(CGameArea));
    pSprite->m_pArea = reinterpret_cast<CGameArea*>(area);
    pSprite->m_pArea->m_header.m_areaType = 0x81;
    CAIAction extendedNight;
    extendedNight.m_actionID = 301;
    extendedNight.m_specificID = 1;
    pSprite->SetCurrAction(extendedNight);
    ClearMessages();
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_pArea->m_header.m_areaType == 0xC1);
    // CMessageSetAreaType is BROADCAST_FORCED_OTHERS and is likewise deleted
    // by Broadcast, so this exercises its construction without a local copy.
    extendedNight.m_specificID = 0;
    pSprite->SetCurrAction(extendedNight);
    ClearMessages();
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_pArea->m_header.m_areaType == 0);

    pSprite->m_pArea = NULL;
    ::operator delete(area);
    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    ClearMessages();
}

TEST_CASE("ai: a named creature is found by its name in any case", "[ai]")
{
    // AddKey (0x550D10) stores the name upper case without spaces and hashes
    // that, as FindKey looks it up (Decode's "Hedron" in a cut scene).
    CNamedCreatureVariableHash hash(64);
    CVariable hedron;
    hedron.SetName(CString("Hedron"));
    hedron.m_intValue = 42;
    CHECK(hash.AddKey(hedron) == FALSE);
    CVariable* pFound = hash.FindKey(CString("HEDRON"));
    REQUIRE(pFound != NULL);
    CHECK(pFound->m_intValue == 42);
    CHECK(pFound->GetName() == "HEDRON");
    REQUIRE(hash.FindKey(CString("hedron")) != NULL);

    // The same name again updates the entry and answers TRUE.
    CVariable again;
    again.SetName(CString("He dron"));
    again.m_intValue = 43;
    CHECK(hash.AddKey(again) == TRUE);
    CHECK(hash.FindKey(CString("HEDRON"))->m_intValue == 43);
}

TEST_CASE("ai: CheckLimits keeps the morale recovery time and caps the morale break at 20", "[ai]")
{
    // 0x446C39-0x446C9E: XP, gold and the recovery time are clamped to
    // 0xFFFFFFFF unsigned (no change), the morale break to 20 at most.
    CDerivedStats stats;
    stats.m_nXP = 1250;
    stats.m_nGold = 87;
    stats.m_nXPValue = 15;
    stats.m_nMoraleRecoveryTime = 60;
    stats.m_nMoraleBreak = 5;
    stats.m_nReputation = 5;
    stats.CheckLimits();
    CHECK(stats.m_nXP == 1250);
    CHECK(stats.m_nGold == 87);
    CHECK(stats.m_nXPValue == 15);
    CHECK(stats.m_nMoraleRecoveryTime == 60);
    CHECK(stats.m_nMoraleBreak == 5);
    CHECK(stats.m_nReputation == 10);
    stats.m_nMoraleBreak = 25;
    stats.CheckLimits();
    CHECK(stats.m_nMoraleBreak == 20);
    // The reputation compares unsigned: a negative one becomes 200.
    stats.m_nReputation = -5;
    stats.CheckLimits();
    CHECK(stats.m_nReputation == 200);
}

TEST_CASE("ai: See at a distance, a dead sprite, Range's equal mode, HP's edges and Class, General and Gender on Myself", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    auto* areaStorage = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(areaStorage, 0, sizeof(CGameArea));
    CGameArea* pArea = reinterpret_cast<CGameArea*>(areaStorage);
    // A 100 by 100 search grid open everywhere: every square's dynamic cost
    // has bit 0x01, which GetLOSCost reads as the visible terrain table's
    // entry 8 (5, passable). The visibility range is 14 squares.
    pArea->m_search.m_GridSquareDimensions = CSize(100, 100);
    std::vector<BYTE> dynamicCost(100 * 100, 0x01);
    pArea->m_search.m_pDynamicCost = dynamicCost.data();
    pArea->m_visibility.field_E = 14;

    CGameAIBase* pCaller = AddAIObject();
    pCaller->m_pArea = pArea;
    pCaller->m_pos = CPoint(400, 300);

    // See: a distinct target four search squares east is seen across open
    // squares, and not across a wall square between them (dynamic cost
    // 0x80: the visible terrain table's entry 0, impassable).
    CGameAIBase* pTarget = AddAIObject();
    pTarget->m_pArea = pArea;
    pTarget->m_pos = CPoint(400 + 4 * CPathSearch::GRID_SQUARE_SIZEX, 300);
    CAIObjectType target(0, 0, 0, 0, 0, 0, 0, 0, pTarget->m_id, 0, 0);
    CAITrigger see(0x401C, target, 0);
    CHECK(pCaller->EvaluateStatusTrigger(see) == TRUE);
    CHECK(pCaller->m_lSeen.GetInstance() == pTarget->m_id);
    const LONG wall = (300 / CPathSearch::GRID_SQUARE_SIZEY) * 100 + 400 / CPathSearch::GRID_SQUARE_SIZEX + 2;
    dynamicCost[wall] = 0x80;
    CHECK(pCaller->EvaluateStatusTrigger(see) == FALSE);
    dynamicCost[wall] = 0x01;

    // Range(object, 2, equal): exactly (2 + 1)^2 squared grid squares.
    CAITrigger rangeEqual(0x4018, target, 2);
    rangeEqual.m_specific2 = 1;
    pTarget->m_pos = CPoint(400 + 3 * CPathSearch::GRID_SQUARE_SIZEX, 300);
    CHECK(pCaller->EvaluateStatusTrigger(rangeEqual) == TRUE);
    pTarget->m_pos = CPoint(400 + 2 * CPathSearch::GRID_SQUARE_SIZEX, 300);
    CHECK(pCaller->EvaluateStatusTrigger(rangeEqual) == FALSE);

    // A dead sprite is not seen unless the trigger asks for the dead too.
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_pArea = pArea;
    pSprite->m_pos = pCaller->m_pos;
    CAIObjectType sprite(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    CAITrigger seeLiving(0x401C, sprite, 0);
    CAITrigger seeDead(0x401C, sprite, 1);
    const DWORD state = pSprite->m_derivedStats.m_generalState;
    CHECK(pCaller->EvaluateStatusTrigger(seeLiving) == TRUE);
    pSprite->m_derivedStats.m_generalState = state | STATE_DEAD;
    CHECK(pCaller->EvaluateStatusTrigger(seeLiving) == FALSE);
    CHECK(pCaller->EvaluateStatusTrigger(seeDead) == TRUE);
    pSprite->m_derivedStats.m_generalState = state;
    pSprite->m_pArea = NULL;

    // HP(..., 9) and HPLT(..., 11) on ten hit points.
    pSprite->m_baseStats.m_hitPoints = 10;
    CAITrigger hpNine(0x4010, sprite, 9);
    CHECK(pCaller->EvaluateStatusTrigger(hpNine) == FALSE);
    CAITrigger hpltEleven(0x4012, sprite, 11);
    CHECK(pCaller->EvaluateStatusTrigger(hpltEleven) == TRUE);

    // Class, General and Gender read the caller's own object type through
    // Myself. Class is a mask bit (class n is bit n - 1).
    pCaller->m_typeAI.m_nClassMask = 1 << (5 - 1);
    pCaller->m_typeAI.m_nGeneral = 1;
    pCaller->m_typeAI.m_nGender = 2;
    CAIObjectType myself(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    myself.m_SpecialCase[0] = 1;
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x400C, myself, 5)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x400C, myself, 3)) == FALSE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x400E, myself, 1)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x400E, myself, 2)) == FALSE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x404D, myself, 2)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x404D, myself, 1)) == FALSE);

    pCaller->m_pArea = NULL;
    pTarget->m_pArea = NULL;
    pArea->m_search.m_pDynamicCost = NULL;
    ClearMessages();
}

TEST_CASE("ai: CheckStat, CheckStatGT and CheckStatLT compare the cause's derived stat", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    CAIObjectType sprite(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // Int1 is the stat (STAT_STR, 36), the value its specific.
    const SHORT saved = pSprite->m_derivedStats.m_nSTR;
    pSprite->m_derivedStats.m_nSTR = 12;
    CAITrigger check(0x4044, sprite, 12);
    check.m_specific2 = STAT_STR;
    CHECK(pCaller->EvaluateStatusTrigger(check) == TRUE);
    check.m_specificID = 11;
    CHECK(pCaller->EvaluateStatusTrigger(check) == FALSE);

    CAITrigger greater(0x4045, sprite, 11);
    greater.m_specific2 = STAT_STR;
    CHECK(pCaller->EvaluateStatusTrigger(greater) == TRUE);
    greater.m_specificID = 12;
    CHECK(pCaller->EvaluateStatusTrigger(greater) == FALSE);

    CAITrigger less(0x4046, sprite, 13);
    less.m_specific2 = STAT_STR;
    CHECK(pCaller->EvaluateStatusTrigger(less) == TRUE);
    less.m_specificID = 12;
    CHECK(pCaller->EvaluateStatusTrigger(less) == FALSE);

    // No sprite (the caller is a plain AI object): FALSE.
    CAIObjectType object(0, 0, 0, 0, 0, 0, 0, 0, pCaller->m_id, 0, 0);
    CAITrigger other(0x4045, object, -100);
    other.m_specific2 = STAT_STR;
    CHECK(pCaller->EvaluateStatusTrigger(other) == FALSE);

    pSprite->m_derivedStats.m_nSTR = saved;
    ClearMessages();
}

// sub_763200's Resist Poison (feat 50, 0x7636E9) and Snake Blood (feat 59,
// 0x7637AE): the race, at character level 1.
TEST_CASE("ai: Resist Poison and Snake Blood need their race at character level 1", "[ai][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_typeAI.m_nClassMask = 1;
    memset(pSprite->m_baseStats.m_classLevels, 0, sizeof(pSprite->m_baseStats.m_classLevels));
    pSprite->m_baseStats.m_classLevels[0] = 1;

    // Resist Poison: a half-orc, or a dwarf of subrace 2.
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HALF_ORC;
    pSprite->m_typeAI.m_nSubRace = 0;
    CHECK(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_DWARF;
    CHECK_FALSE(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));
    pSprite->m_typeAI.m_nSubRace = 2;
    CHECK(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HUMAN;
    CHECK_FALSE(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));

    // Snake Blood: a human.
    CHECK(pSprite->sub_763200(CGAMESPRITE_FEAT_SNAKE_BLOOD, 0));
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HALF_ORC;
    CHECK_FALSE(pSprite->sub_763200(CGAMESPRITE_FEAT_SNAKE_BLOOD, 0));

    // Character level 2 (the classes of the class mask): neither.
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HUMAN;
    pSprite->m_baseStats.m_classLevels[0] = 2;
    CHECK_FALSE(pSprite->sub_763200(CGAMESPRITE_FEAT_SNAKE_BLOOD, 0));
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HALF_ORC;
    CHECK_FALSE(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));
    // A level in a class outside the mask does not count.
    pSprite->m_baseStats.m_classLevels[0] = 1;
    pSprite->m_baseStats.m_classLevels[3] = 4;
    CHECK(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));
    // No class: neither.
    pSprite->m_typeAI.m_nClassMask = 0;
    CHECK_FALSE(pSprite->sub_763200(CGAMESPRITE_FEAT_RESIST_POISON, 0));
}

TEST_CASE("ai: the party specifiers decode to the party member the game picks", "[ai][data]")
{
    REQUIRE_ENGINE();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    // Alive, level 3, ten of twenty hit points: every pick finds it.
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;
    pSprite->m_derivedStats.m_nLevel = 3;
    pSprite->m_derivedStats.m_nMaxHitPoints = 20;
    pSprite->m_baseStats.m_hitPoints = 10;
    // StrongestOfMale counts only males (gender 1).
    pSprite->m_typeAI.m_nGender = 1;

    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait = pGame->m_characterPortraits[0];

    // No party: NOONE.
    pGame->m_nCharacters = 0;
    CAIObjectType nobody(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    nobody.m_SpecialCase[0] = 4;
    nobody.Decode(pCaller);
    CHECK(nobody.Equal(CAIObjectType::NOONE));

    pGame->m_nCharacters = 1;
    pGame->m_characterPortraits[0] = pSprite->m_id;
    // WeakestOf, StrongestOf, MostDamagedOf, LeastDamagedOf,
    // StrongestOfMale, WorstAC, BestAC.
    const BYTE specials[] = { 4, 5, 6, 7, 28, 47, 48 };
    for (BYTE special : specials) {
        INFO("specifier " << static_cast<int>(special));
        CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
        type.m_SpecialCase[0] = special;
        type.Decode(pCaller);
        CHECK(type.m_nInstance == pSprite->m_id);
    }

    // A woman is not the strongest male.
    pSprite->m_typeAI.m_nGender = 2;
    CAIObjectType male(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    male.m_SpecialCase[0] = 28;
    male.Decode(pCaller);
    CHECK(male.Equal(CAIObjectType::NOONE));

    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait;
}

// NearestPC (special 52, 0x40C46C): the nearest live good party member by
// the squared distance to the caller, y not scaled; dead and evil members
// do not count, and none is NOONE.
TEST_CASE("ai: NearestPC decodes to the nearest live good party member", "[ai][data]")
{
    REQUIRE_ENGINE();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameAIBase* pCaller = AddAIObject();
    pCaller->m_pos = CPoint(1000, 1000);
    CGameSprite* pNear = MakeSprite();
    CGameSprite* pFar = MakeSprite();
    for (CGameSprite* pSprite : { pNear, pFar }) {
        pSprite->m_derivedStats.m_generalState &= ~(STATE_DEAD | STATE_INVISIBLE);
        pSprite->m_typeAI.m_nEnemyAlly = 2; // PC
    }
    pNear->m_pos = CPoint(1000, 1100);
    pFar->m_pos = CPoint(1150, 1000);

    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait0 = pGame->m_characterPortraits[0];
    const LONG portrait1 = pGame->m_characterPortraits[1];
    pGame->m_nCharacters = 2;
    pGame->m_characterPortraits[0] = pFar->m_id;
    pGame->m_characterPortraits[1] = pNear->m_id;

    auto decode = [&]() {
        CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
        type.m_SpecialCase[0] = 52;
        type.Decode(pCaller);
        return type;
    };

    CHECK(decode().m_nInstance == pNear->m_id);

    // A dead one, or one of the other side, is not a PC to find.
    pNear->m_derivedStats.m_generalState |= STATE_DEAD;
    CHECK(decode().m_nInstance == pFar->m_id);
    pNear->m_derivedStats.m_generalState &= ~STATE_DEAD;
    pNear->m_typeAI.m_nEnemyAlly = 255;
    CHECK(decode().m_nInstance == pFar->m_id);
    pFar->m_typeAI.m_nEnemyAlly = 255;
    CHECK(decode().Equal(CAIObjectType::NOONE));

    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait0;
    pGame->m_characterPortraits[1] = portrait1;
}

// A creature's death on the host (0x6F5330): its <script name>_DEAD, its
// KILL_<race>_CNT and the creature file's death and kill variables. Only
// the host counts.
TEST_CASE("ai: a death sets the creature's death variables on the host", "[ai][data]")
{
    REQUIRE_ENGINE();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_bBumped = FALSE;
    pSprite->m_bVisibleMonster = FALSE;
    pSprite->m_bInfravisionOn = FALSE;
    std::memset(pSprite->m_scriptName, 0, sizeof(pSprite->m_scriptName));
    std::memcpy(pSprite->m_scriptName, "AITESTGOB", 9);
    pSprite->m_typeAI.m_nRace = 155; // goblin
    pSprite->m_baseStats.field_295[0] = 1;
    pSprite->m_baseStats.field_295[1] = 1;
    std::memset(&pSprite->m_baseStats.field_295[0xD], 0, 64);
    std::memcpy(&pSprite->m_baseStats.field_295[0xD], "  AITEST_DEATHVAR ", 18);
    std::memcpy(&pSprite->m_baseStats.field_295[0x2D], "aitest_a:aitest_b", 17);

    auto value = [&](const char* name) -> LONG {
        CVariable* pVariable = pGame->GetVariables()->FindKey(CString(name));
        return pVariable != NULL ? pVariable->m_intValue : -1;
    };

    CHECK(CGameSprite::GetRaceString(155) == CString("GOBLIN"));
    CHECK(CGameSprite::GetRaceString(114) == CString(""));

    const BOOLEAN bHost = g_pChitin->cNetwork.m_bIsHost;
    g_pChitin->cNetwork.m_bIsHost = FALSE;
    pSprite->sub_6F5330();
    CHECK(value("AITESTGOB_DEAD") == -1);

    g_pChitin->cNetwork.m_bIsHost = TRUE;
    const LONG kills = value("KILL_GOBLIN_CNT");
    pSprite->sub_6F5330();
    CHECK(value("AITESTGOB_DEAD") == 1);
    CHECK(value("KILL_GOBLIN_CNT") == (kills == -1 ? 1 : kills + 1));
    CHECK(value("AITEST_DEATHVAR") == 1);
    CHECK(value("AITEST_A") == 1);
    CHECK(value("AITEST_B") == 1);

    // A second death counts the kills again; the death variables stay 1.
    pSprite->sub_6F5330();
    CHECK(value("AITESTGOB_DEAD") == 1);
    CHECK(value("AITEST_DEATHVAR") == 1);
    CHECK(value("AITEST_A") == 2);
    CHECK(value("AITEST_B") == 2);

    g_pChitin->cNetwork.m_bIsHost = bHost;
    ClearMessages();
}

TEST_CASE("ai: closing AR5004's door on a creature tells the creature to move and closes again", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pClicker = AddAIObject();
    CGameDoor* pDoor = MakeDoor(0x1);
    CGameArea* pArea = pDoor->m_pArea;
    std::memcpy(pArea->m_header.m_areaName, "AR5004", 6);
    pArea->m_pGame = g_pBaldurChitin->GetObjectGame();
    // The door's closed footprint is one search square, occupied (dynamic
    // cost bit 0x02): CanToggleDoor refuses.
    pArea->m_search.m_GridSquareDimensions = CSize(100, 100);
    std::vector<BYTE> dynamicCost(100 * 100, 0);
    pArea->m_search.m_pDynamicCost = dynamicCost.data();
    CPoint closed(25, 25);
    dynamicCost[25 * 100 + 25] = 0x02;
    pDoor->m_pClosedSearch = &closed;
    pDoor->m_nClosedSearch = 1;
    CAIObjectType cause(0, 0, 0, 0, 0, 0, 0, 0, pClicker->m_id, 0, 0);

    // Nobody found (the door is not in the object array): nothing is said
    // and the door stays open.
    pDoor->ToggleDoor(cause, FALSE);
    CHECK((pDoor->m_dwFlags & 0x1) != 0);
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    CHECK(handler.m_messageList.IsEmpty());

    // The creature on that square, next to the door in the front list (an
    // AI object in the array stands in for the door there).
    CGameAIBase* pStandIn = AddAIObject();
    pDoor->m_id = pStandIn->m_id;
    new (&pArea->m_lVertSort) CTypedPtrList<CPtrList, int*>();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_pos = CPoint(25 * CPathSearch::GRID_SQUARE_SIZEX, 25 * CPathSearch::GRID_SQUARE_SIZEY);
    pSprite->m_active = TRUE;
    pSprite->m_activeAI = TRUE;
    pSprite->m_activeImprisonment = TRUE;
    // Its personal space comes from its animation (CHARBASE has none): a
    // human male fighter, as character creation picks (0x6000).
    pSprite->m_animation.SetAnimationType(0x6000, pSprite->m_baseStats.m_colors, 0);
    pStandIn->m_posVertList = pArea->m_lVertSort.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(pStandIn->m_id)));
    pSprite->m_posVertList = pArea->m_lVertSort.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->m_id)));

    pDoor->ToggleDoor(cause, FALSE);
    CHECK((pDoor->m_dwFlags & 0x1) != 0);
    REQUIRE(handler.m_messageList.GetCount() == 2);
    auto* move = dynamic_cast<CMessageAddAction*>(handler.m_messageList.GetHead());
    REQUIRE(move != nullptr);
    CHECK(move->m_action.m_actionID == 292);
    CHECK(move->m_action.m_acteeID.Equal(pSprite->GetAIType()));
    CHECK(move->m_sourceId == pDoor->m_id);
    CHECK(move->m_targetId == pDoor->m_id);
    auto* close = dynamic_cast<CMessageAddAction*>(handler.m_messageList.GetTail());
    REQUIRE(close != nullptr);
    CHECK(close->m_action.m_actionID == 144);
    CHECK(close->m_action.m_acteeID.Equal(pDoor->m_typeAI));
    CHECK(close->m_sourceId == pDoor->m_id);
    CHECK(close->m_targetId == pClicker->m_id);

    pArea->m_lVertSort.RemoveAll();
    pStandIn->m_posVertList = NULL;
    pSprite->m_posVertList = NULL;
    pArea->m_search.m_pDynamicCost = NULL;
    ClearMessages();
}

TEST_CASE("action: the copy constructor copies all three specific ids", "[ai]")
{
    // StartRandomTimer(18, 20, 30) as a script's response carries it:
    // CAIAction's copy constructor (0x409090) copies +0xBE from +0xBE.
    CAIAction action(232, CAIObjectType::NOONE, 18, 20, 30);
    action.m_string1 = "one";
    action.m_string2 = "two";
    action.m_dest = CPoint(7, 9);
    const CAIAction copy(action);
    CHECK(copy.m_actionID == 232);
    CHECK(copy.m_specificID == 18);
    CHECK(copy.m_specificID2 == 20);
    CHECK(copy.m_specificID3 == 30);
    CHECK(copy.m_string1 == "one");
    CHECK(copy.m_string2 == "two");
    CHECK(copy.m_dest == CPoint(7, 9));
}

TEST_CASE("ai: ExecuteAction 160 ApplySpell casts SPWI105 on the sprite and rejects unresolved targets", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();

    // The case 160 arm resolves the actee instance through
    // GetTargetShareAIType after GetTargetShareType comes up empty, then
    // ApplySpell builds one CMessageAddEffect per effect of the spell's
    // ability 0. SPWI105's ability 0 has 13 effects: the first targets the
    // caster itself, the other twelve the resolved sprite.
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    CAIAction applySpell;
    applySpell.m_actionID = 160;
    applySpell.m_acteeID = actee;
    applySpell.m_string1 = "SPWI105";
    pCaller->SetCurrAction(applySpell);

    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    CHECK(handler.m_messageList.GetCount() == 13);

    CMessageAddEffect* pSelf = NULL;
    int nToSprite = 0;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        auto* pAdd = dynamic_cast<CMessageAddEffect*>(handler.m_messageList.GetNext(pos));
        if (pAdd == NULL) {
            continue;
        }
        CHECK(pAdd->m_sourceId == pCaller->m_id);
        CHECK(pAdd->m_effect->m_source == pCaller->m_pos);
        if (pAdd->m_targetId == pSprite->m_id) {
            nToSprite++;
            CHECK(pAdd->m_effect->m_targetType == 2);
            CHECK(pAdd->m_effect->m_target == pSprite->GetPos());
        } else {
            pSelf = pAdd;
            CHECK(pAdd->m_targetId == pCaller->m_id);
            CHECK(pAdd->m_effect->m_targetType == 1);
            CHECK((pAdd->m_effect->m_flags & 0x2) != 0); // the self effect gets the persistent flag
        }
    }
    CHECK(nToSprite == 12);
    CHECK(pSelf != NULL);
    ClearMessages();

    // Rejected: a NOONE actee never resolves a target, so the arm passes
    // NULL and ApplySpell bails before the spell's effects are read.
    CAIAction noTarget = applySpell;
    noTarget.m_acteeID = CAIObjectType::NOONE;
    pCaller->SetCurrAction(noTarget);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(handler.m_messageList.GetCount() == 0);

    // Rejected: the resref names no spell in this install, so the
    // constructed CSpell carries no data and CheckAppropriateTarget fails.
    CAIAction noSpell = applySpell;
    noSpell.m_string1 = "SPWI123";
    pCaller->SetCurrAction(noSpell);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(handler.m_messageList.GetCount() == 0);
    ClearMessages();
}

// CreateCreature (0x4608A0) through the case 7 and 228 arms of ExecuteAction,
// on AR3000 loaded the way a travel loads it (CInfGame::LoadArea 0x5A24D0):
// the area's real search map answers the case 7 nearest-open-square search,
// and the ice trolls' own resref (30ICETRL) builds sprites with live
// animations.
TEST_CASE("ai: ExecuteAction 7 and 228 CreateCreature place and register a new sprite", "[ai][data][ar3000]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear (tests/test_effects.cpp's AR3000 pump).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea = pGame->LoadArea(CString("AR3000"), 0, FALSE, FALSE);
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();
    pCaller->m_pArea = pArea;
    pCaller->m_pos = CPoint(7 * CPathSearch::GRID_SQUARE_SIZEX,
        7 * CPathSearch::GRID_SQUARE_SIZEY);
    const BYTE* pTerrain = pCaller->GetTerrainTable();

    auto queuedCount = [pArea] {
        return pArea->m_lVertSortAdd.GetCount() + pArea->m_lVertSortBackAdd.GetCount()
            + pArea->m_lVertSortFlightAdd.GetCount();
    };
    auto namedId = [pArea](const char* name) -> LONG {
        CVariable* pVariable = pArea->m_namedCreatures.FindKey(CString(name));
        return pVariable != NULL ? pVariable->m_intValue : CGameObjectArray::INVALID_INDEX;
    };
    auto spriteById = [pGame](LONG id) -> CGameSprite* {
        CGameObject* pObject = NULL;
        if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE)
            != CGameObjectArray::SUCCESS) {
            return NULL;
        }
        pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
            return NULL;
        }
        return static_cast<CGameSprite*>(pObject);
    };

    // Rejected: the resref names no creature in this install, so the key
    // table lookup fails before any sprite is built.
    int nQueuedBefore = static_cast<int>(queuedCount());
    CAIAction noCreature;
    noCreature.m_actionID = 7;
    noCreature.m_string1 = "NOSUCHCR";
    noCreature.m_string2 = "NOSUCHNM";
    noCreature.m_dest = CPoint(100, 100);
    pCaller->SetCurrAction(noCreature);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pArea->m_namedCreatures.FindKey(CString("NOSUCHNM")) == NULL);
    CHECK(queuedCount() == nQueuedBefore);
    ClearMessages();

    // Case 228 places on the exact square containing the destination point:
    // the mid-square pixel (10*16+5, 10*12+3) divides back to square (10, 10).
    CAIAction place;
    place.m_actionID = 228;
    place.m_string1 = "30ICETRL";
    place.m_string2 = "TROLL228";
    place.m_specificID = 6; // the facing the sprite constructor keeps (& 0xF)
    place.m_dest = CPoint(10 * CPathSearch::GRID_SQUARE_SIZEX + 5,
        10 * CPathSearch::GRID_SQUARE_SIZEY + 3);
    pCaller->SetCurrAction(place);
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    LONG idPlaced = namedId("TROLL228");
    REQUIRE(idPlaced != CGameObjectArray::INVALID_INDEX);
    CGameSprite* pPlaced = spriteById(idPlaced);
    REQUIRE(pPlaced != NULL);
    CHECK(pPlaced->m_resRef == CResRef("30ICETRL"));
    CHECK(pPlaced->m_pArea == pArea);
    CHECK(pPlaced->GetPos() == CPoint(10 * CPathSearch::GRID_SQUARE_SIZEX, 10 * CPathSearch::GRID_SQUARE_SIZEY));
    // Even facing: stored as 6 & 0xF by the constructor and untouched by the
    // head-turn mask at the end of SetSequence (see the case 7 check below).
    CHECK(pPlaced->m_nDirection == 6);
    CHECK(CString(reinterpret_cast<const char*>(pPlaced->GetScriptName())) == "TROLL228");

    // Case 7 searches for the nearest open square around the point, with the
    // new sprite's personal space and the caller's terrain table. A preview
    // sprite built from the same CRE gives that personal space, and the
    // oracle run before the action sees the search map exactly as the action
    // will: nothing between them adds to or removes from the map (the placed
    // troll above has already marked its own square).
    CCreatureFile previewFile;
    previewFile.SetResRef(CResRef("30ICETRL"), TRUE, TRUE);
    BYTE* previewData = previewFile.GetData();
    REQUIRE(previewData != NULL);
    std::vector<BYTE> previewBytes(previewData, previewData + previewFile.GetDataSize());
    previewFile.Release();
    CGameSprite* pPreview = new CGameSprite(previewBytes.data(),
        static_cast<LONG>(previewBytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    BYTE nPersonalSpace = pPreview->m_animation.GetPersonalSpace();

    // A wall to aim at: the first impassable square inside the map's margin,
    // under the same cost rule the search uses (GetCost with bump checking).
    SHORT nTableIndex;
    CPoint blocked(-1, -1);
    CSize dims = pArea->m_search.m_GridSquareDimensions;
    for (int y = 4; y < dims.cy - 4 && blocked.x == -1; y++) {
        for (int x = 4; x < dims.cx - 4; x++) {
            if (pArea->m_search.GetCost(CPoint(x, y), pTerrain, nPersonalSpace, nTableIndex, TRUE)
                == CPathSearch::COST_IMPASSABLE) {
                blocked = CPoint(x, y);
                break;
            }
        }
    }
    REQUIRE(blocked.x != -1);

    CPoint expected = pArea->m_search.GetNearestOpenSquare(blocked, pTerrain, nPersonalSpace, -1);
    const bool movedOffWall = expected.x != blocked.x || expected.y != blocked.y;
    REQUIRE(movedOffWall);

    CAIAction search;
    search.m_actionID = 7;
    search.m_string1 = "30ICETRL";
    search.m_string2 = "TROLLOPEN";
    search.m_specificID = 13;
    search.m_dest = CPoint(blocked.x * CPathSearch::GRID_SQUARE_SIZEX + 1,
        blocked.y * CPathSearch::GRID_SQUARE_SIZEY + 1);
    pCaller->SetCurrAction(search);
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    LONG idSearched = namedId("TROLLOPEN");
    REQUIRE(idSearched != CGameObjectArray::INVALID_INDEX);
    CGameSprite* pSearched = spriteById(idSearched);
    REQUIRE(pSearched != NULL);
    CHECK(pSearched->m_resRef == CResRef("30ICETRL"));
    CHECK(pSearched->GetPos() == CPoint(expected.x * CPathSearch::GRID_SQUARE_SIZEX, expected.y * CPathSearch::GRID_SQUARE_SIZEY));
    // The facing (m_specificID = 13) survives CreateCreature and the sprite
    // constructor's own `facing & 0xF` store, but the constructor finishes with
    // SetSequence(SEQ_HEAD_TURN) (0x707230), which masks m_nDirection with
    // 0xFE before writing it back, so any odd facing rounds down to the next
    // even direction: 13 becomes 12. (Case 228's facing 6 is already even and
    // passes through unchanged.)
    CHECK(pSearched->m_nDirection == 12);
    CHECK(CString(reinterpret_cast<const char*>(pSearched->GetScriptName())) == "TROLLOPEN");

    // A (-1, -1) destination takes the caller's source position instead (the
    // case 7 search around square (7, 7), where the caller stands).
    CPoint expectedFromCaller = pArea->m_search.GetNearestOpenSquare(
        CPoint(pCaller->m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
            pCaller->m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
        pTerrain,
        nPersonalSpace,
        -1);
    CAIAction fromSource;
    fromSource.m_actionID = 7;
    fromSource.m_string1 = "30ICETRL";
    fromSource.m_string2 = "TROLLSRC";
    fromSource.m_dest = CPoint(-1, -1);
    pCaller->SetCurrAction(fromSource);
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    LONG idFromSource = namedId("TROLLSRC");
    REQUIRE(idFromSource != CGameObjectArray::INVALID_INDEX);
    CGameSprite* pFromSource = spriteById(idFromSource);
    REQUIRE(pFromSource != NULL);
    CHECK(pFromSource->GetPos() == CPoint(expectedFromCaller.x * CPathSearch::GRID_SQUARE_SIZEX, expectedFromCaller.y * CPathSearch::GRID_SQUARE_SIZEY));

    // All three sprites are on the area's sorted lists after the sort a
    // frame's AIUpdate runs.
    pArea->SortLists();
    int nFound = 0;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* objects : lists) {
        POSITION pos = objects->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(objects->GetNext(pos)));
            if (id == idPlaced || id == idSearched || id == idFromSource) {
                nFound++;
            }
        }
    }
    CHECK(nFound == 3);
    ClearMessages();
}

// CreateCreatureObject (0x460D70) through the case 227 arm of ExecuteAction,
// on the same loaded AR3000 as the CreateCreature tests above. The placement
// always runs GetNearestOpenSquare around the TARGET object's position (the
// action has no destination and there is no exact-square variant), and the
// sprite registers under the script name the CRE file itself carries — the
// action's second string is never copied.
TEST_CASE("ai: ExecuteAction 227 CreateCreatureObject places at the target and registers the CRE's own name", "[ai][data][ar3000]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear (tests/test_effects.cpp's AR3000 pump).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea = pGame->LoadArea(CString("AR3000"), 0, FALSE, FALSE);
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();
    pCaller->m_pArea = pArea;
    pCaller->m_pos = CPoint(7 * CPathSearch::GRID_SQUARE_SIZEX,
        7 * CPathSearch::GRID_SQUARE_SIZEY);
    const BYTE* pTerrain = pCaller->GetTerrainTable();

    // The actee: GetTargetShareType(TYPE_AIBASE) only hands back objects
    // whose type has the AIBASE bit, and a plain CGameAIBase (type 0) has
    // none — the target must be a sprite, the way the game always calls it.
    CGameSprite* pTarget = MakeSprite();
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pTarget->m_id, 0, 0);

    auto queuedCount = [pArea] {
        return pArea->m_lVertSortAdd.GetCount() + pArea->m_lVertSortBackAdd.GetCount()
            + pArea->m_lVertSortFlightAdd.GetCount();
    };
    auto spriteById = [pGame](LONG id) -> CGameSprite* {
        CGameObject* pObject = NULL;
        if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE)
            != CGameObjectArray::SUCCESS) {
            return NULL;
        }
        pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
            return NULL;
        }
        return static_cast<CGameSprite*>(pObject);
    };

    int nQueuedBefore = static_cast<int>(queuedCount());

    // Rejected: a NOONE actee never resolves a target, so the arm passes NULL
    // and the handler fails its first instruction before anything runs.
    CAIAction noTarget;
    noTarget.m_actionID = 227;
    noTarget.m_acteeID = CAIObjectType::NOONE;
    noTarget.m_string1 = "30ICETRL";
    pCaller->SetCurrAction(noTarget);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(queuedCount() == nQueuedBefore);
    ClearMessages();

    // Rejected: the resref names no creature in this install, so the key
    // table lookup fails before any sprite is built or name registered.
    CAIAction noCreature = noTarget;
    noCreature.m_acteeID = actee;
    noCreature.m_string1 = "NOSUCHCR";
    noCreature.m_string2 = "NOSUCHNM";
    pCaller->SetCurrAction(noCreature);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pArea->m_namedCreatures.FindKey(CString("NOSUCHNM")) == NULL);
    CHECK(queuedCount() == nQueuedBefore);
    ClearMessages();

    // A preview sprite from the same CRE gives the personal space the search
    // will use and reveals the script name the CRE carries: 227 registers
    // under that name, whatever it is (the action's second string is not
    // copied), so the expected name is read off the preview instead of being
    // pinned to a literal.
    CCreatureFile previewFile;
    previewFile.SetResRef(CResRef("30ICETRL"), TRUE, TRUE);
    BYTE* previewData = previewFile.GetData();
    REQUIRE(previewData != NULL);
    std::vector<BYTE> previewBytes(previewData, previewData + previewFile.GetDataSize());
    previewFile.Release();
    CGameSprite* pPreview = new CGameSprite(previewBytes.data(),
        static_cast<LONG>(previewBytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    BYTE nPersonalSpace = pPreview->m_animation.GetPersonalSpace();
    CString creName(reinterpret_cast<const char*>(pPreview->GetScriptName()));
    REQUIRE(!creName.IsEmpty());

    // A wall to aim the target at: the first impassable square inside the
    // map's margin, under the same cost rule the search uses.
    SHORT nTableIndex;
    CPoint blocked(-1, -1);
    CSize dims = pArea->m_search.m_GridSquareDimensions;
    for (int y = 4; y < dims.cy - 4 && blocked.x == -1; y++) {
        for (int x = 4; x < dims.cx - 4; x++) {
            if (pArea->m_search.GetCost(CPoint(x, y), pTerrain, nPersonalSpace, nTableIndex, TRUE)
                == CPathSearch::COST_IMPASSABLE) {
                blocked = CPoint(x, y);
                break;
            }
        }
    }
    REQUIRE(blocked.x != -1);

    CPoint expected = pArea->m_search.GetNearestOpenSquare(blocked, pTerrain, nPersonalSpace, -1);
    const bool movedOffWall = expected.x != blocked.x || expected.y != blocked.y;
    REQUIRE(movedOffWall);

    // Aim the TARGET object's position at the wall square (m_dest is never
    // read here): the search must run around that square, not the caller's.
    pTarget->m_pos = CPoint(blocked.x * CPathSearch::GRID_SQUARE_SIZEX + 1,
        blocked.y * CPathSearch::GRID_SQUARE_SIZEY + 1);
    CAIAction place;
    place.m_actionID = 227;
    place.m_acteeID = actee;
    place.m_string1 = "30ICETRL";
    place.m_string2 = "TROLLOPEN"; // never copied and never registered
    place.m_specificID = 13; // odd facing; the sprite constructor rounds it down
    pCaller->SetCurrAction(place);
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    CVariable* pPlacedVariable = pArea->m_namedCreatures.FindKey(creName);
    REQUIRE(pPlacedVariable != NULL);
    LONG idPlaced = pPlacedVariable->m_intValue;
    CGameSprite* pPlaced = spriteById(idPlaced);
    REQUIRE(pPlaced != NULL);
    CHECK(pPlaced->m_resRef == CResRef("30ICETRL"));
    CHECK(pPlaced->m_pArea == pArea);
    CHECK(pPlaced->GetPos() == CPoint(expected.x * CPathSearch::GRID_SQUARE_SIZEX, expected.y * CPathSearch::GRID_SQUARE_SIZEY));
    // The facing (m_specificID = 13) is masked to 0xF by the constructor and
    // then rounded down to the next even direction by SetSequence's 0xFE mask
    // (see the CreateCreature case 7 check above): 13 lands as 12.
    CHECK(pPlaced->m_nDirection == 12);
    // The second string did not become the sprite's script name, and no
    // variable was registered under it either.
    CHECK(CString(reinterpret_cast<const char*>(pPlaced->GetScriptName())) == creName);
    CHECK(pArea->m_namedCreatures.FindKey(CString("TROLLOPEN")) == NULL);

    // A (-1, -1) target position takes the caller's source position instead:
    // the search runs around square (7, 7), where the caller stands (the
    // wall sprite above has already marked its own square on the map).
    CPoint expectedFromCaller = pArea->m_search.GetNearestOpenSquare(
        CPoint(pCaller->m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
            pCaller->m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
        pTerrain,
        nPersonalSpace,
        -1);
    pTarget->m_pos = CPoint(-1, -1);
    CAIAction fromSource;
    fromSource.m_actionID = 227;
    fromSource.m_acteeID = actee;
    fromSource.m_string1 = "30ICETRL";
    fromSource.m_specificID = 6; // even facing survives the constructor
    pCaller->SetCurrAction(fromSource);
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    // The same CRE name: the registration replaces the variable's id with
    // the new sprite's (AddKey assigns into the existing entry).
    CVariable* pFromSourceVariable = pArea->m_namedCreatures.FindKey(creName);
    REQUIRE(pFromSourceVariable != NULL);
    LONG idFromSource = pFromSourceVariable->m_intValue;
    REQUIRE(idFromSource != idPlaced);
    CGameSprite* pFromSource = spriteById(idFromSource);
    REQUIRE(pFromSource != NULL);
    CHECK(pFromSource->GetPos() == CPoint(expectedFromCaller.x * CPathSearch::GRID_SQUARE_SIZEX, expectedFromCaller.y * CPathSearch::GRID_SQUARE_SIZEY));
    CHECK(pFromSource->m_nDirection == 6);

    // Both sprites are on the area's sorted lists after the sort a frame's
    // AIUpdate runs.
    pArea->SortLists();
    int nFound = 0;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* objects : lists) {
        POSITION pos = objects->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(objects->GetNext(pos)));
            if (id == idPlaced || id == idFromSource) {
                nFound++;
            }
        }
    }
    CHECK(nFound == 2);
    ClearMessages();
}

TEST_CASE("ai: ExecuteAction 82 CreateItem stores the new item or hands it to the ground pile", "[ai][data][ar3000]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear (tests/test_effects.cpp's AR3000 pump).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea = pGame->LoadArea(CString("AR3000"), 0, FALSE, FALSE);
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    ClearMessages();

    // The pile lookup the full-pack and drop arms go through, counted item
    // by item so stackable counts (CountItem sums usage counts) do not blur
    // the check.
    auto pileHas = [pGame](LONG idSprite, const CResRef& res) -> int {
        LONG idPile = pGame->GetGroundPile(idSprite);
        if (idPile == CGameObjectArray::INVALID_INDEX) {
            return -1;
        }
        CGameContainer* pPile = NULL;
        if (pGame->GetObjectArray()->GetShare(idPile,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pPile),
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            return -1;
        }
        int nCount = 0;
        for (SHORT nSlot = 0; nSlot < pPile->GetNumItems(); nSlot++) {
            CItem* pItem = pPile->GetItem(nSlot);
            if (pItem != NULL && pItem->GetResRef() == res) {
                nCount++;
            }
        }
        pGame->GetObjectArray()->ReleaseShare(idPile, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return nCount;
    };

    // Rejected: the resref names no item in this install, so the key table
    // probe fails before a CItem is built.
    CGameAIBase* pCaller = AddAIObject();
    pCaller->m_pArea = pArea;
    pCaller->m_pos = CPoint(7 * CPathSearch::GRID_SQUARE_SIZEX,
        7 * CPathSearch::GRID_SQUARE_SIZEY);
    CAIAction noItem;
    noItem.m_actionID = 82;
    noItem.m_string1 = "NOSUCHIT";
    pCaller->SetCurrAction(noItem);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // A party member: the item lands in the first blank personal-inventory
    // slot with the three requested usage counts.
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_pArea = pArea;
    pSprite->m_pos = CPoint(8 * CPathSearch::GRID_SQUARE_SIZEX,
        8 * CPathSearch::GRID_SQUARE_SIZEY);
    int nPartyBlank = -1;
    for (int i = 0; i < CScreenInventory::PERSONAL_INVENTORY_SIZE; i++) {
        if (pSprite->m_equipment.m_items[i + 18] == NULL) {
            nPartyBlank = i;
            break;
        }
    }
    REQUIRE(nPartyBlank != -1);

    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait = pGame->m_characterPortraits[0];
    pGame->m_nCharacters = 1;
    pGame->m_characterPortraits[0] = pSprite->m_id;

    CAIAction make;
    make.m_actionID = 82;
    make.m_string1 = "00POTN01";
    make.m_specificID = 5;
    make.m_specificID2 = 7;
    make.m_specificID3 = 0;
    pSprite->SetCurrAction(make);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CItem* pStored = pSprite->m_equipment.m_items[nPartyBlank + 18];
    REQUIRE(pStored != NULL);
    CHECK(pStored->GetResRef() == CResRef("00POTN01"));
    // A potion has one ability, so GetUsageCount gates counts 1 and 2 to 0;
    // the raw members carry what CreateItem handed the constructor. The
    // potion is stackable, and the constructor's clamp lifts a zero third
    // count to 1.
    CHECK(pStored->GetUsageCount(0) == 5);
    CHECK(pStored->m_useCount2 == 7);
    CHECK(pStored->m_useCount3 == 1);

    // A full pack: the ground pile beside the sprite takes the item, but the
    // action reports an error — dropping an item because there was no room
    // is a failure, as PlaceItem's full-pack drop is.
    for (int i = 0; i < CScreenInventory::PERSONAL_INVENTORY_SIZE; i++) {
        if (pSprite->m_equipment.m_items[i + 18] == NULL) {
            pSprite->m_equipment.m_items[i + 18] = new CItem(CResRef("00GEM01"), 0, 0, 0, 0, 0);
        }
    }
    CAIAction overflow;
    overflow.m_actionID = 82;
    overflow.m_string1 = "00GBGA01";
    ClearMessages();
    pSprite->SetCurrAction(overflow);
    CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    // The pile takes the item through CMessageContainerAddItem.
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pileHas(pSprite->m_id, CResRef("00GBGA01")) == 1);

    // Back to no party member.
    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait;

    // A creature outside the party: the first blank of its carried slots,
    // the 15 equipment slots plus both packs.
    CGameSprite* pLone = MakeSprite();
    pLone->m_pArea = pArea;
    pLone->m_pos = CPoint(9 * CPathSearch::GRID_SQUARE_SIZEX,
        9 * CPathSearch::GRID_SQUARE_SIZEY);
    int nCarriedBlank = -1;
    for (int i = 0; i < 28; i++) {
        if (pLone->m_equipment.m_items[i + 15] == NULL) {
            nCarriedBlank = i;
            break;
        }
    }
    REQUIRE(nCarriedBlank != -1);

    CAIAction carried;
    carried.m_actionID = 82;
    carried.m_string1 = "00POTN01";
    carried.m_specificID = 2;
    pLone->SetCurrAction(carried);
    CHECK(pLone->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CItem* pCarried = pLone->m_equipment.m_items[nCarriedBlank + 15];
    REQUIRE(pCarried != NULL);
    CHECK(pCarried->GetResRef() == CResRef("00POTN01"));
    CHECK(pCarried->GetUsageCount(0) == 2);
    CHECK(pileHas(pLone->m_id, CResRef("00POTN01")) == 0);

    // A container takes the item through its own slot list.
    CGameContainer* pContainer = new CGameContainer(pArea, CRect(400, 400, 416, 416));
    CAIAction stow;
    stow.m_actionID = 82;
    stow.m_string1 = "00GEM01";
    ClearMessages();
    pContainer->SetCurrAction(stow);
    CHECK(pContainer->ExecuteAction() == CGameAIBase::ACTION_DONE);
    // The container takes the item through CMessageContainerAddItem.
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    REQUIRE(pContainer->GetNumItems() == 1);
    REQUIRE(pContainer->GetItem(0) != NULL);
    CHECK(pContainer->GetItem(0)->GetResRef() == CResRef("00GEM01"));

    ClearMessages();
}

TEST_CASE("ai: ExecuteAction 116/204 TakePartyItem takes the member's item or splits its stack", "[ai][data][ar3000]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear (tests/test_effects.cpp's AR3000 pump).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea = pGame->LoadArea(CString("AR3000"), 0, FALSE, FALSE);
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    ClearMessages();

    // The taker stands outside the party (PlaceItem hands a non-sprite's
    // gains to a container or deletes them), so its copies land in the
    // highest blank of the carried slots 15..42, PlaceItem's backward scan.
    auto carriedBlank = [](CGameSprite* pSprite) -> SHORT {
        for (SHORT nSlot = 42; nSlot >= 15; nSlot--) {
            if (pSprite->m_equipment.m_items[nSlot] == NULL) {
                return nSlot;
            }
        }
        return -1;
    };
    auto countItems = [](CGameSprite* pSprite) -> int {
        int nCount = 0;
        for (SHORT nSlot = 0; nSlot < 51; nSlot++) {
            if (pSprite->m_equipment.m_items[nSlot] != NULL) {
                nCount++;
            }
        }
        return nCount;
    };

    CGameSprite* pTaker = MakeSprite();
    pTaker->m_pArea = pArea;
    pTaker->m_pos = CPoint(10 * CPathSearch::GRID_SQUARE_SIZEX,
        10 * CPathSearch::GRID_SQUARE_SIZEY);
    CGameSprite* pMember = MakeSprite();
    pMember->m_pArea = pArea;
    pMember->m_pos = CPoint(11 * CPathSearch::GRID_SQUARE_SIZEX,
        11 * CPathSearch::GRID_SQUARE_SIZEY);
    // FindItemPersonal scans all 51 slots, so the member must carry no copy
    // anywhere for the seed below to be its first personal copy.
    REQUIRE(pMember->FindItemPersonal(CString("00POTN01"), 0, FALSE) == -1);
    REQUIRE(pMember->m_equipment.m_items[18] == NULL);

    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait = pGame->m_characterPortraits[0];
    pGame->m_nCharacters = 1;
    pGame->m_characterPortraits[0] = pMember->m_id;

    // 116: the first personal copy leaves the member and the taker gains it.
    {
        pMember->m_equipment.m_items[18] = new CItem(CResRef("00POTN01"), 3, 0, 0, 0, 0);
        const SHORT nTakerSlot = carriedBlank(pTaker);
        REQUIRE(nTakerSlot != -1);

        CAIAction take;
        take.m_actionID = 116;
        take.m_string1 = "00POTN01";
        pTaker->SetCurrAction(take);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_DONE);

        // The removal rides a CMessageRemoveItem aimed at the member's slot.
        const std::vector<CMessageRemoveItem*> removed = Queued<CMessageRemoveItem>();
        REQUIRE(removed.size() == 1);
        CHECK(removed[0]->m_slot == 18);
        CHECK(removed[0]->GetSource() == pTaker->m_id);
        CHECK(removed[0]->GetTarget() == pMember->m_id);

        // The taker's copy is placed directly, charge count intact.
        CItem* pGained = pTaker->m_equipment.m_items[nTakerSlot];
        REQUIRE(pGained != NULL);
        CHECK(pGained->GetResRef() == CResRef("00POTN01"));
        CHECK(pGained->GetUsageCount(0) == 3);

        // Running the queued removal clears the member's slot.
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        CHECK(pMember->m_equipment.m_items[18] == NULL);
        ClearMessages();
    }

    // 204: three charges of a five-charge stack splits it -- the taker keeps
    // the reduced copy and the member gets the leftover two back.
    {
        pMember->m_equipment.m_items[18] = new CItem(CResRef("00POTN01"), 5, 0, 0, 0, 0);
        const SHORT nTakerSlot = carriedBlank(pTaker);
        REQUIRE(nTakerSlot != -1);

        CAIAction takeNum;
        takeNum.m_actionID = 204;
        takeNum.m_string1 = "00POTN01";
        takeNum.m_specificID = 3;
        pTaker->SetCurrAction(takeNum);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_DONE);

        // The stack's removal and the remainder's add-back, both queued.
        const std::vector<CMessageRemoveItem*> removed = Queued<CMessageRemoveItem>();
        REQUIRE(removed.size() == 1);
        CHECK(removed[0]->m_slot == 18);
        const std::vector<CMessageAddItem*> added = Queued<CMessageAddItem>();
        REQUIRE(added.size() == 1);
        CHECK(added[0]->m_item.GetResRef() == CResRef("00POTN01"));
        CHECK(added[0]->m_item.GetUsageCount(0) == 2);
        CHECK(added[0]->GetSource() == pTaker->m_id);
        CHECK(added[0]->GetTarget() == pMember->m_id);

        // The taker's copy carries the reduced count.
        CItem* pGained = pTaker->m_equipment.m_items[nTakerSlot];
        REQUIRE(pGained != NULL);
        CHECK(pGained->GetUsageCount(0) == 2);

        // The FIFO drain clears slot 18 then refills it with the remainder.
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        CItem* pRemainder = pMember->m_equipment.m_items[18];
        REQUIRE(pRemainder != NULL);
        CHECK(pRemainder->GetResRef() == CResRef("00POTN01"));
        CHECK(pRemainder->GetUsageCount(0) == 2);
        ClearMessages();
    }

    // The party carries nothing named by the action: an error, with every
    // inventory and message queue untouched.
    {
        REQUIRE(pMember->FindItemPersonal(CString("00GBGA01"), 0, FALSE) == -1);
        const int nMemberItems = countItems(pMember);
        const int nTakerItems = countItems(pTaker);

        CAIAction none;
        none.m_actionID = 116;
        none.m_string1 = "00GBGA01";
        pTaker->SetCurrAction(none);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_ERROR);
        CHECK(countItems(pMember) == nMemberItems);
        CHECK(countItems(pTaker) == nTakerItems);
        CHECK(Queued<CMessageRemoveItem>().empty());
        CHECK(Queued<CMessageAddItem>().empty());
    }

    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait;
    ClearMessages();
}

TEST_CASE("ai: ExecuteAction 220/226 TakeItemListParty takes 2DA-listed items from the party", "[ai][data][ar3000]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear (tests/test_effects.cpp's AR3000 pump).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea = pGame->LoadArea(CString("AR3000"), 0, FALSE, FALSE);
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    ClearMessages();

    // RNDTRES.2DA names an item in every row's first data column: row 0
    // (RT01_L) 00ax1h01, row 1 (RT01_M) 00bows01. The default cell is "*",
    // which names no item, so a 226 that takes nothing places no fallback.
    auto carriedBlank = [](CGameSprite* pSprite) -> SHORT {
        for (SHORT nSlot = 42; nSlot >= 15; nSlot--) {
            if (pSprite->m_equipment.m_items[nSlot] == NULL) {
                return nSlot;
            }
        }
        return -1;
    };
    auto countItems = [](CGameSprite* pSprite) -> int {
        int nCount = 0;
        for (SHORT nSlot = 0; nSlot < 51; nSlot++) {
            if (pSprite->m_equipment.m_items[nSlot] != NULL) {
                nCount++;
            }
        }
        return nCount;
    };

    CGameSprite* pTaker = MakeSprite();
    pTaker->m_pArea = pArea;
    pTaker->m_pos = CPoint(10 * CPathSearch::GRID_SQUARE_SIZEX,
        10 * CPathSearch::GRID_SQUARE_SIZEY);
    CGameSprite* pMember = MakeSprite();
    pMember->m_pArea = pArea;
    pMember->m_pos = CPoint(11 * CPathSearch::GRID_SQUARE_SIZEX,
        11 * CPathSearch::GRID_SQUARE_SIZEY);
    // FindItemPersonal scans all 51 slots, so the member must carry no copy
    // anywhere for the seeds below to be its first personal copies.
    REQUIRE(pMember->FindItemPersonal(CString("00AX1H01"), 0, FALSE) == -1);
    REQUIRE(pMember->FindItemPersonal(CString("00BOWS01"), 0, FALSE) == -1);
    REQUIRE(pMember->m_equipment.m_items[18] == NULL);
    REQUIRE(pMember->m_equipment.m_items[19] == NULL);

    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait = pGame->m_characterPortraits[0];
    pGame->m_nCharacters = 1;
    pGame->m_characterPortraits[0] = pMember->m_id;

    // 220: every row is walked; the member's copy of row 0's item leaves
    // through a CMessageRemoveItem and the taker gains it.
    {
        pMember->m_equipment.m_items[18] = new CItem(CResRef("00AX1H01"), 3, 0, 0, 0, 0);
        const SHORT nTakerSlot = carriedBlank(pTaker);
        REQUIRE(nTakerSlot != -1);

        CAIAction takeList;
        takeList.m_actionID = 220;
        takeList.m_string1 = "RNDTRES";
        pTaker->SetCurrAction(takeList);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_DONE);

        const std::vector<CMessageRemoveItem*> removed = Queued<CMessageRemoveItem>();
        REQUIRE(removed.size() == 1);
        CHECK(removed[0]->m_slot == 18);
        CHECK(removed[0]->GetSource() == pTaker->m_id);
        CHECK(removed[0]->GetTarget() == pMember->m_id);

        CItem* pGained = pTaker->m_equipment.m_items[nTakerSlot];
        REQUIRE(pGained != NULL);
        CHECK(pGained->GetResRef() == CResRef("00AX1H01"));
        CHECK(pGained->GetUsageCount(0) == 3);

        // Running the queued removal clears the member's slot.
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        CHECK(pMember->m_equipment.m_items[18] == NULL);
        ClearMessages();
    }

    // 226 spends one count per row that yielded anything: the count of 1
    // ends the walk after row 0, so row 1's item stays with the member.
    {
        pMember->m_equipment.m_items[18] = new CItem(CResRef("00AX1H01"), 2, 0, 0, 0, 0);
        pMember->m_equipment.m_items[19] = new CItem(CResRef("00BOWS01"), 4, 0, 0, 0, 0);
        const SHORT nTakerSlot = carriedBlank(pTaker);
        REQUIRE(nTakerSlot != -1);

        CAIAction takeListNum;
        takeListNum.m_actionID = 226;
        takeListNum.m_string1 = "RNDTRES";
        takeListNum.m_specificID = 1;
        pTaker->SetCurrAction(takeListNum);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_DONE);

        const std::vector<CMessageRemoveItem*> removed = Queued<CMessageRemoveItem>();
        REQUIRE(removed.size() == 1);
        CHECK(removed[0]->m_slot == 18);

        CItem* pGained = pTaker->m_equipment.m_items[nTakerSlot];
        REQUIRE(pGained != NULL);
        CHECK(pGained->GetResRef() == CResRef("00AX1H01"));
        CHECK(pGained->GetUsageCount(0) == 2);

        // Row 1 never ran: the member keeps its copy.
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        CItem* pKept = pMember->m_equipment.m_items[19];
        REQUIRE(pKept != NULL);
        CHECK(pKept->GetResRef() == CResRef("00BOWS01"));
        CHECK(pKept->GetUsageCount(0) == 4);
        delete pKept;
        pMember->m_equipment.m_items[19] = NULL;
        ClearMessages();
    }

    // A 226 that takes nothing falls back to the 2DA's default cell, which
    // names no item here: done, and nothing moves.
    {
        const int nMemberItems = countItems(pMember);
        const int nTakerItems = countItems(pTaker);

        CAIAction nothing;
        nothing.m_actionID = 226;
        nothing.m_string1 = "RNDTRES";
        nothing.m_specificID = 1;
        pTaker->SetCurrAction(nothing);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_DONE);
        CHECK(countItems(pMember) == nMemberItems);
        CHECK(countItems(pTaker) == nTakerItems);
        CHECK(Queued<CMessageRemoveItem>().empty());
        CHECK(Queued<CMessageAddItem>().empty());
        ClearMessages();
    }

    // 220 naming a 2DA the game does not ship: the walk never runs and the
    // action still reports done.
    {
        const int nMemberItems = countItems(pMember);
        const int nTakerItems = countItems(pTaker);

        CAIAction missing;
        missing.m_actionID = 220;
        missing.m_string1 = "T83NOEX";
        pTaker->SetCurrAction(missing);
        CHECK(pTaker->ExecuteAction() == CGameAIBase::ACTION_DONE);
        CHECK(countItems(pMember) == nMemberItems);
        CHECK(countItems(pTaker) == nTakerItems);
        CHECK(Queued<CMessageRemoveItem>().empty());
        CHECK(Queued<CMessageAddItem>().empty());
    }

    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait;
    ClearMessages();
}

TEST_CASE("ai: ExecuteAction 141/165 give party gold and experience from a global", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = MakeSprite();
    REQUIRE(pSprite->m_id != CGameObjectArray::INVALID_INDEX);

    // The sprite carries 100 gold in both stats; the gold tail caps the
    // amount at the derived gold and subtracts from both.
    pSprite->m_baseStats.m_gold = 100;
    pSprite->m_derivedStats.m_nGold = 100;

    // A game global the way SetGlobal would have made it, and a local on the
    // sprite for the LOCALS scope. The handler uppercases both strings.
    CVariable var;
    var.SetName(CString("T83_GOLD"));
    var.m_intValue = 40;
    pGame->m_variables.AddKey(var);
    CVariable local;
    local.SetName(CString("T83_LOCAL"));
    local.m_intValue = 7;
    pSprite->GetLocalVariables()->AddKey(local);

    // 141 GivePartyGoldGlobal("t83_gold", "global"): string1 is the name and
    // string2 the scope. The sprite pays, the party's gold message carries
    // the amount, and the running message adds it to the party.
    {
        CAIAction give;
        give.m_actionID = 141;
        give.m_string1 = "t83_gold";
        give.m_string2 = "global";
        pSprite->SetCurrAction(give);
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);

        const std::vector<CMessagePartyGold*> golds = Queued<CMessagePartyGold>();
        REQUIRE(golds.size() == 1);
        CHECK(golds[0]->m_gold == 40);
        CHECK(golds[0]->m_bAdjustment == TRUE);
        CHECK(golds[0]->m_bFeedback == TRUE);
        CHECK(golds[0]->GetSource() == pSprite->m_id);
        CHECK(golds[0]->GetTarget() == pSprite->m_id);
        CHECK(pSprite->m_baseStats.m_gold == 60);
        CHECK(pSprite->m_derivedStats.m_nGold == 60);

        const DWORD nPartyGold = pGame->GetGameSave()->m_nPartyGold;
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        CHECK(pGame->GetGameSave()->m_nPartyGold == nPartyGold + 40);
        ClearMessages();
    }

    // The LOCALS scope reads the sprite's own variable.
    {
        CAIAction give;
        give.m_actionID = 141;
        give.m_string1 = "T83_LOCAL";
        give.m_string2 = "LOCALS";
        pSprite->SetCurrAction(give);
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
        const std::vector<CMessagePartyGold*> golds = Queued<CMessagePartyGold>();
        REQUIRE(golds.size() == 1);
        CHECK(golds[0]->m_gold == 7);
        CHECK(pSprite->m_baseStats.m_gold == 53);
        CHECK(pSprite->m_derivedStats.m_nGold == 53);
        ClearMessages();
    }

    // A variable nobody set reads as 0: a zero message, no gold change.
    {
        CAIAction give;
        give.m_actionID = 141;
        give.m_string1 = "T83_MISSING";
        give.m_string2 = "GLOBAL";
        pSprite->SetCurrAction(give);
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
        const std::vector<CMessagePartyGold*> golds = Queued<CMessagePartyGold>();
        REQUIRE(golds.size() == 1);
        CHECK(golds[0]->m_gold == 0);
        CHECK(pSprite->m_baseStats.m_gold == 53);
        CHECK(pSprite->m_derivedStats.m_nGold == 53);

        const DWORD nPartyGold = pGame->GetGameSave()->m_nPartyGold;
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        CHECK(pGame->GetGameSave()->m_nPartyGold == nPartyGold);
        ClearMessages();
    }

    // A variable above the sprite's gold is capped at its derived gold: the
    // sprite pays all it has and the message carries the cap.
    {
        CVariable high;
        high.SetName(CString("T83_HIGH"));
        high.m_intValue = 500;
        pGame->m_variables.AddKey(high);

        CAIAction give;
        give.m_actionID = 141;
        give.m_string1 = "T83_HIGH";
        give.m_string2 = "GLOBAL";
        pSprite->SetCurrAction(give);
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
        const std::vector<CMessagePartyGold*> golds = Queued<CMessagePartyGold>();
        REQUIRE(golds.size() == 1);
        CHECK(golds[0]->m_gold == 53);
        CHECK(pSprite->m_baseStats.m_gold == 0);
        CHECK(pSprite->m_derivedStats.m_nGold == 0);
        ClearMessages();
    }

    // 165 AddexperiencePartyGlobal("t83_xp", "GLOBAL"): the lone party
    // member's share of the global rides an experience effect (opcode 104).
    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait = pGame->m_characterPortraits[0];
    pGame->m_nCharacters = 1;
    pGame->m_characterPortraits[0] = pSprite->m_id;
    {
        CVariable xp;
        xp.SetName(CString("T83_XP"));
        xp.m_intValue = 250;
        pGame->m_variables.AddKey(xp);

        CAIAction addXP;
        addXP.m_actionID = 165;
        addXP.m_string1 = "t83_xp";
        addXP.m_string2 = "global";
        pSprite->SetCurrAction(addXP);
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);

        const std::vector<CMessageAddEffect*> added = Queued<CMessageAddEffect>();
        REQUIRE(added.size() == 1);
        CHECK(added[0]->GetTarget() == pSprite->m_id);
        REQUIRE(added[0]->m_effect != NULL);
        CHECK(added[0]->m_effect->GetItemEffect()->effectID == CGAMEEFFECT_XP);
        CHECK(added[0]->m_effect->GetItemEffect()->effectAmount == 250);
        ClearMessages();
    }

    // A missing variable reads as 0: the member's share is a zero effect.
    {
        CAIAction addXP;
        addXP.m_actionID = 165;
        addXP.m_string1 = "T83_MISSING";
        addXP.m_string2 = "GLOBAL";
        pSprite->SetCurrAction(addXP);
        CHECK(pSprite->ExecuteAction() == CGameAIBase::ACTION_DONE);
        const std::vector<CMessageAddEffect*> added = Queued<CMessageAddEffect>();
        REQUIRE(added.size() == 1);
        CHECK(added[0]->m_effect->GetItemEffect()->effectAmount == 0);
        ClearMessages();
    }

    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait;
    ClearMessages();
}

// Task 83d's ExecuteAction arms, driven through ExecuteAction as far as they
// are observable without game data: the return codes and the messages they
// queue. The arms whose work needs rest state, a formed party with the export
// flag set, or a loaded area (258 RestUntilHealed's RestParty walk, 267
// ExportParty's and 268 ClearPartyEffects' per-character loops) stop at the
// empty party here; the task report records why.
TEST_CASE("ai: ExecuteAction 189 LeaveAreaLUAPanic 259 SetVisualRange and 270 ClearSpriteEffects",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();

    // 189 LeaveAreaLUAPanic: without a network connection the hand-off is
    // skipped and the action is done (0x466910).
    CAIAction panic;
    panic.m_actionID = 189;
    pCaller->SetCurrAction(panic);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    // 259 SetVisualRange(14): a CMessage92 to self carries the range.
    CAIAction setRange;
    setRange.m_actionID = 259;
    setRange.m_specificID = 14;
    pCaller->SetCurrAction(setRange);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessage92*> ranges = Queued<CMessage92>();
        REQUIRE(ranges.size() == 1);
        CHECK(ranges[0]->field_C == 14);
        CHECK(ranges[0]->m_sourceId == pCaller->m_id);
        CHECK(ranges[0]->m_targetId == pCaller->m_id);
    }
    ClearMessages();

    // 270 ClearSpriteEffects without a target: the arm jumps to 0x44E1DB,
    // which ors the return value to ACTION_DONE before the release block, so
    // the answer is done, not an error.
    CAIAction clearSprite;
    clearSprite.m_actionID = 270;
    pCaller->SetCurrAction(clearSprite);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    ClearMessages();
}

TEST_CASE("ai: ExecuteAction 260 266 267 and 268 without a target or party", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();

    // 260 JumpToPointInstant without a target: 0x451451 loads ACTION_ERROR
    // before the release block, so the answer is an error (unlike 270).
    CAIAction jump;
    jump.m_actionID = 260;
    pCaller->SetCurrAction(jump);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 266 DisplayMessage(10309): the fetched text rides a CMessageDisplayText
    // in the original's colors, to self, with no name.
    CAIAction display;
    display.m_actionID = 266;
    display.m_specificID = 10309; // "You may not rest at this time."
    pCaller->SetCurrAction(display);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageDisplayText*> shown = Queued<CMessageDisplayText>();
        REQUIRE(shown.size() == 1);
        CHECK(shown[0]->m_name.IsEmpty());
        CHECK(shown[0]->m_text == "You may not rest at this time.");
        CHECK(shown[0]->m_textColor == RGB(160, 200, 215));
        CHECK(shown[0]->m_nameColor == RGB(160, 200, 215));
        CHECK(shown[0]->m_marker == -1);
        CHECK(shown[0]->m_sourceId == pCaller->m_id);
        CHECK(shown[0]->m_targetId == pCaller->m_id);
    }
    ClearMessages();

    // 267 ExportParty("t83d"): with no party the loop exports nothing and the
    // header strref (26827) is posted to ids 0.
    CAIAction exportParty;
    exportParty.m_actionID = 267;
    exportParty.m_string1 = "t83d";
    pCaller->SetCurrAction(exportParty);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageDisplayTextRef*> refs = Queued<CMessageDisplayTextRef>();
        REQUIRE(refs.size() == 1);
        CHECK(refs[0]->m_name == static_cast<STRREF>(-1));
        CHECK(refs[0]->m_text == 26827);
        CHECK(refs[0]->m_nameColor == RGB(0, 0, 0));
        CHECK(refs[0]->m_textColor == RGB(255, 255, 0));
        CHECK(refs[0]->m_marker == -1);
        CHECK(refs[0]->m_sourceId == 0);
        CHECK(refs[0]->m_targetId == 0);
    }
    ClearMessages();

    // 268 ClearPartyEffects with no party: the walk is empty and the action
    // is done without a message.
    CAIAction clearParty;
    clearParty.m_actionID = 268;
    pCaller->SetCurrAction(clearParty);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    ClearMessages();
}

// Task 83e's ExecuteAction arms, driven through ExecuteAction as far as they
// are observable without game data. 290/291/292 stop at GetTargetShareType's
// null (no sprite or container resolves for a bare AI object's action), 295's
// arm is still a TODO stub (its receiving-sprite resolver 0x45C290 is in tree
// from Task 83i), and 297 is fully ported. Task 83f ported 296's markup call.
TEST_CASE("ai: ExecuteAction 290 291 292 spell-hit and chunk arms without a target", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();

    // 290 SpellHitEffectPoint: neither a sprite nor a container resolves for
    // the action's target, so the arm answers the entry default, the error
    // code.
    CAIAction spellHitPoint;
    spellHitPoint.m_actionID = 290;
    pCaller->SetCurrAction(spellHitPoint);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 291 SpellHitEffectSprite: no target sprite, same answer.
    CAIAction spellHitSprite;
    spellHitSprite.m_actionID = 291;
    pCaller->SetCurrAction(spellHitSprite);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 292 ChunkCreature: no target sprite means no chunk effect is queued.
    CAIAction chunk;
    chunk.m_actionID = 292;
    pCaller->SetCurrAction(chunk);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    ClearMessages();
}

TEST_CASE("ai: ExecuteAction 294 295 296 and 297 equip transfer markup and screen shake",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();

    // 294 XEquipItem without a target: the null test jumps to 0x44E1DB,
    // which forces ACTION_DONE (like 270, unlike 290).
    CAIAction equip;
    equip.m_actionID = 294;
    pCaller->SetCurrAction(equip);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    // 295 TransferInventory: the arm is still a TODO stub (its resolver
    // 0x45C290 is in tree from Task 83i), so it answers the entry default.
    CAIAction transfer;
    transfer.m_actionID = 295;
    pCaller->SetCurrAction(transfer);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 296 ChangeStoreMarkup("20EMMA", 333, 444, 55) with no network
    // connection: the single-player arm re-marshals the store unchanged --
    // the markups are never read on this path (verified at 0x5C9F5E-0x5C9FA7)
    // -- and the arm still forces ACTION_DONE. The store's own markups go to
    // the temp file at header+0xC/0x10/0x14, not the action's specifics.
    DWORD nBuyBefore;
    DWORD nSellBefore;
    DWORD nAdditionalBefore;
    {
        CStore before(CResRef("20EMMA"));
        nBuyBefore = before.m_header.m_nBuyMarkUp;
        nSellBefore = before.m_header.m_nSellMarkDown;
        nAdditionalBefore = before.m_header.m_nAdditionalMarkDown;
    }
    CAIAction markup;
    markup.m_actionID = 296;
    markup.SetString1("20EMMA");
    markup.m_specificID = 333;
    markup.m_specificID2 = 444;
    markup.m_specificID3 = 55;
    pCaller->SetCurrAction(markup);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageStoreMarkup>().empty());
    CHECK(Queued<CMessageStoreDemand>().empty());
    {
        const std::vector<char> marshalled = TempFile("20EMMA.STO");
        REQUIRE(marshalled.size() >= 8 + 0x1C + sizeof(DWORD));
        CHECK(std::memcmp(marshalled.data(), "STORV9.0", 8) == 0);
        CHECK(Dword(marshalled, 8 + 0xC) == nBuyBefore);
        CHECK(Dword(marshalled, 8 + 0x10) == nSellBefore);
        CHECK(Dword(marshalled, 8 + 0x14) == nAdditionalBefore);
        CHECK(Dword(marshalled, 8 + 0xC) != 333);
        CHECK(Dword(marshalled, 8 + 0x10) != 444);
        CHECK(Dword(marshalled, 8 + 0x14) != 55);
    }

    // 297 ScreenShake(5, 6, 7): the shake rides a CMessageScreenShake to
    // self, override set, in the action's specifics order (duration, x, y).
    CAIAction shake;
    shake.m_actionID = 297;
    shake.m_specificID = 5;
    shake.m_specificID2 = 6;
    shake.m_specificID3 = 7;
    pCaller->SetCurrAction(shake);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageScreenShake*> shakes = Queued<CMessageScreenShake>();
        REQUIRE(shakes.size() == 1);
        CHECK(shakes[0]->m_duration == 5);
        CHECK(shakes[0]->m_magnitudeX == 6);
        CHECK(shakes[0]->m_magnitudeY == 7);
        CHECK(shakes[0]->m_bOverride == 1);
        CHECK(shakes[0]->m_sourceId == pCaller->m_id);
        CHECK(shakes[0]->m_targetId == pCaller->m_id);
    }

    ClearMessages();
}

// Task 83g's case 293 UnHide arm, driven through ExecuteAction on a live
// sprite: the arm clears the target's hiding state, strips the invisible
// effect off the timed list, greys out the stealth indicator, and reports a
// remove-all-of-type effect to itself. The toolbar refresh is skipped: the
// test sprite has no portrait, so GetCharacterPortraitNum answers -1.
TEST_CASE("ai: ExecuteAction 293 UnHide clears hiding and reports the effect strip",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();

    // Hiding, with one invisible effect riding the timed list.
    pSprite->sub_453160(1);
    ITEM_EFFECT invisible;
    CGameEffect::ClearItemEffect(&invisible, CGAMEEFFECT_INVISIBLE);
    CGameEffect* pInvisible = CGameEffect::DecodeEffect(&invisible,
        CPoint(-1, -1),
        -1,
        CPoint(-1, -1));
    pSprite->GetTimedEffectList()->AddTail(pInvisible);
    REQUIRE(pSprite->GetTimedEffectList()->IsTypeOnList(CGAMEEFFECT_INVISIBLE) == TRUE);

    CAIAction unHide;
    unHide.m_actionID = 293;
    unHide.m_acteeID = CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    pCaller->SetCurrAction(unHide);

    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    CHECK(pSprite->m_bHiding == 0);
    CHECK(pSprite->m_nStealthGreyOut == 90);
    CHECK(pSprite->GetTimedEffectList()->IsTypeOnList(CGAMEEFFECT_INVISIBLE) == FALSE);

    // The strip rides a remove-all-of-type effect to self, with the source
    // and target ids both the acting object's own.
    const std::vector<CMessageAddEffect*> messages = Queued<CMessageAddEffect>();
    REQUIRE(messages.size() == 1);
    CHECK(messages[0]->m_sourceId == pCaller->m_id);
    CHECK(messages[0]->m_targetId == pCaller->m_id);
    CHECK(messages[0]->m_effect->m_effectID == 276); // ICEWIND_CGAMEEFFECT_REMOVEALLOFTYPE
    CHECK(messages[0]->m_effect->m_dwFlags == CGAMEEFFECT_INVISIBLE);
    CHECK(messages[0]->m_effect->m_effectAmount == 0);
    ClearMessages();

    // A target that is not hiding takes the tail that forces ACTION_DONE
    // (0x44E1DB) before any of the effect work runs.
    pCaller->SetCurrAction(unHide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageAddEffect>().empty());
    CHECK(pSprite->m_nStealthGreyOut == 90);

    // A null target jumps to the done tail before bShare is set, keeping
    // the ACTION_ERROR entry default -- unlike 294 and 270, which force
    // ACTION_DONE on the same miss.
    CAIAction noTarget = unHide;
    noTarget.m_acteeID = CAIObjectType::NOONE;
    pCaller->SetCurrAction(noTarget);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(Queued<CMessageAddEffect>().empty());
    ClearMessages();
}

// Task 83o's case 68 CallLightning and case 131 Kill arms, driven through
// ExecuteAction on a live sprite. Case 68 aims the caller's area infinity at
// the target's position and queues a death effect (flags 0x100); case 131
// leaves already-dead targets alone and otherwise queues one sourced to the
// target (flags 4, field_18C 1). The caller's area is zeroed storage, as
// MakeDoor gives its doors: CallLightning only writes CInfinity members, so
// no area file is needed.
TEST_CASE("ai: ExecuteAction 68 CallLightning and 131 Kill death-effect arms",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();

    auto* area = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(area, 0, sizeof(CGameArea));
    pCaller->m_pArea = reinterpret_cast<CGameArea*>(area);
    CInfinity* pInfinity = pCaller->m_pArea->GetInfinity();

    pSprite->m_pos = CPoint(111, 222);
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // 68 CallLightning: the strike is aimed at the target's position and the
    // death effect (id 13, amount 0, flags 0x100) rides CMessageAddEffect
    // with the caller as source and the sprite as target.
    pCaller->SetCurrAction(CAIAction(68, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pInfinity->m_bStartLightning == TRUE);
    CHECK(pInfinity->bRenderCallLightning == TRUE);
    CHECK(pInfinity->nRenderLightningTimer == 30);
    CHECK(pInfinity->cLightningPoint.x == 111);
    CHECK(pInfinity->cLightningPoint.y == 222);
    {
        const std::vector<CMessageAddEffect*> adds = Queued<CMessageAddEffect>();
        REQUIRE(adds.size() == 1);
        CHECK(adds[0]->m_sourceId == pCaller->m_id);
        CHECK(adds[0]->m_targetId == pSprite->m_id);
        REQUIRE(adds[0]->m_effect != NULL);
        CHECK(dynamic_cast<CGameEffectDeath*>(adds[0]->m_effect) != NULL);
        CHECK(adds[0]->m_effect->m_effectID == CGAMEEFFECT_DEATH);
        CHECK(adds[0]->m_effect->m_effectAmount == 0);
        CHECK(adds[0]->m_effect->m_dwFlags == 0x100);
    }

    // 68 with no target: the miss jumps to the tail that forces
    // ACTION_DONE (0x44E1DB) -- no strike, no effect.
    ClearMessages();
    pInfinity->bRenderCallLightning = FALSE;
    pInfinity->cLightningPoint = CPoint(-1, -1);
    CAIAction strikeNobody;
    strikeNobody.m_actionID = 68;
    strikeNobody.m_acteeID = CAIObjectType::NOONE;
    pCaller->SetCurrAction(strikeNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pInfinity->bRenderCallLightning == FALSE);
    CHECK(pInfinity->cLightningPoint.x == -1);
    CHECK(pInfinity->cLightningPoint.y == -1);
    CHECK(Queued<CMessageAddEffect>().empty());

    // 131 Kill on a live target: the in-tree sub_761650 stub runs, and the
    // death effect carries flags 4, field_18C 1 and the target's own id as
    // its source.
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(131, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageAddEffect*> adds = Queued<CMessageAddEffect>();
        REQUIRE(adds.size() == 1);
        CHECK(adds[0]->m_sourceId == pCaller->m_id);
        CHECK(adds[0]->m_targetId == pSprite->m_id);
        CGameEffectDeath* pDeath = dynamic_cast<CGameEffectDeath*>(adds[0]->m_effect);
        REQUIRE(pDeath != NULL);
        CHECK(pDeath->m_effectID == CGAMEEFFECT_DEATH);
        CHECK(pDeath->m_effectAmount == 0);
        CHECK(pDeath->m_dwFlags == 4);
        CHECK(pDeath->m_sourceID == pSprite->m_id);
        CHECK(pDeath->field_18C == 1);
    }

    // 131 on an already-dead target (STATE_DEAD in the derived general
    // state): no second effect, still ACTION_DONE -- the miss jumps to the
    // same 0x44E1DB tail.
    ClearMessages();
    pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
    pCaller->SetCurrAction(CAIAction(131, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageAddEffect>().empty());

    // 131 with no target: same forced ACTION_DONE, nothing queued.
    CAIAction killNobody;
    killNobody.m_actionID = 131;
    killNobody.m_acteeID = CAIObjectType::NOONE;
    pCaller->SetCurrAction(killNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageAddEffect>().empty());
    ClearMessages();
}

// Task 83h's case 243 IncrementGlobalOnce arm, driven through ExecuteAction.
// The action's two strings are "SCOPE:name" pairs: string1 names the
// once-guard variable, string2 the variable incremented the first time
// through. The arm splits on ':', trims, and rejoins scope+name without the
// colon -- the format SetGlobal and IncrementGlobal parse with their Left(6)
// scope. The target variable is pre-set through the SetGlobal action: a
// variable IncrementGlobal creates itself starts from an uninitialized
// CVariable (the original's default constructor is empty too), so only the
// increment of an existing variable is deterministic.
TEST_CASE("ai: ExecuteAction 243 IncrementGlobalOnce fires once per guard variable",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();

    CAIAction setTarget;
    setTarget.m_actionID = 30;
    setTarget.m_string1 = "GLOBALTARGET_83H";
    setTarget.m_specificID = 10;
    pCaller->SetCurrAction(setTarget);
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    ClearMessages();

    CAIAction incrementOnce;
    incrementOnce.m_actionID = 243;
    incrementOnce.m_string1 = "GLOBAL:ONCE_83H";
    incrementOnce.m_string2 = "GLOBAL:TARGET_83H";
    incrementOnce.m_specificID = 7;
    pCaller->SetCurrAction(incrementOnce);

    // First run: the guard variable does not exist, so it is set to 1 and
    // string2's variable is incremented by m_specificID. Both helpers report
    // a CMessageSetVariable in queue order.
    REQUIRE(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CVariable* pGuard = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("ONCE_83H"));
    CVariable* pTarget = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("TARGET_83H"));
    REQUIRE(pGuard != NULL);
    REQUIRE(pTarget != NULL);
    CHECK(pGuard->m_intValue == 1);
    CHECK(pTarget->m_intValue == 17);
    const std::vector<CMessageSetVariable*> variables = Queued<CMessageSetVariable>();
    REQUIRE(variables.size() == 2);
    CHECK(variables[0]->m_sName == CString("ONCE_83H"));
    CHECK(variables[0]->m_nValue == 1);
    CHECK(variables[1]->m_sName == CString("TARGET_83H"));
    CHECK(variables[1]->m_nValue == 17);

    // Second run: the guard variable exists at 1, so the arm jumps straight
    // to the tail -- nothing set, nothing incremented, nothing queued.
    pCaller->SetCurrAction(incrementOnce);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pGuard->m_intValue == 1);
    CHECK(pTarget->m_intValue == 17);
    CHECK(Queued<CMessageSetVariable>().size() == 2);
    ClearMessages();
}

// Task 83h's journal arms, driven through ExecuteAction on the game's
// embedded journal. 245 JournalEntryDone flips the engine journal's current
// chapter's quest entry (wType 1) to done (2); 244 DeleteJournalEntry
// removes the entry whose STRREF text matches m_specificID from whichever
// chapter holds it, returning before any other entry is touched. The
// feedback line is muted as tests/test_gam.cpp's AddEntry test does.
TEST_CASE("ai: ExecuteAction 244 and 245 journal delete and quest done", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameJournal* pJournal = g_pBaldurChitin->GetObjectGame()->GetJournal();
    g_pBaldurChitin->m_pEngineJournal->m_nChapter = 0;

    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD textLevel = pOptions->m_nEffectTextLevel;
    pOptions->m_nEffectTextLevel = textLevel & ~0x10u;
    // A quest entry (type 1) in the engine journal's chapter 0, a plain
    // entry (type 4) in chapter 4: DeleteEntry scans all seven chapters.
    REQUIRE(pJournal->AddEntry(2100, 0, 30, 1) == TRUE);
    REQUIRE(pJournal->AddEntry(3100, 4, 40, 4) == TRUE);
    pOptions->m_nEffectTextLevel = textLevel;
    const WORD nEntries = pJournal->CountEntries();
    REQUIRE(nEntries >= 2);

    auto entryType = [pJournal](STRREF text) -> int {
        for (INT nChapter = 0; nChapter < CGameJournal::NUM_CHAPTERS; nChapter++) {
            POSITION pos = pJournal->m_aChapters[nChapter]->GetHeadPosition();
            while (pos != NULL) {
                CGameJournalEntry* pEntry = pJournal->m_aChapters[nChapter]->GetNext(pos);
                if (pEntry->m_strText == text) {
                    return pEntry->m_wType;
                }
            }
        }
        return -1;
    };
    REQUIRE(entryType(2100) == 1);

    // 245: the quest entry in chapter 0 reads done.
    CAIAction questDone;
    questDone.m_actionID = 245;
    questDone.m_specificID = 2100;
    pCaller->SetCurrAction(questDone);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(entryType(2100) == 2);

    // 244: the chapter-4 entry is gone and the quest entry survives intact.
    CAIAction deleteEntry;
    deleteEntry.m_actionID = 244;
    deleteEntry.m_specificID = 3100;
    pCaller->SetCurrAction(deleteEntry);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(entryType(3100) == -1);
    CHECK(pJournal->CountEntries() == nEntries - 1);
    CHECK(entryType(2100) == 2);
    ClearMessages();
}

// Task 83f's CInfGame::ChangeStoreMarkup (0x5C9C90) session arms, driven
// through the member with the network flags faked (GetSessionOpen and
// GetSessionHosting read plain members). The host broadcasts the markups and
// applies them to its server-side copy; a client that already owns a local
// copy of the store is the replay gate: it broadcasts a store demand with
// the markups and invalidates its local copy.
TEST_CASE("ai: ChangeStoreMarkup host and client arms with a session", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CNetwork& network = g_pChitin->cNetwork;
    const BOOLEAN bConnectionBefore = network.m_bConnectionEstablished;
    const BOOLEAN bHostBefore = network.m_bIsHost;

    // Host: the markups land in the server store's header.
    network.m_bConnectionEstablished = TRUE;
    network.m_bIsHost = TRUE;
    CHECK(pGame->ChangeStoreMarkup(CResRef("53HEGGR"), 150, 250, 10) == 1);
    {
        const std::vector<CMessageStoreMarkup*> markups = Queued<CMessageStoreMarkup>();
        REQUIRE(markups.size() == 1);
        CHECK(markups[0]->m_store == CResRef("53HEGGR"));
        CHECK(markups[0]->m_nBuyMarkUp == 150);
        CHECK(markups[0]->m_nSellMarkDown == 250);
        CHECK(markups[0]->m_nAdditionalMarkDown == 10);
        CHECK(markups[0]->GetMsgSubType() == CBaldurMessage::MSG_SUBTYPE_CMESSAGE_STORE_MARKUP);
    }
    // The host already owns the store: no store demand is queued.
    CHECK(Queued<CMessageStoreDemand>().empty());
    {
        CStore* pServerStore = NULL;
        for (int cnt = 0; cnt < 12; cnt++) {
            if (pGame->m_aServerStore[cnt] != NULL && pGame->m_aServerStore[cnt]->m_resRef == CResRef("53HEGGR")) {
                pServerStore = pGame->m_aServerStore[cnt];
            }
        }
        REQUIRE(pServerStore != NULL);
        CHECK(pServerStore->m_header.m_nBuyMarkUp == 150);
        CHECK(pServerStore->m_header.m_nSellMarkDown == 250);
        CHECK(pServerStore->m_header.m_nAdditionalMarkDown == 10);
    }
    // Unlike 0x5C9C90's sibling AddItemToBag, no release follows inside the
    // function; give the slot back while the session flags still hold.
    pGame->ReleaseServerStore(CResRef("53HEGGR"));
    // Nothing runs the broadcast in a test: drop it so the client part sees
    // only its own messages.
    ClearMessages();

    // Client that owns a local copy: the demand is the replay gate's, not a
    // fetch (which would need a live connection), and the local copy is
    // invalidated.
    {
        CStore local(CResRef("50NYM"));
        local.Marshal(pGame->m_sTempDir);
    }
    network.m_bIsHost = FALSE;
    CHECK(pGame->ChangeStoreMarkup(CResRef("50NYM"), 111, 222, 3) == 1);
    {
        const std::vector<CMessageStoreDemand*> demands = Queued<CMessageStoreDemand>();
        REQUIRE(demands.size() == 1);
        CHECK(demands[0]->m_store == CResRef("50NYM"));
        const std::vector<CMessageStoreMarkup*> markups = Queued<CMessageStoreMarkup>();
        REQUIRE(markups.size() == 1);
        CHECK(markups[0]->m_store == CResRef("50NYM"));
        CHECK(markups[0]->m_nBuyMarkUp == 111);
        CHECK(markups[0]->m_nSellMarkDown == 222);
        CHECK(markups[0]->m_nAdditionalMarkDown == 3);
    }
    network.m_bConnectionEstablished = bConnectionBefore;
    network.m_bIsHost = bHostBefore;

    // InvalidateStore cut the local copy to an 8-byte "STOOV9.0".
    {
        const std::vector<char> invalidated = TempFile("50NYM.STO");
        REQUIRE(invalidated.size() == 8);
        CHECK(std::memcmp(invalidated.data(), "STOOV9.0", 8) == 0);
    }

    ClearMessages();
}

// Task 83i's ExecuteAction arms, driven through ExecuteAction where the
// effect is observable without a world screen or a session: the marked and
// set AI types the 281/305 arms store, the world-timer move 279 makes, the
// fatigue effect 280 queues, and the 273/274 split between the queued
// CMessageToggleInterface (a provider is set) and the world screen's
// auto-unhide counter (the null provider's direct path, where the engine
// tests' missing visible area stops HideInterface/UnhideInterface at their
// first guard).
TEST_CASE("ai: ExecuteAction 273 274 279 280 281 and 305 hide unhide rest fatigue mark and target",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();

    // 281 MarkObject / 305 SetMyTarget: the resolved actee's AI type lands
    // in field_342 / field_37E; an actee nobody resolves for stores NOONE.
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    pCaller->SetCurrAction(CAIAction(281, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCaller->field_342.Equal(pSprite->GetAIType()));
    CHECK(!pCaller->field_342.Equal(CAIObjectType::NOONE));

    pCaller->SetCurrAction(CAIAction(305, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCaller->field_37E.Equal(pSprite->GetAIType()));

    CAIAction markNobody;
    markNobody.m_actionID = 281;
    pCaller->SetCurrAction(markNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCaller->field_342.Equal(CAIObjectType::NOONE));

    CAIAction targetNobody;
    targetNobody.m_actionID = 305;
    pCaller->SetCurrAction(targetNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCaller->field_37E.Equal(CAIObjectType::NOONE));

    // 279 AdvanceTime: the specifics are a time of day; 3600 ms past the
    // current one moves the world timer by exactly 3600 (AdvanceCurrentTime
    // applies the delta when it clears 900, and without a session the plain
    // add is the only step). FeedBack gets 3600, under the hour that its
    // FEEDBACK_TIMEPASSEDREST branch displays at, so nothing else moves.
    CTimerWorld* pWorldTimer = g_pBaldurChitin->GetObjectGame()->GetWorldTimer();
    const ULONG nTimeBefore = pWorldTimer->m_gameTime;
    CAIAction rest;
    rest.m_actionID = 279;
    rest.m_specificID = static_cast<LONG>(nTimeBefore % CTimerWorld::TIMESCALE_MSEC_PER_DAY) + 3600;
    pCaller->SetCurrAction(rest);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pWorldTimer->m_gameTime == nTimeBefore + 3600);

    // 280 AdvanceTime (the second one): with a resolved sprite and empty
    // specifics it decodes a fatigue effect carrying specifics2 as its
    // amount and queues it; the caller is the source, the sprite the target.
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(280, actee, 0, 25, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageAddEffect*> adds = Queued<CMessageAddEffect>();
        REQUIRE(adds.size() == 1);
        CHECK(adds[0]->m_sourceId == pCaller->m_id);
        CHECK(adds[0]->m_targetId == pSprite->m_id);
        REQUIRE(adds[0]->m_effect != NULL);
        CHECK(dynamic_cast<CGameEffectFatigue*>(adds[0]->m_effect) != NULL);
        std::unique_ptr<ITEM_EFFECT> pEffect(adds[0]->m_effect->GetItemEffect());
        REQUIRE(pEffect != NULL);
        CHECK(pEffect->effectID == CGAMEEFFECT_FATIGUE);
        CHECK(pEffect->effectAmount == 25);
        CHECK(pEffect->dwFlags == 1);
        CHECK(pEffect->durationType == 1);
    }

    // The same arm answers the entry default and queues nothing when the
    // specifics are non-zero (the rest is refused) or no sprite resolves.
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(280, actee, 1, 25, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CAIAction fatigueNobody;
    fatigueNobody.m_actionID = 280;
    pCaller->SetCurrAction(fatigueNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(Queued<CMessageAddEffect>().empty());

    // 273/274 HideGUI/UnhideGUI: with a service provider the arms queue the
    // message the session side runs, addressed to the acting object itself.
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    ClearMessages();
    CAIAction hide;
    hide.m_actionID = 273;
    pCaller->SetCurrAction(hide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageToggleInterface*> toggles = Queued<CMessageToggleInterface>();
        REQUIRE(toggles.size() == 1);
        CHECK(toggles[0]->m_bHide == TRUE);
        CHECK(toggles[0]->m_sourceId == pCaller->m_id);
        CHECK(toggles[0]->m_targetId == pCaller->m_id);
    }
    ClearMessages();
    CAIAction unhide;
    unhide.m_actionID = 274;
    pCaller->SetCurrAction(unhide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageToggleInterface*> toggles = Queued<CMessageToggleInterface>();
        REQUIRE(toggles.size() == 1);
        CHECK(toggles[0]->m_bHide == FALSE);
    }

    // The null provider drives the world screen directly. HideInterface and
    // UnhideInterface stop at their field_4A24 guard without a visible area,
    // and the UI manager's hidden flag never flips, so what stays observable
    // is the auto-unhide counter: each HideGUI counts one in, UnhideGUI acts
    // only while the counter sub_453780 reads is above zero and counts it
    // back out.
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    CScreenWorld* pWorld = g_pBaldurChitin->GetScreenWorld();
    REQUIRE(pWorld != NULL);
    const INT nAutoUnhideBefore = pWorld->m_nAutoUnhideInterface;
    const int nField4A24 = g_pBaldurChitin->field_4A24;
    g_pBaldurChitin->field_4A24 = 0;
    pWorld->m_nAutoUnhideInterface = 0;
    ClearMessages();

    pCaller->SetCurrAction(hide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pWorld->m_nAutoUnhideInterface == 1);
    pCaller->SetCurrAction(hide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pWorld->m_nAutoUnhideInterface == 2);
    CHECK(Queued<CMessageToggleInterface>().empty());

    pCaller->SetCurrAction(unhide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pWorld->m_nAutoUnhideInterface == 1);
    pCaller->SetCurrAction(unhide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pWorld->m_nAutoUnhideInterface == 0);
    pCaller->SetCurrAction(unhide);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pWorld->m_nAutoUnhideInterface == 0);

    pWorld->m_nAutoUnhideInterface = nAutoUnhideBefore;
    g_pBaldurChitin->field_4A24 = nField4A24;
    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    ClearMessages();
}

// Task 83j's ExecuteAction arms, driven through ExecuteAction where the
// effect is observable without a session or a video stack: the name strrefs
// 312/313 store on the resolved sprite (with the unchanged-strref skip),
// the area header words 301/303 rewrite and mirror (the update messages
// these and 312/313/314 post are to-others broadcasts, which the
// single-player handler consumes without queueing -- the original drops
// them on its to-self-FALSE path at 0x4F77BF -- so the queue staying empty
// is itself the pinned behavior), and the party experience effect 315
// queues on the challenge-rating path.
TEST_CASE("ai: ExecuteAction 301 303 312 313 314 and 315 area flags names endgame and party CR xp",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;

    // 312 SetApparentNameSTRREF / 313 SetRegularNameSTRREF: the resolved
    // sprite's base-stats name takes the specifics; the sprite update the
    // arm posts is a to-others broadcast, so nothing stays local. An
    // unchanged strref skips the message entirely; a target that resolves
    // to nothing takes the miss tail that forces ACTION_DONE.
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    const DWORD nApparent = pSprite->m_baseStats.m_apparentName + 7;
    const DWORD nName = pSprite->m_baseStats.m_name + 7;
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(312, actee, nApparent, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_apparentName == nApparent);
    CHECK(Queued<CMessageSpriteUpdate>().empty());
    // The same strref again: stored value unchanged, nothing posted.
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(312, actee, nApparent, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_apparentName == nApparent);
    CHECK(Queued<CMessageSpriteUpdate>().empty());
    CAIAction renameNobody;
    renameNobody.m_actionID = 312;
    pCaller->SetCurrAction(renameNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageSpriteUpdate>().empty());

    ClearMessages();
    pCaller->SetCurrAction(CAIAction(313, actee, nName, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_name == nName);
    CHECK(Queued<CMessageSpriteUpdate>().empty());
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(313, actee, nName, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_name == nName);
    CHECK(Queued<CMessageSpriteUpdate>().empty());

    // 301 SetExtendedNight / 303 SetRestEncounterChance: the caller's area
    // header is rewritten and mirrored into the infinity, in control or
    // out (the update messages post only in control, and being
    // to-others broadcasts they never queue locally either way). A
    // constructed CGameArea is its header, rest-encounter block and
    // infinity with the constructor's inits, which is all these arms touch.
    CGameArea* pArea = new CGameArea(0);
    CGameArea* pAreaBefore = pCaller->m_pArea;
    pCaller->m_pArea = pArea;
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    CAIAction night;
    night.m_actionID = 301;
    CAIAction restChance;
    restChance.m_actionID = 303;

    pArea->m_header.m_areaType = 0x1205;
    night.m_specificID = 1;
    pCaller->SetCurrAction(night);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pArea->m_header.m_areaType == 0x1245);
    CHECK(pArea->m_cInfinity.m_areaType == 0x1245);
    CHECK(Queued<CMessageSetAreaType>().empty());
    // Specifics zero clears the bit again.
    ClearMessages();
    night.m_specificID = 0;
    pCaller->SetCurrAction(night);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pArea->m_header.m_areaType == 0x1205);
    CHECK(pArea->m_cInfinity.m_areaType == 0x1205);
    CHECK(Queued<CMessageSetAreaType>().empty());

    // 303's both chances clamp at 100 (the min inline at 0x45EA80).
    ClearMessages();
    restChance.m_specificID = 150;
    restChance.m_specificID2 = 30;
    pCaller->SetCurrAction(restChance);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pArea->m_headerRestEncounter.m_probDay == 100);
    CHECK(pArea->m_headerRestEncounter.m_probNight == 30);
    CHECK(Queued<CMessageSetAreaRestEncounter>().empty());

    // Out of control (another provider, a remote player that is not this
    // object): the fields still change, and still nothing queues.
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    const PLAYER_ID idLocalBefore = g_pChitin->cNetwork.m_idLocalPlayer;
    g_pChitin->cNetwork.m_idLocalPlayer = pCaller->m_remotePlayerID + 1;
    ClearMessages();
    night.m_specificID = 1;
    pCaller->SetCurrAction(night);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pArea->m_header.m_areaType == 0x1245);
    CHECK(Queued<CMessageSetAreaType>().empty());
    g_pChitin->cNetwork.m_idLocalPlayer = idLocalBefore;
    // 315's effect travels by Send, which queues only for the local
    // player's objects or under the null provider; put the null provider
    // back for it.
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;

    // 314 EndGame: the arm runs ReadyCharacterTerminationSequence on the
    // game, reads the VISIBLE area's id for the EndGame message it posts
    // (a to-others broadcast, consumed by the single-player handler like
    // the others), and takes the done tail. The termination body needs the
    // object cursor and video stack the engine tests never build
    // (SetCursor derefs the current video mode), so the game-save guard
    // suppresses it here and the pins are: no termination state touched,
    // no message left local, ACTION_DONE. The area must be registered or
    // GetVisibleArea returns NULL and the arm would crash reading its id.
    CGameArea* pEndArea = new CGameArea(9);
    const BYTE nVisibleArea = pGame->m_visibleArea;
    CGameArea* pVisibleBefore = pGame->m_gameAreas[nVisibleArea];
    const int nTermGuard = pGame->m_gameSave.field_1AC;
    const DWORD nModeBefore = pGame->m_gameSave.m_mode;
    pGame->m_gameAreas[nVisibleArea] = pEndArea;
    pGame->m_gameSave.field_1AC = 1;
    ClearMessages();
    CAIAction endGame;
    endGame.m_actionID = 314;
    endGame.m_specificID = 42;
    pCaller->SetCurrAction(endGame);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pGame->m_nCharacterTerminationSequenceDelay == 0);
    CHECK(pGame->m_gameSave.m_mode == nModeBefore);
    CHECK(Queued<CMessageEndGame>().empty());
    pGame->m_gameSave.field_1AC = nTermGuard;
    pGame->m_gameAreas[nVisibleArea] = pVisibleBefore;

    // 315 AddExperiencePartyCR: the CR flag is what separates it from case
    // 164. A party of one living member receives one experience effect
    // (104); the amount is the challenge-rating share, table-driven through
    // MoncRate, so the assertion compares it with a direct
    // AddExperienceParty(specifics, FALSE, TRUE) call rather than a number.
    // The sprite also needs a class level matching m_nLevel: the share
    // calls GetMulticlassingPenalty, whose second-chance loop only
    // re-picks iBestClass from classes with a non-zero level and then
    // asserts in CDerivedStats::GetClassLevel when iBestClass is still 0 --
    // a levelled character without class levels cannot exist in the
    // original, so seed a level-3 fighter.
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;
    pSprite->m_derivedStats.m_nLevel = 3;
    pSprite->m_derivedStats.m_nClassLevels[CAIOBJECTTYPE_C_FIGHTER - 1] = 3;
    const SHORT nCharacters = pGame->m_nCharacters;
    const LONG portrait0 = pGame->m_characterPortraits[0];
    pGame->m_nCharacters = 1;
    pGame->m_characterPortraits[0] = pSprite->m_id;
    ClearMessages();
    CAIAction award;
    award.m_actionID = 315;
    award.m_specificID = 150;
    pCaller->SetCurrAction(award);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    LONG nArmAmount = -1;
    {
        const std::vector<CMessageAddEffect*> adds = Queued<CMessageAddEffect>();
        REQUIRE(adds.size() == 1);
        CHECK(adds[0]->m_sourceId == pSprite->m_id);
        CHECK(adds[0]->m_targetId == pSprite->m_id);
        REQUIRE(adds[0]->m_effect != NULL);
        std::unique_ptr<ITEM_EFFECT> pXP(adds[0]->m_effect->GetItemEffect());
        REQUIRE(pXP != NULL);
        CHECK(pXP->effectID == CGAMEEFFECT_XP);
        nArmAmount = pXP->effectAmount;
    }
    ClearMessages();
    CHECK(nArmAmount == pGame->AddExperienceParty(150, FALSE, TRUE));
    pGame->m_nCharacters = nCharacters;
    pGame->m_characterPortraits[0] = portrait0;

    pCaller->m_pArea = pAreaBefore;
    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    ClearMessages();
}

// CGameAIBase::ExecuteAction (0x44DC10) arms 83k (Task 83k): 316 AddFeat
// (0x452318-0x4523BA), 242 ChangeStat (0x4523BA-0x45248C), 251
// DropInventoryEX (0x45248C-0x452575), 318 SetCreatureAreaFlag
// (0x452575-0x4525EA), 304 AddHP (0x45074B-0x4507E1) and 288 SetDoorFlag
// (0x451638-0x4516C3), each driven through ExecuteAction on a real sprite
// (and a real door for 288, whose share resolution type-checks the target
// through the object's virtual type). AR1000 is loaded once for the drop
// (the ground pile lives in the sprite's area) and the door.
TEST_CASE("ai: ExecuteAction 316 242 251 318 304 and 288 feats stats drops area flags hp and door flags",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear, so a pump thread stands in for the display loop (the AR3000
    // CreateCreature case above uses the same fixture).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea1000 = pGame->LoadArea(CString("AR1000"), 0, FALSE, FALSE);
    REQUIRE(pArea1000 != NULL);
    pArea1000->SortLists();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // 316 AddFeat: specifics1 names the feat. AEGIS_OF_RIME (0) is a plain
    // feat, so sub_763A40's fall-through grants it (the arm passes 0 as the
    // second argument, which skips the prerequisite table), and sub_71E240
    // adds the feat's +5 cold resistance to the base stats. A second call
    // finds the bit set, refuses the grant and still reports ACTION_DONE
    // (0x452318: the refusal keeps the done return). A target that resolves
    // to nothing takes the 0x451451 error tail.
    pSprite->SetFeatValue(CGAMESPRITE_FEAT_AEGIS_OF_RIME, 0);
    pSprite->m_baseStats.m_resistColdBase = 0;
    pCaller->SetCurrAction(CAIAction(316, actee, CGAMESPRITE_FEAT_AEGIS_OF_RIME, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->GetFeatValue(CGAMESPRITE_FEAT_AEGIS_OF_RIME) == 1);
    CHECK(pSprite->m_baseStats.m_resistColdBase == 5);
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(316, actee, CGAMESPRITE_FEAT_AEGIS_OF_RIME, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_resistColdBase == 5);
    CAIAction featNobody;
    featNobody.m_actionID = 316;
    pCaller->SetCurrAction(featNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 242 ChangeStat: specifics1/2 and the low byte of specifics3 are the
    // stat, the value and the modify flag, applied through SetAtOffset;
    // STAT_MAXHITPOINTS writes both the base and the derived maximum. The
    // message half runs only outside the null provider, but
    // CMessageChangeStat is BROADCAST_FORCED_OTHERS: Broadcast's
    // to-self-FALSE counterpart deletes it before the queue in every
    // provider (the original hands it to the network marshal instead --
    // omitted per docs/adr/0002-single-player-only.md), so both halves pin
    // on the fields and an empty queue.
    pSprite->SetAtOffset(STAT_MAXHITPOINTS, 20, FALSE);
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(242, actee, STAT_MAXHITPOINTS, 37, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 37);
    CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 37);
    CHECK(Queued<CMessageChangeStat>().empty());
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(242, actee, STAT_MAXHITPOINTS, 41, 1));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 78);
    CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 78);
    CHECK(Queued<CMessageChangeStat>().empty());
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    // A NULL target jumps straight to the done tail, returning the entry
    // default ACTION_ERROR (0x452487) without releasing a share.
    CAIAction statNobody;
    statNobody.m_actionID = 242;
    pCaller->SetCurrAction(statNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 251 DropInventoryEX: the target's whole inventory is dropped to a
    // ground pile in its area, gold included (the pile item is MISC07);
    // without an area DropInventory early-outs as ACTION_DONE with nothing
    // touched. The equipment broadcast needs an open session, a target out
    // of control and a running cutscene at once, which the test engine
    // never has, so nothing queues either way.
    pSprite->m_baseStats.m_gold = 25;
    pCaller->SetCurrAction(CAIAction(251, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_gold == 25);
    CHECK(Queued<CMessageSpriteEquipment>().empty());
    pSprite->m_pArea = pArea1000;
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(251, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_gold == 0);
    CHECK(Queued<CMessageSpriteEquipment>().empty());
    pSprite->m_pArea = NULL;
    CAIAction dropNobody;
    dropNobody.m_actionID = 251;
    pCaller->SetCurrAction(dropNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 318 SetCreatureAreaFlag: specifics1 is a mask on the target's
    // base-stats flags; a non-zero specifics2 sets it, a zero clears it
    // (0x452575).
    pSprite->m_baseStats.m_flags = 0;
    pCaller->SetCurrAction(CAIAction(318, actee, 0x10, 1, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_flags == 0x10);
    pCaller->SetCurrAction(CAIAction(318, actee, 0x10, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_flags == 0);
    CAIAction flagNobody;
    flagNobody.m_actionID = 318;
    pCaller->SetCurrAction(flagNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 304 AddHP: m_specificID is added to the base hit points, clamped at
    // the derived maximum (0x45074B). A dead target is skipped but still
    // reports ACTION_DONE, and so does a target that resolves to nothing
    // (0x44E1DB tail).
    pSprite->SetAtOffset(STAT_MAXHITPOINTS, 20, FALSE);
    pSprite->m_baseStats.m_hitPoints = 5;
    pCaller->SetCurrAction(CAIAction(304, actee, 10, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    pCaller->SetCurrAction(CAIAction(304, actee, 10, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 20);
    pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
    pSprite->m_baseStats.m_hitPoints = 5;
    pCaller->SetCurrAction(CAIAction(304, actee, 10, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 5);
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;
    CAIAction hpNobody;
    hpNobody.m_actionID = 304;
    pCaller->SetCurrAction(hpNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    // 288 SetDoorFlag: specifics1 is a mask on the resolved door's flags; a
    // non-zero specifics2 sets it, a zero clears it, and the new status is
    // always announced through CMessageDoorStatus with the caller's own id
    // (0x451638). A target that resolves to nothing takes the ACTION_DONE
    // tail (0x45164C).
    CGameDoor* pDoor = NULL;
    {
        CTypedPtrList<CPtrList, int*>& list = pArea1000->m_lVertSort;
        POSITION pos = list.GetHeadPosition();
        while (pos != NULL && pDoor == NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list.GetNext(pos)));
            CGameObject* pObject;
            if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH,
                    &pObject, INFINITE)
                == CGameObjectArray::SUCCESS) {
                if (pObject->GetObjectType() == CGameObject::TYPE_DOOR) {
                    pDoor = static_cast<CGameDoor*>(pObject);
                }
                pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }
    REQUIRE(pDoor != NULL);
    const DWORD dwFlagsBefore = pDoor->GetFlags();
    const DWORD dwMask = 0x20000000;
    REQUIRE((dwFlagsBefore & dwMask) == 0);
    CAIObjectType doorActee(0, 0, 0, 0, 0, 0, 0, 0, pDoor->m_id, 0, 0);
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(288, doorActee, dwMask, 1, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pDoor->GetFlags() == (dwFlagsBefore | dwMask));
    {
        const std::vector<CMessageDoorStatus*> statuses = Queued<CMessageDoorStatus>();
        REQUIRE(statuses.size() == 1);
        CHECK(statuses[0]->m_dwFlags == (dwFlagsBefore | dwMask));
    }
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(288, doorActee, dwMask, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pDoor->GetFlags() == dwFlagsBefore);
    REQUIRE(Queued<CMessageDoorStatus>().size() == 1);
    CAIAction doorNobody;
    doorNobody.m_actionID = 288;
    pCaller->SetCurrAction(doorNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    ClearMessages();
}

// CGameAIBase::ExecuteAction (0x44DC10) arms 83l (Task 83l): 233
// HideCreature (0x452035-0x4520ED), 235 PlaySequence
// (0x450B52-0x450BD9), 278 DropInventoryEXExclude
// (0x4512E9-0x451362), 282 OverrideAreaDifficulty
// (0x44E042-0x44E12E), 283 SetCriticalPathObject
// (0x44E12E-0x44E223) and 325 SetAreaFlags (0x45262C-0x4526AA), each
// driven through ExecuteAction on a real sprite, and a real container and
// door for 278's type dispatch. AR1000 is loaded once: the hide/show and
// drop halves need the sprite to live in an area.
TEST_CASE("ai: ExecuteAction 233 235 278 282 283 and 325 hide sequence drop exclude difficulty critical path and area flags",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The main loop's drawn frames: the area load waits for m_bDisplayStale
    // to clear, so a pump thread stands in for the display loop.
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea1000 = pGame->LoadArea(CString("AR1000"), 0, FALSE, FALSE);
    REQUIRE(pArea1000 != NULL);
    pArea1000->SortLists();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();
    const CGameArea* pCallerAreaBefore = pCaller->m_pArea;
    CGameSprite* pSprite = MakeSprite();
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    pSprite->m_pos = CPoint(10 * CPathSearch::GRID_SQUARE_SIZEX,
        10 * CPathSearch::GRID_SQUARE_SIZEY);

    // 233 HideCreature: the low byte of specifics1 is sub_75F3D0's hide
    // argument and its return is the action return. Without an area the
    // callee refuses (ACTION_ERROR) and the hidden bit stays clear; with
    // the sprite placed in AR1000 the arm hides (bit set, done) and shows
    // (bit clear, done), and a second non-zero argument (2) errors
    // leaving the bit alone -- that matrix is sub_75F3D0's own, pinned in
    // test_sprite_actions.cpp; here it goes through the arm. The
    // CMessage90 half runs only outside the null provider and is
    // BROADCAST_FORCED_OTHERS: Broadcast's to-self-FALSE counterpart
    // deletes it before the queue (the original hands it to the network
    // marshal instead -- omitted per docs/adr/0002-single-player-only.md).
    // A target that resolves to nothing takes the 0x452054 error tail.
    pCaller->SetCurrAction(CAIAction(233, actee, 1, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pSprite->m_baseStats.field_294 == 0);

    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_TCP_IP;
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(233, actee, 1, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pSprite->m_baseStats.field_294 == 0);
    CHECK(Queued<CMessage90>().empty());
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;

    pSprite->m_pArea = pArea1000;
    pCaller->SetCurrAction(CAIAction(233, actee, 1, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.field_294 == 1);
    pCaller->SetCurrAction(CAIAction(233, actee, 2, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pSprite->m_baseStats.field_294 == 1);
    pCaller->SetCurrAction(CAIAction(233, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.field_294 == 0);

    CAIAction hideNobody;
    hideNobody.m_actionID = 233;
    pCaller->SetCurrAction(hideNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 235 PlaySequence: specifics1 is the sequence index, bounds-checked
    // against the SEQ_* table's first (SEQ_ATTACK) and last (SEQ_SLEEP)
    // entries as signed values. In range queues CMessageSetSequence
    // carrying the sequence to the target; out of range and an unresolved
    // target both take the forced-done tail (0x44E1DB) without a message.
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(235, actee, CGameSprite::SEQ_ATTACK, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageSetSequence*> sequences = Queued<CMessageSetSequence>();
        REQUIRE(sequences.size() == 1);
        CHECK(sequences[0]->m_sequence == CGameSprite::SEQ_ATTACK);
        CHECK(sequences[0]->m_sourceId == pCaller->m_id);
        CHECK(sequences[0]->m_targetId == pSprite->m_id);
    }
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(235, actee, CGameSprite::SEQ_SLEEP, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    REQUIRE(Queued<CMessageSetSequence>().size() == 1);
    ClearMessages();

    // Out of range (100 above the jg bound, -1 below the jl bound): the arm
    // jumps to the forced-done tail before it sets the share flag, so the
    // release block drops the target with ReleaseDeny even though the
    // acquire (sub_40CCA0 through sub_45C290) took a GetShare -- a latent
    // imbalance the original listing carries too (the flag slot is only
    // written at 0x450B93, after both bounds tests). Pre-arming the deny
    // count replays the original's release without tripping the ported
    // m_denyCounts assert, and the share the acquire took stays held by the
    // original's bookkeeping; the test drops it again afterwards.
    for (const LONG nOutOfRange : { 100, -1 }) {
        CGameObject* pDenyHeld = NULL;
        REQUIRE(pGame->GetObjectArray()->GetDeny(pSprite->m_id,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pDenyHeld,
                    INFINITE)
            == CGameObjectArray::SUCCESS);
        pCaller->SetCurrAction(CAIAction(235, actee, nOutOfRange, 0, 0));
        CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
        CHECK(Queued<CMessageSetSequence>().empty());
        REQUIRE(pGame->GetObjectArray()->ReleaseShare(pSprite->m_id,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE)
            == CGameObjectArray::SUCCESS);
    }
    CAIAction sequenceNobody;
    sequenceNobody.m_actionID = 235;
    pCaller->SetCurrAction(sequenceNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageSetSequence>().empty());

    // 278 DropInventoryEXExclude: the target's droppable inventory goes to
    // its ground pile except the item whose resref equals the action's
    // second string, so a non-matching string drops the item too. An
    // unresolved target takes the 0x451451 error exit. The no-area
    // early-out is DropInventory's own, pinned by the 251
    // DropInventoryEX test above.
    pSprite->m_baseStats.m_gold = 25;
    REQUIRE(pSprite->m_equipment.m_items[18] == NULL);
    CItem* pPotion = new CItem(CResRef("00POTN01"), 3, 0, 0, 0, 0);
    REQUIRE((pPotion->GetFlagsFile() & 0x4) != 0);
    pSprite->m_equipment.m_items[18] = pPotion;
    CAIAction dropExclude;
    dropExclude.m_actionID = 278;
    dropExclude.m_acteeID = actee;
    dropExclude.m_string1 = "00POTN01";
    pCaller->SetCurrAction(dropExclude);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_equipment.m_items[18] == pPotion);
    CHECK(pSprite->m_baseStats.m_gold == 0);

    dropExclude.m_string1 = "00POTN02"; // no match: the potion drops too
    pCaller->SetCurrAction(dropExclude);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_equipment.m_items[18] == NULL);

    CAIAction dropNobody;
    dropNobody.m_actionID = 278;
    pCaller->SetCurrAction(dropNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // A container target now reaches the ported 0x481D30; any other type
    // (the door) errors at 0x451451. Both ids come from the vert-sort walk
    // the 288 SetDoorFlag test above uses, over the sorted and background
    // lists.
    auto findTypeId = [pGame, pArea1000](BYTE type) -> LONG {
        CTypedPtrList<CPtrList, int*>* lists[2] = { &pArea1000->m_lVertSort,
            &pArea1000->m_lVertSortBack };
        for (CTypedPtrList<CPtrList, int*>* list : lists) {
            POSITION pos = list->GetHeadPosition();
            while (pos != NULL) {
                LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
                CGameObject* pObject;
                if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH,
                        &pObject, INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    const LONG nId = pObject->GetId();
                    pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    if (pObject->GetObjectType() == type) {
                        return nId;
                    }
                }
            }
        }
        return CGameObjectArray::INVALID_INDEX;
    };
    const LONG nContainerId = findTypeId(CGameObject::TYPE_CONTAINER);
    REQUIRE(nContainerId != CGameObjectArray::INVALID_INDEX);
    CAIObjectType containerActee(0, 0, 0, 0, 0, 0, 0, 0, nContainerId, 0, 0);
    CGameObject* pContainerObject;
    REQUIRE(pGame->GetObjectArray()->GetShare(nContainerId, CGameObjectArray::THREAD_ASYNCH,
                &pContainerObject, INFINITE)
        == CGameObjectArray::SUCCESS);
    CGameContainer* pContainer = static_cast<CGameContainer*>(pContainerObject);

    // 0x481D30 through case 278: a resref nothing matches drops every item
    // to the container's ground pile. CompressContainer runs (this harness
    // is InControl), so the emptied slots leave the list.
    CAIAction containerDrop(278, containerActee, 0, 0, 0);
    containerDrop.m_string1 = "00NOMATCH";
    pCaller->SetCurrAction(containerDrop);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pContainer->GetNumItems() == 0);

    // The named item stays and the other one moves to the pile.
    CItem* pKeep = new CItem(CResRef("00POTN01"), 3, 0, 0, 0, 0);
    CItem* pLose = new CItem(CResRef("00POTN02"), 1, 0, 0, 0, 0);
    REQUIRE(pKeep->GetUsageCount(0) == 3); // the ctor's first usage count is the stack
    pContainer->m_lstItems.AddTail(pKeep);
    pContainer->m_lstItems.AddTail(pLose);
    containerDrop.m_string1 = "00POTN01";
    pCaller->SetCurrAction(containerDrop);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pContainer->GetNumItems() == 1);
    REQUIRE(pContainer->GetItem(0) != NULL);
    CHECK(pContainer->GetItem(0)->GetResRef() == CResRef("00POTN01"));

    REQUIRE(pGame->GetObjectArray()->ReleaseShare(nContainerId, CGameObjectArray::THREAD_ASYNCH,
                INFINITE)
        == CGameObjectArray::SUCCESS);

    // 0x481C50 and 0x481EB0 have no ExecuteAction path: they are driven
    // directly, on a fresh container whose list has no NULL slots (the
    // exe's count walk stops at the first one).
    CGameContainer* pCounted = new CGameContainer(pArea1000, CRect(400, 400, 416, 416));
    CItem* pStack = new CItem(CResRef("00POTN01"), 3, 0, 0, 0, 0);
    CItem* pSingle = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);
    pCounted->m_lstItems.AddTail(pStack);
    pCounted->m_lstItems.AddTail(pSingle);
    // The named resref counts nothing, the stack counts its charges, the
    // gem counts one.
    CHECK(pCounted->CountItemExcept(CString("00POTN01")) == 1);
    CHECK(pCounted->CountItemExcept(CString("00GEM01")) == 3);

    // 0x481EB0: the slot's item leaves for the game's disposable list and
    // the add message goes out whether or not the slot held one.
    ClearMessages();
    pCounted->RemoveItem(0);
    CHECK(pCounted->GetNumItems() == 1);
    REQUIRE(pCounted->GetItem(0) != NULL);
    CHECK(pCounted->GetItem(0)->GetResRef() == CResRef("00GEM01"));
    pCounted->RemoveItem(0);
    CHECK(pCounted->GetNumItems() == 0);
    pCounted->RemoveItem(0); // empty list - the message still goes out
    CHECK(Queued<CMessageContainerAddItem>().size() == 3);

    const LONG nDoorId = findTypeId(CGameObject::TYPE_DOOR);
    REQUIRE(nDoorId != CGameObjectArray::INVALID_INDEX);
    CAIObjectType doorActee(0, 0, 0, 0, 0, 0, 0, 0, nDoorId, 0, 0);
    pCaller->SetCurrAction(CAIAction(278, doorActee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // 282 OverrideAreaDifficulty: string1 names the area, specifics1 the
    // level clamped to [-1, 1]. A name not yet in CInfGame::field_4BDC
    // grows the list by one entry with the additive bit set and the
    // absolute bit clear; the same name in any case updates that entry in
    // place, and an empty name or zero specifics leaves the list
    // untouched. Every path reports ACTION_DONE.
    const INT nLevelsBefore = static_cast<INT>(pGame->field_4BDC.GetCount());
    auto findAreaLevel = [pGame](const CString& sArea) -> CInfGameAreaLevel* {
        POSITION pos = pGame->field_4BDC.GetHeadPosition();
        while (pos != NULL) {
            CInfGameAreaLevel* pCurrent = pGame->field_4BDC.GetNext(pos);
            if (pCurrent != NULL && pCurrent->m_sArea.CompareNoCase(sArea) == 0) {
                return pCurrent;
            }
        }
        return NULL;
    };
    CAIAction override;
    override.m_actionID = 282;
    override.m_string1 = "AR1000";
    override.m_specificID = 1;
    pCaller->SetCurrAction(override);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(static_cast<INT>(pGame->field_4BDC.GetCount()) == nLevelsBefore + 1);
    CInfGameAreaLevel* pLevel = findAreaLevel(CString("AR1000"));
    REQUIRE(pLevel != NULL);
    CHECK(pLevel->m_dwFlags == 0x2);
    CHECK(pLevel->m_nLevel == 1);

    override.m_string1 = "ar1000"; // case-insensitive match, in place
    override.m_specificID = -5; // clamped to -1, stored as a byte
    pCaller->SetCurrAction(override);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(static_cast<INT>(pGame->field_4BDC.GetCount()) == nLevelsBefore + 1);
    CHECK(pLevel->m_dwFlags == 0x2);
    CHECK(pLevel->m_nLevel == 0xFF);

    override.m_string1 = "AR2000"; // a second name grows the list again
    override.m_specificID = 5; // clamped to 1
    pCaller->SetCurrAction(override);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(static_cast<INT>(pGame->field_4BDC.GetCount()) == nLevelsBefore + 2);
    CInfGameAreaLevel* pLevel2 = findAreaLevel(CString("AR2000"));
    REQUIRE(pLevel2 != NULL);
    CHECK(pLevel2->m_dwFlags == 0x2);
    CHECK(pLevel2->m_nLevel == 1);

    override.m_specificID = 0; // zero specifics: the guard skips
    pCaller->SetCurrAction(override);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(static_cast<INT>(pGame->field_4BDC.GetCount()) == nLevelsBefore + 2);
    override.m_string1 = ""; // empty name: the guard skips
    override.m_specificID = 1;
    pCaller->SetCurrAction(override);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(static_cast<INT>(pGame->field_4BDC.GetCount()) == nLevelsBefore + 2);

    // 283 SetCriticalPathObject: a non-zero specifics1 sets the base-stats
    // critical-path bit (0x2000) on the resolved sprite, a zero clears it,
    // always with ACTION_DONE. CMessageSpriteUpdate is BROADCAST_OTHERS,
    // so the single-player arm deletes it before the queue. A target that
    // resolves to nothing takes the forced-done tail (0x44E1DB).
    pSprite->m_baseStats.m_flags = 0;
    ClearMessages();
    pCaller->SetCurrAction(CAIAction(283, actee, 1, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_flags == 0x2000);
    CHECK(Queued<CMessageSpriteUpdate>().empty());
    pCaller->SetCurrAction(CAIAction(283, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_flags == 0);
    CAIAction criticalNobody;
    criticalNobody.m_actionID = 283;
    pCaller->SetCurrAction(criticalNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    // 325 SetAreaFlags: specifics2 picks the operation on the caller's
    // area header flags -- 1 and, 2 or, 3 xor -- with specifics1 as the
    // mask; any other value writes nothing and never dereferences the
    // area. The three write arms leave the result register alone, so the
    // action reports the ACTION_ERROR entry default even when it wrote
    // (mirrored from the listing).
    CAIAction areaFlags;
    areaFlags.m_actionID = 325;
    areaFlags.m_specificID = 0x10;
    areaFlags.m_specificID2 = 0; // invalid op with no caller area: no write
    pCaller->SetCurrAction(areaFlags);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    areaFlags.m_specificID2 = 4;
    pCaller->SetCurrAction(areaFlags);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    pCaller->m_pArea = pArea1000;
    const DWORD dwHeaderFlagsBefore = pArea1000->m_header.m_flags;
    pArea1000->m_header.m_flags = 0x0000F0F0;

    areaFlags.m_specificID = 0x0F00;
    areaFlags.m_specificID2 = 2; // or
    pCaller->SetCurrAction(areaFlags);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pArea1000->m_header.m_flags == 0xFFF0);
    areaFlags.m_specificID = 0x00F0;
    areaFlags.m_specificID2 = 3; // xor
    pCaller->SetCurrAction(areaFlags);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pArea1000->m_header.m_flags == 0xFF00);
    areaFlags.m_specificID = 0x0F00;
    areaFlags.m_specificID2 = 1; // and
    pCaller->SetCurrAction(areaFlags);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pArea1000->m_header.m_flags == 0x0F00);
    areaFlags.m_specificID2 = 0; // invalid op: no write
    pCaller->SetCurrAction(areaFlags);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pArea1000->m_header.m_flags == 0x0F00);

    pArea1000->m_header.m_flags = dwHeaderFlagsBefore;

    pSprite->m_pArea = NULL;
    pCaller->m_pArea = const_cast<CGameArea*>(pCallerAreaBefore);
    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    ClearMessages();
}

// CGameAIBase::ExecuteAction (0x44DC10) arms 83m (Task 83m): 185
// SetMasterArea (0x44FDCF-0x44FED7) and 287 SetHPPercent
// (0x4514FB-0x451638), each driven through ExecuteAction on a real sprite
// with AR1000 loaded (185 needs the master-area 2DA and a loaded area; 287
// reads the sprite's base and derived stats). 284 SetScriptName
// (0x44DCDA-0x44E042) is here too: its hash edits are observable for names
// that fill the char[32] exactly, and the test pins the original's quirks:
// the padded old name misses shorter names' hash bucket (a no-op rename),
// and m_scriptName is never written.
// 199 TextScreen is NOT here: the arm's first act is the non-returning
// UTIL_ASSERT(FALSE), so driving it kills the test process; the dead code
// behind it is described in the source comment only.
TEST_CASE("ai: ExecuteAction 185 287 and 284 master area hp percent and script name",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea1000 = pGame->LoadArea(CString("AR1000"), 0, FALSE, FALSE);
    REQUIRE(pArea1000 != NULL);
    pArea1000->SortLists();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();
    const CGameArea* pCallerAreaBefore = pCaller->m_pArea;
    CGameSprite* pSprite = MakeSprite();
    const INT nServiceProvider = g_pChitin->cNetwork.m_nServiceProvider;
    g_pChitin->cNetwork.m_nServiceProvider = CNetwork::SERV_PROV_NULL;
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // 185 SetMasterArea: the string must label a row of the MastArea 2DA
    // (AR1000 does) and name a loaded area; only then does the master
    // pointer move. A name outside the 2DA and a table name whose area is
    // not loaded both error and leave the pointer alone.
    CGameArea* pMasterBefore = pGame->m_pGameAreaMaster;
    CAIAction master;
    master.m_actionID = 185;
    master.m_string1 = "AR9000"; // no 2DA row
    pCaller->SetCurrAction(master);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pGame->m_pGameAreaMaster == pMasterBefore);

    master.m_string1 = "AR5000"; // in the 2DA, not loaded
    pCaller->SetCurrAction(master);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pGame->m_pGameAreaMaster == pMasterBefore);

    master.m_string1 = "AR1000";
    pCaller->SetCurrAction(master);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pGame->m_pGameAreaMaster == pArea1000);
    pGame->m_pGameAreaMaster = pMasterBefore;

    // 287 SetHPPercent: specifics1 is the percent clamped to [0,100] and
    // specifics2 picks the value scaled. With specifics2 zero the value is
    // the derived max hit points, stored into the base hit points only
    // when strictly above the current base; otherwise the base value
    // itself is rescaled in place. A dead target (STATE_DEAD in either
    // stats block) is left untouched. Every path reports ACTION_DONE.
    const DWORD dwBaseStateBefore = pSprite->m_baseStats.m_generalState;
    const DWORD dwDerivedStateBefore = pSprite->m_derivedStats.m_generalState;
    const SHORT nHitPointsBefore = pSprite->m_baseStats.m_hitPoints;
    const SHORT nMaxBefore = pSprite->m_derivedStats.m_nMaxHitPoints;
    pSprite->m_baseStats.m_generalState &= ~STATE_DEAD;
    pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;
    pSprite->m_derivedStats.m_nMaxHitPoints = 40;

    pSprite->m_baseStats.m_hitPoints = 10; // 40 * 0.5 > 10: stored
    pCaller->SetCurrAction(CAIAction(287, actee, 50, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 20);

    pSprite->m_baseStats.m_hitPoints = 30; // 40 * 0.5 <= 30: no store
    pCaller->SetCurrAction(CAIAction(287, actee, 50, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 30);

    // specifics2 != 0: the base value itself is scaled (30 * 0.5).
    pCaller->SetCurrAction(CAIAction(287, actee, 50, 1, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 15);

    // 150 clamps to 100: 40 * 1.0 = 40.
    pSprite->m_baseStats.m_hitPoints = 10;
    pCaller->SetCurrAction(CAIAction(287, actee, 150, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 40);

    // Dead in the base stats: no store, still done.
    pSprite->m_baseStats.m_hitPoints = 33;
    pSprite->m_baseStats.m_generalState |= STATE_DEAD;
    pCaller->SetCurrAction(CAIAction(287, actee, 50, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pSprite->m_baseStats.m_hitPoints == 33);
    pSprite->m_baseStats.m_generalState &= ~STATE_DEAD;

    // An unresolved target takes the 0x44E1DB forced-done tail.
    CAIAction percentNobody;
    percentNobody.m_actionID = 287;
    pCaller->SetCurrAction(percentNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    pSprite->m_baseStats.m_hitPoints = nHitPointsBefore;
    pSprite->m_baseStats.m_generalState = dwBaseStateBefore;
    pSprite->m_derivedStats.m_generalState = dwDerivedStateBefore;
    pSprite->m_derivedStats.m_nMaxHitPoints = nMaxBefore;

    // 284 SetScriptName: the old name is the target's m_scriptName -- read
    // as CString(field, 32), NUL padding included (0x44DCF4) -- and the new
    // one GetString1 (empty becomes "none"). Matching names
    // (case-insensitive) change nothing. Otherwise each of the area and
    // game named-creature hashes drops the old name when it maps to the
    // target's id, registers the new name in the GAME hash (the arm never
    // AddKeys into the area hash), and queues one CMessageSetVariable named
    // by the OLD name (flags 2 = area scope, 4 = GLOBAL, both = joined).
    // The target's m_scriptName is NEVER written. An unresolved target
    // takes the 0x45127A no-release done tail.
    //
    // The padded read is why short names never match: the hash buckets a
    // name by every byte of its GetLength (Hash 0x54FAD0 walks the full 32
    // bytes, NULs included, and a NUL zeroes the product term), so the
    // padded 32-byte lookup lands in a different bucket than the short
    // name's registration and RemoveKey (0x5511C0) finds nothing. In the
    // original the arm is therefore a no-op for any script name shorter
    // than the full field: no removal, no message, no registration. Only a
    // name that fills the char[32] exactly runs the body.
    CAIAction scriptNobody;
    scriptNobody.m_actionID = 284;
    pCaller->SetCurrAction(scriptNobody);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);

    CAIAction rename;
    rename.m_actionID = 284;
    rename.m_acteeID = actee;

    std::strncpy(pSprite->m_scriptName, "OLDNAME", SCRIPTNAME_SIZE - 1);
    pSprite->m_scriptName[SCRIPTNAME_SIZE - 1] = '\0';
    CVariable named;
    named.SetName(CString("OLDNAME"));
    named.m_intValue = pSprite->m_id;
    pArea1000->GetNamedCreatures()->AddKey(named);
    pGame->GetNamedCreatures()->AddKey(named);
    pSprite->m_pArea = pArea1000;

    // Short name: the padded old name hashes into another bucket, so
    // nothing moves even though both hashes hold the name.
    ClearMessages();
    rename.m_string1 = "NEWNAME";
    pCaller->SetCurrAction(rename);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageSetVariable>().empty());
    CHECK(pGame->GetNamedCreatures()->FindKey(CString("OLDNAME")) != NULL);
    CHECK(pArea1000->GetNamedCreatures()->FindKey(CString("OLDNAME")) != NULL);
    CHECK(pGame->GetNamedCreatures()->FindKey(CString("NEWNAME")) == NULL);
    CHECK(std::strcmp(pSprite->m_scriptName, "OLDNAME") == 0);

    // A name that fills the field exactly carries no padding and hashes
    // like its registration.
    static constexpr char oldName[SCRIPTNAME_SIZE + 1] = "OLDNAME32CHARSXXXXXXXXXXXXXXXXXX";
    static constexpr char newName[SCRIPTNAME_SIZE + 1] = "NEWNAME32CHARSXXXXXXXXXXXXXXXXXX";
    static constexpr char unknownOld[SCRIPTNAME_SIZE + 1] = "ZZZUNKNOWN32CHARSXXXXXXXXXXXXXXX";
    static constexpr char unknownNew[SCRIPTNAME_SIZE + 1] = "QQQNEWNAME32CHARSXXXXXXXXXXXXXXX";
    static_assert(oldName[SCRIPTNAME_SIZE] == '\0' && oldName[SCRIPTNAME_SIZE - 1] != '\0',
        "oldName must fill the script-name field exactly");
    static_assert(newName[SCRIPTNAME_SIZE] == '\0' && newName[SCRIPTNAME_SIZE - 1] != '\0',
        "newName must fill the script-name field exactly");
    static_assert(unknownOld[SCRIPTNAME_SIZE] == '\0' && unknownOld[SCRIPTNAME_SIZE - 1] != '\0',
        "unknownOld must fill the script-name field exactly");
    static_assert(unknownNew[SCRIPTNAME_SIZE] == '\0' && unknownNew[SCRIPTNAME_SIZE - 1] != '\0',
        "unknownNew must fill the script-name field exactly");
    std::memcpy(pSprite->m_scriptName, oldName, SCRIPTNAME_SIZE);
    CVariable named32;
    named32.SetName(CString(oldName));
    named32.m_intValue = pSprite->m_id;
    pArea1000->GetNamedCreatures()->AddKey(named32);
    pGame->GetNamedCreatures()->AddKey(named32);

    // Matching name, different case: nothing moves.
    ClearMessages();
    rename.m_string1 = "oldname32charsxxxxxxxxxxxxxxxxxx";
    pCaller->SetCurrAction(rename);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pGame->GetNamedCreatures()->FindKey(CString(oldName)) != NULL);
    CHECK(Queued<CMessageSetVariable>().empty());

    // Rename: both hashes hold the old name, so both removals join one
    // message (flags 2|4) scoped to the area resref.
    rename.m_string1 = newName;
    pCaller->SetCurrAction(rename);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageSetVariable*> sets = Queued<CMessageSetVariable>();
        REQUIRE(sets.size() == 1);
        CString sAreaRes;
        pArea1000->m_resRef.CopyToString(sAreaRes);
        CHECK(sets[0]->m_sScope == sAreaRes);
        CHECK(sets[0]->m_sName == CString(oldName));
        CHECK(sets[0]->m_nValue == pSprite->m_id);
        CHECK(sets[0]->m_dwFlags == 6);
    }
    CHECK(pGame->GetNamedCreatures()->FindKey(CString(oldName)) == NULL);
    CHECK(pArea1000->GetNamedCreatures()->FindKey(CString(oldName)) == NULL);
    {
        CVariable* pRegistered = pGame->GetNamedCreatures()->FindKey(CString(newName));
        REQUIRE(pRegistered != NULL);
        CHECK(pRegistered->m_intValue == pSprite->m_id);
    }
    // The original never writes the target's script name.
    CHECK(std::memcmp(pSprite->m_scriptName, oldName, SCRIPTNAME_SIZE) == 0);

    // The arm never writes m_scriptName; whatever updates it in the live
    // game runs elsewhere. With the field holding the registered name and
    // the area hash empty of it, an empty new name ("none") fires only the
    // game removal, as a fresh GLOBAL message.
    ClearMessages();
    std::memcpy(pSprite->m_scriptName, newName, SCRIPTNAME_SIZE);
    rename.m_string1 = "";
    pCaller->SetCurrAction(rename);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageSetVariable*> sets = Queued<CMessageSetVariable>();
        REQUIRE(sets.size() == 1);
        CHECK(sets[0]->m_sScope == CString("GLOBAL"));
        CHECK(sets[0]->m_sName == CString(newName));
        CHECK(sets[0]->m_dwFlags == 4);
    }
    CHECK(pGame->GetNamedCreatures()->FindKey(CString(newName)) == NULL);
    {
        CVariable* pRegistered = pGame->GetNamedCreatures()->FindKey(CString("NONE"));
        REQUIRE(pRegistered != NULL);
        CHECK(pRegistered->m_intValue == pSprite->m_id);
    }

    // Old name unknown to both hashes: no message, no registration.
    ClearMessages();
    std::memcpy(pSprite->m_scriptName, unknownOld, SCRIPTNAME_SIZE);
    rename.m_string1 = unknownNew;
    pCaller->SetCurrAction(rename);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageSetVariable>().empty());
    CHECK(pGame->GetNamedCreatures()->FindKey(CString(unknownNew)) == NULL);

    std::memset(pSprite->m_scriptName, 0, SCRIPTNAME_SIZE);

    pSprite->m_pArea = NULL;
    pCaller->m_pArea = const_cast<CGameArea*>(pCallerAreaBefore);
    g_pChitin->cNetwork.m_nServiceProvider = nServiceProvider;
    ClearMessages();
}

// Task 83n: case 133 ClearActions. The arm resolves the actee through the
// shared preamble (ExecuteActionGetActee): a missing actee keeps the
// entry-default error, and a sprite immune to the caller's AI type is
// released and dropped the same way. A live target has its queue cleared
// (CMessageClearActions) and a NO_ACTION queued in its place
// (CMessageAddAction) so it stops whatever it was doing.
TEST_CASE("ai: ExecuteAction 133 ClearActions clears the target queue and queues NO_ACTION",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();
    CGameSprite* pSprite = MakeSprite();
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);

    // A missing actee: the entry-default error, and nothing is queued.
    CAIObjectType missing(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id + 1000, 0, 0);
    pCaller->SetCurrAction(CAIAction(133, missing, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    // A sprite immune to the caller's AI type is released and dropped: the
    // same error, still nothing queued.
    pSprite->m_derivedStats.m_cImmunitiesAIType.AddTail(new CAIObjectType(pCaller->m_typeAI));
    pCaller->SetCurrAction(CAIAction(133, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());
    delete pSprite->m_derivedStats.m_cImmunitiesAIType.RemoveTail();

    // A live target: the queue-clear message and the replacement NO_ACTION,
    // both from the caller to the target sprite.
    pCaller->SetCurrAction(CAIAction(133, actee, 0, 0, 0));
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    {
        const std::vector<CMessageClearActions*> cleared = Queued<CMessageClearActions>();
        REQUIRE(cleared.size() == 1);
        CHECK(cleared[0]->GetSource() == pCaller->m_id);
        CHECK(cleared[0]->GetTarget() == pSprite->m_id);
    }
    {
        const std::vector<CMessageAddAction*> added = Queued<CMessageAddAction>();
        REQUIRE(added.size() == 1);
        CHECK(added[0]->GetSource() == pCaller->m_id);
        CHECK(added[0]->GetTarget() == pSprite->m_id);
        CHECK(added[0]->m_action.m_actionID == CAIAction::NO_ACTION);
        CHECK(added[0]->m_action.m_specificID == 0);
        CHECK(added[0]->m_action.m_specificID2 == -1);
        CHECK(added[0]->m_action.m_dest.x == -1);
        CHECK(added[0]->m_action.m_dest.y == -1);
    }

    ClearMessages();
}

TEST_CASE("ai: EvaluateStatusTrigger 0x40B5 0x40B6 0x40B7 0x40C1 0x40C2 and 0x40C3 total item counts",
    "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();

    // The area load waits for m_bDisplayStale to clear, so a pump thread
    // stands in for the display loop (the harness of the 233/235 case).
    std::atomic<bool> pumpRunning(true);
    std::thread pump([&pumpRunning] {
        while (pumpRunning) {
            g_pChitin->m_bDisplayStale = FALSE;
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });
    struct JoinPump {
        std::atomic<bool>& running;
        std::thread& pump;
        ~JoinPump()
        {
            running = false;
            pump.join();
        }
    } joinPump { pumpRunning, pump };

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameArea* pArea1000 = pGame->LoadArea(CString("AR1000"), 0, FALSE, FALSE);
    REQUIRE(pArea1000 != NULL);
    pArea1000->SortLists();
    ClearMessages();

    CGameAIBase* pCaller = AddAIObject();

    // The vert-sort walk of the 278 case: the id of the first registered
    // object of the wanted type over the sorted and background lists.
    auto findTypeId = [pGame, pArea1000](BYTE type) -> LONG {
        CTypedPtrList<CPtrList, int*>* lists[2] = { &pArea1000->m_lVertSort,
            &pArea1000->m_lVertSortBack };
        for (CTypedPtrList<CPtrList, int*>* list : lists) {
            POSITION pos = list->GetHeadPosition();
            while (pos != NULL) {
                LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
                CGameObject* pObject;
                if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH,
                        &pObject, INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    const LONG nId = pObject->GetId();
                    pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    if (pObject->GetObjectType() == type) {
                        return nId;
                    }
                }
            }
        }
        return CGameObjectArray::INVALID_INDEX;
    };

    // Container half: a registered AR1000 container emptied and seeded with
    // a stack of three potions and a single gem. CountItemExcept("") counts
    // both -- 3 for the stack's charges, 1 for the gem.
    const LONG nContainerId = findTypeId(CGameObject::TYPE_CONTAINER);
    REQUIRE(nContainerId != CGameObjectArray::INVALID_INDEX);
    CGameObject* pContainerObject;
    REQUIRE(pGame->GetObjectArray()->GetShare(nContainerId, CGameObjectArray::THREAD_ASYNCH,
                &pContainerObject, INFINITE)
        == CGameObjectArray::SUCCESS);
    CGameContainer* pContainer = static_cast<CGameContainer*>(pContainerObject);
    while (pContainer->GetNumItems() > 0) {
        pContainer->RemoveItem(0);
    }
    ClearMessages();
    pContainer->m_lstItems.AddTail(new CItem(CResRef("00POTN01"), 3, 0, 0, 0, 0));
    pContainer->m_lstItems.AddTail(new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0));

    CAIObjectType containerCause(0, 0, 0, 0, 0, 0, 0, 0, nContainerId, 0, 0);
    // 0x40B5 TotalItemCnt: the container's whole list is 4.
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B5, containerCause, 4)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B5, containerCause, 5)) == FALSE);
    // 0x40B6 TotalItemCntGT: strictly above 3, not above 4.
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B6, containerCause, 3)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B6, containerCause, 4)) == FALSE);
    // 0x40B7 TotalItemCntLT: strictly under 5, not under 4.
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B7, containerCause, 5)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B7, containerCause, 4)) == FALSE);

    // The exclude family against a container: 0x40C1 and 0x40C3 hand
    // CountItemExcept the empty string, so the excluded name changes
    // nothing (still 4); 0x40C2 alone passes the name, dropping the potion
    // stack (1, the gem alone). The m_string1 member is what the arms read.
    CAITrigger excludeContainer(0x40C1, containerCause, 4);
    excludeContainer.m_string1 = "00POTN01";
    CHECK(pCaller->EvaluateStatusTrigger(excludeContainer) == TRUE);
    excludeContainer.m_triggerID = 0x40C3; // LT: 4 < 5
    excludeContainer.m_specificID = 5;
    CHECK(pCaller->EvaluateStatusTrigger(excludeContainer) == TRUE);
    excludeContainer.m_triggerID = 0x40C2; // GT: 1 > 0, but not > 1
    excludeContainer.m_specificID = 0;
    CHECK(pCaller->EvaluateStatusTrigger(excludeContainer) == TRUE);
    excludeContainer.m_specificID = 1;
    CHECK(pCaller->EvaluateStatusTrigger(excludeContainer) == FALSE);

    REQUIRE(pGame->GetObjectArray()->ReleaseShare(nContainerId, CGameObjectArray::THREAD_ASYNCH,
                INFINITE)
        == CGameObjectArray::SUCCESS);

    // Sprite half: a sprite whose 51 slots hold only the seeded pair, so
    // the walk's total is the pair's alone. The potion is stackable, so it
    // counts its charges (3); the gem counts one.
    CGameSprite* pSprite = MakeSprite();
    for (INT nSlot = 0; nSlot < CGameSpriteEquipment::NUM_SLOT; nSlot++) {
        pSprite->m_equipment.m_items[nSlot] = NULL;
    }
    CItem* pPotion = new CItem(CResRef("00POTN01"), 3, 0, 0, 0, 0);
    REQUIRE(pPotion->GetMaxStackable() > 1); // the stack branch is the one taken
    REQUIRE(pPotion->GetUsageCount(0) == 3);
    pSprite->m_equipment.m_items[18] = pPotion;
    pSprite->m_equipment.m_items[19] = new CItem(CResRef("00GEM01"), 1, 0, 0, 0, 0);

    CAIObjectType spriteCause(0, 0, 0, 0, 0, 0, 0, 0, pSprite->m_id, 0, 0);
    // The trio over the sprite's equipment: 3 + 1.
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B5, spriteCause, 4)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B6, spriteCause, 3)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B7, spriteCause, 5)) == TRUE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B5, spriteCause, 4 - 1)) == FALSE);

    // The exclude family over the sprite: the excluded potion is skipped
    // before the stack test, so only the gem's 1 counts.
    CAITrigger excludeSprite(0x40C1, spriteCause, 1);
    excludeSprite.m_string1 = "00POTN01";
    CHECK(pCaller->EvaluateStatusTrigger(excludeSprite) == TRUE);
    excludeSprite.m_triggerID = 0x40C2; // GT: 1 > 0, but not > 1
    excludeSprite.m_specificID = 0;
    CHECK(pCaller->EvaluateStatusTrigger(excludeSprite) == TRUE);
    excludeSprite.m_specificID = 1;
    CHECK(pCaller->EvaluateStatusTrigger(excludeSprite) == FALSE);
    excludeSprite.m_triggerID = 0x40C3; // LT: 1 < 2, but not < 1
    excludeSprite.m_specificID = 2;
    CHECK(pCaller->EvaluateStatusTrigger(excludeSprite) == TRUE);
    excludeSprite.m_specificID = 1;
    CHECK(pCaller->EvaluateStatusTrigger(excludeSprite) == FALSE);

    // An unresolved cause object answers FALSE on every arm (the arms'
    // early return before any counting).
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40B5, CAIObjectType::NOONE, 0)) == FALSE);
    CHECK(pCaller->EvaluateStatusTrigger(CAITrigger(0x40C1, CAIObjectType::NOONE, 0)) == FALSE);

    ClearMessages();
}
