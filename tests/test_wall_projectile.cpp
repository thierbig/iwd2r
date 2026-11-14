// The two wall behaviours of issue #119's G3: the wall target map's gather
// (0x55B1B0, vtable 0x84EEB0 slot 1) and the shared wall arrival
// (0x5820B0, slot 0x70 of both 0x8514CC and 0x851664).
//
// The arrival is driven on a bare CChitin with a synthetic key table, so no
// install is needed: the sound helper finds an ARE_P22 entry (wave type) and
// queues its request without reading a file, which exercises the helper's
// full SetResRef request path. The gather is driven twice: on the same bare
// fixture against an area with an empty vertical list (the corner, polygon,
// bounding-rect and GetAllInPoly call path, with no candidates), and, when
// game data is present, against the real AR1000 of a new game, where the
// party member's position pins the polygon inside/outside results.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CDimmKeyTable.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CProjectile.h"
#include "CProjectileSingleTarget.h"
#include "CRes.h"
#include "CVidMode.h"
#include "CWarp.h"
#include "IcewindCProjectileTargetMap.h"
#include "engine_world.h"

#include <cstdint>
#include <vector>

using namespace engine_world;

namespace {

constexpr USHORT kResTypeWave = 4;
constexpr int kKeyEntries = 128;

// CChitin declares the thread entry points pure and the fixture never starts
// one (the test_infinity.cpp precedent).
class WallChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }
};

// g_pChitin is a real CChitin (its ctor makes the critical sections and the
// CDimm request lists) with a synthetic key table: the entries are all empty
// except ARE_P22 at its hash slot, so CResHelper::SetResRef("ARE_P22") takes
// the GetResObject -> Request path and finds a resource. No file is read:
// Request only queues the CResWave. Every wall is destroyed before this
// fixture's destructor (it releases anything still queued while g_pChitin
// still points here).
class WallFixture {
public:
    WallFixture()
    {
        m_pPreviousChitin = g_pChitin;
        m_pPreviousMixer = CSound::m_pSoundMixer;
        m_previousWarpChitin = CWarp::pChitin;
        m_previousWarpDimm = CWarp::pDimm;
        m_previousWarpVideo = CWarp::pVideo;
        m_previousWarpMixer = CWarp::pSoundMixer;
        g_pChitin = &m_chitin;
        // CVidPalette::SetType(TYPE_RANGE), run by the projectile ctor, asks
        // CChitin::GetCurrentVideoMode() -- the active engine's pVidMode --
        // for its transparent colour.
        m_pEngine = new CWarp();
        m_pEngine->pVidMode = &m_vidMode;
        m_chitin.pActiveEngine = m_pEngine;

        m_entries.resize(kKeyEntries);
        for (CDimmKeyTableEntry& entry : m_entries) {
            entry.resRef = "";
            entry.pRes = NULL;
            entry.nID = -1;
            entry.field_10 = 0;
            entry.nResType = 0xFFFF;
            entry.bUpdated = FALSE;
        }

        CDimmKeyTable& table = m_chitin.cDimm.m_cKeyTable;
        table.m_nEntries = kKeyEntries;
        table.m_pEntries = m_entries.data();

        unsigned int slot = table.Hash(CResRef("ARE_P22"), kResTypeWave);
        CDimmKeyTableEntry& entry = m_entries[slot];
        entry.resRef = CResRef("ARE_P22");
        entry.nResType = kResTypeWave;
        entry.nID = 0;
    }

    ~WallFixture()
    {
        CDimmKeyTable& table = m_chitin.cDimm.m_cKeyTable;
        for (CDimmKeyTableEntry& entry : m_entries) {
            if (entry.pRes != NULL) {
                m_chitin.cDimm.ReleaseResObject(entry.pRes);
            }
        }
        table.m_pEntries = NULL;
        table.m_nEntries = 0;
        m_chitin.pActiveEngine = NULL;
        delete m_pEngine;
        m_pEngine = NULL;
        g_pChitin = m_pPreviousChitin;
        CSound::m_pSoundMixer = m_pPreviousMixer;
        CWarp::pChitin = m_previousWarpChitin;
        CWarp::pDimm = m_previousWarpDimm;
        CWarp::pVideo = m_previousWarpVideo;
        CWarp::pSoundMixer = m_previousWarpMixer;
    }

    WallFixture(const WallFixture&) = delete;
    WallFixture& operator=(const WallFixture&) = delete;

private:
    CChitin* m_pPreviousChitin = NULL;
    CSoundMixer* m_pPreviousMixer = NULL;
    CChitin* m_previousWarpChitin = NULL;
    CDimm* m_previousWarpDimm = NULL;
    CVideo* m_previousWarpVideo = NULL;
    CSoundMixer* m_previousWarpMixer = NULL;
    WallChitin m_chitin;
    CVidMode m_vidMode;
    CWarp* m_pEngine = NULL;
    std::vector<CDimmKeyTableEntry> m_entries;
};

// The gather only needs the area's front vertical list empty for the
// no-candidate case, so a zeroed block of the real size stands in for an
// area that the fixture does not load (the RawBlock precedent in
// test_infinity.cpp). No other member is read while the list is empty.
class EmptyAreaFixture {
public:
    EmptyAreaFixture()
        : m_storage(sizeof(CGameArea), 0)
    {
    }

    CGameArea* Get() { return reinterpret_cast<CGameArea*>(m_storage.data()); }

private:
    std::vector<unsigned char> m_storage;
};

bool Contains(const std::map<LONG, CPoint>& targets, LONG id)
{
    return targets.find(id) != targets.end();
}

} // namespace

// The first arrival starts the ARE_P22 loop once: the resref goes through the
// sound helper's request path (true, true), then fire-and-forget, channel 15
// of the area, play at the wall's position -- and the +0x2B2 byte and
// field_1DE latch. A second arrival only re-latches both and leaves the sound
// untouched.
TEST_CASE("wall arrival (0x5820B0): the moonlight wall starts its loop once", "[wallprojectile]")
{
    WallFixture fixture;

    CProjectileWallOfMoonlight wall;
    wall.m_pos = CPoint(120, 240);
    wall.m_pArea = reinterpret_cast<CGameArea*>(0x1234); // never dereferenced by the fixture's uninitialised mixer
    REQUIRE(wall.field_2B2 == 0);
    REQUIRE(wall.field_1DE == FALSE);
    REQUIRE(wall.m_loopSound.GetResRef() == "");

    wall.OnArrival();

    CHECK(wall.field_2B2 == 1);
    CHECK(wall.field_1DE == TRUE);
    // SetResRef found the synthetic ARE_P22 entry and queued its request.
    CHECK(wall.m_loopSound.GetResRef() == "ARE_P22");
    CHECK(wall.m_loopSound.pRes != NULL);
    CHECK(wall.m_loopSound.bLoading == TRUE);
    CRes* pRes = wall.m_loopSound.pRes;

    // The second arrival is the early exit: flags re-latched, no resource
    // work (the same CResWave, the same resref).
    wall.OnArrival();
    CHECK(wall.field_2B2 == 1);
    CHECK(wall.field_1DE == TRUE);
    CHECK(wall.m_loopSound.GetResRef() == "ARE_P22");
    CHECK(wall.m_loopSound.pRes == pRes);
    CHECK(wall.m_loopSound.bLoading == TRUE);
}

// The early exit re-latches the byte at +0x2B2 to exactly 1 (the ctor leaves
// it 0; the guard treats any non-zero as arrived) and raises field_1DE
// without touching the sound.
TEST_CASE("wall arrival (0x5820B0): an already-arrived wall only re-latches", "[wallprojectile]")
{
    WallFixture fixture;

    CProjectileWallOfMoonlight wall;
    wall.field_2B2 = 7;
    wall.field_1DE = FALSE;
    wall.m_loopSound.cResRef = CResRef("KEEP");
    wall.OnArrival();

    CHECK(wall.field_2B2 == 1);
    CHECK(wall.field_1DE == TRUE);
    CHECK(wall.m_loopSound.GetResRef() == "KEEP");
    CHECK(wall.m_loopSound.pRes == NULL);
}

// The Wall of Fire's vtable holds the same 0x5820B0 body at slot 0x70.
TEST_CASE("wall arrival (0x5820B0): the fire wall shares the body", "[wallprojectile]")
{
    WallFixture fixture;

    CProjectileWallOfFire wall;
    wall.m_pos = CPoint(11, 22);
    wall.m_pArea = reinterpret_cast<CGameArea*>(0x5678);
    REQUIRE(wall.field_2B2 == 0);
    REQUIRE(wall.field_1DE == FALSE);

    wall.OnArrival();

    CHECK(wall.field_2B2 == 1);
    CHECK(wall.field_1DE == TRUE);
    CHECK(wall.m_loopSound.GetResRef() == "ARE_P22");
    CHECK(wall.m_loopSound.pRes != NULL);
    CHECK(wall.m_loopSound.bLoading == TRUE);

    wall.field_2B2 = 9;
    wall.field_1DE = FALSE;
    wall.OnArrival();
    CHECK(wall.field_2B2 == 1);
    CHECK(wall.field_1DE == TRUE);
}

// The gather's geometry reads: each of the four corner offsets is added to
// the owner's position, the bounding rect is the min/max over the four
// points, and GetAllInPoly is called with the polygon, 4, ANYONE, the area's
// visible terrain table and checkBackList FALSE. With an empty area list this
// walks every step and returns the empty map. (The polygon's inside/outside
// effect is pinned by the data case below; the rect values are not
// separately observable through GetAllInPoly.)
TEST_CASE("wall gather (0x55B1B0): corners, bounding rect and the polygon scan", "[wallprojectile]")
{
    WallFixture fixture;
    EmptyAreaFixture area;

    CProjectile owner;
    owner.m_pos = CPoint(300, 400);
    // The gather reads CGameObject::GetArea() (+0x12 in the exe), not the
    // CProjectile shadow at +0xEA that the arrival sound uses.
    owner.CGameObject::m_pArea = area.Get();

    IcewindCProjectileWallTargetMap map;
    map.m_pOwner = &owner;
    map.m_corners[0] = CPoint(-40, -20);
    map.m_corners[1] = CPoint(60, -20);
    map.m_corners[2] = CPoint(60, 30);
    map.m_corners[3] = CPoint(-40, 30);
    // Never armed by the exe: whatever the ctor state has would be read as a
    // wall length/range, and this gather reads neither.
    map.field_36 = 20;
    map.m_nRange = 70;

    std::map<LONG, CPoint> targets = map.GatherTargets();
    CHECK(targets.empty());
}

// The live polygon path on a real area: the party member in AR1000 is the
// candidate, and the corners decide whether its point is inside the quad.
// Rotating the corner set moves the quad away from the member, which must
// drop it from the result; a diamond around it must include it. This exercises
// GetAllInPoly's real object-array and IsPtInPoly path, not a reimplementation
// of it.
TEST_CASE("wall gather (0x55B1B0): the real polygon includes and excludes a sprite", "[wallprojectile][data]")
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
    REQUIRE(pCharacter != NULL);
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->m_visibility.SetAreaExplored();
    pArea->m_visibility.SetAreaVisible(TRUE);

    CProjectile owner;
    owner.m_pos = pCharacter->GetPos();
    // The gather reads CGameObject::GetArea() (+0x12 in the exe), not the
    // CProjectile shadow at +0xEA that the arrival sound uses.
    owner.CGameObject::m_pArea = pArea;

    IcewindCProjectileWallTargetMap map;
    map.m_pOwner = &owner;

    // A quad around the member includes it.
    map.m_corners[0] = CPoint(-50, -50);
    map.m_corners[1] = CPoint(50, -50);
    map.m_corners[2] = CPoint(50, 50);
    map.m_corners[3] = CPoint(-50, 50);
    CHECK(Contains(map.GatherTargets(), pCharacter->GetId()));

    // Non-axis-aligned corners: the min/max rect is wider than the quad, so
    // the polygon test (not the rect) has to keep the member in.
    map.m_corners[0] = CPoint(-100, 10);
    map.m_corners[1] = CPoint(-10, -100);
    map.m_corners[2] = CPoint(100, -10);
    map.m_corners[3] = CPoint(10, 100);
    CHECK(Contains(map.GatherTargets(), pCharacter->GetId()));

    // A quad away from the member excludes it.
    map.m_corners[0] = CPoint(100, 100);
    map.m_corners[1] = CPoint(200, 100);
    map.m_corners[2] = CPoint(200, 200);
    map.m_corners[3] = CPoint(100, 200);
    CHECK_FALSE(Contains(map.GatherTargets(), pCharacter->GetId()));
}
