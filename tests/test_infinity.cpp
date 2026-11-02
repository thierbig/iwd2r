// The area layer's WED/CInfinity fixtures on a synthetic WED,
// CTiledObject::CheckTileState (0x54E5B0) and
// CInfinity::ScrollingRequestRect (0x5D00C0).
//
// A real WED is built from the packed structs in src/BalDataTypes.h and parsed
// with CResWED::Parse, so the accessors read it exactly as the game's do. The
// CheckTileState CInfinity stands in with one CInfTileSet whose VRAM pool is a
// plain TileDefinition array: its swap path only reads the pool's entries and
// the tiles' m_nVRamTile/m_renderCode, so the fixture needs no surfaces. The
// scrolling fixture (T86) maps every grid cell to its own CResInfTile and runs
// on a real CChitin, because RequestTile reaches the tiles through
// CRes::SetPriority/Request and g_pChitin->cDimm.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CInfinity.h"
#include "CResWED.h"
#include "CTiledObject.h"

#include <cstdint>
#include <cstring>
#include <set>
#include <utility>
#include <vector>

namespace {

constexpr DWORD kWEDSize = 0x400;
constexpr DWORD kLayerHeaderOffset = 0x40;
constexpr DWORD kPolyHeaderOffset = 0x60;
constexpr DWORD kTiledObjectsOffset = 0x80;
constexpr DWORD kObjectTileListOffset = 0xC0;
constexpr DWORD kTileDataOffset = 0x100;
constexpr DWORD kTileListOffset = 0x380;
constexpr DWORD kPolyListOffset = 0x390;
constexpr DWORD kPolyPointsOffset = 0x3D8;
constexpr int kTilesAcross = 8;
constexpr int kTilesDown = 8;
constexpr int kTileSetSize = 8;
constexpr int kPolyCount = 4;

// A zeroed block of a class whose constructor/destructor the fixture does not
// run (the game's objects are plain data here; the bits they read are set by
// hand).
template <class T>
struct RawBlock {
    T* get() { return reinterpret_cast<T*>(storage.data()); }

    std::vector<unsigned char> storage = std::vector<unsigned char>(sizeof(T), 0);
};

void Put16(std::vector<BYTE>& image, DWORD at, WORD value)
{
    std::memcpy(image.data() + at, &value, sizeof(value));
}

void Put32(std::vector<BYTE>& image, DWORD at, DWORD value)
{
    std::memcpy(image.data() + at, &value, sizeof(value));
}

// A synthetic WED with one layer, one tiled object and a four-poly poly list,
// plus the CInfinity/tile set/pool the tiled-object functions use. The tile
// data grid is 8x8; each entry's starting tile is i % kTileSetSize and its
// secondary tile (i + 1) % kTileSetSize, so every swap is distinguishable.
struct WedFixture {
    WedFixture()
        : image(kWEDSize, 0)
    {
        std::memcpy(image.data() + 0, "WED ", 4);
        std::memcpy(image.data() + 4, "V1.3", 4);

        Put32(image, 0x08, 1); // nLayers
        Put32(image, 0x0C, 1); // nTiledObjects
        Put32(image, 0x10, kLayerHeaderOffset);
        Put32(image, 0x14, kPolyHeaderOffset);
        Put32(image, 0x18, kTiledObjectsOffset);
        Put32(image, 0x1C, kObjectTileListOffset);

        Put16(image, kLayerHeaderOffset + 0x00, kTilesAcross);
        Put16(image, kLayerHeaderOffset + 0x02, kTilesDown);
        Put16(image, kLayerHeaderOffset + 0x0C, kTileSetSize);
        Put32(image, kLayerHeaderOffset + 0x10, kTileDataOffset);
        Put32(image, kLayerHeaderOffset + 0x14, kTileListOffset);

        Put32(image, kPolyHeaderOffset + 0x00, kPolyCount);
        Put32(image, kPolyHeaderOffset + 0x04, kPolyListOffset);
        Put32(image, kPolyHeaderOffset + 0x08, kPolyPointsOffset);

        for (int i = 0; i < kTilesAcross * kTilesDown; i++) {
            auto* pTileData = reinterpret_cast<WED_TILEDATA*>(image.data() + kTileDataOffset + i * sizeof(WED_TILEDATA));
            // Disjoint primary/secondary pairs (0/1, 2/3, 4/5, 6/7), so one
            // entry's swap cannot change another entry's source render code.
            pTileData->nStartingTile = static_cast<WORD>(2 * (i % 4));
            pTileData->nNumTiles = 1;
            pTileData->nSecondary = static_cast<WORD>(2 * (i % 4) + 1);
            pTileData->bFlags = 0;
            pTileData->wFlags = 0;
        }

        for (int i = 0; i < kTileSetSize; i++) {
            Put16(image, kTileListOffset + 2 * i, static_cast<WORD>(i));
        }

        for (int i = 0; i < kPolyCount; i++) {
            auto* pPoly = reinterpret_cast<WED_POLYLIST*>(image.data() + kPolyListOffset + i * sizeof(WED_POLYLIST));
            pPoly->nType = 0x1;
        }

        SetObject(0, 3, 2, 2);

        m_pResWed->Parse(image.data());
        REQUIRE(m_pResWed->m_bParsed);

        m_pInfinity->pResWED = m_pResWed;

        m_pPool->m_nVTiles = 1;
        m_pPool->m_pTileDefs = m_defs;
        m_defs[0].nTile = 0;
        m_defs[0].nRefCount = 2; // above zero, so a detach never empties it
        m_defs[0].pTileSet = m_pTileSet;

        for (int i = 0; i < kTileSetSize; i++) {
            auto* pTile = m_pResTiles[i];
            pTile->m_nVRamTile = 0;
            pTile->m_renderCode.tileNW = static_cast<BYTE>(0x10 + i);
            pTile->m_renderCode.tileNE = static_cast<BYTE>(0x20 + i);
            pTile->m_renderCode.tileSW = static_cast<BYTE>(0x30 + i);
            pTile->m_renderCode.tileSE = static_cast<BYTE>(0x40 + i);
            pTile->m_pDualTileRes = NULL;
            m_tilePointers[i] = pTile;
        }

        m_pTileSet->m_pVRamPool = m_pPool;
        m_pTileSet->m_pResTiles = m_tilePointers;
        m_pTileSet->m_nTiles = kTileSetSize;

        m_pInfinity->pTileSets[0] = m_pTileSet;

        m_pObject->m_pResWed = m_pResWed;
        m_pObject->m_nWedIndex = 0;
    }

    // The tiled object: which of the object tile list's entries it covers, how
    // many polys of each kind and where their type bytes are (absolute WED
    // offsets, as the game stores them).
    void SetObject(WORD nStartingTile, WORD nNumTiles, WORD nNumPrimaryPolys, WORD nNumSecondaryPolys)
    {
        auto* pObject = reinterpret_cast<WED_TILEDOBJECT*>(image.data() + kTiledObjectsOffset);
        pObject->nStartingTile = nStartingTile;
        pObject->nNumTiles = nNumTiles;
        pObject->nNumPrimaryPolys = nNumPrimaryPolys;
        pObject->nNumSecondaryPolys = nNumSecondaryPolys;
        pObject->nOffsetToPrimaryPolys = kPolyListOffset;
        pObject->nOffsetToSecondaryPolys = kPolyListOffset + 2 * sizeof(WED_POLYLIST);

        for (WORD i = 0; i < nStartingTile + nNumTiles; i++) {
            Put16(image, kObjectTileListOffset + 2 * i, static_cast<WORD>(i));
        }
    }

    BYTE PolyType(int index) const
    {
        return (image.data() + kPolyListOffset + index * sizeof(WED_POLYLIST))[8];
    }

    WED_TILEDATA* TileData(int index)
    {
        return reinterpret_cast<WED_TILEDATA*>(image.data() + kTileDataOffset + index * sizeof(WED_TILEDATA));
    }

    std::vector<BYTE> image;

    RawBlock<CResWED> resWed;
    RawBlock<CInfinity> infinity;
    RawBlock<CInfTileSet> tileSet;
    RawBlock<CVRamPool> pool;
    RawBlock<CResInfTile> resTiles[kTileSetSize];
    RawBlock<CTiledObject> object;

    CResWED* m_pResWed = resWed.get();
    CInfinity* m_pInfinity = infinity.get();
    CInfTileSet* m_pTileSet = tileSet.get();
    CVRamPool* m_pPool = pool.get();
    CResInfTile* m_pResTiles[kTileSetSize] = {
        resTiles[0].get(),
        resTiles[1].get(),
        resTiles[2].get(),
        resTiles[3].get(),
        resTiles[4].get(),
        resTiles[5].get(),
        resTiles[6].get(),
        resTiles[7].get(),
    };
    CTiledObject* m_pObject = object.get();
    CResInfTile** m_tilePointers = m_pResTiles;
    TileDefinition m_defs[1] = {};
};

}

// CTiledObject::CheckTileState (0x54E5B0): when the AI state differs from the
// rendered state the object's tiles take the new state's wFlags and swap their
// VRAM images (primary <-> secondary), and the tiled object's primary polys
// clear the type bit while its secondary polys set it (the reverse for the
// secondary state).
TEST_CASE("infinity: CheckTileState swaps the tile images and the poly types", "[infinity]")
{
    SECTION("primary state")
    {
        WedFixture fixture;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;

        const BYTE primaryBefore = fixture.PolyType(0);
        const BYTE secondaryBefore = fixture.PolyType(2);

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.m_pObject->m_wRenderState == CTiledObject::STATE_PRIMARY_TILE);
        for (int i = 0; i < 3; i++) {
            const WED_TILEDATA* pTileData = fixture.TileData(i);
            CHECK(pTileData->wFlags == CTiledObject::STATE_PRIMARY_TILE);

            // The swap detached nSecondary and attached tileList[nStartingTile],
            // so the primary tile carries the secondary's code.
            CHECK(fixture.m_pResTiles[pTileData->nStartingTile]->m_renderCode.tileNW
                == static_cast<BYTE>(0x10 + pTileData->nSecondary));
        }

        CHECK((fixture.PolyType(0) & 0x2) == 0); // primary poly: cleared
        CHECK((fixture.PolyType(1) & 0x2) == 0);
        CHECK((fixture.PolyType(2) & 0x2) != 0); // secondary poly: set
        CHECK((fixture.PolyType(3) & 0x2) != 0);
        CHECK((primaryBefore & 0x2) == 0);
        CHECK((secondaryBefore & 0x2) == 0);
    }

    SECTION("secondary state")
    {
        WedFixture fixture;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_SECONDARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_PRIMARY_TILE;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.m_pObject->m_wRenderState == CTiledObject::STATE_SECONDARY_TILE);
        for (int i = 0; i < 3; i++) {
            const WED_TILEDATA* pTileData = fixture.TileData(i);
            CHECK(pTileData->wFlags == CTiledObject::STATE_SECONDARY_TILE);

            // The reverse swap: the secondary image gets the primary's code.
            CHECK(fixture.m_pResTiles[pTileData->nSecondary]->m_renderCode.tileNW
                == static_cast<BYTE>(0x10 + pTileData->nStartingTile));
        }

        CHECK((fixture.PolyType(0) & 0x2) != 0); // primary poly: set
        CHECK((fixture.PolyType(1) & 0x2) != 0);
        CHECK((fixture.PolyType(2) & 0x2) == 0); // secondary poly: cleared
        CHECK((fixture.PolyType(3) & 0x2) == 0);
    }
}

TEST_CASE("infinity: CheckTileState's early outs and edge counts", "[infinity]")
{
    SECTION("same state draws nothing")
    {
        WedFixture fixture;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_PRIMARY_TILE;
        const std::vector<BYTE> before = fixture.image;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.image == before);
        CHECK(fixture.m_pResTiles[1]->m_renderCode.tileNW == 0x11);
    }

    SECTION("no WED index draws nothing")
    {
        WedFixture fixture;
        fixture.m_pObject->m_nWedIndex = -1;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;
        const std::vector<BYTE> before = fixture.image;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.image == before);
        CHECK(fixture.m_pObject->m_wRenderState == CTiledObject::STATE_SECONDARY_TILE);
    }

    SECTION("zero and -1 poly counts")
    {
        WedFixture fixture;
        fixture.SetObject(0, 3, 0, 0xFFFF);
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK((fixture.PolyType(0) & 0x2) == 0); // untouched (count 0)
        CHECK((fixture.PolyType(2) & 0x2) == 0); // untouched (0xFFFF)
    }

    SECTION("no tiles only toggles the polys")
    {
        WedFixture fixture;
        fixture.SetObject(0, 0, 2, 2);
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.TileData(0)->wFlags == 0);
        CHECK((fixture.PolyType(2) & 0x2) != 0);
    }
}

// -----------------------------------------------------------------------------
// CInfinity::ScrollingRequestRect (0x5D00C0)
// -----------------------------------------------------------------------------

namespace {

constexpr DWORD kScrollWEDSize = 0x500;
constexpr DWORD kScrollTileDataOffset = 0x100;
constexpr DWORD kScrollTileListOffset = 0x380;
constexpr int kScrollTilesAcross = 8;
constexpr int kScrollTilesDown = 8;
constexpr int kScrollTiles = kScrollTilesAcross * kScrollTilesDown;

// CChitin declares the thread entry points pure, and the fixture never starts
// one: the scrolling functions only need its cDimm and critical sections.
class ScrollChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }
};

// A synthetic 8x8 WED whose cell (x, y) starts tile y * 8 + x and has no
// secondary tile, so RequestTile's request count on a CResInfTile says exactly
// which cells the walk visited and its dwFlags the priority of the last visit.
// The CInfinity's tile set holds the 64 tiles; RequestTile reaches them through
// CRes::SetPriority/Request, so g_pChitin is a real CChitin (its constructor
// makes the critical sections and the empty CDimm request lists).
struct ScrollFixture {
    ScrollFixture()
        : image(kScrollWEDSize, 0)
    {
        m_previousChitin = g_pChitin;
        g_pChitin = &m_chitin;

        std::memcpy(image.data(), "WED ", 4);
        std::memcpy(image.data() + 4, "V1.3", 4);

        Put32(image, 0x08, 1); // nLayers
        Put32(image, 0x0C, 0); // nTiledObjects
        Put32(image, 0x10, kLayerHeaderOffset);
        Put32(image, 0x14, kPolyHeaderOffset);
        Put32(image, 0x18, kTiledObjectsOffset);
        Put32(image, 0x1C, kObjectTileListOffset);

        Put16(image, kLayerHeaderOffset + 0x00, kScrollTilesAcross);
        Put16(image, kLayerHeaderOffset + 0x02, kScrollTilesDown);
        Put16(image, kLayerHeaderOffset + 0x0C, kScrollTiles);
        Put32(image, kLayerHeaderOffset + 0x10, kScrollTileDataOffset);
        Put32(image, kLayerHeaderOffset + 0x14, kScrollTileListOffset);

        Put32(image, kPolyHeaderOffset + 0x00, 0);
        Put32(image, kPolyHeaderOffset + 0x04, 0);
        Put32(image, kPolyHeaderOffset + 0x08, 0);

        for (int i = 0; i < kScrollTiles; i++) {
            auto* pTileData = reinterpret_cast<WED_TILEDATA*>(image.data() + kScrollTileDataOffset + i * sizeof(WED_TILEDATA));
            // The cell's own tile is the secondary one, so RequestTile takes
            // its secondary path (it requests nSecondary in the first block and
            // again in the last one, hence two requests per visit).
            pTileData->nStartingTile = 0;
            pTileData->nNumTiles = 1;
            pTileData->nSecondary = static_cast<WORD>(i);
            // A second layer the fixture does not have (the WED has one
            // layer), so the layer walk is a no-op in both and the trailing
            // secondary request runs.
            pTileData->bFlags = 0x2;
            pTileData->wFlags = CTiledObject::STATE_SECONDARY_TILE;

            Put16(image, kScrollTileListOffset + 2 * i, static_cast<WORD>(i));
        }

        m_pResWed->Parse(image.data());
        REQUIRE(m_pResWed->m_bParsed);

        for (int i = 0; i < kScrollTiles; i++) {
            m_pResTiles[i]->m_pDualTileRes = NULL;
            m_tilePointers[i] = m_pResTiles[i];
        }

        m_pTileSet->m_pVRamPool = NULL; // the request path never touches it
        m_pTileSet->m_pResTiles = m_tilePointers;
        m_pTileSet->m_nTiles = kScrollTiles;

        m_pInfinity->pResWED = m_pResWed;
        m_pInfinity->pTileSets[0] = m_pTileSet;
        m_pInfinity->nTilesX = kScrollTilesAcross;
        m_pInfinity->nTilesY = kScrollTilesDown;
        m_pInfinity->field_68 = -1;
        m_pInfinity->field_6C = -1;
        m_pInfinity->field_70 = -1;
        m_pInfinity->field_74 = -1;
    }

    ~ScrollFixture() { g_pChitin = m_previousChitin; }

    ScrollFixture(const ScrollFixture&) = delete;
    ScrollFixture& operator=(const ScrollFixture&) = delete;

    CResInfTile* TileAt(int x, int y) { return m_pResTiles[y * kScrollTilesAcross + x]; }

    ScrollChitin m_chitin;

    std::vector<BYTE> image;

    RawBlock<CResWED> resWed;
    RawBlock<CInfinity> infinity;
    RawBlock<CInfTileSet> tileSet;
    RawBlock<CResInfTile> resTiles[kScrollTiles];
    CResInfTile* m_tilePointers[kScrollTiles] = {};
    CChitin* m_previousChitin = nullptr;

    CResWED* m_pResWed = resWed.get();
    CInfinity* m_pInfinity = infinity.get();
    CInfTileSet* m_pTileSet = tileSet.get();
    CResInfTile* m_pResTiles[kScrollTiles] = {
        resTiles[0].get(),
        resTiles[1].get(),
        resTiles[2].get(),
        resTiles[3].get(),
        resTiles[4].get(),
        resTiles[5].get(),
        resTiles[6].get(),
        resTiles[7].get(),
        resTiles[8].get(),
        resTiles[9].get(),
        resTiles[10].get(),
        resTiles[11].get(),
        resTiles[12].get(),
        resTiles[13].get(),
        resTiles[14].get(),
        resTiles[15].get(),
        resTiles[16].get(),
        resTiles[17].get(),
        resTiles[18].get(),
        resTiles[19].get(),
        resTiles[20].get(),
        resTiles[21].get(),
        resTiles[22].get(),
        resTiles[23].get(),
        resTiles[24].get(),
        resTiles[25].get(),
        resTiles[26].get(),
        resTiles[27].get(),
        resTiles[28].get(),
        resTiles[29].get(),
        resTiles[30].get(),
        resTiles[31].get(),
        resTiles[32].get(),
        resTiles[33].get(),
        resTiles[34].get(),
        resTiles[35].get(),
        resTiles[36].get(),
        resTiles[37].get(),
        resTiles[38].get(),
        resTiles[39].get(),
        resTiles[40].get(),
        resTiles[41].get(),
        resTiles[42].get(),
        resTiles[43].get(),
        resTiles[44].get(),
        resTiles[45].get(),
        resTiles[46].get(),
        resTiles[47].get(),
        resTiles[48].get(),
        resTiles[49].get(),
        resTiles[50].get(),
        resTiles[51].get(),
        resTiles[52].get(),
        resTiles[53].get(),
        resTiles[54].get(),
        resTiles[55].get(),
        resTiles[56].get(),
        resTiles[57].get(),
        resTiles[58].get(),
        resTiles[59].get(),
        resTiles[60].get(),
        resTiles[61].get(),
        resTiles[62].get(),
        resTiles[63].get(),
    };
};

// Every cell at most once: the interior cells at the high priority, the border
// cells at the medium one, and every other cell untouched. The unit test uses
// an interior rect (2,2)-(6,6) and pins each state's border by hand.
void CheckRequests(ScrollFixture& fixture,
    const std::set<std::pair<int, int>>& high,
    const std::set<std::pair<int, int>>& medium)
{
    for (int y = 0; y < kScrollTilesDown; y++) {
        for (int x = 0; x < kScrollTilesAcross; x++) {
            const CResInfTile* pTile = fixture.TileAt(x, y);
            const bool bHigh = high.count(std::make_pair(x, y)) != 0;
            const bool bMedium = medium.count(std::make_pair(x, y)) != 0;

            CHECK(pTile->field_44 == (bHigh || bMedium ? 2 : 0));
            if (bHigh) {
                CHECK((pTile->dwFlags & 3) == CDimm::PRIORITY_HIGH);
            } else if (bMedium) {
                CHECK((pTile->dwFlags & 3) == CDimm::PRIORITY_MEDIUM);
            } else {
                CHECK((pTile->dwFlags & 3) == 0);
            }
        }
    }
}

const std::set<std::pair<int, int>> kInterior = {
    { 3, 3 }, { 4, 3 }, { 3, 4 }, { 4, 4 }
};

}

TEST_CASE("infinity: ScrollingRequestRect requests the interior and one scroll border", "[infinity]")
{
    using Border = std::set<std::pair<int, int>>;

    ScrollFixture fixture;

    SECTION("state 1: one row above")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 1);
        CheckRequests(fixture, kInterior, Border { { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 } });
    }

    SECTION("state 2: one row above and the right column")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 2);
        CheckRequests(fixture, kInterior, Border { { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 } });
    }

    SECTION("state 3: the right column")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 3);
        CheckRequests(fixture, kInterior, Border { { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 } });
    }

    SECTION("state 4: the right column and one row below")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 4);
        CheckRequests(fixture, kInterior, Border { { 2, 6 }, { 3, 6 }, { 4, 6 }, { 5, 6 }, { 6, 6 }, { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 } });
    }

    SECTION("state 5: one row below")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 5);
        CheckRequests(fixture, kInterior, Border { { 2, 6 }, { 3, 6 }, { 4, 6 }, { 5, 6 } });
    }

    SECTION("state 6: the left column and one row below")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 6);
        CheckRequests(fixture, kInterior, Border { { 1, 6 }, { 2, 6 }, { 3, 6 }, { 4, 6 }, { 5, 6 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 } });
    }

    SECTION("state 7: the left column")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 7);
        CheckRequests(fixture, kInterior, Border { { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 } });
    }

    SECTION("state 8: one row above and the left column")
    {
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 8);
        CheckRequests(fixture, kInterior, Border { { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 } });
    }
}

TEST_CASE("infinity: ScrollingRequestRect clamps to the map and records the rect", "[infinity]")
{
    using Border = std::set<std::pair<int, int>>;

    SECTION("the recorded rect and the scroll state")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 7);

        CHECK(fixture.m_pInfinity->field_68 == 2);
        CHECK(fixture.m_pInfinity->field_6C == 2);
        CHECK(fixture.m_pInfinity->field_70 == 6);
        CHECK(fixture.m_pInfinity->field_74 == 6);
        CHECK(fixture.m_pInfinity->field_196 == 7);
    }

    SECTION("a rect wider than the map has no border")
    {
        // The interior is one tile in from the edges; the border rows/columns
        // clamp to the map's last index, so they are empty.
        Border expected;
        for (int y = 1; y <= 6; y++) {
            for (int x = 1; x <= 6; x++) {
                expected.insert(std::make_pair(x, y));
            }
        }

        ScrollFixture fixture;
        fixture.m_pInfinity->ScrollingRequestRect(0, 0, 8, 8, 3);
        CheckRequests(fixture, expected, {});
    }

    SECTION("a rect wholly above and left requests nothing")
    {
        ScrollFixture fixture;
        const BOOL bResult = fixture.m_pInfinity->ScrollingRequestRect(-3, -3, -1, -1, 8);
        CHECK(bResult);

        for (int i = 0; i < kScrollTiles; i++) {
            CHECK(fixture.m_pResTiles[i]->field_44 == 0);
        }
    }

    SECTION("the flags byte is 1 without a day/night area")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->m_areaType = 0;
        fixture.m_pInfinity->m_requestDayNightCode = 0x2;
        fixture.m_pInfinity->nCurrentRainLevel = 1;
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 7);
        CHECK(fixture.m_pInfinity->field_15E == 1);
    }

    SECTION("the flags byte takes the day/night code")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->m_areaType = 0x40;
        fixture.m_pInfinity->m_requestDayNightCode = 0x2;
        fixture.m_pInfinity->nCurrentRainLevel = 0;
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 7);
        CHECK(fixture.m_pInfinity->field_15E == 0x2);
    }

    SECTION("rain sets bit 2 when the level is nonzero")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->m_areaType = 0x40 | 0x4;
        fixture.m_pInfinity->m_requestDayNightCode = 0x2;
        fixture.m_pInfinity->nCurrentRainLevel = 3;
        fixture.m_pInfinity->ScrollingRequestRect(2, 2, 6, 6, 7);
        CHECK(fixture.m_pInfinity->field_15E == 0x6);
    }
}

// -----------------------------------------------------------------------------
// CInfinity::ScrollingCancelRequestRect (0x5CD3C0)
// -----------------------------------------------------------------------------

namespace {

// The cancel walk reads the rect from field_68/6C/70/74 and the scroll state
// from field_196, and cancels exactly the cells the request walk of T86 visits;
// a visited cell is cancelled twice (the secondary block and the trailing one).
void SetCounts(ScrollFixture& fixture, int value)
{
    for (int i = 0; i < kScrollTiles; i++) {
        fixture.m_pResTiles[i]->field_44 = value;
    }
}

void CheckCancels(ScrollFixture& fixture, int nBefore,
    const std::set<std::pair<int, int>>& cancelled)
{
    for (int y = 0; y < kScrollTilesDown; y++) {
        for (int x = 0; x < kScrollTilesAcross; x++) {
            const CResInfTile* pTile = fixture.TileAt(x, y);
            const bool bCancelled = cancelled.count(std::make_pair(x, y)) != 0;
            CHECK(pTile->field_44 == (bCancelled ? nBefore - 2 : nBefore));
        }
    }
}

}

TEST_CASE("infinity: ScrollingCancelRequestRect cancels the interior and the remembered border", "[infinity]")
{
    using Border = std::set<std::pair<int, int>>;

    ScrollFixture fixture;
    SetCounts(fixture, 5);

    SECTION("state 1: one row above")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 1;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 2: one row above and the right column")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 2;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 3: the right column")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 3;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 4: the right column and one row below")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 4;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 2, 6 }, { 3, 6 }, { 4, 6 }, { 5, 6 }, { 6, 6 }, { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 5: one row below")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 5;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 2, 6 }, { 3, 6 }, { 4, 6 }, { 5, 6 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 6: the left column and one row below")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 6;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 1, 6 }, { 2, 6 }, { 3, 6 }, { 4, 6 }, { 5, 6 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 7: the left column")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 7;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 } });
        CheckCancels(fixture, 5, cancelled);
    }

    SECTION("state 8: one row above and the left column")
    {
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 8;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        Border cancelled = kInterior;
        cancelled.insert({ { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 } });
        CheckCancels(fixture, 5, cancelled);
    }
}

TEST_CASE("infinity: ScrollingCancelRequestRect's clamps, count floor and a1", "[infinity]")
{
    using Border = std::set<std::pair<int, int>>;

    SECTION("the remembered rect is used as is and left alone")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 7;

        const BOOL bResult = fixture.m_pInfinity->ScrollingCancelRequestRect(1);
        CHECK(bResult);

        CHECK(fixture.m_pInfinity->field_68 == 2);
        CHECK(fixture.m_pInfinity->field_6C == 2);
        CHECK(fixture.m_pInfinity->field_70 == 6);
        CHECK(fixture.m_pInfinity->field_74 == 6);
        CHECK(fixture.m_pInfinity->field_196 == 7);
    }

    SECTION("a rect wider than the map has no border")
    {
        Border discarded;
        for (int y = 1; y <= 6; y++) {
            for (int x = 1; x <= 6; x++) {
                discarded.insert(std::make_pair(x, y));
            }
        }

        ScrollFixture fixture;
        SetCounts(fixture, 5);
        fixture.m_pInfinity->field_68 = 0;
        fixture.m_pInfinity->field_6C = 0;
        fixture.m_pInfinity->field_70 = 8;
        fixture.m_pInfinity->field_74 = 8;
        fixture.m_pInfinity->field_196 = 3;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);
        CheckCancels(fixture, 5, discarded);
    }

    SECTION("a rect wholly above and left cancels nothing")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->field_68 = -3;
        fixture.m_pInfinity->field_6C = -3;
        fixture.m_pInfinity->field_70 = -1;
        fixture.m_pInfinity->field_74 = -1;
        fixture.m_pInfinity->field_196 = 8;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        for (int i = 0; i < kScrollTiles; i++) {
            CHECK(fixture.m_pResTiles[i]->field_44 == 0);
        }
    }

    SECTION("the count does not go below zero")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 3;
        fixture.m_pInfinity->ScrollingCancelRequestRect(1);

        for (int i = 0; i < kScrollTiles; i++) {
            CHECK(fixture.m_pResTiles[i]->field_44 == 0);
        }
    }

    SECTION("a1 without bit 0 cancels nothing")
    {
        ScrollFixture fixture;
        fixture.m_pInfinity->field_68 = 2;
        fixture.m_pInfinity->field_6C = 2;
        fixture.m_pInfinity->field_70 = 6;
        fixture.m_pInfinity->field_74 = 6;
        fixture.m_pInfinity->field_196 = 3;
        fixture.m_pInfinity->ScrollingCancelRequestRect(0);

        for (int i = 0; i < kScrollTiles; i++) {
            CHECK(fixture.m_pResTiles[i]->field_44 == 0);
        }
    }
}

namespace {

struct FailedAssert {
    int nLine;
};

// A failed UTIL_ASSERT throws instead of aborting (the pattern
// tests/test_vid.cpp and test_warp.cpp use).
class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }

    void ShutDown(int nLineNumber, const char* szFileName, const char* text) override
    {
        if (nLineNumber != -1) {
            throw FailedAssert { nLineNumber };
        }
        CChitin::ShutDown(nLineNumber, szFileName, text);
    }
};

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

} // namespace

// CInfTileSet::RenderToPrimary (0x5CB260) rejects an unknown dual-tile code:
// its switch's default is UTIL_ASSERT(FALSE) (the exe's 0x5CB415, Infinity.cpp
// line 571); if the assert returns the exe then calls the dual-tile fallback
// (SetRes, SetAddColor and 0x7B1F70), which the survey left unported (#54,
// report-T96.md). The assert fires before any tile is read, so a zeroed tile
// set is enough.
TEST_CASE("infinity: RenderToPrimary asserts on an unknown dual-tile code", "[infinity]")
{
    RawBlock<CInfTileSet> tileSet;
    TILE_CODE code = {};
    ChitinScope scope;

    // 0, 1, 2 and 4 are the handled codes; anything else asserts. The surface
    // is a dummy non-null pointer: the assert fires before it is used, and
    // without that default the shared Render pair returns early on the empty
    // tile set instead of throwing (which is what makes this bite).
    auto* pSurface = reinterpret_cast<LPDIRECTDRAWSURFACE>(static_cast<uintptr_t>(1));
    CHECK_THROWS_AS(tileSet.get()->RenderToPrimary(pSurface, 0, -1, code, 3, 0, 0), FailedAssert);
    CHECK_THROWS_AS(tileSet.get()->RenderToPrimary(pSurface, 0, -1, code, 5, 0, 0), FailedAssert);
    CHECK_THROWS_AS(tileSet.get()->RenderToPrimary(pSurface, 0, -1, code, 0xFF, 0, 0), FailedAssert);
}
