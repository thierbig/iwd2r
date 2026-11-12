#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CCreatureFile.h"
#include "CDimmKeyTable.h"
#include "CGameAnimationTypeCharacter.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CGameSpriteEquipment.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CPathSearch.h"
#include "CRes.h"
#include "CSearchBitmap.h"
#include "IcewindCGameEffects.h"
#include "engine_world.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

// CGameEffectDisguise's destructor (0x4C3AA0). The original's vtable
// 0x84B520 has its scalar deleting destructor 0x49D960 in slot 0, which
// calls the body here, and CGameEffect::OnRemove (0x4A51D0) in slot 0x24:
// the class overrides OnRemove nowhere, so the destructor is the only place
// the sprite is restored, and OnRemove followed by delete restores it once.
//
// The sprite is a CHARBASE.CRE sprite put in AR1000 at a grid square, with
// its animation set to a character animation (personal space 3) and a long
// sword equipped through CItem::Equip, the way CItem::Equip installs a
// disguise: the effect created with field_188 = 1 (the flag only that path
// sets). The disguise animation 0x7100 is a monster (personal space 5), so
// the search map's footprint tells the two animations apart.

namespace {

// The original's OnRemove for this class is the base's, not an override:
// an override would restore in OnRemove and again in the destructor.
static_assert(std::is_same<decltype(&CGameEffectDisguise::OnRemove),
                  void (CGameEffect::*)(CGameSprite*)>::value,
    "CGameEffectDisguise must inherit CGameEffect::OnRemove (vtable 0x84B520 slot 0x24)");

constexpr USHORT ORIGINAL_ANIMATION = 0x6000; // character, personal space 3
constexpr USHORT DISGUISE_ANIMATION = 0x7100; // monster, personal space 5

using namespace engine_world;

CGameSprite* ProbeOrSkip()
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    static FramePump pump;
    struct Once {
        CGameSprite* pSprite = NULL;
        bool emptyChr = false;
        Once()
        {
            std::vector<BYTE> bytes = FinishedCharacter();
            if (bytes.empty()) {
                emptyChr = true;
                return;
            }
            pSprite = NewGameInStartArea(bytes);
        }
    };
    static Once once;
    if (once.emptyChr) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(once.pSprite != NULL);
    REQUIRE(once.pSprite->GetArea() != NULL);
    return once.pSprite;
}

// A CHARBASE.CRE sprite, in no area (the constructor still puts it in the
// game's object array).
CGameSprite* NewCharBaseSprite()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    REQUIRE(file.GetData() != NULL);
    CGameSprite* pSprite = new CGameSprite(file.GetData(),
        file.GetDataSize(),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    file.ReleaseData();
    return pSprite;
}

// The sprite's animation as the restore must set it: a character animation
// (the one whose equipment visuals prove EquipAll ran after the restore).
void SetCharacterAnimation(CGameSprite* pSprite)
{
    pSprite->GetAnimation()->SetAnimationType(ORIGINAL_ANIMATION,
        pSprite->GetBaseStats()->m_colors,
        pSprite->GetDirection());
    REQUIRE(pSprite->GetAnimation()->GetAnimationId() == ORIGINAL_ANIMATION);
    REQUIRE(pSprite->GetAnimation()->GetPersonalSpace() == 3);
}

// A long sword in SLOT_WEAPON, the selected weapon, equipped the way
// EquipAll equips it. Returns the animation's weapon resref.
CString EquipSword(CGameSprite* pSprite)
{
    CItem* pSword = new CItem(CResRef("00SWDL01"), 0, 0, 0, 0, 0);
    REQUIRE(pSword->pRes != NULL);
    pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_WEAPON] = pSword;
    pSprite->m_equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_WEAPON;
    pSprite->EquipAll(TRUE);

    CGameAnimationTypeCharacter* pAnimation = dynamic_cast<CGameAnimationTypeCharacter*>(pSprite->GetAnimation()->m_animation);
    REQUIRE(pAnimation != NULL);
    REQUIRE(!pAnimation->m_weaponResRef.IsEmpty());
    return pAnimation->m_weaponResRef;
}

CGameEffect* MakeDisguise(LONG sourceId, LONG amount)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_DISGUISE);
    effect.effectAmount = amount;
    effect.durationType = 0;
    effect.probabilityUpper = 100;
    return CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), sourceId, CPoint(-1, -1));
}

// The area's dynamic search cost around a grid square. The disguise and the
// restore add and remove exactly the same footprints, so an exact comparison
// pins the order: the animation's personal space used by RemoveObject (the
// disguising one), then the restored one by AddObject.
std::vector<BYTE> Snapshot(CGameArea* pArea, const CPoint& pt, int radius)
{
    std::vector<BYTE> cells;
    for (int y = pt.y - radius; y <= pt.y + radius; y++) {
        for (int x = pt.x - radius; x <= pt.x + radius; x++) {
            cells.push_back(pArea->m_search.m_pDynamicCost[y * pArea->m_search.m_GridSquareDimensions.cx + x]);
        }
    }
    return cells;
}

CPoint ToPixels(const CPoint& pt)
{
    return CPoint(pt.x * CPathSearch::GRID_SQUARE_SIZEX, pt.y * CPathSearch::GRID_SQUARE_SIZEY);
}

CPoint ToGrid(const CPoint& pt)
{
    return CPoint(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY);
}

// A CHARBASE.CRE sprite with the character animation at a free square of
// the area. The square is clear of the disguise animation's wider footprint
// (personal space 5), so ApplyEffect's JumpToPoint, which searches with that
// footprint, keeps the sprite where it is.
CGameSprite* AddCharacterAtFreeCell(CGameArea* pArea, const CPoint& ptGrid)
{
    CGameSprite* pSprite = NewCharBaseSprite();
    SetCharacterAnimation(pSprite);
    pSprite->AddToArea(pArea, ToPixels(ptGrid), 0, CGameObject::LIST_FRONT);

    REQUIRE(pSprite->GetArea() == pArea);
    REQUIRE(ToGrid(pSprite->GetPos()) == ptGrid);
    REQUIRE(pSprite->field_7430 == TRUE);
    return pSprite;
}

// Undo AddToArea and the object array entry. A sprite built from a file has
// m_bDeleteOnRemove set, so RemoveFromArea deletes the array entry and the
// sprite; one that was never in an area needs the array entry removed here.
void DestroySprite(CGameSprite* pSprite)
{
    if (pSprite->GetArea() != NULL) {
        pSprite->RemoveFromArea();
    } else {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(pSprite->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            NULL,
            INFINITE);
        delete pSprite;
    }
}

struct InDestroyGame {
    InDestroyGame()
        : pGame(g_pBaldurChitin->GetObjectGame())
        , saved(pGame->m_bInDestroyGame)
    {
    }

    ~InDestroyGame()
    {
        pGame->m_bInDestroyGame = saved;
    }

    CInfGame* pGame;
    BOOLEAN saved;
};

} // namespace

TEST_CASE("Disguise destructor (0x4C3AA0) restores an item's disguise", "[effects][disguise][data]")
{
    CGameSprite* pProbe = ProbeOrSkip();
    CGameArea* pArea = pProbe->GetArea();
    REQUIRE(pArea != NULL);

    // The square every section works on: the search map's nearest free one
    // six squares to the probe's right.
    CPoint ptNear = ToGrid(pProbe->GetPos());
    ptNear.x += 6;
    REQUIRE(ptNear.x >= 5);
    REQUIRE(ptNear.x < pArea->m_search.m_GridSquareDimensions.cx - 5);
    REQUIRE(ptNear.y >= 5);
    REQUIRE(ptNear.y < pArea->m_search.m_GridSquareDimensions.cy - 5);
    const CPoint ptGrid = pArea->m_search.GetNearestOpenSquare(ptNear, pProbe->m_terrainTable, 5, -1);

    SECTION("delete restores the animation, search map and equipment once")
    {
        CGameSprite* pSprite = AddCharacterAtFreeCell(pArea, ptGrid);
        REQUIRE(ToGrid(pSprite->GetPos()) == ptGrid);

        // Equipment: EquipAll ran against the original animation.
        CString weaponResRef = EquipSword(pSprite);

        const std::vector<BYTE> before = Snapshot(pArea, ptGrid, 4);

        CGameEffect* pEffect = MakeDisguise(pSprite->GetId(), DISGUISE_ANIMATION);
        REQUIRE(pEffect != NULL);
        CGameEffectDisguise* pDisguise = dynamic_cast<CGameEffectDisguise*>(pEffect);
        REQUIRE(pDisguise != NULL);
        pDisguise->field_188 = 1;

        // The sound member the base destructor tears down.
        pEffect->m_sound.SetResRef(CResRef("EFF_E05"), FALSE, TRUE);
        REQUIRE(pEffect->m_sound.GetRes() != NULL);
        CDimmKeyTableEntry* pSoundKey = pEffect->m_sound.GetRes()->m_pDimmKeyTableEntry;
        REQUIRE(pSoundKey != NULL);
        const short nSoundRefs = pSoundKey->field_10;

        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        CHECK(pSprite->GetAnimation()->GetPersonalSpace() == 5);
        // The animation the effect keeps for the removal.
        CHECK(pDisguise->field_18C == ORIGINAL_ANIMATION);
        CHECK(pSprite->field_7430 == TRUE);
        CHECK(ToGrid(pSprite->GetPos()) == ptGrid);
        const std::vector<BYTE> disguised = Snapshot(pArea, ptGrid, 4);
        CHECK(disguised != before);

        // OnRemove is the base's no-op: it does not restore (the static_assert
        // above says the class does not override it), so the delete below is
        // the one restore.
        pEffect->OnRemove(pSprite);
        CHECK(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        CHECK(pSprite->field_7430 == TRUE);
        CHECK(Snapshot(pArea, ptGrid, 4) == disguised);

        // Delete through the base pointer, as the effect lists do: the
        // virtual destructor's slot 0 calls the restoring body.
        CGameEffect* pBase = pEffect;
        delete pBase;

        // The animation is the one the effect saved when it was applied.
        CHECK(pSprite->GetAnimation()->GetAnimationId() == ORIGINAL_ANIMATION);
        CHECK(pSprite->GetAnimation()->GetPersonalSpace() == 3);

        // The search map is back to its state before the disguise: the
        // disguised footprint was removed before the animation changed and
        // the restored one added after it.
        CHECK(Snapshot(pArea, ptGrid, 4) == before);
        CHECK(pSprite->field_7430 == TRUE);

        // EquipAll ran against the restored animation, not the disguising
        // one: the new animation type carries the sword's resref.
        CGameAnimationTypeCharacter* pRestored = dynamic_cast<CGameAnimationTypeCharacter*>(pSprite->GetAnimation()->m_animation);
        REQUIRE(pRestored != NULL);
        CHECK(pRestored->m_weaponResRef == weaponResRef);

        // The CSound member was released by the base destructor.
        CHECK(pSoundKey->field_10 == nSoundRefs - 1);

        DestroySprite(pSprite);
    }

    SECTION("a cast disguise (field_188 == 0) is not restored on delete")
    {
        CGameSprite* pSprite = AddCharacterAtFreeCell(pArea, ptGrid);
        REQUIRE(ToGrid(pSprite->GetPos()) == ptGrid);

        CGameEffect* pEffect = MakeDisguise(pSprite->GetId(), DISGUISE_ANIMATION);
        REQUIRE(pEffect != NULL);
        // field_188 is 0 for an effect a spell applied; DecodeEffect leaves it
        // at CGameEffect's constructor's value.
        dynamic_cast<CGameEffectDisguise*>(pEffect)->field_188 = 0;

        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        const std::vector<BYTE> disguised = Snapshot(pArea, ptGrid, 4);

        delete pEffect;

        // The destructor's guard skips everything: no animation restore, no
        // search update, no equipment pass.
        CHECK(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        CHECK(pSprite->GetAnimation()->GetPersonalSpace() == 5);
        CHECK(Snapshot(pArea, ptGrid, 4) == disguised);
        CHECK(pSprite->field_7430 == TRUE);

        DestroySprite(pSprite);
    }

    SECTION("destroying the game skips the restore")
    {
        CGameSprite* pSprite = AddCharacterAtFreeCell(pArea, ptGrid);
        REQUIRE(ToGrid(pSprite->GetPos()) == ptGrid);

        CGameEffect* pEffect = MakeDisguise(pSprite->GetId(), DISGUISE_ANIMATION);
        REQUIRE(pEffect != NULL);
        dynamic_cast<CGameEffectDisguise*>(pEffect)->field_188 = 1;

        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        const std::vector<BYTE> disguised = Snapshot(pArea, ptGrid, 4);

        {
            InDestroyGame destroying;
            destroying.pGame->m_bInDestroyGame = TRUE;

            delete pEffect;
        }

        CHECK(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        CHECK(Snapshot(pArea, ptGrid, 4) == disguised);
        CHECK(pSprite->field_7430 == TRUE);

        DestroySprite(pSprite);
    }

    SECTION("a source that is not in the object array skips the restore")
    {
        CGameSprite* pSprite = AddCharacterAtFreeCell(pArea, ptGrid);
        REQUIRE(ToGrid(pSprite->GetPos()) == ptGrid);

        CGameEffect* pEffect = MakeDisguise(pSprite->GetId(), DISGUISE_ANIMATION);
        REQUIRE(pEffect != NULL);
        dynamic_cast<CGameEffectDisguise*>(pEffect)->field_188 = 1;

        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        const std::vector<BYTE> disguised = Snapshot(pArea, ptGrid, 4);

        // GetShare's BAD_INDEX arm: the destructor returns without restoring
        // and without a ReleaseShare.
        pEffect->m_sourceID = CGameObjectArray::INVALID_INDEX;
        delete pEffect;

        CHECK(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        CHECK(Snapshot(pArea, ptGrid, 4) == disguised);
        CHECK(pSprite->field_7430 == TRUE);

        DestroySprite(pSprite);
    }

    SECTION("a dead sprite keeps its animation but is re-placed")
    {
        CGameSprite* pSprite = AddCharacterAtFreeCell(pArea, ptGrid);
        REQUIRE(ToGrid(pSprite->GetPos()) == ptGrid);

        CGameEffect* pEffect = MakeDisguise(pSprite->GetId(), DISGUISE_ANIMATION);
        REQUIRE(pEffect != NULL);
        dynamic_cast<CGameEffectDisguise*>(pEffect)->field_188 = 1;

        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        const std::vector<BYTE> disguised = Snapshot(pArea, ptGrid, 4);

        // STATE_DEAD in the base general state (sprite+0x5BC) skips the
        // animation restore and its AddObject; the remove, JumpToPoint,
        // SetIdleSequence and EquipAll still run.
        pSprite->m_baseStats.m_generalState |= STATE_DEAD;
        delete pEffect;

        CHECK(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);
        CHECK(pSprite->GetAnimation()->GetPersonalSpace() == 5);
        // JumpToPoint put the footprint back with the animation the sprite
        // still has.
        CHECK(pSprite->field_7430 == TRUE);
        CHECK(Snapshot(pArea, ptGrid, 4) == disguised);

        DestroySprite(pSprite);
    }

    SECTION("a sprite in no area still restores its animation")
    {
        CGameSprite* pSprite = NewCharBaseSprite();
        REQUIRE(pSprite->GetArea() == NULL);
        SetCharacterAnimation(pSprite);

        CGameEffect* pEffect = MakeDisguise(pSprite->GetId(), DISGUISE_ANIMATION);
        REQUIRE(pEffect != NULL);
        dynamic_cast<CGameEffectDisguise*>(pEffect)->field_188 = 1;

        REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
        REQUIRE(pSprite->GetAnimation()->GetAnimationId() == DISGUISE_ANIMATION);

        delete pEffect;

        // The two search blocks are the area's; the animation still comes
        // back and the sprite is still re-placed (JumpToPoint's no-area arm).
        CHECK(pSprite->GetAnimation()->GetAnimationId() == ORIGINAL_ANIMATION);
        CHECK(pSprite->GetAnimation()->GetPersonalSpace() == 3);

        DestroySprite(pSprite);
    }
}
