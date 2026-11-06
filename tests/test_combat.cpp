#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameAnimationTypeMonsterIcewind.h"
#include "CGameArea.h"
#include "CGameContainer.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CGameSpriteEquipment.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSpell.h"
#include "CUIControlTextDisplay.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CVidMode.h"
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
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Combat on the engine in this process (milestone M3): CGameSprite::Hit
// 0x73B760, Damage 0x73D560, GetAC 0x723CC0, OneSwing 0x7446F0, Swing
// 0x739B60 and the Attack action 0x737EF0, with a party member of a new
// game in AR1000 and a goblin (10GOB.CRE) put next to it. The rand draws
// of each function are pinned against the VC6 generator. Every engine works
// on a private view of the install (tests/data_view.h).

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

// The CRE of the named pregenerated character (Characters/<name>.chr), as
// tests/test_spellbook_screen.cpp does. The fixture names it because the
// first file in an install's Characters folder is whatever that install
// holds (a played one collects exported characters), and these tests pin
// hit points.
std::vector<BYTE> NamedCharacter(const std::string& name)
{
    std::ifstream in(Child(Child(g_view, "Characters"), name + ".chr"), std::ios::binary);
    const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
        const uint32_t offset = Le32(chr, 0x28);
        const uint32_t size = Le32(chr, 0x2C);
        if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
            return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
    }
    return std::vector<BYTE>();
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

// A creature of the game's files put on the area's front list at `pos`.
CGameSprite* AddCreature(CGameArea* pArea, const char* resRef, const CPoint& pos)
{
    CCreatureFile creatureData;
    creatureData.SetResRef(CResRef(resRef), TRUE, TRUE);
    if (creatureData.GetDataSize() == 0) {
        return NULL;
    }
    CGameSprite* pSprite = new CGameSprite(creatureData.GetData(),
        creatureData.GetDataSize(),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        pos,
        0);
    pSprite->SetResRef(CResRef(resRef));
    creatureData.ReleaseData();
    pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_FRONT);
    return pSprite;
}

// The number of VC6 rand() draws that lead from `before` to `after` (the
// generator's state), or -1 beyond `limit`.
int Draws(unsigned int before, unsigned int after, int limit = 1000)
{
    unsigned int state = before;
    for (int n = 0; n <= limit; n++) {
        if (state == after) {
            return n;
        }
        state = state * 214013u + 2531011u;
    }
    return -1;
}

// The values rand() returns from the state `state` on.
std::vector<int> NextRands(unsigned int state, int count)
{
    std::vector<int> values;
    for (int n = 0; n < count; n++) {
        state = state * 214013u + 2531011u;
        values.push_back(static_cast<int>((state >> 16) & 0x7FFF));
    }
    return values;
}

// A seed for the n-th case: consecutive small seeds start the generator
// with nearly equal first values.
unsigned int Seed(unsigned int n)
{
    return n * 0x9E3779B1u;
}

// CUtil::UtilRandInt on a given rand() value.
LONG RandInt(LONG nRange, LONG nWeight, int value)
{
    LONG nValue = nWeight + ((nRange * value) >> 15);
    if (nValue >= nRange) {
        nValue = nRange - 1;
    }
    if (nValue < 0) {
        nValue = 0;
    }
    return nValue;
}

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// A new game with the party member and a goblin next to it, both on the
// area's front list.
struct Fight {
    CGameSprite* pCharacter = NULL;
    CGameSprite* pGoblin = NULL;
    CGameArea* pArea = NULL;
};

bool SetUpFight(Fight& fight, std::string& why)
{
    std::string error;
    if (!StartEngine(error)) {
        why = error;
        return false;
    }
    std::vector<BYTE> cre = NamedCharacter("wrchar3");
    if (cre.empty()) {
        why = "Characters/wrchar3.chr not found or not CHR V2.2";
        return false;
    }
    // The engine seeds rand() from the clock (`srand(time)` in
    // CChitin::InitializeVariables 0x7902C0) and the new game draws from it
    // (the creatures' first animation frames, so the frame an idle sequence
    // ends on): seeded here, the fighters start the same in every run, and
    // a fight after the test's own srand() draws its rolls in the same
    // order.
    srand(1);
    fight.pCharacter = NewGameInStartArea(cre);
    fight.pArea = fight.pCharacter->GetArea();
    if (fight.pArea == NULL) {
        why = "the party member is in no area";
        return false;
    }
    fight.pArea->m_visibility.SetAreaExplored();
    const CPoint& pos = fight.pCharacter->GetPos();
    fight.pGoblin = AddCreature(fight.pArea, "10GOB", CPoint(pos.x + 2 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    if (fight.pGoblin == NULL) {
        why = "10GOB.CRE not found";
        return false;
    }
    fight.pArea->SortLists();
    ClearMessages();
    return true;
}

// One tick as the area runs it, for the fighters and the projectiles in
// flight only: the game time, each object's AIUpdate when DoAIUpdate lets
// it, the messages, then the area's lists sorted. (Not the whole world screen: the area's scripts
// start AR1000's cut-scene, which switches engines and starts the sound.)
void Tick(CGameArea* pArea, std::initializer_list<CGameSprite*> fighters)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->GetWorldTimer()->UpdateTime(FALSE);
    g_pChitin->nAUCounter++;
    for (CGameSprite* pFighter : fighters) {
        if (pFighter->DoAIUpdate(pGame->GetWorldTimer()->m_active, g_pChitin->nAUCounter)) {
            pFighter->AIUpdate();
        }
    }

    std::vector<LONG> projectiles;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                if (pObject->IsProjectile()) {
                    projectiles.push_back(id);
                }
                pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            }
        }
    }
    for (LONG id : projectiles) {
        CGameObject* pObject;
        if (pGame->GetObjectArray()->GetDeny(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
            if (pObject->DoAIUpdate(pGame->GetWorldTimer()->m_active, g_pChitin->nAUCounter)) {
                pObject->AIUpdate();
            }
            pGame->GetObjectArray()->ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
    }

    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    // The objects added during the tick (a projectile that flies, as Magic
    // Missile's) join the lists the next tick walks, and the removed ones
    // leave them.
    pArea->SortLists();
}

}

TEST_CASE("combat: a party member melees a goblin in AR1000 until it dies and casts a spell", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    // The goblin is hostile to the party.
    REQUIRE(pGoblin->GetAIType().m_nEnemyAlly > CAIObjectType::EA_GOODCUTOFF);
    const SHORT goblinStart = pGoblin->GetBaseStats()->m_hitPoints;
    const SHORT characterStart = pCharacter->GetBaseStats()->m_hitPoints;
    REQUIRE(goblinStart > 0);
    INFO("goblin hit points " << goblinStart << ", character " << characterStart);

    CAIAction attack(CAIAction::ATTACK, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId()), 0, 0, 0);
    pCharacter->ClearActions(FALSE);
    pCharacter->AddAction(attack);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    int swings = 0;
    SHORT lowest = goblinStart;
    SHORT previous = goblinStart;
    bool hpRose = false;
    while (frames < 6000) {
        Tick(fight.pArea, { pCharacter, pGoblin });
        if (pCharacter->m_attackFrame == 0) {
            swings++;
        }
        SHORT hp = pGoblin->GetBaseStats()->m_hitPoints;
        if (hp > previous) {
            hpRose = true;
        }
        previous = hp;
        lowest = std::min(lowest, hp);
        frames++;
        if ((pGoblin->GetDerivedStats()->m_generalState & STATE_DEAD) != 0) {
            break;
        }
    }
    INFO("frames " << frames << ", rounds " << swings << ", goblin hit points " << pGoblin->GetBaseStats()->m_hitPoints);
    CHECK((pGoblin->GetDerivedStats()->m_generalState & STATE_DEAD) != 0);
    CHECK(lowest < goblinStart);
    CHECK_FALSE(hpRose);
    // The character keeps hit points between none and its start (the goblin
    // may hit back).
    CHECK(pCharacter->GetBaseStats()->m_hitPoints <= characterStart);
    CHECK(pCharacter->GetBaseStats()->m_hitPoints >= 0);

    // One spell: Magic Missile (SPWI112) on a second goblin a few squares
    // off, through CGameAIBase::FireSpell (0x467C50), its projectile flying
    // until it delivers the damage.
    const CPoint& pos = pCharacter->GetPos();
    CGameSprite* pTarget = AddCreature(fight.pArea, "10GOB", CPoint(pos.x - 6 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    REQUIRE(pTarget != NULL);
    fight.pArea->SortLists();
    const SHORT targetStart = pTarget->GetBaseStats()->m_hitPoints;
    REQUIRE(targetStart > 0);
    CHECK(pCharacter->FireSpell(CResRef("SPWI112"), pTarget) == CGameAIBase::ACTION_DONE);
    int spellFrames = 0;
    while (spellFrames < 600 && pTarget->GetBaseStats()->m_hitPoints == targetStart) {
        Tick(fight.pArea, { pCharacter, pTarget });
        spellFrames++;
    }
    INFO("spell frames " << spellFrames << ", target hit points " << targetStart << " -> " << pTarget->GetBaseStats()->m_hitPoints);
    // Magic Missile does 1d4+1 at the first caster levels.
    CHECK(pTarget->GetBaseStats()->m_hitPoints < targetStart);
    CHECK(pTarget->GetBaseStats()->m_hitPoints >= targetStart - 5);
}

// DropInventory (0x754EE0), as a dying goblin drops its loot: every
// droppable item but the excepted one and the gold (as MISC07) go onto its
// ground pile; the fist stays.
TEST_CASE("combat: DropInventory puts a goblin's droppable items and gold on its ground pile", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pGoblin = fight.pGoblin;
    CGameSpriteEquipment* pEquipment = pGoblin->GetEquipment();

    std::vector<CString> droppable;
    CString sKept;
    for (SHORT nSlot = 0; nSlot < CGameSpriteEquipment::NUM_SLOT; nSlot++) {
        CItem* pItem = pEquipment->m_items[nSlot];
        if (nSlot == CGameSpriteEquipment::SLOT_FIST || pItem == NULL || (pItem->GetFlagsFile() & 0x4) == 0) {
            continue;
        }
        if (sKept.IsEmpty()) {
            // The first droppable item is the exception: it stays.
            sKept = pItem->GetResRef().GetResRefStr();
        } else {
            droppable.push_back(pItem->GetResRef().GetResRefStr());
        }
    }
    INFO("droppable " << droppable.size() << ", kept " << static_cast<const char*>(sKept));
    pGoblin->GetBaseStats()->m_gold = 7;

    CHECK(pGoblin->DropInventory(FALSE, sKept) == CGameAIBase::ACTION_DONE);
    // The pile takes the items through CMessageContainerAddItem.
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    LONG nPile = g_pBaldurChitin->GetObjectGame()->GetGroundPile(pGoblin->GetId());
    REQUIRE(nPile != CGameObjectArray::INVALID_INDEX);
    CGameContainer* pPile = nullptr;
    REQUIRE(g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nPile, CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pPile), INFINITE)
        == CGameObjectArray::SUCCESS);
    std::vector<CString> onPile;
    WORD nGold = 0;
    for (SHORT nSlot = 0; nSlot < pPile->GetNumItems(); nSlot++) {
        CItem* pItem = pPile->GetItem(nSlot);
        if (pItem == NULL) {
            continue;
        }
        if (pItem->GetResRef() == "MISC07") {
            nGold = pItem->GetUsageCount(0);
        } else {
            onPile.push_back(pItem->GetResRef().GetResRefStr());
        }
    }
    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nPile, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    CHECK(onPile.size() == droppable.size());
    CHECK(nGold == 7);
    CHECK(pGoblin->GetBaseStats()->m_gold == 0);
    bool kept = false;
    for (SHORT nSlot = 0; nSlot < CGameSpriteEquipment::NUM_SLOT; nSlot++) {
        CItem* pItem = pEquipment->m_items[nSlot];
        if (pItem != NULL && nSlot != CGameSpriteEquipment::SLOT_FIST && (pItem->GetFlagsFile() & 0x4) != 0) {
            CHECK(pItem->GetResRef() == static_cast<const char*>(sKept));
            kept = true;
        }
    }
    CHECK(kept == !sKept.IsEmpty());
    CHECK(pEquipment->m_items[CGameSpriteEquipment::SLOT_FIST] != NULL);
    ClearMessages();
}

// EquipMostDamagingMelee (0x75DFD0): the goblin's melee weapon set, not the
// fist; with the magic weapon slot in use nothing changes (ACTION_ERROR).
TEST_CASE("combat: EquipMostDamagingMelee takes the goblin's melee weapon back from its fists", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pGoblin = fight.pGoblin;
    CGameSpriteEquipment* pEquipment = pGoblin->GetEquipment();
    const BYTE nWeapon = pEquipment->m_selectedWeapon;
    INFO("selected weapon slot " << static_cast<int>(nWeapon));
    REQUIRE(nWeapon >= CGameSpriteEquipment::SLOT_WEAPON);
    REQUIRE(pEquipment->m_items[nWeapon] != NULL);

    pGoblin->SelectWeaponAbility(CGameSpriteEquipment::SLOT_FIST, 0, 0, 1);
    REQUIRE(pEquipment->m_selectedWeapon == CGameSpriteEquipment::SLOT_FIST);
    CHECK(pGoblin->EquipMostDamagingMelee() == CGameAIBase::ACTION_DONE);
    CHECK(pEquipment->m_selectedWeapon == nWeapon);

    // EquipWeapon (299) is done at once for a sprite holding a weapon.
    pGoblin->ClearActions(FALSE);
    pGoblin->m_curAction = CAIAction(299, CPoint(-1, -1), 0, -1);
    CHECK(pGoblin->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pEquipment->m_selectedWeapon == nWeapon);
    ClearMessages();
}

TEST_CASE("combat: GetAC sums the armour class modifiers and the capped dexterity bonus", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pGoblin = fight.pGoblin;
    CDerivedStats* pStats = pGoblin->GetDerivedStats();
    REQUIRE(pGoblin->GetClassLevel(CAIOBJECTTYPE_C_MONK) == 0);

    CItem* pArmor = pGoblin->m_equipment.m_items[1];
    pGoblin->m_equipment.m_items[1] = NULL;
    pStats->field_6 = 1;
    pStats->field_8 = 2;
    pStats->field_A = 3;
    pStats->field_C = 4;
    pStats->m_nDEX = 14;
    pStats->m_generalState &= ~STATE_BLIND;

    srand(7);
    const unsigned int before = iwd2r_rand_state();
    CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 + 2);
    // The touch armour class leaves out field_6.
    CHECK(pGoblin->sub_723DB0() == 2 + 3 + 4 + 10 + 2);
    CHECK(iwd2r_rand_state() == before);

    // Blind, without blind fight, the dexterity bonus is lost; a penalty
    // stays.
    pStats->m_generalState |= STATE_BLIND;
    REQUIRE_FALSE(pGoblin->sub_763150(CGAMESPRITE_FEAT_BLIND_FIGHT));
    CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10);
    pStats->m_nDEX = 6;
    CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 - 2);
    pStats->m_generalState &= ~STATE_BLIND;

    // Armour caps the dexterity bonus by its type: leather (60) at 6, chain
    // mail (62) at 2, plate (64) at 0. The goblin wears none, so it is
    // given each; only the item type is read.
    struct Armor {
        const char* resRef;
        WORD type;
        INT cap;
    };
    const Armor armors[] = {
        { "00LEAT01", 60, 6 },
        { "00CHAN01", 62, 2 },
        { "00PLAT01", 64, 0 },
    };
    pStats->m_nDEX = 30;
    for (const Armor& armor : armors) {
        INFO(armor.resRef);
        CItem* pWorn = new CItem(CResRef(armor.resRef), 0, 0, 0, 0, 0);
        REQUIRE(pWorn->GetItemType() == armor.type);
        pGoblin->m_equipment.m_items[1] = pWorn;
        CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 + armor.cap);
        pGoblin->m_equipment.m_items[1] = NULL;
        delete pWorn;
    }
    // Without armour, the whole bonus (+10 at 30).
    CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 + 10);
    pGoblin->m_equipment.m_items[1] = pArmor;
}

TEST_CASE("combat: Hit draws the d20 and the confirmation d20 of a threat", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    // The fist: a melee weapon every character has.
    CItem* pWeapon = pCharacter->m_equipment.m_items[10];
    REQUIRE(pWeapon != NULL);
    pWeapon->Demand();
    const ITEM_ABILITY* pAbility = pWeapon->GetAbility(0);
    REQUIRE(pAbility != NULL);
    REQUIRE(pAbility->type == 1);
    const SHORT nRange = pWeapon->sub_4E9840(pAbility);
    pWeapon->Release();
    const SHORT nBonus = pCharacter->GetCriticalHitBonus();
    const SHORT nLuck = pCharacter->GetDerivedStats()->m_nLuck;
    const bool bImmune = (pGoblin->GetBaseStats()->field_2FB & 0x2) != 0;

    int threats = 0;
    int hits = 0;
    for (unsigned int seed = 1; seed <= 200; seed++) {
        srand(Seed(seed));
        const unsigned int before = iwd2r_rand_state();
        const int value = NextRands(before, 1)[0];
        const LONG nRoll = RandInt(20, nLuck, value) + 1;
        BOOL bCritical = FALSE;
        const BOOL bHit = pCharacter->Hit(pGoblin, pWeapon, 0, &bCritical, FALSE, 1);
        const int draws = Draws(before, iwd2r_rand_state());
        const bool bThreat = nRange + nBonus + nRoll > 19 && !bImmune;
        INFO("seed " << seed << ", roll " << nRoll);
        CHECK(draws == (bThreat ? 2 : 1));
        if (nRoll == 20) {
            CHECK(bHit);
        }
        if (nRoll == 1) {
            CHECK_FALSE(bHit);
        }
        if (bCritical) {
            CHECK(bThreat);
            CHECK(bHit);
        }
        threats += bThreat ? 1 : 0;
        hits += bHit ? 1 : 0;
        ClearMessages();
    }
    CHECK(threats > 0);
    CHECK(hits > 0);
    CHECK(hits < 200);

    // A blinking target avoids the attack on rand() % 100 < 50, and the
    // feedback (63, strref 18353) is the target's (0x73B947 `mov ecx,ebx`).
    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD nEffectTextLevel = pOptions->m_nEffectTextLevel;
    pOptions->m_nEffectTextLevel |= 0x2;
    pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_BLINK] = false;
    pGoblin->GetDerivedStats()->m_spellStates[SPLSTATE_BLINK] = true;
    int blinks = 0;
    for (unsigned int seed = 1; seed <= 20; seed++) {
        ClearMessages();
        srand(Seed(seed));
        const unsigned int before = iwd2r_rand_state();
        if (NextRands(before, 1)[0] % 100 >= 50) {
            continue;
        }
        INFO("blink seed " << seed);
        BOOL bCritical = FALSE;
        CHECK_FALSE(pCharacter->Hit(pGoblin, pWeapon, 0, &bCritical, FALSE, 1));
        CHECK(Draws(before, iwd2r_rand_state()) == 1);
        CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
        REQUIRE(handler.m_messageList.GetCount() == 1);
        auto* pMessage = dynamic_cast<CMessageDisplayTextRef*>(handler.m_messageList.GetHead());
        REQUIRE(pMessage != NULL);
        CHECK(pMessage->m_text == 18353);
        CHECK(pMessage->GetSource() == pGoblin->GetId());
        CHECK(pMessage->GetTarget() == pGoblin->GetId());
        blinks++;
    }
    CHECK(blinks > 0);
    pGoblin->GetDerivedStats()->m_spellStates[SPLSTATE_BLINK] = false;
    pOptions->m_nEffectTextLevel = nEffectTextLevel;
    ClearMessages();
}

TEST_CASE("combat: Damage rolls each die of the weapon once and Kai maximises them", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    // No sneak attack: its dice would draw too.
    pCharacter->GetBaseStats()->m_rogueLevel = 0;

    CItem* pWeapon = pCharacter->m_equipment.m_items[10];
    REQUIRE(pWeapon != NULL);
    pWeapon->Demand();
    const ITEM_ABILITY* pAbility = pWeapon->GetAbility(0);
    REQUIRE(pAbility != NULL);
    const int nDice = pAbility->damageDiceCount;
    const int nSize = pAbility->damageDice;
    pWeapon->Release();
    const SHORT nLuck = pCharacter->GetDerivedStats()->m_nLuck;
    REQUIRE(nDice > 0);

    for (unsigned int seed = 1; seed <= 50; seed++) {
        INFO("seed " << seed);
        pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = false;
        srand(Seed(seed));
        unsigned int before = iwd2r_rand_state();
        std::vector<int> values = NextRands(before, nDice);
        CGameEffect* pRolled = pCharacter->Damage(pWeapon, NULL, 0, FALSE, pGoblin->m_typeAI,
            pGoblin->m_nDirection, pCharacter->GetDirection(pGoblin->GetPos()), pGoblin, FALSE);
        CHECK(Draws(before, iwd2r_rand_state()) == nDice);
        REQUIRE(pRolled != NULL);
        CHECK(pRolled->m_effectID == CGAMEEFFECT_DAMAGE);
        CHECK(pRolled->m_sourceID == pCharacter->GetId());
        CHECK(pRolled->m_effectAmount >= 1);
        int nRolled = 0;
        for (int value : values) {
            nRolled += RandInt(nSize, nLuck, value) + 1;
        }

        pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = true;
        srand(Seed(seed));
        before = iwd2r_rand_state();
        CGameEffect* pMaximised = pCharacter->Damage(pWeapon, NULL, 0, FALSE, pGoblin->m_typeAI,
            pGoblin->m_nDirection, pCharacter->GetDirection(pGoblin->GetPos()), pGoblin, FALSE);
        CHECK(Draws(before, iwd2r_rand_state()) == 0);
        REQUIRE(pMaximised != NULL);
        CHECK(pMaximised->m_effectAmount - pRolled->m_effectAmount == nDice * nSize - nRolled);

        // A critical multiplies the damage (the fist's multiplier).
        pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = true;
        CGameEffect* pCritical = pCharacter->Damage(pWeapon, NULL, 0, TRUE, pGoblin->m_typeAI,
            pGoblin->m_nDirection, pCharacter->GetDirection(pGoblin->GetPos()), pGoblin, FALSE);
        REQUIRE(pCritical != NULL);
        CHECK(pCritical->m_effectAmount
            == static_cast<SHORT>(pMaximised->m_effectAmount * static_cast<SHORT>(pWeapon->GetCriticalHitMultiplier())));

        delete pRolled;
        delete pMaximised;
        delete pCritical;
        ClearMessages();
    }
    pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = false;
}

TEST_CASE("combat: OneSwing and Swing draw the attack speed at the start of a round", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    srand(11);
    unsigned int before = iwd2r_rand_state();
    int value = NextRands(before, 1)[0];
    pCharacter->m_attackFrame = 0;
    pCharacter->OneSwing();
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CHECK(pCharacter->m_speedFactor == std::max(0, std::min(10, 5 + value % 6 - 3)));

    before = iwd2r_rand_state();
    pCharacter->m_attackFrame = 3;
    pCharacter->OneSwing();
    CHECK(Draws(before, iwd2r_rand_state()) == 0);
    ClearMessages();

    // Swing at the start of a round: the speed takes one UtilRandInt and the
    // round's frame starts at 0.
    srand(12);
    before = iwd2r_rand_state();
    pCharacter->m_attackFrame = -1;
    pCharacter->m_equipment.m_selectedWeapon = 10;
    pCharacter->m_equipment.m_selectedWeaponAbility = 0;
    pCharacter->Swing(pGoblin);
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CHECK(pCharacter->m_attackFrame == 0);
    CHECK(pCharacter->m_speedFactor >= 0);
    CHECK(pCharacter->m_speedFactor <= 10);
    ClearMessages();
}

TEST_CASE("combat: a goblin given AttackReevaluate swings at the party member", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    const SHORT characterStart = pCharacter->GetBaseStats()->m_hitPoints;

    // AttackReevaluate (134) on the party member, the target chosen again
    // after 2000 AI updates (the action's count).
    CAIAction attack(134, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId()), 2000, 0, 0);
    pGoblin->ClearActions(FALSE);
    pGoblin->AddAction(attack);
    pCharacter->ClearActions(FALSE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    int blows = 0;
    SHORT previous = characterStart;
    bool hpRose = false;
    while (frames < 1500 && pCharacter->GetBaseStats()->m_hitPoints > 0) {
        Tick(fight.pArea, { pCharacter, pGoblin });
        if (pGoblin->m_attackFrame == 0) {
            blows++;
        }
        SHORT hp = pCharacter->GetBaseStats()->m_hitPoints;
        if (hp > previous) {
            hpRose = true;
        }
        previous = hp;
        frames++;
    }
    INFO("frames " << frames << ", rounds " << blows << ", character hit points " << characterStart << " -> "
                   << pCharacter->GetBaseStats()->m_hitPoints);
    CHECK(blows == 16);
    CHECK(pGoblin->m_curAction.m_actionID == 134);
    // Hit points only drop; with the new game seeded (SetUpFight) every run
    // lands the same blows.
    CHECK_FALSE(hpRose);
    CHECK(characterStart == 9);
    CHECK(pCharacter->GetBaseStats()->m_hitPoints == 6);
}

TEST_CASE("combat: a party member casts Magic Missile through the Spell action", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    // A first-level wizard, whatever the probe's class (GetCasterLevel reads
    // the derived class levels).
    pCharacter->GetDerivedStats()->m_wizardLevel = 1;
    const SHORT goblinStart = pGoblin->GetBaseStats()->m_hitPoints;

    // Spell (31) with the resref, the class (wizard, 11) and the level.
    CAIAction spell(CAIAction::SPELL, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId()), CString("SPWI112"), 0,
        CAIOBJECTTYPE_C_WIZARD);
    spell.m_specificID3 = 1;
    pCharacter->ClearActions(FALSE);
    pCharacter->AddAction(spell);
    pGoblin->ClearActions(FALSE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    bool cast = false;
    while (frames < 600 && pGoblin->GetBaseStats()->m_hitPoints == goblinStart) {
        Tick(fight.pArea, { pCharacter, pGoblin });
        if (pCharacter->m_bStartedCasting) {
            cast = true;
        }
        frames++;
    }
    INFO("frames " << frames << ", goblin hit points " << goblinStart << " -> " << pGoblin->GetBaseStats()->m_hitPoints
                   << ", cast counter " << pCharacter->m_castCounter);
    CHECK(cast);
    CHECK(pGoblin->GetBaseStats()->m_hitPoints < goblinStart);
    CHECK(pGoblin->GetBaseStats()->m_hitPoints >= goblinStart - 5);
}

namespace {

// DirectDraw shim surfaces for the current video mode (XRGB8888, as the
// running engine's): the back buffer at the screen's size and the FX
// surfaces 2 and 3 at 512 by 512. The engine in these tests never activates
// its video mode, so it has none. The destructor restores the mode.
class Surfaces {
public:
    explicit Surfaces(CVidMode* pVidMode, int width, int height)
        : m_pVidMode(pVidMode)
        , m_nSurfacesBefore(pVidMode->m_nSurfaces)
        , m_pSurfacesBefore(pVidMode->pSurfaces)
        , m_nBppBefore(g_pChitin->cVideo.m_nBpp)
        , m_shifts { pVidMode->m_dwRBitShift, pVidMode->m_dwGBitShift, pVidMode->m_dwBBitShift }
    {
        for (IDirectDrawSurface*& pSurface : m_surfaces) {
            pSurface = NULL;
        }
        if (DirectDrawCreate(NULL, &m_pDirectDraw, NULL) != DD_OK) {
            m_pDirectDraw = NULL;
            return;
        }
        m_surfaces[CVIDINF_SURFACE_BACK] = Create(width, height);
        m_surfaces[CVIDINF_SURFACE_2] = Create(CVIDINF_FX_WIDTH, CVIDINF_FX_HEIGHT);
        m_surfaces[CVIDINF_SURFACE_3] = Create(CVIDINF_FX_WIDTH, CVIDINF_FX_HEIGHT);
        pVidMode->m_nSurfaces = 9;
        pVidMode->pSurfaces = m_surfaces;
        g_pChitin->cVideo.m_nBpp = 32;
        pVidMode->m_dwRBitShift = 16;
        pVidMode->m_dwGBitShift = 8;
        pVidMode->m_dwBBitShift = 0;
    }
    ~Surfaces()
    {
        m_pVidMode->m_nSurfaces = m_nSurfacesBefore;
        m_pVidMode->pSurfaces = m_pSurfacesBefore;
        g_pChitin->cVideo.m_nBpp = m_nBppBefore;
        m_pVidMode->m_dwRBitShift = m_shifts[0];
        m_pVidMode->m_dwGBitShift = m_shifts[1];
        m_pVidMode->m_dwBBitShift = m_shifts[2];
        for (IDirectDrawSurface* pSurface : m_surfaces) {
            if (pSurface != NULL) {
                pSurface->Release();
            }
        }
        if (m_pDirectDraw != NULL) {
            m_pDirectDraw->Release();
        }
    }
    Surfaces(const Surfaces&) = delete;
    Surfaces& operator=(const Surfaces&) = delete;

    bool Ok() const
    {
        return m_surfaces[CVIDINF_SURFACE_BACK] != NULL && m_surfaces[CVIDINF_SURFACE_2] != NULL
            && m_surfaces[CVIDINF_SURFACE_3] != NULL;
    }

    // The back buffer's pixels in `rect`, after filling it with `fill` when
    // `clear` is set.
    std::vector<DWORD> Back(const CRect& rect, bool clear = false, DWORD fill = 0)
    {
        std::vector<DWORD> pixels;
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        if (!m_pVidMode->LockSurface(CVIDINF_SURFACE_BACK, &ddsd, rect)) {
            return pixels;
        }
        for (int y = 0; y < rect.Height(); y++) {
            DWORD* row = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch);
            if (clear) {
                std::fill(row, row + rect.Width(), fill);
            }
            pixels.insert(pixels.end(), row, row + rect.Width());
        }
        m_pVidMode->UnLockSurface(CVIDINF_SURFACE_BACK, ddsd.lpSurface);
        return pixels;
    }

private:
    IDirectDrawSurface* Create(int width, int height)
    {
        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
        desc.dwWidth = width;
        desc.dwHeight = height;
        IDirectDrawSurface* pSurface = NULL;
        if (m_pDirectDraw->CreateSurface(&desc, &pSurface, NULL) != DD_OK) {
            return NULL;
        }
        return pSurface;
    }

    CVidMode* m_pVidMode;
    IDirectDraw* m_pDirectDraw = NULL;
    IDirectDrawSurface* m_surfaces[9];
    UINT m_nSurfacesBefore;
    IDirectDrawSurface** m_pSurfacesBefore;
    USHORT m_nBppBefore;
    DWORD m_shifts[3];
};

// The number of pixels whose colour differs between two captures of one
// rectangle (the glyph tag byte aside).
int Changed(const std::vector<DWORD>& before, const std::vector<DWORD>& after)
{
    int changed = 0;
    for (size_t n = 0; n < before.size() && n < after.size(); n++) {
        if ((before[n] & 0xFFFFFF) != (after[n] & 0xFFFFFF)) {
            changed++;
        }
    }
    return changed;
}

}

// CGameSprite::RenderDamageArrow 0x708FC0: with the locator feedback at 4 a
// party member wholly outside the viewport gets the LOCATER.BAM arrow whose
// sequence points at it (0 east, 2 north, 3 north-west, 4 west) drawn at the
// screen's edge; on screen it gets none, nor at level 2 without a damage
// flash. RenderSpriteEffect 0x7093E0 draws the fire particles
// UpdateSpriteEffect placed. Neither draws rand().
TEST_CASE("combat: RenderDamageArrow points at an off-screen party member; RenderSpriteEffect draws the particles", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CInfinity* pInfinity = fight.pArea->GetInfinity();
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();
    REQUIRE(pVidMode != NULL);
    const CRect rScreen = pInfinity->rViewPort;
    REQUIRE(rScreen.Width() > 0);
    REQUIRE(rScreen.Height() > 0);
    Surfaces surfaces(pVidMode, rScreen.right, rScreen.bottom);
    REQUIRE(surfaces.Ok());
    const CPoint pos = pCharacter->GetPos();
    const int savedX = pInfinity->nCurrentX;
    const int savedY = pInfinity->nCurrentY;
    // CInfinity::Render sets the infinity's video mode each frame.
    CVidMode* const savedMode = pInfinity->pVidMode;
    pInfinity->pVidMode = pVidMode;
    // Faded in, as after the game loads (a faded-out screen draws nothing).
    const BYTE savedFade = pVidMode->m_nFade;
    pVidMode->m_nFade = CVidMode::NUM_FADE_FRAMES;
    const DWORD savedLevel = pGame->GetOptions()->m_nLocatorFeedbackLevel;
    srand(13);
    const unsigned int seed = iwd2r_rand_state();

    struct Case {
        int x;
        int y;
        SHORT sequence;
    };
    // The viewport's top left corner and the arrow the party member gets.
    const Case cases[] = {
        { pos.x - rScreen.Width() - 400, pos.y - rScreen.Height() / 2, 0 },
        { pos.x - rScreen.Width() / 2, pos.y + 400, 2 },
        { pos.x + 400, pos.y + 400, 3 },
        { pos.x + 400, pos.y - rScreen.Height() / 2, 4 },
    };
    pGame->GetOptions()->m_nLocatorFeedbackLevel = 4;
    for (const Case& c : cases) {
        pInfinity->nCurrentX = c.x;
        pInfinity->nCurrentY = c.y;
        pGame->m_vcLocator.SequenceSet(7);
        const std::vector<DWORD> before = surfaces.Back(rScreen, true, 0x123456);
        pCharacter->RenderDamageArrow(fight.pArea, pVidMode, CVIDINF_SURFACE_BACK);
        INFO("viewport at " << c.x << "," << c.y << ", party member at " << pos.x << "," << pos.y);
        CHECK(pGame->m_vcLocator.m_nCurrentSequence == c.sequence);
        CHECK(Changed(before, surfaces.Back(rScreen)) > 0);
    }

    // On screen: no arrow.
    pInfinity->nCurrentX = pos.x - rScreen.Width() / 2;
    pInfinity->nCurrentY = pos.y - rScreen.Height() / 2;
    pGame->m_vcLocator.SequenceSet(7);
    std::vector<DWORD> before = surfaces.Back(rScreen, true, 0x123456);
    pCharacter->RenderDamageArrow(fight.pArea, pVidMode, CVIDINF_SURFACE_BACK);
    CHECK(pGame->m_vcLocator.m_nCurrentSequence == 7);
    CHECK(Changed(before, surfaces.Back(rScreen)) == 0);

    // Level 2 shows the arrow only while the damage flashes.
    pGame->GetOptions()->m_nLocatorFeedbackLevel = 2;
    pInfinity->nCurrentX = cases[0].x;
    pInfinity->nCurrentY = cases[0].y;
    pCharacter->m_bBloodFlashOn = FALSE;
    pCharacter->RenderDamageArrow(fight.pArea, pVidMode, CVIDINF_SURFACE_BACK);
    CHECK(pGame->m_vcLocator.m_nCurrentSequence == 7);
    pGame->GetOptions()->m_nLocatorFeedbackLevel = savedLevel;
    // No arrow drew rand().
    CHECK(iwd2r_rand_state() == seed);

    // The fire particles. The splash cell has nothing to play, so the
    // particles show at once. The effect's rectangle comes from the
    // animation's current frame, which a sprite that was never drawn has not
    // loaded (12 pixels wide here): give it a drawn creature's size and put
    // one particle (sequence 0, frame 0) in its middle, where it lies inside
    // whether the animation mirrors or not.
    pInfinity->nCurrentX = pos.x - rScreen.Width() / 2;
    pInfinity->nCurrentY = pos.y - rScreen.Height() / 2;
    pCharacter->StartSpriteEffect(1, 2, 30, FALSE);
    REQUIRE(pCharacter->m_spriteEffectDuration > 0);
    REQUIRE(pCharacter->m_pSpriteEffectArray != NULL);
    pCharacter->m_rSpriteEffectFX = CRect(0, 0, 96, 96);
    pCharacter->m_ptSpriteEffectReference = CPoint(48, 80);
    pCharacter->m_pSpriteEffectArray[0] = 0;
    pCharacter->m_pSpriteEffectArrayPosition[0].x = 0;
    pCharacter->m_pSpriteEffectArrayPosition[0].y = -40;
    const unsigned int beforeRender = iwd2r_rand_state();
    before = surfaces.Back(rScreen, true, 0x123456);
    pCharacter->RenderSpriteEffect(pVidMode, CVIDINF_SURFACE_BACK);
    CHECK(iwd2r_rand_state() == beforeRender);
    CHECK(Changed(before, surfaces.Back(rScreen)) > 0);

    pInfinity->nCurrentX = savedX;
    pInfinity->nCurrentY = savedY;
    pInfinity->pVidMode = savedMode;
    pVidMode->m_nFade = savedFade;
}

namespace {

// The queued messages of one kind, oldest first.
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

// CGameAIBase::ForceSpell 0x461190 (ForceSpell 113, ReallyForceSpell 181)
// and ForceSpellPoint 0x461B80 (114), called with the action counter at
// the points the casting time divides: Magic Missile (SPWI112) has speed 1,
// so 10 ticks, the conjuring sequence before tick 6 and the casting one
// from there. A creature's first tick starts the casting (sub_755A70); its
// voice and its rand() need a cast time of 3, so no tick draws until the
// missile flies (Magic Missile's projectile is not type 304). ReallyForceSpell
// fires at once.
TEST_CASE("combat: ForceSpell and ForceSpellPoint wait for the casting time and then fire", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    const CAIObjectType goblin(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId());

    struct Tick {
        SHORT count;
        SHORT result;
        int conjure;
        int cast;
        int fire;
    };
    // The action counter, the result and the messages the call queues.
    const Tick ticks[] = {
        { 0, CGameAIBase::ACTION_NORMAL, 1, 0, 0 },
        { 5, CGameAIBase::ACTION_NORMAL, 1, 0, 0 },
        { 6, CGameAIBase::ACTION_NORMAL, 0, 1, 0 },
        { 9, CGameAIBase::ACTION_NORMAL, 0, 1, 0 },
        { 10, CGameAIBase::ACTION_DONE, 0, 1, 1 },
    };

    for (int point = 0; point < 2; point++) {
        INFO((point ? "ForceSpellPoint" : "ForceSpell"));
        if (point) {
            CAIAction action(CAIAction::FORCESPELLPOINT, pGoblin->GetPos(), 1, 0);
            action.m_string1 = "SPWI112";
            pCharacter->m_curAction = action;
        } else {
            pCharacter->m_curAction = CAIAction(CAIAction::FORCESPELL, goblin, CString("SPWI112"), 1, 0);
        }
        for (const Tick& tick : ticks) {
            INFO("action count " << tick.count);
            ClearMessages();
            pCharacter->m_nSequence = 0;
            pCharacter->m_actionCount = tick.count;
            srand(Seed(static_cast<unsigned int>(tick.count + 100 * point)));
            const unsigned int before = iwd2r_rand_state();
            const SHORT result = point ? pCharacter->ForceSpellPoint() : pCharacter->ForceSpell(pGoblin);
            CHECK(result == tick.result);
            // Magic Missile's missile changes its speed by rand() % 20 - 10
            // when it is fired at the goblin (CProjectile84E030::Fire); at a
            // point it has no target to fire at and fires nothing.
            CHECK(Draws(before, iwd2r_rand_state()) == (tick.fire != 0 && point == 0 ? 1 : 0));
            int conjure = 0;
            int cast = 0;
            for (CMessageSetSequence* message : Queued<CMessageSetSequence>()) {
                conjure += message->m_sequence == CGameSprite::SEQ_CONJURE ? 1 : 0;
                cast += message->m_sequence == CGameSprite::SEQ_CAST ? 1 : 0;
            }
            CHECK(conjure == tick.conjure);
            CHECK(cast == tick.cast);
            const std::vector<CMessageFireProjectile*> fire = Queued<CMessageFireProjectile>();
            CHECK(static_cast<int>(fire.size()) == tick.fire);
            if (!fire.empty()) {
                CHECK(fire[0]->m_projectileTargetId == (point ? -1 : pGoblin->GetId()));
                CHECK(fire[0]->m_projectileTarget == pGoblin->GetPos());
            }
        }
    }

    // ReallyForceSpell: no casting time.
    ClearMessages();
    pCharacter->m_curAction = CAIAction(CAIAction::REALLYFORCESPELL, goblin, CString("SPWI112"), 1, 0);
    pCharacter->m_actionCount = 0;
    CHECK(pCharacter->ForceSpell(pGoblin) == CGameAIBase::ACTION_DONE);
    CHECK(Queued<CMessageSetSequence>().empty());
    CHECK(Queued<CMessageFireProjectile>().size() == 1);

    // No target: an error.
    pCharacter->m_curAction = CAIAction(CAIAction::FORCESPELL, goblin, CString("SPWI112"), 1, 0);
    CHECK(pCharacter->ForceSpell(NULL) == CGameAIBase::ACTION_ERROR);

    // No spell (no resref, no spell id and none kept at +0x588): an error,
    // and nothing is queued.
    ClearMessages();
    const int savedSpell = pCharacter->field_588;
    pCharacter->field_588 = 0;
    pCharacter->m_curAction = CAIAction(CAIAction::FORCESPELL, goblin, CString(""), 0, 0);
    pCharacter->m_actionCount = 0;
    CHECK(pCharacter->ForceSpell(pGoblin) == CGameAIBase::ACTION_ERROR);
    CHECK(Queued<CMessage>().empty());
    pCharacter->field_588 = savedSpell;
    ClearMessages();
}

// The ForceSpell action through ExecuteAction: the party member's Magic
// Missile hurts the goblin.
TEST_CASE("combat: a party member's ForceSpell action hurts the goblin", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    const SHORT goblinStart = pGoblin->GetBaseStats()->m_hitPoints;

    pCharacter->ClearActions(FALSE);
    pCharacter->AddAction(CAIAction(CAIAction::FORCESPELL, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId()),
        CString("SPWI112"), 1, 0));
    pGoblin->ClearActions(FALSE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    while (frames < 600 && pGoblin->GetBaseStats()->m_hitPoints == goblinStart) {
        Tick(fight.pArea, { pCharacter, pGoblin });
        frames++;
    }
    INFO("frames " << frames << ", goblin hit points " << goblinStart << " -> " << pGoblin->GetBaseStats()->m_hitPoints);
    CHECK(frames > 10);
    CHECK(pGoblin->GetBaseStats()->m_hitPoints < goblinStart);
    CHECK(pGoblin->GetBaseStats()->m_hitPoints >= goblinStart - 5);
}

// Swing's missile branch (0x73A679-0x73AC06): a goblin archer (10GOBAR,
// a bow and arrows) given Attack at the party member six squares away shoots
// from where it stands: its rounds go by, it never closes to melee, and the
// arrows that hit carry Damage's effect to the party member. The arrow really
// flies (DecodeProjectile type 6 -> CProjectileArrow, ctor 0x5324A0), so the
// effect is applied on the arrival frame, a few frames after the shot.
TEST_CASE("combat: a goblin archer shoots arrows at the party member", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    const CPoint pos = pCharacter->GetPos();
    const CPoint archerPos(pos.x - 6 * CPathSearch::GRID_SQUARE_SIZEX, pos.y);
    CGameSprite* pArcher = AddCreature(fight.pArea, "10GOBAR", archerPos);
    if (pArcher == NULL) {
        FAIL("10GOBAR.CRE not found");
    }
    fight.pArea->SortLists();
    ClearMessages();
    fight.pGoblin->ClearActions(FALSE);
    pCharacter->ClearActions(FALSE);
    const SHORT characterStart = pCharacter->GetBaseStats()->m_hitPoints;

    CItem* pWeapon = pArcher->GetEquipment()->m_items[pArcher->GetEquipment()->m_selectedWeapon];
    REQUIRE(pWeapon != NULL);
    pWeapon->Demand();
    const ITEM_ABILITY* pAbility = pWeapon->GetAbility(pArcher->GetEquipment()->m_selectedWeaponAbility);
    const int type = pAbility != NULL ? (pAbility->type & 0xFF) : -1;
    pWeapon->Release();
    INFO("archer's weapon " << pWeapon->GetResRef().GetResRefStr() << ", ability type " << type);
    REQUIRE(type == 2);

    // A fixed rand() stream to start from; with the new game seeded too
    // (SetUpFight) every run draws the same rolls.
    srand(21);
    pArcher->ClearActions(FALSE);
    pArcher->AddAction(CAIAction(CAIAction::ATTACK, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId()), 0, 0, 0));

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    int rounds = 0;
    int nearest = INT_MAX;
    SHORT hp = characterStart;
    bool hpRose = false;
    while (frames < 1500 && pCharacter->GetBaseStats()->m_hitPoints > 0) {
        Tick(fight.pArea, { pCharacter, pArcher });
        if (pArcher->m_attackFrame == 0) {
            rounds++;
        }
        const CPoint& at = pArcher->GetPos();
        nearest = std::min(nearest, std::abs(at.x - pos.x) / CPathSearch::GRID_SQUARE_SIZEX);
        if (pCharacter->GetBaseStats()->m_hitPoints > hp) {
            hpRose = true;
        }
        hp = pCharacter->GetBaseStats()->m_hitPoints;
        frames++;
    }
    INFO("frames " << frames << ", rounds " << rounds << ", nearest " << nearest << " squares, character hit points "
                   << characterStart << " -> " << pCharacter->GetBaseStats()->m_hitPoints);
    CHECK(rounds == 16);
    CHECK(nearest >= 5);
    CHECK_FALSE(hpRose);
    CHECK(characterStart == 9);
    // The arrow's flight moves the damage effect's application draws from the
    // fire frame to the arrival frame, which offsets the fixed rand stream
    // for every later roll: with srand(21) three of the sixteen shots hit for
    // 1 damage each, 9 -> 6. (The old expectation of 7 was calibrated against
    // the pre-arrow fallback CProjectile84D8B8, which applies its effects the
    // moment it is fired — two hits, no flight draws in between.)
    CHECK(pCharacter->GetBaseStats()->m_hitPoints == 6);
}

namespace {

// The text of a string reference, as the TLK has it.
std::string Tlk(STRREF strRef)
{
    STR_RES strRes;
    g_pBaldurChitin->m_cTlkTable.Fetch(strRef, strRes);
    return std::string(static_cast<LPCSTR>(strRes.szText));
}

}

// CItem::sub_4EA580 0x4EA580 and its usability list 0x4EA750: the item's
// description, an empty line, the heading ("usable by" 31531 when anything
// makes the item usable, else "not usable by" 31530) in RGB(200, 200, 0), and
// the list. 00AX1H83 has only bit 0x4000 (strref 31532) set: it is usable by
// everyone else. 00BARD01 (0x7FD and every kit bit) is usable by the class
// of bit 0x2 (1083) only, so the "not usable by" list shows. An item without
// restrictions (00SWDL01) gets the description only.
TEST_CASE("combat: CItem's information text lists who can use the item", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CUIControlTextDisplay* pDisplay = g_pBaldurChitin->m_pEngineWorld->m_pActiveDialogDisplay;
    REQUIRE(pDisplay != NULL);
    REQUIRE(pDisplay->m_plstStrings != NULL);
    REQUIRE(g_pBaldurChitin->pActiveEngine == g_pBaldurChitin->m_pEngineWorld);

    // The strings the call adds: (text, colour).
    auto added = [&](const char* resRef, std::vector<std::pair<std::string, COLORREF>>& lines) {
        CItem item(CResRef(resRef), 0, 0, 0, 0, 1);
        const INT_PTR before = pDisplay->m_plstStrings->GetCount();
        const unsigned int state = iwd2r_rand_state();
        item.sub_4EA580(pDisplay, RGB(200, 200, 0));
        CHECK(iwd2r_rand_state() == state);
        INT_PTR n = 0;
        POSITION pos = pDisplay->m_plstStrings->GetHeadPosition();
        while (pos != NULL) {
            CDisplayString* pString = pDisplay->m_plstStrings->GetNext(pos);
            if (n++ >= before) {
                lines.emplace_back(std::string(static_cast<LPCSTR>(pString->GetString())), pString->GetTextColor());
            }
        }
        std::string all;
        for (const auto& line : lines) {
            all += line.first + "\n";
        }
        return all;
    };
    auto headingColour = [&](const std::vector<std::pair<std::string, COLORREF>>& lines, const std::string& heading) {
        for (const auto& line : lines) {
            if (line.first.find(heading) != std::string::npos) {
                return line.second;
            }
        }
        return static_cast<COLORREF>(0xFFFFFFFF);
    };

    std::vector<std::pair<std::string, COLORREF>> lines;
    std::string all = added("00AX1H83", lines);
    INFO(all);
    // The description wraps over several lines: its first words.
    CHECK(all.find(Tlk(11108).substr(0, 24)) != std::string::npos);
    CHECK(headingColour(lines, Tlk(31531)) == RGB(200, 200, 0));
    CHECK(all.find(Tlk(31530)) == std::string::npos);
    for (STRREF strRef : { 31534, 31536, 34, 1083, 1079, 1080, 16799, 33, 1078, 1077, 1082, 32, 9987 }) {
        CHECK(all.find("  " + Tlk(strRef)) != std::string::npos);
    }
    CHECK(all.find("  " + Tlk(31532)) == std::string::npos);

    lines.clear();
    all = added("00BARD01", lines);
    INFO(all);
    CHECK(headingColour(lines, Tlk(31530)) == RGB(200, 200, 0));
    for (STRREF strRef : { 34, 1079, 1080, 16799, 33, 1078, 1077, 1082, 32, 9987 }) {
        CHECK(all.find("  " + Tlk(strRef)) != std::string::npos);
    }
    CHECK(all.find("  " + Tlk(1083)) == std::string::npos);

    lines.clear();
    all = added("00SWDL01", lines);
    INFO(all);
    CHECK(all.find(Tlk(32681).substr(0, 24)) != std::string::npos);
    CHECK(all.find(Tlk(31530)) == std::string::npos);
    CHECK(all.find(Tlk(31531)) == std::string::npos);
}

// CGameSprite::CheckIfUsedUp 0x75D450 after a goblin archer's shot. With
// arrows left it changes nothing and answers FALSE. With none left
// (00AROW01: one charge per arrow, usage flag 1) the arrows vanish: the fist
// is selected and the weapon set chosen again (sub_726810), UNUSABLE is
// triggered with the arrows' name, and it answers TRUE (Swing then ends the
// attack). An ability the item lacks triggers UNUSABLE too and keeps the
// item.
TEST_CASE("combat: CheckIfUsedUp keeps arrows that are left and drops spent ones", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    const CPoint pos = fight.pCharacter->GetPos();
    CGameSprite* pArcher = AddCreature(fight.pArea, "10GOBAR", CPoint(pos.x - 6 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    if (pArcher == NULL) {
        FAIL("10GOBAR.CRE not found");
    }
    CGameSpriteEquipment* pEquipment = pArcher->GetEquipment();
    const SHORT nSlot = pEquipment->m_selectedWeapon;
    const SHORT nAbility = static_cast<SHORT>(pEquipment->m_selectedWeaponAbility);
    CItem* pArrows = pEquipment->m_items[nSlot];
    REQUIRE(pArrows != NULL);
    REQUIRE(pArrows->GetResRef() == CResRef("00AROW01"));

    auto unusableTriggers = [](const char* name) {
        int count = 0;
        for (CMessageSetTrigger* message : Queued<CMessageSetTrigger>()) {
            if (message->m_trigger.m_triggerID == CAITRIGGER_UNUSABLE
                && strcmp(message->m_trigger.m_string1, name) == 0) {
                count++;
            }
        }
        return count;
    };

    srand(5);
    const unsigned int state = iwd2r_rand_state();

    // Arrows left.
    ClearMessages();
    pArrows->SetUsageCount(0, 5);
    CHECK_FALSE(pArcher->CheckIfUsedUp(nSlot, nAbility));
    CHECK(pEquipment->m_items[nSlot] == pArrows);
    CHECK(unusableTriggers("00AROW01") == 0);

    // An ability the arrows lack.
    ClearMessages();
    CHECK(pArcher->CheckIfUsedUp(nSlot, 5));
    CHECK(pEquipment->m_items[nSlot] == pArrows);
    CHECK(unusableTriggers("00AROW01") == 1);

    // None left.
    ClearMessages();
    pArrows->SetUsageCount(0, 0);
    CHECK(pArcher->CheckIfUsedUp(nSlot, nAbility));
    CHECK(pEquipment->m_items[nSlot] == NULL);
    CHECK(pEquipment->m_selectedWeapon != nSlot);
    CHECK(unusableTriggers("00AROW01") == 1);
    // CheckIfUsedUp draws nothing itself; the unequips it causes recalculate
    // the effect lists while the world timer is stopped (CItem::Unequip):
    // two ProcessEffectList runs of five draws (four save d20s and a
    // rand() % 100).
    CHECK(Draws(state, iwd2r_rand_state()) == 10);
    ClearMessages();
}

// The battle cry of the Attack action 0x737EF0. On the action's first tick
// a party member draws rand() % 6 and shouts (CMessagePlaySound 5) on a 0;
// any other creature shouts without a draw. Swing then draws the round's
// speed (one UtilRandInt).
TEST_CASE("combat: Attack draws the battle cry of a party member only", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    REQUIRE(pGame->GetCharacterPortraitNum(pCharacter->GetId()) != -1);
    REQUIRE(pGame->GetCharacterPortraitNum(pGoblin->GetId()) == -1);

    const auto shouts = [] {
        int count = 0;
        for (CMessagePlaySound* message : Queued<CMessagePlaySound>()) {
            count += message->m_soundId == 5 ? 1 : 0;
        }
        return count;
    };

    int shouted = 0;
    int silent = 0;
    for (unsigned int seed = 1; seed <= 30; seed++) {
        INFO("seed " << seed);
        ClearMessages();
        pCharacter->m_curAction = CAIAction(CAIAction::ATTACK, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId()), 0, 0, 0);
        pCharacter->m_actionCount = 0;
        pCharacter->m_attackFrame = -1;
        pCharacter->m_equipment.m_selectedWeapon = 10;
        pCharacter->m_equipment.m_selectedWeaponAbility = 0;
        srand(Seed(seed));
        const unsigned int before = iwd2r_rand_state();
        const bool bShout = NextRands(before, 1)[0] % 6 == 0;
        pCharacter->Attack(pGoblin);
        // Swing started the round.
        CHECK(pCharacter->m_attackFrame == 0);
        CHECK(Draws(before, iwd2r_rand_state()) == 2);
        CHECK(shouts() == (bShout ? 1 : 0));
        shouted += bShout ? 1 : 0;
        silent += bShout ? 0 : 1;
    }
    CHECK(shouted > 0);
    CHECK(silent > 0);

    // The goblin shouts without a draw.
    ClearMessages();
    pGoblin->m_curAction = CAIAction(CAIAction::ATTACK, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId()), 0, 0, 0);
    pGoblin->m_actionCount = 0;
    pGoblin->m_attackFrame = -1;
    srand(Seed(31));
    const unsigned int before = iwd2r_rand_state();
    pGoblin->Attack(pCharacter);
    CHECK(pGoblin->m_attackFrame == 0);
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CHECK(shouts() == 1);
    ClearMessages();
}

// The rand() draws of the Spell action 0x740270. A goblin casts Magic
// Missile (SPWI112, SpellNoDec) at the party member. On the first tick: the
// concentration check an enemy within GetNearest's range 24 (0x741B6B; in
// pixels, so the caster stands one square away) calls for (UtilRandInt(20,
// luck)), then the spellcraft roll (rand() % 20) of a party member who sees
// a creature cast. When the casting time is over: the spell failure roll
// (rand() % 100), then, after a hit while casting (+0x5582), a concentration
// check. A projectile of type 304 (SPPR613's missile 305) adds rand() %
// 1000000 for the fire message's +0x20 (0x74271F).
TEST_CASE("combat: Spell draws the concentration and spellcraft rolls and then the failure and type-304 rolls", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    const CPoint pos = pCharacter->GetPos();
    CGameSprite* pGoblin = AddCreature(fight.pArea, "10GOB", CPoint(pos.x + CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    REQUIRE(pGoblin != NULL);
    fight.pArea->SortLists();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    REQUIRE(pGame->GetCharacterPortraitNum(pGoblin->GetId()) == -1);
    const CAIObjectType character(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId());
    const CAIObjectType goblin(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId());

    // A first-level wizard sure to keep its concentration; the party member
    // knows spellcraft.
    pGoblin->GetDerivedStats()->m_wizardLevel = 1;
    pGoblin->GetDerivedStats()->m_nSkills[3] = 60;
    pCharacter->GetDerivedStats()->m_nSkills[13] = 10;
    CAIAction missile(CAIAction::SPELLNODEC, character, CString("SPWI112"), 0, CAIOBJECTTYPE_C_WIZARD);
    missile.m_specificID3 = 1;
    pGoblin->m_curAction = missile;
    pGoblin->m_actionCount = 0;
    pGoblin->m_nDirection = pGoblin->GetDirection(pCharacter->GetPos());
    pGoblin->m_bStartedCasting = FALSE;
    pGoblin->m_castCounter = -1;
    pGoblin->field_5582 = 0;
    pGoblin->field_9D14 = 0;
    pGoblin->field_9D15 = 0;

    ClearMessages();
    srand(31);
    unsigned int before = iwd2r_rand_state();
    CHECK(pGoblin->Spell(pCharacter) == CGameAIBase::ACTION_NORMAL);
    CHECK(pGoblin->m_bStartedCasting);
    CHECK(pGoblin->field_9D14 == 1);
    CHECK(pGoblin->field_9D15 == 0);
    CHECK(Draws(before, iwd2r_rand_state()) == 2);

    // The casting time is over after a hit for 3.
    ClearMessages();
    pGoblin->m_actionCount = 10;
    pGoblin->m_castCounter = 100;
    pGoblin->field_5582 = 3;
    pGoblin->field_9D14 = 0;
    srand(32);
    before = iwd2r_rand_state();
    CHECK(pGoblin->Spell(pCharacter) == CGameAIBase::ACTION_DONE);
    CHECK(pGoblin->field_9D14 == 1);
    // The two rolls, then the missile's speed (CProjectile84E030::Fire).
    CHECK(Draws(before, iwd2r_rand_state()) == 3);

    // The party member, a cleric, finishes SPPR613 at the goblin: the
    // failure roll, then the type-304 draw.
    ClearMessages();
    pCharacter->GetDerivedStats()->m_clericLevel = 20;
    CAIAction blades(CAIAction::SPELLNODEC, goblin, CString("SPPR613"), 0, CAIOBJECTTYPE_C_CLERIC);
    blades.m_specificID3 = 6;
    pCharacter->m_curAction = blades;
    pCharacter->m_actionCount = 10;
    pCharacter->m_nDirection = pCharacter->GetDirection(pGoblin->GetPos());
    pCharacter->m_bStartedCasting = TRUE;
    pCharacter->m_castCounter = 1000;
    pCharacter->field_5582 = 0;
    pCharacter->field_9D14 = 1;
    pCharacter->field_9D15 = 0;
    delete pCharacter->m_curSpell;
    pCharacter->m_curSpell = new CSpell(CResRef("SPPR613"));
    srand(33);
    before = iwd2r_rand_state();
    const std::vector<int> values = NextRands(before, 2);
    CHECK(pCharacter->Spell(pGoblin) == CGameAIBase::ACTION_DONE);
    CHECK(Draws(before, iwd2r_rand_state()) == 2);
    const std::vector<CMessageFireProjectile*> fire = Queued<CMessageFireProjectile>();
    REQUIRE(fire.size() == 1);
    CHECK(fire[0]->m_projectileType == 304);
    CHECK(fire[0]->field_20 == values[1] % 1000000);
    ClearMessages();
}

// sub_756930 (0x756930), at the first frame of an attack sequence: the
// sound the swing plays (+0x7408) and its frame (+0x740C).
TEST_CASE("combat: a swing picks its weapon's sound and the next one when it would repeat", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pGoblin = fight.pGoblin;
    pGoblin->field_70FB = FALSE;
    pGoblin->GetDerivedStats()->m_generalState &= ~STATE_SILENCED;
    const SHORT savedSequence = pGoblin->m_animation.m_nSequence;
    pGoblin->m_animation.m_nSequence = CGAMESPRITE_SEQ_ATTACK_SLASH;

    // A long sword (type 20): "Sw_Swd" and 01 to 05, rand() % 5.
    CItem sword(CResRef("00CWSWDB"), 0, 0, 0, 0, 0);
    REQUIRE(sword.GetItemType() == 20);
    srand(5);
    const int n = NextRands(iwd2r_rand_state(), 1)[0] % 5;
    pGoblin->field_7408 = "";
    pGoblin->field_740C = 7;
    unsigned int before = iwd2r_rand_state();
    pGoblin->sub_756930(&sword, NULL);
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CString expected;
    expected.Format("Sw_Swd0%d", n + 1);
    CHECK(pGoblin->field_7408 == expected);
    CHECK(pGoblin->field_740C == 0);

    // The same draw again: the next sound, after the fifth the first.
    srand(5);
    pGoblin->sub_756930(&sword, NULL);
    expected.Format("Sw_Swd0%d", n == 4 ? 1 : n + 2);
    CHECK(pGoblin->field_7408 == expected);

    // Silenced: no sound and no draw.
    pGoblin->GetDerivedStats()->m_generalState |= STATE_SILENCED;
    before = iwd2r_rand_state();
    pGoblin->sub_756930(&sword, NULL);
    CHECK(Draws(before, iwd2r_rand_state()) == 0);
    CHECK(pGoblin->field_7408 == expected);
    pGoblin->GetDerivedStats()->m_generalState &= ~STATE_SILENCED;

    // An amulet has no swing sound: outside the three attack sequences the
    // creature's sound set has none either and "Sw_" is kept as it is.
    CItem amulet(CResRef("00AMUL01"), 0, 0, 0, 0, 0);
    const WORD amuletType = amulet.GetItemType();
    INFO("amulet type " << amuletType);
    REQUIRE((amuletType < 15 || (amuletType > 30 && amuletType != 44 && amuletType != 57 && amuletType != 69)));
    REQUIRE(amuletType != 0);
    pGoblin->m_animation.m_nSequence = CGAMESPRITE_SEQ_ATTACK;
    before = iwd2r_rand_state();
    pGoblin->sub_756930(&amulet, NULL);
    CHECK(Draws(before, iwd2r_rand_state()) == 0);
    CHECK(pGoblin->field_7408 == CString("Sw_"));

    // The creature's own sounds for the sequence (its INI's att4 for a
    // sequence other than the three): one at random with its frame, the
    // next one when it is the one played last.
    pGoblin->field_70FB = TRUE;
    CGameSpriteSoundEntry first;
    first.field_0 = "GOBSW1";
    first.field_4 = 3;
    CGameSpriteSoundEntry second;
    second.field_0 = "GOBSW2";
    second.field_4 = 5;
    pGoblin->field_7344.AddTail(first);
    pGoblin->field_7344.AddTail(second);
    srand(9);
    const int m = NextRands(iwd2r_rand_state(), 1)[0] % 2;
    before = iwd2r_rand_state();
    pGoblin->sub_756930(&sword, NULL);
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CHECK(pGoblin->field_7408 == CString(m == 0 ? "GOBSW1" : "GOBSW2"));
    CHECK(pGoblin->field_740C == (m == 0 ? 3 : 5));
    srand(9);
    pGoblin->sub_756930(&sword, NULL);
    CHECK(pGoblin->field_7408 == CString(m == 0 ? "GOBSW2" : "GOBSW1"));
    CHECK(pGoblin->field_740C == (m == 0 ? 5 : 3));

    pGoblin->field_7344.RemoveAll();
    pGoblin->field_70FB = FALSE;
    pGoblin->m_animation.m_nSequence = savedSequence;
}

// CGameArea 0x46DDC0, the area's n-th nearest search from a point (Feat
// Cleave's next target, CAIObjectType's n-th nearest objects).
TEST_CASE("combat: the area finds the n-th nearest enemy of a point", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    const CPoint pos = pCharacter->GetPos();
    CGameSprite* pFarGoblin = AddCreature(fight.pArea, "10GOB", CPoint(pos.x - 5 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    REQUIRE(pFarGoblin != NULL);
    fight.pArea->SortLists();
    pCharacter->GetDerivedStats()->m_generalState &= ~STATE_SUMMONED_CREATURE;
    pGoblin->GetDerivedStats()->m_generalState &= ~STATE_SUMMONED_CREATURE;
    pFarGoblin->GetDerivedStats()->m_generalState &= ~STATE_SUMMONED_CREATURE;

    const CAIObjectType enemy(CAIOBJECTTYPE_EA_ENEMY);
    const BYTE* terrain = pCharacter->GetTerrainTable();
    CGameArea* pArea = fight.pArea;

    // The nearest, the second nearest; the third does not exist and the
    // farthest one found stands in for it.
    CHECK(pArea->sub_46DDC0(pos.x, pos.y, enemy, 400, terrain, FALSE, FALSE, 0, FALSE) == pGoblin->GetId());
    CHECK(pArea->sub_46DDC0(pos.x, pos.y, enemy, 400, terrain, FALSE, FALSE, 1, FALSE) == pFarGoblin->GetId());
    CHECK(pArea->sub_46DDC0(pos.x, pos.y, enemy, 400, terrain, FALSE, FALSE, 2, FALSE) == pFarGoblin->GetId());

    // Out of range: only the near goblin is within 3 squares.
    CHECK(pArea->sub_46DDC0(pos.x, pos.y, enemy, 3 * CPathSearch::GRID_SQUARE_SIZEX, terrain, FALSE, FALSE, 1, FALSE) == pGoblin->GetId());

    // Summoned creatures only: none.
    CHECK(pArea->sub_46DDC0(pos.x, pos.y, enemy, 400, terrain, FALSE, FALSE, 0, TRUE) == CGameObjectArray::INVALID_INDEX);
    pFarGoblin->GetDerivedStats()->m_generalState |= STATE_SUMMONED_CREATURE;
    CHECK(pArea->sub_46DDC0(pos.x, pos.y, enemy, 400, terrain, FALSE, FALSE, 0, TRUE) == pFarGoblin->GetId());
    pFarGoblin->GetDerivedStats()->m_generalState &= ~STATE_SUMMONED_CREATURE;

    // A point off the area: none.
    CHECK(pArea->sub_46DDC0(-1, pos.y, enemy, 400, terrain, FALSE, FALSE, 0, FALSE) == CGameObjectArray::INVALID_INDEX);

    // 0x46BAD0, the same from the party member on the front list, outward
    // from its place in the list.
    CHECK(pArea->sub_46BAD0(pCharacter->GetId(), enemy, 400, terrain, FALSE, FALSE, FALSE, 0, FALSE) == pGoblin->GetId());
    CHECK(pArea->sub_46BAD0(pCharacter->GetId(), enemy, 400, terrain, FALSE, FALSE, FALSE, 1, FALSE) == pFarGoblin->GetId());
    CHECK(pArea->sub_46BAD0(pCharacter->GetId(), enemy, 400, terrain, FALSE, FALSE, FALSE, 2, FALSE) == pFarGoblin->GetId());
    // A dead goblin is left out.
    const DWORD state = pGoblin->GetDerivedStats()->m_generalState;
    pGoblin->GetDerivedStats()->m_generalState = state | STATE_DEAD;
    CHECK(pArea->sub_46BAD0(pCharacter->GetId(), enemy, 400, terrain, FALSE, FALSE, FALSE, 0, FALSE) == pFarGoblin->GetId());
    pGoblin->GetDerivedStats()->m_generalState = state;

    // CAIObjectType::Decode: SecondNearestEnemyOf (29) of the party member
    // is the far goblin.
    CAIObjectType second(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    second.m_SpecialCase[0] = 29;
    second.Decode(pCharacter);
    INFO("decoded instance " << second.m_nInstance);
    CHECK(second.m_nInstance == pFarGoblin->GetId());
}

// CBaldurMessage::BroadcastUpdateCharacterSlot (0x42DCF0), which SaveGame
// calls: each party member's effect list and visual effects are redone at
// its next AI update.
TEST_CASE("combat: a save marks the party members for an effect list update", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    REQUIRE(pGame->m_characters[0] == pCharacter->GetId());

    const BOOLEAN bConnection = g_pChitin->cNetwork.m_bConnectionEstablished;

    // No open session: nothing.
    g_pChitin->cNetwork.m_bConnectionEstablished = FALSE;
    pCharacter->field_562C = FALSE;
    pCharacter->m_bForceVisualEffects = FALSE;
    g_pBaldurChitin->GetBaldurMessage()->BroadcastUpdateCharacterSlot(TRUE, 0, FALSE);
    CHECK_FALSE(pCharacter->field_562C);
    CHECK_FALSE(pCharacter->m_bForceVisualEffects);

    // The single player game's open session: every slot's character, and
    // only the party's.
    g_pChitin->cNetwork.m_bConnectionEstablished = TRUE;
    pGoblin->field_562C = FALSE;
    const LONG progress = g_pChitin->cProgressBar.m_nActionProgress;
    g_pBaldurChitin->GetBaldurMessage()->BroadcastUpdateCharacterSlot(TRUE, 0, FALSE);
    CHECK(pCharacter->field_562C);
    CHECK(pCharacter->m_bForceVisualEffects);
    CHECK_FALSE(pGoblin->field_562C);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == progress);

    // With the progress bar: 500000 units a slot, six slots.
    g_pBaldurChitin->GetBaldurMessage()->BroadcastUpdateCharacterSlot(TRUE, 0, TRUE);
    CHECK(g_pChitin->cProgressBar.m_nActionProgress == progress + 6 * 500000);
    CHECK_FALSE(g_pChitin->cProgressBar.m_bWaiting);
    g_pChitin->cProgressBar.m_nActionProgress = progress;

    g_pChitin->cNetwork.m_bConnectionEstablished = bConnection;
}

// GiveItem (action 15, CGameSprite 0x757540) and CMessageAddItem's Run
// (0x4F9150): the goblin's weapon leaves it and a copy lands in the party
// member's first empty inventory slot.
TEST_CASE("combat: GiveItem moves a goblin's weapon into the party member's inventory", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    // The goblin's selected weapon.
    CGameSpriteEquipment* pEquipment = pGoblin->GetEquipment();
    SHORT nSlot = pEquipment->m_selectedWeapon;
    CItem* pWeapon = pEquipment->m_items[nSlot];
    REQUIRE(pWeapon != NULL);
    const CString sWeapon = pWeapon->GetResRef().GetResRefStr();
    INFO("weapon " << static_cast<const char*>(sWeapon) << " in slot " << nSlot);

    // No target: an error.
    CAIAction give(15, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId()), 0, 0, 0);
    give.m_string1 = sWeapon;
    pGoblin->m_curAction = give;
    CHECK(pGoblin->GiveItem(NULL) == CGameAIBase::ACTION_ERROR);

    // An item the goblin does not carry (nor in a bag): an error.
    CAIAction missing = give;
    missing.m_string1 = "NOSUCH";
    pGoblin->m_curAction = missing;
    CHECK(pGoblin->GiveItem(pCharacter) == CGameAIBase::ACTION_ERROR);

    // The weapon: done, the goblin's slot is empty, and the message gives
    // the party member a copy in its first empty inventory slot.
    INT nFirstEmpty = -1;
    for (INT n = 18; n < 42; n++) {
        if (pCharacter->GetEquipment()->m_items[n] == NULL) {
            nFirstEmpty = n;
            break;
        }
    }
    REQUIRE(nFirstEmpty != -1);
    ClearMessages();
    pGoblin->m_curAction = give;
    CHECK(pGoblin->GiveItem(pCharacter) == CGameAIBase::ACTION_DONE);
    CHECK(pEquipment->m_items[nSlot] == NULL);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CItem* pGiven = pCharacter->GetEquipment()->m_items[nFirstEmpty];
    REQUIRE(pGiven != NULL);
    CHECK(pGiven->GetResRef() == CResRef(sWeapon));
    ClearMessages();
}

// GiveItemCreate (action 140, 0x44EEB9-0x44EF37, and 0x464EB0), which
// Reig's dialog runs for the short sword: a new item, with the action's
// three numbers as its use counts, lands in the party member's first empty
// inventory slot (CMessageAddItem); the caller keeps nothing.
TEST_CASE("combat: GiveItemCreate puts a new short sword in the party member's inventory", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    CAIObjectType actee(pCharacter->GetAIType());
    actee.m_nInstance = pCharacter->GetId();
    CAIAction create(140, actee, 0, 0, 0);
    create.m_string1 = "00Swds01";

    // No target, or an item the game does not have: an error.
    pGoblin->m_curAction = create;
    CHECK(pGoblin->GiveItemCreate(NULL) == CGameAIBase::ACTION_ERROR);
    CAIAction missing = create;
    missing.m_string1 = "NOSUCH";
    pGoblin->m_curAction = missing;
    CHECK(pGoblin->GiveItemCreate(pCharacter) == CGameAIBase::ACTION_ERROR);

    INT nFirstEmpty = -1;
    for (INT n = 18; n < 42; n++) {
        if (pCharacter->GetEquipment()->m_items[n] == NULL) {
            nFirstEmpty = n;
            break;
        }
    }
    REQUIRE(nFirstEmpty != -1);
    INT nGoblinItems = 0;
    for (INT n = 0; n < 51; n++) {
        if (pGoblin->GetEquipment()->m_items[n] != NULL) {
            nGoblinItems++;
        }
    }

    // Through ExecuteAction, the action's actee: done, and the message
    // gives the party member the sword.
    ClearMessages();
    pGoblin->m_curAction = create;
    CHECK(pGoblin->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CItem* pGiven = pCharacter->GetEquipment()->m_items[nFirstEmpty];
    REQUIRE(pGiven != NULL);
    CHECK(pGiven->GetResRef() == CResRef("00SWDS01"));
    INT nGoblinItemsAfter = 0;
    for (INT n = 0; n < 51; n++) {
        if (pGoblin->GetEquipment()->m_items[n] != NULL) {
            nGoblinItemsAfter++;
        }
    }
    CHECK(nGoblinItemsAfter == nGoblinItems);
    ClearMessages();
}

// SetBestWeapon (action 255, 0x72AB84-0x72ACB5): the target within the
// action's range in search squares asks for a melee weapon, beyond it for a
// ranged one.
TEST_CASE("combat: SetBestWeapon picks the goblin's melee weapon within its range", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    CGameSpriteEquipment* pEquipment = pGoblin->GetEquipment();
    const BYTE nWeapon = pEquipment->m_selectedWeapon;
    REQUIRE(nWeapon >= CGameSpriteEquipment::SLOT_WEAPON);
    const CAIObjectType character(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId());

    // The party member is two search squares away: a range of 5 holds it.
    pGoblin->ClearActions(FALSE);
    pGoblin->SelectWeaponAbility(CGameSpriteEquipment::SLOT_FIST, 0, 0, 1);
    REQUIRE(pEquipment->m_selectedWeapon == CGameSpriteEquipment::SLOT_FIST);
    pGoblin->m_curAction = CAIAction(255, character, 5, 0, 0);
    CHECK(pGoblin->ExecuteAction() == CGameAIBase::ACTION_DONE);
    // The fists are a melee ability: they stay.
    CHECK(pEquipment->m_selectedWeapon == CGameSpriteEquipment::SLOT_FIST);

    // Beyond a range of 0 the goblin wants a ranged weapon; it has none, so
    // EquipRanged falls back to its most damaging melee weapon.
    pGoblin->m_curAction = CAIAction(255, character, 0, 0, 0);
    CHECK(pGoblin->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pEquipment->m_selectedWeapon == nWeapon);

    // No target: an error.
    pGoblin->m_curAction = CAIAction(255, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, 0x7FFF0000), 5, 0, 0);
    CHECK(pGoblin->ExecuteAction() == CGameAIBase::ACTION_ERROR);
    ClearMessages();
}

// SetBestWeapon's other two branches, with AR1000's goblin archer (10GOBAR:
// a shortbow, arrows, and 001D3P, a melee weapon in its second set), which
// starts with the arrows (the first quiver slot, a ranged ability) selected:
// beyond the range a ranged ability stays; within it the archer takes its
// most damaging melee weapon.
TEST_CASE("combat: SetBestWeapon keeps an archer's arrows beyond its range and takes a melee weapon within it", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    const CPoint pos = pCharacter->GetPos();
    CGameSprite* pArcher = AddCreature(fight.pArea, "10GOBAR", CPoint(pos.x + 2 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    REQUIRE(pArcher != NULL);
    CGameSpriteEquipment* pEquipment = pArcher->GetEquipment();
    REQUIRE(pEquipment->m_selectedWeapon == 11);
    const CAIObjectType character(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId());

    // The ability type of the archer's selected weapon (1 melee, 2 ranged).
    auto selectedType = [pEquipment]() {
        CItem* pItem = pEquipment->m_items[pEquipment->m_selectedWeapon];
        REQUIRE(pItem != NULL);
        pItem->Demand();
        const ITEM_ABILITY* pAbility = pItem->GetAbility(pEquipment->m_selectedWeaponAbility);
        const int type = pAbility != NULL ? (pAbility->type & 0xFF) : -1;
        pItem->Release();
        return type;
    };
    REQUIRE(selectedType() == 2);

    // Two search squares away, beyond a range of 0: the arrows stay.
    pArcher->m_curAction = CAIAction(255, character, 0, 0, 0);
    CHECK(pArcher->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pEquipment->m_selectedWeapon == 11);

    // Within a range of 5: a melee weapon.
    pArcher->m_curAction = CAIAction(255, character, 5, 0, 0);
    CHECK(pArcher->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pEquipment->m_selectedWeapon != 11);
    CHECK(selectedType() == 1);
    ClearMessages();
}

// CAIObjectType::Decode's NearestEnemyOf (12, 0x40BF48) and Nearest (14,
// 0x40C208): Nearest asks the area for the second nearest of the current
// object's type, as SecondNearest does.
TEST_CASE("combat: NearestEnemyOf decodes to the nearest goblin and Nearest to the second nearest", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    const CPoint pos = pCharacter->GetPos();
    CGameSprite* pMiddle = AddCreature(fight.pArea, "10GOB", CPoint(pos.x + 4 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    CGameSprite* pFar = AddCreature(fight.pArea, "10GOB", CPoint(pos.x + 7 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    REQUIRE(pMiddle != NULL);
    REQUIRE(pFar != NULL);
    fight.pArea->SortLists();

    // NearestEnemyOf(Myself) of the party member: the nearest goblin.
    CAIObjectType enemy(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    enemy.m_SpecialCase[0] = 12;
    enemy.Decode(pCharacter);
    CHECK(enemy.m_nInstance == pGoblin->GetId());

    // Nearest with the goblins' type, from the nearest goblin: of the two
    // other goblins, the second nearest (the far one), not the nearest (the
    // middle one).
    CAIObjectType nearest;
    nearest.Set(pGoblin->GetAIType());
    nearest.m_sName = "";
    nearest.m_nInstance = -1;
    nearest.m_SpecialCase[0] = 14;
    nearest.Decode(pGoblin);
    INFO("middle " << pMiddle->GetId() << ", far " << pFar->GetId() << ", decoded " << nearest.m_nInstance);
    CHECK(nearest.m_nInstance == pFar->GetId());
}

// Unmarshal (0x70C600) resets a loaded character's empty action bar buttons
// from QSLOTS.2DA (the inlined ResetQuickSlots, 0x70C8E6-0x70C96E).
TEST_CASE("combat: a loaded party member's empty action bar buttons get its class's defaults", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CSavedGamePartyCreature partyCreature;
    pCharacter->Marshal(partyCreature, FALSE);

    // Slot 0 keeps a button of its own; the others are empty.
    for (INT nSlot = 0; nSlot < CGAMESAVECHARACTER_NUM_CUSTOM_BUTTONS22; nSlot++) {
        partyCreature.field_19A[nSlot] = nSlot == 0 ? 2 : 0;
    }
    pCharacter->Unmarshal(&partyCreature, TRUE, FALSE);

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    INT nClass = pCharacter->GetDerivedStats()->GetBestClass() - 1;
    INFO("class row " << nClass);
    CHECK(pCharacter->field_3D14[0] == 2);
    for (INT nSlot = 1; nSlot < CGAMESAVECHARACTER_NUM_CUSTOM_BUTTONS22; nSlot++) {
        INFO("slot " << nSlot);
        CHECK(pCharacter->field_3D14[nSlot] == atol(ruleTables.m_tQuickSlots.GetAt(CPoint(nSlot, nClass))));
    }
    ClearMessages();
}

// CGameArea::GetTintColor (0x470F10): the light map's colour at the corners
// of the point's 16 by 12 cell, interpolated.
TEST_CASE("combat: the area tints a point with its light map", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameArea* pArea = fight.pArea;
    REQUIRE(pArea->m_bmLum.pRes != NULL);

    // A flying object is never tinted.
    CHECK(pArea->GetTintColor(CPoint(400, 300), CGameObject::LIST_FLIGHT) == RGB(255, 255, 255));

    // The day map alone (no day and night code in the area type).
    const WORD areaType = pArea->m_cInfinity.m_areaType;
    pArea->m_cInfinity.m_areaType = areaType & ~0x40;

    RGBQUAD c00;
    RGBQUAD c10;
    RGBQUAD c01;
    RGBQUAD c11;
    REQUIRE(pArea->m_bmLum.GetPixelColor(c00, 25, 25, FALSE));
    REQUIRE(pArea->m_bmLum.GetPixelColor(c10, 26, 25, FALSE));
    REQUIRE(pArea->m_bmLum.GetPixelColor(c01, 25, 26, FALSE));
    REQUIRE(pArea->m_bmLum.GetPixelColor(c11, 26, 26, FALSE));

    // On a cell's corner: that pixel's colour.
    CHECK(pArea->GetTintColor(CPoint(25 * 16, 25 * 12), CGameObject::LIST_FRONT)
        == RGB(c00.rgbRed, c00.rgbGreen, c00.rgbBlue));

    // In the middle: the four corners weighed by 8/16 and 6/12, each step
    // truncated.
    auto mid = [](BYTE a, BYTE b, BYTE c, BYTE d) {
        INT nTop = (a * 8 + b * 8) / 16;
        INT nBottom = (c * 8 + d * 8) / 16;
        return static_cast<BYTE>((nTop * 6 + nBottom * 6) / 12);
    };
    CHECK(pArea->GetTintColor(CPoint(25 * 16 + 8, 25 * 12 + 6), CGameObject::LIST_FRONT)
        == RGB(mid(c00.rgbRed, c10.rgbRed, c01.rgbRed, c11.rgbRed),
            mid(c00.rgbGreen, c10.rgbGreen, c01.rgbGreen, c11.rgbGreen),
            mid(c00.rgbBlue, c10.rgbBlue, c01.rgbBlue, c11.rgbBlue)));

    // 5 pixels across and 3 down in the first cell (row by row) whose four
    // corners have four different reds (that cell's corners are uniform):
    // the left corners weigh 11/16 and the right ones 5/16, the top edge
    // 9/12 and the bottom one 3/12, so swapping x and y or two corners
    // changes the colour.
    INT nCellX = -1;
    INT nCellY = -1;
    RGBQUAD d00 = {};
    RGBQUAD d10 = {};
    RGBQUAD d01 = {};
    RGBQUAD d11 = {};
    for (INT y = 0; y < 200 && nCellX == -1; y++) {
        for (INT x = 0; x < 300; x++) {
            if (!pArea->m_bmLum.GetPixelColor(d00, x, y, FALSE)
                || !pArea->m_bmLum.GetPixelColor(d10, x + 1, y, FALSE)
                || !pArea->m_bmLum.GetPixelColor(d01, x, y + 1, FALSE)
                || !pArea->m_bmLum.GetPixelColor(d11, x + 1, y + 1, FALSE)) {
                break;
            }
            const BYTE reds[4] = { d00.rgbRed, d10.rgbRed, d01.rgbRed, d11.rgbRed };
            bool bDistinct = true;
            for (int i = 0; i < 4; i++) {
                for (int j = i + 1; j < 4; j++) {
                    bDistinct = bDistinct && reds[i] != reds[j];
                }
            }
            if (bDistinct) {
                nCellX = x;
                nCellY = y;
                break;
            }
        }
    }
    REQUIRE(nCellX != -1);
    INFO("cell " << nCellX << "," << nCellY);
    auto off = [](BYTE a, BYTE b, BYTE c, BYTE d) {
        INT nTop = (a * 11 + b * 5) / 16;
        INT nBottom = (c * 11 + d * 5) / 16;
        return static_cast<BYTE>((nTop * 9 + nBottom * 3) / 12);
    };
    CHECK(pArea->GetTintColor(CPoint(nCellX * 16 + 5, nCellY * 12 + 3), CGameObject::LIST_FRONT)
        == RGB(off(d00.rgbRed, d10.rgbRed, d01.rgbRed, d11.rgbRed),
            off(d00.rgbGreen, d10.rgbGreen, d01.rgbGreen, d11.rgbGreen),
            off(d00.rgbBlue, d10.rgbBlue, d01.rgbBlue, d11.rgbBlue)));
    CHECK(GetRValue(pArea->GetTintColor(CPoint(nCellX * 16 + 5, nCellY * 12 + 3), CGameObject::LIST_FRONT))
        != off(d00.rgbRed, d01.rgbRed, d10.rgbRed, d11.rgbRed));

    pArea->m_cInfinity.m_areaType = areaType;
}

// CGameSprite::ApplyTriggers (0x728580), the sprite's override of vtable
// +0xAC: a KILLED trigger (0x4B) raises the killer's morale by 3 and, for a
// party member's kill of an enemy, shares the enemy's challenge rating
// experience through an XP effect message per member and keeps the total
// in the enemy's derived experience value (first_fight's kill: the
// original's party member had 37 experience points at frame 5452, ours 0);
// each trigger is read once (flag 0x2). HITBY (0x20) lowers morale by 2 as
// the hit points fall below half.
TEST_CASE("combat: a party member's kill gives the party the goblin's experience", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    REQUIRE(pGoblin->GetAIType().m_nEnemyAlly > CAIObjectType::EA_EVILCUTOFF);
    REQUIRE(pCharacter->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF);

    pCharacter->m_baseStats.m_morale = 10;
    ClearMessages();
    CAITrigger killed(CAITRIGGER_KILLED, pGoblin->GetAIType(), 0);
    killed.m_triggerCause.m_nInstance = pGoblin->GetId();
    killed.m_specificID = pGoblin->m_baseStats.m_xpValue;
    pCharacter->SetTrigger(killed);
    pCharacter->ApplyTriggers();
    CHECK(pCharacter->m_baseStats.m_morale == 13);

    // One XP effect (104) for the one party member, the total kept in the
    // goblin's derived stats.
    LONG nGiven = pGoblin->m_derivedStats.m_nXPValue;
    CHECK(nGiven > 0);
    INT nXPMessages = 0;
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        CMessageAddEffect* pMessage = dynamic_cast<CMessageAddEffect*>(handler.m_messageList.GetNext(pos));
        if (pMessage != NULL && pMessage->m_effect->m_effectID == 104) {
            nXPMessages++;
            CHECK(pMessage->GetTarget() == pCharacter->GetId());
            CHECK(pMessage->m_effect->m_effectAmount == nGiven);
        }
    }
    CHECK(nXPMessages == 1);

    // Read once: a second pass changes nothing.
    ClearMessages();
    pCharacter->ApplyTriggers();
    CHECK(pCharacter->m_baseStats.m_morale == 13);
    CHECK(handler.m_messageList.IsEmpty());

    // HITBY: from full hit points to below half, morale drops by 2.
    const SHORT nMax = pCharacter->m_derivedStats.m_nMaxHitPoints;
    REQUIRE(nMax >= 4);
    const SHORT nHitPoints = pCharacter->m_baseStats.m_hitPoints;
    pCharacter->field_7110 = nMax;
    pCharacter->m_baseStats.m_hitPoints = static_cast<SHORT>((nMax - 1) / 2);
    if (pCharacter->m_baseStats.m_hitPoints * 100 / nMax < 25) {
        pCharacter->m_baseStats.m_hitPoints = static_cast<SHORT>(nMax * 40 / 100);
    }
    INFO("hit points " << pCharacter->m_baseStats.m_hitPoints << " of " << nMax);
    REQUIRE(pCharacter->m_baseStats.m_hitPoints * 100 / nMax < 50);
    REQUIRE(pCharacter->m_baseStats.m_hitPoints * 100 / nMax >= 25);
    CAITrigger hit(CAITRIGGER_HITBY, pGoblin->GetAIType(), 0);
    pCharacter->SetTrigger(hit);
    pCharacter->ApplyTriggers();
    CHECK(pCharacter->m_baseStats.m_morale == 11);
    CHECK(pCharacter->field_7110 == pCharacter->m_baseStats.m_hitPoints);
    pCharacter->m_baseStats.m_hitPoints = nHitPoints;
    ClearMessages();
}

// CGameSprite::HandlePersistantEffects (0x733200), which ProcessEffectList
// runs while the world timer is active (0x72FBA7-0x72FBC3): each persistent
// effect's AIUpdate, and the finished ones removed. A colour pulse of period
// 3 (as Magic Missile's glow on its target) takes its first tick, goes on
// at the second (counter 2 < 3) and ends at the third (counter 3, compared
// as incremented at 0x4C1777; ours ended a tick early), clearing the colour
// effects and setting field_562C so the effect list's saving throws are
// rolled again (first_fight's dead goblin at frame 5511).
TEST_CASE("combat: a colour pulse runs its period and asks for the effect list again", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pGoblin = fight.pGoblin;
    pGoblin->m_persistantEffects.RemoveAll();

    CPersistantEffectColorEffect* pPulse = new CPersistantEffectColorEffect();
    pPulse->m_effectFinalRed = 200;
    pPulse->m_effectFinalGreen = 100;
    pPulse->m_effectFinalBlue = 50;
    pPulse->m_effectDegreeRed = 0;
    pPulse->m_effectDegreeGreen = 0;
    pPulse->m_effectDegreeBlue = 0;
    pPulse->m_undo = TRUE;
    pPulse->m_colorEffect = 1;
    pPulse->m_period = 3;
    pGoblin->m_persistantEffects.AddTail(pPulse);
    pGoblin->field_562C = 0;

    CHECK(pGoblin->HandlePersistantEffects(1) == TRUE);
    REQUIRE(pGoblin->m_persistantEffects.GetCount() == 1);
    CHECK(pPulse->m_counter == 1);
    CHECK(pPulse->m_effectDegreeRed == 200 / 3);
    CHECK(pGoblin->HandlePersistantEffects(1) == TRUE);
    REQUIRE(pGoblin->m_persistantEffects.GetCount() == 1);
    CHECK(pPulse->m_counter == 2);
    CHECK(pPulse->m_effectDegreeRed == 2 * (200 / 3));
    CHECK(pGoblin->field_562C == 0);

    CHECK(pGoblin->HandlePersistantEffects(1) == TRUE);
    CHECK(pGoblin->m_persistantEffects.IsEmpty());
    CHECK(pGoblin->field_562C == 1);
    ClearMessages();
}

// CGameAnimationTypeMonsterIcewind::SetSequence (0x6E7DF0), the goblin's
// animation, which takes the sprite's sequence numbers: DIE (5) takes the
// DE cells and draws no frame (its jump table entry, 0x6E7F0F, ends at
// 0x6E80DB; ours fell into HEAD_TURN's case, which draws a random first
// frame on the SD cells: first_fight's Magic Missile kill drew once more
// than the original at frame 5450), HEAD_TURN (6) after it draws one, and
// SLEEP (17) takes the SL cells (0x6E7F57).
TEST_CASE("combat: an Icewind monster dies on its DE cells without a frame draw", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameAnimationTypeMonsterIcewind* pAnimation = dynamic_cast<CGameAnimationTypeMonsterIcewind*>(fight.pGoblin->m_animation.m_animation);
    REQUIRE(pAnimation != NULL);

    srand(1);
    CHECK(pAnimation->SetSequence(5) == 5);
    CHECK(pAnimation->m_currentVidCellBase == &pAnimation->m_deVidCellBase);
    CHECK(pAnimation->m_currentVidCellExtend == &pAnimation->m_deVidCellExtend);
    CHECK(iwd2r_rand_state() == 1);

    CHECK(pAnimation->SetSequence(6) == 6);
    CHECK(pAnimation->m_currentVidCellBase == &pAnimation->m_sdVidCellBase);
    CHECK(iwd2r_rand_state() == 1u * 214013u + 2531011u);

    srand(1);
    CHECK(pAnimation->SetSequence(17) == 17);
    CHECK(pAnimation->m_currentVidCellBase == &pAnimation->m_slVidCellBase);
    CHECK(pAnimation->m_currentVidCellExtend == &pAnimation->m_slVidCellExtend);
    CHECK(iwd2r_rand_state() == 1);
    ClearMessages();
}
