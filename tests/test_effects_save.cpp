#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameEffectList.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSpell.h"
#include "CVariableHash.h"
#include "Icewind586B70.h"
#include "IcewindCGameEffects.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "msvc_rand.h"
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
#include <set>
#include <string>
#include <thread>
#include <vector>

// Saving throws (CGameEffect::CheckSave 0x4A42F0 and CGameEffectDamage's
// override 0x4A9E30), the effects' add hooks (OnAdd 0x4A3FF0 and the
// OnAddSpecific overrides) and Slippery Mind's second save (0x4A5EF0), on a
// party member standing in AR1000 and on creatures made from the game's CRE
// files. Every engine works on a private view of the install
// (tests/data_view.h). The rand counts are pinned with the VC6 generator's
// state (compat/msvc_rand.h).

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

// The engine as iwd2r_main builds it (tests/test_effects_iwd.cpp's).
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
// panels loaded (tests/test_effects_iwd.cpp's NewGameInStartArea).
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

std::vector<CMessage*> Messages()
{
    std::vector<CMessage*> messages;
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        messages.push_back(handler.m_messageList.GetNext(pos));
    }
    return messages;
}

// The queued trigger messages, in order: (trigger, caller, target, specific).
struct TriggerSent {
    SHORT id;
    LONG caller;
    LONG target;
    LONG specific;
};

std::vector<TriggerSent> Triggers()
{
    std::vector<TriggerSent> sent;
    for (CMessage* pMessage : Messages()) {
        CMessageSetTrigger* pTrigger = dynamic_cast<CMessageSetTrigger*>(pMessage);
        if (pTrigger != nullptr) {
            sent.push_back({ pTrigger->m_trigger.m_triggerID,
                pTrigger->GetSource(),
                pTrigger->GetTarget(),
                pTrigger->m_trigger.m_specificID });
        }
    }
    return sent;
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

std::vector<BYTE> LoadCreature(const char* resRef)
{
    CCreatureFile file;
    file.SetResRef(CResRef(resRef), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

// A goblin from 00GOB.CRE, in the object array but in no area.
CGameSprite* MakeGoblin()
{
    std::vector<BYTE> bytes = LoadCreature("00GOB");
    REQUIRE(!bytes.empty());
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

std::unique_ptr<CGameEffect> Decode(WORD effectID, DWORD dwFlags, LONG effectAmount, LONG sourceID)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    effect.dwFlags = dwFlags;
    effect.effectAmount = effectAmount;
    return std::unique_ptr<CGameEffect>(CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), sourceID, CPoint(-1, -1)));
}

// A sprite whose only save modifiers are its three save bonuses: a human
// with the party's allegiance (no difficulty penalty), no +5 flag, no monk
// levels and no specialisation.
void Neutralize(CGameSprite* pSprite)
{
    pSprite->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_PC;
    pSprite->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HUMAN;
    pSprite->m_typeAI.m_nSubRace = 0;
    pSprite->m_baseStats.field_2FB &= ~0x1;
    pSprite->m_baseStats.m_classLevels[CAIOBJECTTYPE_C_MONK - 1] = 0;
    pSprite->m_baseStats.m_specialization = 0;
    pSprite->m_derivedStats.m_nSaveVSFortitude = 0;
    pSprite->m_derivedStats.m_nSaveVSReflex = 0;
    pSprite->m_derivedStats.m_nSaveVSWill = 0;
    pSprite->m_derivedStats.m_nResistMagic = 0;
    pSprite->m_derivedStats.m_nMirrorImages = 0;
    pSprite->m_derivedStats.m_nStoneSkins = 0;
}

// The four save rolls as the sprite keeps them (0x70F6..0x70F9).
struct Rolls {
    BYTE fortitude;
    BYTE reflex;
    BYTE will;
    BYTE resistMagic;
};

BOOL Check(CGameEffect* pEffect, CGameSprite* pSprite, Rolls& rolls)
{
    return pEffect->CheckSave(pSprite, &rolls.fortitude, &rolls.reflex, &rolls.will, &rolls.resistMagic);
}

unsigned int Advance(unsigned int state, int draws)
{
    for (int i = 0; i < draws; ++i) {
        state = state * 214013u + 2531011u;
    }
    return state;
}

// How many rand draws took the generator from before to after (-1 when more
// than 1000).
int Draws(unsigned int before, unsigned int after)
{
    for (int n = 0; n <= 1000; n++) {
        if (Advance(before, n) == after) {
            return n;
        }
    }
    return -1;
}

bool HasEffect(CGameEffectList& list, WORD effectID, const CResRef& sourceRes)
{
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* pEffect = list.GetNext(pos);
        if (pEffect->m_effectID == effectID && pEffect->m_sourceRes == sourceRes) {
            return true;
        }
    }
    return false;
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

}

TEST_CASE("CheckSave (0x4A42F0): roll plus bonus against 10 + modifier + level; 20 saves, 1 fails", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);
    pGoblin->m_derivedStats.m_nSaveVSFortitude = 3;
    pGoblin->m_derivedStats.m_nSaveVSReflex = 4;
    pGoblin->m_derivedStats.m_nSaveVSWill = 5;

    std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, CGameObjectArray::INVALID_INDEX);
    // No dwarf bonus: the bonus applies only without m_flags bit 2.
    pEffect->m_flags = 0x2;
    pEffect->m_saveMod = 2;
    pEffect->m_spellLevel = 1;

    iwd2r_srand(11);

    SECTION("no saving throw: the effect applies")
    {
        pEffect->m_savingThrow = 0;
        Rolls rolls { 1, 1, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        CHECK(rolls.fortitude == 1);
    }

    SECTION("fortitude: 10 + 3 makes 13, 9 + 3 does not; the roll is marked shown")
    {
        pEffect->m_savingThrow = 0x4;
        Rolls made { 10, 1, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, made) == FALSE);
        CHECK(made.fortitude == (10 | 0x80));
        // A marked roll counts by its low bits and stays marked.
        CHECK(Check(pEffect.get(), pGoblin, made) == FALSE);
        CHECK(made.fortitude == (10 | 0x80));

        Rolls failed { 9, 1, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, failed) == TRUE);
        CHECK(failed.fortitude == (9 | 0x80));
        CHECK(failed.reflex == 1);
        CHECK(failed.will == 1);
        CHECK(failed.resistMagic == 1);
    }

    SECTION("reflex and will use their own roll and bonus")
    {
        pEffect->m_savingThrow = 0x8;
        Rolls reflex { 20, 9, 20, 20 };
        CHECK(Check(pEffect.get(), pGoblin, reflex) == FALSE);
        reflex = { 20, 8, 20, 20 };
        CHECK(Check(pEffect.get(), pGoblin, reflex) == TRUE);

        pEffect->m_savingThrow = 0x10;
        Rolls will { 20, 20, 8, 20 };
        CHECK(Check(pEffect.get(), pGoblin, will) == FALSE);
        will = { 20, 20, 7, 20 };
        CHECK(Check(pEffect.get(), pGoblin, will) == TRUE);
    }

    SECTION("several save bits: the last (will) decides")
    {
        pEffect->m_savingThrow = 0x4 | 0x10;
        Rolls rolls { 20, 1, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        CHECK(rolls.fortitude == (20 | 0x80));
        CHECK(rolls.will == (1 | 0x80));
    }

    SECTION("a natural 20 always saves, a natural 1 always fails")
    {
        pEffect->m_savingThrow = 0x4;
        pGoblin->m_derivedStats.m_nSaveVSFortitude = -100;
        Rolls twenty { 20, 1, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, twenty) == FALSE);

        pGoblin->m_derivedStats.m_nSaveVSFortitude = 100;
        Rolls one { 1, 1, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, one) == TRUE);
    }

    SECTION("the racial, school and difficulty modifiers")
    {
        pEffect->m_savingThrow = 0x10;
        Rolls rolls { 1, 1, 5, 1 };
        // 5 + 5 = 10 < 13.
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);

        // An elf against enchantment: +2.
        pEffect->m_school = 4;
        pGoblin->m_typeAI.m_nRace = CAIOBJECTTYPE_R_ELF;
        rolls = { 1, 1, 6, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);

        // A drow (the creature's own m_typeAI): +2 more on will.
        pEffect->m_school = 0;
        pGoblin->m_typeAI.m_nSubRace = CAIOBJECTTYPE_SUBRACE_ELF_DROW;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        pGoblin->m_typeAI.m_nSubRace = 0;
        pGoblin->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HUMAN;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);

        // A dwarf without m_flags bit 2: +2.
        pEffect->m_flags = 0;
        pGoblin->m_typeAI.m_nRace = CAIOBJECTTYPE_R_DWARF;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        pEffect->m_flags = 0x2;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        pGoblin->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HUMAN;

        // The creature's +5 flag.
        pGoblin->m_baseStats.field_2FB |= 0x1;
        rolls = { 1, 1, 3, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        pGoblin->m_baseStats.field_2FB &= ~0x1;

        // Outside the party the first two difficulty levels take 4 and 2.
        CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
        const DWORD difficulty = pOptions->m_nDifficultyLevel;
        pGoblin->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_ENEMY;
        pOptions->m_nDifficultyLevel = 1;
        rolls = { 1, 1, 11, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        rolls = { 1, 1, 12, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        pOptions->m_nDifficultyLevel = 2;
        rolls = { 1, 1, 10, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        rolls = { 1, 1, 9, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        pOptions->m_nDifficultyLevel = 3;
        rolls = { 1, 1, 8, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        pOptions->m_nDifficultyLevel = difficulty;
    }

    // CheckSave rolls no dice of its own: the sprite's rolls come from
    // ProcessEffectList (0x72DE60).
    CHECK(iwd2r_rand_state() == 11);

    ClearMessages();
    delete pGoblin;
}

TEST_CASE("CheckSave (0x4A42F0): spell resistance, cutscenes and the poison guards", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);
    iwd2r_srand(5);

    SECTION("spell resistance: SR above caster level + penetration + d20 resists once shown")
    {
        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 3, 0, CGameObjectArray::INVALID_INDEX);
        pEffect->m_sourceFlags = 0x400;
        pEffect->m_flags = 0x1;
        pEffect->m_casterLevel = 10;
        pEffect->m_spellPenetrationBonus = 2;
        pGoblin->m_derivedStats.m_nResistMagic = 18;

        Rolls rolls { 20, 20, 20, 5 };
        // 10 + 2 + 5 = 17 < 18: resisted; the flags lose their low bits and the
        // roll is marked.
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        CHECK(pEffect->m_dwFlags == 0);
        CHECK(rolls.resistMagic == (5 | 0x80));

        // Resisted again with the marked roll: nothing else changes.
        pEffect->m_dwFlags = 3;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        CHECK(pEffect->m_dwFlags == 3);

        // 17 against 17 goes through to the saves (none: applies).
        pGoblin->m_derivedStats.m_nResistMagic = 17;
        rolls = { 20, 20, 20, 5 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);

        // Bit 2 of m_flags (or no bit 1) skips the check.
        pGoblin->m_derivedStats.m_nResistMagic = 100;
        pEffect->m_flags = 0x3;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        pEffect->m_flags = 0;
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);

        // Effect 451 halves the resistance.
        std::unique_ptr<CGameEffect> pHalf = Decode(451, 0, 0, CGameObjectArray::INVALID_INDEX);
        pHalf->m_sourceFlags = 0x400;
        pHalf->m_flags = 0x1;
        pHalf->m_casterLevel = 10;
        pGoblin->m_derivedStats.m_nResistMagic = 32;
        rolls = { 20, 20, 20, 6 };
        CHECK(Check(pHalf.get(), pGoblin, rolls) == TRUE);
        pGoblin->m_derivedStats.m_nResistMagic = 34;
        rolls = { 20, 20, 20, 6 };
        CHECK(Check(pHalf.get(), pGoblin, rolls) == FALSE);
    }

    SECTION("a negative resistance always resists: the compare is unsigned (0x4A44AA)")
    {
        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, CGameObjectArray::INVALID_INDEX);
        pEffect->m_sourceFlags = 0x400;
        pEffect->m_flags = 0x1;
        pEffect->m_casterLevel = 30;
        pGoblin->m_derivedStats.m_nResistMagic = -5;
        Rolls rolls { 20, 20, 20, 20 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        pGoblin->m_derivedStats.m_nResistMagic = 0;
        rolls = { 20, 20, 20, 20 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
    }

    SECTION("in a cutscene only the creature's own effects, 104 and a forced death apply")
    {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        pGame->m_gameSave.m_cutScene = TRUE;
        Rolls rolls { 1, 1, 1, 1 };

        std::unique_ptr<CGameEffect> pOther = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, CGameObjectArray::INVALID_INDEX);
        CHECK(Check(pOther.get(), pGoblin, rolls) == FALSE);
        pOther->m_sourceID = pGoblin->GetId();
        CHECK(Check(pOther.get(), pGoblin, rolls) == TRUE);

        std::unique_ptr<CGameEffect> p104 = Decode(104, 0, 0, CGameObjectArray::INVALID_INDEX);
        CHECK(Check(p104.get(), pGoblin, rolls) == TRUE);

        std::unique_ptr<CGameEffect> pDeath = Decode(CGAMEEFFECT_DEATH, 0, 0, CGameObjectArray::INVALID_INDEX);
        CHECK(Check(pDeath.get(), pGoblin, rolls) == FALSE);
        static_cast<CGameEffectDeath*>(pDeath.get())->field_18C = 1;
        CHECK(Check(pDeath.get(), pGoblin, rolls) == TRUE);

        pGame->m_gameSave.m_cutScene = FALSE;
    }

    SECTION("poison and the poison icon do not get through mirror images or stone skins")
    {
        Rolls rolls { 1, 1, 1, 1 };
        std::unique_ptr<CGameEffect> pPoison = Decode(CGAMEEFFECT_POISON, 0, 1, CGameObjectArray::INVALID_INDEX);
        std::unique_ptr<CGameEffect> pIcon = Decode(CGAMEEFFECT_PORTRAITICON, 6, 0, CGameObjectArray::INVALID_INDEX);
        std::unique_ptr<CGameEffect> pOtherIcon = Decode(CGAMEEFFECT_PORTRAITICON, 7, 0, CGameObjectArray::INVALID_INDEX);
        CHECK(Check(pPoison.get(), pGoblin, rolls) == TRUE);

        pGoblin->m_derivedStats.m_nMirrorImages = 2;
        CHECK(Check(pPoison.get(), pGoblin, rolls) == FALSE);
        CHECK(Check(pIcon.get(), pGoblin, rolls) == FALSE);
        CHECK(Check(pOtherIcon.get(), pGoblin, rolls) == TRUE);
        // Only the images' low byte counts.
        pGoblin->m_derivedStats.m_nMirrorImages = 0x100;
        CHECK(Check(pPoison.get(), pGoblin, rolls) == TRUE);

        pGoblin->m_derivedStats.m_nMirrorImages = 0;
        pGoblin->m_derivedStats.m_nStoneSkins = 1;
        CHECK(Check(pPoison.get(), pGoblin, rolls) == FALSE);
    }

    CHECK(iwd2r_rand_state() == 5);
    ClearMessages();
    delete pGoblin;
}

TEST_CASE("CheckSave (0x4A42F0): poison and fear modifiers, a paladin's aura against SPWI420", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);

    // Snake Blood (+2) and Resist Poison (+4) go through the feat check
    // CGameSprite::sub_763200, whose cases for them are not ported yet (it
    // returns FALSE), so only the dwarf's bonus is tested here.
    SECTION("poison: a dwarf +2")
    {
        std::unique_ptr<CGameEffect> pPoison = Decode(CGAMEEFFECT_POISON, 0, 1, CGameObjectArray::INVALID_INDEX);
        pPoison->m_savingThrow = 0x4;
        pPoison->m_flags = 0x2;
        Rolls rolls { 9, 1, 1, 1 };
        CHECK(Check(pPoison.get(), pGoblin, rolls) == TRUE);
        pGoblin->m_typeAI.m_nRace = CAIOBJECTTYPE_R_DWARF;
        rolls = { 8, 1, 1, 1 };
        CHECK(Check(pPoison.get(), pGoblin, rolls) == FALSE);
    }

    SECTION("fear (24): a halfling +2 on will")
    {
        std::unique_ptr<CGameEffect> pPanic = Decode(CGAMEEFFECT_PANIC, 0, 0, CGameObjectArray::INVALID_INDEX);
        pPanic->m_savingThrow = 0x10;
        pPanic->m_flags = 0x2;
        Rolls rolls { 1, 1, 8, 1 };
        CHECK(Check(pPanic.get(), pGoblin, rolls) == TRUE);
        pGoblin->m_typeAI.m_nRace = CAIOBJECTTYPE_R_HALFLING;
        rolls = { 1, 1, 8, 1 };
        CHECK(Check(pPanic.get(), pGoblin, rolls) == FALSE);
    }

    SECTION("a party member near a living party paladin of level 2 gets +4 against SPWI420")
    {
        Neutralize(pProbe);
        pProbe->m_derivedStats.m_nClassLevels[CAIOBJECTTYPE_C_PALADIN - 1] = 2;
        pProbe->m_derivedStats.m_generalState &= ~STATE_DEAD;

        std::unique_ptr<CGameEffect> pFear = Decode(CGAMEEFFECT_PANIC, 0, 0, CGameObjectArray::INVALID_INDEX);
        pFear->m_savingThrow = 0x10;
        pFear->m_flags = 0x2;
        Rolls rolls { 1, 1, 6, 1 };
        CHECK(Check(pFear.get(), pProbe, rolls) == TRUE);
        pFear->m_sourceRes = CResRef("SPWI420");
        rolls = { 1, 1, 6, 1 };
        CHECK(Check(pFear.get(), pProbe, rolls) == FALSE);

        pProbe->m_derivedStats.m_nClassLevels[CAIOBJECTTYPE_C_PALADIN - 1] = 1;
        rolls = { 1, 1, 6, 1 };
        CHECK(Check(pFear.get(), pProbe, rolls) == TRUE);
        pProbe->m_derivedStats.m_nClassLevels[CAIOBJECTTYPE_C_PALADIN - 1] = 0;
    }

    ClearMessages();
    delete pGoblin;
}

TEST_CASE("CGameEffectDamage::CheckSave (0x4A9E30): mirror images, iron skins and half damage", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);
    ClearMessages();
    iwd2r_srand(3);

    std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, 0, 5, CGameObjectArray::INVALID_INDEX);

    SECTION("a mirror image takes the hit: one image fewer, on every image effect, shown")
    {
        CGameEffect* pTimed = Decode(CGAMEEFFECT_MIRRORIMAGE, 0, 3, CGameObjectArray::INVALID_INDEX).release();
        CGameEffect* pEquiped = Decode(CGAMEEFFECT_MIRRORIMAGE, 0, 3, CGameObjectArray::INVALID_INDEX).release();
        pGoblin->m_timedEffectList.AddTail(pTimed);
        pGoblin->m_equipedEffectList.AddTail(pEquiped);
        pGoblin->m_derivedStats.m_nMirrorImages = 3;

        Rolls rolls { 1, 1, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, rolls) == FALSE);
        CHECK(pGoblin->m_derivedStats.m_nMirrorImages == 2);
        CHECK(pTimed->m_effectAmount == 2);
        CHECK(pEquiped->m_effectAmount == 2);

        std::vector<CMessage*> messages = Messages();
        REQUIRE(messages.size() == 1);
        CMessageVisualEffect* pVisual = dynamic_cast<CMessageVisualEffect*>(messages[0]);
        REQUIRE(pVisual != nullptr);
        CHECK(pVisual->m_nEffectType == 3);
        // NOTE: Not m_nEffectProperty: CMessageVisualEffect's constructor in
        // CMessage.cpp assigns the parameter to itself (the original stores
        // it at +0xD, 0x4F6801), so the member is unset.
        CHECK(pVisual->GetTarget() == pGoblin->GetId());
    }

    SECTION("iron skins count down and then let the save decide")
    {
        CGameEffect* pSkins = Decode(CGAMEEFFECT_STONESKINS, 1, 2, CGameObjectArray::INVALID_INDEX).release();
        pGoblin->m_timedEffectList.AddTail(pSkins);
        pGoblin->m_derivedStats.m_nStoneSkins = 2;

        Rolls rolls { 1, 1, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, rolls) == TRUE);
        CHECK(pGoblin->m_derivedStats.m_nStoneSkins == 1);
        CHECK(pSkins->m_effectAmount == 1);

        // The last skin removes the effect.
        CHECK(Check(pDamage.get(), pGoblin, rolls) == TRUE);
        CHECK(pGoblin->m_derivedStats.m_nStoneSkins == 0);
        CHECK(!HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_STONESKINS, CResRef("")));

        std::vector<CMessage*> messages = Messages();
        REQUIRE(messages.size() == 2);
        CMessageVisualEffect* pVisual = dynamic_cast<CMessageVisualEffect*>(messages[1]);
        REQUIRE(pVisual != nullptr);
        CHECK(pVisual->m_nEffectType == 23);
    }

    SECTION("mode 3: a made save halves (the flags stay), a failed one takes it all")
    {
        pDamage->m_savingThrow = 0x4;
        pDamage->m_flags = 0x2;
        pGoblin->m_derivedStats.m_nSaveVSFortitude = 0;

        pDamage->m_dwFlags = 0x3;
        Rolls made { 15, 1, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, made) == TRUE);
        CHECK(pDamage->m_dwFlags == 0x3);

        Rolls failed { 2, 1, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, failed) == TRUE);
        CHECK(pDamage->m_dwFlags == 0x0);

        // Without mode 3 a made save refuses the damage.
        pDamage->m_dwFlags = 0x10000;
        made = { 15, 1, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, made) == FALSE);
        CHECK(pDamage->m_dwFlags == 0x10000);
    }

    SECTION("evasion avoids a made reflex save; improved evasion halves a failed one")
    {
        pDamage->m_savingThrow = 0x8;
        pDamage->m_flags = 0x2;
        pDamage->m_dwFlags = 0x3;
        // Rogue 10: evasion, and improved evasion's feat check.
        pGoblin->m_baseStats.m_classLevels[CAIOBJECTTYPE_C_ROGUE - 1] = 10;
        // No armour.
        CItem* pArmor = pGoblin->m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR];
        pGoblin->m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR] = NULL;
        pGoblin->m_derivedStats.m_generalState &= ~(STATE_DEAD | STATE_HELPLESS | STATE_STUNNED | STATE_SLEEPING);
        pGoblin->m_nSequence = CGameSprite::SEQ_READY;

        Rolls made { 1, 15, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, made) == FALSE);
        CHECK(pDamage->m_dwFlags == 0x3);

        Rolls failed { 1, 2, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, failed) == TRUE);
        CHECK(pDamage->m_dwFlags == 0x0);

        // Improved evasion halves even a failed save.
        pDamage->m_dwFlags = 0x3;
        pGoblin->m_baseStats.m_feats[CGAMESPRITE_FEAT_IMPROVED_EVASION >> 5] |= 1 << (CGAMESPRITE_FEAT_IMPROVED_EVASION & 0x1F);
        failed = { 1, 2, 1, 1 };
        CHECK(Check(pDamage.get(), pGoblin, failed) == TRUE);
        CHECK(pDamage->m_dwFlags == 0x3);

        pGoblin->m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR] = pArmor;
    }

    CHECK(iwd2r_rand_state() == 3);
    ClearMessages();
    delete pGoblin;
}

TEST_CASE("OnAdd (0x4A3FF0) and Damage's OnAddSpecific (0x4A9B50) tell the target who attacked it", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();

    SECTION("a hostile effect from the party: AttackedBy, and the target's hostility bit")
    {
        pGoblin->field_58C &= ~0x200000;
        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, pProbe->GetId());
        pEffect->m_sourceFlags = 0x400;
        pEffect->sub_4A3FF0(pGoblin);

        CHECK((pGoblin->field_58C & 0x200000) != 0);
        std::vector<TriggerSent> sent = Triggers();
        REQUIRE(sent.size() == 1);
        CHECK(sent[0].id == CAITRIGGER_ATTACKEDBY);
        CHECK(sent[0].caller == pProbe->GetId());
        CHECK(sent[0].target == pGoblin->GetId());
    }

    SECTION("hostile damage: HitBy and AttackedBy, then the damage's own pair with the damage type")
    {
        std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, 0x00100000, 5, pProbe->GetId());
        pDamage->m_sourceFlags = 0x400;
        pDamage->sub_4A3FF0(pGoblin);

        std::vector<TriggerSent> sent = Triggers();
        REQUIRE(sent.size() == 4);
        CHECK(sent[0].id == CAITRIGGER_HITBY);
        CHECK(sent[0].specific == 0);
        CHECK(sent[1].id == CAITRIGGER_ATTACKEDBY);
        CHECK(sent[2].id == CAITRIGGER_HITBY);
        CHECK(sent[2].specific == 0x10);
        CHECK(sent[3].id == CAITRIGGER_ATTACKEDBY);
        for (const TriggerSent& one : sent) {
            CHECK(one.caller == pProbe->GetId());
            CHECK(one.target == pGoblin->GetId());
        }
    }

    SECTION("the target's teams become the party's enemies (CGameSprite::sub_6F50A0)")
    {
        // Teams 0 and 2.
        pGoblin->field_58C = 0x5;
        CVariableHash* pVariables = g_pBaldurChitin->GetObjectGame()->GetVariables();
        CVariable* pTeam0 = pVariables->FindKey(CString("TEAM_0_ENEMY"));
        if (pTeam0 != NULL) {
            pTeam0->m_intValue = 0;
        }

        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, pProbe->GetId());
        pEffect->m_sourceFlags = 0x400;
        pEffect->sub_4A3FF0(pGoblin);

        CHECK(pGoblin->field_58C == (0x5 | 0x200000));
        pTeam0 = pVariables->FindKey(CString("TEAM_0_ENEMY"));
        REQUIRE(pTeam0 != NULL);
        CHECK(pTeam0->m_intValue == 1);
        CVariable* pTeam2 = pVariables->FindKey(CString("TEAM_2_ENEMY"));
        REQUIRE(pTeam2 != NULL);
        CHECK(pTeam2->m_intValue == 1);
    }

    SECTION("an enemy source leaves the party member's hostility bit alone")
    {
        pProbe->field_58C &= ~0x200000;
        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, pGoblin->GetId());
        pEffect->m_sourceFlags = 0x400;
        pEffect->sub_4A3FF0(pProbe);
        CHECK((pProbe->field_58C & 0x200000) == 0);
        CHECK(Triggers().size() == 1);
    }

    SECTION("no hostile flag or no source: nothing sent")
    {
        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, pProbe->GetId());
        pEffect->sub_4A3FF0(pGoblin);
        pEffect->m_sourceFlags = 0x400;
        pEffect->m_sourceID = CGameObjectArray::INVALID_INDEX;
        pEffect->sub_4A3FF0(pGoblin);
        CHECK(Messages().empty());
    }

    ClearMessages();
    delete pGoblin;
}

TEST_CASE("CheckAdd (0x4A3310) refuses a hostile effect on a flagged creature but sends the triggers", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();
    pGoblin->m_baseStats.m_flags |= 0x20000;

    std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, pProbe->GetId());
    pEffect->m_sourceFlags = 0x400;
    BYTE roll1 = 10;
    BYTE roll2 = 10;
    BYTE roll3 = 10;
    BYTE roll4 = 10;
    BYTE probability = 50;

    iwd2r_srand(9);
    CHECK(pEffect->sub_4A3310(pGoblin, &roll1, &roll2, &roll3, &roll4, &probability) == FALSE);
    // It returns before the discarded draw at 0x4A38A6.
    CHECK(iwd2r_rand_state() == 9);

    std::vector<TriggerSent> sent = Triggers();
    REQUIRE(sent.size() == 2);
    CHECK(sent[0].id == CAITRIGGER_HITBY);
    CHECK(sent[1].id == CAITRIGGER_ATTACKEDBY);
    CHECK(sent[0].caller == pProbe->GetId());
    CHECK(sent[0].target == pGoblin->GetId());

    ClearMessages();
    delete pGoblin;
}

TEST_CASE("Death's OnAddSpecific (0x4AD3C0): Killed, Die, Died and the _DEAD global", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    SECTION("a creature killed by the party member, in no area")
    {
        strcpy(reinterpret_cast<char*>(pGoblin->m_scriptName), "TESTGOB");
        pGoblin->m_baseStats.m_generalState &= ~STATE_DEAD;
        pGoblin->m_baseStats.m_xpValue = 35;
        CString sName = CGameAIBase::DEAD_GLOBAL_PREFIX + "TESTGOB";
        REQUIRE(pGame->GetVariables()->FindKey(sName) == NULL);

        std::unique_ptr<CGameEffect> pDeath = Decode(CGAMEEFFECT_DEATH, 0, 0, pProbe->GetId());
        pDeath->OnAddSpecific(pGoblin);

        std::vector<TriggerSent> sent = Triggers();
        REQUIRE(sent.size() == 2);
        CHECK(sent[0].id == CAITRIGGER_KILLED);
        CHECK(sent[0].caller == pGoblin->GetId());
        CHECK(sent[0].target == pProbe->GetId());
        CHECK(sent[0].specific == 35);
        CHECK(sent[1].id == CAITRIGGER_DIE);
        CHECK(sent[1].caller == pProbe->GetId());
        CHECK(sent[1].target == pGoblin->GetId());

        // The first death adds the global with 1 and sends it as a value.
        CVariable* pVariable = pGame->GetVariables()->FindKey(sName);
        REQUIRE(pVariable != NULL);
        CHECK(pVariable->m_intValue == 1);
        CMessageSetVariable* pSet = nullptr;
        for (CMessage* pMessage : Messages()) {
            if (dynamic_cast<CMessageSetVariable*>(pMessage) != nullptr) {
                pSet = static_cast<CMessageSetVariable*>(pMessage);
            }
        }
        REQUIRE(pSet != nullptr);
        CHECK(pSet->m_sName == sName);
        CHECK(pSet->m_sScope == "GLOBAL");
        CHECK(pSet->m_nValue == 1);
        CHECK(pSet->m_bIncrement == FALSE);

        // A later death sends an increment instead.
        ClearMessages();
        pGoblin->m_baseStats.m_generalState |= STATE_DEAD;
        pDeath->OnAddSpecific(pGoblin);
        sent = Triggers();
        REQUIRE(sent.size() == 1);
        CHECK(sent[0].id == CAITRIGGER_DIE);
        pSet = nullptr;
        for (CMessage* pMessage : Messages()) {
            if (dynamic_cast<CMessageSetVariable*>(pMessage) != nullptr) {
                pSet = static_cast<CMessageSetVariable*>(pMessage);
            }
        }
        REQUIRE(pSet != nullptr);
        CHECK(pSet->m_bIncrement == TRUE);
        CHECK(pGame->GetVariables()->FindKey(sName)->m_intValue == 1);
    }

    SECTION("a party member in AR1000: Died to everyone in help range, 0x88 to the party")
    {
        CTypedPtrList<CPtrList, LONG*> targets;
        pProbe->GetArea()->GetAllInRange(pProbe->GetPos(),
            CAIObjectType::ANYONE,
            pProbe->GetHelpRange(),
            pProbe->GetTerrainTable(),
            targets,
            FALSE,
            FALSE);
        std::multiset<LONG> expected;
        POSITION pos = targets.GetHeadPosition();
        while (pos != NULL) {
            expected.insert(static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos))));
        }

        std::unique_ptr<CGameEffect> pDeath = Decode(CGAMEEFFECT_DEATH, 0, 0, CGameObjectArray::INVALID_INDEX);
        pDeath->OnAddSpecific(pProbe);

        std::vector<TriggerSent> sent = Triggers();
        REQUIRE(!sent.empty());
        CHECK(sent[0].id == CAITRIGGER_DIE);
        CHECK(sent[0].caller == pProbe->GetId());
        CHECK(sent[0].target == pProbe->GetId());

        std::multiset<LONG> died;
        std::vector<LONG> party;
        for (const TriggerSent& one : sent) {
            if (one.id == CAITRIGGER_DIED) {
                CHECK(one.caller == pProbe->GetId());
                died.insert(one.target);
            } else if (one.id == 0x88) {
                CHECK(one.caller == pProbe->GetId());
                party.push_back(one.target);
            }
        }
        CHECK(died == expected);
        REQUIRE(party.size() == static_cast<size_t>(pGame->m_nCharacters));
        CHECK(party[0] == pProbe->GetId());
    }

    ClearMessages();
    delete pGoblin;
}

TEST_CASE("OnAddSpecific: regeneration caps its time (0x4B65B0), charm forgets its allegiance (0x4A6220)", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    const ULONG now = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;

    SECTION("mode 0: at most m_effectAmount seconds")
    {
        std::unique_ptr<CGameEffect> pRegen = Decode(CGAMEEFFECT_REGENERATION, 0, 10, CGameObjectArray::INVALID_INDEX);
        pRegen->m_duration = now + 1000;
        pRegen->OnAddSpecific(pGoblin);
        CHECK(pRegen->m_duration == now + 150);
        pRegen->m_duration = now + 100;
        pRegen->OnAddSpecific(pGoblin);
        CHECK(pRegen->m_duration == now + 100);
    }

    SECTION("mode 1: at most m_effectAmount percent of the hit points")
    {
        std::unique_ptr<CGameEffect> pRegen = Decode(CGAMEEFFECT_REGENERATION, 1, 50, CGameObjectArray::INVALID_INDEX);
        pGoblin->m_baseStats.m_hitPoints = 40;
        pRegen->m_duration = now + 1000;
        pRegen->OnAddSpecific(pGoblin);
        CHECK(pRegen->m_duration == now + 20);
    }

    SECTION("other modes keep the time")
    {
        std::unique_ptr<CGameEffect> pRegen = Decode(CGAMEEFFECT_REGENERATION, 2, 50, CGameObjectArray::INVALID_INDEX);
        pRegen->m_duration = now + 1000;
        pRegen->OnAddSpecific(pGoblin);
        CHECK(pRegen->m_duration == now + 1000);
    }

    SECTION("charm and dominate start over")
    {
        std::unique_ptr<CGameEffect> pCharm = Decode(CGAMEEFFECT_CHARM, 0, 0, CGameObjectArray::INVALID_INDEX);
        pCharm->m_numDice = 4;
        pCharm->m_diceSize = 1;
        pCharm->OnAddSpecific(pGoblin);
        CHECK(pCharm->m_numDice == static_cast<DWORD>(-1));
        CHECK(pCharm->m_diceSize == static_cast<DWORD>(-1));

        std::unique_ptr<CGameEffect> pDominate = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, CGameObjectArray::INVALID_INDEX);
        pDominate->m_numDice = 4;
        pDominate->OnAddSpecific(pGoblin);
        CHECK(pDominate->m_numDice == static_cast<DWORD>(-1));
    }

    ClearMessages();
    delete pGoblin;
}

// Dominate (opcode 263, 0x5645A0): the first pass takes the source's
// allegiance (or the stored one on later passes), a non-zero effect amount
// gates the target's general type, a PC source goes through the summon
// registry and queues the ATTACKREEVALUATE action, and the five-way switch
// on m_dwFlags (exe +0x1c) checks m_durationType (exe +0x20) in every arm.
// Its source-success arm also posts the CMessage95 target/source pair
// (vtable 0x84D37C); the first-pass section checks its exact payload.
TEST_CASE("Dominate (0x5645A0): the first pass takes the source's allegiance and the later passes replay it", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Icewind586B70* pTable = Icewind586B70::Instance();

    // pProbe is shared by every case in this file. Snapshot the bytes and
    // states this effect can change and put them back exactly.
    const BYTE probeEnemyAlly = pProbe->m_typeAI.m_nEnemyAlly;
    const BYTE probeSubRace = pProbe->m_typeAI.m_nSubRace;
    const BYTE probeLiveEnemyAlly = pProbe->m_liveTypeAI.m_nEnemyAlly;
    const BYTE probeLiveSubRace = pProbe->m_liveTypeAI.m_nSubRace;
    const BYTE probeStartEnemyAlly = pProbe->m_startTypeAI.m_nEnemyAlly;
    const BYTE probeStartSubRace = pProbe->m_startTypeAI.m_nSubRace;
    const DWORD probeDerived = pProbe->m_derivedStats.m_generalState;
    const DWORD probeBase = pProbe->m_baseStats.m_generalState;

    const CAIObjectType before(pGoblin->GetAIType());
    const CAIObjectType beforeLive(pGoblin->m_liveTypeAI);
    const CAIObjectType beforeStart(pGoblin->m_startTypeAI);
    const DWORD beforeDerived = pGoblin->m_derivedStats.m_generalState;
    const DWORD beforeBase = pGoblin->m_baseStats.m_generalState;
    const BOOL beforeInterrupt = pGoblin->m_interrupt;

    auto resetProbe = [&]() {
        pProbe->m_typeAI.m_nEnemyAlly = probeEnemyAlly;
        pProbe->m_typeAI.m_nSubRace = probeSubRace;
        pProbe->m_liveTypeAI.m_nEnemyAlly = probeLiveEnemyAlly;
        pProbe->m_liveTypeAI.m_nSubRace = probeLiveSubRace;
        pProbe->m_startTypeAI.m_nEnemyAlly = probeStartEnemyAlly;
        pProbe->m_startTypeAI.m_nSubRace = probeStartSubRace;
        pProbe->m_derivedStats.m_generalState = probeDerived;
        pProbe->m_baseStats.m_generalState = probeBase;
    };

    auto resetTarget = [&]() {
        pGoblin->m_typeAI.Set(before);
        pGoblin->m_liveTypeAI.Set(beforeLive);
        pGoblin->m_startTypeAI.Set(beforeStart);
        pGoblin->m_derivedStats.m_generalState = beforeDerived;
        pGoblin->m_baseStats.m_generalState = beforeBase;
        pGoblin->m_interrupt = beforeInterrupt;
        while (!pGoblin->m_queuedActions.IsEmpty()) {
            delete pGoblin->m_queuedActions.RemoveHead();
        }
    };

    auto cleanup = [&]() {
        resetProbe();
        resetTarget();
        pTable->Clear();
        ClearMessages();
    };

    SECTION("decodes to the dominate class and copies as one")
    {
        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, CGameObjectArray::INVALID_INDEX);
        REQUIRE(dynamic_cast<IcewindCGameEffectDominate*>(pEffect.get()) != nullptr);
        std::unique_ptr<CGameEffect> pCopy(pEffect->Copy());
        REQUIRE(dynamic_cast<IcewindCGameEffectDominate*>(pCopy.get()) != nullptr);
    }

    SECTION("a non-zero effect amount only dominates its general type")
    {
        pGoblin->m_typeAI.m_nGeneral = 1;

        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 2, CGameObjectArray::INVALID_INDEX);
        pEffect->m_durationType = 0;
        pEffect->OnAddSpecific(pGoblin);
        CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CHECK(pEffect->m_numDice == static_cast<DWORD>(-1));
        CHECK(pEffect->m_diceSize == static_cast<DWORD>(-1));
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == before.m_nEnemyAlly);
        CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == beforeLive.m_nEnemyAlly);
        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
        CHECK(pGoblin->m_derivedStats.m_generalState == beforeDerived);
        CHECK(pGoblin->m_baseStats.m_generalState == beforeBase);
        CHECK(pGoblin->m_queuedActions.IsEmpty());
    }

    SECTION("the first pass from a PC source shares it, links it and queues the action")
    {
        resetProbe();
        pGoblin->m_typeAI.m_nGeneral = 1;
        pProbe->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_PC;

        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, pProbe->GetId());
        pEffect->m_durationType = 0;
        pEffect->OnAddSpecific(pGoblin);
        CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

        // The source is a PC (not an enemy), so the pre-switch allegiance is
        // zero; flags 0 turns it into EA_0x847C3A. The goblin has no
        // portrait, so this is not a party charm.
        CHECK(pEffect->m_done == FALSE);
        CHECK(pEffect->m_numDice == CAIObjectType::EA_0x847C3A);
        CHECK(pEffect->m_diceSize == 0);
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_0x847C3A);
        CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == CAIObjectType::EA_0x847C3A);
        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) != 0);
        CHECK((pGoblin->m_baseStats.m_generalState & STATE_CHARMED) == 0);
        CHECK(pGoblin->m_interrupt == TRUE);

        CHECK(pTable->mEntries[0].field_0 == pGoblin->GetId());
        CHECK(pTable->mEntries[0].field_4 == pProbe->GetId());
        CHECK(pTable->mEntries[0].field_8 == true);

        REQUIRE(!pGoblin->m_queuedActions.IsEmpty());
        CAIAction* pAction = pGoblin->m_queuedActions.GetHead();
        REQUIRE(pAction != nullptr);
        CHECK(pAction->m_actionID == CAIAction::ATTACKREEVALUATE);
        CHECK(pAction->m_acteeID.m_nEnemyAlly == 0);
        CHECK(pAction->m_specificID == 45);
        CHECK(pAction->m_specificID2 == -1);
        CHECK(pAction->m_specificID3 == 0);
        CHECK(pAction->m_string1 == "");
        CHECK(pAction->m_string2 == "");
        CHECK(pAction->m_dest == CPoint(-1, -1));
        CHECK(pAction->m_internalFlags == 0);

        std::vector<CMessage95*> links;
        for (CMessage* pCurrent : Messages()) {
            if (CMessage95* pLink = dynamic_cast<CMessage95*>(pCurrent)) {
                links.push_back(pLink);
            }
        }
        REQUIRE(links.size() == 1);
        CMessage95* pMessage = links[0];
        REQUIRE(pMessage != nullptr);
        CHECK(pMessage->m_link1Id == pGoblin->GetId());
        CHECK(pMessage->m_link2Id == pProbe->GetId());
        CHECK(pMessage->GetSource() == pGoblin->GetId());
        CHECK(pMessage->GetTarget() == pGoblin->GetId());
    }

    SECTION("an enemy source is remapped, but the action keeps the pre-switch type")
    {
        resetProbe();
        pGoblin->m_typeAI.m_nGeneral = 1;
        pProbe->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_ENEMY;

        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, pProbe->GetId());
        pEffect->m_durationType = 0;
        pEffect->OnAddSpecific(pGoblin);
        CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

        CHECK(pEffect->m_done == FALSE);
        CHECK(pEffect->m_numDice == CAIObjectType::EA_CHARMED_PC);
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_CHARMED_PC);
        CHECK(pTable->mEntries[0].field_0 == 0);

        REQUIRE(!pGoblin->m_queuedActions.IsEmpty());
        CHECK(pGoblin->m_queuedActions.GetHead()->m_acteeID.m_nEnemyAlly == CAIObjectType::EA_ENEMY);
    }

    SECTION("a full summon registry finishes the first pass without linking or changing the sprite")
    {
        resetProbe();
        pGoblin->m_typeAI.m_nGeneral = 1;
        pProbe->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_PC;
        for (int index = 0; index < 6; index++) {
            pTable->mEntries[index] = Icewind586B70::Entry(1000 + index, 2000 + index);
        }

        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, pProbe->GetId());
        pEffect->m_durationType = 0;
        pEffect->OnAddSpecific(pGoblin);
        CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

        CHECK(pEffect->m_done == TRUE);
        CHECK(pEffect->m_numDice == static_cast<DWORD>(-1));
        CHECK(pEffect->m_diceSize == static_cast<DWORD>(-1));
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == before.m_nEnemyAlly);
        CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == beforeLive.m_nEnemyAlly);
        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
        CHECK(pGoblin->m_derivedStats.m_generalState == beforeDerived);
        CHECK(pGoblin->m_baseStats.m_generalState == beforeBase);
        CHECK(pGoblin->m_queuedActions.IsEmpty());
    }

    SECTION("no source takes the flags defaults")
    {
        pGoblin->m_typeAI.m_nGeneral = 1;

        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, CGameObjectArray::INVALID_INDEX);
        pEffect->m_durationType = 0;
        pEffect->OnAddSpecific(pGoblin);
        CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

        CHECK(pEffect->m_done == FALSE);
        CHECK(pEffect->m_numDice == CAIObjectType::EA_0x847C3A);
        CHECK(pEffect->m_diceSize == 0);
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_0x847C3A);
        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) != 0);
        CHECK(pGoblin->m_queuedActions.IsEmpty());
    }

    SECTION("a stored temporary allegiance replays without a source")
    {
        pGoblin->m_typeAI.m_nGeneral = 1;

        std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 0, 0, CGameObjectArray::INVALID_INDEX);
        pEffect->m_durationType = 0;
        pEffect->OnAddSpecific(pGoblin);
        pEffect->m_numDice = CAIObjectType::EA_ENEMY;
        pEffect->m_diceSize = 1;
        CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

        CHECK(pEffect->m_done == FALSE);
        CHECK(pEffect->m_numDice == CAIObjectType::EA_CHARMED_PC);
        CHECK(pEffect->m_diceSize == 1);
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_CHARMED_PC);
        CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == CAIObjectType::EA_CHARMED_PC);
        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) != 0);
        CHECK((pGoblin->m_baseStats.m_generalState & STATE_CHARMED) == 0);
        CHECK(pGoblin->m_queuedActions.IsEmpty());
    }

    // Each jump-table arm, with m_duration deliberately opposite to the
    // duration type: an implementation that reads m_duration (+0x24) or uses
    // durationType as the switch index fails here.
    SECTION("each jump-table arm separates m_dwFlags from m_durationType and m_duration")
    {
        for (DWORD flags = 0; flags <= 4; flags++) {
            for (DWORD durationType = 0; durationType <= 1; durationType++) {
                resetTarget();
                pTable->Clear();
                ClearMessages();
                pGoblin->m_typeAI.m_nGeneral = 1;

                std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, flags, 0, CGameObjectArray::INVALID_INDEX);
                pEffect->m_durationType = durationType;
                pEffect->m_duration = durationType == 1 ? 0 : 1;
                pEffect->OnAddSpecific(pGoblin);
                pEffect->m_numDice = CAIObjectType::EA_ALLY;
                pEffect->m_diceSize = 0;
                CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

                INFO("flags=" << flags << " durationType=" << durationType);
                CHECK(pEffect->m_duration == (durationType == 1 ? 0 : 1));
                CHECK(pEffect->m_numDice == CAIObjectType::EA_ALLY);
                CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_ALLY);
                CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == CAIObjectType::EA_ALLY);

                if (durationType == 1) {
                    CHECK(pEffect->m_done == TRUE);
                    CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == CAIObjectType::EA_ALLY);
                    CHECK((pGoblin->m_baseStats.m_generalState & STATE_CHARMED) != 0);
                } else {
                    CHECK(pEffect->m_done == FALSE);
                    if (flags == 1 || flags == 3 || flags == 4) {
                        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == CAIObjectType::EA_EVILCUTOFF);
                    } else {
                        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
                    }
                    CHECK((pGoblin->m_baseStats.m_generalState & STATE_CHARMED) == 0);
                }

                CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) != 0);
            }
        }
    }

    SECTION("the zero default is 7 for flags 0 and 1, 5 for flags 2 to 4")
    {
        for (DWORD flags = 0; flags <= 4; flags++) {
            resetTarget();
            pTable->Clear();
            ClearMessages();
            pGoblin->m_typeAI.m_nGeneral = 1;

            std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, flags, 0, CGameObjectArray::INVALID_INDEX);
            pEffect->m_durationType = 0;
            pEffect->m_duration = 1;
            pEffect->OnAddSpecific(pGoblin);
            pEffect->m_numDice = 0;
            pEffect->m_diceSize = 0;
            CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

            const BYTE expected = flags <= 1 ? CAIObjectType::EA_0x847C3A : CAIObjectType::EA_CONTROLLED;
            INFO("flags=" << flags);
            CHECK(pEffect->m_duration == 1);
            CHECK(pEffect->m_done == FALSE);
            CHECK(pEffect->m_numDice == expected);
            CHECK(pGoblin->GetAIType().m_nEnemyAlly == expected);
            CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == expected);
            if (flags == 1 || flags == 3 || flags == 4) {
                CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == CAIObjectType::EA_EVILCUTOFF);
            } else {
                CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
            }
        }
    }

    SECTION("a party charm leaves the start type and m_done alone in flags 1, 3 and 4")
    {
        const DWORD arms[] = { 1, 3, 4 };
        for (DWORD flags : arms) {
            resetTarget();
            pTable->Clear();
            ClearMessages();
            pGoblin->m_typeAI.m_nGeneral = 1;

            std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, flags, 0, CGameObjectArray::INVALID_INDEX);
            pEffect->m_durationType = 0;
            pEffect->m_duration = 1;
            pEffect->OnAddSpecific(pGoblin);
            pEffect->m_numDice = CAIObjectType::EA_ENEMY;
            pEffect->m_diceSize = 1;
            CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

            INFO("flags=" << flags);
            CHECK(pEffect->m_done == FALSE);
            CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
            CHECK(pEffect->m_numDice == (flags == 1 ? CAIObjectType::EA_CHARMED_PC : CAIObjectType::EA_ENEMY));
        }
    }

    SECTION("a portrait also leaves the start type and m_done alone in flags 1, 3 and 4")
    {
        const DWORD arms[] = { 1, 3, 4 };
        for (DWORD flags : arms) {
            resetProbe();
            pProbe->m_typeAI.m_nGeneral = 1;

            std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, flags, 0, CGameObjectArray::INVALID_INDEX);
            pEffect->m_durationType = 0;
            pEffect->m_duration = 1;
            pEffect->OnAddSpecific(pProbe);
            pEffect->m_numDice = CAIObjectType::EA_ENEMY;
            pEffect->m_diceSize = 0;
            CHECK(pEffect->ApplyEffect(pProbe) == TRUE);

            INFO("flags=" << flags);
            CHECK(pEffect->m_done == FALSE);
            CHECK(pProbe->m_startTypeAI.m_nEnemyAlly == probeStartEnemyAlly);
            CHECK(pEffect->m_numDice == (flags == 1 ? CAIObjectType::EA_CHARMED_PC : CAIObjectType::EA_ENEMY));
        }
    }

    SECTION("flags above 4 skip the switch and the AI-type update, but still write the state")
    {
        for (DWORD durationType = 0; durationType <= 1; durationType++) {
            resetTarget();
            pTable->Clear();
            ClearMessages();
            pGoblin->m_typeAI.m_nGeneral = 1;

            std::unique_ptr<CGameEffect> pEffect = Decode(ICEWIND_CGAMEEFFECT_DOMINATE, 5, 0, CGameObjectArray::INVALID_INDEX);
            pEffect->m_durationType = durationType;
            pEffect->m_duration = durationType == 1 ? 0 : 1;
            pEffect->OnAddSpecific(pGoblin);
            pEffect->m_numDice = 0;
            pEffect->m_diceSize = 0;
            CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);

            INFO("durationType=" << durationType);
            CHECK(pEffect->m_duration == (durationType == 1 ? 0 : 1));
            CHECK(pEffect->m_numDice == 0);
            CHECK(pGoblin->GetAIType().m_nEnemyAlly == before.m_nEnemyAlly);
            CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == beforeLive.m_nEnemyAlly);
            CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == beforeStart.m_nEnemyAlly);
            CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) != 0);
            CHECK(((pGoblin->m_baseStats.m_generalState & STATE_CHARMED) != 0) == (durationType == 1));
        }
    }

    cleanup();
    delete pGoblin;
}

TEST_CASE("Slippery Mind's OnRemove (0x4A5EF0): a made save breaks the charm, a failed one keeps it", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);
    pGoblin->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_CHARMED;
    pGoblin->m_liveTypeAI.m_nEnemyAlly = CAIObjectType::EA_CHARMED;
    pGoblin->m_startTypeAI.m_nEnemyAlly = CAIObjectType::EA_CHARMED;
    pGoblin->m_derivedStats.m_generalState |= STATE_CHARMED;
    ClearMessages();

    std::unique_ptr<CGameEffect> pSlippery = Decode(ICEWIND_CGAMEEFFECT_SLIPPERYMIND, 0, 0, pGoblin->GetId());
    pSlippery->m_savingThrow = 0x10;
    pSlippery->m_flags = 0x2;
    pSlippery->m_effectAmount2 = CAIObjectType::EA_ENEMY;
    pSlippery->m_effectAmount3 = 201;
    pSlippery->m_effectAmount4 = 202;

    iwd2r_srand(21);

    SECTION("made: the old allegiances come back and the charm is removed")
    {
        pGoblin->field_70F8 = 20;
        pSlippery->OnRemove(pGoblin);

        CHECK(pSlippery->m_done == TRUE);
        CHECK(pGoblin->field_70F8 == 20);
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_ENEMY);
        CHECK(pGoblin->m_liveTypeAI.m_nEnemyAlly == 201);
        CHECK(pGoblin->m_startTypeAI.m_nEnemyAlly == 202);
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) == 0);

        bool removal = false;
        for (CMessage* pMessage : Messages()) {
            CMessageAddEffect* pAdd = dynamic_cast<CMessageAddEffect*>(pMessage);
            if (pAdd != nullptr && pAdd->m_effect->m_dwFlags == CGAMEEFFECT_CHARM) {
                removal = true;
            }
        }
        CHECK(removal);
    }

    SECTION("failed: the charm stays")
    {
        pGoblin->field_70F8 = 1;
        pSlippery->OnRemove(pGoblin);

        CHECK(pSlippery->m_done == TRUE);
        CHECK(pGoblin->field_70F8 == 1);
        CHECK(pGoblin->GetAIType().m_nEnemyAlly == CAIObjectType::EA_CHARMED);
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_CHARMED) != 0);
        for (CMessage* pMessage : Messages()) {
            CHECK(dynamic_cast<CMessageAddEffect*>(pMessage) == nullptr);
        }
    }

    CHECK(iwd2r_rand_state() == 21);
    ClearMessages();
    delete pGoblin;
}

// Hold Person (SPPR208) as the game casts it: its second effect, a hold
// (109) with a will save, from the cleric's spell through AddEffect
// (0x733050) onto a goblin. CheckAdd draws its one discarded number; the save
// itself rolls nothing (the sprite's will roll at 0x70F8 decides).
TEST_CASE("Hold Person (SPPR208) on a goblin: a made will save refuses the hold, a failed one holds it", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();

    CSpell spell { CResRef("SPPR208") };
    CGameEffect* pHold = spell.GetAbilityEffect(0, 1, pProbe, CAIOBJECTTYPE_C_CLERIC, 0, 5);
    REQUIRE(pHold != NULL);
    REQUIRE(pHold->m_effectID == CGAMEEFFECT_HOLDCREATURE);
    REQUIRE(pHold->m_savingThrow == 0x10);
    pHold->m_sourceID = pProbe->GetId();
    const CResRef source("SPPR208");

    pGoblin->field_70F6 = 10;
    pGoblin->field_70F7 = 10;
    pGoblin->field_70F9 = 10;
    pGoblin->field_70FA = 0;

    // The game running, as when a spell lands: with the world timer stopped
    // AddEffect also runs ProcessEffectList, which rolls the dice again.
    CTimerWorld* pTimer = g_pBaldurChitin->GetObjectGame()->GetWorldTimer();
    const BOOL active = pTimer->m_active;
    pTimer->m_active = TRUE;

    SECTION("a natural 20: saved, the hold is refused and the goblin not held")
    {
        pGoblin->field_70F8 = 20;
        iwd2r_srand(31);
        pGoblin->AddEffect(pHold, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
        CHECK(Draws(31, iwd2r_rand_state()) == 1);

        CHECK(!HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_HOLDCREATURE, source));
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_HELPLESS) == 0);
        CHECK(pGoblin->field_70F8 == (20 | 0x80));
    }

    SECTION("a natural 1: failed, the goblin is held")
    {
        pGoblin->field_70F8 = 1;
        iwd2r_srand(31);
        pGoblin->AddEffect(pHold, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
        CHECK(Draws(31, iwd2r_rand_state()) == 1);

        CHECK(HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_HOLDCREATURE, source));
        CHECK((pGoblin->m_derivedStats.m_generalState & STATE_HELPLESS) != 0);
        CHECK(pGoblin->m_derivedStats.m_spellStates.test(SPLSTATE_HELD));
        CHECK(pGoblin->field_70F8 == (1 | 0x80));

        // SPPR208's flags (0x4000) lack the hostile bit 0x400: OnAdd sends
        // no AttackedBy and leaves the hostility bit alone.
        CHECK(pHold->m_sourceFlags == 0x4000);
        for (const TriggerSent& one : Triggers()) {
            CHECK(one.id != CAITRIGGER_ATTACKEDBY);
        }
    }

    SECTION("a natural 1 with the hostile flag: held, and OnAdd tells the goblin who attacked it")
    {
        pHold->m_sourceFlags |= 0x400;
        pGoblin->field_58C = 0;
        pGoblin->field_70F8 = 1;
        iwd2r_srand(31);
        pGoblin->AddEffect(pHold, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
        CHECK(Draws(31, iwd2r_rand_state()) == 1);

        CHECK(HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_HOLDCREATURE, source));
        CHECK(pGoblin->field_58C == 0x200000);
        std::vector<TriggerSent> sent = Triggers();
        REQUIRE(sent.size() == 1);
        CHECK(sent[0].id == CAITRIGGER_ATTACKEDBY);
        CHECK(sent[0].caller == pProbe->GetId());
        CHECK(sent[0].target == pGoblin->GetId());
    }

    pTimer->m_active = static_cast<BOOLEAN>(active);
    ClearMessages();
    delete pGoblin;
}

namespace {

CString Token(const char* key)
{
    CString value;
    if (!g_pBaldurChitin->GetTlkTable().m_mapTokens.Lookup(CString(key), value)) {
        return CString("<unset>");
    }
    return value;
}

CString Text(STRREF strRef)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(strRef, strRes);
    return strRes.szText;
}

}

TEST_CASE("Damage's feedback line (0x4AA6D0) and its type name (0x4AB370)", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();
    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD level = pOptions->m_nEffectTextLevel;
    iwd2r_srand(41);

    SECTION("the damage type names")
    {
        const struct {
            DWORD type;
            STRREF name;
        } types[] = {
            { 0x0, 25023 },
            { 0x10000, 25027 },
            { 0x20000, 25026 },
            { 0x40000, 25025 },
            { 0x80000, 25024 },
            { 0x100000, 25022 },
            { 0x200000, 25018 },
            { 0x400000, 25019 },
            { 0x1000000, 25021 },
            { 0x2000000, 25022 },
            { 0x4000000, 25023 },
            { 0x8000000, 25023 },
            { 0x10000000, 25019 },
            { 0x20000000, 10912 },
            { 0x40000000, 10911 },
        };
        for (const auto& one : types) {
            INFO("type " << one.type);
            std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, one.type, 5, CGameObjectArray::INVALID_INDEX);
            CHECK(static_cast<CGameEffectDamage*>(pDamage.get())->sub_4AB370() == Text(one.name));
        }
    }

    SECTION("damage from the party member: AMOUNT, RESISTED, TYPE and DAMAGER")
    {
        pOptions->m_nEffectTextLevel |= 0x2;
        std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, 0x10000, 7, pProbe->GetId());
        static_cast<CGameEffectDamage*>(pDamage.get())->sub_4AA6D0(pGoblin, -3);
        CHECK(Token("AMOUNT") == "7");
        CHECK(Token("RESISTED") == "3");
        CHECK(Token("TYPE") == Text(25027));
        CHECK(Token("DAMAGER") == pProbe->GetName());
    }

    SECTION("without the option's bit 2 a hit says nothing")
    {
        pOptions->m_nEffectTextLevel &= ~0x2;
        g_pBaldurChitin->GetTlkTable().SetToken(CString("AMOUNT"), CString("none"));
        std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, 0, 7, pProbe->GetId());
        static_cast<CGameEffectDamage*>(pDamage.get())->sub_4AA6D0(pGoblin, 0);
        CHECK(Token("AMOUNT") == "none");
    }

    SECTION("no damage from a named source: DAMAGEE and TYPE")
    {
        pOptions->m_nEffectTextLevel &= ~0x2;
        std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, 0x20000, 0, pProbe->GetId());
        static_cast<CGameEffectDamage*>(pDamage.get())->sub_4AA6D0(pGoblin, 0);
        CHECK(Token("DAMAGEE") == pGoblin->GetName());
        CHECK(Token("TYPE") == Text(25026));
    }

    pOptions->m_nEffectTextLevel = level;
    CHECK(iwd2r_rand_state() == 41);
    ClearMessages();
    delete pGoblin;
}

TEST_CASE("Calm (0x4A5E50) removes berserk from both effect lists", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();

    CGameEffect* pTimed = Decode(CGAMEEFFECT_BERSERK, 0, 0, CGameObjectArray::INVALID_INDEX).release();
    CGameEffect* pEquiped = Decode(CGAMEEFFECT_BERSERK, 0, 0, CGameObjectArray::INVALID_INDEX).release();
    CGameEffect* pIcon = Decode(CGAMEEFFECT_PORTRAITICON, 4, 4, CGameObjectArray::INVALID_INDEX).release();
    pGoblin->m_timedEffectList.AddTail(pTimed);
    pGoblin->m_equipedEffectList.AddTail(pEquiped);
    pGoblin->m_timedEffectList.AddTail(pIcon);
    pGoblin->m_baseStats.m_generalState |= STATE_BERSERK;
    pGoblin->m_derivedStats.m_generalState |= STATE_BERSERK;

    std::unique_ptr<CGameEffect> pCalm = Decode(CGAMEEFFECT_CALM, 0, 0, CGameObjectArray::INVALID_INDEX);
    CHECK(pCalm->ApplyEffect(pGoblin) == TRUE);
    CHECK(pCalm->m_done == TRUE);
    CHECK((pGoblin->m_baseStats.m_generalState & STATE_BERSERK) == 0);
    CHECK((pGoblin->m_derivedStats.m_generalState & STATE_BERSERK) == 0);
    CHECK(!HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_BERSERK, CResRef("")));
    CHECK(!HasEffect(pGoblin->m_equipedEffectList, CGAMEEFFECT_BERSERK, CResRef("")));
    CHECK(!HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_PORTRAITICON, CResRef("")));

    ClearMessages();
    delete pGoblin;
}

// Replace Self (151). The vanilla spells that use it (SPWI961: GDOPP71,
// SPIN150: EMMRCHD, SPWI983: PHEOGURD, SPIN176: DEDDOG, ...) name creatures
// CHITIN.KEY does not have, so in the game it only ends; with a creature
// that exists (00GOB here), on a goblin standing in AR1000 with p2 0 (no
// death), the goblin leaves the area and the new one takes its place under
// its script name.
TEST_CASE("Replace self (0x4BD050) puts the new creature where the old one stood", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();
    CGameArea* pArea = pProbe->GetArea();
    REQUIRE(pArea != NULL);
    const CPoint pos(pProbe->GetPos().x + 40, pProbe->GetPos().y);
    pGoblin->AddToArea(pArea, pos, 0, CGameObject::LIST_FRONT);
    REQUIRE(pGoblin->GetArea() == pArea);
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();

    SECTION("a missing creature: done, nothing else")
    {
        std::unique_ptr<CGameEffect> pReplace = Decode(CGAMEEFFECT_REPLACESELF, 0, 0, pProbe->GetId());
        pReplace->m_res = CResRef("GDOPP71");
        const INT_PTR added = pArea->m_lVertSortAdd.GetCount();
        CHECK(pReplace->ApplyEffect(pGoblin) == TRUE);
        CHECK(pReplace->m_done == TRUE);
        CHECK(pGoblin->m_removeFromArea == FALSE);
        CHECK(pArea->m_lVertSortAdd.GetCount() == added);
    }

    SECTION("an existing creature replaces the goblin")
    {
        std::unique_ptr<CGameEffect> pReplace = Decode(CGAMEEFFECT_REPLACESELF, 0, 0, pProbe->GetId());
        pReplace->m_res = CResRef("00GOB");
        std::set<LONG> before;
        POSITION posAdd = pArea->m_lVertSortAdd.GetHeadPosition();
        while (posAdd != NULL) {
            before.insert(static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSortAdd.GetNext(posAdd))));
        }

        CHECK(pReplace->ApplyEffect(pGoblin) == TRUE);
        CHECK(pReplace->m_done == TRUE);
        CHECK(pGoblin->m_removeFromArea == TRUE);
        CHECK(!HasEffect(pGoblin->m_timedEffectList, CGAMEEFFECT_DEATH, CResRef("")));

        CGameSprite* pNew = nullptr;
        posAdd = pArea->m_lVertSortAdd.GetHeadPosition();
        while (posAdd != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSortAdd.GetNext(posAdd)));
            CGameObject* pObject;
            if (before.count(id) == 0
                && pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                pNew = dynamic_cast<CGameSprite*>(pObject);
            }
        }
        REQUIRE(pNew != nullptr);
        CHECK(pNew != pGoblin);
        CHECK(pNew->m_resRef == CResRef("00GOB"));
        CHECK(pNew->GetArea() == pArea);
        CHECK(pNew->GetPos() == pos);
        CVariable* pNamed = pArea->m_namedCreatures.FindKey(CString(reinterpret_cast<const char*>(pNew->GetScriptName())));
        REQUIRE(pNamed != NULL);
        CHECK(pNamed->m_intValue == pNew->GetId());
    }

    ClearMessages();
}

// A damage effect's DisplayString is the shared empty slot (0x4A51D0), not
// the base's effect text line: with effect text bit 8 the only line a hit
// shows is the feedback line (0x4AA6D0).
TEST_CASE("Damage (0x4A51D0 DisplayString): a hit shows only its feedback line", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);
    pGoblin->m_baseStats.m_hitPoints = 50;
    pGoblin->m_derivedStats.m_nMaxHitPoints = 50;
    pGoblin->m_derivedStats.m_nResistCrushing = 0;
    ClearMessages();

    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD level = pOptions->m_nEffectTextLevel;
    pOptions->m_nEffectTextLevel = 0x2 | 0x8;
    CTimerWorld* pTimer = g_pBaldurChitin->GetObjectGame()->GetWorldTimer();
    const BOOL active = pTimer->m_active;
    pTimer->m_active = TRUE;

    SECTION("the base's line would be the effect text 14027; the damage's is empty")
    {
        std::unique_ptr<CGameEffect> pDamage = Decode(CGAMEEFFECT_DAMAGE, 0, 3, pProbe->GetId());
        pDamage->CGameEffect::DisplayString(pGoblin);
        std::vector<CMessage*> messages = Messages();
        REQUIRE(messages.size() == 1);
        CMessageDisplayTextRef* pRef = dynamic_cast<CMessageDisplayTextRef*>(messages[0]);
        REQUIRE(pRef != nullptr);
        CHECK(pRef->m_text == 14027);

        ClearMessages();
        pDamage->DisplayString(pGoblin);
        CHECK(Messages().empty());
    }

    SECTION("through AddEffect: one feedback line, no effect text line")
    {
        CGameEffect* pDamage = Decode(CGAMEEFFECT_DAMAGE, 0, 3, pProbe->GetId()).release();
        pDamage->m_durationType = 1;
        pGoblin->field_70FA = 0;
        pGoblin->AddEffect(pDamage, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

        int lines = 0;
        for (CMessage* pMessage : Messages()) {
            CMessageDisplayTextRef* pRef = dynamic_cast<CMessageDisplayTextRef*>(pMessage);
            if (pRef != nullptr) {
                CHECK(pRef->m_text != 14027);
            }
            CMessageDisplayText* pText = dynamic_cast<CMessageDisplayText*>(pMessage);
            if (pText != nullptr) {
                lines++;
                CHECK(pText->m_text == Text(25028));
            }
        }
        CHECK(lines == 1);
        CHECK(pGoblin->m_baseStats.m_hitPoints == 47);
    }

    pOptions->m_nEffectTextLevel = level;
    pTimer->m_active = static_cast<BOOLEAN>(active);
    ClearMessages();
    delete pGoblin;
}

namespace {

// The CMessageDisplayText lines queued since the last ClearMessages.
std::vector<CMessageDisplayText*> TextLines()
{
    std::vector<CMessageDisplayText*> lines;
    for (CMessage* pMessage : Messages()) {
        CMessageDisplayText* pText = dynamic_cast<CMessageDisplayText*>(pMessage);
        if (pText != nullptr) {
            lines.push_back(pText);
        }
    }
    return lines;
}

}

TEST_CASE("CheckSave (0x4A42F0): trap sense, and the save and resistance lines (0x765830, 0x5864F0)", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    Neutralize(pGoblin);
    ClearMessages();
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameOptions* pOptions = pGame->GetOptions();
    const DWORD level = pOptions->m_nEffectTextLevel;

    SECTION("trap sense adds to a reflex save against a trap's effect")
    {
        CGameObject* pTrap = nullptr;
        CTypedPtrList<CPtrList, int*>* lists[] = { &pProbe->GetArea()->m_lVertSort, &pProbe->GetArea()->m_lVertSortBack };
        for (CTypedPtrList<CPtrList, int*>* list : lists) {
            POSITION pos = list->GetHeadPosition();
            while (pos != NULL && pTrap == nullptr) {
                LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
                CGameObject* pObject;
                if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                    pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                    if (pObject->GetObjectType() == CGameObject::TYPE_TRIGGER) {
                        pTrap = pObject;
                    }
                }
            }
        }
        REQUIRE(pTrap != nullptr);

        // TRAPSAVE.2DA gives barbarians 1 from level 10 and 2 from 13.
        pGoblin->m_derivedStats.m_nClassLevels[CAIOBJECTTYPE_C_BARBARIAN - 1] = 13;
        const INT nTrapSense = pGame->GetRuleTables().GetTrapSenseBonus(pGoblin->m_derivedStats);
        REQUIRE(nTrapSense >= 1);
        REQUIRE(nTrapSense <= 8);

        // DC 10, reflex 0: the roll 10 - trap sense makes it only against a
        // trap.
        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, pTrap->GetId());
        pEffect->m_savingThrow = 0x8;
        pEffect->m_flags = 0x2;
        const BYTE roll = static_cast<BYTE>(10 - nTrapSense);
        Rolls rolls { 1, roll, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);

        pEffect->m_sourceID = pProbe->GetId();
        rolls = { 1, roll, 1, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
    }

    SECTION("the save line: the save, the source's name, the roll with effect text bit 8, the result")
    {
        CSpell spell { CResRef("SPPR208") };
        const CString sSpell = Text(spell.GetGenericName());
        REQUIRE(!sSpell.IsEmpty());

        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, CGameObjectArray::INVALID_INDEX);
        pEffect->m_savingThrow = 0x10;
        pEffect->m_flags = 0x2;
        pEffect->m_saveMod = 2;
        pEffect->m_spellLevel = 1;
        pEffect->m_sourceRes = CResRef("SPPR208");
        pGoblin->m_derivedStats.m_nSaveVSWill = 5;

        pOptions->m_nEffectTextLevel = 0x1 | 0x8;
        Rolls rolls { 1, 1, 9, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        std::vector<CMessageDisplayText*> lines = TextLines();
        REQUIRE(lines.size() == 1);
        CHECK(lines[0]->m_name == pGoblin->GetName());
        CHECK(lines[0]->m_text == Text(14005) + " (" + sSpell + ") : 9 + 5 = 14 : " + Text(33753));

        // The roll is shown once.
        ClearMessages();
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        CHECK(TextLines().empty());

        // A failed save with a negative bonus.
        ClearMessages();
        pGoblin->m_derivedStats.m_nSaveVSWill = -7;
        rolls = { 1, 1, 9, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        lines = TextLines();
        REQUIRE(lines.size() == 1);
        CHECK(lines[0]->m_text == Text(14005) + " (" + sSpell + ") : 9 - 7 = 2 : " + Text(33752));

        // Without bit 8 no roll; without bit 1 no line.
        ClearMessages();
        pOptions->m_nEffectTextLevel = 0x1;
        rolls = { 1, 1, 9, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        lines = TextLines();
        REQUIRE(lines.size() == 1);
        CHECK(lines[0]->m_text == Text(14005) + " (" + sSpell + ") : " + Text(33752));

        ClearMessages();
        pOptions->m_nEffectTextLevel = 0;
        rolls = { 1, 1, 9, 1 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == TRUE);
        CHECK(TextLines().empty());
    }

    SECTION("the resistance line names the source: an item, or nothing for no resref")
    {
        CItem item(CResRef("00POTN02"), 1, 0, 0, 0, 0);
        const CString sItem = Text(item.GetIdentifiedName());
        REQUIRE(!sItem.IsEmpty());

        std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_HOLDCREATURE, 0, 0, CGameObjectArray::INVALID_INDEX);
        pEffect->m_sourceFlags = 0x400;
        pEffect->m_flags = 0x1;
        pEffect->m_casterLevel = 10;
        pEffect->m_sourceRes = CResRef("00POTN02");
        pGoblin->m_derivedStats.m_nResistMagic = 18;

        // Shown whatever the effect text option says.
        pOptions->m_nEffectTextLevel = 0;
        Rolls rolls { 20, 20, 20, 5 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        std::vector<CMessageDisplayText*> lines = TextLines();
        REQUIRE(lines.size() == 1);
        CHECK(lines[0]->m_name == pGoblin->GetName());
        CHECK(lines[0]->m_text == Text(19224) + " (" + sItem + ")");

        ClearMessages();
        pEffect->m_sourceRes = CResRef("");
        rolls = { 20, 20, 20, 5 };
        CHECK(Check(pEffect.get(), pGoblin, rolls) == FALSE);
        lines = TextLines();
        REQUIRE(lines.size() == 1);
        CHECK(lines[0]->m_text == Text(19224));
    }

    pOptions->m_nEffectTextLevel = level;
    ClearMessages();
    delete pGoblin;
}

// Death (0x4AB510) with vis-probe's callees wired: the sprite's death
// variables (0x6F5330), the dropped inventory (0x754EE0), and the messages of
// subtype 89 and 96 for the sprite at the end of the queue, in that order.
TEST_CASE("Death (0x4AB510) ends with the messages 89 and 96 for the sprite", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameArea* pArea = pProbe->GetArea();
    REQUIRE(pArea != NULL);
    CGameSprite* pGoblin = MakeGoblin();
    pGoblin->AddToArea(pArea, CPoint(pProbe->GetPos().x - 60, pProbe->GetPos().y + 50), 0, CGameObject::LIST_FRONT);
    pArea->SortLists();
    ClearMessages();

    std::unique_ptr<CGameEffect> pDeath = Decode(CGAMEEFFECT_DEATH, 0, 0, pProbe->GetId());
    iwd2r_srand(37);
    pDeath->ApplyEffect(pGoblin);
    INFO("draws " << Draws(37, iwd2r_rand_state()));
    CHECK(Draws(37, iwd2r_rand_state()) == 0);
    CHECK(pDeath->m_done == TRUE);

    std::vector<CMessage*> messages = Messages();
    REQUIRE(messages.size() >= 2);
    CMessage89* p89 = dynamic_cast<CMessage89*>(messages[messages.size() - 2]);
    CMessage96* p96 = dynamic_cast<CMessage96*>(messages.back());
    REQUIRE(p89 != nullptr);
    REQUIRE(p96 != nullptr);
    CHECK(p89->m_spriteId == pGoblin->GetId());
    CHECK(p89->GetSource() == pGoblin->GetId());
    CHECK(p89->GetTarget() == pGoblin->GetId());
    CHECK(p96->m_spriteId == pGoblin->GetId());
    ClearMessages();
}

// The 0x586DC0 wiring (the summon limit table, Icewind586B70): Death
// (0x4AB510, the call at 0x4AD165) and Skill Unsummon (0x4B4BD0, the call
// at 0x4B4BFB) unsummon a summoned creature of the table before they queue
// the message of subtype 89: its entry is cleared and the unsummon visual
// (spell hit 0x15, "MSumm1H", which picks no random sequence) is fired at
// it. The whole path draws no rand; another summoner's entry stays.
TEST_CASE("Death and Skill Unsummon unsummon through the summon limit table (0x586DC0)", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameArea* pArea = pProbe->GetArea();
    REQUIRE(pArea != NULL);
    CGameSprite* pGoblin = MakeGoblin();
    pGoblin->AddToArea(pArea, CPoint(pProbe->GetPos().x - 60, pProbe->GetPos().y + 50), 0, CGameObject::LIST_FRONT);
    pArea->SortLists();

    Icewind586B70* pTable = Icewind586B70::Instance();
    pTable->Clear();

    SECTION("Death (0x4AB510)")
    {
        pTable->mEntries[0] = Icewind586B70::Entry(pGoblin->GetId(), pProbe->GetId());
        pTable->mEntries[1] = Icewind586B70::Entry(12345, 67890);
        ClearMessages();
        std::unique_ptr<CGameEffect> pDeath = Decode(CGAMEEFFECT_DEATH, 0, 0, pProbe->GetId());
        iwd2r_srand(37);
        pDeath->ApplyEffect(pGoblin);
        CHECK(pDeath->m_done == TRUE);

        // Spell hit 0x15 draws nothing: no random sequence, no roll.
        INFO("draws " << Draws(37, iwd2r_rand_state()));
        CHECK(Draws(37, iwd2r_rand_state()) == 0);

        CHECK(pTable->mEntries[0].field_0 == 0);
        CHECK(pTable->mEntries[0].field_4 == 0);
        CHECK(pTable->mEntries[1].field_0 == 12345);
        CHECK(pTable->mEntries[1].field_4 == 67890);

        // The unsummon visual is fired before the subtype 89 and 96 messages,
        // with the hide's group update between it and them.
        std::vector<CMessage*> messages = Messages();
        REQUIRE(messages.size() >= 4);
        CMessageFireProjectile* pFire = dynamic_cast<CMessageFireProjectile*>(messages[messages.size() - 4]);
        CMessage89* p89 = dynamic_cast<CMessage89*>(messages[messages.size() - 2]);
        CMessage96* p96 = dynamic_cast<CMessage96*>(messages.back());
        REQUIRE(pFire != nullptr);
        REQUIRE(p89 != nullptr);
        REQUIRE(p96 != nullptr);
        CHECK(pFire->m_projectileType == 0x1015);
        CHECK(pFire->m_projectileTargetId == pGoblin->GetId());
        CHECK(pFire->GetTarget() == pGoblin->GetId());
        CHECK(p89->m_spriteId == pGoblin->GetId());
        ClearMessages();
    }

    SECTION("Skill Unsummon (0x4B4BD0)")
    {
        pTable->mEntries[0] = Icewind586B70::Entry(pGoblin->GetId(), pProbe->GetId());
        pTable->mEntries[1] = Icewind586B70::Entry(12345, 67890);
        ClearMessages();
        std::unique_ptr<CGameEffect> pUnsummon = Decode(CGAMEEFFECT_SKILLUNSUMMON, 0, 0, pProbe->GetId());
        iwd2r_srand(43);
        pUnsummon->ApplyEffect(pGoblin);
        CHECK(pUnsummon->m_done == TRUE);

        INFO("draws " << Draws(43, iwd2r_rand_state()));
        CHECK(Draws(43, iwd2r_rand_state()) == 0);

        std::string list;
        for (CMessage* pMessage : Messages()) {
            list += " t" + std::to_string(pMessage->GetMsgType()) + "/" + std::to_string(pMessage->GetMsgSubType());
        }
        INFO("effect queue:" << list);

        CHECK(pTable->mEntries[0].field_0 == 0);
        CHECK(pTable->mEntries[0].field_4 == 0);
        CHECK(pTable->mEntries[1].field_0 == 12345);
        CHECK(pTable->mEntries[1].field_4 == 67890);
        CHECK(pGoblin->m_removeFromArea == TRUE);

        // The unsummon visual's message, then the hide's group update, then
        // the subtype 89 message.
        std::vector<CMessage*> messages = Messages();
        REQUIRE(messages.size() >= 3);
        CMessageFireProjectile* pFire = dynamic_cast<CMessageFireProjectile*>(messages[messages.size() - 3]);
        CMessage89* p89 = dynamic_cast<CMessage89*>(messages.back());
        REQUIRE(pFire != nullptr);
        REQUIRE(p89 != nullptr);
        CHECK(pFire->m_projectileType == 0x1015);
        CHECK(pFire->m_projectileTargetId == pGoblin->GetId());
        CHECK(p89->m_spriteId == pGoblin->GetId());
        ClearMessages();
    }

    pTable->Clear();
}

// The hit sound (CGameEffectDamage 0x4A8DF0) remembers the last one the
// sprite played (+0x7102): the same pick twice in a row moves on to the next
// digit, wrapping to 1. One draw each time. (The sound's message goes to the
// other players only, so the remembered name is what shows.)
TEST_CASE("Damage's hit sound (0x4A8DF0) does not repeat the sprite's last one", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameSprite* pGoblin = MakeGoblin();
    ClearMessages();
    std::unique_ptr<CGameEffect> pEffect = Decode(CGAMEEFFECT_DAMAGE, 0, 0, pProbe->GetId());
    CGameEffectDamage* pDamage = dynamic_cast<CGameEffectDamage*>(pEffect.get());
    REQUIRE(pDamage != nullptr);

    pGoblin->m_sLastHitSound = "";
    iwd2r_srand(43);
    pDamage->sub_4A8DF0(0x1000000, pGoblin);
    CHECK(Draws(43, iwd2r_rand_state()) == 1);
    const std::string first(static_cast<const char*>(pGoblin->m_sLastHitSound));
    INFO("first " << first);
    REQUIRE(first.size() > 4);
    CHECK(first.compare(0, 5, "H_SL_") == 0);

    // The same draw again: the next digit.
    iwd2r_srand(43);
    pDamage->sub_4A8DF0(0x1000000, pGoblin);
    CHECK(Draws(43, iwd2r_rand_state()) == 1);
    const std::string second(static_cast<const char*>(pGoblin->m_sLastHitSound));
    INFO("second " << second);
    CHECK(second != first);
    CHECK(second.substr(0, second.size() - 1) == first.substr(0, first.size() - 1));
    CHECK((second.back() == first.back() + 1 || second.back() == '1'));

    // And once more: back to the first pick, which differs from the last.
    iwd2r_srand(43);
    pDamage->sub_4A8DF0(0x1000000, pGoblin);
    CHECK(std::string(static_cast<const char*>(pGoblin->m_sLastHitSound)) == first);

    ClearMessages();
    delete pGoblin;
}

// Cleave (opcode 442, 0x4B9AA0): with a kill to follow up (the secondary
// type set) the attacker looks for the nearest enemy within its reach
// (CGameArea::sub_46DDC0), passing over the one in m_special once; it shows
// feedback 87, readies a swing (the attack frame -2, the sequence message)
// and queues an attack on it; with none it is done. No draws.
TEST_CASE("Cleave (0x4B9AA0) turns on the next enemy in reach", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameArea* pArea = pProbe->GetArea();
    REQUIRE(pArea != NULL);
    CGameSprite* pGoblin = MakeGoblin();
    pGoblin->AddToArea(pArea, CPoint(pProbe->GetPos().x + 30, pProbe->GetPos().y + 10), 0, CGameObject::LIST_FRONT);
    pArea->SortLists();
    ClearMessages();
    while (!pProbe->m_queuedActions.IsEmpty()) {
        delete pProbe->m_queuedActions.RemoveHead();
    }

    auto sequenceMessages = []() {
        int count = 0;
        for (CMessage* pMessage : Messages()) {
            CMessageSetSequence* pSequence = dynamic_cast<CMessageSetSequence*>(pMessage);
            if (pSequence != nullptr) {
                count++;
                CHECK(pSequence->m_sequence == CGameSprite::SEQ_ATTACK);
            }
        }
        return count;
    };

    // No kill to follow up (the secondary type, which effects start with
    // set, cleared): nothing.
    std::unique_ptr<CGameEffect> pCleave = Decode(ICEWIND_CGAMEEFFECT_FEATCLEAVE, 0, 5, pProbe->GetId());
    pCleave->m_secondaryType = 0;
    CHECK(pCleave->ApplyEffect(pProbe) == TRUE);
    CHECK(pCleave->m_done == FALSE);
    CHECK(sequenceMessages() == 0);
    CHECK(pProbe->m_queuedActions.IsEmpty());

    // The goblin just felled is passed over, and nothing else is in reach:
    // done.
    pCleave = Decode(ICEWIND_CGAMEEFFECT_FEATCLEAVE, 0, 5, pProbe->GetId());
    pCleave->m_secondaryType = 1;
    pCleave->m_special = static_cast<DWORD>(pGoblin->GetId());
    CHECK(pCleave->ApplyEffect(pProbe) == TRUE);
    CHECK(pCleave->m_done == TRUE);
    CHECK(pProbe->m_queuedActions.IsEmpty());
    CHECK(sequenceMessages() == 0);

    // Another kill: the goblin in reach is attacked.
    pCleave = Decode(ICEWIND_CGAMEEFFECT_FEATCLEAVE, 0, 5, pProbe->GetId());
    pCleave->m_secondaryType = 1;
    pCleave->m_special = static_cast<DWORD>(CGameObjectArray::INVALID_INDEX - 1);
    iwd2r_srand(47);
    CHECK(pCleave->ApplyEffect(pProbe) == TRUE);
    CHECK(Draws(47, iwd2r_rand_state()) == 0);
    CHECK(pCleave->m_done == FALSE);
    CHECK(pProbe->m_attackFrame == -2);
    CHECK(pProbe->field_5636 == 5);
    CHECK(pProbe->m_interrupt == TRUE);
    REQUIRE(!pProbe->m_queuedActions.IsEmpty());
    CAIAction* pAction = pProbe->m_queuedActions.GetHead();
    CHECK(pAction->m_actionID == CAIAction::ATTACK);
    CHECK(pAction->m_specificID == pGoblin->GetId());
    CHECK(sequenceMessages() == 1);

    while (!pProbe->m_queuedActions.IsEmpty()) {
        delete pProbe->m_queuedActions.RemoveHead();
    }
    ClearMessages();
}

// Polymorph (opcode 135, 0x4BB110) into a wolf (12WOLF): a sprite of the
// creature is made to copy from and deleted again; the goblin takes its
// animation (CMessageAnimationChange goes to the others), its armor class, attacks,
// resistances and physical scores, is marked polymorphed and loses its
// spell types and buttons 2 to 11. Without a resource it ends the
// polymorph instead.
TEST_CASE("Polymorph (0x4BB110) gives a goblin a wolf's shape", "[effects][checksave][data]")
{
    PROBE_OR_SKIP(pProbe);
    CGameArea* pArea = pProbe->GetArea();
    REQUIRE(pArea != NULL);
    CGameSprite* pGoblin = MakeGoblin();
    pGoblin->AddToArea(pArea, CPoint(pProbe->GetPos().x + 90, pProbe->GetPos().y - 40), 0, CGameObject::LIST_FRONT);
    pArea->SortLists();
    ClearMessages();

    std::vector<BYTE> wolfBytes = LoadCreature("12WOLF");
    REQUIRE(!wolfBytes.empty());

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    std::unique_ptr<CGameEffect> pPolymorph = Decode(CGAMEEFFECT_POLYMORPH, 0, 0, pProbe->GetId());
    pPolymorph->m_res = CResRef("12WOLF");
    const SHORT nNextId = pArray->m_nNextObjectId;
    iwd2r_srand(53);
    CHECK(pPolymorph->ApplyEffect(pGoblin) == TRUE);
    INFO("draws " << Draws(53, iwd2r_rand_state()));
    // The draws of the shape's construction (the CGameSprite constructor)
    // and of the goblin's sequences.
    CHECK(Draws(53, iwd2r_rand_state()) == 9);
    CHECK(pPolymorph->m_done == FALSE);

    CHECK(pGoblin->GetAnimation()->GetAnimationId() == 0x7B00);
    CHECK(pGoblin->m_derivedStats.m_bPolymorphed == TRUE);
    CHECK(pGoblin->m_derivedStats.m_disabledSpellTypes[0] == TRUE);
    CHECK(pGoblin->m_derivedStats.m_disabledSpellTypes[1] == TRUE);
    for (int n = 2; n <= 11; n++) {
        CHECK(pGoblin->m_derivedStats.m_disabledButtons[n] == TRUE);
    }
    // The shape was made and deleted again.
    CHECK(pArray->m_nNextObjectId != nNextId);
    CGameObject* pObject;
    CHECK(pArray->GetShare(nNextId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS);

    // CMessageAnimationChange goes to the other players only.
    ClearMessages();

    // A wolf of the same file has the stats the goblin took.
    CGameSprite* pWolf = new CGameSprite(wolfBytes.data(), static_cast<LONG>(wolfBytes.size()), 0, 0, static_cast<DWORD>(-1), 0, 0, INT_MAX, CPoint(-1, -1), 0);
    CHECK(pGoblin->m_derivedStats.m_nNumberOfAttacks == pWolf->m_derivedStats.m_nNumberOfAttacks);
    CHECK(pGoblin->m_derivedStats.m_nSTR == pWolf->m_derivedStats.m_nSTR);
    CHECK(pGoblin->m_derivedStats.m_nResistFire == pWolf->m_derivedStats.m_nResistFire);
    CHECK(pGoblin->m_derivedStats.m_nACSlashingMod == pWolf->m_derivedStats.m_nACSlashingMod);
    pArray->Delete(pWolf->GetId(), CGameObjectArray::THREAD_ASYNCH, NULL, INFINITE);
    delete pWolf;

    // Without a resource: the polymorph ends.
    std::unique_ptr<CGameEffect> pEnd = Decode(CGAMEEFFECT_POLYMORPH, 0, 0, pProbe->GetId());
    CHECK(pEnd->ApplyEffect(pGoblin) == TRUE);
    CHECK(pEnd->m_done == TRUE);
    CHECK(pEnd->m_forceRepass == TRUE);
    ClearMessages();
}
