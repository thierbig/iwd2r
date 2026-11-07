#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CGameAnimationType84EEB8.h"
#include "CGameChunk.h"
#include "CGameEffect.h"
#include "CGameObject84D624.h"
#include "CGameObject84F0DC.h"
#include "CGameObject84F14C.h"
#include "CGameObjectArray.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CProjectile84DFA4.h"
#include "CProjectile84E940.h"
#include "CProjectile84E9C0.h"
#include "CProjectile850F9C.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
#include "CProjectileShaft.h"
#include "CProjectileSingleTarget.h"
#include "CProjectileSpellHit.h"
#include "CProjectileSunray.h"
#include "CSpell.h"
#include "CTimerWorld.h"
#include "CVidPalette.h"
#include "IcewindCGameEffects.h"
#include "engine_world.h"
#include "msvc_rand.h"
#include "render_surfaces.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <string>
#include <vector>

// The particle object of the sparkles and explosions (CGameObject84D624,
// 0x51AE40) and the projectiles of the Visual Spell Hit effect (opcode 141)
// in AR1000 of a new game: built, updated until they remove themselves, and
// drawn into DirectDraw shim surfaces (tests/render_surfaces.h's
// AreaScope). The rand draws of their constructors, AddToArea and AIUpdate
// are pinned: a replay depends on their count and order. Every engine works
// on a private view of the install (tests/data_view.h).

using namespace engine_world;
using namespace render_surfaces;

namespace {

unsigned int Advance(unsigned int state, int draws)
{
    for (int i = 0; i < draws; ++i) {
        state = state * 214013u + 2531011u;
    }
    return state;
}

// How many rand draws took the generator from before to after (-1 when more
// than 2000).
int Draws(unsigned int before, unsigned int after)
{
    for (int n = 0; n <= 2000; n++) {
        if (Advance(before, n) == after) {
            return n;
        }
    }
    return -1;
}

// The k-th number rand() returns after srand(seed), k from 1.
int RandAt(unsigned int seed, int k)
{
    return static_cast<int>((Advance(seed, k) >> 16) & 0x7FFF);
}

struct World {
    CGameSprite* pCharacter = NULL;
    CGameArea* pArea = NULL;
};

// A new game in AR1000 with its party member, the area explored and
// visible; made once per process (a second NewGame asserts).
bool NewWorld(World& world, std::string& why)
{
    static World made;
    if (made.pArea != NULL) {
        world = made;
        return true;
    }
    if (DataDir() == nullptr) {
        why = "IWD2_DATA unset";
        return false;
    }
    std::string error;
    if (!StartEngine(error)) {
        why = error;
        return false;
    }
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        why = "no CHR V2.2 file in the Characters folder";
        return false;
    }
    world.pCharacter = NewGameInStartArea(cre);
    world.pArea = world.pCharacter->GetArea();
    if (world.pArea == NULL) {
        why = "the party member is in no area";
        return false;
    }
    world.pArea->m_visibility.SetAreaExplored();
    world.pArea->m_visibility.SetAreaVisible(TRUE);
    made = world;
    return true;
}

// The engine starts before the frame pump, which writes to it, and the
// new game after.
#define WORLD_OR_SKIP(world)                  \
    if (DataDir() == nullptr) {               \
        SKIP("[iwd2r-skip] IWD2_DATA unset"); \
    }                                         \
    {                                         \
        std::string error;                    \
        if (!StartEngine(error)) {            \
            SKIP("[iwd2r-skip] " + error);    \
        }                                     \
    }                                         \
    FramePump pump;                           \
    World world;                              \
    {                                         \
        std::string why;                      \
        if (!NewWorld(world, why)) {          \
            SKIP("[iwd2r-skip] " + why);      \
        }                                     \
    }

// Whether the object array still holds the object.
bool Alive(LONG id)
{
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CGameObject* pObject;
    if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
        return false;
    }
    pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    return true;
}

// Runs the object's AIUpdate until it removes itself (at most `limit`
// times); returns how many updates it took, -1 when it stayed.
int UpdateUntilGone(LONG id, int limit)
{
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (int frame = 0; frame < limit; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            return frame;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    return Alive(id) ? -1 : limit;
}

// The objects the area holds in its add lists (what was added since the
// last SortLists).
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

// What the object's Render alone leaves in the viewport, cleared to black
// first.
std::vector<DWORD> RenderObject(AreaScope& scope, CGameObject* pObject)
{
    scope.Back(true, 0);
    pObject->Render(pObject->GetArea(), scope.VidMode(), CVIDINF_SURFACE_BACK);
    return scope.Back();
}

// The pixels that are not black, inside and outside a screen rectangle.
struct Lit {
    int inside = 0;
    int outside = 0;
};

Lit CountLit(const std::vector<DWORD>& pixels, const CRect& rScreen, const CRect& rIn)
{
    Lit lit;
    for (int y = 0; y < rScreen.Height(); y++) {
        for (int x = 0; x < rScreen.Width(); x++) {
            if ((pixels[y * rScreen.Width() + x] & 0xFFFFFF) == 0) {
                continue;
            }
            if (rIn.PtInRect(CPoint(x, y))) {
                lit.inside++;
            } else {
                lit.outside++;
            }
        }
    }
    return lit;
}

}

// An ice explosion (the death effect's and Sparkle type 1's): its five
// colours are SPRKLCLR.2DA's CSPARKLE_COLORTYPE_ICE row; AddToArea makes
// three streams of 14 particles fanned around the direction from its
// position to its source, two draws each (84), and sets its bounds 50
// around the position. Its updates draw nothing (TYPE_EXPLOSION); the
// slowest particle (gravity 50, rising at 1000) lands on the 65th update
// from a height of 40, and the object removes itself. Its render draws only
// inside its bounds.
TEST_CASE("visual projectiles: an explosion fans out, lands and draws inside its bounds", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x + 60, world.pCharacter->GetPos().y);

    CGameObject84D624* pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_ICE,
        CGameObject84D624::TYPE_EXPLOSION,
        CGameObject84D624::EXPLOSION_PUFFS);
    CHECK(pExplosion->m_rgbColors[0] == RGB(235, 245, 245));
    CHECK(pExplosion->m_rgbColors[1] == RGB(194, 233, 244));
    CHECK(pExplosion->m_rgbColors[2] == RGB(127, 218, 246));
    CHECK(pExplosion->m_rgbColors[3] == RGB(114, 197, 226));
    CHECK(pExplosion->m_rgbColors[4] == RGB(70, 132, 161));
    CHECK(pExplosion->m_wType == CGameObject84D624::TYPE_EXPLOSION);
    CHECK(pExplosion->m_nPuffs == 25);
    CHECK(pExplosion->m_nPuff == 0);
    CHECK(pExplosion->m_nGravity == 50);
    CHECK(pExplosion->m_rBounding.IsRectEmpty());
    CHECK(pExplosion->m_particleList.IsEmpty());
    CHECK(pExplosion->GetId() == CGameObjectArray::INVALID_INDEX);

    // The source due east: the streams go north-east, east and south-east.
    pExplosion->SetSource(CPoint(pos.x + 100, pos.y));
    iwd2r_srand(7);
    pExplosion->AddToArea(pArea, pos, 40, CGameObject::LIST_FRONT);
    CHECK(Draws(7, iwd2r_rand_state()) == 84);
    const LONG id = pExplosion->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pExplosion->GetArea() == pArea);
    CHECK(pExplosion->m_posZ == 40);
    CHECK(pExplosion->m_nFadeHeights[0] == 39);
    CHECK(pExplosion->m_nFadeHeights[1] == 34);
    CHECK(pExplosion->m_nFadeHeights[2] == 30);
    CHECK(pExplosion->m_nFadeHeights[3] == 22);
    CHECK(pExplosion->m_rBounding == CRect(pos.x - 50, pos.y - 40 - 50, pos.x + 50, pos.y - 40 + 50));
    REQUIRE(pExplosion->m_particleList.GetCount() == 42);
    CParticle* pFirst = pExplosion->m_particleList.GetHead();
    CParticle* pMiddle = pExplosion->m_particleList.GetAt(pExplosion->m_particleList.FindIndex(14));
    CParticle* pLast = pExplosion->m_particleList.GetTail();
    CHECK(pFirst->m_vel.x == 700);
    CHECK(pFirst->m_vel.y == -700);
    CHECK(pFirst->m_vel.z == 1000);
    CHECK(pFirst->m_nGravity == 50);
    CHECK(pFirst->m_pos.z == 40 << 10);
    CHECK(pMiddle->m_vel.x == 1000);
    CHECK(pMiddle->m_vel.y == 0);
    CHECK(pMiddle->m_nGravity == 50);
    CHECK(pLast->m_vel.x == 700);
    CHECK(pLast->m_vel.y == 700);
    CHECK(pLast->m_nGravity == 50 + 13 * 10);
    for (POSITION p = pExplosion->m_particleList.GetHeadPosition(); p != NULL;) {
        CParticle* pParticle = pExplosion->m_particleList.GetNext(p);
        // Within 18 of the position less 10 (two draws of 0-9, doubled).
        CHECK(pParticle->m_pos.x >> 10 >= pos.x - 10);
        CHECK(pParticle->m_pos.x >> 10 <= pos.x + 8);
    }

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pExplosion);
        scope.Shot(pixels, "explosion");
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(pExplosion->m_rBounding));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(9);
    CHECK(UpdateUntilGone(id, 200) == 65);
    CHECK(Draws(9, iwd2r_rand_state()) == 0);
    CHECK(!Alive(id));
    pArea->SortLists();
}

// The sparkle a projectile trails (TYPE_SPARKLE): 14 particles in a puff,
// two draws each (28); each update draws one number per particle (it may
// turn one back) and lets them fall.
TEST_CASE("visual projectiles: a sparkle puff draws one number per particle each update", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x - 40, world.pCharacter->GetPos().y + 20);

    CGameObject84D624* pSparkle = new CGameObject84D624(2, CGameObject84D624::TYPE_SPARKLE, 25);
    // CSPARKLE_COLORTYPE_BLUE, the table's first row.
    CHECK(pSparkle->m_rgbColors[0] == RGB(235, 241, 245));
    CHECK(pSparkle->m_rgbColors[4] == RGB(70, 92, 161));

    iwd2r_srand(5);
    pSparkle->AddToArea(pArea, pos, 30, CGameObject::LIST_FRONT);
    CHECK(Draws(5, iwd2r_rand_state()) == 28);
    const LONG id = pSparkle->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pSparkle->m_rBounding == CRect(pos.x - 5, pos.y - 5 - 30, pos.x + 4, pos.y + 4));
    REQUIRE(pSparkle->m_particleList.GetCount() == 14);
    CHECK(pSparkle->m_particleList.GetHead()->m_vel.x == 4);
    CHECK(pSparkle->m_particleList.GetHead()->m_nGravity == 50);
    CHECK(pSparkle->m_particleList.GetTail()->m_vel.x == -4);
    CHECK(pSparkle->m_particleList.GetTail()->m_nGravity == 50 + 13 * 10);

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pSparkle);
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(pSparkle->m_rBounding));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(6);
    pSparkle->AIUpdate();
    CHECK(Draws(6, iwd2r_rand_state()) == 14);
    CHECK(pSparkle->m_particleList.GetCount() == 14);

    CHECK(UpdateUntilGone(id, 400) > 0);
    CHECK(!Alive(id));
    pArea->SortLists();
}

// The shower (Sparkle type 3, TYPE_SHOWER): a puff of 14 at AddToArea, then
// one more each update, around its position in turn, until it has made
// m_nPuffs; each update also draws one number per particle it has.
TEST_CASE("visual projectiles: a shower adds a puff each update until it has made its puffs", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x + 20, world.pCharacter->GetPos().y - 30);

    CGameObject84D624* pShower = new CGameObject84D624(4, CGameObject84D624::TYPE_SHOWER, 3);
    iwd2r_srand(3);
    pShower->AddToArea(pArea, pos, 30, CGameObject::LIST_FRONT);
    CHECK(Draws(3, iwd2r_rand_state()) == 28);
    const LONG id = pShower->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pShower->m_nPuff == 0);
    CHECK(pShower->m_rBounding == CRect(pos.x - 13, pos.y - 30 - 13, pos.x + 13, pos.y + 13));
    CHECK(pShower->m_particleList.GetCount() == 14);

    const int expected[] = { 14 + 28, 28 + 28, 42 + 28, 56 };
    for (int update = 0; update < 4; update++) {
        INFO("update " << update);
        iwd2r_srand(100 + update);
        pShower->AIUpdate();
        CHECK(Draws(100 + update, iwd2r_rand_state()) == expected[update]);
        CHECK(pShower->m_nPuff == static_cast<DWORD>(update < 3 ? update + 1 : 3));
    }
    CHECK(pShower->m_particleList.GetCount() == 56);

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pShower);
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(pShower->m_rBounding));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    CHECK(UpdateUntilGone(id, 400) > 0);
    CHECK(!Alive(id));
    pArea->SortLists();
}

namespace {

// The particle objects the area holds in its add lists.
std::vector<CGameObject84D624*> AddedParticles(CGameArea* pArea)
{
    std::vector<CGameObject84D624*> particles;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameObject84D624* pParticles = dynamic_cast<CGameObject84D624*>(pObject)) {
            particles.push_back(pParticles);
        }
    }
    return particles;
}

// The chunks the area holds in its add lists.
std::vector<CGameChunk*> AddedChunks(CGameArea* pArea)
{
    std::vector<CGameChunk*> chunks;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameChunk* pChunk = dynamic_cast<CGameChunk*>(pObject)) {
            chunks.push_back(pChunk);
        }
    }
    return chunks;
}

std::unique_ptr<CGameEffect> DecodeAt(WORD effectID, DWORD dwFlags, LONG effectAmount, const CPoint& source, LONG sourceID, const CPoint& target)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    effect.dwFlags = dwFlags;
    effect.effectAmount = effectAmount;
    return std::unique_ptr<CGameEffect>(CGameEffect::DecodeEffect(&effect, source, sourceID, target));
}

}

// The death effect's shatters with gore (dwFlags 0x80, ice, and 0x40,
// stone; CGameEffectDeath::ApplyEffect 0x4AB510 at 0x4AC69B and 0x4AC8DC):
// two explosions at the creature on the area's back list, one flying away
// from the effect's source and one from its mirror (the source negated), as
// high as half the creature's fx rect, their particles' gravity starting at
// 100. Each explosion draws 84 numbers. Then CGameSprite::Shatter
// (0x70A810, at 0x4ACD31) breaks the creature into 10 to 25 chunks: one
// number for its bounce, one for the count, six for each chunk.
TEST_CASE("visual projectiles: a shattering death throws two explosions", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD nGore = pOptions->m_nGore;
    pOptions->m_nGore = 1;

    // The stone shatter sets the die sequence first, as the original does
    // at 0x4AC907, but that draws nothing: CGameSprite::SetSequence's
    // switch draws its rand (0x7077AE) for SEQ_ATTACK only (case 5 goes
    // straight to the switch's exit, 0x7078BC), and the animation's case 5
    // breaks at 0x6E7F0F instead of falling into TWITCH's random frame
    // (vis-probe round 10; the first_fight replay pins the goblin's death).
    struct Shatter {
        DWORD dwFlags;
        COLORREF rgbStart;
        int before;
    };
    const Shatter shatters[] = {
        { 0x80, RGB(235, 245, 245), 168 },
        { 0x40, RGB(150, 150, 150), 168 },
    };
    int n = 0;
    for (const Shatter& shatter : shatters) {
        INFO("death flags " << shatter.dwFlags);
        const CPoint pos(world.pCharacter->GetPos().x + 80 + 40 * n++, world.pCharacter->GetPos().y + 30);
        CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
        REQUIRE(pGoblin != NULL);
        pArea->SortLists();

        const CPoint source(pos.x - 30, pos.y - 10);
        std::unique_ptr<CGameEffect> pDeath = DecodeAt(CGAMEEFFECT_DEATH, shatter.dwFlags, 0, source, world.pCharacter->GetId(), pos);
        REQUIRE(pDeath != nullptr);
        iwd2r_srand(17);
        pDeath->ApplyEffect(pGoblin);
        const int nChunks = RandAt(17, shatter.before + 2) % 16 + 10;
        INFO("draws " << Draws(17, iwd2r_rand_state()));
        CHECK(Draws(17, iwd2r_rand_state()) == shatter.before + 2 + 6 * nChunks);
        CHECK(AddedChunks(pArea).size() == static_cast<size_t>(nChunks));

        std::vector<CGameObject84D624*> explosions = AddedParticles(pArea);
        REQUIRE(explosions.size() == 2);
        CRect rFx;
        CPoint ptReference;
        pGoblin->GetAnimation()->CalculateFxRect(rFx, ptReference, pGoblin->m_posZ);
        for (CGameObject84D624* pExplosion : explosions) {
            CHECK(pExplosion->m_wType == CGameObject84D624::TYPE_EXPLOSION);
            CHECK(pExplosion->m_rgbColors[0] == shatter.rgbStart);
            CHECK(pExplosion->m_nGravity == 100);
            CHECK(pExplosion->m_particleList.GetCount() == 42);
            CHECK(pExplosion->GetPos() == pos);
            CHECK(pExplosion->m_posZ == static_cast<SHORT>(rFx.Height() * 5 / 10));
            CHECK(pExplosion->GetVertListType() == CGameObject::LIST_BACK);
        }
        CHECK(explosions[0]->m_ptSource == source);
        CHECK(explosions[1]->m_ptSource == CPoint(-source.x, -source.y));

        for (CGameObject84D624* pExplosion : explosions) {
            CHECK(UpdateUntilGone(pExplosion->GetId(), 400) > 0);
        }
        for (CGameChunk* pChunk : AddedChunks(pArea)) {
            pChunk->RemoveFromArea();
        }
        pArea->SortLists();
    }

    pOptions->m_nGore = nGore;
}

// The Sparkle effect (opcode 41, CGameEffectSparkle::ApplyEffect 0x4B1700)
// at a creature, its source unset (-1, -1): type 1 is an explosion of the
// effect amount's colour at the creature's position one lower, as high as
// 8/10 of its fx rect, on the front list (84 draws); type 3 a shower that
// makes a puff for each tick left of the effect's duration (28 draws);
// other types add nothing. The effect is done after one application.
TEST_CASE("visual projectiles: the Sparkle effect adds an explosion or a shower", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    CRect rFx;
    CPoint ptReference;
    pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, pSprite->m_posZ);
    const CPoint pos(pSprite->GetPos().x, pSprite->GetPos().y + 1);

    std::unique_ptr<CGameEffect> pSparkle = DecodeAt(CGAMEEFFECT_SPARKLE, 1, 7, CPoint(-1, -1), pSprite->GetId(), pSprite->GetPos());
    REQUIRE(pSparkle != nullptr);
    iwd2r_srand(23);
    pSparkle->ApplyEffect(pSprite);
    CHECK(Draws(23, iwd2r_rand_state()) == 84);
    CHECK(pSparkle->m_done == TRUE);
    std::vector<CGameObject84D624*> added = AddedParticles(pArea);
    REQUIRE(added.size() == 1);
    CHECK(added[0]->m_wType == CGameObject84D624::TYPE_EXPLOSION);
    // CSPARKLE_COLORTYPE_RED.
    CHECK(added[0]->m_rgbColors[0] == RGB(248, 240, 241));
    CHECK(added[0]->m_ptSource == CPoint(-1, -1));
    CHECK(added[0]->GetPos() == pos);
    CHECK(added[0]->m_posZ == static_cast<SHORT>(rFx.Height() * 8 / 10));
    CHECK(added[0]->GetVertListType() == CGameObject::LIST_FRONT);
    CHECK(UpdateUntilGone(added[0]->GetId(), 400) > 0);
    pArea->SortLists();

    pSparkle = DecodeAt(CGAMEEFFECT_SPARKLE, 3, 12, CPoint(-1, -1), pSprite->GetId(), pSprite->GetPos());
    REQUIRE(pSparkle != nullptr);
    pSparkle->m_duration = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime + 5;
    iwd2r_srand(29);
    pSparkle->ApplyEffect(pSprite);
    CHECK(Draws(29, iwd2r_rand_state()) == 28);
    added = AddedParticles(pArea);
    REQUIRE(added.size() == 1);
    CHECK(added[0]->m_wType == CGameObject84D624::TYPE_SHOWER);
    CHECK(added[0]->m_nPuffs == 5);
    // CSPARKLE_COLORTYPE_ORANGE.
    CHECK(added[0]->m_rgbColors[0] == RGB(255, 239, 232));
    CHECK(UpdateUntilGone(added[0]->GetId(), 400) > 0);
    pArea->SortLists();

    pSparkle = DecodeAt(CGAMEEFFECT_SPARKLE, 2, 7, CPoint(-1, -1), pSprite->GetId(), pSprite->GetPos());
    REQUIRE(pSparkle != nullptr);
    iwd2r_srand(31);
    pSparkle->ApplyEffect(pSprite);
    CHECK(Draws(31, iwd2r_rand_state()) == 0);
    CHECK(AddedParticles(pArea).empty());
    CHECK(pSparkle->m_done == TRUE);
}

namespace {

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The one object of this class the area holds in its add lists, or NULL.
template <class T>
T* AddedOne(CGameArea* pArea)
{
    std::vector<CGameObject*> objects = AddedObjects(pArea);
    if (objects.size() != 1) {
        return NULL;
    }
    return dynamic_cast<T*>(objects.front());
}

// The pixels the render lit, all of them, and those outside `rIn` (a world
// rectangle).
Lit RenderLit(AreaScope& scope, CGameObject* pObject, const CRect& rIn, const std::string& shot)
{
    const std::vector<DWORD> pixels = RenderObject(scope, pObject);
    scope.Shot(pixels, shot);
    return CountLit(pixels, scope.Screen(), scope.ToScreen(rIn));
}

}

// The burst (CProjectile84DFA4, SPBOOM, 0x530300): its colour range set
// (0x52B150), PlayAt (vtable 0x88, 0x530510) puts it at a point without
// firing it; CProjectileBAM::Render (0x52B190) draws its frame around the
// point; its updates (0x5306F0) play the sequence once, then it removes
// itself. No rand draws.
TEST_CASE("visual projectiles: the burst plays once at a point and removes itself", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    pArea->SortLists();
    const CPoint pos(world.pCharacter->GetPos().x + 50, world.pCharacter->GetPos().y + 40);

    CProjectile84DFA4* pBurst = new CProjectile84DFA4();
    CHECK(pBurst->m_animation->m_nCurrentSequence == 0);
    CHECK(pBurst->field_1D8 == 1);
    CHECK(pBurst->field_1DE == TRUE);
    CHECK(pBurst->field_1BE == FALSE);
    CHECK(pBurst->field_1D4 == 0);
    CHECK(pBurst->m_fireSoundRef == "");
    pBurst->SetColorRange(0, 0x41);

    iwd2r_srand(41);
    pBurst->PlayAt(pArea, pos);
    CHECK(Draws(41, iwd2r_rand_state()) == 0);
    const LONG id = pBurst->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pBurst->GetPos() == pos);
    CHECK(pBurst->GetArea() == pArea);
    CHECK(AddedOne<CProjectile84DFA4>(pArea) == pBurst);

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        CRect rFx;
        CPoint ptReference;
        pBurst->CalculateFxRect(rFx, ptReference, NULL);
        const CRect rBounds(pos.x - ptReference.x, pos.y - ptReference.y, pos.x - ptReference.x + rFx.Width(), pos.y - ptReference.y + rFx.Height());
        const Lit lit = RenderLit(scope, pBurst, rBounds, "burst");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    const int nLength = pBurst->m_animation->GetSequenceLength(0, FALSE);
    REQUIRE(nLength > 1);
    iwd2r_srand(42);
    CHECK(UpdateUntilGone(id, 400) == nLength - 1);
    CHECK(Draws(42, iwd2r_rand_state()) == 0);
    pArea->SortLists();
    ClearMessages();
}

// The shaft (CProjectileShaft, 0x5348C0): fired at a sprite (0x535100) it
// delivers its effects at once (an empty group here), stands 13 below the
// sprite, draws its sequences stacked upwards (0x534DD0) and goes when its
// duration runs out (0x534C10). It draws a number each update only with a
// random frame.
TEST_CASE("visual projectiles: the shaft stands on its target for its duration", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();
    const CPoint pos = pSprite->GetPos();

    CProjectileShaft* pShaft = new CProjectileShaft(CString("FlmStrk"), CString(""), 0xFF, FALSE, FALSE, 30);
    CHECK(pShaft->m_nDuration == 30);
    CHECK(pShaft->m_bTransparent == FALSE);
    CHECK(pShaft->m_bRandomFrame == FALSE);
    CHECK(pShaft->m_bHasHeight == FALSE);
    CHECK(pShaft->DetermineHeight(pSprite) == 0);
    REQUIRE(pShaft->m_animation.GetNumberSequences(FALSE) > 1);

    iwd2r_srand(43);
    pShaft->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    CHECK(Draws(43, iwd2r_rand_state()) == 0);
    const LONG id = pShaft->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pShaft->GetPos() == CPoint(pos.x, pos.y + 13));
    CHECK(AddedOne<CProjectileShaft>(pArea) == pShaft);
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageGroup* pGroup = dynamic_cast<CMessageGroup*>(handler.m_messageList.GetHead());
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->m_messages.empty());
    ClearMessages();

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(CPoint(pos.x, pos.y - 100));
        // A column over the shaft's foot.
        const CRect rColumn(pos.x - 150, pos.y - 600, pos.x + 150, pos.y + 13 + 60);
        const Lit lit = RenderLit(scope, pShaft, rColumn, "shaft");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(44);
    CHECK(UpdateUntilGone(id, 100) == 30);
    CHECK(Draws(44, iwd2r_rand_state()) == 0);
    pArea->SortLists();

    // A random frame each update: one draw per update but the last.
    pShaft = new CProjectileShaft(CString("HLYMITE"), CString(""), 0xFF, TRUE, TRUE, 5);
    pShaft->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    REQUIRE(pShaft->m_animation.GetSequenceLength(0, FALSE) > 0);
    iwd2r_srand(45);
    CHECK(UpdateUntilGone(pShaft->GetId(), 100) == 5);
    CHECK(Draws(45, iwd2r_rand_state()) == 4);
    pArea->SortLists();
    ClearMessages();
}

// The door (CProjectile84E940, DDoorH, 0x535320): fired at a sprite
// (0x535D20) it stands one above it and delivers its effects; it plays
// "DDoorH2", "DDoorH1" four times and "DDoorH2" backwards (0x5357B0), then
// arrives. No rand draws.
TEST_CASE("visual projectiles: the door opens, loops, closes and arrives", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();
    const CPoint pos = pSprite->GetPos();

    CProjectile84E940* pDoor = new CProjectile84E940(CString("DDoorH"), CString(""), CString(""), 0, 0);
    CHECK(pDoor->m_sAnimation == "DDoorH");
    CHECK(pDoor->m_nState == 0);
    CHECK(pDoor->m_bPalette == FALSE);
    CHECK(pDoor->m_animation.GetResRef() == CResRef("DDoorH2"));

    iwd2r_srand(51);
    pDoor->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    const LONG id = pDoor->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pDoor->GetPos() == CPoint(pos.x, pos.y - 1));
    CHECK(AddedOne<CProjectile84E940>(pArea) == pDoor);
    ClearMessages();

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        CPoint ptReference;
        CSize frameSize;
        pDoor->m_animation.GetCurrentCenterPoint(ptReference, FALSE);
        pDoor->m_animation.GetCurrentFrameSize(frameSize, FALSE);
        const CPoint ptPos(pos.x, pos.y - 1 + pArea->GetHeightOffset(pDoor->GetPos(), pDoor->m_listType));
        const CRect rBounds(ptPos.x - ptReference.x, ptPos.y - ptReference.y, ptPos.x - ptReference.x + frameSize.cx, ptPos.y - ptReference.y + frameSize.cy);
        const Lit lit = RenderLit(scope, pDoor, rBounds, "door");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    std::vector<int> states;
    for (int frame = 0; frame < 2000 && Alive(id); frame++) {
        if (states.empty() || states.back() != pDoor->m_nState) {
            states.push_back(pDoor->m_nState);
        }
        if (pDoor->m_nState == 1 && states.size() == 2) {
            CHECK(pDoor->m_animation.GetResRef() == CResRef("DDoorH1"));
        }
        pDoor->AIUpdate();
    }
    CHECK(!Alive(id));
    CHECK(states == std::vector<int> { 0, 1, 2, 3, 4, 5 });
    CHECK(Draws(51, iwd2r_rand_state()) == 0);
    pArea->SortLists();
    ClearMessages();
}

// The Visual Spell Hit effect (opcode 141, CGameEffectVisualSpellHit::
// ApplyEffect 0x4BBCB0) as spells carry it, applied to a goblin in AR1000:
// SPWI984's three hits (modes 24, 25, 26: air, earth and water in palette
// 6) queue the projectile of their type (0x88..0x8A) and play their local
// hit (CProjectile84E9C0, 0x535F20) at the goblin until it arrives;
// SPWI980's burst (mode 32) and SPWI995's door (mode 38) play their
// objects. The hits are drawn into the shim surfaces on the way.
TEST_CASE("visual projectiles: a spell's Visual Spell Hit plays on a goblin and finishes", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x + 70, world.pCharacter->GetPos().y + 20);
    CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
    REQUIRE(pGoblin != NULL);
    pArea->SortLists();
    ClearMessages();

    // The opcode 141 effects of the spell's first ability.
    auto hits = [&](const char* resRef) {
        std::vector<std::unique_ptr<CGameEffect>> effects;
        CSpell spell { CResRef(resRef) };
        for (LONG n = 0; n < 32; n++) {
            std::unique_ptr<CGameEffect> pEffect(spell.GetAbilityEffect(0, n, pGoblin, 0, 0, 0));
            if (pEffect == nullptr) {
                break;
            }
            if (pEffect->m_effectID == CGAMEEFFECT_VISUALSPELLHIT) {
                effects.push_back(std::move(pEffect));
            }
        }
        return effects;
    };

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(pos);

    {
        INFO("SPWI984");
        std::vector<std::unique_ptr<CGameEffect>> effects = hits("SPWI984");
        REQUIRE(effects.size() == 3);
        const DWORD modes[] = { 24, 25, 26 };
        const WORD types[] = { 0x88, 0x89, 0x8A };
        const char* animations[] = { "SHAir", "SHEarth", "SHWater" };
        const char* palettes[] = { "SHAir6", "SHEarth6", "SHWater6" };
        for (size_t n = 0; n < effects.size(); n++) {
            INFO("mode " << modes[n]);
            CGameEffect* pEffect = effects[n].get();
            CHECK(pEffect->m_dwFlags == modes[n]);
            CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);
            CHECK(pEffect->m_done == TRUE);

            CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
            REQUIRE(handler.m_messageList.GetCount() == 2);
            CMessageFireProjectile* message = dynamic_cast<CMessageFireProjectile*>(handler.m_messageList.GetHead());
            REQUIRE(message != nullptr);
            CHECK(message->m_projectileType == types[n]);
            CHECK(message->m_projectileTargetId == pGoblin->GetId());
            CHECK(message->GetSource() == pGoblin->GetId());
            CHECK(dynamic_cast<CMessageGroup*>(handler.m_messageList.GetTail()) != nullptr);
            ClearMessages();

            CProjectile84E9C0* pHit = AddedOne<CProjectile84E9C0>(pArea);
            REQUIRE(pHit != nullptr);
            CHECK(pHit->m_animation.GetResRef() == CResRef(animations[n]));
            CHECK(pHit->m_bPalette == TRUE);
            CHECK(pHit->m_palette.GetResRef() == CResRef(palettes[n]));
            CHECK(pHit->m_dwFlags == 8);
            CHECK(pHit->m_bHasHeight == TRUE);
            CHECK(pHit->GetPos() == CPoint(pos.x, pos.y + 1));
            CHECK(pHit->m_posZ == pHit->DetermineHeight(pGoblin));
            CHECK(pHit->m_posZ == pGoblin->GetAnimation()->GetCastHeight());

            const std::vector<DWORD> pixels = RenderObject(scope, pHit);
            scope.Shot(pixels, std::string("hit_") + animations[n]);
            const Lit lit = CountLit(pixels, scope.Screen(), CRect());
            CHECK(lit.outside > 0);
            CHECK(pHit->m_dwFlags == (8 | CInfinity::FXPREP_COPYFROMBACK));

            const LONG id = pHit->GetId();
            pArea->SortLists();
            iwd2r_srand(61);
            CHECK(UpdateUntilGone(id, 400) > 0);
            CHECK(Draws(61, iwd2r_rand_state()) == 0);
            pArea->SortLists();
            ClearMessages();
        }
    }

    {
        INFO("SPWI980");
        std::vector<std::unique_ptr<CGameEffect>> effects = hits("SPWI980");
        REQUIRE(effects.size() == 1);
        CHECK(effects[0]->m_dwFlags == 32);
        effects[0]->ApplyEffect(pGoblin);
        CHECK(effects[0]->m_done == TRUE);
        CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());
        CProjectile84DFA4* pBurst = AddedOne<CProjectile84DFA4>(pArea);
        REQUIRE(pBurst != nullptr);
        CHECK(pBurst->GetPos() == CPoint(pos.x, pos.y - 2));
        // Drawn on the goblin's square: projectiles take the visible
        // terrain table (0x8A8154), whose entry for a square with a creature
        // on it (8) is passable.
        const Lit lit = RenderLit(scope, pBurst, CRect(), "spwi980");
        CHECK(lit.outside > 0);
        const LONG id = pBurst->GetId();
        pArea->SortLists();
        CHECK(UpdateUntilGone(id, 400) > 0);
        pArea->SortLists();
    }

    {
        INFO("SPWI995");
        std::vector<std::unique_ptr<CGameEffect>> effects = hits("SPWI995");
        REQUIRE(effects.size() == 1);
        CHECK(effects[0]->m_dwFlags == 38);
        effects[0]->ApplyEffect(pGoblin);
        CHECK(effects[0]->m_done == TRUE);
        CProjectile84E940* pDoor = AddedOne<CProjectile84E940>(pArea);
        REQUIRE(pDoor != nullptr);
        CHECK(pDoor->m_sAnimation == "DDoorH");
        CHECK(pDoor->GetPos() == CPoint(pos.x, pos.y - 1));
        ClearMessages();
        const LONG id = pDoor->GetId();
        pArea->SortLists();
        CHECK(UpdateUntilGone(id, 2000) > 0);
        pArea->SortLists();
        ClearMessages();
    }
}

// A sparkling projectile in flight (CProjectileBAM::AIUpdate 0x52B900 at
// 0x52BC88, CProjectileSingleTarget::AIUpdate 0x578AB0 at 0x578E38) leaves
// a puff of its sparkle colour (TYPE_SPARKLE, 14 particles, 28 draws) at
// its position and height each update.
TEST_CASE("visual projectiles: a sparkling projectile leaves a puff each update", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    const CPoint pos(pSprite->GetPos().x + 200, pSprite->GetPos().y + 60);
    CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
    REQUIRE(pGoblin != NULL);
    pArea->SortLists();
    ClearMessages();

    CProjectile* projectiles[] = {
        new CProjectile84DF1C(),
        new CProjectileSingleTarget(CResRef("SPMAGMIS")),
    };
    for (CProjectile* pProjectile : projectiles) {
        pProjectile->m_bSparkle = TRUE;
        pProjectile->m_nSparkleColour = 8;
        pProjectile->Fire(pArea, pSprite->GetId(), pGoblin->GetId(), pos, 0, 0);
        const LONG id = pProjectile->GetId();
        REQUIRE(id != CGameObjectArray::INVALID_INDEX);
        pArea->SortLists();

        iwd2r_srand(71);
        pProjectile->AIUpdate();
        CHECK(Draws(71, iwd2r_rand_state()) == 28);
        std::vector<CGameObject84D624*> added = AddedParticles(pArea);
        REQUIRE(added.size() == 1);
        CHECK(added[0]->m_wType == CGameObject84D624::TYPE_SPARKLE);
        // CSPARKLE_COLORTYPE_WHITE.
        CHECK(added[0]->m_rgbColors[0] == RGB(245, 245, 245));
        CHECK(added[0]->GetPos() == pProjectile->GetPos());
        CHECK(added[0]->m_posZ == pProjectile->m_posZ);
        CHECK(added[0]->GetVertListType() == CGameObject::LIST_FRONT);
        CHECK(added[0]->m_particleList.GetCount() == 14);

        pProjectile->RemoveSelf();
        CHECK(UpdateUntilGone(added[0]->GetId(), 400) > 0);
        pArea->SortLists();
        ClearMessages();
    }
}

// The explosion's streams for other directions (0x51CE50): a source due
// north fans them north-west, north and north-east; a source south-west of
// the position (as far south as west) south, south-west and west.
TEST_CASE("visual projectiles: an explosion fans its streams away from its source", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x - 70, world.pCharacter->GetPos().y - 30);

    struct Fan {
        CPoint source;
        CPoint vel[3];
    };
    const Fan fans[] = {
        { CPoint(pos.x, pos.y - 100), { CPoint(-700, -700), CPoint(0, -1000), CPoint(700, -700) } },
        { CPoint(pos.x - 100, pos.y + 100), { CPoint(0, 1000), CPoint(-700, 700), CPoint(-1000, 0) } },
    };
    for (const Fan& fan : fans) {
        INFO("source " << fan.source.x - pos.x << "," << fan.source.y - pos.y);
        CGameObject84D624* pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_STONE,
            CGameObject84D624::TYPE_EXPLOSION,
            CGameObject84D624::EXPLOSION_PUFFS);
        pExplosion->SetSource(fan.source);
        pExplosion->AddToArea(pArea, pos, 30, CGameObject::LIST_FRONT);
        REQUIRE(pExplosion->m_particleList.GetCount() == 42);
        for (int stream = 0; stream < 3; stream++) {
            CParticle* pFirst = pExplosion->m_particleList.GetAt(pExplosion->m_particleList.FindIndex(14 * stream));
            CHECK(pFirst->m_vel.x == fan.vel[stream].x);
            CHECK(pFirst->m_vel.y == fan.vel[stream].y);
        }
        CHECK(UpdateUntilGone(pExplosion->GetId(), 400) > 0);
        pArea->SortLists();
    }
}

// CProjectileSingleTarget::Render (0x578480) draws a spell hit
// (CProjectileSpellHit, DecodeSpellHitProjectile type 9) on the creature it
// was fired at: on its square, which the projectiles' visible terrain
// table passes.
TEST_CASE("visual projectiles: a spell hit draws on its target", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(9, NULL, FALSE);
    REQUIRE(pProjectile != nullptr);
    pProjectile->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);
    const LONG id = pProjectile->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    ClearMessages();

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(pSprite->GetPos());
    // A few frames in, its animation is under way.
    pProjectile->AIUpdate();
    pProjectile->AIUpdate();
    const CPoint pos = pSprite->GetPos();
    const Lit lit = RenderLit(scope, pProjectile, CRect(pos.x - 200, pos.y - 250, pos.x + 200, pos.y + 100), "spellhit");
    CHECK(lit.inside > 0);
    CHECK(lit.outside == 0);

    CHECK(UpdateUntilGone(id, 2000) > 0);
    pArea->SortLists();
    ClearMessages();
}

namespace {

// A point near `near` whose grid square and its neighbours are open ground
// (not impassable, not deep water: CGameChunk removes itself landing in
// table index 12).
bool DryGround(CGameArea* pArea, const CPoint& near, CPoint& found)
{
    for (int dy = 0; dy < 200; dy += 12) {
        for (int dx = 0; dx < 300; dx += 16) {
            const CPoint pos(near.x + dx, near.y + dy);
            bool dry = true;
            for (int gy = -2; gy <= 2 && dry; gy++) {
                for (int gx = -3; gx <= 3 && dry; gx++) {
                    CPoint grid(pos.x / CPathSearch::GRID_SQUARE_SIZEX + gx, pos.y / CPathSearch::GRID_SQUARE_SIZEY + gy);
                    SHORT nTableIndex = 0;
                    BYTE cost = pArea->m_search.GetLOSCost(grid, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, nTableIndex, FALSE);
                    dry = cost != CPathSearch::COST_IMPASSABLE && nTableIndex != 12;
                }
            }
            if (dry) {
                found = pos;
                return true;
            }
        }
    }
    return false;
}

}

// CGameSprite::Shatter (0x70A810) types 0 (ice) and 1 (stone): it lifts the
// creature by half its reference height, draws its bounce, tints its eight
// colour ranges, clears its animation ID (the creature is no longer drawn)
// and throws rand() % 16 + 10 chunks of SPCHUNKS (animation 0x100) on the
// front list at its position. Each chunk draws four numbers in Shatter
// (bounce, rise, then the velocity's y and x) and two in its animation
// (sequence and frame); it lives 128 ticks after landing and fades over the
// last 32 (CHUNK_DURATION + 1).
TEST_CASE("visual projectiles: Shatter tints the creature and throws ice or stone chunks", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;

    struct Kind {
        BYTE nType;
        BYTE nColor;
    };
    const Kind kinds[] = {
        { 0, CVidPalette::ICE },
        { 1, CVidPalette::STONE },
    };
    int n = 0;
    for (const Kind& kind : kinds) {
        INFO("type " << static_cast<int>(kind.nType));
        const CPoint pos(world.pCharacter->GetPos().x + 80 + 40 * n++, world.pCharacter->GetPos().y + 30);
        CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
        REQUIRE(pGoblin != NULL);
        pArea->SortLists();

        CRect rFx;
        CPoint ptReference;
        pGoblin->GetAnimation()->CalculateFxRect(rFx, ptReference, pGoblin->m_posZ);
        const LONG nHeight = ptReference.y;
        REQUIRE(nHeight * 3 / 5 != 0);
        const INT_PTR nRanges = pGoblin->m_derivedStats.m_appliedColorRanges.GetCount();

        const unsigned int seed = 23 + kind.nType;
        iwd2r_srand(seed);
        pGoblin->Shatter(kind.nType);

        const int nChunks = RandAt(seed, 2) % 16 + 10;
        CHECK(Draws(seed, iwd2r_rand_state()) == 2 + 6 * nChunks);
        CHECK(pGoblin->m_posZ == -(nHeight / 2));
        CHECK(pGoblin->m_animationRunning == TRUE);
        CHECK(pGoblin->m_posZDelta == (RandAt(seed, 1) % 3 + 1) * pGoblin->m_doBounce);
        CHECK(pGoblin->m_baseStats.m_animationType == 0);
        REQUIRE(pGoblin->m_derivedStats.m_appliedColorRanges.GetCount() == nRanges + 8);
        POSITION posRange = pGoblin->m_derivedStats.m_appliedColorRanges.FindIndex(static_cast<int>(nRanges));
        for (int nRange = 0; nRange < 8; nRange++) {
            CColorRange* pColorRange = pGoblin->m_derivedStats.m_appliedColorRanges.GetNext(posRange);
            CHECK(pColorRange->m_range == nRange);
            CHECK(pColorRange->m_color == kind.nColor);
        }

        std::vector<CGameChunk*> chunks = AddedChunks(pArea);
        REQUIRE(chunks.size() == static_cast<size_t>(nChunks));
        for (CGameChunk* pChunk : chunks) {
            CHECK(pChunk->GetPos() == pos);
            CHECK(pChunk->GetVertListType() == CGameObject::LIST_FRONT);
            CHECK(pChunk->m_animation.GetAnimationId() == 0x100);
            CHECK(pChunk->m_bMoving == TRUE);
            CHECK(pChunk->m_nBounces == 3);
            CHECK(pChunk->m_nLife == 128);
            CHECK(pChunk->m_nFade == 32);
            CHECK(memcmp(pChunk->m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(pChunk->m_terrainTable)) == 0);
        }

        // The first chunk's numbers are the third to the sixth.
        CGameChunk* pFirst = chunks.front();
        CHECK(pFirst->m_posZDelta == (RandAt(seed, 3) % 3 + 1) * pGoblin->m_doBounce);
        CHECK(pFirst->m_posZ == -(RandAt(seed, 4) % (nHeight * 3 / 5)) - nHeight / 5);
        CHECK(pFirst->m_velocity == CPoint(RandAt(seed, 6) % 7 - 3, RandAt(seed, 5) % 7 - 3));

        STRREF strError = 0;
        CHECK(pFirst->CanSaveGame(strError) == TRUE);
        CHECK(strError == 16502);

        for (CGameChunk* pChunk : chunks) {
            pChunk->RemoveFromArea();
        }
        pArea->SortLists();
    }
}

// Shatter type 2 (flesh): seven chunks of the blood animations 0x210, 0x200,
// 0x200, 0x220, 0x220, 0x240 and 0x240 (four numbers each in Shatter, one
// for the animation's frame), then AddBlood's exploding death, which makes
// blood only with the gore option on. The exploding blood sizes itself by
// the video mode's FX surface (CBlood::CalcBoundingRect), so the shim
// surfaces are in place.
TEST_CASE("visual projectiles: Shatter throws seven flesh chunks and blood", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD nGoreOption = pOptions->m_nGoreOption;
    AreaScope scope(pArea);
    REQUIRE(scope.Ok());

    const WORD animations[7] = { 0x210, 0x200, 0x200, 0x220, 0x220, 0x240, 0x240 };
    for (DWORD nGore = 0; nGore < 2; nGore++) {
        INFO("gore option " << nGore);
        pOptions->m_nGoreOption = nGore;
        const CPoint pos(world.pCharacter->GetPos().x + 80 + 40 * static_cast<int>(nGore), world.pCharacter->GetPos().y + 30);
        CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
        REQUIRE(pGoblin != NULL);
        pArea->SortLists();
        const INT_PTR nBlood = pGoblin->m_lstBlood.GetCount();

        iwd2r_srand(29);
        pGoblin->Shatter(2);
        const int draws = Draws(29, iwd2r_rand_state());
        INFO("draws " << draws);
        if (nGore == 0) {
            CHECK(draws == 1 + 7 * 5);
            CHECK(pGoblin->m_lstBlood.GetCount() == nBlood);
        } else {
            CHECK(draws > 1 + 7 * 5);
            CHECK(pGoblin->m_lstBlood.GetCount() == nBlood + 1);
        }
        CHECK(pGoblin->m_baseStats.m_animationType == 0);

        std::vector<CGameChunk*> chunks = AddedChunks(pArea);
        REQUIRE(chunks.size() == 7);
        for (int nChunk = 0; nChunk < 7; nChunk++) {
            CHECK(chunks[nChunk]->m_animation.GetAnimationId() == animations[nChunk]);
        }

        for (CGameChunk* pChunk : chunks) {
            pChunk->RemoveFromArea();
        }
        pArea->SortLists();
    }

    pOptions->m_nGoreOption = nGoreOption;
}

// CGameChunk (0x70F4F0) on open ground: AIUpdate (0x70F800) lowers it by
// its fall speed, which grows by one each tick; landing, it draws one
// number for each of its three bounces (the bounce speed rand() % 3 + 1
// times the bounces left), then lies still and removes itself after its
// 128 ticks of life. Render (0x70FA90) draws it on screen.
TEST_CASE("visual projectiles: a chunk falls, bounces three times and fades where it lies", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    pArea->SortLists();
    CPoint pos;
    REQUIRE(DryGround(pArea, CPoint(world.pCharacter->GetPos().x + 60, world.pCharacter->GetPos().y + 40), pos));

    BYTE colorRangeValues[7];
    memset(colorRangeValues, CVidPalette::STONE, sizeof(colorRangeValues));
    iwd2r_srand(31);
    CGameChunk* pChunk = new CGameChunk(0x100, colorRangeValues, CString(""), pArea, pos, CPoint(1, 0), -20, 6, 128, 32);
    CHECK(Draws(31, iwd2r_rand_state()) == 2);
    const LONG id = pChunk->GetId();
    REQUIRE(Alive(id));
    CHECK(pChunk->GetPos() == pos);
    CHECK(pChunk->m_posZ == -20);

    {
        // 20 above the ground: the piece on top, its shadow on the ground,
        // posZ lower (CGameAnimationTypeEffect::Render 0x6A36A0). The
        // shadow only darkens, so the buffer is cleared to grey first.
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pChunk);
        scope.Shot(pixels, "chunk");
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(CRect(pos.x - 40, pos.y - 80, pos.x + 40, pos.y + 40)));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);

        const DWORD grey = 0x00808080;
        scope.Back(true, grey);
        pChunk->Render(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK);
        const std::vector<DWORD> greyPixels = scope.Back();
        int top = INT_MAX;
        int bottom = -1;
        for (int y = 0; y < scope.Screen().Height(); y++) {
            for (int x = 0; x < scope.Screen().Width(); x++) {
                if ((greyPixels[y * scope.Screen().Width() + x] & 0xFFFFFF) != grey) {
                    top = std::min(top, y);
                    bottom = std::max(bottom, y);
                }
            }
        }
        INFO("drawn rows " << top << " to " << bottom);
        CHECK(bottom - top + 1 > 20);
    }

    iwd2r_srand(37);
    int updates = 0;
    while (pChunk->m_bMoving && updates < 400) {
        pChunk->AIUpdate();
        updates++;
    }
    REQUIRE(Alive(id));
    CHECK(!pChunk->m_bMoving);
    CHECK(pChunk->m_nBounces == 0);
    CHECK(pChunk->m_posZ == 0);
    CHECK(pChunk->m_nLife == 128);
    CHECK(updates > 3);
    CHECK(Draws(37, iwd2r_rand_state()) == 3);

    iwd2r_srand(41);
    CHECK(UpdateUntilGone(id, 400) == 128);
    CHECK(Draws(41, iwd2r_rand_state()) == 0);
    pArea->SortLists();
}

// The sunray (CProjectileSunray, spell hit 57, 0x57E860): fired at a sprite
// (0x57F050) it stands 13 below it and delivers its effects; it plays its
// rising sequences once (state 0), its standing ones until its 325 ticks
// are up (state 1), then its fading ones once (state 2), and removes
// itself (0x57EB30). Render (0x57ED30) stacks the state's four sequences.
// No rand draws.
TEST_CASE("visual projectiles: the sunray rises, stands for its duration and fades", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();
    const CPoint pos = pSprite->GetPos();

    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(57, NULL, FALSE);
    CProjectileSunray* pSunray = dynamic_cast<CProjectileSunray*>(pProjectile);
    REQUIRE(pSunray != nullptr);
    REQUIRE(pSunray->m_animation.GetNumberSequences(FALSE) >= 12);

    iwd2r_srand(47);
    pSunray->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    CHECK(Draws(47, iwd2r_rand_state()) == 0);
    const LONG id = pSunray->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pSunray->GetPos() == CPoint(pos.x, pos.y + 13));
    CHECK(AddedOne<CProjectileSunray>(pArea) == pSunray);
    ClearMessages();

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(CPoint(pos.x, pos.y - 100));
    const CRect rColumn(pos.x - 150, pos.y - 600, pos.x + 150, pos.y + 13 + 60);
    {
        const Lit lit = RenderLit(scope, pSunray, rColumn, "sunray");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }
    // Render leaves the block's last sequence set and keeps the frame
    // (CVidCell::SequenceSet), so the updates play that sequence, as the
    // original's do: the rising block ends after its length in updates.
    CHECK(pSunray->m_animation.m_nCurrentSequence == 3);
    const int nRising = pSunray->m_animation.GetSequenceLength(3, FALSE);
    REQUIRE(nRising > 1);

    iwd2r_srand(48);
    int updates = 0;
    while (pSunray->m_nState == CProjectileSunray::STATE_RISING && updates < 400) {
        pSunray->AIUpdate();
        updates++;
    }
    CHECK(pSunray->m_nState == CProjectileSunray::STATE_STANDING);
    CHECK(updates == nRising);
    CHECK(pSunray->m_animation.m_nCurrentFrame == 0);

    // The 325th update ends the duration: frame 0 of the fading block, then
    // the same update advances it (or ends a one-frame sequence).
    while (pSunray->m_nState == CProjectileSunray::STATE_STANDING && updates < 400) {
        pSunray->AIUpdate();
        updates++;
    }
    CHECK(updates == 325);
    CHECK(pSunray->m_nState == CProjectileSunray::STATE_FADING);
    REQUIRE(Alive(id));
    CHECK(pSunray->m_animation.m_nCurrentFrame == 1);
    {
        const Lit lit = RenderLit(scope, pSunray, rColumn, "sunray_fading");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }
    CHECK(pSunray->m_animation.m_nCurrentSequence == 11);
    const int nFading = pSunray->m_animation.GetSequenceLength(11, FALSE);
    REQUIRE(nFading > 1);
    // From frame 1 to the last, then the update that removes it.
    CHECK(UpdateUntilGone(id, 400) == nFading - 1);
    CHECK(Draws(48, iwd2r_rand_state()) == 0);
    pArea->SortLists();
    ClearMessages();
}

namespace {

// The explosion pieces the area holds in its add lists.
std::vector<CGameObject84F14C*> AddedPieces(CGameArea* pArea)
{
    std::vector<CGameObject84F14C*> pieces;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameObject84F14C* pPiece = dynamic_cast<CGameObject84F14C*>(pObject)) {
            pieces.push_back(pPiece);
        }
    }
    return pieces;
}

const std::string& PieceAnimation(CGameObject84F14C* pPiece)
{
    return static_cast<CGameAnimationType84EEB8*>(pPiece->m_animation.m_animation)->m_sAnimation;
}

}

// Fireball (projectile type 38, CProjectileArea84F580 0x571E80): fired at a
// point it flies there as FirebaT at twice the area projectile's speed and
// explodes (CProjectileArea::OnArrival 0x56F410): an explosion object
// (CGameObject84F0DC 0x56BF30) plays FirebaX and sends its ring out to the
// radius, 200, in (200 - 1) / 40 + 1 ticks (0x56D0A0). The first particle
// in a square of its grid throws a FirebaR flame (CGameObject84F14C
// 0x56DF00), at most 20; each try at a lingering FirebaA ember draws one
// number (rand() % 10000 against the chance) and adds a point, taken where
// an ember is made (0x56E280). Each piece's animation draws one number for
// its random sequence. The flames fly on and die; the embers wander over
// the taken points.
TEST_CASE("visual projectiles: Fireball flies, bursts in a ring of flames and leaves embers", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(38, pSprite, 0);
    CProjectileArea84F580* pFireball = dynamic_cast<CProjectileArea84F580*>(pProjectile);
    REQUIRE(pFireball != nullptr);
    CHECK(pFireball->m_projectileType == 37);
    CHECK(pFireball->m_nRadius == 200);
    CHECK(pFireball->m_speed == 40);
    CHECK(pFireball->m_bTravels == TRUE);
    CHECK(pFireball->field_1DE == TRUE);
    CHECK(pFireball->m_nDuration == 45);
    CHECK(pFireball->m_fireSoundRef == "TRA_06");
    CHECK(pFireball->m_explosion1.m_animation == "FirebaX");
    CHECK(pFireball->m_explosion1.m_sound == "RNG_M03");
    CHECK(pFireball->m_explosion2.m_animation == "FirebaR");
    CHECK(pFireball->m_explosion2.field_2E == 20);
    CHECK(pFireball->m_explosion3.m_animation == "FirebaA");
    CHECK(pFireball->m_explosion3.field_34 == TRUE);

    CPoint target;
    REQUIRE(DryGround(pArea, CPoint(pSprite->GetPos().x + 160, pSprite->GetPos().y + 40), target));
    pFireball->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, target, 0, 0);
    const LONG id = pFireball->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    pArea->SortLists();
    ClearMessages();

    int flight = 0;
    while (!pFireball->m_bExploded && flight < 200) {
        pFireball->AIUpdate();
        flight++;
    }
    REQUIRE(pFireball->m_bExploded);
    CHECK(flight > 1);
    // It arrives within one step (its speed plus one) of the point.
    const LONG dx = pFireball->GetPos().x - target.x;
    const LONG dy = pFireball->GetPos().y - target.y;
    CHECK(dy * dy * 16 / 9 + dx * dx <= 41 * 41);
    CProjectileAreaPoints* pPoints = pFireball->m_explosion3.m_pPoints;
    REQUIRE(pPoints != nullptr);
    CHECK(pPoints->m_points.empty());
    CHECK(pPoints->m_points.capacity() >= 512);

    CGameObject84F0DC* pExplosion = AddedOne<CGameObject84F0DC>(pArea);
    REQUIRE(pExplosion != nullptr);
    const LONG explosionId = pExplosion->GetId();
    CHECK(pExplosion->GetPos() == pFireball->GetPos());
    CHECK(pExplosion->m_bAnimation == TRUE);
    CHECK(pExplosion->m_nExpandTicks == (200 - 1) / 40 + 1);
    CHECK(pExplosion->m_nWallMode == CGameObject84F0DC::WALL_STOP);
    CHECK(pExplosion->m_nDuration == 45);
    CHECK(pExplosion->m_nGridX == (200 - 1) / 16 + 1);
    CHECK(pExplosion->m_nArc1 > 1);
    CHECK(pExplosion->m_nArc2 > 1);
    CHECK(pExplosion->m_explosion3.m_pPoints == pPoints);
    CHECK(memcmp(pExplosion->m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(pExplosion->m_terrainTable)) == 0);
    // The particles start at the centre.
    CHECK(pExplosion->m_pParticles[0].x == 0);
    CHECK(pExplosion->m_pParticles[0].vx == 0);
    CHECK(pExplosion->m_pParticles[0].vy == 40 << 10);

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(target);
    const CRect rBurst(target.x - 260, target.y - 260, target.x + 260, target.y + 260);
    {
        const Lit lit = RenderLit(scope, pExplosion, rBurst, "fireball_burst");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    // The expansion and the burst's end. Three ticks in, the burst and its
    // flames together light more than the burst alone did.
    iwd2r_srand(53);
    int updates = 0;
    int nBurstLit = 0;
    {
        scope.Back(true, 0);
        pExplosion->Render(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK);
        nBurstLit = CountLit(scope.Back(), scope.Screen(), scope.ToScreen(rBurst)).inside;
    }
    while (Alive(explosionId) && updates < 400) {
        pExplosion->AIUpdate();
        updates++;
        if (updates == 3) {
            REQUIRE(Alive(explosionId));
            scope.Back(true, 0);
            pExplosion->Render(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK);
            for (CGameObject84F14C* pPiece : AddedPieces(pArea)) {
                pPiece->Render(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK);
            }
            const std::vector<DWORD> pixels = scope.Back();
            scope.Shot(pixels, "fireball_ring");
            const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(rBurst));
            CHECK(lit.inside > nBurstLit);
            CHECK(lit.outside == 0);
        }
    }
    CHECK(!Alive(explosionId));
    CHECK(updates >= 5);

    std::vector<CGameObject84F14C*> pieces = AddedPieces(pArea);
    int nFlames = 0;
    int nEmbers = 0;
    for (CGameObject84F14C* pPiece : pieces) {
        if (PieceAnimation(pPiece) == "FirebaR") {
            nFlames++;
            CHECK(pPiece->m_pPoints == nullptr);
            CHECK(pPiece->m_nLife <= 5);
        } else {
            CHECK(PieceAnimation(pPiece) == "FirebaA");
            nEmbers++;
            CHECK(pPiece->m_pPoints == pPoints);
            CHECK(pPiece->m_bWander == TRUE);
            CHECK(pPoints->m_points[pPiece->m_nPoint].m_bTaken == 1);
        }
        CHECK(pPiece->m_nWallMode == CGameObject84F0DC::WALL_STOP);
    }
    CHECK(nFlames > 0);
    CHECK(nFlames <= 20);
    CHECK(pPoints->m_nRefCount == nEmbers);
    int nTaken = 0;
    for (const CProjectileAreaPoints::Point& point : pPoints->m_points) {
        nTaken += point.m_bTaken;
    }
    CHECK(nTaken == nEmbers);

    // One number for each try at an ember, one for each piece's sequence.
    CVidCell flame(CResRef("FirebaR"), FALSE);
    CVidCell ember(CResRef("FirebaA"), FALSE);
    const int nFlameDraws = flame.GetNumberSequences(FALSE) != 0 ? 1 : 0;
    const int nEmberDraws = ember.GetNumberSequences(FALSE) != 0 ? 1 : 0;
    INFO("flames " << nFlames << ", embers " << nEmbers << ", tries " << pPoints->m_points.size());
    CHECK(Draws(53, iwd2r_rand_state()) == static_cast<int>(pPoints->m_points.size()) + nFlames * nFlameDraws + nEmbers * nEmberDraws);

    {
        const Lit lit = RenderLit(scope, pieces.front(), rBurst, "fireball_piece");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    // The pieces burn out; the last ember deletes the points.
    std::vector<LONG> ids;
    for (CGameObject84F14C* pPiece : pieces) {
        ids.push_back(pPiece->GetId());
    }
    for (int frame = 0; frame < 2000; frame++) {
        bool any = false;
        for (LONG pieceId : ids) {
            CGameObject* pObject;
            CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
            if (pArray->GetShare(pieceId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pArray->ReleaseShare(pieceId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                pObject->AIUpdate();
                any = true;
            }
        }
        if (!any) {
            break;
        }
    }
    for (LONG pieceId : ids) {
        CHECK(!Alive(pieceId));
    }

    CHECK(UpdateUntilGone(id, 200) > 0);
    pArea->SortLists();
    ClearMessages();
}

namespace {

// The cloud pieces the area holds in its add lists.
std::vector<CGameObject85BF7C*> AddedCloudPieces(CGameArea* pArea)
{
    std::vector<CGameObject85BF7C*> pieces;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameObject85BF7C* pPiece = dynamic_cast<CGameObject85BF7C*>(pObject)) {
            pieces.push_back(pPiece);
        }
    }
    return pieces;
}

}

// The cloud explosion (CGameObject85BFE8 0x7105B0) of type 1 (a stinking
// cloud: EFF_M18a, flying pieces of effect animation 0x500 and lingering
// ones of 0x510, both STNKCLDD) on open ground, radius 32 at speed 8: it
// leaves a lingering piece at its centre (life (4 << 1) + the duration),
// then for three ticks each particle not stopped draws one number (a
// lingering piece when rand() % (((ticks left)^3 / 1000 + 10) * 3 / 2) is
// 0) and the first in a square throws a flying piece (0x711D90); it draws
// nothing itself (0x7122E0). A piece (CGameObject85BF7C 0x70FD50) draws one
// number each tick it lives through (its sound, one chance in 200;
// 0x710090). The effect animations 0x500 and 0x510 draw nothing.
TEST_CASE("visual projectiles: a cloud explosion throws flying and lingering pieces", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    pArea->SortLists();
    CPoint pos;
    REQUIRE(DryGround(pArea, CPoint(world.pCharacter->GetPos().x + 60, world.pCharacter->GetPos().y + 40), pos));

    BYTE colorRangeValues[7] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 };
    iwd2r_srand(59);
    CGameObject85BFE8* pCloud = new CGameObject85BFE8(1, colorRangeValues, pArea, pos, 32, 8, 1, 30);
    CHECK(Draws(59, iwd2r_rand_state()) == 0);
    const LONG id = pCloud->GetId();
    REQUIRE(Alive(id));
    CHECK(pCloud->GetPos() == pos);
    CHECK(pCloud->m_nExpandTicks == 31 / 8 + 1);
    CHECK(pCloud->m_nFlyingAnimation == 0x500);
    CHECK(pCloud->m_nLingeringAnimation == 0x510);
    CHECK(pCloud->m_sPieceSound == "EFF_M18c");
    CHECK(memcmp(pCloud->m_colorRangeValues, colorRangeValues, sizeof(colorRangeValues)) == 0);
    CHECK(memcmp(pCloud->m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(pCloud->m_terrainTable)) == 0);

    std::vector<CGameObject85BF7C*> pieces = AddedCloudPieces(pArea);
    REQUIRE(pieces.size() == 1);
    CHECK(pieces.front()->GetPos() == pos);
    CHECK(pieces.front()->m_nLife == (4 << 1) + 30);
    CHECK(pieces.front()->m_velocity == CPoint(0, 0));
    CHECK(pieces.front()->m_animation.GetAnimationId() == 0x510);

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(pos);
    {
        // The explosion draws nothing; its pieces do.
        const std::vector<DWORD> pixels = RenderObject(scope, pCloud);
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(CRect(pos.x - 200, pos.y - 200, pos.x + 200, pos.y + 200)));
        CHECK(lit.inside == 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(61);
    int nRolls = 0;
    int ticks = 0;
    while (Alive(id) && ticks < 400) {
        if (pCloud->m_nExpandTicks > 1) {
            for (int nParticle = 0; nParticle < 4 * (pCloud->m_nArc1 + pCloud->m_nArc2); nParticle++) {
                nRolls += pCloud->m_pStates[nParticle] != 2 ? 1 : 0;
            }
        }
        pCloud->AIUpdate();
        ticks++;
    }
    CHECK(!Alive(id));
    CHECK(nRolls > 0);
    CHECK(Draws(61, iwd2r_rand_state()) == nRolls);

    pieces = AddedCloudPieces(pArea);
    int nFlying = 0;
    for (CGameObject85BF7C* pPiece : pieces) {
        if (pPiece->m_animation.GetAnimationId() == 0x500) {
            nFlying++;
            CHECK(pPiece->m_nLife <= 3);
        } else {
            CHECK(pPiece->m_animation.GetAnimationId() == 0x510);
            CHECK(pPiece->m_velocity == CPoint(0, 0));
        }
        CHECK(pPiece->m_nWallMode == 1);
    }
    CHECK(nFlying > 0);
    {
        const Lit lit = RenderLit(scope, pieces.front(), CRect(pos.x - 200, pos.y - 200, pos.x + 200, pos.y + 200), "cloud_piece");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    // The pieces live out their lives, one number for each tick a piece
    // lives through.
    std::vector<LONG> ids;
    for (CGameObject85BF7C* pPiece : pieces) {
        ids.push_back(pPiece->GetId());
    }
    iwd2r_srand(67);
    int nLived = 0;
    for (int frame = 0; frame < 400; frame++) {
        bool any = false;
        for (LONG pieceId : ids) {
            CGameObject* pObject;
            CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
            if (pArray->GetShare(pieceId, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pArray->ReleaseShare(pieceId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                pObject->AIUpdate();
                nLived += Alive(pieceId) ? 1 : 0;
                any = true;
            }
        }
        if (!any) {
            break;
        }
    }
    for (LONG pieceId : ids) {
        CHECK(!Alive(pieceId));
    }
    CHECK(Draws(67, iwd2r_rand_state()) == nLived);
    pArea->SortLists();
}

namespace {

// An area projectile type as DecodeProjectile (0x51EAF0) builds it: its
// radius, its three explosion descriptions, its timing and its targets.
struct AreaType {
    WORD nType;
    int nRadius;
    const char* x;
    const char* xSound;
    const char* r;
    LONG nMaxFlames;
    const char* a;
    const char* aSound;
    BOOLEAN bWander;
    BOOLEAN bRandom;
    BOOLEAN bOnce;
    LONG nChance;
    LONG nDuration;
    LONG nInterval;
    LONG nHitModulus;
    BYTE nEnemyAlly; // 0: the default target type, ANYONE
};

const AreaType kAreaTypes[] = {
    { 42, 150, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_EVILCUTOFF },
    { 64, 150, "WebX", "EFF_M19", "", 0x7FFFFFFF, "WebA", "ARE_M03", 1, 1, 0, 450, 1500, 10, 10, 0 },
    { 67, 140, "", "EFF_P16", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 92, 200, "FStormX", "EFF_P45", "", 0x7FFFFFFF, "FStormA", "ARE_P03", 1, 1, 0, 250, 105, 10, 10, 0 },
    { 95, 100, "SCloudX", "RNG_M01", "SCloudR", 13, "SCloudA", "ARE_M02", 1, 1, 0, 1000, 1000, 10, 10, 0 },
    { 101, 100, "GreaseX", "EFF_M31b", "", 0x7FFFFFFF, "GreaseA", "ARE_M01", 1, 1, 0, 1000, 1000, 10, 10, 0 },
    { 187, 150, "CloudKX", "RNG_M01", "CloudKR", 13, "CloudKA", "ARE_M02", 1, 1, 0, 500, 1000, 10, 10, 0 },
    { 190, 150, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 209, 150, "MRageX", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 212, 150, "DFogX", "RNG_M01", "DFogR", 20, "DFogA", "ARE_M02", 1, 1, 0, 250, 1100, 10, 10, 0 },
    { 213, 150, "SStoneA", "EFF_P48", "", 0x7FFFFFFF, "SStoneA", "ARE_P04", 1, 1, 0, 500, 1200, 10, 10, 0 },
    { 235, 200, "EntangX", "", "", 0x7FFFFFFF, "EntangA", "ARE_P01", 1, 1, 0, 450, 1000, 10, 10, 0 },
    { 236, 250, "Area1X", "EFF_P99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 237, 250, "Area1X", "EFF_P99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 238, 300, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 239, 300, "Area2X", "EFF_M99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 240, 300, "Area2X", "EFF_M99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 241, 150, "Area4X", "EFF_M99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 242, 300, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 243, 350, "Area1X", "EFF_P99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 244, 250, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 245, 150, "AlteraX", "ARE_M19", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 246, 150, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 247, 200, "AlteraX", "ARE_M19", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 248, 200, "AlteraX", "ARE_M19", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 249, 200, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 250, 200, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 252, 250, "AlteraX", "ARE_M19", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 253, 600, "Area1X", "EFF_P99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 254, 300, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 255, 150, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 256, 300, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 257, 100, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 258, 200, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 260, 600, "Area1X", "EFF_P99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 262, 275, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 263, 200, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 264, 200, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 266, 350, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 270, 70, "FSeedsX", "EFF_P45", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 273, 300, "Area3X", "EFF_M99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 275, 200, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 276, 300, "SoPainX", "EFF_P49", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 277, 300, "SoHopeX", "EFF_P50", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 278, 150, "PWKillX", "EFF_M39", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 280, 100, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 281, 250, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 282, 100, "", "", "SCloudR", 7, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 284, 150, "CloudKX", "RNG_M01", "CloudKR", 13, "CloudKA", "ARE_M02", 1, 1, 0, 500, 1000, 10, 10, 0 },
    { 286, 150, "", "", "", 0x7FFFFFFF, "ICloudB", "", 0, 1, 1, 500, 45, 10000, 10, 0 },
    { 287, 150, "", "", "", 0x7FFFFFFF, "ICloudA", "", 1, 1, 0, 500, 200, 10000, 10, 0 },
    { 288, 300, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 295, 250, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 299, 120, "", "", "", 0x7FFFFFFF, "SEaterA", "ARE_M18", 1, 1, 0, 200, 45, 10000, 10, 0 },
    { 300, 150, "SGrowtX", "EFF_P111", "", 0x7FFFFFFF, "SGrowtA", "AFT_P24", 1, 1, 0, 250, 2000, 10, 10, 0 },
    { 301, 300, "", "", "", 0x7FFFFFFF, "CloudBA", "ARE_P24", 1, 1, 0, 100, 200, 10, 10, 0 },
    { 307, 120, "", "EFF_P104", "", 0x7FFFFFFF, "MoEldaA", "", 1, 1, 0, 1200, 45, 10000, 10, 0 },
    { 308, 70, "", "", "", 0x7FFFFFFF, "", "EFF_P101", 0, 0, 0, 250, 300, 5, 20, 0 },
    { 309, 200, "", "", "", 0x7FFFFFFF, "CoPestA", "ARE_P25", 1, 1, 0, 175, 400, 10, 10, 0 },
    { 310, 200, "", "ARE_P28", "", 0x7FFFFFFF, "", "AFT_P27", 0, 0, 0, 250, 1000, 10, 10000, 0 },
    { 311, 70, "", "", "", 0x7FFFFFFF, "", "EFF_P100", 0, 0, 0, 250, 200, 5, 20, 0 },
    { 317, 120, "", "", "", 0x7FFFFFFF, "SuffocA", "AFT_M18", 1, 1, 0, 1000, 400, 10, 10, 0 },
    { 318, 300, "ADHWilX", "ARE_M17", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 320, 225, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 321, 250, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 322, 150, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 323, 200, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 335, 250, "FirebaX", "RNG_M03", "FirebaR", 0x14, "FirebaA", "", 1, 1, 1, 250, 45, 10000, 10, 0 },
    { 336, 150, "PWStunH", "EFF_M38", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 337, 200, "", "", "", 0x7FFFFFFF, "HSmiteA", "", 1, 1, 0, 300, 45, 10000, 10, 0 },
    { 338, 200, "", "", "", 0x7FFFFFFF, "UBlighA", "", 1, 1, 0, 300, 45, 10000, 10, 0 },
    { 339, 200, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 340, 300, "SoHopeX", "EFF_P50", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 341, 300, "SoHopeX", "EFF_P50", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 342, 500, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 349, 200, "DBreatH", "ARE_M21", "", 0x7FFFFFFF, "DBreatA", "", 0, 1, 1, 500, 45, 10000, 10, 0 },
    { 351, 125, "", "", "CloudKR", 7, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 357, 150, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 358, 125, "Area1X", "EFF_M99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 359, 300, "Area1X", "EFF_P99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 362, 300, "Area1X", "EFF_W99", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 363, 300, "SoPainX", "EFF_P49", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 364, 300, "SoPainX", "EFF_P49", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 365, 300, "SoPainX", "EFF_P49", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ENEMY },
    { 366, 300, "", "", "", 0x7FFFFFFF, "MSwarmA", "ARE_P03", 0, 0, 0, 250, 45, 10, 10000, 0 },
    { 367, 200, "AlteraX", "ARE_M19", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 369, 70, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 371, 70, "AlteraX", "ARE_M19", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, CAIObjectType::EA_ALLY },
    { 372, 160, "AbjuraX", "ARE_M20", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 373, 200, "EnchanX", "ARE_M21", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 377, 200, "", "", "", 0x7FFFFFFF, "CoPestA", "ARE_P25", 1, 1, 0, 175, 300, 10, 10, 0 },
    { 381, 200, "GABreaX", "ARE_M21", "", 0x7FFFFFFF, "GABreaA", "", 0, 1, 1, 500, 45, 10000, 10, 0 },
    { 384, 65, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
    { 385, 65, "", "", "", 0x7FFFFFFF, "", "", 0, 0, 0, 250, 45, 10000, 10, 0 },
};

// A spell-hit visual type as DecodeProjectile (0x51EAF0) builds it (ctor
// 0x535F20, CProjectile84E9C0): its animation and its palette tint (empty:
// untinted). The sound, flags 8, trans 0 and the cleared sound refs are the
// same for every type.
struct VisualHitType {
    WORD nType;
    const char* sAnimation;
    const char* sPalette;
};

const VisualHitType kVisualHitTypes[] = {
    { 119, "SHAir", "" },
    { 120, "SHEarth", "" },
    { 121, "SHWater", "" },
    { 122, "SHAir", "SHAir1" },
    { 123, "SHEarth", "SHEarth1" },
    { 124, "SHWater", "SHWater1" },
    { 125, "SHAir", "SHAir2" },
    { 126, "SHEarth", "SHEarth2" },
    { 127, "SHWater", "SHWater2" },
    { 128, "SHAir", "SHAir3" },
    { 129, "SHEarth", "SHEarth3" },
    { 130, "SHWater", "SHWater3" },
    { 131, "SHAir", "SHAir4" },
    { 132, "SHEarth", "SHEarth4" },
    { 133, "SHWater", "SHWater4" },
    { 134, "SHAir", "SHAir5" },
    { 135, "SHEarth", "SHEarth5" },
    { 136, "SHWater", "SHWater5" },
    { 137, "SHAir", "SHAir6" },
    { 138, "SHEarth", "SHEarth6" },
    { 139, "SHWater", "SHWater6" },
    { 140, "SHAir", "SHAir7" },
    { 141, "SHEarth", "SHEarth7" },
    { 142, "SHWater", "SHWater7" },
    { 148, "SHAir", "SHAir7" },
    { 196, "SHAREA", "" },
    { 197, "SHAREA", "SHAREA1" },
    { 198, "SHAREA", "" },
    { 199, "SHAREA", "SHAREA3" },
    { 200, "SHAREA", "SHAREA4" },
    { 201, "SHAREA", "SHAREA5" },
    { 202, "SHAREA", "SHAREA6" },
    { 203, "SHAREA", "SHAREA2" },
    { 208, "SPFDEATH", "" },
};

}

// The area projectile types the game's spells and items use (DecodeProjectile
// 0x51EAF0): the plain CProjectileArea ones built in the switch and the
// classes of 0x571310 to 0x577B20. The party member casts (EA_PC): a spell
// on friends targets EA_ALLY, one on enemies EA_ENEMY, Sleep those evil
// (EA_EVILCUTOFF). Decoding draws nothing.
TEST_CASE("visual projectiles: the area spells decode to their area projectiles", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    for (const AreaType& spec : kAreaTypes) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileArea* pArea = dynamic_cast<CProjectileArea*>(pProjectile);
        REQUIRE(pArea != nullptr);
        CHECK(pArea->m_projectileType == spec.nType - 1);
        CHECK(pArea->m_nRadius == spec.nRadius);
        CHECK(pArea->m_explosion1.m_animation == spec.x);
        CHECK(pArea->m_explosion1.m_sound == spec.xSound);
        CHECK(pArea->m_explosion2.m_animation == spec.r);
        CHECK(pArea->m_explosion2.field_2E == spec.nMaxFlames);
        CHECK(pArea->m_explosion3.m_animation == spec.a);
        CHECK(pArea->m_explosion3.m_sound == spec.aSound);
        CHECK(pArea->m_explosion3.field_34 == spec.bWander);
        CHECK(pArea->m_explosion3.field_35 == spec.bRandom);
        CHECK(pArea->m_explosion3.field_36 == spec.bOnce);
        CHECK(pArea->m_explosion3.field_38 == spec.nChance);
        CHECK(pArea->m_nDuration == spec.nDuration);
        CHECK(pArea->m_nTriggerInterval == spec.nInterval);
        CHECK(pArea->m_nHitModulus == spec.nHitModulus);
        CHECK(pArea->m_bTravels == (spec.nType == 270 ? TRUE : FALSE));
        if (spec.nType == 366) {
            // 366 also stores 1 at 0x1D8.
            CHECK(pArea->field_1D8 == 1);
        }
        if (spec.nEnemyAlly != 0) {
            CHECK(pArea->m_targetType.m_nEnemyAlly == spec.nEnemyAlly);
        } else {
            CHECK(pArea->m_targetType.m_nEnemyAlly == CAIObjectType::ANYONE.m_nEnemyAlly);
        }
        delete pProjectile;
    }

    CProjectile* pProjectile = CProjectile::DecodeProjectile(244, world.pCharacter, 0);
    CHECK(static_cast<CProjectileArea*>(pProjectile)->m_bCheckForNonSprites == TRUE);
    delete pProjectile;
    pProjectile = CProjectile::DecodeProjectile(190, world.pCharacter, 0);
    CHECK(dynamic_cast<CProjectileArea84F9C4*>(pProjectile) != nullptr);
    CHECK(pProjectile->m_fireSoundRef == "TRA_04C");
    CHECK(pProjectile->m_arrivalSoundRef == "EFF_M05");
    delete pProjectile;
    pProjectile = CProjectile::DecodeProjectile(246, world.pCharacter, 0);
    CHECK(dynamic_cast<CProjectileArea850C0C*>(pProjectile) != nullptr);
    CHECK(pProjectile->m_fireSoundRef == "TRA_04A");
    CHECK(pProjectile->m_arrivalSoundRef == "CAS_P02");
    delete pProjectile;
    pProjectile = CProjectile::DecodeProjectile(357, world.pCharacter, 0);
    CHECK(dynamic_cast<CProjectileHaltUndead*>(pProjectile) != nullptr);
    CHECK(pProjectile->m_fireSoundRef == "TRA_04C");
    CHECK(pProjectile->m_arrivalSoundRef == "EFF_M05");
    delete pProjectile;
    // The portal flips retarget everything but sprites and carry only
    // their opening/closing sound.
    pProjectile = CProjectile::DecodeProjectile(294, world.pCharacter, 0);
    CProjectilePortalAnimFlip* pPortal = dynamic_cast<CProjectilePortalAnimFlip*>(pProjectile);
    REQUIRE(pPortal != nullptr);
    CHECK(pPortal->m_projectileType == 293);
    CHECK(pPortal->m_nRadius == 0x7D0);
    CHECK(pPortal->m_explosion1.m_sound == "PRTL_OP");
    CHECK(pPortal->m_targetType.m_nEnemyAlly == CAIObjectType::NOT_SPRITE.m_nEnemyAlly);
    delete pProjectile;
    pProjectile = CProjectile::DecodeProjectile(297, world.pCharacter, 0);
    pPortal = dynamic_cast<CProjectilePortalAnimFlip*>(pProjectile);
    REQUIRE(pPortal != nullptr);
    CHECK(pPortal->m_projectileType == 296);
    CHECK(pPortal->m_nRadius == 0x7D0);
    CHECK(pPortal->m_explosion1.m_sound == "PRTL_CL");
    CHECK(pPortal->m_targetType.m_nEnemyAlly == CAIObjectType::NOT_SPRITE.m_nEnemyAlly);
    delete pProjectile;
}

// The spell-hit visuals the game's spells and items use (DecodeProjectile
// 0x51EAF0, the 0x535F20 cases): the elemental hits of the Visual Spell Hit
// effect, their tinted variants, the area hits and the death visual. They
// decode without touching rand.
TEST_CASE("visual projectiles: the spell-hit visuals decode to their tinted hits", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    for (const VisualHitType& spec : kVisualHitTypes) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile84E9C0* pHit = dynamic_cast<CProjectile84E9C0*>(pProjectile);
        REQUIRE(pHit != nullptr);
        CHECK(pHit->m_projectileType == spec.nType - 1);
        // SPFDEATH's BAM is not in this install's keyfile, so
        // CResHelper::SetResRef (src/CRes.h) leaves the animation ref empty
        // - it keeps the ref only when GetResObject resolves it. Every other
        // animation resolves to itself.
        const char* sAnimStored = (spec.nType == 208) ? "" : spec.sAnimation;
        CHECK(pHit->m_animation.GetResRef() == CResRef(sAnimStored));
        CHECK(pHit->m_bPalette == (spec.sPalette[0] != '\0' ? TRUE : FALSE));
        if (spec.sPalette[0] != '\0') {
            CHECK(pHit->m_palette.GetResRef() == CResRef(spec.sPalette));
        }
        CHECK(pHit->m_dwFlags == 8);
        CHECK(pHit->m_nTransValue == 0);
        CHECK(pHit->m_fireSoundRef == "");
        CHECK(pHit->m_arrivalSoundRef == "");
        delete pProjectile;
    }
}

// The magic-missile barrages (DecodeProjectile 0x51EAF0, one body for types
// 68 to 78, ctor 0x531120 CProjectile84E0C4): 1 to 11 missiles of type 217,
// the TRA_22 fire sound, no arrival sound and the height flag. The palette
// type is the WORD 1 at 0x85E84A. Decoding draws nothing.
TEST_CASE("visual projectiles: the magic missile barrages decode to their missiles", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    for (WORD nType = 68; nType <= 78; nType++) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile84E0C4* pMissile = dynamic_cast<CProjectile84E0C4*>(pProjectile);
        REQUIRE(pMissile != nullptr);
        CHECK(pMissile->m_projectileType == nType - 1);
        CHECK(pMissile->m_nCount == nType - 67);
        CHECK(pMissile->m_nType == 217);
        CHECK(pMissile->m_animation->GetResRef() == CResRef("SPMAGMIS"));
        CHECK(pMissile->m_fireSoundRef == "TRA_22");
        CHECK(pMissile->m_arrivalSoundRef == "");
        CHECK(pMissile->m_bHasHeight == TRUE);
        delete pProjectile;
    }
}

// The follow-up barrages (DecodeProjectile 0x51EAF0, one body for types 81
// to 91, ctor 0x534600 CProjectileMAGMISS): 1 to 11 MAGMISS shots (child
// type 22) every 300 ticks keeping the source, over the party member's own
// target type - a PC caster (EA_PC <= EA_NOTEVIL) picks EA_EVILCUTOFF - and
// with line of sight checked. Decoding draws nothing.
TEST_CASE("visual projectiles: the follow-up magic missile barrages decode to their launchers", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    for (WORD nType = 81; nType <= 91; nType++) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileMAGMISS* pLauncher = dynamic_cast<CProjectileMAGMISS*>(pProjectile);
        REQUIRE(pLauncher != nullptr);
        CHECK(pLauncher->m_projectileType == nType - 1);
        CHECK(pLauncher->m_remainingShots == nType - 80);
        CHECK(pLauncher->m_childType == 22);
        // The ctor hands the cell "MAGMISS", but CHITIN.KEY only holds a
        // MAGMISS entry of type 4 - not the BAM type 1000 the CVidCell
        // helper looks up - so SetResRef misses and stores "".
        CHECK(pLauncher->m_animation->GetResRef() == CResRef(""));
        CHECK(pLauncher->m_interval == 300);
        CHECK(pLauncher->m_keepSource == TRUE);
        CHECK(pLauncher->m_targetType.m_nEnemyAlly == CAIObjectType::EA_EVILCUTOFF);
        CHECK(pLauncher->m_checkLOS == TRUE);
        delete pProjectile;
    }
}

// The wave-puff cones (DecodeProjectile 0x51EAF0, ctor 0x579B40
// CProjectile850DDC, one entry per type): each builds its cone on the
// invisible MMissiT cell (0x1DE FALSE) with its puff animation in m_sPuff,
// arms the visual effect, and tunes the fields below; the two silent types
// keep the empty fire sound. Decoding draws nothing.
TEST_CASE("visual projectiles: the cone types decode to their wave-puff cones", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* sPuff;
        LONG nWidth;
        LONG nLength;
        SHORT nSpeed;
        LONG nSpacing;
        LONG nPuffSpeed;
        LONG nPuffSpeedRandom;
        LONG nFrames;
        LONG nInterval;
        BYTE bRandomPuffFrame;
        BYTE b317;
        LONG l2EE;
        const char* sFireSound;
    };
    const Spec kCones[] = {
        { 22, "BHandsT", 200, 80, 20, 50, 35, 45, 15, 5, 1, 0, 16, "TRA_15" },
        { 25, "CoColdT", 200, 350, 60, 50, 35, 45, 8, 4, 1, 1, 1, "TRA_08" },
        { 97, "CSprayT", 200, 200, 20, 30, 35, 45, 15, 5, 1, 1, 1, "TRA_16" },
        { 272, "PSprayT", 175, 400, 20, 30, 35, 45, 15, 5, 1, 1, 1, "TRA_21" },
        { 296, "CoColdT", 200, 120, 40, 50, 35, 45, 8, 4, 1, 1, 1, "TRA_08" },
        { 303, "TSprayT", 600, 300, 60, 35, 35, 45, 8, 4, 1, 0, 16, "" },
        { 315, "ShoutT", 100, 300, 30, 1000, 75, 0, 12, 2, 0, 1, 16, "TRA_08" },
        { 319, "GShoutT", 100, 300, 30, 1000, 75, 0, 12, 2, 0, 1, 16, "TRA_08" },
        { 343, "WOWispT", 200, 200, 20, 30, 35, 45, 15, 5, 1, 1, 1, "" },
        { 368, "FFingeT", 200, 80, 20, 50, 35, 45, 15, 5, 1, 0, 16, "TRA_15" },
        { 374, "ShoutT", 120, 300, 30, 1000, 75, 0, 12, 2, 0, 1, 16, "TRA_08" },
        { 379, "HDFBreT", 300, 300, 60, 50, 35, 45, 8, 4, 1, 1, 1, "TRA_08" },
        { 382, "GShoutT", 120, 300, 30, 1000, 75, 0, 12, 2, 0, 1, 16, "TRA_08" },
        { 386, "HDFBreT", 150, 150, 60, 50, 35, 45, 8, 4, 1, 1, 1, "TRA_08" },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kCones) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile850DDC* pCone = dynamic_cast<CProjectile850DDC*>(pProjectile);
        REQUIRE(pCone != nullptr);
        CHECK(pCone->m_projectileType == spec.nType - 1);
        CHECK(pCone->m_sPuff == spec.sPuff);
        CHECK(pCone->m_animation->GetResRef() == CResRef("MMissiT"));
        CHECK(pCone->field_1DE == FALSE);
        CHECK(pCone->m_nWidth == spec.nWidth);
        CHECK(pCone->m_nLength == spec.nLength);
        CHECK(pCone->m_nHalfWidth == 25);
        CHECK(pCone->m_speed == spec.nSpeed);
        CHECK(pCone->m_nPuffSpacing == spec.nSpacing);
        CHECK(pCone->m_nPuffSpeed == spec.nPuffSpeed);
        CHECK(pCone->m_nPuffSpeedRandom == spec.nPuffSpeedRandom);
        CHECK(pCone->m_nFrames == spec.nFrames);
        CHECK(pCone->m_nWaveInterval == spec.nInterval);
        CHECK(pCone->m_bRandomPuffFrame == spec.bRandomPuffFrame);
        CHECK(pCone->field_317 == spec.b317);
        CHECK(pCone->field_2EE == spec.l2EE);
        CHECK(pCone->m_fireSoundRef == spec.sFireSound);
        delete pProjectile;
    }
}

// The MFMissT barrages (DecodeProjectile 0x51EAF0, ctor 0x5309C0
// CProjectile84E030): one body for types 324 to 334 - 1 to 11 missiles of
// child type 314 - plus type 345's seven of child type 344, on the MFMissT
// cell (CHITIN.KEY holds MFMISST as BAM type 1000, so the cell keeps the
// name), with the palette WORD 1 at 0x85E84A, the TRA_55 fire sound, no
// arrival sound and the height flag. The children are decoded with pSprite
// NULL; type 314 is not wired yet, so those children land on the default
// path's CProjectile84D8B8 fallback (344 wires to the MFMissT
// single-target). Decoding draws nothing.
TEST_CASE("visual projectiles: the MFMissT barrages decode to their batteries", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        WORD nChildType;
        WORD nCount;
    };
    const Spec kBatteries[] = {
        { 324, 313, 1 },
        { 325, 313, 2 },
        { 326, 313, 3 },
        { 327, 313, 4 },
        { 328, 313, 5 },
        { 329, 313, 6 },
        { 330, 313, 7 },
        { 331, 313, 8 },
        { 332, 313, 9 },
        { 333, 313, 10 },
        { 334, 313, 11 },
        { 345, 343, 7 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kBatteries) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile84E030* pBattery = dynamic_cast<CProjectile84E030*>(pProjectile);
        REQUIRE(pBattery != nullptr);
        CHECK(pBattery->m_projectileType == spec.nType - 1);
        CHECK(pBattery->m_nType == spec.nChildType);
        CHECK(pBattery->m_nCount == spec.nCount);
        CHECK(pBattery->m_animation->GetResRef() == CResRef("MFMissT"));
        CHECK(pBattery->m_fireSoundRef == "TRA_55");
        CHECK(pBattery->m_arrivalSoundRef == "");
        CHECK(pBattery->m_bHasHeight == TRUE);
        delete pProjectile;
    }
}

// The spell-hit cells (DecodeProjectile 0x51EAF0, ctor 0x57E490
// CProjectileSpellHit): one animation played where the spell lands, its
// effects delivered the moment it fires. DecodeProjectile's prologue
// default-constructs the visual effect (0x586A40) and every entry but
// GateX applies sub_586A90(TRUE) before the ctor copies it (m_dwFlags
// 0x20200; GateX keeps the default 0x20000). The five summoning and
// elemental hits also take the EFF_M13 arrival sound and draw behind
// sprites. All nine cells are BAMs in CHITIN.KEY (CEElemX is keyed as the
// seven-char CEELEMX), so the animation keeps its name; the ctor clears
// the height flag after the base set it. Decoding draws nothing.
TEST_CASE("visual projectiles: the spell-hit cells decode to their hits", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* cell;
        BOOL bArmed;
        const char* arrival;
        BOOL bDrawBehind;
    };
    const Spec kHits[] = {
        { 289, "MSumm1X", TRUE, "EFF_M13", TRUE },
        { 290, "ASumm1X", TRUE, "EFF_M13", TRUE },
        { 291, "CEElemX", TRUE, "EFF_M13", TRUE },
        { 292, "CFElemX", TRUE, "EFF_M13", TRUE },
        { 293, "CWElemX", TRUE, "EFF_M13", TRUE },
        { 347, "PortalH", TRUE, "", FALSE },
        { 353, "IllusH", TRUE, "", FALSE },
        { 354, "CCDamaH", TRUE, "", FALSE },
        { 361, "GateX", FALSE, "", FALSE },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kHits) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSpellHit* pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
        REQUIRE(pSpellHit != nullptr);
        CHECK(pSpellHit->m_projectileType == spec.nType - 1);
        CHECK(pSpellHit->m_animation->GetResRef() == CResRef(spec.cell));
        CHECK(pSpellHit->m_visualEffect.m_dwFlags == (spec.bArmed ? 0x20200u : 0x20000u));
        CHECK(pSpellHit->m_arrivalSoundRef == spec.arrival);
        CHECK(pSpellHit->m_bDrawBehind == spec.bDrawBehind);
        CHECK(pSpellHit->m_bHasHeight == FALSE);
        delete pProjectile;
    }
}

// The single-target missiles (DecodeProjectile 0x51EAF0, ctor 0x578110
// CProjectileSingleTarget): one animation flying to one target, speed 20
// and field_1D8 0 from the ctor, m_bHasHeight and field_1C2 left set,
// field_2A0 defaulting to 1. Most entries arm the object's visual effect
// (sub_586A90(TRUE), m_dwFlags 0x20200) and set field_1D8 0x10; ALanceT,
// VSpherT and MFMissT double the speed; MSporeT alone skips the arming
// (bare 0x20000) after displaying effect feedback 4386, and ALanceT alone
// clears field_2A0. Types 269 and 271 share the OFSpheT body byte for
// byte. All eight cells are BAMs in CHITIN.KEY, so the animation keeps
// its name. Decoding draws nothing.
TEST_CASE("visual projectiles: the single-target missiles decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* cell;
        SHORT nSpeed;
        WORD nField1D8;
        const char* fire;
        BOOL bArmed;
        BYTE nField2A0;
    };
    const Spec kFlights[] = {
        { 24, "COrbT", 20, 0, "TRA_23", TRUE, 1 },
        { 251, "IcelanT", 20, 0x10, "TRA_19", TRUE, 1 },
        { 268, "DisintT", 20, 0x10, "TRA_20", TRUE, 1 },
        { 269, "OFSpheT", 20, 0x10, "", TRUE, 1 },
        { 271, "OFSpheT", 20, 0x10, "", TRUE, 1 },
        { 285, "MSporeT", 20, 0, "", FALSE, 1 },
        { 298, "ALanceT", 40, 0x10, "TRA_57", TRUE, 0 },
        { 316, "VSpherT", 40, 1, "TRA_60", TRUE, 1 },
        { 344, "MFMissT", 40, 0, "", TRUE, 1 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kFlights) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSingleTarget* pSingle = dynamic_cast<CProjectileSingleTarget*>(pProjectile);
        REQUIRE(pSingle != nullptr);
        CHECK(pSingle->m_projectileType == spec.nType - 1);
        CHECK(pSingle->m_animation->GetResRef() == CResRef(spec.cell));
        CHECK(pSingle->m_speed == spec.nSpeed);
        CHECK(pSingle->field_1D8 == spec.nField1D8);
        CHECK(pSingle->m_fireSoundRef == spec.fire);
        CHECK(pSingle->m_arrivalSoundRef == "");
        CHECK(pSingle->m_visualEffect.m_dwFlags == (spec.bArmed ? 0x20200u : 0x20000u));
        CHECK(pSingle->m_bHasHeight == TRUE);
        CHECK(pSingle->field_1C2 == TRUE);
        CHECK(pSingle->field_2A0 == spec.nField2A0);
        delete pProjectile;
    }
}

// The sparkle-stream bolts (DecodeProjectile 0x51EAF0, ctor 0x52CA10
// CProjectileSparkle): the magic-missile bolt body — SPMAGMIS cell, speed
// doubled to 40, field_E6 |= 8, field_1DE cleared — then all but type 37
// swap the cell to TRAVEL, all but 51 to 53 arm the alternate palette
// bitmap (SetAltPalette 0x529660: resref, double-size clear, field_29C 1)
// with one of the eight STTRAVL palette bitmaps, and all but 37 set the
// sparkle tail (m_bSparkle, m_nSparkleColour, m_bHasHeight) before the two
// sound refs. 51 to 53 carry only the colour; 65 doubles the flight speed
// once more (80) after re-clearing field_1DE; 37 keeps the bare SPMAGMIS
// cell with no tail. The ctor's palette recolour is keyed off the WORD at
// 0x85E848, which is 0, so no missile recolours. Decoding draws nothing.
TEST_CASE("visual projectiles: the sparkle-stream bolts decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* cell;
        SHORT nSpeed;
        WORD nColour;
        BOOL bSparkle;
        const char* fire;
        const char* altPalette;
    };
    const Spec kSparkles[] = {
        { 37, "SPMAGMIS", 40, 0, FALSE, "TRA_02", nullptr },
        { 47, "TRAVEL", 40, 2, TRUE, "TRA_04A", "STTRAVL1" },
        { 48, "TRAVEL", 40, 4, TRUE, "TRA_04C", "STTRAVL2" },
        { 49, "TRAVEL", 40, 6, TRUE, "", "STTRAVL5" },
        { 50, "TRAVEL", 40, 9, TRUE, "TRA_01", "STTRAVL7" },
        { 51, "TRAVEL", 40, 0xA, TRUE, "", nullptr },
        { 52, "TRAVEL", 40, 1, TRUE, "", nullptr },
        { 53, "TRAVEL", 40, 3, TRUE, "", nullptr },
        { 54, "TRAVEL", 40, 7, TRUE, "", "STTRAVL6" },
        { 55, "TRAVEL", 40, 5, TRUE, "TRA_04D", "STTRAVL3" },
        { 65, "TRAVEL", 80, 0xA, TRUE, "TRA_04C", "STTRAVL2" },
        { 184, "TRAVEL", 40, 0xB, TRUE, "TRA_04A", "STTRAVL4" },
        { 185, "TRAVEL", 40, 0xC, TRUE, "TRA_03", "STTRAVL8" },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kSparkles) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSparkle* pSparkle = dynamic_cast<CProjectileSparkle*>(pProjectile);
        REQUIRE(pSparkle != nullptr);
        CHECK(pSparkle->m_projectileType == spec.nType - 1);
        CHECK(pSparkle->m_animation->GetResRef() == CResRef(spec.cell));
        CHECK(pSparkle->m_speed == spec.nSpeed);
        CHECK(pSparkle->field_1DE == FALSE);
        CHECK(pSparkle->field_E6 == 0x20008);
        CHECK(pSparkle->m_bSparkle == spec.bSparkle);
        CHECK(pSparkle->m_nSparkleColour == spec.nColour);
        CHECK(pSparkle->m_bHasHeight == TRUE);
        CHECK(pSparkle->m_fireSoundRef == spec.fire);
        CHECK(pSparkle->m_arrivalSoundRef == "");
        if (spec.altPalette != nullptr) {
            CHECK(pSparkle->field_1E2.GetResRef() == CResRef(spec.altPalette));
            CHECK(pSparkle->field_1E2.m_bDoubleSize == FALSE);
            CHECK(pSparkle->field_29C == 1);
        } else {
            CHECK(pSparkle->field_29C == 0);
        }
        delete pProjectile;
    }
}

// The arrows (DecodeProjectile 0x51EAF0, ctor 0x5324A0 CProjectileArrow):
// the ARARROW BAM with the palette ranges 5, 4 and 2 recoloured by the
// ctor, then the height flag and the empty sound tails from the case body.
// Type 5 alone points the cell's palette back at the projectile's own and
// recolours range 2 from the ctor's 0x2E to 0x39 after that. Decoding
// draws nothing.
TEST_CASE("visual projectiles: the arrows decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        BYTE nRange2;
    };
    const Spec kArrows[] = {
        { 2, 0x2E },
        { 5, 0x39 },
        { 6, 0x2E },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kArrows) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileArrow* pArrow = dynamic_cast<CProjectileArrow*>(pProjectile);
        REQUIRE(pArrow != nullptr);
        CHECK(pArrow->m_projectileType == spec.nType - 1);
        CHECK(pArrow->m_animation->GetResRef() == CResRef("ARARROW"));
        CHECK(pArrow->m_bHasHeight == TRUE);
        CHECK(pArrow->m_fireSoundRef == "");
        CHECK(pArrow->m_arrivalSoundRef == "");
        CHECK(pArrow->m_palette.m_rangeColors[2] == spec.nRange2);
        CHECK(pArrow->m_palette.m_rangeColors[4] == 0x17);
        CHECK(pArrow->m_palette.m_rangeColors[5] == 0x1B);
        delete pProjectile;
    }
}

// The throwing axes (DecodeProjectile 0x51EAF0, ctor 0x531BE0
// CProjectileAxe): the AXE BAM, the height flag and the two empty sound
// tails from the case body. Decoding draws nothing.
TEST_CASE("visual projectiles: the throwing axes decode to their flights", "[visualprojectile][data]")
{
    const WORD kTypes[] = { 7, 10, 11 };
    WORLD_OR_SKIP(world);
    for (WORD nType : kTypes) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileAxe* pAxe = dynamic_cast<CProjectileAxe*>(pProjectile);
        REQUIRE(pAxe != nullptr);
        CHECK(pAxe->m_projectileType == nType - 1);
        CHECK(pAxe->m_animation->GetResRef() == CResRef("AXE"));
        CHECK(pAxe->m_bHasHeight == TRUE);
        CHECK(pAxe->m_fireSoundRef == "");
        CHECK(pAxe->m_arrivalSoundRef == "");
        delete pProjectile;
    }
}

// The bolts (DecodeProjectile 0x51EAF0, ctor 0x531360 CProjectileBolt):
// the BOLT BAM, the height flag and the two empty sound tails from the
// case body. Decoding draws nothing.
TEST_CASE("visual projectiles: the bolts decode to their flights", "[visualprojectile][data]")
{
    const WORD kTypes[] = { 12, 15, 16 };
    WORLD_OR_SKIP(world);
    for (WORD nType : kTypes) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileBolt* pBolt = dynamic_cast<CProjectileBolt*>(pProjectile);
        REQUIRE(pBolt != nullptr);
        CHECK(pBolt->m_projectileType == nType - 1);
        CHECK(pBolt->m_animation->GetResRef() == CResRef("BOLT"));
        CHECK(pBolt->m_bHasHeight == TRUE);
        CHECK(pBolt->m_fireSoundRef == "");
        CHECK(pBolt->m_arrivalSoundRef == "");
        delete pProjectile;
    }
}

// The sling stones (DecodeProjectile 0x51EAF0, ctor 0x5319A0
// CProjectileSlingStone): the MAGICSTN BAM with palette range 0 recoloured
// to 27 by the ctor, then the height flag and the empty sound tails from
// the case body. Type 41 alone points the cell's palette back at the
// projectile's own and recolours range 0 from the ctor's 27 to 0x1C after
// that. Decoding draws nothing.
TEST_CASE("visual projectiles: the sling stones decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        BYTE nRange0;
    };
    const Spec kStones[] = { { 17, 27 }, { 20, 27 }, { 21, 27 }, { 41, 0x1C } };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kStones) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSlingStone* pStone = dynamic_cast<CProjectileSlingStone*>(pProjectile);
        REQUIRE(pStone != nullptr);
        CHECK(pStone->m_projectileType == spec.nType - 1);
        CHECK(pStone->m_animation->GetResRef() == CResRef("MAGICSTN"));
        CHECK(pStone->m_bHasHeight == TRUE);
        CHECK(pStone->m_fireSoundRef == "");
        CHECK(pStone->m_arrivalSoundRef == "");
        CHECK(pStone->m_palette.m_rangeColors[0] == spec.nRange0);
        delete pProjectile;
    }
}

// The daggers (DecodeProjectile 0x51EAF0, ctor 0x532000
// CProjectileDagger): the DAGGER BAM with the palette ranges 5 and 4
// recoloured by the ctor, then the height flag and the empty sound tails
// from the case body. Decoding draws nothing.
TEST_CASE("visual projectiles: the daggers decode to their flights", "[visualprojectile][data]")
{
    const WORD kTypes[] = { 27, 30, 31 };
    WORLD_OR_SKIP(world);
    for (WORD nType : kTypes) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileDagger* pDagger = dynamic_cast<CProjectileDagger*>(pProjectile);
        REQUIRE(pDagger != nullptr);
        CHECK(pDagger->m_projectileType == nType - 1);
        CHECK(pDagger->m_animation->GetResRef() == CResRef("DAGGER"));
        CHECK(pDagger->m_bHasHeight == TRUE);
        CHECK(pDagger->m_fireSoundRef == "");
        CHECK(pDagger->m_arrivalSoundRef == "");
        CHECK(pDagger->m_palette.m_rangeColors[4] == 30);
        CHECK(pDagger->m_palette.m_rangeColors[5] == 27);
        delete pProjectile;
    }
}

// The darts (DecodeProjectile 0x51EAF0, ctor 0x531790 CProjectileDart):
// the DART BAM, the height flag and the two empty sound tails from the
// case body. Decoding draws nothing.
TEST_CASE("visual projectiles: the darts decode to their flights", "[visualprojectile][data]")
{
    const WORD kTypes[] = { 32, 35, 36 };
    WORLD_OR_SKIP(world);
    for (WORD nType : kTypes) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileDart* pDart = dynamic_cast<CProjectileDart*>(pProjectile);
        REQUIRE(pDart != nullptr);
        CHECK(pDart->m_projectileType == nType - 1);
        CHECK(pDart->m_animation->GetResRef() == CResRef("DART"));
        CHECK(pDart->m_bHasHeight == TRUE);
        CHECK(pDart->m_fireSoundRef == "");
        CHECK(pDart->m_arrivalSoundRef == "");
        delete pProjectile;
    }
}

// The spears (DecodeProjectile 0x51EAF0, ctor 0x531DF0 CProjectileSpear):
// the SPEAR BAM, the height flag and the two empty sound tails from the
// case body. Decoding draws nothing.
TEST_CASE("visual projectiles: the spears decode to their flights", "[visualprojectile][data]")
{
    const WORD kTypes[] = { 56, 59, 60 };
    WORLD_OR_SKIP(world);
    for (WORD nType : kTypes) {
        INFO("type " << nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSpear* pSpear = dynamic_cast<CProjectileSpear*>(pProjectile);
        REQUIRE(pSpear != nullptr);
        CHECK(pSpear->m_projectileType == nType - 1);
        CHECK(pSpear->m_animation->GetResRef() == CResRef("SPEAR"));
        CHECK(pSpear->m_bHasHeight == TRUE);
        CHECK(pSpear->m_fireSoundRef == "");
        CHECK(pSpear->m_arrivalSoundRef == "");
        delete pProjectile;
    }
}

// The burning-hands stream (DecodeProjectile 0x51EAF0, ctor 0x532CA0
// CProjectileSPBRNHND): the SPBRNHND BAM; the case body sets the height
// flag first, then rebinds the cell's palette to the projectile's own and
// recolours range 0 from the ctor's 71 to 0x43, and plays EFF_M20 on
// fire. Decoding draws nothing.
TEST_CASE("visual projectiles: the burning-hands stream decodes to its flight", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    iwd2r_srand(71);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(26, world.pCharacter, 0);
    CHECK(Draws(71, iwd2r_rand_state()) == 0);
    CProjectileSPBRNHND* pHands = dynamic_cast<CProjectileSPBRNHND*>(pProjectile);
    REQUIRE(pHands != nullptr);
    CHECK(pHands->m_projectileType == 25);
    CHECK(pHands->m_animation->GetResRef() == CResRef("SPBRNHND"));
    CHECK(pHands->m_bHasHeight == TRUE);
    CHECK(pHands->m_fireSoundRef == "EFF_M20");
    CHECK(pHands->m_arrivalSoundRef == "");
    CHECK(pHands->m_palette.m_rangeColors[0] == 0x43);
    delete pProjectile;
}

// The flame arrows (DecodeProjectile 0x51EAF0, ctor 0x532720
// CProjectileSPFLMARR): the SPFLMARR BAM with palette range 0 tinted 0x43
// and the seven trail colours 0x41 by the ctor. Types 9/14/29/34 swap the
// animation cell to the mundane AXE/BOLT/DAGGER/DART ammunition and play
// TRA_10; type 4 keeps the flame cell and plays TRA_24. Types 102/103/188
// rebind the cell's palette to the projectile's own, recolour range 0 to
// 0x45/0x44/0x47 and set all seven trail colours to the same value. Every
// variant ends with the height flag and the empty arrival tail. Decoding
// draws nothing.
TEST_CASE("visual projectiles: the flame arrows decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* cell;
        const char* fire;
        BYTE nRange0;
        BYTE nTrail;
    };
    const Spec kFlames[] = {
        { 4, "SPFLMARR", "TRA_24", 0x43, 0x41 },
        { 9, "AXE", "TRA_10", 0x43, 0x41 },
        { 14, "BOLT", "TRA_10", 0x43, 0x41 },
        { 29, "DAGGER", "TRA_10", 0x43, 0x41 },
        { 34, "DART", "TRA_10", 0x43, 0x41 },
        { 102, "SPFLMARR", "TRA_10", 0x45, 0x45 },
        { 103, "SPFLMARR", "TRA_10", 0x44, 0x44 },
        { 188, "SPFLMARR", "TRA_10", 0x47, 0x47 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kFlames) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSPFLMARR* pFlame = dynamic_cast<CProjectileSPFLMARR*>(pProjectile);
        REQUIRE(pFlame != nullptr);
        CHECK(pFlame->m_projectileType == spec.nType - 1);
        CHECK(pFlame->m_animation->GetResRef() == CResRef(spec.cell));
        CHECK(pFlame->m_bHasHeight == TRUE);
        CHECK(pFlame->m_fireSoundRef == spec.fire);
        CHECK(pFlame->m_arrivalSoundRef == "");
        CHECK(pFlame->m_palette.m_rangeColors[0] == spec.nRange0);
        for (int i = 0; i < 7; ++i)
            CHECK(pFlame->m_trailColorRanges[i] == spec.nTrail);
        delete pProjectile;
    }
}

// The exploding weapon missiles (DecodeProjectile 0x51EAF0, ctor 0x52E230
// CProjectileExplodingWeapon): the SPFIREBL burst carrier with palette range
// 0 tinted 0x43 and handed to the cell, mirror bounds 15/11, mirroring on,
// leaf render param 0x80, 16 directions, doubled speed, all seven trail
// colours 'A', trail tick 0 and explosion colour 0x43. Types
// 8/13/18/28/33/57 swap the animation cell to the mundane
// AXE/BOLT/MAGICSTN/DAGGER/DART/SPEAR ammunition — only the bolt plays a
// launch sound (TRA_10). Types 104/204 rebind the cell's palette to the
// projectile's own before recolouring — range 0, the trail colours and the
// explosion colour to 0x45/0x47 — and play TRA_06. Every variant ends with
// the height flag and the empty arrival tail. Decoding draws nothing.
TEST_CASE("visual projectiles: the exploding weapons decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* cell;
        const char* fire;
        BYTE nRange0;
        BYTE nTrail;
        BYTE nExplode;
    };
    const Spec kWeapons[] = {
        { 8, "AXE", "", 0x43, 0x41, 0x43 },
        { 13, "BOLT", "TRA_10", 0x43, 0x41, 0x43 },
        { 18, "MAGICSTN", "", 0x43, 0x41, 0x43 },
        { 28, "DAGGER", "", 0x43, 0x41, 0x43 },
        { 33, "DART", "", 0x43, 0x41, 0x43 },
        { 57, "SPEAR", "", 0x43, 0x41, 0x43 },
        { 104, "SPFIREBL", "TRA_06", 0x45, 0x45, 0x45 },
        { 204, "SPFIREBL", "TRA_06", 0x47, 0x47, 0x47 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kWeapons) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileExplodingWeapon* pWeapon = dynamic_cast<CProjectileExplodingWeapon*>(pProjectile);
        REQUIRE(pWeapon != nullptr);
        CHECK(pWeapon->m_projectileType == spec.nType - 1);
        CHECK(pWeapon->m_animation->GetResRef() == CResRef(spec.cell));
        CHECK(pWeapon->m_bHasHeight == TRUE);
        CHECK(pWeapon->m_fireSoundRef == spec.fire);
        CHECK(pWeapon->m_arrivalSoundRef == "");
        CHECK(pWeapon->m_palette.m_rangeColors[0] == spec.nRange0);
        for (int i = 0; i < 7; ++i)
            CHECK(pWeapon->m_trailColorRanges[i] == spec.nTrail);
        CHECK(pWeapon->m_trailTick == 0);
        CHECK(pWeapon->m_explodeColorRange == spec.nExplode);
        CHECK(pWeapon->field_1CA == 0xF);
        CHECK(pWeapon->field_1CE == 0xB);
        CHECK(pWeapon->field_1C6 == TRUE);
        CHECK(pWeapon->field_1D2 == 0x80);
        CHECK(pWeapon->field_1D8 == 0x10);
        CHECK(pWeapon->m_childProjectileType == 0x4E);
        // The doubled speed, pinned against a directly built sibling.
        CProjectileExplodingWeapon direct;
        CHECK(pWeapon->m_speed == direct.m_speed);
        delete pProjectile;
    }
}

// The exploding flame arrow (DecodeProjectile 0x51EAF0, ctor 0x52E9F0
// CProjectileExplodingFlame): the SPFLMARR burst carrier retuned for the
// flight — animation sequence 0, range 0 recoloured to 0x43 and range 2 to
// 0x39 from the game master bitmap, sixteen directions with the
// tint/mirror/shadow switches cleared, the speed tripled, the axe's launch
// sound TRA_10 over an empty arrival sound, and the default 'A' trail
// colour ranges. The case body only adds the height flag and the two
// sounds; the shared tail stamps type 3-1 = 2. SPFLMARR binds normally on
// this install (keyfile carries the uppercase entry).
TEST_CASE("visual projectiles: the exploding flame decodes to its flight", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    iwd2r_srand(71);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(3, world.pCharacter, 0);
    CHECK(Draws(71, iwd2r_rand_state()) == 0);
    CProjectileExplodingFlame* pFlame = dynamic_cast<CProjectileExplodingFlame*>(pProjectile);
    REQUIRE(pFlame != nullptr);
    CHECK(pFlame->m_projectileType == 2);
    CHECK(pFlame->m_animation->GetResRef() == CResRef("SPFLMARR"));
    CHECK(pFlame->m_bHasHeight == TRUE);
    CHECK(pFlame->m_fireSoundRef == "TRA_10");
    CHECK(pFlame->m_arrivalSoundRef == "");
    CHECK(pFlame->m_palette.m_rangeColors[0] == 0x43);
    CHECK(pFlame->m_palette.m_rangeColors[2] == 0x39);
    for (int i = 0; i < 7; ++i)
        CHECK(pFlame->m_trailColorRanges[i] == 0x41);
    CHECK(pFlame->m_trailTick == 0);
    CHECK(pFlame->m_childProjectileType == 0x4E);
    CHECK(pFlame->field_1D8 == 0x10);
    CHECK(pFlame->field_1BE == FALSE);
    CHECK(pFlame->field_1C2 == FALSE);
    CHECK(pFlame->field_1C6 == FALSE);
    CHECK(pFlame->field_1D4 == FALSE);
    // The tripled speed, pinned against a directly built flame.
    CProjectileExplodingFlame direct;
    CHECK(pFlame->m_speed == direct.m_speed);
    delete pProjectile;
}

// The acid blobs (DecodeProjectile 0x51EAF0, ctor 0x532260
// CProjectileAcidBlob): the ACIDBLOB BAM with palette range 0 recoloured
// to 73 by the ctor, then the height flag and the two empty sound tails
// from the case body. Types 192/193/194 additionally recolour range 0 to
// 0x56/0x6E/0x1A through SetColorRange (which rebinds the cell's palette
// to the projectile's own). Decoding draws nothing.
TEST_CASE("visual projectiles: the acid blobs decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        BYTE nRange0;
    };
    const Spec kBlobs[] = { { 108, 73 }, { 192, 0x56 }, { 193, 0x6E }, { 194, 0x1A } };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kBlobs) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileAcidBlob* pBlob = dynamic_cast<CProjectileAcidBlob*>(pProjectile);
        REQUIRE(pBlob != nullptr);
        CHECK(pBlob->m_projectileType == spec.nType - 1);
        CHECK(pBlob->m_animation->GetResRef() == CResRef("ACIDBLOB"));
        CHECK(pBlob->m_bHasHeight == TRUE);
        CHECK(pBlob->m_fireSoundRef == "");
        CHECK(pBlob->m_arrivalSoundRef == "");
        CHECK(pBlob->m_palette.m_rangeColors[0] == spec.nRange0);
        delete pProjectile;
    }
}

// The standing shafts (DecodeProjectile 0x51EAF0, ctor 0x5348C0
// CProjectileShaft): columns of light stacked from the ground up. The decoder
// picks the animation, transparency, frame mode, duration and the two sound
// tails (and one tint); the palette arm of the ctor stays dormant because
// every shaft decodes with nColor = 0xFF.
TEST_CASE("the shafts decode to their columns", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);

    struct Spec {
        WORD nType;
        const char* sAnimation;
        const char* sFireSound;
        BOOL bTransparent;
        WORD nDuration;
        BOOL bTinted;
    };

    // Four of the shafts run for twice the engine's milliseconds-per-second
    // timescale constant; Call Lightning's bolt is a fixed 20 frames.
    const WORD nScaled = CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2;

    const Spec kSpecs[] = {
        { 23, "CallLiH", "EFF_P19", TRUE, 20, FALSE },
        { 66, "HMightH", "", TRUE, nScaled, FALSE },
        { 146, "FlmStrk", "", FALSE, nScaled, FALSE },
        { 147, "HLYMITE", "", TRUE, nScaled, FALSE },
        { 195, "HLYMITE", "EFF_M10", TRUE, nScaled, TRUE },
    };

    for (const auto& spec : kSpecs) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);

        CProjectileShaft* pShaft = dynamic_cast<CProjectileShaft*>(pProjectile);
        REQUIRE(pShaft != nullptr);
        CHECK(pShaft->m_projectileType == spec.nType - 1);
        CHECK(pShaft->m_animation.GetResRef() == CResRef(spec.sAnimation));
        CHECK(pShaft->m_bTransparent == spec.bTransparent);
        CHECK(pShaft->m_bRandomFrame == FALSE);
        CHECK(pShaft->m_nDuration == spec.nDuration);
        // The shaft flows store no height flag, unlike the BAM families.
        CHECK(pShaft->m_bHasHeight == FALSE);
        CHECK(pShaft->m_fireSoundRef == spec.sFireSound);
        CHECK(pShaft->m_arrivalSoundRef == "");
        if (spec.bTinted) {
            CHECK(pShaft->m_animation.GetTintColor() == 0xFF);
        }
        delete pProjectile;
    }
}

// The dimension door (DecodeProjectile 0x51EAF0, ctor 0x535320
// CProjectile84E940): the empty-palette arm of the ctor runs, the door
// animation opens and closes, and the decoder only adds the sound tails.
TEST_CASE("the dimension door decodes to its flight", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);

    iwd2r_srand(71);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(110, world.pCharacter, 0);
    CHECK(Draws(71, iwd2r_rand_state()) == 0);

    CProjectile84E940* pDoor = dynamic_cast<CProjectile84E940*>(pProjectile);
    REQUIRE(pDoor != nullptr);
    CHECK(pDoor->m_projectileType == 109);
    CHECK(pDoor->m_sAnimation == "SPDimDr");
    CHECK(pDoor->m_animation.GetResRef() == CResRef("SPDimDr2"));
    CHECK(pDoor->m_bPalette == FALSE);
    CHECK(pDoor->m_dwFlags == 0);
    CHECK(pDoor->m_nTransValue == 0);
    CHECK(pDoor->m_bHasHeight == FALSE);
    CHECK(pDoor->m_fireSoundRef == "EFF_M09");
    CHECK(pDoor->m_arrivalSoundRef == "");
    delete pProjectile;
}

// The spell-hit bursts (DecodeProjectile 0x51EAF0, ctor 0x530300
// CProjectile84DFA4): the decoder re-pins field_1D8 to 1 and overrides the
// ctor's colour range (SetColorRange 0x52B150) per type — 0x43/0x41/0x45,
// the same three colours the Visual Spell Hit modes 0x20..0x22 use.
TEST_CASE("the spell-hit bursts decode to their colour ranges", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);

    struct Spec {
        WORD nType;
        WORD nColor;
    };
    const Spec kSpecs[] = {
        { 143, 0x43 },
        { 144, 0x41 },
        { 145, 0x45 },
    };

    for (const Spec& spec : kSpecs) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);

        CProjectile84DFA4* pBurst = dynamic_cast<CProjectile84DFA4*>(pProjectile);
        REQUIRE(pBurst != nullptr);
        CHECK(pBurst->m_projectileType == spec.nType - 1);
        CHECK(pBurst->m_animation->GetResRef() == CResRef("SPBOOM"));
        CHECK(pBurst->field_1D8 == 1);
        CHECK(pBurst->field_1DE == TRUE);
        CHECK(pBurst->m_bHasHeight == FALSE);
        CHECK(pBurst->m_fireSoundRef == "");
        CHECK(pBurst->m_arrivalSoundRef == "");
        delete pProjectile;
    }
}

// The boulder (DecodeProjectile 0x51EAF0, entry 0x51F2E4, ctor 0x531570
// CProjectileBoulder): the decoder's only post-ctor work is pinning the
// height flag and re-assigning both sound refs to the empty string.
TEST_CASE("visual projectiles: the boulder decodes to its flight", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    iwd2r_srand(71);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(375, world.pCharacter, 0);
    CHECK(Draws(71, iwd2r_rand_state()) == 0);
    CProjectileBoulder* pBoulder = dynamic_cast<CProjectileBoulder*>(pProjectile);
    REQUIRE(pBoulder != nullptr);
    CHECK(pBoulder->m_projectileType == 374);
    CHECK(pBoulder->m_bHasHeight == TRUE);
    CHECK(pBoulder->m_fireSoundRef == "");
    CHECK(pBoulder->m_arrivalSoundRef == "");
    delete pProjectile;
}

// The scorcher beam (DecodeProjectile 0x51EAF0, entry 0x51FF03, ctor
// 0x57AEB0 IcewindCProjectileScorcher): the decoder's only post-ctor work
// is re-assigning the fire sound to TRA_17 over the ctor's TRA_M04.
TEST_CASE("visual projectiles: the scorcher decodes to its beam", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    iwd2r_srand(73);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(109, world.pCharacter, 0);
    CHECK(Draws(73, iwd2r_rand_state()) == 0);
    IcewindCProjectileScorcher* pScorcher = dynamic_cast<IcewindCProjectileScorcher*>(pProjectile);
    REQUIRE(pScorcher != nullptr);
    CHECK(pScorcher->m_projectileType == 108);
    CHECK(pScorcher->m_fireSoundRef == "TRA_17");
    delete pProjectile;
}

// The casting glows (DecodeProjectile 0x51EAF0, ctor 0x57CAC0
// CProjectile850F9C): one per school, sound "" and delay 0. The loop
// count is the decoder's third parameter — nUnknown when nonzero, else
// 0x32 (the exe's test al,al two-arm). The glow BAMs live in CHITIN.KEY,
// so the ctor's sequence-length check may rewrite m_nLoops; each decode
// is therefore compared against a direct construction with the same
// count, pinning the arm rather than the BAM's sequence length.
TEST_CASE("visual projectiles: the casting glows read the loop count from the caller", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);

    struct
    {
        WORD nType;
        const char* sSchool;
    } schools[] = {
        { 111, "NecroCG" },
        { 112, "AlterCG" },
        { 113, "EnchaCG" },
        { 114, "AbjurCG" },
        { 115, "IllusCG" },
        { 116, "ConjuCG" },
        { 117, "InvocCG" },
        { 118, "DivinCG" },
    };

    for (const auto& school : schools) {
        for (BYTE nUnknown : { BYTE(0), BYTE(7) }) {
            CAPTURE(school.nType, nUnknown);

            CProjectile* pDecoded = CProjectile::DecodeProjectile(school.nType, world.pCharacter, nUnknown);
            CProjectile850F9C* pGlow = dynamic_cast<CProjectile850F9C*>(pDecoded);
            REQUIRE(pGlow != nullptr);

            CProjectile850F9C direct(school.sSchool, "", (nUnknown != 0) ? nUnknown : 0x32, 0, IcewindCVisualEffect());

            CHECK(pGlow->m_projectileType == school.nType - 1);
            CHECK(pGlow->m_fireSoundRef == direct.m_fireSoundRef);
            CHECK(pGlow->m_nLoops == direct.m_nLoops);
            CHECK(pGlow->m_nDelay == direct.m_nDelay);

            delete pDecoded;
        }
    }
}

// The plain area effects (DecodeProjectile 0x51EAF0): nine no-arg area
// ctors the decoder calls with no post-construction work at all — the ctor
// carries the whole description.
TEST_CASE("the plain area effects decode straight to their ctors", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);

    struct Spec {
        WORD nType;
        const char* sAnimation;
        const char* sSound;
        LONG nRadius;
        LONG nField1D8;
    };
    const Spec kSpecs[] = {
        { 98, "IStormX", "", 200, 1 }, // 0x573460
        { 211, "AStormX", "", 200, 1 }, // 0x571170
        { 214, "ICloudX", "EFF_M40", 100, 1 }, // 0x573600
        { 215, "PFireX", "EFF_P45", 60, 1 }, // 0x574140
        { 216, "IPlaguX", "RNG_P01", 250, 1 }, // 0x573AF0
        { 217, "SSSwarX", "RNG_M02", 250, 16 }, // 0x5745E0
        { 267, "DSpellX", "EFF_M42", 300, 0 }, // 0x571930
        { 279, "DFogX", "RNG_M01", 250, 1 }, // 0x573CC0
        { 306, "", "ARE_P27", 0x15E, 0 }, // 0x575B90
        { 383, "", "", 100, 1 }, // 0x577590
    };

    for (const Spec& spec : kSpecs) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);

        CProjectileArea* pArea = dynamic_cast<CProjectileArea*>(pProjectile);
        REQUIRE(pArea != nullptr);
        CHECK(pArea->m_projectileType == spec.nType - 1);
        CHECK(pArea->m_explosion1.m_animation == spec.sAnimation);
        CHECK(pArea->m_explosion1.m_sound == spec.sSound);
        CHECK(pArea->m_nRadius == spec.nRadius);
        CHECK(pArea->field_1D8 == spec.nField1D8);
        if (spec.nType == 217) {
            // The swarm also flies on SSSwarT with its speed doubled, and
            // throws up to 26 SSSwarR balls in a single hit-modulus-10 burst.
            CHECK(pArea->m_bTravels == TRUE);
            CHECK(pArea->field_1DE == TRUE);
            CHECK(pArea->m_animation->GetResRef() == CResRef("SSSwarT"));
            CHECK(pArea->m_fireSoundRef == CResRef("TRA_18"));
            CHECK(pArea->m_explosion2.m_animation == "SSSwarR");
            CHECK(pArea->m_explosion2.field_2C == TRUE);
            CHECK(pArea->m_explosion2.field_2E == 26);
            CHECK(pArea->m_nTriggerInterval == 10000);
            CHECK(pArea->m_nHitModulus == 10);
            CHECK(pArea->m_nDuration == 45);
            CProjectileSnowballSwarm swarmDirect;
            CHECK(pArea->m_speed == swarmDirect.m_speed);
        }
        if (spec.nType == 383) {
            // The boulder also travels with its own sound pair.
            CHECK(pArea->m_bTravels == TRUE);
            CHECK(pArea->m_fireSoundRef == CResRef("AM6103e"));
            CHECK(pArea->m_arrivalSoundRef == CResRef("AM5101e"));
        }
        delete pProjectile;
    }
}

// The sparkle bursts (DecodeProjectile 0x51EAF0, ctor 0x5297B0
// CProjectileSparkleExplosion): the delayed-burst carrier built on the
// exploding intermediate (0x52CCE0 — child 78, anyone filter, 0x100 strike
// and pre-check ranges, one strike, 100-tick linger), with TRAVEL handed to
// the ctor as the cell (no post-construct swap) and the flight speed left at
// the BAM default 20. Every entry overwrites the ctor's child with the
// burst's own, arms the sparkle tail, sets the trails flag (field_E6 |= 8 —
// load-bearing, the ctor chain leaves 0x20000) and the two sound refs — the
// arrival ref is a real sound here, not the empty string. 153 to 155 skip
// the alternate palette; 182 reuses 157's child (54) with colour 11; 183's
// child (184) is itself a sparkle-stream bolt. Decoding draws nothing.
TEST_CASE("visual projectiles: the sparkle bursts decode to their children", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* altPalette;
        WORD nColour;
        SHORT nChild;
        const char* fire;
        const char* arrival;
    };
    const Spec kBursts[] = {
        { 149, "STTRAVL1", 2, 0x2E, "TRA_04A", "CAS_M08" },
        { 150, "STTRAVL2", 4, 0x2F, "TRA_04C", "EFF_M05" },
        { 151, "STTRAVL5", 6, 0x30, "TRA_04B", "CAS_P03" },
        { 152, "STTRAVL7", 9, 0x31, "", "" },
        { 153, nullptr, 0xA, 0x32, "", "" },
        { 154, nullptr, 1, 0x33, "", "" },
        { 155, nullptr, 3, 0x34, "", "" },
        { 156, "STTRAVL6", 7, 0x35, "", "" },
        { 157, "STTRAVL3", 5, 0x36, "TRA_04A", "CAS_P02" },
        { 182, "STTRAVL4", 0xB, 0x36, "", "" },
        { 183, "STTRAVL8", 0xC, 0xB8, "TRA_03", "EFF_M04" },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kBursts) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSparkleExplosion* pBurst = dynamic_cast<CProjectileSparkleExplosion*>(pProjectile);
        REQUIRE(pBurst != nullptr);
        CHECK(pBurst->m_projectileType == spec.nType - 1);
        CHECK(pBurst->m_animation->GetResRef() == CResRef("TRAVEL"));
        CHECK(pBurst->m_speed == 20);
        CHECK(pBurst->field_1DE == TRUE);
        CHECK(pBurst->field_E6 == 0x20008);
        CHECK(pBurst->m_bSparkle == TRUE);
        CHECK(pBurst->m_nSparkleColour == spec.nColour);
        CHECK(pBurst->m_bHasHeight == TRUE);
        CHECK(pBurst->m_childProjectileType == spec.nChild);
        CHECK(pBurst->m_fireSoundRef == spec.fire);
        CHECK(pBurst->m_arrivalSoundRef == spec.arrival);
        // The exploding-intermediate ctor's arming, untouched by the case.
        CHECK(pBurst->m_strikeRange == 0x100);
        CHECK(pBurst->m_preCheckRange == 0x100);
        CHECK(pBurst->m_strikesLeft == 1);
        CHECK(pBurst->m_lingerPeriod == 100);
        if (spec.altPalette != nullptr) {
            CHECK(pBurst->field_1E2.GetResRef() == CResRef(spec.altPalette));
            CHECK(pBurst->field_1E2.m_bDoubleSize == FALSE);
            CHECK(pBurst->field_29C == 1);
        } else {
            CHECK(pBurst->field_29C == 0);
        }
        delete pProjectile;
    }
}

// The bare exploding missiles (DecodeProjectile 0x51EAF0, ctor 0x52CCE0
// CProjectileExploding, one entry per type): each allocates on the "MAGMISS"
// cell — which CHITIN.KEY only holds as a type-4 entry, not the BAM type
// 1000 the CVidCell helper looks up, so SetResRef misses and stores "" — and
// tweaks the ctor's arming: 93 swaps the child to 22 and keeps the BAM
// ctor's field_1DE TRUE (the only type here that flies); 94, 158, 159 and
// 205 ground it (field_1DE FALSE) and re-store the ctor's child 78; 205
// alone tightens the strike and pre-check ranges from 0x100 to 0x50. 158
// and 159 also narrow the anyone target type by the caster's EA against
// the GOODCUTOFF global, both arms storing one: at-or-below picks
// EA_GOODCUTOFF (158) / EA_NOTGOOD (159), above it the other way round.
// Decoding draws nothing.
TEST_CASE("visual projectiles: the bare exploding missiles decode to their tweaks", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        SHORT nChild;
        BOOL bFlying;
        WORD nStrike;
        bool bPick;
        BYTE nTargetEA;
    };
    const Spec kExploding[] = {
        { 93, 0x16, TRUE, 0x100, false, 0 },
        { 94, 0x4E, FALSE, 0x100, false, 0 },
        { 158, 0x4E, FALSE, 0x100, true, CAIObjectType::EA_GOODCUTOFF },
        { 159, 0x4E, FALSE, 0x100, true, CAIObjectType::EA_NOTGOOD },
        { 205, 0x4E, FALSE, 0x50, false, 0 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kExploding) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileExploding* pMissile = dynamic_cast<CProjectileExploding*>(pProjectile);
        REQUIRE(pMissile != nullptr);
        CHECK(pMissile->m_projectileType == spec.nType - 1);
        CHECK(pMissile->m_animation->GetResRef() == CResRef(""));
        CHECK(pMissile->m_speed == 20);
        CHECK(pMissile->field_1DE == spec.bFlying);
        CHECK(pMissile->field_E6 == 0x20000);
        CHECK(pMissile->m_childProjectileType == spec.nChild);
        CHECK(pMissile->m_strikeRange == spec.nStrike);
        CHECK(pMissile->m_preCheckRange == spec.nStrike);
        CHECK(pMissile->m_strikesLeft == 1);
        CHECK(pMissile->m_lingerPeriod == 100);
        CHECK(pMissile->m_fireSoundRef == CResRef(""));
        CHECK(pMissile->m_arrivalSoundRef == CResRef(""));
        if (spec.bPick) {
            // The PC caster (EA_PC <= EA_GOODCUTOFF) takes the pick branch.
            CHECK(pMissile->m_targetType.m_nEnemyAlly == spec.nTargetEA);
        } else {
            CHECK(pMissile->m_targetType.m_nEnemyAlly == CAIObjectType::ANYONE.m_nEnemyAlly);
        }
        delete pProjectile;
    }
}

// The recursing bolt wrappers (DecodeProjectile 0x51EAF0, six entries
// 0x524092-0x5245E8, no ctor of their own): each re-decodes an already
// registered bolt as its carrier — its own type minus 11 (160-168, entry
// 0x524092) or minus 20 (169-177, 0x524179), both landing in the sparkle
// bursts 149-157, the fixed bursts 182/183 (178-181, entries 0x524260,
// 0x524342, 0x524424, 0x524506) or the exploding missile 94 (186, 0x5245E8)
// — and the common exit (0x528E1C) then re-stamps the carrier with the
// wrapper's own type minus one. 160-181 retarget the carrier exactly like
// 158/159: the guard always reads the GOODCUTOFF global, and both arms
// store one — GOODCUTOFF at-or-below for 160-168, 180 and 181 (NOTGOOD
// above), NOTGOOD at-or-below for 169-177, 178 and 179 (GOODCUTOFF above).
// 186 picks no target but flips the carrier's m_bCheckNonSprites.
// Decoding draws nothing.
TEST_CASE("visual projectiles: the recursing bolt wrappers decode via their carrier", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        WORD nInner;
        SHORT nChild;
        bool bPick;
        BYTE nTargetEA;
    };
    const Spec kWrappers[] = {
        { 160, 149, 0x2E, true, CAIObjectType::EA_GOODCUTOFF },
        { 161, 150, 0x2F, true, CAIObjectType::EA_GOODCUTOFF },
        { 162, 151, 0x30, true, CAIObjectType::EA_GOODCUTOFF },
        { 163, 152, 0x31, true, CAIObjectType::EA_GOODCUTOFF },
        { 164, 153, 0x32, true, CAIObjectType::EA_GOODCUTOFF },
        { 165, 154, 0x33, true, CAIObjectType::EA_GOODCUTOFF },
        { 166, 155, 0x34, true, CAIObjectType::EA_GOODCUTOFF },
        { 167, 156, 0x35, true, CAIObjectType::EA_GOODCUTOFF },
        { 168, 157, 0x36, true, CAIObjectType::EA_GOODCUTOFF },
        { 169, 149, 0x2E, true, CAIObjectType::EA_NOTGOOD },
        { 170, 150, 0x2F, true, CAIObjectType::EA_NOTGOOD },
        { 171, 151, 0x30, true, CAIObjectType::EA_NOTGOOD },
        { 172, 152, 0x31, true, CAIObjectType::EA_NOTGOOD },
        { 173, 153, 0x32, true, CAIObjectType::EA_NOTGOOD },
        { 174, 154, 0x33, true, CAIObjectType::EA_NOTGOOD },
        { 175, 155, 0x34, true, CAIObjectType::EA_NOTGOOD },
        { 176, 156, 0x35, true, CAIObjectType::EA_NOTGOOD },
        { 177, 157, 0x36, true, CAIObjectType::EA_NOTGOOD },
        { 178, 182, 0x36, true, CAIObjectType::EA_NOTGOOD },
        { 179, 183, 0xB8, true, CAIObjectType::EA_NOTGOOD },
        { 180, 182, 0x36, true, CAIObjectType::EA_GOODCUTOFF },
        { 181, 183, 0xB8, true, CAIObjectType::EA_GOODCUTOFF },
        { 186, 94, 0x4E, false, 0 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kWrappers) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileExploding* pMissile = dynamic_cast<CProjectileExploding*>(pProjectile);
        REQUIRE(pMissile != nullptr);
        // The common exit re-stamps the carrier with the wrapper's type.
        CHECK(pMissile->m_projectileType == spec.nType - 1);
        // The right inner decode ran: its own child survived the wrapper.
        CHECK(pMissile->m_childProjectileType == spec.nChild);
        CHECK(pMissile->m_speed == 20);
        if (spec.nType == 186) {
            // Carrier 94: the grounded exploding missile, check-non-sprites
            // flipped, no target picked.
            CHECK(dynamic_cast<CProjectileSparkleExplosion*>(pProjectile) == nullptr);
            CHECK(pMissile->m_animation->GetResRef() == CResRef(""));
            CHECK(pMissile->field_1DE == FALSE);
            CHECK(pMissile->field_E6 == 0x20000);
            CHECK(pMissile->m_bCheckNonSprites == 1);
            CHECK(pMissile->m_targetType.m_nEnemyAlly == CAIObjectType::ANYONE.m_nEnemyAlly);
        } else {
            // Carrier 149-157/182/183: a sparkle burst, flying, trails on.
            CProjectileSparkleExplosion* pBurst = dynamic_cast<CProjectileSparkleExplosion*>(pProjectile);
            REQUIRE(pBurst != nullptr);
            CHECK(pBurst->m_animation->GetResRef() == CResRef("TRAVEL"));
            CHECK(pBurst->field_1DE == TRUE);
            CHECK(pBurst->field_E6 == 0x20008);
            CHECK(pBurst->m_bCheckNonSprites == 0);
            CHECK(pBurst->m_strikeRange == 0x100);
            CHECK(pBurst->m_strikesLeft == 1);
            if (spec.bPick) {
                // The PC caster (EA_PC <= EA_GOODCUTOFF) takes the pick branch.
                CHECK(pBurst->m_targetType.m_nEnemyAlly == spec.nTargetEA);
            }
        }
        if (!spec.bPick) {
            CHECK(pMissile->m_targetType.m_nEnemyAlly == CAIObjectType::ANYONE.m_nEnemyAlly);
        }
        delete pProjectile;
    }
}

// The EA cutoff picks (DecodeProjectile 0x51EAF0, entries 0x523E42-0x5245E8)
// have two storing arms: the branch on the caster's EA against the
// GOODCUTOFF global never skips the pick, a hostile caster just retargets
// the other way. A caster held above the cutoff (EA_EVILCUTOFF) gets the
// else arm: the other EA for 158 and the 160-168/180-181 bodies, back to
// GOODCUTOFF for the 169-177 body. The sprite's EA is flipped only around
// the decode; the world is a per-process singleton.
TEST_CASE("visual projectiles: a caster above the cutoff gets the other target EA", "[visualprojectile][data]")
{
    static_assert(CAIOBJECTTYPE_EA_EVILCUTOFF > CAIOBJECTTYPE_EA_GOODCUTOFF,
        "the hostile caster must sit above the cutoff");

    struct Spec {
        WORD nType;
        BYTE nElseEA;
    };
    const Spec kElsePicks[] = {
        { 158, CAIObjectType::EA_NOTGOOD },
        { 160, CAIObjectType::EA_NOTGOOD },
        { 169, CAIObjectType::EA_GOODCUTOFF },
        { 180, CAIObjectType::EA_NOTGOOD },
    };
    WORLD_OR_SKIP(world);
    const BYTE nSavedEA = world.pCharacter->m_typeAI.m_nEnemyAlly;
    for (const Spec& spec : kElsePicks) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        world.pCharacter->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_EVILCUTOFF;
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        world.pCharacter->m_typeAI.m_nEnemyAlly = nSavedEA;
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileExploding* pMissile = dynamic_cast<CProjectileExploding*>(pProjectile);
        REQUIRE(pMissile != nullptr);
        CHECK(pMissile->m_targetType.m_nEnemyAlly == spec.nElseEA);
        delete pProjectile;
    }
}

namespace {

// An area projectile that records the targets it would fire its secondary
// projectile at instead of firing it (CreateSecondary, vtable 0x98).
template <class T>
class Recording : public T {
public:
    void CreateSecondary(LONG nTargetId) override
    {
        CGameObject* pObj;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTargetId, CGameObjectArray::THREAD_ASYNCH, &pObj, INFINITE) == CGameObjectArray::SUCCESS) {
            std::cerr << "SEC " << std::hex << nTargetId << " '" << (pObj->GetObjectType() == CGameObject::TYPE_SPRITE ? static_cast<CGameSprite*>(pObj)->GetName() : "?") << "' " << std::dec << pObj->GetPos().x << "," << pObj->GetPos().y << std::endl;
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        m_secondaries.push_back(nTargetId);
    }

    std::vector<LONG> m_secondaries;
};

// Goblins in a row from `near`, on open ground.
std::vector<CGameSprite*> AddGoblins(CGameArea* pArea, const CPoint& near, size_t count)
{
    std::vector<CGameSprite*> goblins;
    for (size_t n = 0; n < count; n++) {
        CPoint pos;
        if (!DryGround(pArea, CPoint(near.x + 40 * static_cast<LONG>(n), near.y), pos)) {
            break;
        }
        CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
        if (pGoblin == NULL) {
            break;
        }
        goblins.push_back(pGoblin);
    }
    pArea->SortLists();
    return goblins;
}

// The secondary projectiles (type 79, CProjectile84DF1C) the area holds in
// its add lists, by target.
std::vector<LONG> AddedSecondaries(CGameArea* pArea)
{
    std::vector<LONG> targets;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CProjectile84DF1C* pSecondary = dynamic_cast<CProjectile84DF1C*>(pObject)) {
            targets.push_back(pSecondary->m_targetId);
        }
    }
    return targets;
}

}

// Hold Person (type 190, CProjectileArea84F9C4::ApplyToTargets 0x572F90):
// one number, rand() % 4; it keeps the sprites in the list, in order, up to
// and including the 1st to 4th humanoid (general G_HUMANOID). Ids that are
// not in the object array are skipped.
TEST_CASE("visual projectiles: Hold Person holds the sprites up to the first to fourth humanoid", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(world.pCharacter->GetPos().x + 80, world.pCharacter->GetPos().y + 30), 6);
    REQUIRE(goblins.size() == 6);
    const BYTE generals[] = { CAIObjectType::G_HUMANOID,
        CAIObjectType::G_MONSTER,
        CAIObjectType::G_HUMANOID,
        CAIObjectType::G_HUMANOID,
        CAIObjectType::G_HUMANOID,
        CAIObjectType::G_HUMANOID };
    std::list<LONG> targets;
    targets.push_back(CGameObjectArray::INVALID_INDEX);
    for (size_t n = 0; n < goblins.size(); n++) {
        goblins[n]->m_typeAI.m_nGeneral = generals[n];
        targets.push_back(goblins[n]->GetId());
    }
    // The goblin each roll stops at: the first, third, fourth or fifth.
    const size_t last[] = { 0, 2, 3, 4 };

    bool seen[4] = {};
    for (unsigned int seed = 1; seed <= 24; seed++) {
        INFO("seed " << seed);
        const int nRoll = RandAt(seed, 1) % 4;
        seen[nRoll] = true;
        std::unique_ptr<Recording<CProjectileArea84F9C4>> pHold(new Recording<CProjectileArea84F9C4>());
        std::list<LONG> list = targets;
        iwd2r_srand(seed);
        pHold->ApplyToTargets(list);
        CHECK(Draws(seed, iwd2r_rand_state()) == 1);
        std::vector<LONG> expected;
        for (size_t n = 0; n <= last[nRoll]; n++) {
            expected.push_back(goblins[n]->GetId());
        }
        CHECK(pHold->m_secondaries == expected);
    }
    CHECK((seen[0] && seen[1] && seen[2] && seen[3]));

    // Without enough humanoids it keeps every sprite.
    for (CGameSprite* pGoblin : goblins) {
        pGoblin->m_typeAI.m_nGeneral = CAIObjectType::G_ANIMAL;
    }
    std::unique_ptr<Recording<CProjectileArea84F9C4>> pHold(new Recording<CProjectileArea84F9C4>());
    std::list<LONG> list = targets;
    pHold->ApplyToTargets(list);
    CHECK(pHold->m_secondaries.size() == goblins.size());
}

// Sleep (type 42, CProjectileArea84FEA4::ApplyToTargets 0x574430): two
// numbers, 2 + rand() % 4 + rand() % 4 levels; it takes the sprites below
// level 5, in order, whose level still fits in what is left.
TEST_CASE("visual projectiles: Sleep takes the sprites below level 5 while their levels fit", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(world.pCharacter->GetPos().x + 80, world.pCharacter->GetPos().y + 30), 6);
    REQUIRE(goblins.size() == 6);
    const BYTE levels[] = { 1, 5, 3, 2, 4, 1 };
    std::list<LONG> targets;
    targets.push_back(CGameObjectArray::INVALID_INDEX);
    for (size_t n = 0; n < goblins.size(); n++) {
        goblins[n]->GetDerivedStats()->m_nLevel = levels[n];
        targets.push_back(goblins[n]->GetId());
    }

    bool seen[9] = {};
    for (unsigned int seed = 1; seed <= 200; seed++) {
        INFO("seed " << seed);
        const int nLevels = 2 + RandAt(seed, 1) % 4 + RandAt(seed, 2) % 4;
        seen[nLevels] = true;
        std::vector<LONG> expected;
        int nLeft = nLevels;
        for (size_t n = 0; n < goblins.size(); n++) {
            if (levels[n] < 5 && nLeft - levels[n] >= 0) {
                expected.push_back(goblins[n]->GetId());
                nLeft -= levels[n];
            }
        }
        std::unique_ptr<Recording<CProjectileArea84FEA4>> pSleep(new Recording<CProjectileArea84FEA4>());
        std::list<LONG> list = targets;
        iwd2r_srand(seed);
        pSleep->ApplyToTargets(list);
        CHECK(Draws(seed, iwd2r_rand_state()) == 2);
        CHECK(pSleep->m_secondaries == expected);
        // Two levels take the first goblin and the last one; eight take
        // all of them but the level 5 one and the level 4 one after the 1,
        // 3 and 2.
        if (nLevels == 2) {
            CHECK(expected == std::vector<LONG> { goblins[0]->GetId(), goblins[5]->GetId() });
        } else if (nLevels == 8) {
            CHECK(expected == std::vector<LONG> { goblins[0]->GetId(), goblins[2]->GetId(), goblins[3]->GetId(), goblins[5]->GetId() });
        }
    }
    for (int nLevels = 2; nLevels <= 8; nLevels++) {
        INFO("levels " << nLevels);
        CHECK(seen[nLevels]);
    }
}

namespace {

// The add-effect messages the handler holds, drained (the caller owns and
// deletes them; each one owns its effect).
std::vector<CMessageAddEffect*> AddEffectMessages()
{
    std::vector<CMessageAddEffect*> messages;
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        CMessage* pMessage = handler.m_messageList.RemoveHead();
        CMessageAddEffect* pAdd = dynamic_cast<CMessageAddEffect*>(pMessage);
        if (pAdd != NULL) {
            messages.push_back(pAdd);
        } else {
            delete pMessage;
        }
    }
    return messages;
}

// CUtil::UtilRandInt's math on a known rand() value.
int Scaled(int nRand, int nRange, int nWeight)
{
    int nValue = nWeight + ((nRange * nRand) >> 15);
    if (nValue >= nRange) {
        nValue = nRange - 1;
    }
    if (nValue < 0) {
        nValue = 0;
    }
    return nValue;
}

}

// Turn Undead (type 376, CProjectileArea850A38::ApplyToTargets 0x576DC0):
// three numbers, a d20 turn roll and two d6 thresholds; the list is first
// rewritten to its sprites above level -1 (the 0x577380 filter), then only
// the undead that are not already turned and whose level fits both the
// turn roll and the threshold stay, and each of them is sent the effects of
// one EffTU spell as add-effect messages whose caller is the turning
// sprite. The spell is EffTU1/EffTU3 when the turner's turn undead level
// reaches twice the target's level (Evil/other turners), EffTU2/EffTU4
// when it does not.
TEST_CASE("visual projectiles: Turn Undead turns the undead that fit the rolls", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pTurner = world.pCharacter;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(pTurner->GetPos().x + 80, pTurner->GetPos().y + 30), 6);
    REQUIRE(goblins.size() == 6);
    // Goblin 0 fits every roll at level 0; 1 fits while the rolls reach 1;
    // 2 is already turned; 3 is not undead; 4 is above every roll (the best
    // d20 only reaches turn roll 3); 5 never fits. The filter's "above level
    // -1" guard reads the level through GetAtOffset's class-level byte
    // (0x4471c3, zero-extended), so it can never fire for a real sprite and
    // only the entry whose share fails is dropped.
    const BYTE levels[] = { 0, 1, 1, 1, 4, 50 };
    std::list<LONG> targets;
    targets.push_back(CGameObjectArray::INVALID_INDEX);
    for (size_t n = 0; n < goblins.size(); n++) {
        goblins[n]->GetDerivedStats()->m_nLevel = levels[n];
        goblins[n]->m_typeAI.m_nGeneral = (n == 3) ? CAIObjectType::G_MONSTER : CAIObjectType::G_UNDEAD;
        targets.push_back(goblins[n]->GetId());
    }
    goblins[2]->GetDerivedStats()->m_spellStates[SPLSTATE_TURNED] = true;

    // The turner's maths, mirrored: charisma modifier, luck and turn undead
    // level decide with the three rolls.
    const INT nChrMod = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetAbilityScoreModifier(pTurner->GetDerivedStats()->m_nCHR);
    const int nLuck = pTurner->GetDerivedStats()->m_nLuck;
    const SHORT nTurnLevel = pTurner->GetTurnUndeadLevel();

    const BYTE nOldAlignment = pTurner->m_typeAI.m_nAlignment;
    for (int evil = 0; evil < 2; evil++) {
        INFO((evil ? "evil " : "good ") << "turner");
        pTurner->m_typeAI.m_nAlignment = evil ? CAIObjectType::CHAOTIC_EVIL : CAIObjectType::NEUTRAL_GOOD;

        // Seeds whose first draw spreads the d20 over the roll's range: turn
        // rolls below 0, exactly 0, and 1..3 (the maximum this charisma and
        // turn undead level can reach). Small consecutive seeds would not do:
        // their first draw stays under 32768/20, so the d20 is always 0.
        const unsigned int seeds[] = { 1, 2497, 4504, 5006, 6009, 6511, 8016, 9521 };
        bool seenG0 = false;
        bool seenG1 = false;
        for (unsigned int seed : seeds) {
            INFO("seed " << seed);
            const int nRoll = nChrMod + 1 + Scaled(RandAt(seed, 1), 20, nLuck);
            const int nTurnRoll = (nRoll - 10 >= 0)
                ? (nRoll - 10) / 3 + nTurnLevel
                : (nRoll - 9) / 3 - 1 + nTurnLevel;
            const int nThreshold = nChrMod + Scaled(RandAt(seed, 2), 6, nLuck) + Scaled(RandAt(seed, 3), 6, nLuck) + 2;
            seenG0 = seenG0 || nTurnRoll >= 0;
            seenG1 = seenG1 || (nTurnRoll >= levels[1] && nThreshold >= levels[1]);

            std::unique_ptr<CProjectileArea850A38> pTurn(new CProjectileArea850A38());
            pTurn->m_sourceId = pTurner->GetId();
            std::list<LONG> list = targets;
            ClearMessages();
            iwd2r_srand(seed);
            pTurn->ApplyToTargets(list);
            CHECK(Draws(seed, iwd2r_rand_state()) == 3);

            // The filter rewrites the list to the sprites it could take and
            // whose type and level pass, in order.
            std::vector<LONG> kept;
            for (size_t n = 0; n < goblins.size(); n++) {
                kept.push_back(goblins[n]->GetId());
            }
            CHECK(list == std::list<LONG>(kept.begin(), kept.end()));

            // Only goblin 0 and, while its level fits both rolls, goblin 1
            // are turned; each of them gets one message per effect of the
            // tier's spell, so the targets are compared per unique sprite.
            std::vector<CMessageAddEffect*> messages = AddEffectMessages();
            std::vector<LONG> turned;
            for (CMessageAddEffect* pMessage : messages) {
                CHECK(pMessage->GetSource() == pTurner->GetId());
                if (std::find(turned.begin(), turned.end(), pMessage->GetTarget()) == turned.end()) {
                    turned.push_back(pMessage->GetTarget());
                }
            }
            for (size_t n = 0; n < turned.size(); n++) {
                CHECK((turned[n] == goblins[0]->GetId() || turned[n] == goblins[1]->GetId()));
            }
            if (nTurnRoll >= levels[1] && nThreshold >= levels[1]) {
                CHECK(turned.size() == 2);
                CHECK(turned[0] == goblins[0]->GetId());
                CHECK(turned[1] == goblins[1]->GetId());
            } else if (nTurnRoll >= levels[0]) {
                CHECK(turned.size() == 1);
                CHECK(turned[0] == goblins[0]->GetId());
            } else {
                CHECK(turned.empty());
            }

            // Each message carries one EffTU effect: its caster level is the
            // turner's turn undead level, and effect 280's die count is the
            // turner's id, so the turned state remembers who turned it.
            for (CMessageAddEffect* pMessage : messages) {
                CGameEffect* pEffect = pMessage->m_effect;
                CHECK(pEffect->m_casterLevel == static_cast<DWORD>(nTurnLevel));
                const LONG nLevel = (pMessage->GetTarget() == goblins[0]->GetId()) ? levels[0] : levels[1];
                const char* szTier;
                if (evil) {
                    szTier = (nTurnLevel >= 2 * nLevel) ? "EffTU1" : "EffTU2";
                } else {
                    szTier = (nTurnLevel >= 2 * nLevel) ? "EffTU3" : "EffTU4";
                }
                CHECK(pEffect->m_sourceRes == szTier);
                if (pEffect->m_effectID == 280) {
                    CHECK(pEffect->m_numDice == static_cast<DWORD>(pTurner->GetId()));
                }
            }
            for (CMessageAddEffect* pMessage : messages) {
                delete pMessage;
            }
        }
        CHECK(seenG0);
        CHECK(seenG1);
    }
    pTurner->m_typeAI.m_nAlignment = nOldAlignment;

    // A source that is not in the object array turns nobody (and, as in the
    // original, the share it failed to take is still released).
    std::unique_ptr<CProjectileArea850A38> pTurn(new CProjectileArea850A38());
    pTurn->m_sourceId = CGameObjectArray::INVALID_INDEX;
    std::list<LONG> list = targets;
    ClearMessages();
    iwd2r_srand(97);
    pTurn->ApplyToTargets(list);
    CHECK(Draws(97, iwd2r_rand_state()) == 0);
    CHECK(AddEffectMessages().empty());
}

// Cloudkill (type 187, CProjectileArea84F310 0x571310) does not travel: it
// arrives where it is fired (CProjectileArea::Fire 0x56F820, OnArrival
// 0x56F410) and its explosion object takes its three descriptions. It
// checks for targets every 10 updates (m_nTriggerInterval) and hits each
// one inside on its every 10th check (m_nHitModulus), so once every 100
// updates, until its 1000 updates are over. No rand draws.
TEST_CASE("visual projectiles: Cloudkill lingers 1000 updates and hits who stands in it every 100", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(pSprite->GetPos().x + 400, pSprite->GetPos().y + 30), 1);
    REQUIRE(goblins.size() == 1);
    CGameSprite* pGoblin = goblins.front();
    ClearMessages();

    Recording<CProjectileArea84F310>* pCloud = new Recording<CProjectileArea84F310>();
    pCloud->m_projectileType = 186;
    iwd2r_srand(61);
    pCloud->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, pGoblin->GetPos(), 0, 0);
    const LONG id = pCloud->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pCloud->m_bExploded == TRUE);
    CHECK(pCloud->GetPos() == pGoblin->GetPos());
    REQUIRE(pCloud->m_explosion3.m_pPoints != nullptr);
    std::vector<CGameObject84F0DC*> explosions;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameObject84F0DC* pExplosion = dynamic_cast<CGameObject84F0DC*>(pObject)) {
            explosions.push_back(pExplosion);
        }
    }
    REQUIRE(explosions.size() == 1);
    CGameObject84F0DC* pExplosion = explosions.front();
    CHECK(pExplosion->GetPos() == pGoblin->GetPos());
    CHECK(pExplosion->m_nDuration == 1000);
    CHECK(pExplosion->m_animation.GetResRef() == CResRef("CloudKX"));
    CHECK(pExplosion->m_explosion2.m_animation == "CloudKR");
    CHECK(pExplosion->m_explosion2.field_2E == 13);
    CHECK(pExplosion->m_explosion3.m_animation == "CloudKA");
    CHECK(pExplosion->m_explosion3.m_pPoints == pCloud->m_explosion3.m_pPoints);
    CHECK(pExplosion->m_nWallMode == CGameObject84F0DC::WALL_STOP);
    pArea->SortLists();
    ClearMessages();

    std::vector<int> hits;
    int updates = 0;
    while (Alive(id) && updates < 2000) {
        const size_t before = pCloud->m_secondaries.size();
        pCloud->AIUpdate();
        updates++;
        if (Alive(id) && pCloud->m_secondaries.size() != before) {
            // The cloud's own explosion object stands in it too, and its
            // type (a Set of NOT_SPRITE, whose subRace byte 0xFF the copy
            // maps to 0) still matches the cloud's default target type, so
            // both it and the goblin are hit each trigger.
            REQUIRE(pCloud->m_secondaries.size() == before + 2);
            CHECK(pCloud->m_secondaries.back() == pGoblin->GetId());
            hits.push_back(updates);
        }
    }
    CHECK(updates == 1000);
    CHECK(Draws(61, iwd2r_rand_state()) == 0);
    const std::vector<int> expected = { 1, 101, 201, 301, 401, 501, 601, 701, 801, 901 };
    CHECK(hits == expected);

    pExplosion->RemoveFromArea();
    pArea->SortLists();
    ClearMessages();
}

// The breath (type 349, CProjectileArea85072C 0x575EA0): fired, it does not
// arrive (its OnArrival, 0x799CA0, is empty); its AIUpdate (0x576040)
// counts updates and arrives on the seventh (CProjectileArea::OnArrival:
// the DBreatH explosion), then works as an area projectile from that one
// on. No rand draws.
TEST_CASE("visual projectiles: a breath arrives on its seventh update", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(pSprite->GetPos().x + 400, pSprite->GetPos().y + 30), 1);
    REQUIRE(goblins.size() == 1);
    CGameSprite* pGoblin = goblins.front();
    ClearMessages();

    Recording<CProjectileArea85072C>* pBreath = new Recording<CProjectileArea85072C>();
    pBreath->m_projectileType = 348;
    iwd2r_srand(63);
    pBreath->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, pGoblin->GetPos(), 0, 0);
    const LONG id = pBreath->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pBreath->m_bExploded == FALSE);
    CHECK(AddedOne<CProjectileArea85072C>(pArea) == pBreath);
    CHECK(pBreath->m_nTicks == 0);

    for (int update = 1; update <= 6; update++) {
        pBreath->AIUpdate();
        CHECK(pBreath->m_nTicks == update);
        CHECK(pBreath->m_bExploded == FALSE);
    }
    CHECK(AddedObjects(pArea).size() == 1);
    CHECK(pBreath->m_secondaries.empty());
    // The engine sorts the area's lists before its update pump; the area
    // projectile's GetTargets walks the sorted list from its own place in
    // it (m_posVertList).
    pArea->SortLists();

    pBreath->AIUpdate();
    CHECK(pBreath->m_nTicks == 7);
    CHECK(pBreath->m_bExploded == TRUE);
    CHECK(pBreath->field_1DE == FALSE);
    CGameObject84F0DC* pExplosion = nullptr;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameObject84F0DC* pFound = dynamic_cast<CGameObject84F0DC*>(pObject)) {
            pExplosion = pFound;
        }
    }
    REQUIRE(pExplosion != nullptr);
    CHECK(pExplosion->m_animation.GetResRef() == CResRef("DBreatH"));
    CHECK(pExplosion->m_explosion3.m_animation == "DBreatA");
    CHECK(pExplosion->m_explosion3.field_36 == TRUE);
    // The same update checks for targets: every sprite inside the 200
    // radius, not just the goblin it was fired at. The goblin's ground is
    // within 200 of Eldgull, a creature standing in the start area, so it
    // is hit too, after the goblin in the list's order (sorted by y).
    REQUIRE(pBreath->m_secondaries.size() == 2);
    CHECK(pBreath->m_secondaries.front() == pGoblin->GetId());
    CHECK(pBreath->m_secondaries[1] != pGoblin->GetId());
    CHECK(Draws(63, iwd2r_rand_state()) == 0);

    CHECK(UpdateUntilGone(id, 100) == 44);
    pExplosion->RemoveFromArea();
    pArea->SortLists();
    ClearMessages();
}

// Flame Strike (type 67, CProjectileArea84F754::Fire 0x572540): a column of
// flame (CProjectileShaft "FStrikH", 28 updates) fired with the same
// arguments, then the area projectile, which has no explosion animation.
TEST_CASE("visual projectiles: Flame Strike fires a column of flame and its area", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(67, pSprite, 0);
    CProjectileArea84F754* pStrike = dynamic_cast<CProjectileArea84F754*>(pProjectile);
    REQUIRE(pStrike != nullptr);
    CPoint target;
    REQUIRE(DryGround(pArea, CPoint(pSprite->GetPos().x + 160, pSprite->GetPos().y + 40), target));
    iwd2r_srand(65);
    pStrike->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, target, 0, 0);
    CHECK(Draws(65, iwd2r_rand_state()) == 0);
    std::vector<CGameObject*> objects = AddedObjects(pArea);
    REQUIRE(objects.size() == 2);
    CProjectileShaft* pShaft = dynamic_cast<CProjectileShaft*>(objects[0]);
    REQUIRE(pShaft != nullptr);
    CHECK(objects[1] == pStrike);
    CHECK(pShaft->m_nDuration == 28);
    CHECK(pShaft->m_bTransparent == FALSE);
    CHECK(pShaft->m_bRandomFrame == FALSE);
    CHECK(pShaft->m_sourceId == pSprite->GetId());
    CHECK(pShaft->GetPos() == CPoint(target.x, target.y + 13));
    CHECK(pStrike->m_bExploded == TRUE);
    CHECK(pStrike->GetPos() == target);
    // The engine sorts the area's lists before its update pump; the area
    // projectile's GetTargets walks the sorted list from its own place in
    // it (m_posVertList).
    pArea->SortLists();

    CHECK(UpdateUntilGone(pShaft->GetId(), 100) == 28);
    CHECK(UpdateUntilGone(pStrike->GetId(), 100) == 45);
    pArea->SortLists();
    ClearMessages();
}

// Dispel Magic (type 246, CProjectileArea850C0C::CreateSecondary
// 0x577C70): at a sprite, it first removes its Globes of Invulnerability
// (effect 450) at once, then fires the secondary projectile as every area
// projectile does (CProjectileArea::CreateSecondary 0x5701E0).
TEST_CASE("visual projectiles: Dispel Magic removes a Globe of Invulnerability before its secondary", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(pSprite->GetPos().x + 400, pSprite->GetPos().y + 30), 1);
    REQUIRE(goblins.size() == 1);
    CGameSprite* pGoblin = goblins.front();
    ClearMessages();

    std::unique_ptr<CGameEffect> pGlobe = DecodeAt(ICEWIND_CGAMEEFFECT_GLOBEOFINVULNERABILITY, 0, 0, pGoblin->GetPos(), pGoblin->GetId(), pGoblin->GetPos());
    REQUIRE(pGlobe != nullptr);
    pGoblin->m_timedEffectList.AddTail(pGlobe.release());
    auto globes = [&]() {
        int count = 0;
        POSITION pos = pGoblin->m_timedEffectList.GetHeadPosition();
        while (pos != NULL) {
            count += pGoblin->m_timedEffectList.GetNext(pos)->m_effectID == ICEWIND_CGAMEEFFECT_GLOBEOFINVULNERABILITY;
        }
        return count;
    };
    REQUIRE(globes() == 1);

    CProjectile* pProjectile = CProjectile::DecodeProjectile(246, pSprite, 0);
    CProjectileArea850C0C* pDispel = dynamic_cast<CProjectileArea850C0C*>(pProjectile);
    REQUIRE(pDispel != nullptr);
    pDispel->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, pGoblin->GetPos(), 0, 0);
    REQUIRE(pDispel->GetId() != CGameObjectArray::INVALID_INDEX);
    pArea->SortLists();
    ClearMessages();

    iwd2r_srand(67);
    pDispel->CreateSecondary(pGoblin->GetId());
    // One draw: AddEffect runs the effect through CheckAdd (0x4A3310),
    // which draws a number and discards it (0x4A38A6).
    CHECK(Draws(67, iwd2r_rand_state()) == 1);
    CHECK(globes() == 0);
    CHECK(AddedSecondaries(pArea) == std::vector<LONG> { pGoblin->GetId() });
    pArea->SortLists();

    // Not a sprite: neither.
    pDispel->CreateSecondary(pDispel->GetId());
    CHECK(AddedSecondaries(pArea).empty());

    CHECK(UpdateUntilGone(pDispel->GetId(), 100) > 0);
    pArea->SortLists();
    ClearMessages();
}

// Acid Fog (type 212, CProjectileArea84F3AC::CreateSecondary 0x5716B0):
// its secondary projectile at any sprite, without the checks of
// CProjectileArea::CreateSecondary (0x5701E0): a dead sprite or one in the
// excluded targets is hit too; not a sprite is not.
TEST_CASE("visual projectiles: Acid Fog fires its secondary at any sprite, dead or excluded", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    std::vector<CGameSprite*> goblins = AddGoblins(pArea, CPoint(pSprite->GetPos().x + 400, pSprite->GetPos().y + 30), 1);
    REQUIRE(goblins.size() == 1);
    CGameSprite* pGoblin = goblins.front();
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(212, pSprite, 0);
    CProjectileArea84F3AC* pFog = dynamic_cast<CProjectileArea84F3AC*>(pProjectile);
    REQUIRE(pFog != nullptr);
    pFog->Fire(pArea, pSprite->GetId(), CGameObjectArray::INVALID_INDEX, pGoblin->GetPos(), 0, 0);
    REQUIRE(pFog->GetId() != CGameObjectArray::INVALID_INDEX);
    pArea->SortLists();
    ClearMessages();

    const DWORD generalState = pGoblin->GetDerivedStats()->m_generalState;
    pGoblin->GetDerivedStats()->m_generalState |= STATE_DEAD;
    pFog->m_excludedTargets.insert(pGoblin->GetId());
    pGoblin->m_posOld = CPoint(0, 0);
    iwd2r_srand(69);
    pFog->CreateSecondary(pGoblin->GetId());
    CHECK(Draws(69, iwd2r_rand_state()) == 0);
    CHECK(AddedSecondaries(pArea) == std::vector<LONG> { pGoblin->GetId() });
    CHECK(pGoblin->m_posOld == pGoblin->GetPos());
    pGoblin->GetDerivedStats()->m_generalState = generalState;
    pArea->SortLists();

    pFog->CreateSecondary(pFog->GetId());
    CHECK(AddedSecondaries(pArea).empty());
    pArea->SortLists();
    ClearMessages();
}

// The spell-school travel projectiles (DecodeProjectile 0x51EAF0, ctor
// 0x57E370 CProjectile8511AC): one per school of magic (types 227-234,
// AbjurT through DivinT) for a spell whose projectile is its school's
// travel BAM. The ctor flags the visual effect, doubles the flight speed
// with a native WORD shift, clears the strike fields, forbids a callback
// projectile, turns height on and stamps the TRA_02 fire sound over a dead
// "" store with an empty arrival sound. Decoding draws nothing.
TEST_CASE("visual projectiles: the school travels decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* sSchool;
    };
    const Spec kSchools[] = {
        { 227, "AbjurT" },
        { 228, "AlterT" },
        { 229, "InvocT" },
        { 230, "NecroT" },
        { 231, "ConjuT" },
        { 232, "EnchaT" },
        { 233, "IllusT" },
        { 234, "DivinT" },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kSchools) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile8511AC* pTravel = dynamic_cast<CProjectile8511AC*>(pProjectile);
        REQUIRE(pTravel != nullptr);
        CHECK(pTravel->m_projectileType == spec.nType - 1);
        CHECK(pTravel->m_animation->GetResRef() == CResRef(spec.sSchool));
        CHECK(pTravel->m_visualEffect.m_dwFlags == 0x20200u);
        CHECK(pTravel->field_1BE == FALSE);
        CHECK(pTravel->field_1C2 == FALSE);
        CHECK(pTravel->field_1D4 == 0);
        CHECK(pTravel->m_callBackProjectile == CGameObjectArray::INVALID_INDEX);
        CHECK(pTravel->m_sVisualEffect == "");
        CHECK(pTravel->field_182 == CGameObjectArray::INVALID_INDEX);
        CHECK(pTravel->m_bHasHeight == TRUE);
        CHECK(pTravel->m_fireSoundRef == CResRef("TRA_02"));
        CHECK(pTravel->m_arrivalSoundRef == CResRef(""));
        // The speed is the cell's frame width doubled by a native WORD
        // shift; pin it against a directly built sibling, not a literal.
        CProjectile8511AC direct(spec.sSchool);
        CHECK(pTravel->m_speed == direct.m_speed);
        delete pProjectile;
    }
}

TEST_CASE("visual projectiles: the line beams decode to their rakes", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        const char* sCell;
        const char* sFire;
        LONG nBeam;
        SHORT nSpeed;
        BOOL bHeight;
        BOOL bOffset;
        int nGround; // field_2A0 when the case stores it, -1 when it doesn't
    };
    const Spec kBolts[] = {
        { 40, "LightnT", "TRA_09", 400, 20, TRUE, TRUE, -1 },
        { 302, "SWaveX", "TRA_56", 400, 10, FALSE, FALSE, 0 },
        { 313, "LoDisrT", "TRA_59", 600, 20, TRUE, TRUE, -1 },
        { 378, "HDABreT", "TRA_59", 600, 20, TRUE, TRUE, -1 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kBolts) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileLightningBolt* pBolt = dynamic_cast<CProjectileLightningBolt*>(pProjectile);
        REQUIRE(pBolt != nullptr);
        CHECK(pBolt->m_projectileType == spec.nType - 1);
        CHECK(pBolt->m_animation->GetResRef() == CResRef(spec.sCell));
        CHECK(pBolt->m_speed == spec.nSpeed);
        CHECK(pBolt->field_1D8 == 16);
        CHECK(pBolt->m_bHasHeight == spec.bHeight);
        CHECK(pBolt->field_1C2 == spec.bOffset);
        if (spec.nGround >= 0) {
            CHECK(pBolt->field_2A0 == spec.nGround);
        }
        CHECK(pBolt->m_visualEffect.m_dwFlags == 0x20200u);
        CHECK(pBolt->m_beamRange == spec.nBeam);
        CHECK(pBolt->m_fireSoundRef == CResRef(spec.sFire));
        // The ctor arms the strike tracker; the case bodies only retune its
        // range (a DWORD store over the map's WORD plus tail padding).
        CHECK(pBolt->m_targetMap.m_pOwner == pBolt);
        CHECK(pBolt->m_targetMap.m_servicePeriod == 2);
        CHECK(pBolt->m_targetMap.m_strikeInterval == 33);
        CHECK(pBolt->m_targetMap.m_nRange == 50);
        delete pProjectile;
    }
}

TEST_CASE("visual projectiles: the skull trap and glyph decode to their bursts", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        SHORT nEngineType; // the ctor's third argument, stamped to +0x4B2
        const char* sCell;
        const char* sShadow;
        SHORT nStrikeRange;
        const char* sFire;
        const char* sArrival;
        const char* sExplode;
        DWORD nMirror; // field_1C6, the fork's m_mirror
    };
    const Spec kTraps[] = {
        { 96, 0x400, "sklT", "ShSkull", 0x64, "", "ARE_M06", "EFF_M35", 0 },
        { 100, 0x410, "glphwrdT", "ShGlyph", 0x50, "EFF_P22a", "", "EFF_P22b", 1 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kTraps) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectileSkullTrap* pSkull = dynamic_cast<CProjectileSkullTrap*>(pProjectile);
        REQUIRE(pSkull != nullptr);
        CHECK(pSkull->m_projectileType == spec.nType - 1);
        CHECK(pSkull->m_nType == spec.nEngineType);
        CHECK(pSkull->m_bHasHeight == TRUE);
        CHECK(pSkull->m_strikeRange == spec.nStrikeRange);
        CHECK(pSkull->m_preCheckRange == 0x32);
        CHECK(pSkull->m_animation->GetResRef() == CResRef(spec.sCell));
        CHECK(pSkull->m_shadow->GetResRef() == CResRef(spec.sShadow));
        CHECK(pSkull->m_fireSoundRef == CResRef(spec.sFire));
        CHECK(pSkull->m_arrivalSoundRef == CResRef(spec.sArrival));
        CHECK(pSkull->m_explodeSound == spec.sExplode);
        if (spec.nMirror != 0) {
            CHECK(pSkull->m_loopFireSound == TRUE);
        } else {
            CHECK(pSkull->m_loopArrivalSound == TRUE);
        }
        CHECK(pSkull->field_1BE == FALSE);
        CHECK(pSkull->field_1C2 == FALSE);
        CHECK(pSkull->field_1C6 == spec.nMirror);
        CHECK(pSkull->field_1D4 == TRUE);
        CHECK(pSkull->field_1D8 == 1);
        CHECK(pSkull->m_childProjectileType == 0x4E);
        CHECK(pSkull->m_strikesLeft == 1);
        CHECK(pSkull->m_lingerPeriod == 100);
        CHECK(pSkull->m_bPreScan == 1);
        // Both explosion cells are armed: cell 1 carries the travel BAM,
        // cell 2 the burst BAM.
        CHECK(pSkull->m_explodeCell1.GetResRef() == CResRef(spec.sCell));
        CHECK(pSkull->m_bExplodeCell1Active == 1);
        CHECK(pSkull->m_explodeCell2.GetResRef() == CResRef(spec.sShadow));
        CHECK(pSkull->m_bExplodeCell2Active == 1);
        // The speed is the cell's frame width doubled by a native WORD
        // shift; pin it against a directly built sibling, not a literal.
        CProjectileSkullTrap direct(CResRef(spec.sCell), CResRef(spec.sShadow), spec.nEngineType);
        CHECK(pSkull->m_speed == direct.m_speed);
        delete pProjectile;
    }
}

// The plain flame jets (DecodeProjectile 0x51EAF0, ctor 0x52FC20
// CProjectile84DE0C): the SPFIREBL carrier with palette range 0 recoloured
// 0x43 off the master bitmap and handed to the cell, mirror bounds 15/11
// with mirroring on, the leaf render param 0x80, field_1D4 clear, 16
// directions and the speed tripled, then the no-callback / no-visual-effect
// stamps. Type 80 takes the ctor's palette as cast; type 105 rebinds the
// cell's palette to the projectile's own and widens range 0 one entry to
// 0x44 (SetPalette before SetRange, the reverse of the ctor's order). Both
// case bodies end with the height flag, the TRA_06 launch sound and the
// empty arrival tail. Decoding draws nothing.
TEST_CASE("visual projectiles: the flame jets decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        BYTE nRange0;
    };
    const Spec kJets[] = {
        { 80, 0x43 },
        { 105, 0x44 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kJets) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile84DE0C* pJet = dynamic_cast<CProjectile84DE0C*>(pProjectile);
        REQUIRE(pJet != nullptr);
        CHECK(pJet->m_projectileType == spec.nType - 1);
        CHECK(pJet->m_animation->GetResRef() == CResRef("SPFIREBL"));
        CHECK(pJet->m_bHasHeight == TRUE);
        CHECK(pJet->m_fireSoundRef == "TRA_06");
        CHECK(pJet->m_arrivalSoundRef == "");
        CHECK(pJet->m_palette.m_rangeColors[0] == spec.nRange0);
        CHECK(pJet->field_1CA == 0xF);
        CHECK(pJet->field_1CE == 0xB);
        CHECK(pJet->field_1BE == FALSE);
        CHECK(pJet->field_1C2 == FALSE);
        CHECK(pJet->field_1C6 == TRUE);
        CHECK(pJet->field_1D2 == 0x80);
        CHECK(pJet->field_1D4 == FALSE);
        CHECK(pJet->field_1D8 == 0x10);
        CHECK(pJet->m_callBackProjectile == CGameObjectArray::INVALID_INDEX);
        CHECK(pJet->field_182 == CGameObjectArray::INVALID_INDEX);
        CHECK(pJet->m_sVisualEffect == "");
        // The tripled speed, pinned against the plain type-79 sibling (the
        // same SPFIREBL base with no speed store) and a directly built one.
        CProjectile84DF1C plain;
        CProjectile84DE0C direct("SPFIREBL");
        CHECK(direct.m_speed == plain.m_speed * 3);
        CHECK(pJet->m_speed == direct.m_speed);
        delete pProjectile;
    }
}

TEST_CASE("visual projectiles: the searing-light shots decode to their flights", "[visualprojectile][data]")
{
    struct Spec {
        WORD nType;
        LONG nBounces;
    };
    const Spec kShots[] = {
        { 206, 1 },
        { 380, 6 },
    };
    WORLD_OR_SKIP(world);
    for (const Spec& spec : kShots) {
        INFO("type " << spec.nType);
        iwd2r_srand(71);
        CProjectile* pProjectile = CProjectile::DecodeProjectile(spec.nType, world.pCharacter, 0);
        CHECK(Draws(71, iwd2r_rand_state()) == 0);
        CProjectile84DE94* pLight = dynamic_cast<CProjectile84DE94*>(pProjectile);
        REQUIRE(pLight != nullptr);
        CHECK(pLight->m_projectileType == spec.nType - 1);
        // The ctor binds "SPLightB" (0x8AD0AC, mixed case — the exe's own
        // spelling), but this install's CHITIN.KEY indexes no SPLightB in
        // ANY casing (whole-file brute force), and the exe's FindKey
        // compares resrefs as two raw dwords (0x78B494-0x78B4A4 —
        // case-sensitive), so CResHelper::SetResRef stores "" in the
        // original engine on this data too. Assert the parity outcome; the
        // intended resref is covered by the ctor-body port and evidence.
        CHECK(pLight->m_animation->GetResRef() == "");
        CHECK(pLight->m_bHasHeight == TRUE);
        CHECK(pLight->m_fireSoundRef == "TRA_09");
        CHECK(pLight->m_arrivalSoundRef == "");
        CHECK(pLight->m_nBounces == spec.nBounces);
        delete pProjectile;
    }
    // The ctor's own stamp: a directly built shot starts unbounced (the
    // exe seeds +0x2A0 with 0xFFFFFFFF; the decoder re-stamps 1 or 6), and
    // the same keyfile fact above leaves its cell unbound here.
    CProjectile84DE94 direct;
    CHECK(direct.m_nBounces == CGameObjectArray::INVALID_INDEX);
    CHECK(direct.m_animation->GetResRef() == "");
}

TEST_CASE("visual projectiles: the cow shot decodes to its flight", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    iwd2r_srand(71);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(189, world.pCharacter, 0);
    CHECK(Draws(71, iwd2r_rand_state()) == 0);
    CProjectile84DA4C* pCow = dynamic_cast<CProjectile84DA4C*>(pProjectile);
    REQUIRE(pCow != nullptr);
    CHECK(pCow->m_projectileType == 188);
    // Unlike SPLightB, ACOWG1 IS in this install's CHITIN.KEY (uppercase,
    // offset 117972), so the cell binds for real.
    CHECK(pCow->m_animation->GetResRef() == CResRef("ACOWG1"));
    CHECK(pCow->m_bHasHeight == TRUE);
    CHECK(pCow->m_fireSoundRef == "AMB_E42A");
    CHECK(pCow->m_arrivalSoundRef == "AMB_E42A");
    // The decoder's WORD stamp at +0x1D8 (interleaved with the fire-sound
    // pushes in the exe's store order).
    CHECK(pCow->field_1D8 == 1);
    // The decoder doubles the base ctor's speed (shl ax,1) — assert against
    // a directly built shot rather than pinning the base's default.
    CProjectile84DA4C cowDirect(CResRef("ACOWG1"));
    CHECK(pCow->m_speed == static_cast<SHORT>(cowDirect.m_speed * 2));
    delete pProjectile;
}

TEST_CASE("visual projectiles: the type-352 shot decodes to the bare flight", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(352, world.pCharacter, 0);
    CProjectile84DAD4* pBare = dynamic_cast<CProjectile84DAD4*>(pProjectile);
    REQUIRE(pBare != nullptr);
    // The case body (0x527F57) carries no stores of its own: everything
    // below is the base constructor's own clearing plus the shared tail's
    // type stamp (352 - 1).
    CHECK(pBare->m_projectileType == 351);
    CHECK(pBare->m_speed == 0);
    CHECK(pBare->m_bHasHeight == FALSE);
    CHECK(pBare->m_fireSoundRef == "");
    CHECK(pBare->m_arrivalSoundRef == "");
    delete pProjectile;
}

TEST_CASE("visual projectiles: the chain-lightning hop decodes to its carrier", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    iwd2r_srand(71);
    CProjectile* pProjectile = CProjectile::DecodeProjectile(210, world.pCharacter, 0);
    CHECK(Draws(71, iwd2r_rand_state()) == 0);
    IcewindCProjectileChainLightning* pBolt =
        dynamic_cast<IcewindCProjectileChainLightning*>(pProjectile);
    REQUIRE(pBolt != nullptr);
    // The case body (0x5250B0) only news the carrier with an empty
    // struck-target set; everything else is the ctor 0x57C510's own tuning
    // plus the shared tail's type stamp (210 - 1 = 0xD1).
    CHECK(pBolt->m_projectileType == 209);
    CHECK(pBolt->m_animation->GetResRef() == CResRef("CLightT"));
    CHECK(pBolt->m_fireSoundRef == CResRef("TRA_09"));
    CHECK(pBolt->m_visualEffect.m_dwFlags == 0x20200u);
    CHECK(pBolt->m_bHasHeight == FALSE);
    CHECK(pBolt->field_1C2 == FALSE);
    CHECK(pBolt->field_1D8 == 16);
    REQUIRE(pBolt->m_struckTargets.empty());
    // The ctor doubles the speed with a native WORD shift; the decode adds
    // no retune of its own, so pin the field against a directly built
    // carrier rather than a literal.
    std::set<LONG> emptyTargets;
    IcewindCProjectileChainLightning boltDirect(emptyTargets);
    CHECK(pBolt->m_speed == boltDirect.m_speed);
    delete pProjectile;
}
