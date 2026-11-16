#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "CProjectile.h"
#include "CProjectile850F9C.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
#include "CProjectileSingleTarget.h"
#include "CProjectileSpellHit.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "ScreenLayout.h"
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
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The projectiles (CProjectile*.cpp) and the messages they send
// (CMessageGroup, CMessageFireProjectile) on a party member standing in
// AR1000. Every engine works on a private view of the install
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

std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
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

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The party member every case works on, made once.
CGameSprite* Probe()
{
    static CGameSprite* pSprite = NULL;
    if (pSprite == NULL) {
        std::vector<BYTE> cre = FinishedCharacter();
        if (cre.empty()) {
            return NULL;
        }
        pSprite = NewGameInStartArea(cre);
    }
    return pSprite;
}

// An effect of this opcode decoded as the game decodes it, its source the
// sprite itself.
std::unique_ptr<CGameEffect> Decode(CGameSprite* pSprite, WORD effectID, DWORD dwFlags, LONG effectAmount)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    effect.dwFlags = dwFlags;
    effect.effectAmount = effectAmount;
    return std::unique_ptr<CGameEffect>(CGameEffect::DecodeEffect(&effect, pSprite->GetPos(), pSprite->GetId(), pSprite->GetPos()));
}

#define PROBE_OR_SKIP(pSprite)                                          \
    if (DataDir() == nullptr) {                                         \
        SKIP("[iwd2r-skip] IWD2_DATA unset");                           \
    }                                                                   \
    std::string error;                                                  \
    REQUIRE(StartEngine(error));                                        \
    INFO(error);                                                        \
    FramePump pump;                                                     \
    CGameSprite* pSprite = Probe();                                     \
    if (pSprite == NULL) {                                              \
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder"); \
    }

// A message that records the order it ran in and that it was deleted.
class RecordingMessage : public CMessage {
public:
    RecordingMessage(std::vector<int>& runs, int& deleted, int tag)
        : CMessage(CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX)
        , m_runs(runs)
        , m_deleted(deleted)
        , m_tag(tag)
    {
    }
    ~RecordingMessage() override
    {
        m_deleted++;
    }
    void Run() override
    {
        m_runs.push_back(m_tag);
    }

private:
    std::vector<int>& m_runs;
    int& m_deleted;
    int m_tag;
};

// The CMessageGroup the handler holds as its last message, or NULL.
CMessageGroup* LastGroup()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    if (handler.m_messageList.IsEmpty()) {
        return NULL;
    }
    return dynamic_cast<CMessageGroup*>(handler.m_messageList.GetTail());
}

}

TEST_CASE("projectile: a message group runs its messages in order and deletes them", "[projectile]")
{
    std::vector<int> runs;
    int deleted = 0;
    std::list<CMessage*> messages;
    messages.push_back(new RecordingMessage(runs, deleted, 1));
    messages.push_back(new RecordingMessage(runs, deleted, 2));
    messages.push_back(new RecordingMessage(runs, deleted, 3));

    CMessageGroup* pGroup = new CMessageGroup(messages, 7, 9);
    CHECK(pGroup->GetSource() == 7);
    CHECK(pGroup->GetTarget() == 9);
    CHECK(pGroup->GetCommType() == CMessage::SEND);
    CHECK(pGroup->GetMsgType() == CBaldurMessage::MSG_TYPE_CMESSAGE);
    CHECK(pGroup->GetMsgSubType() == 105);
    REQUIRE(pGroup->m_messages.size() == 3);

    pGroup->Run();
    CHECK(runs == std::vector<int> { 1, 2, 3 });
    CHECK(deleted == 0);

    delete pGroup;
    CHECK(deleted == 3);
}

TEST_CASE("projectile: DeliverEffects sends the effects to the target as one message group", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(1, NULL, 0);
    REQUIRE(pProjectile != nullptr);
    CHECK(pProjectile->m_projectileType == 0);
    pProjectile->m_sourceId = pSprite->GetId();
    pProjectile->m_targetId = pSprite->GetId();

    std::unique_ptr<CGameEffect> pFirst = Decode(pSprite, CGAMEEFFECT_AC, 0, 0);
    std::unique_ptr<CGameEffect> pSecond = Decode(pSprite, CGAMEEFFECT_AC, 0, 0);
    REQUIRE(pFirst != nullptr);
    REQUIRE(pSecond != nullptr);
    CGameEffect* first = pFirst.release();
    CGameEffect* second = pSecond.release();
    pProjectile->AddEffect(first);
    pProjectile->AddEffect(second);

    pProjectile->DeliverEffects();
    CHECK(pProjectile->m_effectList.IsEmpty());

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->GetSource() == pSprite->GetId());
    CHECK(pGroup->GetTarget() == pSprite->GetId());
    REQUIRE(pGroup->m_messages.size() == 2);
    CMessageAddEffect* pAddFirst = dynamic_cast<CMessageAddEffect*>(pGroup->m_messages.front());
    CMessageAddEffect* pAddSecond = dynamic_cast<CMessageAddEffect*>(pGroup->m_messages.back());
    REQUIRE(pAddFirst != nullptr);
    REQUIRE(pAddSecond != nullptr);
    CHECK(pAddFirst->m_effect == first);
    CHECK(pAddSecond->m_effect == second);
    CHECK(pAddFirst->GetSource() == pSprite->GetId());
    CHECK(pAddFirst->GetTarget() == pSprite->GetId());

    ClearMessages();
    delete pProjectile;
}

TEST_CASE("projectile: DeliverEffects drops the effects of an immune target", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(1, NULL, 0);
    pProjectile->m_sourceId = pSprite->GetId();
    pProjectile->m_targetId = pSprite->GetId();

    SECTION("immune to the projectile")
    {
        pProjectile->m_projectileType = 77;
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(77));
        pProjectile->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
        pProjectile->DeliverEffects();
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.RemoveTail();
    }
    SECTION("immune to the spell level")
    {
        pProjectile->m_nSpellLevel = 3;
        BOOL saved = pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[3];
        pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[3] = TRUE;
        pProjectile->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
        pProjectile->DeliverEffects();
        pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[3] = saved;
    }

    CHECK(pProjectile->m_effectList.IsEmpty());
    CHECK(LastGroup() == nullptr);

    ClearMessages();
    delete pProjectile;
}

TEST_CASE("projectile: the instant projectile delivers on Fire and the handler runs the group", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(1, NULL, 0);
    CGameEffect* pEffect = Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release();
    // A timed effect that lasts, so it stays on the sprite's list.
    pEffect->m_durationType = 0;
    pEffect->m_duration = 100000;
    pProjectile->AddEffect(pEffect);
    CHECK(pSprite->m_timedEffectList.Find(pEffect) == NULL);
    // Fire delivers the effects and deletes the projectile.
    pProjectile->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->m_messages.size() == 1);

    handler.AsynchronousUpdate();
    CHECK(handler.m_messageList.IsEmpty());
    // The group's CMessageAddEffect gave the effect to the sprite.
    POSITION posEffect = pSprite->m_timedEffectList.Find(pEffect);
    CHECK(posEffect != NULL);
    if (posEffect != NULL) {
        pSprite->m_timedEffectList.RemoveAt(posEffect);
        delete pEffect;
    }

    ClearMessages();
}

TEST_CASE("projectile: CMessageFireProjectile runs without a service provider and fires nothing", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    CMessage* message = new CMessageFireProjectile(0x1004,
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        pSprite->GetId(),
        pSprite->GetId(),
        0);
    handler.AddMessage(message, FALSE);
    REQUIRE(handler.m_messageList.GetCount() == 1);

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    SHORT nMaxArrayIndex = pArray->m_nMaxArrayIndex;
    SHORT nNextObjectId = pArray->m_nNextObjectId;
    SHORT nDeletedListHead = pArray->m_deletedListHead;
    INT_PTR nAdded = pSprite->GetArea()->m_lVertSortAdd.GetCount();

    handler.AsynchronousUpdate();
    CHECK(handler.m_messageList.IsEmpty());
    // Nothing was added to the object array or to the area.
    CHECK(pArray->m_nMaxArrayIndex == nMaxArrayIndex);
    CHECK(pArray->m_nNextObjectId == nNextObjectId);
    CHECK(pArray->m_deletedListHead == nDeletedListHead);
    CHECK(pSprite->GetArea()->m_lVertSortAdd.GetCount() == nAdded);

    ClearMessages();
}

TEST_CASE("projectile: the spell hit decoder builds the school's animation, sound and glow", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    INT nTimed = pSprite->m_timedEffectList.GetCount();
    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(1, pSprite, FALSE);
    CProjectileSpellHit* pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
    REQUIRE(pSpellHit != nullptr);
    CHECK(pSpellHit->m_fireSoundRef == "EFF_M02");
    CHECK(pSpellHit->m_bPickRandomSequence == TRUE);
    CHECK(pSpellHit->m_bDestroyOnEndOfSequence == TRUE);
    CHECK(pSpellHit->m_bHasHeight == FALSE);
    CHECK(pSprite->m_timedEffectList.GetCount() >= nTimed);
    delete pSpellHit;

    pProjectile = CProjectile::DecodeSpellHitProjectile(1, NULL, TRUE);
    pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
    REQUIRE(pSpellHit != nullptr);
    CHECK(pSpellHit->m_fireSoundRef == "EFF_P01");
    delete pSpellHit;

    pProjectile = CProjectile::DecodeSpellHitProjectile(74, NULL, FALSE);
    pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
    REQUIRE(pSpellHit != nullptr);
    CHECK(pSpellHit->m_bDrawBehind == TRUE);
    CHECK(pSpellHit->m_bLoopingSequence == TRUE);
    CHECK(pSpellHit->m_nLoopingSequence == 1);
    CHECK(pSpellHit->m_nLoopCount == 0xA5);
    CHECK(pSpellHit->m_bPickRandomSequence == FALSE);
    CHECK(pSpellHit->m_bDestroyOnEndOfSequence == FALSE);
    delete pSpellHit;

    pProjectile = CProjectile::DecodeSpellHitProjectile(0, NULL, FALSE);
    CHECK(dynamic_cast<CProjectile84D8B8*>(pProjectile) != nullptr);
    delete pProjectile;

    // DecodeProjectile hands a type above 0x1000 to the spell hit decoder.
    pProjectile = CProjectile::DecodeProjectile(0x1002, NULL, 0);
    CHECK(dynamic_cast<CProjectileSpellHit*>(pProjectile) != nullptr);
    CHECK(pProjectile->m_projectileType == 0x1001);
    delete pProjectile;

    ClearMessages();
}

TEST_CASE("projectile: a fired spell hit plays in the area and removes itself at the end", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(9, NULL, FALSE);
    REQUIRE(pProjectile != nullptr);
    pProjectile->m_targetId = pSprite->GetId();
    pProjectile->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);

    LONG id = pProjectile->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pProjectile->GetPos().x == pSprite->GetPos().x);
    CHECK(pProjectile->GetPos().y == pSprite->GetPos().y + 1);
    CHECK(pProjectile->GetArea() == pSprite->GetArea());

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    bool removed = false;
    for (int frame = 0; frame < 2000 && !removed; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            removed = true;
            break;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    CHECK(removed);

    ClearMessages();
}

TEST_CASE("projectile: an area projectile explodes and sends its effects to the targets in range", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectileArea* pArea = new CProjectileArea(300);
    CHECK(pArea->m_nRadius == 300);
    CHECK(pArea->m_nSecondaryType == 78);
    CHECK(pArea->m_nDuration == 45);

    pArea->TargetEnemy(pSprite);
    CHECK(pArea->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ENEMY);
    pArea->TargetFriend(pSprite);
    CHECK(pArea->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ALLY);

    pArea->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
    pArea->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);
    LONG id = pArea->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pArea->GetPos() == pSprite->GetPos());

    // It does not travel: Fire explodes it where it is.
    CHECK(pArea->m_bExploded == TRUE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    // The area puts it on its vertical list (as its AI update does between
    // the frames); the next update finds the party member in range and fires
    // the secondary projectile (type 79, an invisible CProjectileBAM) at it.
    pSprite->GetArea()->SortLists();
    pArea->AIUpdate();
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());
    CHECK(pArea->m_targetHits.count(pSprite->GetId()) == 1);
    CHECK(pArea->m_targetHits[pSprite->GetId()] == 1);

    CTypedPtrList<CPtrList, int*>& added = pSprite->GetArea()->m_lVertSortAdd;
    REQUIRE(!added.IsEmpty());
    LONG secondaryId = static_cast<LONG>(reinterpret_cast<INT_PTR>(added.GetTail()));
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CGameObject* pSecondary;
    REQUIRE(pArray->GetShare(secondaryId, CGameObjectArray::THREAD_ASYNCH, &pSecondary, INFINITE) == CGameObjectArray::SUCCESS);
    pArray->ReleaseShare(secondaryId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    CProjectileBAM* pBAM = dynamic_cast<CProjectileBAM*>(pSecondary);
    REQUIRE(pBAM != nullptr);
    CHECK(pBAM->m_projectileType == 78);
    CHECK(pBAM->m_sourceId == id);
    CHECK(pBAM->m_targetId == pSprite->GetId());
    CHECK(pBAM->field_1DE == FALSE);

    // At the target, it delivers a copy of the effect and removes itself.
    pBAM->AIUpdate();
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->GetTarget() == pSprite->GetId());
    CHECK(pGroup->GetSource() == id);
    REQUIRE(pGroup->m_messages.size() == 1);
    CMessageAddEffect* pAdd = dynamic_cast<CMessageAddEffect*>(pGroup->m_messages.front());
    REQUIRE(pAdd != nullptr);
    CHECK(pAdd->m_effect->m_effectID == CGAMEEFFECT_AC);
    CHECK(pArray->GetShare(secondaryId, CGameObjectArray::THREAD_ASYNCH, &pSecondary, INFINITE) != CGameObjectArray::SUCCESS);
    ClearMessages();
    pSprite->GetArea()->SortLists();

    // The next target search skips the sprite until it has been found ten
    // times.
    std::list<LONG> targets = pArea->GetTargets();
    CHECK(targets.empty());
    CHECK(pArea->m_targetHits[pSprite->GetId()] == 2);

    // It removes itself when its duration runs out.
    bool removed = false;
    for (int frame = 0; frame < 100 && !removed; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            removed = true;
            break;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    CHECK(removed);

    ClearMessages();
}

TEST_CASE("projectile: the cloak of fear area leaves its source out", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectileArea8504BC* pArea = new CProjectileArea8504BC(256);
    pArea->m_sourceId = pSprite->GetId();
    pArea->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
    std::list<LONG> targets;
    targets.push_back(pSprite->GetId());
    pArea->ApplyToTargets(targets);
    CHECK(targets.empty());
    CHECK(LastGroup() == nullptr);

    delete pArea;
    ClearMessages();
}

TEST_CASE("projectile: the casting glow waits, loops and arrives at the caster", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    IcewindCVisualEffect visualEffect;
    CProjectile850F9C* pGlow = new CProjectile850F9C(CString("AbjurCG"), CString(""), 0, 2, visualEffect);
    CHECK(pGlow->m_nDelay == 2);
    CHECK(pGlow->m_nLoops == 0);
    CHECK(pGlow->m_glow.m_nCurrentSequence == 2);
    CHECK(pGlow->m_bHasHeight == TRUE);

    pGlow->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);
    LONG id = pGlow->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    // It has no effects, so it delivers an empty group at once.
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->m_messages.empty());
    ClearMessages();

    pGlow->AIUpdate();
    pGlow->AIUpdate();
    CHECK(pGlow->m_nDelay == 0);

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    bool removed = false;
    for (int frame = 0; frame < 2000 && !removed; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            removed = true;
            break;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    CHECK(removed);

    ClearMessages();
}

namespace {

// The objects the area holds in its add lists (what Fire added since the last
// SortLists).
std::vector<CGameObject*> AddedObjects(CGameArea* pArea)
{
    std::vector<CGameObject*> objects;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (CTypedPtrList<CPtrList, int*>* pList : lists) {
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pList->GetNext(pos)));
            CGameObject* pObject;
            if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                objects.push_back(pObject);
            }
        }
    }
    return objects;
}

bool Near(LONG a, LONG b)
{
    return a - b <= 1 && b - a <= 1;
}

}

TEST_CASE("projectile: the cone builds its animation and its defaults", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);

    CProjectile850DDC* pCone = new CProjectile850DDC("", "");
    CHECK(pCone->m_nWidth == 200);
    CHECK(pCone->m_nLength == 350);
    CHECK(pCone->m_nHalfWidth == 25);
    CHECK(pCone->m_nPuffSpacing == 20);
    CHECK(pCone->m_nFrames == 15);
    CHECK(pCone->m_nWaveInterval == 5);
    CHECK(pCone->m_speed == 20);
    CHECK(pCone->m_sPuff.empty());
    // An empty animation falls back to MMissiT, with +0x1DE cleared.
    REQUIRE(pCone->m_animation != nullptr);
    CHECK(pCone->field_1DE == FALSE);
    CHECK(pCone->m_bHasHeight == TRUE);
    CHECK(pCone->field_2A0 == 0);
    delete pCone;

    pCone = new CProjectile850DDC("SPMAGMIS", "SPMAGMIS");
    CHECK(pCone->field_1DE == TRUE);
    CHECK(pCone->m_sPuff == "SPMAGMIS");
    delete pCone;
}

TEST_CASE("projectile: the cone's Fire lays out its corners and puff points", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CGameArea* pArea = pSprite->GetArea();
    pArea->SortLists();

    CPoint ptSource;
    CProjectile::GetSourcePosition(pSprite->GetId(), ptSource, TRUE);

    // Straight east, as far as the cone is long.
    CProjectile850DDC* pCone = new CProjectile850DDC("", "SPMAGMIS");
    CPoint ptTarget(ptSource.x + 350, ptSource.y);
    pCone->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, ptTarget, 7, 3);
    CHECK(pCone->m_ptSource == ptSource);
    CHECK(pCone->m_nHeight == 7);
    CHECK(pCone->m_nType == 3);
    CHECK(pCone->m_nPuffs == 5);
    CHECK(pCone->m_ptEnd == ptTarget);
    // At the source: 25 either side; at the far end (375 out): 125 either
    // side.
    CHECK(pCone->m_corners[0].x == ptSource.x);
    CHECK(Near(pCone->m_corners[0].y, ptSource.y + 25));
    CHECK(pCone->m_corners[1].x == ptSource.x);
    CHECK(Near(pCone->m_corners[1].y, ptSource.y - 25));
    CHECK(Near(pCone->m_corners[2].x, ptSource.x + 375));
    CHECK(Near(pCone->m_corners[2].y, ptSource.y - 125));
    CHECK(Near(pCone->m_corners[3].x, ptSource.x + 375));
    CHECK(Near(pCone->m_corners[3].y, ptSource.y + 125));
    // Ten points 20 apart across the end, then the end once more.
    REQUIRE(pCone->m_puffPoints.size() == 11);
    for (size_t n = 0; n < 10; n++) {
        const LONG offset = static_cast<LONG>(n) - 5;
        CHECK(pCone->m_puffPoints[n].x == ptTarget.x);
        CHECK(Near(pCone->m_puffPoints[n].y, ptTarget.y - 20 * offset));
    }
    CHECK(pCone->m_puffPoints[5] == ptTarget);
    CHECK(pCone->m_puffPoints[10] == ptTarget);

    // The first frame sends a puff to each point, at 35-79% of the cone's
    // speed.
    pArea->SortLists();
    pCone->AIUpdate();
    CHECK(pCone->m_nFrame == 1);
    std::vector<CGameObject*> puffs = AddedObjects(pArea);
    REQUIRE(puffs.size() == 11);
    for (size_t n = 0; n < puffs.size(); n++) {
        CProjectile850D54* pPuff = dynamic_cast<CProjectile850D54*>(puffs[n]);
        REQUIRE(pPuff != nullptr);
        CHECK(pPuff->m_sourceId == pSprite->GetId());
        CHECK(pPuff->m_targetId == CGameObjectArray::INVALID_INDEX);
        CHECK(pPuff->m_speed >= 7);
        CHECK(pPuff->m_speed <= 15);
        CHECK(pPuff->field_1D8 == 1);
    }
    pArea->SortLists();
    for (CGameObject* pObject : puffs) {
        static_cast<CProjectile*>(pObject)->RemoveSelf();
    }

    // Frames 2-5: no wave; frame 6: the next one.
    for (int frame = 2; frame <= 5; frame++) {
        pCone->AIUpdate();
    }
    CHECK(AddedObjects(pArea).empty());
    pCone->AIUpdate();
    CHECK(AddedObjects(pArea).size() == 11);
    pArea->SortLists();
    for (CGameObject* pObject : AddedObjects(pArea)) {
        static_cast<CProjectile*>(pObject)->RemoveSelf();
    }

    pCone->RemoveSelf();
    ClearMessages();
}

TEST_CASE("projectile: the cone gives copies of its effects to what is inside it", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CGameArea* pArea = pSprite->GetArea();
    pArea->SortLists();

    CProjectile850DDC* pCone = new CProjectile850DDC("", "");
    pCone->m_pArea = pArea;
    pCone->m_sourceId = CGameObjectArray::INVALID_INDEX;
    CGameEffect* pEffect = Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release();
    pCone->AddEffect(pEffect);

    // A square around the sprite, seen from just west of it.
    const CPoint pos = pSprite->GetPos();
    pCone->m_corners[0] = CPoint(pos.x - 40, pos.y - 40);
    pCone->m_corners[1] = CPoint(pos.x - 40, pos.y + 40);
    pCone->m_corners[2] = CPoint(pos.x + 40, pos.y + 40);
    pCone->m_corners[3] = CPoint(pos.x + 40, pos.y - 40);
    pCone->m_ptSource = CPoint(pos.x - 20, pos.y);
    pCone->DeliverEffects();
    CHECK(pCone->m_bDelivered == TRUE);
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    bool found = false;
    POSITION pos2 = handler.m_messageList.GetHeadPosition();
    while (pos2 != NULL) {
        CMessageAddEffect* pAdd = dynamic_cast<CMessageAddEffect*>(handler.m_messageList.GetNext(pos2));
        REQUIRE(pAdd != nullptr);
        if (pAdd->GetTarget() == pSprite->GetId()) {
            found = true;
            CHECK(pAdd->GetSource() == CGameObjectArray::INVALID_INDEX);
            CHECK(pAdd->m_effect != pEffect);
            CHECK(pAdd->m_effect->m_effectID == CGAMEEFFECT_AC);
        }
    }
    CHECK(found);
    // The cone keeps its own effects.
    CHECK(pCone->m_effectList.GetCount() == 1);
    ClearMessages();

    // The source itself is left out.
    pCone->m_sourceId = pSprite->GetId();
    pCone->DeliverEffects();
    pos2 = handler.m_messageList.GetHeadPosition();
    while (pos2 != NULL) {
        CHECK(handler.m_messageList.GetNext(pos2)->GetTarget() != pSprite->GetId());
    }
    ClearMessages();

    delete pCone;
}

TEST_CASE("projectile: a fired cone arrives, delivers and removes itself after its frames", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CGameArea* pArea = pSprite->GetArea();
    pArea->SortLists();

    // Aimed at the sprite itself (the target id wins over the point), it
    // flies the short way to it.
    CProjectile850DDC* pCone = new CProjectile850DDC("", "");
    CPoint ptTarget(pSprite->GetPos().x + 100, pSprite->GetPos().y);
    pCone->Fire(pArea, pSprite->GetId(), pSprite->GetId(), ptTarget, 0, 0);
    LONG id = pCone->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    pArea->SortLists();

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    bool removed = false;
    int frames = 0;
    int delivered = -1;
    for (; frames < 400; frames++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            removed = true;
            break;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        CProjectile850DDC* pLive = dynamic_cast<CProjectile850DDC*>(pObject);
        REQUIRE(pLive != nullptr);
        if (delivered < 0 && pLive->m_bDelivered) {
            delivered = frames;
        }
        pObject->AIUpdate();
    }
    // It delivers on arrival, stays, and goes on the first frame after both
    // its arrival and its 15 frames.
    CHECK(removed);
    REQUIRE(delivered >= 0);
    CHECK(frames == std::max(delivered, 15) + 1);
    ClearMessages();
}

TEST_CASE("projectile: Magic Missile fires its missiles at the target with copies of its effects", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    // Type 70 is three missiles (types 68 to 72 are one to five), each of
    // type 218.
    CProjectile* pProjectile = CProjectile::DecodeProjectile(70, NULL, 0);
    CProjectile84E0C4* pMissiles = dynamic_cast<CProjectile84E0C4*>(pProjectile);
    REQUIRE(pMissiles != nullptr);
    CHECK(pMissiles->m_projectileType == 69);
    CHECK(pMissiles->m_fireSoundRef == CResRef("TRA_22"));
    CHECK(pMissiles->m_bHasHeight == TRUE);
    CHECK(pMissiles->m_nCount == 3);
    CHECK(pMissiles->m_nType == 217);
    CHECK(pMissiles->m_speed == 20);
    CHECK(pMissiles->field_1D2 == 0x80);
    CHECK(pMissiles->field_1D8 == 1);

    std::vector<CProjectile*> missiles;
    POSITION pos = pMissiles->m_projectiles.GetHeadPosition();
    while (pos != NULL) {
        missiles.push_back(pMissiles->m_projectiles.GetNext(pos));
    }
    REQUIRE(missiles.size() == 3);
    for (size_t n = 0; n < missiles.size(); n++) {
        CProjectile8510A4* pMissile = dynamic_cast<CProjectile8510A4*>(missiles[n]);
        REQUIRE(pMissile != nullptr);
        CHECK(pMissile->m_projectileType == 217);
        CHECK(pMissile->m_speed == 40);
        CHECK(pMissile->m_bHasHeight == TRUE);
        CHECK(pMissile->field_1CA == 15);
        CHECK(pMissile->field_1CE == 11);
        // Only the first one plays its fire sound.
        CHECK(pMissile->m_fireSoundRef == CResRef(n == 0 ? "TRA_02" : ""));
    }

    pMissiles->m_nSpellLevel = 1;
    pMissiles->m_sourceRes = "SPWI112";
    pMissiles->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());

    // Fired at the caster itself: the line of flight has no length, so its
    // velocity is (1, 1) and the first pair drifts by (1, -1) and (-1, 1).
    pMissiles->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (size_t n = 0; n < missiles.size(); n++) {
        CProjectile* pMissile = missiles[n];
        INFO("missile " << n);
        CGameObject* pObject;
        REQUIRE(pArray->GetShare(pMissile->GetId(), CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS);
        pArray->ReleaseShare(pMissile->GetId(), CGameObjectArray::THREAD_ASYNCH, INFINITE);
        CHECK(pObject == pMissile);
        CHECK(pMissile->m_sourceId == pSprite->GetId());
        CHECK(pMissile->m_targetId == pSprite->GetId());
        CHECK(pMissile->m_nSpellLevel == 1);
        CHECK(pMissile->m_sourceRes == CResRef("SPWI112"));
        CHECK(pMissile->m_speed >= 30);
        CHECK(pMissile->m_speed < 50);
        REQUIRE(pMissile->m_effectList.GetCount() == 1);
        CHECK(pMissile->m_effectList.GetHead()->m_effectID == CGAMEEFFECT_AC);
        CHECK(pMissile->m_effectList.GetHead()->m_projectileType == 217);
    }
    CHECK(missiles[0]->m_driftX == 1);
    CHECK(missiles[0]->m_driftY == -1);
    CHECK(missiles[1]->m_driftX == -1);
    CHECK(missiles[1]->m_driftY == 1);
    CHECK(missiles[0]->m_driftDecay == 0);
    CHECK(missiles[1]->m_driftDecay == 0);
    CHECK(missiles[0]->m_bTrackTargetHeight == TRUE);
    CHECK(missiles[1]->m_bTrackTargetHeight == TRUE);
    // The odd one out flies straight.
    CHECK(missiles[2]->m_driftX == 0);
    CHECK(missiles[2]->m_driftY == 0);
    CHECK(missiles[2]->m_driftDecay == 20);
    CHECK(missiles[2]->m_bTrackTargetHeight == FALSE);

    // Each one flies to its target (from the caster's launch point, which
    // depends on its facing: a few updates at most), delivers its copy of
    // the effect and removes itself.
    for (size_t n = 0; n < missiles.size(); n++) {
        INFO("missile " << n);
        LONG id = missiles[n]->GetId();
        const INT nBefore = g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount();
        bool removed = false;
        for (int update = 0; update < 100 && !removed; update++) {
            CGameObject* pObject;
            if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
                removed = true;
                break;
            }
            pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            pObject->AIUpdate();
        }
        CHECK(removed);
        CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == nBefore + 1);
        CMessageGroup* pGroup = LastGroup();
        REQUIRE(pGroup != nullptr);
        CHECK(pGroup->GetTarget() == pSprite->GetId());
        CHECK(pGroup->GetSource() == pSprite->GetId());
    }
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == 3);

    ClearMessages();
    pSprite->GetArea()->SortLists();
}

// CMessageSpellHitEffectSecondaryProjectile (vtable 0x84D4F4): the ctor
// stores the type it receives plus one; MarshalMessage writes 35 bytes,
// UnmarshalMessage reads them back in the same order, and Run decodes the
// message's type and fires it at the staged projectile target. The remote
// array maps the sprite's remote ids back to its local id so the round trip
// can compare the object-id fields.
TEST_CASE("projectile: the secondary spell-hit message round-trips its wire fields", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameRemoteObjectArray* pRemotes = pGame->GetRemoteObjectArray();
    REQUIRE(pRemotes != NULL);

    const PLAYER_ID nSavedPlayer = pSprite->m_remotePlayerID;
    const LONG nSavedObject = pSprite->m_remoteObjectID;
    pSprite->m_remotePlayerID = 7;
    pSprite->m_remoteObjectID = 0x00010001;

    // The remote object array's Find reads a node out of the bucket for the
    // upper half of the remote object id (1 here); stage one for the sprite.
    CGameRemoteObjectListEntry* pSavedEntry = pRemotes->m_pArray[1];
    auto* pEntry = new CGameRemoteObjectListEntry();
    pEntry->remotePlayerID = pSprite->m_remotePlayerID;
    pEntry->remoteObjectID = pSprite->m_remoteObjectID;
    pEntry->localObjectID = pSprite->GetId();
    pEntry->pNext = NULL;
    pRemotes->m_pArray[1] = pEntry;

    const CPoint ptTarget(321, 654);
    CMessageSpellHitEffectSecondaryProjectile sent(0x1000,
        pSprite->GetId(),
        ptTarget,
        45,
        pSprite->GetId(),
        pSprite->GetId(),
        9);
    // The ctor adds one: 0x1000 + 1 is the spell-hit type 0's real type.
    CHECK(sent.m_projectileType == 0x1001);
    CHECK(sent.m_projectileTargetId == pSprite->GetId());
    CHECK(sent.m_height == 45);
    CHECK(sent.field_1E == 9);
    CHECK(sent.field_20 == 0);

    BYTE buffer[3 + 35] = {};
    BYTE* pData = buffer + CNetwork::SPEC_MSG_HEADER_LENGTH;
    DWORD dwSize = 0;
    sent.MarshalMessage(&pData, &dwSize);
    CHECK(dwSize == 35);

    CMessageSpellHitEffectSecondaryProjectile received(0, 0, CPoint(-1, -1), 0, 0, 0, 0);
    CHECK(received.UnmarshalMessage(buffer, dwSize + CNetwork::SPEC_MSG_HEADER_LENGTH) == TRUE);
    CHECK(received.m_targetId == pSprite->GetId());
    CHECK(received.m_projectileType == sent.m_projectileType);
    CHECK(received.m_projectileTargetId == sent.m_projectileTargetId);
    CHECK(received.m_projectileTarget == ptTarget);
    CHECK(received.m_height == sent.m_height);
    CHECK(received.field_1E == sent.field_1E);
    CHECK(received.field_20 == sent.field_20);

    pRemotes->m_pArray[1] = pSavedEntry;
    delete pEntry;
    pSprite->m_remotePlayerID = nSavedPlayer;
    pSprite->m_remoteObjectID = nSavedObject;
}

TEST_CASE("projectile: the secondary spell-hit message Run fires its decoded projectile", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    REQUIRE(pSprite->GetArea() != NULL);

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    const SHORT nNextObjectIdBefore = pArray->m_nNextObjectId;
    auto objectListCount = [pSprite]() {
        CGameArea* pArea = pSprite->GetArea();
        return pArea->m_lVertSortAdd.GetCount() + pArea->m_lVertSortBackAdd.GetCount()
            + pArea->m_lVertSortFlightAdd.GetCount();
    };
    const INT nObjectsBefore = objectListCount();

    // The ctor stores 0x1002, which DecodeProjectile routes to the spell-hit
    // table as type 1 (its own type then reads 0x1001).
    CMessageSpellHitEffectSecondaryProjectile message(0x1001,
        pSprite->GetId(),
        CPoint(300, 400),
        0,
        pSprite->GetId(),
        pSprite->GetId(),
        0);
    message.Run();

    // CProjectileSpellHit::Fire adds the decoded projectile to the object
    // array and to the area's flight list.
    CHECK(pArray->m_nNextObjectId == nNextObjectIdBefore + 1);
    CHECK(objectListCount() == nObjectsBefore + 1);

    CProjectile* pFired = NULL;
    for (SHORT n = 0; n <= pArray->m_nMaxArrayIndex; n++) {
        const int nObjectId = pArray->m_pArray[n].m_objectId;
        if (nObjectId < static_cast<int>(nNextObjectIdBefore)
            || nObjectId >= static_cast<int>(pArray->m_nNextObjectId)) {
            continue;
        }
        CGameObject* pObject = NULL;
        const LONG nId = static_cast<LONG>(nObjectId) | (static_cast<LONG>(n) << 16);
        if (pArray->GetShare(nId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE)
            == CGameObjectArray::SUCCESS) {
            pArray->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            pFired = dynamic_cast<CProjectile*>(pObject);
        }
    }
    REQUIRE(pFired != NULL);
    CHECK(pFired->m_projectileType == 0x1001);
    pFired->RemoveSelf();

    ClearMessages();
}
