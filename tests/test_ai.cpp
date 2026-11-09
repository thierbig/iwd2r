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
#include "CGameDoor.h"
#include "CGameEffect.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CScreenCreateChar.h"
#include "CVariableHash.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {

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
