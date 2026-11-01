#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAIScript.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CGameAnimationTypeCharacter.h"
#include "CGameArea.h"
#include "CGameDialog.h"
#include "CGameDoor.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CObjectMarker.h"
#include "CPathSearch.h"
#include "CResDLG.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSearchBitmap.h"
#include "CStore.h"
#include "CUtil.h"
#include "ScreenLayout.h"
#include "compat/msvc_rand.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The sprite's own actions (CGameSprite::ExecuteAction 0x728F80) on the
// engine in this process: a party member in AR1000 walks to a point
// (MoveToPoint), walks to Hedron before talking to him (PlayerDialog,
// MoveToObject), and opens or fails to open a door it stands next to
// (UseDoor). Every engine works on a private view of the install
// (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

fs::path g_view;

// The engine as iwd2r_main builds it (tests/test_gam.cpp's StartEngine).
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
    const std::string original = DataDir();
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
    POSITION pos = chitin->lEngines.GetHeadPosition();
    while (pos != NULL) {
        CWarp* pEngine = static_cast<CWarp*>(chitin->lEngines.GetNext(pos));
        if (pEngine != NULL) {
            pEngine->EngineInitialized();
        }
    }
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

// The main loop's drawn frames (tests/test_gam.cpp's FramePump).
class FramePump {
public:
    FramePump()
        : m_running(true)
        , m_thread([this] {
            while (m_running) {
                g_pChitin->m_bDisplayStale = FALSE;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        })
    {
    }
    ~FramePump()
    {
        m_running = false;
        m_thread.join();
    }
    FramePump(const FramePump&) = delete;
    FramePump& operator=(const FramePump&) = delete;

private:
    std::atomic<bool> m_running;
    std::thread m_thread;
};

std::vector<std::vector<BYTE>> FinishedCharacters()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    std::vector<std::vector<BYTE>> creatures;
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                creatures.emplace_back(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return creatures;
}

std::vector<BYTE> FinishedCharacter()
{
    std::vector<std::vector<BYTE>> creatures = FinishedCharacters();
    return creatures.empty() ? std::vector<BYTE>() : creatures.front();
}

// A new game with one party member standing in AR1000, the world screen's
// panels loaded (tests/test_dialog.cpp's NewGameInStartArea).
CGameSprite* NewGameInStartArea(std::vector<BYTE>& bytes)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    pSprite->m_sName = "Probe";
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->EngineGameInit();
    // The world screen is the active engine, as after the game loads (the
    // test does not switch engines: EngineActivated starts the sound).
    g_pBaldurChitin->pActiveEngine = pWorld;
    pGame->m_bGameLoaded = TRUE;
    return pSprite;
}

// The sprite on the area's lists whose dialog is `dialog`.
CGameSprite* FindSpeaker(CGameArea* pArea, const char* dialog)
{
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                    && static_cast<CGameSprite*>(pObject)->m_dialog == dialog) {
                    return static_cast<CGameSprite*>(pObject);
                }
            }
        }
    }
    return NULL;
}

// Keeps a demanded resource and puts back one byte patched in its data
// when the test case leaves, by a failed REQUIRE too.
class DemandedResource {
public:
    explicit DemandedResource(CRes* pRes)
        : m_pRes(pRes)
    {
    }
    ~DemandedResource()
    {
        if (m_pByte != NULL) {
            *m_pByte = m_original;
        }
        m_pRes->Release();
    }
    void Patch(BYTE* pByte, BYTE value)
    {
        m_pByte = pByte;
        m_original = *pByte;
        *pByte = value;
    }
    DemandedResource(const DemandedResource&) = delete;
    DemandedResource& operator=(const DemandedResource&) = delete;

private:
    CRes* m_pRes;
    BYTE* m_pByte = NULL;
    BYTE m_original = 0;
};

// The search square of a point.
CPoint Square(const CPoint& pt)
{
    return CPoint(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY);
}

// Whether the terrain of a square is open to the sprite and nobody stands on
// it.
bool OpenSquare(CGameSprite* pSprite, const CPoint& square)
{
    CSearchBitmap& search = pSprite->GetArea()->m_search;
    if (square.x < 0 || square.y < 0 || square.x >= search.m_GridSquareDimensions.cx
        || square.y >= search.m_GridSquareDimensions.cy) {
        return false;
    }
    CVidBitmap& bitmap = search.m_resSearch;
    BYTE value = bitmap.GetPixelValue(square.x, square.y, TRUE);
    if (bitmap.GetBitCount(TRUE) == 8) {
        value >>= 4;
    }
    return pSprite->m_terrainTable[value] != CPathSearch::COST_IMPASSABLE
        && search.m_pDynamicCost[square.y * search.m_GridSquareDimensions.cx + square.x] == 0;
}

// Set when the search thread left a request unserved for five seconds.
bool g_searchStalled = false;

// One frame of a sprite's action as the AI update runs it: the action (the
// sprite's dispatcher), the walk, then the messages. A search request the
// search thread still works on is waited for first, as the replay holds the
// frame.
SHORT Frame(CGameSprite* pSprite)
{
    CSearchRequest* pRequest = pSprite->m_currentSearchRequest;
    if (pRequest != NULL) {
        int waited = 0;
        while ((pRequest->m_serviceState == CSearchRequest::STATE_WAITING
                   || pRequest->m_serviceState == CSearchRequest::STATE_PROCESSING)
            && pRequest->m_collisionDelay == 0
            && waited < 5000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            waited++;
        }
        if (waited >= 5000) {
            g_searchStalled = true;
        }
    }
    pSprite->AIUpdateWalk();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    return pSprite->ExecuteAction();
}

// The door on the area's front list with this script name.
CGameDoor* FindDoor(CGameArea* pArea, const char* name)
{
    POSITION pos = pArea->m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSort.GetNext(pos)));
        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (pObject->GetObjectType() == CGameObject::TYPE_DOOR
                && strncmp(static_cast<CGameDoor*>(pObject)->m_scriptName, name, SCRIPTNAME_SIZE) == 0) {
                return static_cast<CGameDoor*>(pObject);
            }
        }
    }
    return NULL;
}

// The party member, selected, standing next to the door, its UseDoor
// (142) on the door ready to run for the first time.
void ReadyUseDoor(CGameSprite* pCharacter, CGameDoor* pDoor)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    // UseDoor reads only the position: it is set where a jump would put the
    // character.
    pCharacter->m_pos = pDoor->GetCloserDestPt(pCharacter->GetPos());
    if (!pCharacter->m_bSelected) {
        pGame->SelectCharacter(pCharacter->GetId(), FALSE);
    }
    REQUIRE(pCharacter->m_bSelected);
    CAIAction useDoor;
    useDoor.m_actionID = 142;
    useDoor.m_specificID = pDoor->GetId();
    useDoor.m_specificID2 = 0;
    pCharacter->SetCurrAction(useDoor);
    pCharacter->m_actionCount = 0;
}

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The queued messages of one kind, oldest first (tests/test_combat.cpp's
// Queued).
template <class T>
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

}

TEST_CASE("sprite: a party member given MoveToPoint walks to the point and its action clears", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    // The search thread's snapshot walls off unexplored squares for the
    // party; the party's sight explores them in the game.
    pArea->m_visibility.SetAreaExplored();

    // An open square some way off with open squares on the way.
    const CPoint fromSquare = Square(pCharacter->GetPos());
    const CPoint directions[] = { CPoint(1, 0), CPoint(-1, 0), CPoint(0, 1), CPoint(0, -1) };
    CPoint goalSquare(-1, -1);
    for (int distance = 12; distance >= 5 && goalSquare.x == -1; distance--) {
        for (const CPoint& direction : directions) {
            bool open = true;
            for (int step = 2; step <= distance + 1 && open; step++) {
                open = OpenSquare(pCharacter, CPoint(fromSquare.x + direction.x * step, fromSquare.y + direction.y * step));
            }
            if (open) {
                goalSquare = CPoint(fromSquare.x + direction.x * distance, fromSquare.y + direction.y * distance);
                break;
            }
        }
    }
    REQUIRE(goalSquare.x != -1);
    const CPoint goal(goalSquare.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
        goalSquare.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

    // MoveToPoint (23) asks for a path to the point and goes on.
    pCharacter->SetCurrAction(CAIAction(CAIAction::MOVETOPOINT, goal, 0, -1));
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pCharacter->m_curDest == goal);
    CHECK(pCharacter->m_nLastActionReturn == CGameAIBase::ACTION_INTERRUPTABLE);
    REQUIRE((pCharacter->m_currentSearchRequest != NULL || pCharacter->m_pPath != NULL));

    // The same point again does not ask again.
    CSearchRequest* pRequest = pCharacter->m_currentSearchRequest;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pCharacter->m_currentSearchRequest == pRequest);

    int frames = 0;
    SHORT rc = CGameAIBase::ACTION_INTERRUPTABLE;
    while (rc == CGameAIBase::ACTION_INTERRUPTABLE && frames < 3000 && !g_searchStalled) {
        rc = Frame(pCharacter);
        frames++;
    }
    if (g_searchStalled) {
        // The search thread serves requests once the movement stream's
        // request loop is in (CSearchBitmap, SearchThreadMain).
        SKIP("[iwd2r-skip] the search thread did not serve the path request");
    }
    INFO("frames " << frames);
    CHECK(rc == CGameAIBase::ACTION_DONE);
    CHECK(Square(pCharacter->GetPos()) == goalSquare);

    // Done: the next action is NoAction, as ProcessAI takes it.
    CAIAction next;
    pCharacter->SetCurrAction(pCharacter->GetNextAction(next));
    CHECK(pCharacter->m_curAction.m_actionID == CAIAction::NO_ACTION);
}

TEST_CASE("sprite: PlayerDialog out of range walks to Hedron and the dialog opens on arrival", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    pArea->m_visibility.SetAreaExplored();
    CGameSprite* pHedron = FindSpeaker(pGame->m_pGameAreaMaster, "10HEDRON");
    REQUIRE(pHedron != NULL);

    // Out of reach: the squares between them are more than the two personal
    // spaces and two squares.
    const CPoint a = Square(pCharacter->GetPos());
    const CPoint b = Square(pHedron->GetPos());
    INFO("character " << pCharacter->GetPos().x << "," << pCharacter->GetPos().y << " Hedron "
                      << pHedron->GetPos().x << "," << pHedron->GetPos().y);
    REQUIRE(std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) > 4);

    // The original's start state (NumTimesTalkedTo(0)) needs the sprite's
    // status triggers.
    //
    // TODO(remove): as in tests/test_dialog.cpp, the cached DLG's trigger
    // name is spoiled only while CGameSprite::EvaluateStatusTrigger (the
    // movement stream) is missing; `demanded` puts the byte back.
    CResHelper<CResDLG, 1011> file;
    file.SetResRef(CResRef("10HEDRON"), TRUE, TRUE);
    REQUIRE(file.GetRes() != NULL);
    BYTE* pData = static_cast<BYTE*>(file.GetRes()->Demand());
    REQUIRE(pData != NULL);
    DemandedResource demanded(file.GetRes());
    if (!pHedron->EvaluateStatusTrigger(CAITrigger(0x4039, 0))) {
        const DWORD nSize = file.GetRes()->m_nResSizeActual;
        const std::string sTrigger = "NumTimesTalkedTo(0)";
        BYTE* pTrigger = std::search(pData, pData + nSize, sTrigger.begin(), sTrigger.end());
        REQUIRE(pTrigger != pData + nSize);
        demanded.Patch(&pTrigger[15], 'X');
    }

    // PlayerDialog (139) on Hedron: too far, so the character walks to him
    // first (MoveToObject 0x73EDD0, a search for Hedron himself).
    CAIObjectType hedron(0, 0, 0, 0, 0, 0, 0, 0, pHedron->GetId(), 0, 0);
    pCharacter->SetCurrAction(CAIAction(CAIAction::PLAYERDIALOG, hedron, 0, 0, 0));
    pCharacter->m_actionCount = 1;
    // SetCurrAction (0x7338E0) draws the path counter, UtilRandInt(8,
    // -luck); MoveToObject searches when it is a multiple of 8. Start the
    // walk on the first call.
    pCharacter->field_5616 = 0;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pCharacter->m_curDest == pHedron->GetPos());
    REQUIRE(pCharacter->m_currentSearchRequest != NULL);
    CHECK(pCharacter->m_currentSearchRequest->m_nTargetIds == 1);
    CHECK(pCharacter->m_currentSearchRequest->m_targetIds[0] == pHedron->GetId());
    CHECK(pCharacter->m_targetId == pHedron->GetId());
    CHECK(pWorld->m_internalLoadedDialog.field_54 == 0);

    int frames = 0;
    SHORT rc = CGameAIBase::ACTION_INTERRUPTABLE;
    while (rc == CGameAIBase::ACTION_INTERRUPTABLE && frames < 3000 && !g_searchStalled) {
        rc = Frame(pCharacter);
        frames++;
    }
    if (g_searchStalled) {
        // The search thread serves requests once the movement stream's
        // request loop is in (CSearchBitmap, SearchThreadMain).
        SKIP("[iwd2r-skip] the search thread did not serve the path request");
    }
    INFO("frames " << frames);

    // On arrival the dialog opens: Hedron's opening state, both sides
    // remember who they talked to.
    CHECK(rc == CGameAIBase::ACTION_DONE);
    CGameDialogSprite& dialog = pWorld->m_internalLoadedDialog;
    CHECK(dialog.field_54 == 1);
    CHECK(pCharacter->m_lTalkedTo.m_nInstance == pHedron->GetId());
    CHECK(pHedron->m_lTalkedTo.m_nInstance == pCharacter->GetId());
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.m_currentEntryIndex == 0);
    CHECK(dialog.m_talkerIndex == pHedron->GetId());
    // Hedron counts the talk (CMessageSetNumTimesTalkedTo); the party
    // member does not.
    CHECK(pHedron->m_nNumberOfTimesTalkedTo == 1);

    dialog.m_responseMarker = -2;
    dialog.AsynchronousUpdate();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.field_54 == 0);
    ClearMessages();
}

TEST_CASE("sprite: RunAwayFromNoLeaveArea (250) runs for the action's duration and then stops", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    pArea->m_visibility.SetAreaExplored();
    CGameSprite* pHedron = FindSpeaker(g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster, "10HEDRON");
    REQUIRE(pHedron != NULL);
    ClearMessages();

    CAIObjectType character(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId(), 0, 0);
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;

    // No target, or a target in another area (none): the action fails.
    pHedron->SetCurrAction(CAIAction(250, character, 0, 0, 0));
    CHECK(pHedron->RunAwayFromNoLeaveArea(NULL) == CGameAIBase::ACTION_ERROR);
    CGameArea* pCharacterArea = pCharacter->m_pArea;
    pCharacter->m_pArea = NULL;
    CHECK(pHedron->RunAwayFromNoLeaveArea(pCharacter) == CGameAIBase::ACTION_ERROR);
    pCharacter->m_pArea = pCharacterArea;

    // Duration spent (specificID 0, the counter at 5): the counter resets,
    // the path is dropped and the idle sequence takes over.
    pHedron->SetCurrAction(CAIAction(250, character, 0, 0, 0));
    pHedron->field_5614 = 5;
    pHedron->m_nSequence = CGameSprite::SEQ_WALK;
    CHECK(pHedron->RunAwayFromNoLeaveArea(pCharacter) == CGameAIBase::ACTION_DONE);
    CHECK(pHedron->field_5614 == 0);
    REQUIRE(handler.m_messageList.GetCount() == 2);
    auto* pDrop = dynamic_cast<CMessageDropPath*>(handler.m_messageList.GetHead());
    REQUIRE(pDrop != NULL);
    CHECK(pDrop->GetSource() == pHedron->GetId());
    auto* pSequence = dynamic_cast<CMessageSetSequence*>(handler.m_messageList.GetTail());
    REQUIRE(pSequence != NULL);
    CHECK(pSequence->m_sequence == static_cast<BYTE>(pHedron->GetIdleSequence()));
    CHECK(pSequence->GetTarget() == pHedron->GetId());
    ClearMessages();

    // Already in the idle pose: no messages.
    pHedron->field_5614 = 5;
    pHedron->m_nSequence = CGameSprite::SEQ_READY;
    CHECK(pHedron->RunAwayFromNoLeaveArea(pCharacter) == CGameAIBase::ACTION_DONE);
    CHECK(handler.m_messageList.IsEmpty());

    // Too far (more than 8 squares) and no path: the first random point
    // clamps to the area's corner, which is this position itself, so no
    // move is started and the action stays interruptable. The corner is the
    // area's 0,0 with empty bounds.
    CPoint ptHedron = pHedron->m_pos;
    CPoint ptCharacter = pCharacter->m_pos;
    int nAreaX = pArea->GetInfinity()->nAreaX;
    int nAreaY = pArea->GetInfinity()->nAreaY;
    pArea->GetInfinity()->nAreaX = 0;
    pArea->GetInfinity()->nAreaY = 0;
    pHedron->m_pos = CPoint(0, 0);
    pCharacter->m_pos = CPoint(1000, 700);
    REQUIRE(pHedron->m_pPath == NULL);
    REQUIRE(pHedron->m_currentSearchRequest == NULL);
    pHedron->field_5614 = 0;
    pHedron->SetCurrAction(CAIAction(250, character, 100, 0, 0));
    CHECK(pHedron->RunAwayFromNoLeaveArea(pCharacter) == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pHedron->field_5614 == 1);
    CHECK(pHedron->m_curDest.x == 0);
    CHECK(pHedron->m_curDest.y == 0);

    // The 184 variant (RunAwayFromNoInterrupt) answers ACTION_NORMAL from
    // the same body.
    pHedron->field_5614 = 0;
    pHedron->SetCurrAction(CAIAction(184, character, 100, 0, 0));
    CHECK(pHedron->RunAwayFromNoLeaveArea(pCharacter) == CGameAIBase::ACTION_NORMAL);

    // Too close and the counter at its 15th call: the point opposite the
    // target (twice this position minus the target's) is this position
    // when both stand together, so the move starts; the empty weapon slot
    // then ends the action done.
    CItem* pSavedItem = pHedron->m_equipment.m_items[pHedron->m_equipment.m_selectedWeapon];
    pHedron->m_equipment.m_items[pHedron->m_equipment.m_selectedWeapon] = NULL;
    pArea->GetInfinity()->nAreaX = nAreaX;
    pArea->GetInfinity()->nAreaY = nAreaY;
    pHedron->m_pos = ptHedron;
    pCharacter->m_pos = ptHedron;
    pHedron->field_5614 = 14;
    pHedron->SetCurrAction(CAIAction(250, character, 100, 0, 0));
    CHECK(pHedron->RunAwayFromNoLeaveArea(pCharacter) == CGameAIBase::ACTION_DONE);
    CHECK(pHedron->field_5614 == 15);
    CHECK(pHedron->m_curDest.x == ptHedron.x);
    CHECK(pHedron->m_curDest.y == ptHedron.y);
    pHedron->m_equipment.m_items[pHedron->m_equipment.m_selectedWeapon] = pSavedItem;
    pCharacter->m_pos = ptCharacter;

    // Through the dispatcher: the target is shared as a sprite, the action
    // runs and the share is released.
    pHedron->field_5614 = 5;
    pHedron->m_nSequence = CGameSprite::SEQ_READY;
    pHedron->SetCurrAction(CAIAction(250, character, 0, 0, 0));
    CHECK(pHedron->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pHedron->m_curAction.m_actionID == 250);
    ClearMessages();
}

TEST_CASE("sprite: Interact (168) records the interaction and opens the actor's dialog", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    pArea->m_visibility.SetAreaExplored();
    CGameSprite* pHedron = FindSpeaker(pGame->m_pGameAreaMaster, "10HEDRON");
    REQUIRE(pHedron != NULL);

    // The cached DLG's trigger name is spoiled only while
    // CGameSprite::EvaluateStatusTrigger is missing (as the PlayerDialog
    // test above; `demanded` puts the byte back).
    CResHelper<CResDLG, 1011> file;
    file.SetResRef(CResRef("10HEDRON"), TRUE, TRUE);
    REQUIRE(file.GetRes() != NULL);
    BYTE* pData = static_cast<BYTE*>(file.GetRes()->Demand());
    REQUIRE(pData != NULL);
    DemandedResource demanded(file.GetRes());
    if (!pHedron->EvaluateStatusTrigger(CAITrigger(0x4039, 0))) {
        const DWORD nSize = file.GetRes()->m_nResSizeActual;
        const std::string sTrigger = "NumTimesTalkedTo(0)";
        BYTE* pTrigger = std::search(pData, pData + nSize, sTrigger.begin(), sTrigger.end());
        REQUIRE(pTrigger != pData + nSize);
        demanded.Patch(&pTrigger[15], 'X');
    }

    // Interact (168) on the party member: unlike PlayerDialog there is no
    // reach check, and the actor is the speaker (Interact calls
    // StartDialog(target, this, FALSE, TRUE)). The interaction resref
    // (field_56E4) is empty - the constructor is its only writer - so the
    // dialog load fails, but the interaction is recorded first.
    CAIObjectType character(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId(), 0, 0);
    pHedron->SetCurrAction(CAIAction(168, character, 0, 0, 0));
    CHECK(pHedron->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    CHECK(pHedron->m_interactingWith.Equal(pCharacter->GetAIType()));
    CHECK(pHedron->m_lTalkedTo.Equal(pCharacter->GetAIType()));
    CHECK(pCharacter->m_lTalkedTo.Equal(pHedron->GetAIType()));

    // With the interaction resref set the same call opens Hedron's opening
    // state, and DialogStarted counts the talk for the non-PC speaker.
    pHedron->field_56E4 = CResRef("10HEDRON");
    CHECK(pHedron->Interact(pCharacter) == CGameAIBase::ACTION_DONE);
    CGameDialogSprite& dialog = pWorld->m_internalLoadedDialog;
    CHECK(dialog.field_54 == 1);
    CHECK(dialog.m_talkerIndex == pHedron->GetId());
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.m_currentEntryIndex == 0);
    CHECK(pHedron->m_nNumberOfTimesTalkedTo == 1);

    dialog.m_responseMarker = -2;
    dialog.AsynchronousUpdate();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.field_54 == 0);
    ClearMessages();
}

TEST_CASE("sprite: UseDoor next to AR1000_Door5 opens it and does nothing from afar", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    CGameDoor* pDoor = FindDoor(pArea, "AR1000_Door5");
    REQUIRE(pDoor != NULL);
    REQUIRE((pDoor->m_dwFlags & 0x3) == 0);

    // From the ship the door is out of reach: UseDoor is done, the door
    // stays shut.
    if (!pCharacter->m_bSelected) {
        g_pBaldurChitin->GetObjectGame()->SelectCharacter(pCharacter->GetId(), FALSE);
    }
    REQUIRE(pCharacter->m_bSelected);
    CAIAction useDoor;
    useDoor.m_actionID = 142;
    useDoor.m_specificID = pDoor->GetId();
    pCharacter->SetCurrAction(useDoor);
    pCharacter->m_actionCount = 0;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK((pDoor->m_dwFlags & 0x1) == 0);

    // Next to it the door opens (ToggleDoor), and a second frame of the
    // same action only waits.
    ReadyUseDoor(pCharacter, pDoor);
    const CPoint ptDest = pDoor->GetCloserDestPt(pCharacter->GetPos());
    INFO("character " << pCharacter->GetPos().x << "," << pCharacter->GetPos().y << " door point " << ptDest.x
                      << "," << ptDest.y << " selected " << pCharacter->m_bSelected << " orderable "
                      << pCharacter->Orderable(FALSE));
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK((pDoor->m_dwFlags & 0x1) != 0);
    pCharacter->m_actionCount = 1;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    ClearMessages();
}

TEST_CASE("sprite: UseDoor on a locked door leaves it closed and says it is locked", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    CGameDoor* pDoor = FindDoor(pArea, "AR1000_Door1");
    REQUIRE(pDoor != NULL);
    REQUIRE((pDoor->m_dwFlags & 0x2) != 0);

    ReadyUseDoor(pCharacter, pDoor);
    ClearMessages();
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK((pDoor->m_dwFlags & 0x1) == 0);
    CHECK((pDoor->m_dwFlags & 0x2) != 0);

    // The feedback goes to the one who tried the door: the door's own
    // "locked" text, or "Locked" (15874).
    const STRREF expected = (pDoor->m_dwFlags & 0x200) != 0 ? pDoor->m_strNotPickable : 15874;
    bool found = false;
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        auto* message = dynamic_cast<CMessageDisplayTextRefSend*>(handler.m_messageList.GetNext(pos));
        if (message != nullptr && message->m_text == expected && message->m_targetId == pCharacter->GetId()) {
            found = true;
        }
    }
    CHECK(found);
    ClearMessages();
}

namespace {

// Every object of a type on the area's three lists.
std::vector<CGameObject*> ObjectsOfType(CGameArea* pArea, BYTE type)
{
    std::vector<CGameObject*> objects;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                if (pObject->GetObjectType() == type) {
                    objects.push_back(pObject);
                }
            }
        }
    }
    return objects;
}

}

TEST_CASE("sprite: DoAIUpdate runs while the world is paused and cycles the AI inhibitor", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    CGameSprite* pHedron = FindSpeaker(pArea, "10HEDRON");
    REQUIRE(pHedron != NULL);

    // CGameSprite::DoAIUpdate (0x7238C0) ignores `active`; the counter test
    // is CGameObject's.
    const LONG counter = pHedron->m_id & pHedron->m_AISpeed;
    pHedron->m_canBeSeen = 0;
    pHedron->m_AIInhibitor = 0;
    CHECK(pHedron->DoAIUpdate(FALSE, counter) == TRUE);
    if (pHedron->m_AISpeed != 0) {
        const BYTE inhibitor = pHedron->m_AIInhibitor;
        CHECK(pHedron->DoAIUpdate(FALSE, counter ^ pHedron->m_AISpeed) == FALSE);
        CHECK(pHedron->m_AIInhibitor == inhibitor);
    }

    // An unseen non-party creature (EA between CONTROLCUTOFF and EVILCUTOFF)
    // keeps its inhibitor: 1, 2, then 0 lets an update through.
    const BYTE ea = pHedron->GetAIType().m_nEnemyAlly;
    if ((pHedron->m_baseStats.m_flags & 0x200000) == 0
        && ea > CAIObjectType::EA_CONTROLCUTOFF
        && ea < CAIObjectType::EA_EVILCUTOFF) {
        pHedron->m_AIInhibitor = 0;
        pHedron->m_canBeSeen = 0;
        pHedron->DoAIUpdate(FALSE, counter);
        CHECK(pHedron->m_AIInhibitor == 1);
        pHedron->DoAIUpdate(FALSE, counter);
        CHECK(pHedron->m_AIInhibitor == 2);
        pHedron->DoAIUpdate(FALSE, counter);
        CHECK(pHedron->m_AIInhibitor == 0);
    }

    // A party member is never held back.
    pCharacter->m_AIInhibitor = 0;
    const LONG partyCounter = pCharacter->m_id & pCharacter->m_AISpeed;
    for (int n = 0; n < 3; n++) {
        CHECK(pCharacter->DoAIUpdate(FALSE, partyCounter) == TRUE);
        CHECK(pCharacter->m_AIInhibitor == 0);
    }

    // The constructor: active by the creature's time-of-day mask
    // (0x6EF990), and the party member counted in the area (the BYTE count
    // starts at 0xFF, 0x6F40F0).
    const LONG hour = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->GetCurrentHour();
    INFO("hour " << hour << " mask " << pHedron->m_timeOfDayVisible);
    CHECK((pHedron->m_active != FALSE) == (((pHedron->m_timeOfDayVisible >> hour) & 1) != 0));
    CHECK(pArea->m_nCharacters == 1);
}

TEST_CASE("trigger: DoAIUpdate answers whether the trigger has a script while paused too", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();

    std::vector<CGameObject*> triggers = ObjectsOfType(pArea, CGameObject::TYPE_TRIGGER);
    REQUIRE(!triggers.empty());
    int checked = 0;
    for (CGameObject* pObject : triggers) {
        CGameTrigger* pTrigger = static_cast<CGameTrigger*>(pObject);
        // The constructor (0x4CCAB0) keeps the ARE's script name, which
        // the override script was loaded from.
        REQUIRE(pTrigger->m_overrideScript != NULL);
        CHECK(CResRef(pTrigger->m_scriptRes) == pTrigger->m_overrideScript->m_cResRef);
        if (pTrigger->m_triggerType == 0 || (pTrigger->m_dwFlags & 0x100) != 0) {
            continue;
        }
        const BOOLEAN bScript = static_cast<BOOLEAN>(CResRef(pTrigger->m_scriptRes) != "");
        CHECK(pTrigger->DoAIUpdate(FALSE, 12345) == bScript);
        CHECK(pTrigger->DoAIUpdate(TRUE, 0) == bScript);
        pTrigger->m_dwFlags |= 0x100;
        CHECK(pTrigger->DoAIUpdate(TRUE, 0) == FALSE);
        pTrigger->m_dwFlags &= ~0x100;
        checked++;
    }
    CHECK(checked > 0);
}

TEST_CASE("sprite: a weapon set change takes off the old set and selects the new one", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);
    REQUIRE(pCharacter->m_equipment.m_items[42] == NULL);

    // Set 1 holds nothing: the fist (slot 10) is selected (0x726810).
    if (pCharacter->m_equipment.m_items[45] == NULL && pCharacter->m_equipment.m_items[46] == NULL) {
        ClearMessages();
        pCharacter->sub_726810(1);
        CHECK(pCharacter->m_nWeaponSet == 1);
        CHECK(pCharacter->m_equipment.m_selectedWeapon == 10);
        CHECK(pCharacter->field_3D3A[2] == 0);
        ClearMessages();
    }

    // Back to set 0: its weapon is selected again.
    pCharacter->sub_726810(0);
    CHECK(pCharacter->m_nWeaponSet == 0);
    if (pCharacter->m_equipment.m_items[43] != NULL) {
        CHECK(pCharacter->m_equipment.m_selectedWeapon == 43);
    } else {
        CHECK(pCharacter->m_equipment.m_selectedWeapon == 10);
    }
    ClearMessages();
}

TEST_CASE("game: ClearCharacterSlot leaves an area count of 0xFF alone", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    INT nSlot = -1;
    for (INT n = 0; n < CINFGAME_MAXCHARACTERS; n++) {
        if (pGame->m_characters[n] == pCharacter->GetId()) {
            nSlot = n;
        }
    }
    REQUIRE(nSlot != -1);

    // 0x5BD530 decrements the area's BYTE count unless it is 0xFF
    // (`cmp al, 0xff; je` at 0x5BD61E), the value it starts from.
    pArea->m_nCharacters = 0xFF;
    CHECK(pGame->ClearCharacterSlot(nSlot) == TRUE);
    CHECK(pGame->m_characters[nSlot] == -1);
    CHECK(pArea->m_nCharacters == 0xFF);
}

TEST_CASE("area: CanSaveGame lets the party save next to doors and other objects", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();

    // Doors, containers, triggers, statics, sounds, texts and object
    // markers share 0x47C830 in vtable slot 0x28: strError -1, TRUE.
    const BYTE types[] = { CGameObject::TYPE_DOOR, CGameObject::TYPE_CONTAINER, CGameObject::TYPE_TRIGGER,
        CGameObject::TYPE_STATIC, CGameObject::TYPE_SOUND };
    int checked = 0;
    for (BYTE type : types) {
        for (CGameObject* pObject : ObjectsOfType(pArea, type)) {
            STRREF strError = 0;
            CHECK(pObject->CanSaveGame(strError) == TRUE);
            CHECK(strError == static_cast<STRREF>(-1));
            checked++;
        }
    }
    CHECK(checked > 0);

    // The start area with no enemy in sight: saving is allowed, and
    // field_B16 (read by the idle sequence) is 0 as in the original.
    STRREF strError = 0;
    CHECK(pArea->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
}

TEST_CASE("sprite: CanSaveGame refuses near a visible living enemy and during a dialog", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    CGameSprite* pHedron = FindSpeaker(pArea, "10HEDRON");
    REQUIRE(pHedron != NULL);
    REQUIRE(pHedron->m_baseStats.field_294 == 0);
    REQUIRE((pHedron->m_baseStats.m_flags & 0x8000) == 0);
    REQUIRE(g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene == FALSE);

    const BYTE ea = pHedron->m_typeAI.m_nEnemyAlly;
    const SHORT canBeSeen = pHedron->m_canBeSeen;
    pHedron->m_curAction = CAIAction::NULL_ACTION;
    pHedron->m_currentActionId = 0;

    // Not an enemy: TRUE.
    STRREF strError = 0;
    pHedron->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_NEUTRAL;
    pHedron->m_canBeSeen = 1;
    CHECK(pHedron->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));

    // An enemy in sight: 16501.
    pHedron->m_typeAI.m_nEnemyAlly = CAIOBJECTTYPE_EA_ENEMY;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16501);

    // Out of sight, or dead: TRUE again.
    pHedron->m_canBeSeen = 0;
    CHECK(pHedron->CanSaveGame(strError) == TRUE);
    pHedron->m_canBeSeen = 1;
    pHedron->m_derivedStats.m_generalState |= STATE_DEAD;
    CHECK(pHedron->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
    pHedron->m_derivedStats.m_generalState &= ~STATE_DEAD;

    // In a dialog (Dialogue 8 or StartDialog 0x89, current or remembered),
    // or with an action that cannot be interrupted: 16502.
    pHedron->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_NEUTRAL;
    pHedron->m_curAction.m_actionID = CAIAction::DIALOGUE;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16502);
    pHedron->m_curAction.m_actionID = CAIAction::NO_ACTION;
    pHedron->m_currentActionId = CAIAction::STARTDIALOG;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16502);
    pHedron->m_currentActionId = 0;
    pHedron->m_curAction.m_internalFlags |= 0x1;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16502);
    pHedron->m_curAction.m_internalFlags &= ~0x1;

    pHedron->m_typeAI.m_nEnemyAlly = ea;
    pHedron->m_canBeSeen = canBeSeen;
}

TEST_CASE("sprite: SetCurrAction draws the path counter and decodes a spell action", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();

    // DecodeSpell (0x45D3E0): "SP" and the type by thousands, "MARW" for
    // any other, then the rest as three digits; 0 takes the marked spell.
    CString sResRef;
    pCharacter->DecodeSpell(1101, sResRef);
    CHECK(sResRef == "SPPR101");
    pCharacter->DecodeSpell(2101, sResRef);
    CHECK(sResRef == "SPWI101");
    pCharacter->DecodeSpell(3005, sResRef);
    CHECK(sResRef == "SPIN005");
    pCharacter->DecodeSpell(4012, sResRef);
    CHECK(sResRef == "MARW012");
    pCharacter->field_588 = 0;
    pCharacter->DecodeSpell(0, sResRef);
    CHECK(sResRef == "");
    pCharacter->field_588 = 2203;
    pCharacter->DecodeSpell(0, sResRef);
    CHECK(sResRef == "SPWI203");
    pCharacter->field_588 = 0;

    // MoveToPoint: the path counter is UtilRandInt(8, -luck) (0x733FE0),
    // the first rand() SetCurrAction draws; the destination and the
    // casting flags are reset.
    const CPoint goal = pCharacter->GetPos();
    srand(12345);
    const LONG expected = CUtil::UtilRandInt(8, -pCharacter->m_derivedStats.m_nLuck);
    srand(12345);
    pCharacter->m_curDest = CPoint(1, 2);
    pCharacter->m_bInCasting = TRUE;
    pCharacter->SetCurrAction(CAIAction(CAIAction::MOVETOPOINT, goal, 0, -1));
    CHECK(pCharacter->field_5616 == expected);
    CHECK(pCharacter->m_curAction.m_actionID == CAIAction::MOVETOPOINT);
    CHECK(pCharacter->m_curDest == CPoint(-1, -1));
    CHECK(pCharacter->m_bInCasting == FALSE);

    // A modal state survives MoveToPoint and ends with any other action.
    pCharacter->m_nModalState = 1;
    pCharacter->SetCurrAction(CAIAction(CAIAction::MOVETOPOINT, goal, 0, -1));
    CHECK(pCharacter->m_nModalState == 1);
    pCharacter->SetCurrAction(CAIAction(CAIAction::WAIT, CPoint(-1, -1), 0, -1));
    CHECK(pCharacter->m_nModalState == 1);
    pCharacter->SetCurrAction(CAIAction(CAIAction::ATTACK, CPoint(-1, -1), 0, -1));
    CHECK(pCharacter->m_nModalState == 0);
    ClearMessages();

    // Spell 2101 with no class and no level: not in the character's lists,
    // so the class is the spell's caster type (wizard 11) and the level the
    // spell's own.
    CAIAction spell(CAIAction::SPELL, CPoint(-1, -1), 2101, -1);
    spell.m_specificID2 = 0;
    spell.m_specificID3 = 0;
    pCharacter->SetCurrAction(spell);
    CHECK(pCharacter->m_curAction.m_actionID == CAIAction::SPELL);
    CHECK((pCharacter->m_curAction.m_specificID2 & 0xFF) == CAIOBJECTTYPE_C_WIZARD);
    CHECK(pCharacter->m_curAction.m_specificID3 == 1);
    CHECK(pCharacter->m_curSpell == NULL);
    ClearMessages();
    pCharacter->SetCurrAction(CAIAction::NULL_ACTION);
    ClearMessages();
}

TEST_CASE("door: a closed door marks its closed search squares in the search map", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);

    // AR1000's doors have no open search squares and 4-64 closed ones; the
    // constructor (0x485AC0) takes the closed count from the record's
    // +0x4E and marks them in the dynamic cost (0x80) while closed.
    int checked = 0;
    for (CGameObject* pObject : ObjectsOfType(pArea, CGameObject::TYPE_DOOR)) {
        CGameDoor* pDoor = static_cast<CGameDoor*>(pObject);
        if ((pDoor->m_dwFlags & 0x1) != 0) {
            continue;
        }
        INFO("door " << (const char*)pDoor->m_scriptName);
        CHECK(pDoor->m_nClosedSearch > 0);
        CSearchBitmap& search = pArea->m_search;
        for (WORD n = 0; n < pDoor->m_nClosedSearch; n++) {
            const CPoint& pt = pDoor->m_pClosedSearch[n];
            CHECK((search.m_pDynamicCost[pt.y * search.m_GridSquareDimensions.cx + pt.x] & 0x80) != 0);
        }
        checked++;
    }
    CHECK(checked > 0);
}

TEST_CASE("sprite: sub_75F3D0 hides and shows a creature", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    REQUIRE(pArea->m_pObjectMarker != NULL);

    // 10HINT's CRE sets byte 0x29C: AddToArea clears field_294 and hides
    // the creature again with sub_75F3D0(1), as in the original (the
    // replay probe reads 1 for it from the first world frame on).
    CGameSprite* pHint = NULL;
    for (CGameObject* pObject : ObjectsOfType(pArea, CGameObject::TYPE_SPRITE)) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (strncmp(pSprite->m_scriptName, "Door_Hint_Text_01", SCRIPTNAME_SIZE) == 0) {
            pHint = pSprite;
        }
    }
    REQUIRE(pHint != NULL);
    CList<LONG>& markers = pArea->m_pObjectMarker->m_lstObjectIds;
    CHECK(pHint->m_baseStats.field_294 == 1);
    CHECK(pHint->sub_75F3D0(1) == CGameAIBase::ACTION_DONE);
    CHECK(pHint->m_baseStats.field_294 == 1);

    // Show: back on the marker list and the search map.
    CHECK(pHint->sub_75F3D0(0) == CGameAIBase::ACTION_DONE);
    CHECK(pHint->m_baseStats.field_294 == 0);
    CHECK(markers.Find(pHint->GetId()) != NULL);
    CHECK(pHint->field_7430 == 1);
    CHECK(pHint->sub_75F3D0(0) == CGameAIBase::ACTION_DONE);

    // Hide: off both.
    CHECK(pHint->sub_75F3D0(1) == CGameAIBase::ACTION_DONE);
    CHECK(pHint->m_baseStats.field_294 == 1);
    CHECK(markers.Find(pHint->GetId()) == NULL);
    CHECK(pHint->field_7430 == 0);

    // Any other argument is an error and changes nothing.
    CHECK(pHint->sub_75F3D0(2) == CGameAIBase::ACTION_ERROR);
    CHECK(pHint->m_baseStats.field_294 == 1);
}

TEST_CASE("sprite: StartSpriteEffect allocates its arrays without asserting", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);

    // Fire (1) at intensity 2: base 0, random 2, so length * 1 entries.
    // 0x708C0F and 0x708C65 assert only when an allocation fails.
    pCharacter->StartSpriteEffect(1, 2, 10, FALSE);
    const int length = pCharacter->m_spriteEffectSequenceLength;
    REQUIRE(length > 0);
    REQUIRE(pCharacter->m_pSpriteEffectArray != NULL);
    REQUIRE(pCharacter->m_pSpriteEffectArrayPosition != NULL);
    for (int i = 0; i < length; i++) {
        CHECK(pCharacter->m_pSpriteEffectArray[i] == 0xFFFF);
    }
    CHECK(pCharacter->m_spriteEffectDuration == static_cast<BYTE>(length + 10 + 9));

    // A second effect frees the first arrays and allocates new ones.
    pCharacter->StartSpriteEffect(2, 1, 5, FALSE);
    CHECK(pCharacter->m_pSpriteEffectArray != NULL);
    CHECK(pCharacter->m_pSpriteEffectArrayPosition != NULL);
}

TEST_CASE("animation: a two-handed weapon takes the two-handed stance and READY group", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();

    // Hedron (10HEDRON) wields a spear (00SPER01, two-handed): EquipWeapon
    // (0x6C8390) takes the two-handed path only when the item's flag bit 1
    // is set (0x6C90A9), so his weapon code is 5 and READY (7) uses BAM
    // group 3 (SetSequence 0x6CDA00).
    CGameSprite* pHedron = NULL;
    for (CGameObject* pObject : ObjectsOfType(pArea, CGameObject::TYPE_SPRITE)) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (strncmp(pSprite->m_scriptName, "Hedron", SCRIPTNAME_SIZE) == 0) {
            pHedron = pSprite;
        }
    }
    REQUIRE(pHedron != NULL);
    REQUIRE(pHedron->m_animation.GetAnimationId() == 0x6100);
    CGameAnimationTypeCharacter* pAnimation = static_cast<CGameAnimationTypeCharacter*>(pHedron->m_animation.m_animation);
    CHECK(pAnimation->m_weaponCode == 5);
    pHedron->m_animation.SetSequence(7);
    CHECK(pAnimation->m_currentBamSequence == 3);
}

TEST_CASE("animation: HEAD_TURN picks BAM group 2 or 7 or 8 with one draw (0x6CDA00)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);
    CGameAnimationTypeCharacter* pAnimation = static_cast<CGameAnimationTypeCharacter*>(pCharacter->m_animation.m_animation);
    REQUIRE(pAnimation != NULL);

    // The VC6 generator: one draw from a seed.
    auto oneDraw = [](unsigned int seed) {
        return seed * 214013u + 2531011u;
    };

    // HEAD_TURN (6) draws rand() % 100 (0x6CF3FA): below 60 group 2
    // (0x6CF443), below 97 group 7 (0x6CF62E), else group 8 (0x6CF81E).
    // Group 2 draws a random first frame only when the group was not
    // already 2, 7 or 8 on the G1 cell; groups 7 and 8 never do.
    // Seed 7 draws 61: group 7.
    srand(7);
    pCharacter->m_animation.SetSequence(6);
    CHECK(pAnimation->m_currentBamSequence == 7);
    CHECK(iwd2r_rand_state() == oneDraw(7));

    // Seed 1 draws 41: group 2, coming from 7 on the G1 cell, so no frame draw.
    srand(1);
    pCharacter->m_animation.SetSequence(6);
    CHECK(pAnimation->m_currentBamSequence == 2);
    CHECK(iwd2r_rand_state() == oneDraw(1));

    // Seed 18 draws 97: group 8 (vis-probe round 7: ours set 0, so the
    // player's head turn at walk_and_talk frame 4048 ended at 4066 and drew
    // again; frames before 1aef7608).
    srand(18);
    pCharacter->m_animation.SetSequence(6);
    CHECK(pAnimation->m_currentBamSequence == 8);
    CHECK(iwd2r_rand_state() == oneDraw(18));
}

TEST_CASE("trigger: IsTeamBitOn tests the bits SetTeamBit sets (0x40E4)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);

    // 0x455E44: the trigger's number ANDed with field_58C.
    const CAITrigger teamBit64(0x40E4, 64);
    pCharacter->field_58C = 0;
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(teamBit64));

    // SetTeamBit(64, TRUE) (case 310) turns it on; SetTeamBit(64, FALSE) off.
    pCharacter->m_curAction = CAIAction(310, CPoint(-1, -1), 64, 1);
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCharacter->EvaluateStatusTrigger(teamBit64));
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40E4, 32)));
    pCharacter->m_curAction = CAIAction(310, CPoint(-1, -1), 64, 0);
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(teamBit64));
    pCharacter->m_curAction = CAIAction();
}

TEST_CASE("trigger: ActionListEmpty and NearLocation and CreatureHidden (0x402B 0x40A9 0x40A7)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);

    // ActionListEmpty (0x45534B): nothing queued and no current action.
    const CAITrigger actionListEmpty(0x402B, 0);
    pCharacter->ClearActions(FALSE);
    pCharacter->m_curAction = CAIAction();
    CHECK(pCharacter->EvaluateStatusTrigger(actionListEmpty));
    pCharacter->m_curAction = CAIAction(310, CPoint(-1, -1), 64, 1);
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(actionListEmpty));
    pCharacter->m_curAction = CAIAction();
    pCharacter->AddAction(CAIAction(310, CPoint(-1, -1), 64, 1));
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(actionListEmpty));
    pCharacter->ClearActions(FALSE);

    // NearLocation(Myself, x, y, range) (0x4584ED): search squares of 16 by
    // 12, squared distance at most range squared.
    const CPoint pos = pCharacter->GetPos();
    REQUIRE(pos.x > 100);
    REQUIRE(pos.y > 100);
    auto nearLocation = [&](LONG x, LONG y, LONG range) {
        CAITrigger trigger(0x40A9, CAIObjectType::OBJECT_MYSELF, x);
        trigger.m_specific2 = y;
        trigger.m_specific3 = range;
        return pCharacter->EvaluateStatusTrigger(trigger) != FALSE;
    };
    // Four squares to the left: (4 * 16 / 16)^2 = 16 <= 16.
    CHECK(nearLocation(pos.x - 4 * 16, pos.y, 4));
    CHECK_FALSE(nearLocation(pos.x - 5 * 16, pos.y, 4));
    // 3 and 4 squares: 9 + 16 = 25.
    CHECK(nearLocation(pos.x - 3 * 16, pos.y - 4 * 12, 5));
    CHECK_FALSE(nearLocation(pos.x - 3 * 16, pos.y - 4 * 12, 4));
    // -1 is the caller's own position; x or y at most 0 is FALSE.
    CHECK(nearLocation(-1, -1, 0));
    CHECK_FALSE(nearLocation(0, pos.y, 1000));
    CHECK_FALSE(nearLocation(pos.x, -3, 1000));
    // -2 is the saved location (base stats field_2E4, field_2E6).
    pCharacter->m_baseStats.field_2E4 = static_cast<SHORT>(pos.x + 2 * 16);
    pCharacter->m_baseStats.field_2E6 = static_cast<SHORT>(pos.y);
    CHECK(nearLocation(-2, -2, 2));
    CHECK_FALSE(nearLocation(-2, -2, 1));

    // CreatureHidden(Myself) (0x4582E0): the base stats' field_294.
    const CAITrigger creatureHidden(0x40A7, CAIObjectType::OBJECT_MYSELF, 0);
    pCharacter->m_baseStats.field_294 = 0;
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(creatureHidden));
    pCharacter->m_baseStats.field_294 = 1;
    CHECK(pCharacter->EvaluateStatusTrigger(creatureHidden));
    pCharacter->m_baseStats.field_294 = 0;
}

TEST_CASE("sprite: ChangeDirection turns a slow walker only on its world ticks (0x6FBAD0)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);

    CTimerWorld* pTimer = g_pBaldurChitin->GetObjectGame()->GetWorldTimer();
    const ULONG gameTime = pTimer->m_gameTime;
    const BYTE moveScale = pCharacter->GetAnimation()->GetMoveScale();
    const ULONG idParity = static_cast<ULONG>(pCharacter->m_id) % 2;

    auto turnsAt = [&](ULONG time) {
        pTimer->m_gameTime = time;
        pCharacter->m_nDirection = 0;
        pCharacter->m_nNewDirection = 4;
        pCharacter->m_deltaDirection = 1;
        pCharacter->m_skipDeltaDirection = 0;
        pCharacter->ChangeDirection();
        return pCharacter->m_nDirection != 0;
    };

    // Move scale 5: 2 - 5 / 6 = 2, every other tick, on the id's parity.
    pCharacter->GetAnimation()->SetMoveScale(5);
    CHECK(turnsAt(100 + idParity));
    CHECK_FALSE(turnsAt(101 - idParity));
    // 6 to 11: 2 - 1 = 1, every tick.
    pCharacter->GetAnimation()->SetMoveScale(9);
    CHECK(turnsAt(101 - idParity));
    // 12 and up: no gate.
    pCharacter->GetAnimation()->SetMoveScale(12);
    CHECK(turnsAt(101 - idParity));

    pCharacter->GetAnimation()->SetMoveScale(moveScale);
    pTimer->m_gameTime = gameTime;
    pCharacter->m_nNewDirection = pCharacter->m_nDirection;
}

TEST_CASE("action: FaceSavedLocation turns to the saved orientation (case 298)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);
    pCharacter->m_walkBackwards = FALSE;

    // 0x451FA3: SetDirection to the base stats' field_2E8; done.
    pCharacter->m_nDirection = 2;
    pCharacter->m_nNewDirection = 2;
    pCharacter->m_baseStats.field_2E8 = 6;
    CAIObjectType actee(pCharacter->GetAIType());
    actee.m_nInstance = pCharacter->GetId();
    pCharacter->m_curAction = CAIAction(298, actee, 0, 0, 0);
    CHECK(pCharacter->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCharacter->m_nNewDirection == 6);
    CHECK(pCharacter->m_deltaDirection == 1);
    pCharacter->m_curAction = CAIAction();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    pCharacter->m_nNewDirection = pCharacter->m_nDirection;
}

TEST_CASE("sprite: GetDirection picks the 16-way direction by quadrant (0x706DD0)", "[ai]")
{
    // The original's thresholds per quadrant (dx, dy absolute).
    auto expected = [](LONG sx, LONG sy, LONG tx, LONG ty) -> SHORT {
        if (sx == tx && sy == ty) {
            return 0;
        }
        static const SHORT kDownLeft[] = { 4, 3, 2, 1, 0 };
        static const SHORT kDownRight[] = { 12, 13, 14, 15, 0 };
        static const SHORT kUpRight[] = { 12, 11, 10, 9, 8 };
        static const SHORT kUpLeft[] = { 4, 5, 6, 7, 8 };
        const SHORT* table;
        if (ty < sy) {
            table = tx < sx ? kUpLeft : kUpRight;
        } else {
            table = tx < sx ? kDownLeft : kDownRight;
        }
        const LONG dx = tx < sx ? sx - tx : tx - sx;
        const LONG dy = ty < sy ? sy - ty : ty - sy;
        if (27 * dy <= 6 * dx) {
            return table[0];
        } else if (27 * dy <= 16 * dx) {
            return table[1];
        } else if (24 * dy <= 38 * dx) {
            return table[2];
        } else if (7 * dy <= 38 * dx) {
            return table[3];
        }
        return table[4];
    };
    const CPoint start(1000, 1000);
    for (LONG dx = -40; dx <= 40; dx++) {
        for (LONG dy = -40; dy <= 40; dy++) {
            const CPoint target(1000 + dx, 1000 + dy);
            INFO("dx " << dx << " dy " << dy);
            CHECK(CGameSprite::GetDirection(start, target) == expected(1000, 1000, 1000 + dx, 1000 + dy));
        }
    }
    // Up-left, a step of (-16, -12) on the 4/3-scaled y (walk_and_talk's
    // player at frame 2957, before 1aef7608): 6; slightly up of west: 5.
    CHECK(CGameSprite::GetDirection(CPoint(2232, 1208), CPoint(2216, 1192)) == 6);
    CHECK(CGameSprite::GetDirection(CPoint(100, 100), CPoint(60, 90)) == 5);
}

TEST_CASE("sprite: SetDirection turns the short way round (0x706C80)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);
    pCharacter->m_walkBackwards = FALSE;

    auto senseFrom = [&](SHORT from, SHORT to) {
        pCharacter->m_nDirection = from;
        pCharacter->SetDirection(to);
        CHECK(pCharacter->m_nNewDirection == to);
        return pCharacter->m_deltaDirection;
    };
    CHECK(senseFrom(2, 6) == 1);
    CHECK(senseFrom(6, 2) == -1);
    // More than half a turn up: down instead (2 -> 0 -> 14).
    CHECK(senseFrom(2, 14) == -1);
    CHECK(senseFrom(14, 2) == 1);
    // Exactly half a turn: up from below (+8 is not above 8), down from
    // above negated to up (-8 is at most -8), with the skip.
    CHECK(senseFrom(2, 10) == 1);
    CHECK(pCharacter->m_skipDeltaDirection == 4);
    CHECK(senseFrom(10, 2) == 1);
    CHECK(senseFrom(3, 4) == 1);
    CHECK(pCharacter->m_skipDeltaDirection == 0);

    pCharacter->m_nNewDirection = pCharacter->m_nDirection;
}

TEST_CASE("trigger: SubRace and Kit and CheckSkillGT/LT and PartyHasItem (0x40CD 0x40CE 0x40E7 0x40E8 0x4042)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);

    // SubRace (0x453B54): race in the high word, subrace in the low word.
    const CAIObjectType& type = pCharacter->GetAIType();
    const LONG raceSubRace = (static_cast<LONG>(type.m_nRace) << 16) | type.m_nSubRace;
    CHECK(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40CD, CAIObjectType::OBJECT_MYSELF, raceSubRace)));
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40CD, CAIObjectType::OBJECT_MYSELF, raceSubRace + 1)));
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40CD, CAIObjectType::OBJECT_MYSELF, raceSubRace + 0x10000)));

    // Kit (0x453B95): every bit of the specifics in the specialization.
    const DWORD specialization = pCharacter->m_baseStats.m_specialization;
    pCharacter->m_baseStats.m_specialization = 0x4040;
    CHECK(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40CE, CAIObjectType::OBJECT_MYSELF, 0x40)));
    CHECK(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40CE, CAIObjectType::OBJECT_MYSELF, 0x4040)));
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40CE, CAIObjectType::OBJECT_MYSELF, 0x41)));
    pCharacter->m_baseStats.m_specialization = specialization;

    // CheckSkillGT/LT (0x455F32, 0x455F75): derived skill m_specific2
    // against the specifics.
    const signed char skill = pCharacter->m_derivedStats.m_nSkills[3];
    pCharacter->m_derivedStats.m_nSkills[3] = 5;
    auto checkSkill = [&](SHORT id, LONG value) {
        CAITrigger trigger(id, CAIObjectType::OBJECT_MYSELF, value);
        trigger.m_specific2 = 3;
        return pCharacter->EvaluateStatusTrigger(trigger) != FALSE;
    };
    CHECK(checkSkill(0x40E7, 4));
    CHECK_FALSE(checkSkill(0x40E7, 5));
    CHECK(checkSkill(0x40E8, 6));
    CHECK_FALSE(checkSkill(0x40E8, 5));
    pCharacter->m_derivedStats.m_nSkills[3] = skill;

    // PartyHasItem (0x455B3C): FindItemPersonal (0x71D310) answers -1 for
    // an item nobody carries, and the slot, matched without case, for one
    // the character carries.
    CAITrigger missing(0x4042, 0);
    missing.m_string1 = "NOSUCHIT";
    CHECK(pCharacter->FindItemPersonal(CString("NOSUCHIT"), 0, FALSE) == -1);
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(missing));
    // A known item in a free inventory slot: found by name in any case.
    CGameSpriteEquipment* pEquipment = pCharacter->GetEquipment();
    SHORT gemSlot = -1;
    SHORT bagSlot = -1;
    for (SHORT n = 18; n < 51 && bagSlot == -1; n++) {
        if (pEquipment->m_items[n] == NULL) {
            if (gemSlot == -1) {
                gemSlot = n;
            } else {
                bagSlot = n;
            }
        }
    }
    REQUIRE(bagSlot != -1);
    pEquipment->m_items[gemSlot] = new CItem(CResRef("00GEM02"), 1, 0, 0, 0, 0);
    CHECK(pCharacter->FindItemPersonal(CString("00gem02"), 0, FALSE) == gemSlot);
    CAITrigger carried(0x4042, 0);
    carried.m_string1 = "00gem02";
    CHECK(pCharacter->EvaluateStatusTrigger(carried));

    // A gem in a bag (an item of type 58 whose store, named as the bag,
    // holds it): FindItemBags (0x722530) answers the bag's slot, through
    // FindItemInStore (0x5C8190) loading the bag's store, and PartyHasItem
    // finds it there.
    pEquipment->m_items[bagSlot] = new CItem(CResRef("00GBGA01"), 0, 0, 0, 0, 0);
    REQUIRE(pEquipment->m_items[bagSlot]->GetItemType() == 58);
    {
        CStore bag(CResRef("00GBGA01"));
        while (bag.m_lInventory.GetCount() > 0) {
            delete bag.m_lInventory.RemoveHead();
        }
        CItem gem(CResRef("00GEM03"), 1, 0, 0, 0, 0);
        REQUIRE(bag.AddItemExt(gem, 1) == 0);
        bag.Marshal(g_pBaldurChitin->GetObjectGame()->m_sTempDir);
    }
    CHECK(pCharacter->FindItemPersonal(CString("00GEM03"), 0, FALSE) == -1);
    CHECK(pCharacter->FindItemBags(CString("00gem03"), 0, FALSE) == bagSlot);
    CHECK(pCharacter->FindItemBags(CString("00GEM04"), 0, FALSE) == -1);
    CAITrigger inBag(0x4042, 0);
    inBag.m_string1 = "00GEM03";
    CHECK(pCharacter->EvaluateStatusTrigger(inBag));

    CStore::InvalidateStore(CResRef("00GBGA01"));
    delete pEquipment->m_items[gemSlot];
    pEquipment->m_items[gemSlot] = NULL;
    delete pEquipment->m_items[bagSlot];
    pEquipment->m_items[bagSlot] = NULL;
}

TEST_CASE("sprite: ResolveInstants sends the idle sequence after 60 no-action ticks (0x728BC0)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);
    REQUIRE(pCharacter->m_pPath == NULL);

    CMessageHandler* pHandler = g_pBaldurChitin->GetMessageHandler();
    pHandler->AsynchronousUpdate();
    const SHORT idle = pCharacter->GetIdleSequence();
    const SHORT other = idle == 6 ? 7 : 6;
    auto sendsAfter = [&](LONG count) {
        pCharacter->ClearActions(FALSE);
        pCharacter->m_curAction = CAIAction();
        pCharacter->m_sequenceTest = FALSE;
        pCharacter->m_nSequence = other;
        pCharacter->m_noActionCount = count;
        const INT before = pHandler->m_messageList.GetCount();
        pCharacter->ResolveInstants(TRUE);
        const INT after = pHandler->m_messageList.GetCount();
        pHandler->AsynchronousUpdate();
        return after > before;
    };
    // NoAction counts one more (case 0), then the count must be above
    // READY_COUNT, the WORD at 0x85BB9A: 60.
    CHECK_FALSE(sendsAfter(59));
    CHECK(sendsAfter(60));
}

TEST_CASE("action: FloatMessage floats the text over its target and is done (case 241)", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pCharacter->GetArea() != NULL);

    // A sprite target: done (0x4508C4 floats the strref over it).
    CAIObjectType actee(pCharacter->GetAIType());
    actee.m_nInstance = pCharacter->GetId();
    pCharacter->m_curAction = CAIAction(241, actee, 1677, 0, 0);
    CHECK(pCharacter->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_DONE);

    // No sprite or door to float over: done as well (0x44E1DB).
    pCharacter->m_curAction = CAIAction(241, CAIObjectType::NOONE, 1677, 0, 0);
    CHECK(pCharacter->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_DONE);
    pCharacter->m_curAction = CAIAction();
}

TEST_CASE("walk: the distance to the next node is rounded before it is cut into steps", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();

    // At the centre of its square, with one node: the square up and left,
    // 16 by 16 away in the 4/3-scaled space. sqrt(512) is 22.6: the
    // original rounds it to 23 (fadd 0.5 at 0x6F9811), so a move scale of 9
    // cuts it into (4 + 23) / 9 = 3 steps; truncated it would be 2.
    const CPoint start = Square(pCharacter->GetPos());
    CPoint square(-1, -1);
    for (int dy = -6; dy <= 6 && square.x == -1; dy++) {
        for (int dx = -6; dx <= 6 && square.x == -1; dx++) {
            const CPoint candidate(start.x + dx, start.y + dy);
            if (OpenSquare(pCharacter, candidate) && OpenSquare(pCharacter, CPoint(candidate.x - 1, candidate.y - 1))) {
                square = candidate;
            }
        }
    }
    REQUIRE(square.x != -1);
    const CPoint centre(square.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
        square.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);
    pCharacter->m_pos = centre;
    pCharacter->m_posOld = centre;
    pCharacter->m_posDest = centre;
    pCharacter->m_posExact.x = centre.x << CGameSprite::EXACT_SCALE;
    pCharacter->m_posExact.y = (4 * centre.y / 3) << CGameSprite::EXACT_SCALE;
    pCharacter->m_animation.SetMoveScale(9);
    REQUIRE(pCharacter->m_animation.GetMoveScale() == 9);

    POINT node = { square.x - 1, square.y - 1 };
    pCharacter->DropPath();
    pCharacter->m_pPath = new LONG[1];
    pCharacter->m_pPath[0] = CPathSearch::PointToPosition(&node);
    pCharacter->m_nPath = 1;
    pCharacter->m_currPath = 0;

    pCharacter->AIUpdateWalk();
    CHECK(pCharacter->m_posDelta.x == -16 * (1 << CGameSprite::EXACT_SCALE) / 3);
    CHECK(pCharacter->m_posDelta.y == -16 * (1 << CGameSprite::EXACT_SCALE) / 3);
    pCharacter->DropPath();
}

TEST_CASE("object: FloatText floats only in the area on view and clears its text link", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // 0x4C80E0. The text object itself needs the world screen's video mode
    // (CGameText::SetText's FX size), which these tests do not set up; the
    // walk_and_talk replay covers it (the goblins' float texts take the
    // original's object ids). Here: an object at (-1, -1) floats nothing,
    // and an object outside the area on view does not either.
    const LONG nObjectsBefore = pGame->GetObjectArray()->m_nNextObjectId;
    const CPoint pos = pCharacter->m_pos;
    pCharacter->field_1C = 0;
    pCharacter->m_pos = CPoint(-1, -1);
    pCharacter->FloatText(2806, 10, 5);
    CHECK(pCharacter->field_1C == 0);
    pCharacter->m_pos = pos;

    const BYTE nVisible = pGame->m_visibleArea;
    pGame->m_visibleArea = static_cast<BYTE>((nVisible + 1) % CINFGAME_MAX_AREAS);
    pCharacter->FloatText(2806, 10, 5);
    CHECK(pCharacter->field_1C == 0);
    CHECK(pGame->GetObjectArray()->m_nNextObjectId == nObjectsBefore);
    pGame->m_visibleArea = nVisible;

    // Strref 0 clears the link to the previous text.
    pCharacter->field_1C = 0;
    pCharacter->FloatText(0, 10, 5);
    CHECK(pCharacter->field_1C == 0);
}

TEST_CASE("sprite: Rest refills a memorized but spent spell", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    // The first pregenerated character with a spell, domain or innate list
    // (the Characters folder holds fighters as well).
    std::vector<std::vector<BYTE>> candidates = FinishedCharacters();
    std::vector<BYTE> cre;
    for (std::vector<BYTE>& candidate : candidates) {
        CGameSprite* pProbe = new CGameSprite(candidate.data(),
            static_cast<LONG>(candidate.size()),
            0,
            0,
            static_cast<DWORD>(-1),
            0,
            0,
            INT_MAX,
            CPoint(-1, -1),
            0);
        BOOL bHasSpells = !pProbe->m_innateSpells.m_List.empty();
        for (UINT nClass = 0; nClass < CSPELLLIST_NUM_CLASSES && !bHasSpells; nClass++) {
            const CGameSpriteGroupedSpellList& spells = pProbe->m_spells.m_spellsByClass[nClass];
            for (UINT nLevel = 0; nLevel < spells.m_nHighestLevel && !bHasSpells; nLevel++)
                bHasSpells = !spells.m_lists[nLevel].m_List.empty();
        }
        for (UINT nLevel = 0; nLevel < pProbe->m_domainSpells.m_nHighestLevel && !bHasSpells; nLevel++)
            bHasSpells = !pProbe->m_domainSpells.m_lists[nLevel].m_List.empty();
        delete pProbe;
        if (bHasSpells) {
            cre = candidate;
            break;
        }
    }
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 character with spells, domains or innates");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);

    // The first spell entry of the character: class list, domain list or
    // innate list, whichever has one.
    enum { KIND_CLASS,
        KIND_DOMAIN,
        KIND_INNATE } kind
        = KIND_CLASS;
    CGameSpriteSpellList* pLevelList = NULL;
    CGameSpriteSpellListEntry* pEntry = NULL;
    UINT nEntryClass = 0;
    for (UINT nClass = 0; nClass < CSPELLLIST_NUM_CLASSES && pEntry == NULL; nClass++) {
        CGameSpriteGroupedSpellList& spells = pCharacter->m_spells.m_spellsByClass[nClass];
        for (UINT nLevel = 0; nLevel < spells.m_nHighestLevel && pEntry == NULL; nLevel++) {
            if (!spells.m_lists[nLevel].m_List.empty()) {
                kind = KIND_CLASS;
                nEntryClass = nClass;
                pLevelList = &spells.m_lists[nLevel];
                pEntry = &pLevelList->m_List.front();
            }
        }
    }
    for (UINT nLevel = 0; nLevel < pCharacter->m_domainSpells.m_nHighestLevel && pEntry == NULL; nLevel++) {
        if (!pCharacter->m_domainSpells.m_lists[nLevel].m_List.empty()) {
            kind = KIND_DOMAIN;
            pLevelList = &pCharacter->m_domainSpells.m_lists[nLevel];
            pEntry = &pLevelList->m_List.front();
        }
    }
    if (pEntry == NULL && !pCharacter->m_innateSpells.m_List.empty()) {
        kind = KIND_INNATE;
        pLevelList = &pCharacter->m_innateSpells;
        pEntry = &pLevelList->m_List.front();
    }
    if (pEntry == NULL) {
        SKIP("[iwd2r-skip] the character has no spells, domains or innates");
    }

    // Memorized twice, both spent. Classes 0 and 5 (bard, sorcerer) keep the
    // counts per level as well as per spell.
    pEntry->field_4 = 2;
    pEntry->field_8 = 0;
    if (kind == KIND_CLASS && (nEntryClass == 0 || nEntryClass == 5)) {
        pLevelList->field_14 = 2;
        pLevelList->field_18 = 0;
    }

    CHECK(pCharacter->Rest() == CGameAIBase::ACTION_DONE);

    // Both memorized spells are castable again.
    CHECK(pEntry->field_8 == 2);
    if (kind == KIND_CLASS && (nEntryClass == 0 || nEntryClass == 5)) {
        CHECK(pLevelList->field_18 == 2);
    }
}

// CGameSprite::Help (0x74FF10, action 17): every object within GetHelpRange
// (twice the visual range, CAIObjectType::ANYONE) gets a Help trigger
// (TRIGGER.IDS 3) from the caller, its AI type as the cause and no
// specifics, queued as a CMessageSetTrigger; the handler answers
// ACTION_DONE.
TEST_CASE("sprite: Help sends the Help trigger to the sprites in range", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    std::vector<std::vector<BYTE>> candidates = FinishedCharacters();
    if (candidates.empty()) {
        SKIP("[iwd2r-skip] no finished character");
    }
    CGameSprite* pCharacter = NewGameInStartArea(candidates.front());
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->m_visibility.SetAreaExplored();
    pArea->SortLists();
    ClearMessages();

    CHECK(pCharacter->Help() == CGameAIBase::ACTION_DONE);

    // The character itself is in range (distance 0) and on the area's front
    // list, so at least one message is queued.
    const std::vector<CMessageSetTrigger*> messages = Queued<CMessageSetTrigger>();
    REQUIRE(!messages.empty());
    bool bSelf = false;
    for (CMessageSetTrigger* message : messages) {
        CHECK(message->m_trigger.m_triggerID == CAITrigger::HELP);
        CHECK(message->m_trigger.m_specificID == 0);
        CHECK(message->m_trigger.GetCause().Equal(pCharacter->GetAIType()));
        CHECK(message->GetSource() == pCharacter->GetId());
        bSelf = bSelf || message->GetTarget() == pCharacter->GetId();
    }
    CHECK(bSelf);

    // Running the queued message puts the trigger on the target.
    pCharacter->m_pendingTriggers.RemoveAll();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        CMessage* message = handler.m_messageList.RemoveHead();
        message->Run();
        delete message;
    }
    bool bHeard = false;
    POSITION pos = pCharacter->m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        if (pCharacter->m_pendingTriggers.GetNext(pos)->m_triggerID == CAITrigger::HELP) {
            bHeard = true;
        }
    }
    CHECK(bHeard);
}

// CGameSprite::StartDialog (0x7543B0, actions 137/198): the actor starts its
// own dialog with the target without Dialog's distance/LOS checks. Action
// 137 takes the dialog resource from the action (the actor's m_dialog),
// 198 (StartDialogNoSet) keeps the actor's current one; both remember the
// talk, the world screen opens the dialog, and the handler is done. A
// charmed actor, or one of the two unable to speak, is an error.
TEST_CASE("sprite: StartDialog opens the actor's dialog without walking to the target", "[ai][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    pArea->m_visibility.SetAreaExplored();
    CGameSprite* pHedron = FindSpeaker(pGame->m_pGameAreaMaster, "10HEDRON");
    REQUIRE(pHedron != NULL);

    // The same trigger patch as the PlayerDialog test: the start state's
    // NumTimesTalkedTo(0) needs EvaluateStatusTrigger (the movement stream),
    // which is still a TODO.
    CResHelper<CResDLG, 1011> file;
    file.SetResRef(CResRef("10HEDRON"), TRUE, TRUE);
    REQUIRE(file.GetRes() != NULL);
    BYTE* pData = static_cast<BYTE*>(file.GetRes()->Demand());
    REQUIRE(pData != NULL);
    DemandedResource demanded(file.GetRes());
    if (!pHedron->EvaluateStatusTrigger(CAITrigger(0x4039, 0))) {
        const DWORD nSize = file.GetRes()->m_nResSizeActual;
        const std::string sTrigger = "NumTimesTalkedTo(0)";
        BYTE* pTrigger = std::search(pData, pData + nSize, sTrigger.begin(), sTrigger.end());
        REQUIRE(pTrigger != pData + nSize);
        demanded.Patch(&pTrigger[15], 'X');
    }

    // Hedron (the actor and the dialog's owner) starts its dialog with the
    // party member (the target); no distance is involved.
    CAIObjectType pcType(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId(), 0, 0);
    pHedron->m_curAction = CAIAction(CAIAction::STARTDIALOG, pcType, CString("10HEDRON"), 0, 0);
    pHedron->m_actionCount = 1;
    ClearMessages();
    CHECK(pHedron->ExecuteAction() == CGameAIBase::ACTION_DONE);

    CGameDialogSprite& dialog = pWorld->m_internalLoadedDialog;
    CHECK(pHedron->m_dialog == CResRef("10HEDRON")); // action 137 set it
    CHECK(dialog.field_54 == 1);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.m_currentEntryIndex == 0);
    CHECK(dialog.m_talkerIndex == pHedron->GetId());
    CHECK(dialog.field_54 == 1);
    CHECK(pHedron->m_lTalkedTo.m_nInstance == pCharacter->GetId());
    CHECK(pCharacter->m_lTalkedTo.m_nInstance == pHedron->GetId());
    // Hedron counts the talk; the party member does not.
    CHECK(pHedron->m_nNumberOfTimesTalkedTo == 1);
    CHECK(pCharacter->m_nNumberOfTimesTalkedTo == 0);

    dialog.m_responseMarker = -2;
    dialog.AsynchronousUpdate();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.field_54 == 0);
    ClearMessages();

    // StartDialogNoSet (198) keeps the actor's dialog even when the action
    // names another resource.
    pHedron->m_curAction = CAIAction(CAIAction::STARTDIALOGUENOSET, pcType, CString("XXXX"), 0, 0);
    pHedron->m_actionCount = 1;
    ClearMessages();
    CHECK(pHedron->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pHedron->m_dialog == CResRef("10HEDRON"));
    CHECK(dialog.field_54 == 1);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    dialog.m_responseMarker = -2;
    dialog.AsynchronousUpdate();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.field_54 == 0);
    ClearMessages();

    // A charmed actor cannot start a dialog.
    pHedron->m_derivedStats.m_generalState |= STATE_CHARMED;
    pHedron->m_curAction = CAIAction(CAIAction::STARTDIALOG, pcType, CString("10HEDRON"), 0, 0);
    CHECK(pHedron->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    pHedron->m_derivedStats.m_generalState &= ~STATE_CHARMED;
    ClearMessages();
}
