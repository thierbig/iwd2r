// The 32-bpp pixel functions of the video layer on plain buffers.
//
// Fixture: CVidPoly::FillConvexPoly picks its span function from
// g_pChitin->cVideo.m_nBpp (SetHLineFunction), so a bare CChitin subclass is
// installed as g_pChitin with m_nBpp 32 for each test.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CParticle.h"
#include "CResBitmap.h"
#include "CResCell.h"
#include "CResTile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidFont.h"
#include "CVidImage.h"
#include "CVidInf.h"
#include "CVidPoly.h"
#include "CVisibility.h"
#include "CWarp.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ios>
#include <memory>
#include <utility>
#include <vector>

extern CChitin* g_pChitin;

namespace {

// A failed UTIL_ASSERT, thrown by TestChitin::ShutDown.
struct FailedAssert {
    int nLine;
};

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }

    // CUtil::UtilAssert (0x780C00), the only caller with a line number,
    // calls ShutDown and then aborts; throwing here lets a test see the
    // state at a failed assert instead of losing the process.
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
        m_chitin.cVideo.m_nBpp = 32;
        g_pChitin = &m_chitin;
    }
    ~ChitinScope() { g_pChitin = m_previous; }

    ChitinScope(const ChitinScope&) = delete;
    ChitinScope& operator=(const ChitinScope&) = delete;

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

constexpr int kWidth = 24;
constexpr int kHeight = 16;
constexpr DWORD kColor = 0x00C0FFEE;

// Fills the polygon solid (flag 0x1, DrawHLine32) into a cleared 24x16
// buffer clipped to rClip and returns the buffer.
std::vector<DWORD> Fill(std::vector<CVIDPOLY_VERTEX> vertices, const CRect& rClip)
{
    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    CVidPoly poly;
    poly.SetPoly(vertices.data(), static_cast<WORD>(vertices.size()));
    // The surface pointer is the clip rectangle's top left, as
    // CVidInf::BKRenderConvexPoly locks it.
    DWORD* surface = pixels.data() + rClip.top * kWidth + rClip.left;
    REQUIRE(poly.FillConvexPoly(reinterpret_cast<WORD*>(surface), kWidth * 4, rClip, kColor, 0x1, CPoint(0, 0)));
    return pixels;
}

// The buffer with rows[y] = {first, last} filled (inclusive), others clear.
std::vector<DWORD> Expected(const std::vector<std::pair<int, std::pair<int, int>>>& rows)
{
    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    for (const auto& [y, span] : rows) {
        for (int x = span.first; x <= span.second; ++x) {
            pixels[y * kWidth + x] = kColor;
        }
    }
    return pixels;
}

const CRect kWhole(0, 0, kWidth, kHeight);

}

// A triangle whose right edge (10,0)-(13,4) steps fractionally: Dx 3, Dy 4,
// so its error term carries on three rows. The spans follow the original's
// edge walk (0x7C0F40): the error term starts at AdjUp - Dy and drops by Dy
// on a carry; both edges' Dy are measured from the top vertex.
TEST_CASE("fill_convex_poly_triangle", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 10, 0 }, { 13, 4 }, { 6, 4 } }, kWhole);
    CHECK(pixels == Expected({ { 0, { 10, 10 } }, { 1, { 9, 11 } }, { 2, { 8, 12 } }, { 3, { 7, 12 } }, { 4, { 6, 13 } } }));
}

// A taller triangle with a long fractional left edge, (12,1)-(3,13): Dx 9,
// Dy 12.
TEST_CASE("fill_convex_poly_long_edge", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 12, 1 }, { 20, 13 }, { 3, 13 } }, kWhole);
    // Left: x = 12 - (9 * dy + 12 - 3 - 1) / 12 walking down, i.e. the
    // original's run 0, AdjUp 9, ErrTerm -3, carry when the term turns
    // positive. Right: Dx 8, Dy 12, run 0, AdjUp 8, ErrTerm -4.
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    int left = 12;
    int leftErr = 9 - 12;
    int right = 12;
    int rightErr = 8 - 12;
    for (int y = 1; y <= 12; ++y) {
        rows.push_back({ y, { left, right } });
        leftErr += 9;
        if (leftErr > 0) {
            --left;
            leftErr -= 12;
        }
        rightErr += 8;
        if (rightErr > 0) {
            ++right;
            rightErr -= 12;
        }
    }
    rows.push_back({ 13, { 3, 20 } });
    // The hand walk ends at the bottom vertices, as an edge walk must.
    REQUIRE(left == 3);
    REQUIRE(right == 20);
    CHECK(pixels == Expected(rows));
}

// A rectangle fills its rows and columns inclusive.
TEST_CASE("fill_convex_poly_rectangle", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 2, 3 }, { 8, 3 }, { 8, 7 }, { 2, 7 } }, kWhole);
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    for (int y = 3; y <= 7; ++y) {
        rows.push_back({ y, { 2, 8 } });
    }
    CHECK(pixels == Expected(rows));
}

// The same rectangle clipped to (4,4)-(7,6): the clip's last row and column
// are drawn (rClip.right - 1 and rClip.bottom - 1 inclusive).
TEST_CASE("fill_convex_poly_rectangle_clipped", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 2, 3 }, { 8, 3 }, { 8, 7 }, { 2, 7 } }, CRect(4, 4, 7, 6));
    CHECK(pixels == Expected({ { 4, { 4, 6 } }, { 5, { 4, 6 } } }));
}

namespace {

// Fills the polygon with CVidPoly::FillPoly (0x7C13A0) into a cleared 24x16
// buffer clipped to rClip. FillPoly caches edge tables by the vertex
// list's address and count, so each test passes its own static array.
std::vector<DWORD> FillNonConvex(CVIDPOLY_VERTEX* vertices, WORD count, const CRect& rClip, DWORD dwFlags, const CPoint& ptRef = CPoint(0, 0))
{
    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    CVidPoly poly;
    poly.SetPoly(vertices, count);
    DWORD* surface = pixels.data() + rClip.top * kWidth + rClip.left;
    REQUIRE(poly.FillPoly(reinterpret_cast<WORD*>(surface), kWidth * 4, rClip, kColor, dwFlags, ptRef));
    return pixels;
}

// The notched square (2,2) (6,6) (10,2) (10,10) (2,10), by hand from the
// original's edge walk: the edge table holds four edges sorted by top y
// then x (the bottom edge is horizontal and skipped); once the table is
// empty an edge ends when its count reaches -1 (0x7C1911), so the bottom
// row 10 is drawn; spans run between pairs of active edges, inclusive.
std::vector<std::pair<int, std::pair<int, int>>> NotchRows()
{
    return {
        { 3, { 2, 3 } },
        { 3, { 9, 10 } },
        { 4, { 2, 4 } },
        { 4, { 8, 10 } },
        { 5, { 2, 5 } },
        { 5, { 7, 10 } },
        { 6, { 2, 10 } },
        { 7, { 2, 10 } },
        { 8, { 2, 10 } },
        { 9, { 2, 10 } },
        { 10, { 2, 10 } },
        { 2, { 2, 2 } },
        { 2, { 10, 10 } },
    };
}

}

TEST_CASE("fill_poly_notched_square", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    CHECK(FillNonConvex(notch, 5, kWhole, 0x1) == Expected(NotchRows()));
    // The second fill takes the edge table from the cache (0x7C1DE0) and
    // draws the same pixels.
    CHECK(FillNonConvex(notch, 5, kWhole, 0x1) == Expected(NotchRows()));
}

// The notch clipped to (4,3)-(9,8): rows 3..7, columns 4..8 (the clip's last
// row and column are drawn, as in FillConvexPoly).
TEST_CASE("fill_poly_notched_square_clipped", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    for (const auto& [y, span] : NotchRows()) {
        if (y < 3 || y > 7) {
            continue;
        }
        const int first = std::max(span.first, 4);
        const int last = std::min(span.second, 8);
        if (first <= last) {
            rows.push_back({ y, { first, last } });
        }
    }
    CHECK(FillNonConvex(notch, 5, CRect(4, 3, 9, 8), 0x1) == Expected(rows));
}

// CVidInf::FXRender with a polygon (0x79D680), the fill of the container
// and door highlights: FillPoly, or FillConvexPoly when bConvex is set, on
// the FX surface FXLock locked (m_SurfaceDesc), with the clip, colour,
// flags and reference point passed through; FALSE with nothing locked.
TEST_CASE("fx_render_poly_fills_the_locked_fx_surface", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    CVidPoly poly;
    poly.SetPoly(notch, 5);
    CHECK_FALSE(mode.FXRender(&poly, kWhole, kColor, 0x1, CPoint(0, 0), FALSE));

    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    mode.m_SurfaceDesc.lpSurface = pixels.data();
    mode.m_SurfaceDesc.lPitch = kWidth * 4;
    CHECK(mode.FXRender(&poly, kWhole, kColor, 0x1, CPoint(0, 0), FALSE));
    CHECK(pixels == Expected(NotchRows()));

    static CVIDPOLY_VERTEX triangle[] = { { 1, 1 }, { 12, 3 }, { 4, 9 } };
    CVidPoly convex;
    convex.SetPoly(triangle, 3);
    std::fill(pixels.begin(), pixels.end(), 0);
    CHECK(mode.FXRender(&convex, kWhole, kColor, 0x1, CPoint(0, 0), TRUE));
    CHECK(pixels == Fill({ { 1, 1 }, { 12, 3 }, { 4, 9 } }, kWhole));
    mode.m_SurfaceDesc.lpSurface = NULL;
}

// Flag 0x8 flips the rows inside the clip: row y lands on clip.bottom - 1 -
// (y - clip.top).
TEST_CASE("fill_poly_flipped", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    for (const auto& [y, span] : NotchRows()) {
        rows.push_back({ kHeight - 1 - y, span });
    }
    CHECK(FillNonConvex(notch, 5, kWhole, 0x1 | 0x8) == Expected(rows));
}

// Dithered (flag 0x2, which also skips the cache): DrawHLineDithered32
// fills every other pixel, starting where (x + ref.x + y + ref.y) is odd.
TEST_CASE("fill_poly_dithered", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    std::vector<DWORD> expected(kWidth * kHeight, 0);
    for (const auto& [y, span] : NotchRows()) {
        for (int x = span.first; x <= span.second; ++x) {
            if (((x + y) & 1) != 0) {
                expected[y * kWidth + x] = kColor;
            }
        }
    }
    CHECK(FillNonConvex(notch, 5, kWhole, 0x2) == expected);
}

// A polygon whose edges are all horizontal has an empty edge table; the
// original reads through NULL there (bounded: nothing is drawn).
TEST_CASE("fill_poly_flat", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX flat[] = { { 2, 4 }, { 8, 4 }, { 12, 4 } };
    CHECK(FillNonConvex(flat, 3, kWhole, 0x1) == std::vector<DWORD>(kWidth * kHeight, 0));
}

// DrawHLineShaded32 blends a pixel toward the colour by the colour's
// intensity when the pixel is not dark (sum > 31) and its sum / 8 is at most
// that intensity; the top byte ends up clear.
TEST_CASE("draw_hline_shaded32_blends", "[vid]")
{
    CVidPoly poly;
    // Colour (r 90, g 60, b 30): intensity 60, inverse 195.
    const DWORD color = 0x005A3C1E;
    DWORD row[6] = {
        0xFF102030, // sum 96, 96 / 8 = 12 <= 60: blended
        0x00050505, // sum 15: too dark, kept
        0x00FFFFFF, // sum 765, 95 > 60: kept
        0x00808080, // sum 384, 48 <= 60: blended
        0x00010203, // outside the span
        0x00102030, // outside the span
    };
    poly.DrawHLineShaded32(row, 0, 3, color, CRect(0, 0, 6, 1), CPoint(0, 0));
    auto blend = [](DWORD c, DWORD p) { return (c * 60 + 195 * p) >> 8; };
    CHECK(row[0] == ((blend(90, 0x10) << 16) | (blend(60, 0x20) << 8) | blend(30, 0x30)));
    CHECK(row[1] == 0x00050505);
    CHECK(row[2] == 0x00FFFFFF);
    CHECK(row[3] == ((blend(90, 0x80) << 16) | (blend(60, 0x80) << 8) | blend(30, 0x80)));
    CHECK(row[4] == 0x00010203);
    CHECK(row[5] == 0x00102030);

    // Mirrored: xMin 0 is the pixel at rSurface's width, walking left.
    DWORD mirrored[6] = { 0, 0, 0, 0, 0x00808080, 0x00808080 };
    poly.DrawHLineShadedMirrored32(mirrored, 0, 0, color, CRect(0, 0, 5, 1), CPoint(0, 0));
    CHECK(mirrored[4] == 0x00808080);
    CHECK(mirrored[5] == ((blend(90, 0x80) << 16) | (blend(60, 0x80) << 8) | blend(30, 0x80)));
}

// The uncompressed paths of the 32-bpp cell blends walk outside the frame
// as the original does (rows four times apart in sub_7D0950, an extra row
// and column in sub_7D1190). Ours writes only inside the range it is given
// (ruling P4-10): a raw frame at the bottom right of an 8x8 surface leaves
// the guard pixels after it untouched, and sub_7D1190 reads no frame data
// past the frame.
TEST_CASE("cell_blend_raw_frames_stay_in_the_surface", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    // A 4x4 raw frame of palette index 9 (bright white), transparent 255.
    std::vector<BYTE> bam(0x40, 0);
    bam[0xB] = 255;
    std::memset(bam.data() + 0x20, 9, 16);
    std::vector<RGBQUAD> palette(256);
    palette[9] = RGBQUAD { 255, 255, 255, 0 };
    FRAMEENTRY frame = {};
    frame.nWidth = 4;
    frame.nHeight = 4;
    frame.nOffset = 0x20 | 0x80000000;

    auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(resStorage, 0, sizeof(CResCell));
    auto* res = reinterpret_cast<CResCell*>(resStorage);
    res->m_bParsed = TRUE;
    res->m_pBamHeader = reinterpret_cast<BAMHEADER*>(bam.data());
    res->m_pPalette = palette.data();
    auto* cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(cellStorage, 0, sizeof(CVidCell));
    auto* cell = reinterpret_cast<CVidCell*>(cellStorage);
    static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
    cell->m_pFrame = &frame;
    cell->m_bShadowOn = TRUE;

    for (int translucent = 0; translucent < 2; ++translucent) {
        // An 8x8 surface followed by guard pixels.
        std::vector<DWORD> pixels(8 * 8 + 4096, 0x00101010);
        const BYTE* begin = reinterpret_cast<const BYTE*>(pixels.data());
        const CVidCell::SurfaceRange range = { begin, begin + 8 * 8 * 4 };
        DWORD* at = pixels.data() + 4 * 8 + 4;
        if (translucent != 0) {
            CHECK(cell->sub_7D0950(at, 8 * 4, 0, 128, range));
        } else {
            CHECK(cell->sub_7D1190(at, 8 * 4, 0, range));
        }
        for (size_t i = 8 * 8; i < pixels.size(); ++i) {
            INFO("guard pixel " << i << ", translucent " << translucent);
            REQUIRE(pixels[i] == 0x00101010);
        }
        // The frame's first pixel is drawn.
        CHECK(pixels[4 * 8 + 4] != 0x00101010);
    }

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(cellStorage);
    ::operator delete(resStorage);
    ::operator delete(static_cast<void*>(engine));
}

// The same through CVidCell::SurfaceExtent: the current CVidInf's locked
// description (an 8x10 rectangle at (3, 2) of a 16-pixel-pitch surface of
// 12 rows, as BKLock locks the viewport) gives the bytes from its first
// pixel to the end of its last row. A raw 4x4 frame drawn at the
// rectangle's (4, 2) by sub_7D1190 walks 5 rows of 5 pixels, 17 pixels
// apart, all inside those bytes. Its first 16 pixels use the frame's data
// and are drawn; the last 9 would use the bytes after the frame data (the
// original reads them) and must stay as they were. In the second pass those
// bytes are index 9, white, so without sub_7D1190's frame-data bound they
// would be drawn and the test fails (checked by removing the bound). In the
// first pass the frame data ends its buffer, which only a sanitizer would
// notice being read past. Every other surface pixel stays as it was.
TEST_CASE("cell_blend_raw_frames_through_the_locked_surface", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    constexpr int kPitch = 16;
    std::vector<DWORD> surface(kPitch * 12, 0x00101010);
    DWORD* locked = surface.data() + 2 * kPitch + 3;
    mode->m_SurfaceDesc.lpSurface = locked;
    mode->m_SurfaceDesc.lPitch = kPitch * 4;
    mode->m_SurfaceDesc.dwWidth = 8;
    mode->m_SurfaceDesc.dwHeight = 10;

    const CVidCell::SurfaceRange range = CVidCell::SurfaceExtent(locked, kPitch * 4);
    const BYTE* first = reinterpret_cast<const BYTE*>(locked);
    REQUIRE(range.pBegin == first);
    REQUIRE(range.pEnd == first + 9 * kPitch * 4 + 8 * 4);
    // Another pointer is not the locked surface.
    CHECK(CVidCell::SurfaceExtent(surface.data(), kPitch * 4).pBegin == nullptr);

    std::vector<RGBQUAD> palette(256);
    palette[9] = RGBQUAD { 255, 255, 255, 0 };
    palette[3] = RGBQUAD { 200, 200, 200, 0 };
    FRAMEENTRY frame = {};
    frame.nWidth = 4;
    frame.nHeight = 4;
    frame.nOffset = 0x20 | 0x80000000;

    for (int sentinel = 0; sentinel < 2; ++sentinel) {
        // Without a sentinel the frame data ends the buffer; with one, 16
        // bytes of index 9 follow it.
        std::vector<BYTE> bam(0x20 + 16 + (sentinel != 0 ? 16 : 0), 9);
        std::memset(bam.data(), 0, 0x20);
        bam[0xB] = 255;
        std::memset(bam.data() + 0x20, 3, 16);

        auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
        std::memset(resStorage, 0, sizeof(CResCell));
        auto* res = reinterpret_cast<CResCell*>(resStorage);
        res->m_bParsed = TRUE;
        res->m_pBamHeader = reinterpret_cast<BAMHEADER*>(bam.data());
        res->m_pPalette = palette.data();
        auto* cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
        std::memset(cellStorage, 0, sizeof(CVidCell));
        auto* cell = reinterpret_cast<CVidCell*>(cellStorage);
        static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
        cell->m_pFrame = &frame;
        cell->m_bShadowOn = TRUE;

        std::fill(surface.begin(), surface.end(), 0x00101010);
        DWORD* at = locked + 2 * kPitch + 4;
        CHECK(cell->sub_7D1190(at, kPitch * 4, 0, range));

        std::vector<bool> drawn(surface.size(), false);
        for (int pos = 0; pos < 25; ++pos) {
            DWORD* pixel = at + (pos / 5) * (kPitch + 1) + pos % 5;
            const BYTE* b = reinterpret_cast<const BYTE*>(pixel);
            const bool inside = b >= range.pBegin && b + 4 <= range.pEnd;
            // The whole walk is writable, so only the data bound keeps the
            // last 9 pixels as they were.
            REQUIRE(inside);
            if (pos < 16) {
                drawn[static_cast<size_t>(pixel - surface.data())] = true;
            }
        }
        for (size_t i = 0; i < surface.size(); ++i) {
            INFO("pixel " << i << ", sentinel " << sentinel);
            if (drawn[i]) {
                CHECK(surface[i] != 0x00101010);
            } else {
                CHECK(surface[i] == 0x00101010);
            }
        }

        ::operator delete(cellStorage);
        ::operator delete(resStorage);
    }

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// The 16- and 24-bit paths. iwd2r runs at 32 bpp only (the DirectDraw shim
// lists no other mode), so the game never reaches these; they are ported
// for the record and checked here by hand and by oracle cases against the
// original (vid/cell_blt8to16, vid/cell_blt8to24, vid/blt_tile8to16,
// vid/draw_line16, vid/draw_ellipse24).
namespace {

// A 4x3 frame, transparent index 0:
//   5 0 6 7
//   0 0 8 0
//   9 9 0 1
const BYTE kFrame[12] = { 5, 0, 6, 7, 0, 0, 8, 0, 9, 9, 0, 1 };
// The same run-length encoded: a transparent pixel and its run - 1.
const BYTE kFrameRle[] = { 5, 0, 0, 6, 7, 0, 1, 8, 0, 0, 9, 9, 0, 0, 1 };

struct CellFixture {
    explicit CellFixture(bool compressed)
    {
        bam.assign(0x40, 0);
        bam[0xB] = 0;
        if (compressed) {
            std::memcpy(bam.data() + 0x20, kFrameRle, sizeof(kFrameRle));
        } else {
            std::memcpy(bam.data() + 0x20, kFrame, sizeof(kFrame));
        }
        frame.nWidth = 4;
        frame.nHeight = 3;
        frame.nOffset = 0x20 | (compressed ? 0u : 0x80000000u);
        resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
        std::memset(resStorage, 0, sizeof(CResCell));
        auto* res = reinterpret_cast<CResCell*>(resStorage);
        res->m_bParsed = TRUE;
        res->m_pBamHeader = reinterpret_cast<BAMHEADER*>(bam.data());
        cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
        std::memset(cellStorage, 0, sizeof(CVidCell));
        cell = reinterpret_cast<CVidCell*>(cellStorage);
        static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
        cell->m_pFrame = &frame;
        palette.resize(256);
        for (int i = 0; i < 256; ++i) {
            CVidImage::rgbTempPal[i] = 0xEE000000u | static_cast<DWORD>(i) * 0x010203u;
            // For the fast blits, which realize the palette themselves.
            palette[i] = RGBQUAD { static_cast<BYTE>(i), static_cast<BYTE>(255 - i),
                static_cast<BYTE>(i * 7), 0 };
        }
        res->m_pPalette = palette.data();
    }
    ~CellFixture()
    {
        ::operator delete(cellStorage);
        ::operator delete(resStorage);
    }
    CellFixture(const CellFixture&) = delete;
    CellFixture& operator=(const CellFixture&) = delete;

    std::vector<BYTE> bam;
    std::vector<RGBQUAD> palette;
    FRAMEENTRY frame = {};
    unsigned char* resStorage;
    unsigned char* cellStorage;
    CVidCell* cell;
};

constexpr int kDepthW = 8;
constexpr int kDepthH = 5;

// The pixels { x, y, palette index } a blit of kFrame from source (1, 0) to
// (2, 1) draws: source columns 1..3 land on x 2..4.
std::vector<std::pair<std::pair<int, int>, int>> FrameAt21()
{
    return { { { 3, 1 }, 6 }, { { 4, 1 }, 7 }, { { 3, 2 }, 8 }, { { 2, 3 }, 9 }, { { 4, 3 }, 1 } };
}

}

TEST_CASE("blt8to16_point_and_clip", "[vid]")
{
    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);

        std::vector<WORD> pixels(kDepthW * kDepthH, 0xAAAA);
        REQUIRE(fixture.cell->Blt8To16(pixels.data(), kDepthW * 2, CPoint(2, 1), CRect(0, 0, kDepthW, kDepthH), 0,
            CPoint(1, 0)));
        std::vector<WORD> expected(kDepthW * kDepthH, 0xAAAA);
        for (const auto& [at, index] : FrameAt21()) {
            expected[at.second * kDepthW + at.first] = static_cast<WORD>(CVidImage::rgbTempPal[index]);
        }
        CHECK(pixels == expected);

        // Clipped to (3,0)-(8,3): rows 1 and 2, columns 3 and 4.
        std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0xAAAA));
        REQUIRE(fixture.cell->Blt8To16(pixels.data(), kDepthW * 2, CPoint(2, 1), CRect(3, 0, kDepthW, 3), 0,
            CPoint(1, 0)));
        std::fill(expected.begin(), expected.end(), static_cast<WORD>(0xAAAA));
        expected[1 * kDepthW + 3] = static_cast<WORD>(CVidImage::rgbTempPal[6]);
        expected[1 * kDepthW + 4] = static_cast<WORD>(CVidImage::rgbTempPal[7]);
        expected[2 * kDepthW + 3] = static_cast<WORD>(CVidImage::rgbTempPal[8]);
        CHECK(pixels == expected);
    }
}

TEST_CASE("blt8to24_point", "[vid]")
{
    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);

        std::vector<BYTE> pixels(kDepthW * kDepthH * 3, 0xAA);
        REQUIRE(fixture.cell->Blt8To24(pixels.data(), kDepthW * 3, CPoint(2, 1), CRect(0, 0, kDepthW, kDepthH), 0,
            CPoint(1, 0)));
        std::vector<BYTE> expected(kDepthW * kDepthH * 3, 0xAA);
        for (const auto& [at, index] : FrameAt21()) {
            const DWORD rgb = CVidImage::rgbTempPal[index];
            BYTE* p = expected.data() + (at.second * kDepthW + at.first) * 3;
            p[0] = static_cast<BYTE>(rgb);
            p[1] = static_cast<BYTE>(rgb >> 8);
            p[2] = static_cast<BYTE>(rgb >> 16);
        }
        CHECK(pixels == expected);
    }
}

// Blt8To16 (0x7AF1C0): the whole frame at the surface pointer through the
// realized 16-bit palette, one WORD a pixel, rows bottom-up by lPitch in
// bytes, the transparent index left untouched. The raw walk reads one byte
// ahead (0x7AF384), unlike the 24- and 32-bit siblings. Registered as an
// oracle case (vid/cell_blt8to16_fast); this pins the walk in the harness's
// own terms.
TEST_CASE("blt8to16_fast", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    const auto setMode16 = [&mode](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode->m_dwRBitMask = 0x1Fu << rShift;
        mode->m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode->m_dwBBitMask = 0x1F;
        mode->m_dwRBitShift = rShift;
        mode->m_dwGBitShift = 5;
        mode->m_dwBBitShift = 0;
        mode->field_C2 = 3;
        mode->field_C6 = 8 - gBits;
        mode->field_CA = 3;
    };

    for (int format = 0; format < 2; ++format) {
        setMode16(format == 0);
        for (int compressed = 0; compressed < 2; ++compressed) {
            CellFixture fixture(compressed != 0);
            INFO("format " << format << ", compressed " << compressed);
            for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
                INFO("shadow " << shadowOn);
                fixture.cell->m_bShadowOn = shadowOn != 0;

                std::vector<WORD> pixels(kDepthW * kDepthH, 0xAAAA);
                REQUIRE(fixture.cell->sub_7AF1C0(pixels.data(), kDepthW * 2, 0));

                std::vector<WORD> expected(kDepthW * kDepthH, 0xAAAA);
                for (int y = 0; y < 3; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        // Raw frames read one byte ahead, so the frame lands
                        // shifted; past its end the BAM's zero is index 0.
                        const int at = y * 4 + x + (compressed ? 0 : 1);
                        const BYTE index = at < 12 ? kFrame[at] : 0;
                        if (index != 0) {
                            expected[y * kDepthW + x] = static_cast<WORD>(CVidImage::rgbTempPal[index]);
                        }
                    }
                }
                CHECK(pixels == expected);

                // Entry 1 takes only the low word of the mode's shadow colour
                // unless the cell draws its shadow entry.
                const WORD entry = static_cast<WORD>(CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY]);
                if (shadowOn != 0) {
                    CHECK(entry != static_cast<WORD>(mode->field_24 & 0xFFFF));
                } else {
                    CHECK(entry == static_cast<WORD>(mode->field_24 & 0xFFFF));
                }
            }
        }
    }

    // The compressed walk's early row end: the original's loop closes with
    // `ja` (0x7AF358), which reads the carry of the transparent comparison
    // above (`dec` does not set it), so a drawn index below the transparent
    // one ends the row early. 3x1, transparent 104, indices 139, 101, 1: the
    // row ends after 101, so 1 lands in the next row (none here) and the
    // third pixel keeps the destination.
    CellFixture quirk(true);
    quirk.bam[0xB] = 104;
    quirk.bam[0x20] = 139;
    quirk.bam[0x21] = 101;
    quirk.bam[0x22] = 1;
    quirk.frame.nWidth = 3;
    quirk.frame.nHeight = 1;
    quirk.cell->m_bShadowOn = TRUE;
    std::vector<WORD> quirkPixels(kDepthW * kDepthH, 0xAAAA);
    CHECK(quirk.cell->sub_7AF1C0(quirkPixels.data(), kDepthW * 2, 0));
    CHECK(quirkPixels[0] == static_cast<WORD>(CVidImage::rgbTempPal[139]));
    CHECK(quirkPixels[1] == static_cast<WORD>(CVidImage::rgbTempPal[101]));
    CHECK(quirkPixels[2] == 0xAAAA); // below the transparent index: row ended

    // A zero-size frame draws nothing and answers TRUE.
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<WORD> untouched(16, 0xAAAA);
    CHECK(empty.cell->sub_7AF1C0(untouched.data(), kDepthW * 2, 0) == TRUE);
    CHECK(untouched == std::vector<WORD>(16, 0xAAAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// Blt8To24 (0x7CECD0): the whole frame at the surface pointer through the
// realized 24-bit palette, three bytes a pixel, rows bottom-up by lPitch in
// bytes, the transparent index left untouched. Registered as an oracle case
// (vid/cell_blt8to24_fast); this pins the walk in the harness's own terms.
TEST_CASE("blt8to24_fast", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);
        for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
            INFO("shadow " << shadowOn);
            fixture.cell->m_bShadowOn = shadowOn != 0;

            std::vector<BYTE> pixels(kDepthW * kDepthH * 3, 0xAA);
            REQUIRE(fixture.cell->sub_7CECD0(pixels.data(), kDepthW * 3, 0));

            // The realized palette, read back after the call: the palette
            // conversion itself is the oracle case's business, the walk is this.
            std::vector<BYTE> expected(kDepthW * kDepthH * 3, 0xAA);
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const BYTE index = kFrame[y * 4 + x];
                    if (index == 0) {
                        continue; // transparent
                    }
                    const DWORD rgb = CVidImage::rgbTempPal[index];
                    BYTE* p = expected.data() + (y * kDepthW + x) * 3;
                    p[0] = static_cast<BYTE>(rgb);
                    p[1] = static_cast<BYTE>(rgb >> 8);
                    p[2] = static_cast<BYTE>(rgb >> 16);
                }
            }
            CHECK(pixels == expected);

            // The frame's index 1 (bottom right) takes the mode's shadow
            // colour unless the cell draws its shadow entry.
            const BYTE* shadow = pixels.data() + (2 * kDepthW + 3) * 3;
            const DWORD drawn = static_cast<DWORD>(shadow[0]) | (static_cast<DWORD>(shadow[1]) << 8)
                | (static_cast<DWORD>(shadow[2]) << 16);
            CHECK(drawn == (shadowOn != 0 ? static_cast<DWORD>(CVidImage::rgbTempPal[1]) : static_cast<DWORD>(mode->field_24)));
        }
    }

    // A zero-size frame draws nothing and answers TRUE.
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<BYTE> untouched(16, 0xAA);
    CHECK(empty.cell->sub_7CECD0(untouched.data(), kDepthW * 3, 0) == TRUE);
    CHECK(untouched == std::vector<BYTE>(16, 0xAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// Blt8To16ClearShadow (0x7AF8A0): the whole frame at the surface pointer
// through the realized 16-bit palette, one WORD a pixel, rows bottom-up by
// lPitch in bytes, the transparent index left untouched. With the cell's
// shadow on, index 1 is half the destination (per channel through the mode's
// masks) instead of a palette entry; the raw walk does that for any index
// below two. With the shadow off the entry is the mode's shadow colour and
// the frame is drawn plainly. Registered as an oracle case
// (vid/cell_clear_shadow16); this pins the walk in the harness's own terms.
TEST_CASE("clear_shadow16", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    DWORD shadowMask = 0;
    const auto setMode16 = [&](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode->m_dwRBitMask = 0x1Fu << rShift;
        mode->m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode->m_dwBBitMask = 0x1F;
        mode->m_dwRBitShift = rShift;
        mode->m_dwGBitShift = 5;
        mode->m_dwBBitShift = 0;
        mode->field_C2 = 3;
        mode->field_C6 = 8 - gBits;
        mode->field_CA = 3;
        shadowMask = ((mode->m_dwRBitMask >> 1) & mode->m_dwRBitMask)
            + ((mode->m_dwGBitMask >> 1) & mode->m_dwGBitMask)
            + ((mode->m_dwBBitMask >> 1) & mode->m_dwBBitMask);
    };

    constexpr WORD kDst = 0xFFFF;

    for (int format = 0; format < 2; ++format) {
        setMode16(format == 0);
        const WORD halved = static_cast<WORD>((kDst >> 1) & shadowMask);
        for (int compressed = 0; compressed < 2; ++compressed) {
            CellFixture fixture(compressed != 0);
            for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
                INFO("format " << format << ", compressed " << compressed << ", shadow " << shadowOn);
                fixture.cell->m_bShadowOn = shadowOn != 0;

                std::vector<WORD> pixels(kDepthW * kDepthH, kDst);
                REQUIRE(fixture.cell->sub_7AF8A0(pixels.data(), kDepthW * 2, 0));

                std::vector<WORD> expected(kDepthW * kDepthH, kDst);
                for (int y = 0; y < 3; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        const BYTE index = kFrame[y * 4 + x];
                        if (index == 0) {
                            continue; // transparent
                        }
                        const size_t at = static_cast<size_t>(y) * kDepthW + x;
                        if (shadowOn != 0 && (compressed != 0 ? index == 1 : index < 2)) {
                            expected[at] = halved;
                        } else {
                            // The realized palette, read back after the call:
                            // with the shadow off entry 1 is the mode's colour.
                            expected[at] = static_cast<WORD>(CVidImage::rgbTempPal[index]);
                        }
                    }
                }
                CHECK(pixels == expected);
                CHECK(halved == (format == 0 ? 0x7BEF : 0x3DEF));
            }
        }
    }

    // The raw walk halves for any index below two (cmp eax,1; ja at 0x7AFADB),
    // the compressed one only index 1; a nonzero transparent index lets index
    // 0 reach the write. 3x1, transparent 200, indices 0, 1, 5.
    {
        setMode16(true);
        const WORD halved = static_cast<WORD>((kDst >> 1) & shadowMask);
        for (int compressed = 0; compressed < 2; ++compressed) {
            CellFixture fixture(compressed != 0);
            fixture.bam[0xB] = 200;
            fixture.bam[0x20] = 0;
            fixture.bam[0x21] = 1;
            fixture.bam[0x22] = 5;
            fixture.frame.nWidth = 3;
            fixture.frame.nHeight = 1;
            fixture.cell->m_bShadowOn = TRUE;
            std::vector<WORD> row(kDepthW, kDst);
            CHECK(fixture.cell->sub_7AF8A0(row.data(), kDepthW * 2, 0));
            INFO("compressed " << compressed);
            CHECK(row[0] == (compressed != 0 ? static_cast<WORD>(CVidImage::rgbTempPal[0]) : halved));
            CHECK(row[1] == halved);
            CHECK(row[2] == static_cast<WORD>(CVidImage::rgbTempPal[5]));
        }

        // With the shadow off the compressed branch stays a plain palette
        // write: the original halves there with a half mask it never computes
        // (an uninitialized read), which cannot be reproduced; the 32-bit
        // sibling writes the realized palette, as this does.
        CellFixture off(true);
        off.bam[0xB] = 200;
        off.bam[0x20] = 0;
        off.bam[0x21] = 1;
        off.bam[0x22] = 5;
        off.frame.nWidth = 3;
        off.frame.nHeight = 1;
        off.cell->m_bShadowOn = FALSE;
        std::vector<WORD> row(kDepthW, kDst);
        CHECK(off.cell->sub_7AF8A0(row.data(), kDepthW * 2, 0));
        CHECK(row[0] == static_cast<WORD>(CVidImage::rgbTempPal[0]));
        CHECK(row[1] == static_cast<WORD>(CVidImage::rgbTempPal[1]));
        CHECK(row[1] == static_cast<WORD>(mode->field_24 & 0xFFFF));
        CHECK(row[2] == static_cast<WORD>(CVidImage::rgbTempPal[5]));
    }

    // A zero-size frame draws nothing and answers TRUE (the original has no
    // guard and its do-while loops would spin; Render never passes one).
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<WORD> untouched(kDepthW * kDepthH, 0xAAAA);
    CHECK(empty.cell->sub_7AF8A0(untouched.data(), kDepthW * 2, 0) == TRUE);
    CHECK(untouched == std::vector<WORD>(kDepthW * kDepthH, 0xAAAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// sub_7CF3F0 (0x7CF3F0): the clear-shadow twin of sub_7CECD0. With the cell's
// shadow on, the source's shadow entry halves the destination pixel under it
// (each channel trimmed of its low bit by the mode's masks); with the shadow
// off, the palette's shadow entry holds the mode's field_24 and index 1 draws
// that. Registered as an oracle case (vid/cell_clear_shadow24); this pins the
// walk and the two branches in the harness's own terms.
TEST_CASE("blt8to24_clear_shadow", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitMask = 0x00FF0000;
    mode->m_dwGBitMask = 0x0000FF00;
    mode->m_dwBBitMask = 0x000000FF;
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    const DWORD dwShadowMask = 0x007F7F7F;

    // Every 24-bit pixel the call sees before it overwrites it.
    std::vector<BYTE> before(kDepthW * kDepthH * 3);
    for (size_t i = 0; i < before.size(); ++i) {
        before[i] = static_cast<BYTE>(0x30 + i * 5);
    }

    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);
        for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
            INFO("shadow " << shadowOn);
            fixture.cell->m_bShadowOn = shadowOn != 0;

            std::vector<BYTE> pixels = before;
            REQUIRE(fixture.cell->sub_7CF3F0(pixels.data(), kDepthW * 3, 0));

            std::vector<BYTE> expected = before;
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const BYTE index = kFrame[y * 4 + x];
                    if (index == 0) {
                        continue; // left as it was
                    }
                    BYTE* p = expected.data() + (y * kDepthW + x) * 3;
                    if (shadowOn != 0 && index == CVidPalette::SHADOW_ENTRY) {
                        DWORD rgb = (static_cast<DWORD>(p[2]) << 16)
                            | (static_cast<DWORD>(p[1]) << 8)
                            | static_cast<DWORD>(p[0]);
                        rgb = (rgb >> 1) & dwShadowMask;
                        p[0] = static_cast<BYTE>(rgb);
                        p[1] = static_cast<BYTE>(rgb >> 8);
                        p[2] = static_cast<BYTE>(rgb >> 16);
                    } else {
                        const DWORD rgb = CVidImage::rgbTempPal[index];
                        p[0] = static_cast<BYTE>(rgb);
                        p[1] = static_cast<BYTE>(rgb >> 8);
                        p[2] = static_cast<BYTE>(rgb >> 16);
                    }
                }
            }
            CHECK(pixels == expected);

            // The shadow entry (bottom right) is the branch under test.
            const BYTE* shadow = pixels.data() + (2 * kDepthW + 3) * 3;
            const DWORD drawn = static_cast<DWORD>(shadow[0]) | (static_cast<DWORD>(shadow[1]) << 8)
                | (static_cast<DWORD>(shadow[2]) << 16);
            const DWORD shadowSource = (static_cast<DWORD>(before[(2 * kDepthW + 3) * 3 + 2]) << 16)
                | (static_cast<DWORD>(before[(2 * kDepthW + 3) * 3 + 1]) << 8)
                | static_cast<DWORD>(before[(2 * kDepthW + 3) * 3]);
            CHECK(drawn == (shadowOn != 0 ? ((shadowSource >> 1) & dwShadowMask) : static_cast<DWORD>(mode->field_24)));
        }
    }

    // The raw path skips palette index 0, not the frame's transparent index
    // (0x7CF61D): with the transparent index changed to one the frame does not
    // use, the zero pixels stay untouched and index 1 still draws the shadow
    // colour.
    CellFixture raw(false);
    raw.bam[0xB] = 4;
    raw.cell->m_bShadowOn = FALSE;
    std::vector<BYTE> pixels = before;
    REQUIRE(raw.cell->sub_7CF3F0(pixels.data(), kDepthW * 3, 0));
    std::vector<BYTE> expected = before;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            const BYTE index = kFrame[y * 4 + x];
            if (index == 0) {
                continue;
            }
            const DWORD rgb = CVidImage::rgbTempPal[index];
            BYTE* p = expected.data() + (y * kDepthW + x) * 3;
            p[0] = static_cast<BYTE>(rgb);
            p[1] = static_cast<BYTE>(rgb >> 8);
            p[2] = static_cast<BYTE>(rgb >> 16);
        }
    }
    CHECK(pixels == expected);

    // A zero-size frame draws nothing and answers TRUE before any palette work.
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<BYTE> untouched(16, 0xAA);
    CHECK(empty.cell->sub_7CF3F0(untouched.data(), kDepthW * 3, 0) == TRUE);
    CHECK(untouched == std::vector<BYTE>(16, 0xAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// Blt8To16Brightest (0x7AFB50): the whole frame at the surface pointer
// through the realized 16-bit palette, one WORD a pixel, rows bottom-up by
// lPitch in bytes, the transparent index left untouched, and every drawn pixel
// the per-channel brighter of the destination and the palette (through the
// mode's masks). Registered as an oracle case (vid/cell_brightest16); this
// pins the walk and the max in the harness's own terms.
TEST_CASE("brightest16", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    const auto setMode16 = [&](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode->m_dwRBitMask = 0x1Fu << rShift;
        mode->m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode->m_dwBBitMask = 0x1F;
        mode->m_dwRBitShift = rShift;
        mode->m_dwGBitShift = 5;
        mode->m_dwBBitShift = 0;
        mode->field_C2 = 3;
        mode->field_C6 = 8 - gBits;
        mode->field_CA = 3;
    };

    constexpr WORD kDst = 0x1234;
    const auto brighter = [](DWORD a, DWORD b) { return a > b ? a : b; };
    const auto brightest = [&](WORD dest, WORD pal) {
        return static_cast<WORD>(
            brighter(dest & mode->m_dwRBitMask, pal & mode->m_dwRBitMask)
            | brighter(dest & mode->m_dwGBitMask, pal & mode->m_dwGBitMask)
            | brighter(dest & mode->m_dwBBitMask, pal & mode->m_dwBBitMask));
    };

    for (int format = 0; format < 2; ++format) {
        setMode16(format == 0);
        for (int compressed = 0; compressed < 2; ++compressed) {
            CellFixture fixture(compressed != 0);
            for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
                INFO("format " << format << ", compressed " << compressed << ", shadow " << shadowOn);
                fixture.cell->m_bShadowOn = shadowOn != 0;

                std::vector<WORD> pixels(kDepthW * kDepthH, kDst);
                REQUIRE(fixture.cell->sub_7AFB50(pixels.data(), kDepthW * 2, 0));

                std::vector<WORD> expected(kDepthW * kDepthH, kDst);
                for (int y = 0; y < 3; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        const BYTE index = kFrame[y * 4 + x];
                        if (index == 0) {
                            continue; // transparent
                        }
                        // The realized palette, read back after the call: with
                        // the shadow off entry 1 is the mode's colour.
                        const WORD pal = static_cast<WORD>(CVidImage::rgbTempPal[index]);
                        expected[y * kDepthW + x] = brightest(kDst, pal);
                    }
                }
                CHECK(pixels == expected);

                // kFrame's (0, 0) is index 5 - palette {5, 250, 35} (B, G, R)
                // against 0x1234: 5-6-5 keeps R 4, G 62, B 20 (0x27D4), 5-5-5
                // R 4, G 31, B 20 (0x13F4).
                CHECK(pixels[0] == (format == 0 ? 0x27D4 : 0x13F4));
            }
        }
    }

    // A zero-size frame draws nothing and answers TRUE (the original has no
    // guard and its do-while loops would spin; Render never passes one).
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<WORD> untouched(kDepthW * kDepthH, 0xAAAA);
    CHECK(empty.cell->sub_7AFB50(untouched.data(), kDepthW * 2, 0) == TRUE);
    CHECK(untouched == std::vector<WORD>(kDepthW * kDepthH, 0xAAAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// sub_7CF650 (0x7CF650): the brightest twin of sub_7CECD0. Each channel of a
// drawn pixel keeps the larger of the destination byte and the realized
// palette's, compared and written separately (B, G, R); the transparent index
// is left untouched. Registered as an oracle case (vid/cell_brightest24);
// this pins the walk, the per-channel max and the shadow-off palette in the
// harness's own terms.
TEST_CASE("blt8to24_brightest", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    // A destination with channels on both sides of the palette's bytes.
    std::vector<BYTE> before(kDepthW * kDepthH * 3);
    for (size_t i = 0; i < before.size(); ++i) {
        before[i] = static_cast<BYTE>((i * 37 + 11) & 0xFF);
    }

    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);
        for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
            INFO("shadow " << shadowOn);
            fixture.cell->m_bShadowOn = shadowOn != 0;

            std::vector<BYTE> pixels = before;
            REQUIRE(fixture.cell->sub_7CF650(pixels.data(), kDepthW * 3, 0));

            std::vector<BYTE> expected = before;
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const BYTE index = kFrame[y * 4 + x];
                    if (index == 0) {
                        continue; // transparent, left as it was
                    }
                    BYTE* p = expected.data() + (y * kDepthW + x) * 3;
                    const DWORD rgb = CVidImage::rgbTempPal[index];
                    p[0] = std::max(p[0], static_cast<BYTE>(rgb));
                    p[1] = std::max(p[1], static_cast<BYTE>(rgb >> 8));
                    p[2] = std::max(p[2], static_cast<BYTE>(rgb >> 16));
                }
            }
            CHECK(pixels == expected);

            // The shadow entry (bottom right) draws from the palette like any
            // other index; with the shadow off the palette's entry 1 is the
            // mode's field_24, whose bytes take part in the max.
            const size_t at = (2 * kDepthW + 3) * 3;
            const RGBQUAD& shadowColor = fixture.palette[CVidPalette::SHADOW_ENTRY];
            const DWORD paletteValue = (static_cast<DWORD>(shadowColor.rgbRed) << 16)
                | (static_cast<DWORD>(shadowColor.rgbGreen) << 8) | shadowColor.rgbBlue;
            CHECK(CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY]
                == (shadowOn != 0 ? paletteValue : static_cast<DWORD>(mode->field_24)));
            const DWORD drawn = CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY];
            for (int channel = 0; channel < 3; ++channel) {
                const BYTE paletteByte = static_cast<BYTE>(drawn >> (channel * 8));
                CHECK(pixels[at + channel] == std::max(before[at + channel], paletteByte));
            }
        }
    }

    // A zero-size frame draws nothing and answers TRUE before any palette work.
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<BYTE> untouched(16, 0xAA);
    CHECK(empty.cell->sub_7CF650(untouched.data(), kDepthW * 3, 0) == TRUE);
    CHECK(untouched == std::vector<BYTE>(16, 0xAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// Blt8To24Translucent (0x7CF8D0): the fast 24-bit walk with each drawn
// pixel blended into the destination per byte, dst = (src * (256 -
// nTransVal) + dst * nTransVal) >> 8, through the realized palette. A pixel
// whose realized colour is the mode's shadow colour is skipped in the
// compressed walk (not drawn opaque as in the 32-bit form); the raw walk
// tests the palette entry's blue byte against the whole shadow colour and
// the transparent index (the original's clobbered register), so a shadow
// colour above 255 never skips there. Registered as an oracle case
// (vid/cell_translucent24); this pins the walk and the blend by hand.
TEST_CASE("blt8to24_translucent", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    // The expected canvas for a call: the realized palette read back after
    // it (the conversion is the oracle case's business, as blt8to24_fast's),
    // each drawn pixel (src * (256 - trans) + 0xAA * trans) >> 8 a byte.
    const auto expectedFor = [](int trans, bool isCompressed, const CVidInf* vidMode) {
        std::vector<BYTE> expected(kDepthW * kDepthH * 3, 0xAA);
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 4; ++x) {
                const BYTE index = kFrame[y * 4 + x];
                const DWORD rgb = CVidImage::rgbTempPal[index] & 0xFFFFFF;
                if (isCompressed) {
                    if (index == 0 || rgb == static_cast<DWORD>(vidMode->field_24)) {
                        continue; // the transparent index; the shadow colour skipped
                    }
                } else if (static_cast<BYTE>(rgb) == static_cast<DWORD>(vidMode->field_24)
                    || static_cast<BYTE>(rgb) == 0) {
                    continue; // the raw walk's blue-byte tests (0 is the fixture's index)
                }
                BYTE* p = expected.data() + (y * kDepthW + x) * 3;
                p[0] = static_cast<BYTE>(((rgb & 0xFF) * (256 - trans) + 0xAA * trans) >> 8);
                p[1] = static_cast<BYTE>((((rgb >> 8) & 0xFF) * (256 - trans) + 0xAA * trans) >> 8);
                p[2] = static_cast<BYTE>((((rgb >> 16) & 0xFF) * (256 - trans) + 0xAA * trans) >> 8);
            }
        }
        return expected;
    };

    for (int compressed = 0; compressed < 2; ++compressed) {
        INFO("compressed " << compressed);
        for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
            INFO("shadow " << shadowOn);
            CellFixture fixture(compressed != 0);
            fixture.cell->m_bShadowOn = shadowOn != 0;

            // Half blend.
            std::vector<BYTE> pixels(kDepthW * kDepthH * 3, 0xAA);
            REQUIRE(fixture.cell->sub_7CF8D0(pixels.data(), kDepthW * 3, 0, 128));
            CHECK(pixels == expectedFor(128, compressed != 0, mode.get()));

            // The frame's index 1 (bottom right) is the shadow entry: with
            // the cell's shadow off it is the mode's colour — skipped under
            // RLE (stays 0xAA), blended under raw (the quirk above); with
            // the shadow on it keeps the palette's own entry 1 and blends
            // like any other colour.
            const DWORD entry = CVidImage::rgbTempPal[1] & 0xFFFFFF;
            const std::vector<BYTE> blendedEntry = {
                static_cast<BYTE>(((entry & 0xFF) + 0xAA) >> 1),
                static_cast<BYTE>((((entry >> 8) & 0xFF) + 0xAA) >> 1),
                static_cast<BYTE>(((entry >> 16) + 0xAA) >> 1),
            };
            const BYTE* shadow = pixels.data() + (2 * kDepthW + 3) * 3;
            if (compressed != 0) {
                CHECK(std::vector<BYTE>(shadow, shadow + 3)
                    == (shadowOn != 0 ? blendedEntry : std::vector<BYTE> { 0xAA, 0xAA, 0xAA }));
            } else {
                CHECK(std::vector<BYTE>(shadow, shadow + 3)
                    == (shadowOn != 0 ? blendedEntry : std::vector<BYTE> { 0x5D, 0x66, 0x6E }));
            }

            // nTransVal 256 leaves the destination; 0 draws the source.
            std::fill(pixels.begin(), pixels.end(), 0xAA);
            REQUIRE(fixture.cell->sub_7CF8D0(pixels.data(), kDepthW * 3, 0, 256));
            CHECK(pixels == std::vector<BYTE>(kDepthW * kDepthH * 3, 0xAA));
            REQUIRE(fixture.cell->sub_7CF8D0(pixels.data(), kDepthW * 3, 0, 0));
            CHECK(pixels == expectedFor(0, compressed != 0, mode.get()));
        }
    }

    // A zero-size frame draws nothing and answers TRUE.
    CellFixture empty(false);
    empty.frame.nWidth = 0;
    empty.frame.nHeight = 0;
    std::vector<BYTE> untouched(16, 0xAA);
    CHECK(empty.cell->sub_7CF8D0(untouched.data(), kDepthW * 3, 0, 128) == TRUE);
    CHECK(untouched == std::vector<BYTE>(16, 0xAA));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// Blt8To16Translucent (0x7AFED0): the whole frame at the surface pointer
// through the realized 16-bit palette, one WORD a pixel, rows bottom-up by
// lPitch in bytes, each drawn pixel blended through the 64x64x32 table at
// weight nTransVal >> 3 (the table's row is the destination channel, its
// column the palette's). A palette word equal to the mode's shadow colour is
// left untouched. The raw walk draws height + 1 rows and does not skip the
// transparent index; the compressed one does. Registered as an oracle case
// (vid/cell_translucent16); this pins the walk and the blend in the harness's
// own terms.
TEST_CASE("translucent16", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x00332211;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    const auto setMode16 = [&](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode->m_dwRBitMask = 0x1Fu << rShift;
        mode->m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode->m_dwBBitMask = 0x1F;
        mode->m_dwRBitShift = rShift;
        mode->m_dwGBitShift = 5;
        mode->m_dwBBitShift = 0;
        mode->field_C2 = 3;
        mode->field_C6 = 8 - gBits;
        mode->field_CA = 3;
    };

    constexpr WORD kDst = 0x1234;
    const auto blend = [&](WORD dest, WORD pal, DWORD weight) {
        const DWORD masks[3] = { mode->m_dwRBitMask, mode->m_dwGBitMask, mode->m_dwBBitMask };
        const DWORD shifts[3] = { mode->m_dwRBitShift, mode->m_dwGBitShift, mode->m_dwBBitShift };
        WORD out = 0;
        for (int c = 0; c < 3; ++c) {
            const DWORD d = (dest & masks[c]) >> shifts[c];
            const DWORD p = (pal & masks[c]) >> shifts[c];
            const BYTE value = CVidImage::m_pBlendTable[(d * 0x40 + p) * 0x20 + weight];
            out |= static_cast<WORD>(value << shifts[c]);
        }
        return out;
    };

    for (int format = 0; format < 2; ++format) {
        setMode16(format == 0);
        for (int compressed = 0; compressed < 2; ++compressed) {
            CellFixture fixture(compressed != 0);
            for (int shadowOn = 0; shadowOn < 2; ++shadowOn) {
                for (int trans = 0; trans < 3; ++trans) {
                    const INT nTransVal = trans == 0 ? 0 : (trans == 1 ? 128 : 256);
                    INFO("format " << format << ", compressed " << compressed << ", shadow " << shadowOn
                                   << ", nTransVal " << nTransVal);
                    fixture.cell->m_bShadowOn = shadowOn != 0;

                    std::vector<WORD> pixels(kDepthW * kDepthH, kDst);
                    REQUIRE(fixture.cell->sub_7AFED0(pixels.data(), kDepthW * 2, 0, nTransVal));

                    // The raw walk draws one row past the frame (the BAM's zero
                    // bytes there are index 0) and blends every pixel; the
                    // compressed walk draws height rows and skips transparent
                    // tokens.
                    const int rows = compressed != 0 ? 3 : 4;
                    const DWORD weight = static_cast<DWORD>(nTransVal) >> 3;
                    std::vector<WORD> expected(kDepthW * kDepthH, kDst);
                    for (int y = 0; y < rows; ++y) {
                        for (int x = 0; x < 4; ++x) {
                            const BYTE index = y < 3 ? kFrame[y * 4 + x] : 0;
                            if (compressed != 0 && index == 0) {
                                continue; // transparent token
                            }
                            const WORD pal = static_cast<WORD>(CVidImage::rgbTempPal[index]);
                            if (pal != static_cast<WORD>(mode->field_24 & 0xFFFF)) {
                                expected[y * kDepthW + x] = blend(kDst, pal, weight);
                            }
                        }
                    }
                    CHECK(pixels == expected);

                    // Weight 0 is the palette itself: kFrame's (0, 0) is index
                    // 5, whose realized value is 0x27C0 at 5-6-5 and 0x13E0 at
                    // 5-5-5.
                    if (nTransVal == 0) {
                        CHECK(pixels[0] == (format == 0 ? 0x27C0 : 0x13E0));
                    }

                    // The shadow entry: with the cell's shadow off entry 1 is
                    // the mode's colour and the pixel keeps the destination.
                    if (shadowOn == 0) {
                        CHECK(pixels[2 * kDepthW + 3] == kDst);
                    } else {
                        CHECK(pixels[2 * kDepthW + 3] != kDst);
                    }
                }
            }
        }
    }

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}

// BltTile8To16 writes pixel pairs as one DWORD. Dithered (0x8), even rows
// keep the first pixel of each pair and put field_24's low word in the
// second; odd rows put field_24 in the first and OR its high word into the
// second (0x7B23A1).
TEST_CASE("blt_tile8to16", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x12345678;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;
    for (int i = 0; i < 256; ++i) {
        CVidImage::rgbTempPal[i] = 0x00010000u * static_cast<DWORD>(i) + 0x0100u * static_cast<DWORD>(i);
    }
    auto* tileStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidTile)));
    std::memset(tileStorage, 0, sizeof(CVidTile));
    auto* tile = reinterpret_cast<CVidTile*>(tileStorage);

    // A 4x2 tile from a 6-wide data block (data jump 2) into an 8x2 surface.
    BYTE data[12] = { 1, 2, 3, 4, 99, 99, 5, 6, 7, 8, 99, 99 };
    std::vector<WORD> pixels(8 * 2, 0xAAAA);
    REQUIRE(tile->BltTile8To16(pixels.data(), 8 * 2, data, CSize(4, 2), 2, 0));
    CHECK(pixels == std::vector<WORD> { 0x0100, 0x0200, 0x0300, 0x0400, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0x0500, 0x0600, 0x0700, 0x0800, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA });

    std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0xAAAA));
    REQUIRE(tile->BltTile8To16(pixels.data(), 8 * 2, data, CSize(4, 2), 2, 0x8));
    CHECK(pixels == std::vector<WORD> { 0x0100, 0x5678, 0x0300, 0x5678, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0x5678, 0x0600 | 0x1234, 0x5678, 0x0800 | 0x1234, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA });

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(tileStorage);
    ::operator delete(static_cast<void*>(engine));
}

// DrawLine16 from (0,0) to (5,2): a run-sliced line (ScanLine16, 0x79A860)
// with whole step 2: runs of two pixels on each row. Vertical and
// horizontal lines are drawn inclusive of both ends.
TEST_CASE("draw_line16", "[vid]")
{
    CVidInf mode;
    const CRect rSurface(0, 0, kDepthW, kDepthH);
    std::vector<WORD> pixels(kDepthW * kDepthH, 0);
    REQUIRE(mode.DrawLine16(5, 2, 0, 0, pixels.data(), kDepthW, rSurface, 0x7777, TRUE));
    std::vector<WORD> expected(kDepthW * kDepthH, 0);
    for (const auto& [x, y] : std::vector<std::pair<int, int>> { { 0, 0 }, { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 } }) {
        expected[y * kDepthW + x] = 0x7777;
    }
    CHECK(pixels == expected);

    std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0));
    REQUIRE(mode.DrawLine16(6, 0, 6, 4, pixels.data(), kDepthW, rSurface, 0x1111, TRUE));
    REQUIRE(mode.DrawLine16(1, 4, 3, 4, pixels.data(), kDepthW, rSurface, 0x2222, TRUE));
    std::fill(expected.begin(), expected.end(), static_cast<WORD>(0));
    for (int y = 0; y <= 4; ++y) {
        expected[y * kDepthW + 6] = 0x1111;
    }
    for (int x = 1; x <= 3; ++x) {
        expected[4 * kDepthW + x] = 0x2222;
    }
    CHECK(pixels == expected);
}

// DrawEllipse24 draws the outline through the four ends of its axes, the
// same with the whole surface as the clip as without one, symmetric about
// the centre.
TEST_CASE("draw_ellipse24", "[vid]")
{
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    constexpr int kW = 24;
    constexpr int kH = 16;
    std::vector<BYTE> plain(kW * kH * 3, 0);
    std::vector<BYTE> clipped(kW * kH * 3, 0);
    DDSURFACEDESC ddsd = {};
    ddsd.lPitch = kW * 3;
    ddsd.lpSurface = plain.data();
    mode.DrawEllipse24(CPoint(11, 7), CSize(7, 4), ddsd, nullptr, RGB(0x10, 0x20, 0x30));
    ddsd.lpSurface = clipped.data();
    const CRect rWhole(0, 0, kW, kH);
    mode.DrawEllipse24(CPoint(11, 7), CSize(7, 4), ddsd, &rWhole, RGB(0x10, 0x20, 0x30));
    CHECK(plain == clipped);

    auto lit = [&](int x, int y) {
        const BYTE* p = plain.data() + (y * kW + x) * 3;
        return p[0] == 0x30 && p[1] == 0x20 && p[2] == 0x10;
    };
    CHECK(lit(4, 7));
    CHECK(lit(18, 7));
    CHECK(lit(11, 3));
    CHECK(lit(11, 11));
    CHECK_FALSE(lit(11, 7));
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int mx = 22 - x;
            const int my = 14 - y;
            if (mx >= 0 && mx < kW && my >= 0 && my < kH) {
                INFO("pixel " << x << "," << y);
                CHECK(lit(x, y) == lit(mx, my));
            }
        }
    }
}

// CVidMode::OutlinePoly (0x799540) locks the rectangle of surface 0 and
// draws each edge and the closing one with DrawLine32, the points moved by
// -(ptOffset) relative to the surface (the lock's origin is rSurface's top
// left). Compared with DrawLine32 drawing the same edges into a plain
// buffer; the triangle's top edge and left edge are checked by hand.
TEST_CASE("outline_poly32", "[vid]")
{
    ChitinScope scope;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
    desc.dwWidth = 16;
    desc.dwHeight = 12;
    IDirectDrawSurface* pSurface = NULL;
    REQUIRE(pDirectDraw->CreateSurface(&desc, &pSurface, NULL) == DD_OK);

    CVidInf mode;
    IDirectDrawSurface* surfaces[1] = { pSurface };
    mode.m_nSurfaces = 1;
    mode.pSurfaces = surfaces;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;

    auto read = [&]() {
        std::vector<DWORD> pixels(16 * 12);
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 16, 12)));
        for (int y = 0; y < 12; ++y) {
            std::memcpy(pixels.data() + y * 16, reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 16 * 4);
        }
        mode.UnLockSurface(0, ddsd.lpSurface);
        return pixels;
    };
    auto clear = [&]() {
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 16, 12)));
        for (int y = 0; y < 12; ++y) {
            std::memset(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 0, 16 * 4);
        }
        mode.UnLockSurface(0, ddsd.lpSurface);
    };

    const CPoint triangle[3] = { CPoint(3, 3), CPoint(11, 3), CPoint(3, 9) };
    const DWORD color = 0x00302010; // RGB(0x30, 0x20, 0x10) with red at bit 16
    for (int shifted = 0; shifted < 2; ++shifted) {
        INFO("shifted " << shifted);
        clear();
        // Shifted: the rectangle (2,1)-(16,12), whose top left the lock
        // returns, and offset (1,1) put the triangle 1 left and 1 up.
        const CRect rSurface = shifted != 0 ? CRect(2, 1, 16, 12) : CRect(0, 0, 16, 12);
        const CPoint ptOffset = shifted != 0 ? CPoint(1, 1) : CPoint(0, 0);
        REQUIRE(mode.OutlinePoly(triangle, 3, rSurface, RGB(0x30, 0x20, 0x10), ptOffset));
        const std::vector<DWORD> pixels = read();

        const int dx = shifted != 0 ? 1 : 0;
        const int dy = shifted != 0 ? 1 : 0;
        std::vector<DWORD> expected(16 * 12, 0);
        const CRect rWhole(0, 0, 16, 12);
        for (int i = 0; i < 3; ++i) {
            const CPoint& a = triangle[i];
            const CPoint& b = triangle[(i + 1) % 3];
            mode.DrawLine32(a.x - dx, a.y - dy, b.x - dx, b.y - dy, expected.data(), 16, rWhole, color, FALSE);
        }
        CHECK(pixels == expected);
        for (int x = 3; x <= 11; ++x) {
            CHECK(pixels[(3 - dy) * 16 + x - dx] == color);
        }
        for (int y = 3; y <= 9; ++y) {
            CHECK(pixels[(y - dy) * 16 + 3 - dx] == color);
        }
    }

    // No vertices: nothing is drawn.
    clear();
    CHECK_FALSE(mode.OutlinePoly(triangle, 0, CRect(0, 0, 16, 12), RGB(1, 2, 3), CPoint(0, 0)));
    CHECK(read() == std::vector<DWORD>(16 * 12, 0));

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    pSurface->Release();
    pDirectDraw->Release();
}

// CVidImage's constructor (0x7ACA60) builds the shared blend table on the
// first image: entry (k, m, i) at k * 0x800 + m * 0x20 + i is
// (m * (64 - 2i) + k * 2i) >> 6. The destructor (0x7ACC40) frees it with
// the last image.
TEST_CASE("vid_image_blend_table", "[vid]")
{
    const INT refsBefore = CVidImage::m_nBlendTableRefs;
    {
        CVidImage image;
        CHECK(CVidImage::m_nBlendTableRefs == refsBefore + 1);
        REQUIRE(CVidImage::m_pBlendTable != nullptr);
        for (int k : { 0, 1, 17, 63 }) {
            for (int m : { 0, 5, 40, 63 }) {
                for (int i : { 0, 1, 16, 31 }) {
                    INFO("k " << k << " m " << m << " i " << i);
                    CHECK(CVidImage::m_pBlendTable[k * 0x800 + m * 0x20 + i] == ((m * (64 - 2 * i) + k * 2 * i) >> 6));
                }
            }
        }
    }
    CHECK(CVidImage::m_nBlendTableRefs == refsBefore);
    if (refsBefore == 0) {
        CHECK(CVidImage::m_pBlendTable == nullptr);
    }
}

// CVidInf::FXBltToBack (0x79CC90) copies the FX rectangle to the back
// buffer at (x, y) less the reference point, clipped, skipping the colour
// key. Clipped on the left, the source starts that many pixels in and the
// destination at the clip's left (0x79CDD0): FX columns 2..7 land on x
// 4..9, not shifted left.
TEST_CASE("fx_blt_to_back_clips_left", "[vid]")
{
    ChitinScope scope;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    auto make = [&](DWORD w, DWORD h) {
        DDSURFACEDESC desc;
        std::memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
        desc.dwWidth = w;
        desc.dwHeight = h;
        IDirectDrawSurface* p = NULL;
        REQUIRE(pDirectDraw->CreateSurface(&desc, &p, NULL) == DD_OK);
        return p;
    };
    IDirectDrawSurface* pBack = make(16, 12);
    IDirectDrawSurface* pFx = make(16, 16);
    DDCOLORKEY key = { 0x0000FF00, 0x0000FF00 };
    pFx->SetColorKey(DDCKEY_SRCBLT, &key);

    CVidInf mode;
    IDirectDrawSurface* surfaces[9] = { pBack, NULL, pFx, NULL, NULL, NULL, NULL, NULL, NULL };
    mode.m_nSurfaces = 9;
    mode.pSurfaces = surfaces;

    // FX row y, column x holds 0x100 * y + x + 1; the back buffer is clear.
    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);
    REQUIRE(mode.LockSurface(2, &ddsd, CRect(0, 0, 16, 16)));
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch)[x] = 0x100 * y + x + 1;
        }
    }
    mode.UnLockSurface(2, ddsd.lpSurface);

    CRect rFX(0, 0, 8, 4);
    REQUIRE(mode.FXBltToBack(rFX, 6, 5, 4, 0, CRect(4, 0, 16, 12), 0));

    REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 16, 12)));
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 16; ++x) {
            const DWORD pixel = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch)[x];
            const bool inside = y >= 5 && y < 9 && x >= 4 && x < 10;
            INFO("x " << x << " y " << y);
            CHECK(pixel == (inside ? static_cast<DWORD>(0x100 * (y - 5) + (x - 2) + 1) : 0u));
        }
    }
    mode.UnLockSurface(0, ddsd.lpSurface);

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    pFx->Release();
    pBack->Release();
    pDirectDraw->Release();
}

// CVidMode::DrawPoints (0x7977D0), which CVidInf::FXRender (0x79D450) uses
// for particles: each point is a surface coordinate (lPitch in bytes);
// unless bClipped, a point outside rSurface is skipped.
TEST_CASE("draw_points32", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    CPoint points[4] = { CPoint(1, 1), CPoint(6, 2), CPoint(7, 3), CPoint(0, 0) };
    std::vector<DWORD> pixels(8 * 4, 0);
    REQUIRE(mode.DrawPoints(points, 4, reinterpret_cast<WORD*>(pixels.data()), 8 * 4, CRect(1, 1, 7, 4), RGB(1, 2, 3), FALSE));
    std::vector<DWORD> expected(8 * 4, 0);
    expected[1 * 8 + 1] = 0x00010203;
    expected[2 * 8 + 6] = 0x00010203;
    CHECK(pixels == expected);

    std::fill(pixels.begin(), pixels.end(), 0u);
    REQUIRE(mode.DrawPoints(points, 4, reinterpret_cast<WORD*>(pixels.data()), 8 * 4, CRect(1, 1, 7, 4), RGB(1, 2, 3), TRUE));
    expected[3 * 8 + 7] = 0x00010203;
    expected[0] = 0x00010203;
    CHECK(pixels == expected);
}

// CVidPoly::IsPtInPoly (0x7C0E40) on a two-point polygon tests the point
// against its one segment (the end standing in for the previous point).
TEST_CASE("is_pt_in_poly_two_points", "[vid]")
{
    const CPoint segment[2] = { CPoint(0, 0), CPoint(10, 10) };
    CHECK(CVidPoly::IsPtInPoly(segment, 2, CPoint(4, 4)));
    CHECK_FALSE(CVidPoly::IsPtInPoly(segment, 2, CPoint(4, 5)));
    const CPoint square[4] = { CPoint(0, 0), CPoint(10, 0), CPoint(10, 10), CPoint(0, 10) };
    CHECK(CVidPoly::IsPtInPoly(square, 4, CPoint(5, 5)));
    CHECK_FALSE(CVidPoly::IsPtInPoly(square, 4, CPoint(15, 5)));
}

namespace {

// Installs a zeroed CWarp holding mode as the active engine, so
// g_pChitin->GetCurrentVideoMode() returns mode (inside a ChitinScope).
class ActiveModeScope {
public:
    explicit ActiveModeScope(CVidMode* mode)
        : m_engine(reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp))))
    {
        std::memset(static_cast<void*>(m_engine), 0, sizeof(CWarp));
        m_engine->pVidMode = mode;
        g_pChitin->pActiveEngine = m_engine;
    }
    ~ActiveModeScope()
    {
        g_pChitin->pActiveEngine = nullptr;
        ::operator delete(static_cast<void*>(m_engine));
    }

    ActiveModeScope(const ActiveModeScope&) = delete;
    ActiveModeScope& operator=(const ActiveModeScope&) = delete;

private:
    CWarp* m_engine;
};

}

// DrawHLineShaded16 (0x7C1FD0) puts in each pixel the colour's low word
// scaled by the pixel's intensity / 4 (ScaleColor16 0x79AD20 of
// GetIntensity16 0x79AD90: the channels brought to 8 bits, green counted
// twice). 5-6-5 masks; dwColor's high word is ignored.
TEST_CASE("draw_hline_shaded16", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitMask = 0xF800;
    mode.m_dwGBitMask = 0x07E0;
    mode.m_dwBBitMask = 0x001F;
    mode.m_dwRBitShift = 11;
    mode.m_dwGBitShift = 5;
    mode.m_dwBBitShift = 0;
    mode.field_C2 = 3;
    mode.field_C6 = 2;
    mode.field_CA = 3;
    ActiveModeScope active(&mode);

    // 0x8410 is (16, 32, 16): intensity 128 + 2 * 128 + 128 = 512, scale 128.
    // 0xFFFF is (31, 63, 31): intensity 248 + 2 * 252 + 248 = 1000, scale 250.
    CHECK(mode.GetIntensity16(0x8410) == 512);
    CHECK(mode.GetIntensity16(0xABCDFFFF) == 1000);
    CHECK(mode.ScaleColor16(0xF81F, 128) == ((15u << 11) | 15u));

    CVidPoly poly;
    const DWORD color = 0xABCDF81F; // magenta (31, 0, 31)
    WORD row[4] = { 0x8410, 0xFFFF, 0x0000, 0x1234 };
    poly.DrawHLineShaded16(row, 0, 2, color, CRect(0, 0, 4, 1), CPoint(0, 0));
    CHECK(row[0] == ((15 << 11) | 15)); // 31 * 128 >> 8
    CHECK(row[1] == ((30 << 11) | 30)); // 31 * 250 >> 8
    CHECK(row[2] == 0x0000);
    CHECK(row[3] == 0x1234);

    // Mirrored: xMin 1 to xMax 2 are the pixels at width - 1 and width - 2.
    WORD mirrored[6] = { 0x1111, 0x1111, 0x8410, 0xFFFF, 0x1111, 0x1111 };
    poly.DrawHLineShadedMirrored16(mirrored, 1, 2, color, CRect(0, 0, 4, 1), CPoint(0, 0));
    CHECK(mirrored[0] == 0x1111);
    CHECK(mirrored[1] == 0x1111);
    CHECK(mirrored[2] == ((15 << 11) | 15));
    CHECK(mirrored[3] == ((30 << 11) | 30));
    CHECK(mirrored[4] == 0x1111);
    CHECK(mirrored[5] == 0x1111);
}

// DrawHLineShaded24 (0x7D6CA0) blends toward the colour as the 32-bit one
// does, but reads only each pixel's first byte v and takes v, v + 1 and
// v + 2 for its three channels (0x7D6D1F); the blend goes to the bytes in
// the order colour bits 16-23, 8-15, 0-7.
TEST_CASE("draw_hline_shaded24", "[vid]")
{
    CVidPoly poly;
    // dwColor bytes 0x1E, 0x3C, 0x5A: intensity 60, inverse 195.
    const DWORD color = 0x005A3C1E;
    auto blend = [](int c, int p) { return static_cast<BYTE>((c * 60 + 195 * p) >> 8); };
    BYTE row[5 * 3] = {
        0x10, 0x99, 0x77, // v 16: sum 51, 51 / 8 = 6 <= 60: blended
        0x05, 0x01, 0x02, // v 5: sum 18, too dark: kept
        0xF0, 0x01, 0x02, // v 240: sum 723, 90 > 60: kept
        0x80, 0x00, 0x00, // v 128: sum 387, 48 <= 60: blended
        0x10, 0x10, 0x10, // outside the span
    };
    poly.DrawHLineShaded24(row, 0, 3, color, CRect(0, 0, 5, 1), CPoint(0, 0));
    const BYTE expected[5 * 3] = {
        blend(0x5A, 16),
        blend(0x3C, 17),
        blend(0x1E, 18),
        0x05,
        0x01,
        0x02,
        0xF0,
        0x01,
        0x02,
        blend(0x5A, 128),
        blend(0x3C, 129),
        blend(0x1E, 130),
        0x10,
        0x10,
        0x10,
    };
    CHECK(std::vector<BYTE>(row, row + 15) == std::vector<BYTE>(expected, expected + 15));

    // Mirrored: xMin 1 to xMax 2 are the pixels at width - 1 and width - 2.
    BYTE mirrored[6 * 3];
    std::memset(mirrored, 0x80, sizeof(mirrored));
    poly.DrawHLineShadedMirrored24(mirrored, 1, 2, color, CRect(0, 0, 4, 1), CPoint(0, 0));
    for (int pixel = 0; pixel < 6; ++pixel) {
        INFO("pixel " << pixel);
        const BYTE* p = mirrored + pixel * 3;
        if (pixel == 2 || pixel == 3) {
            CHECK(p[0] == blend(0x5A, 128));
            CHECK(p[1] == blend(0x3C, 129));
            CHECK(p[2] == blend(0x1E, 130));
        } else {
            CHECK((p[0] == 0x80 && p[1] == 0x80 && p[2] == 0x80));
        }
    }
}

// The cell's sequence checks compare unsigned (GetCurrentCenterPoint
// 0x7AD5AC, GetCurrentFrameSize 0x7AD88C, IsEndOfSequence 0x7AE084,
// GetFrame 0x7B09CA): a negative current sequence counts as past the end,
// so the frame comes from sequence 0 (GetFrame resets it to 0) and
// IsEndOfSequence answers FALSE. Two sequences of one frame each, 4x3
// centred (1, 2) and 7x5 centred (3, 4).
TEST_CASE("cell_negative_sequence_is_past_the_end", "[vid]")
{
    BAMHEADER header = {};
    header.nSequences = 2;
    SEQUENCEENTRY sequences[2] = { { 1, 0 }, { 1, 1 } };
    WORD frameList[2] = { 0, 1 };
    FRAMEENTRY frames[2] = { { 4, 3, 1, 2, 0 }, { 7, 5, 3, 4, 0 } };

    auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(resStorage, 0, sizeof(CResCell));
    auto* res = reinterpret_cast<CResCell*>(resStorage);
    res->m_bParsed = TRUE;
    res->m_pBamHeader = &header;
    res->m_pSequences = sequences;
    res->m_pFrameList = frameList;
    res->m_pFrames = frames;
    auto* cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(cellStorage, 0, sizeof(CVidCell));
    auto* cell = reinterpret_cast<CVidCell*>(cellStorage);
    static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
    cell->field_C8 = 1;

    for (const SHORT sequence : { SHORT(-1), SHORT(-2), SHORT(2), SHORT(1) }) {
        INFO("sequence " << sequence);
        const bool inRange = sequence == 1;
        cell->m_nCurrentSequence = sequence;
        cell->m_nCurrentFrame = 0;
        CSize size;
        REQUIRE(cell->GetCurrentFrameSize(size, TRUE));
        CHECK(size.cx == (inRange ? 7 : 4));
        CHECK(size.cy == (inRange ? 5 : 3));
        CPoint center;
        REQUIRE(cell->GetCurrentCenterPoint(center, TRUE));
        CHECK(center.x == (inRange ? 3 : 1));
        CHECK(center.y == (inRange ? 4 : 2));
        CHECK(cell->IsEndOfSequence(TRUE) == (inRange ? TRUE : FALSE));
        REQUIRE(cell->CVidCell::GetFrame(TRUE));
        CHECK(cell->m_nCurrentSequence == (inRange ? 1 : 0));
        CHECK(cell->m_pFrame == &frames[inRange ? 1 : 0]);
    }

    ::operator delete(cellStorage);
    ::operator delete(resStorage);
}

// DrawHLine16 (0x7C1AA0) writes an odd first pixel of dwColor's low word,
// then pixel pairs as the DWORD dwColor | dwColor << 16; the mirrored one
// (0x7C1AF0) walks left from width - xMin, the pair's high word landing on
// its higher-addressed pixel. A colour with bits above 15 shows the pairs.
TEST_CASE("draw_hline16_pairs", "[vid]")
{
    CVidPoly poly;
    const DWORD color = 0xABCD1234; // pair 0xBBFD1234
    std::vector<WORD> row(8, 0x1111);
    poly.DrawHLine16(row.data(), 1, 3, color, CRect(0, 0, 8, 1), CPoint(0, 0));
    CHECK(row == std::vector<WORD> { 0x1111, 0x1234, 0x1234, 0xBBFD, 0x1111, 0x1111, 0x1111, 0x1111 });

    std::fill(row.begin(), row.end(), static_cast<WORD>(0x1111));
    poly.DrawHLine16(row.data(), 2, 5, color, CRect(0, 0, 8, 1), CPoint(0, 0));
    CHECK(row == std::vector<WORD> { 0x1111, 0x1111, 0x1234, 0xBBFD, 0x1234, 0xBBFD, 0x1111, 0x1111 });

    // Mirrored, width 7: xMin 1 to 3 are pixels 6 (odd), 4 and 5 (a pair).
    std::fill(row.begin(), row.end(), static_cast<WORD>(0x1111));
    poly.DrawHLineMirrored16(row.data(), 1, 3, color, CRect(0, 0, 7, 1), CPoint(0, 0));
    CHECK(row == std::vector<WORD> { 0x1111, 0x1111, 0x1111, 0x1111, 0x1234, 0xBBFD, 0x1234, 0x1111 });

    // An empty span draws nothing.
    std::fill(row.begin(), row.end(), static_cast<WORD>(0x1111));
    poly.DrawHLine16(row.data(), 4, 3, color, CRect(0, 0, 8, 1), CPoint(0, 0));
    poly.DrawHLineMirrored16(row.data(), 4, 3, color, CRect(0, 0, 7, 1), CPoint(0, 0));
    CHECK(row == std::vector<WORD>(8, 0x1111));
}

// CVidInf::RenderFlash (0x79F520) fills rClip of surface nSurface with the
// colour in the surface's format (DDBLT_COLORFILL with the DDBLTFX); the
// other surfaces are left alone.
TEST_CASE("render_flash_fills_its_surface", "[vid]")
{
    ChitinScope scope;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
    desc.dwWidth = 8;
    desc.dwHeight = 6;
    IDirectDrawSurface* surfaces[2] = { NULL, NULL };
    REQUIRE(pDirectDraw->CreateSurface(&desc, &surfaces[0], NULL) == DD_OK);
    REQUIRE(pDirectDraw->CreateSurface(&desc, &surfaces[1], NULL) == DD_OK);

    CVidInf mode;
    mode.m_nSurfaces = 2;
    mode.pSurfaces = surfaces;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;

    auto read = [&](UINT nSurface) {
        std::vector<DWORD> pixels(8 * 6);
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(nSurface, &ddsd, CRect(0, 0, 8, 6)));
        for (int y = 0; y < 6; ++y) {
            std::memcpy(pixels.data() + y * 8, reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 8 * 4);
        }
        mode.UnLockSurface(nSurface, ddsd.lpSurface);
        return pixels;
    };
    for (UINT nSurface = 0; nSurface < 2; ++nSurface) {
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(nSurface, &ddsd, CRect(0, 0, 8, 6)));
        for (int y = 0; y < 6; ++y) {
            std::memset(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 0, 8 * 4);
        }
        mode.UnLockSurface(nSurface, ddsd.lpSurface);
    }

    mode.RenderFlash(1, RGB(0x10, 0x20, 0x30), 0, CRect(1, 1, 5, 4));
    std::vector<DWORD> expected(8 * 6, 0);
    for (int y = 1; y < 4; ++y) {
        for (int x = 1; x < 5; ++x) {
            expected[y * 8 + x] = 0x00102030;
        }
    }
    CHECK(read(1) == expected);
    CHECK(read(0) == std::vector<DWORD>(8 * 6, 0));

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    surfaces[1]->Release();
    surfaces[0]->Release();
    pDirectDraw->Release();
}

// DrawLine24 (0x7CA0D0) draws 3-byte pixels (the colour's low word, then
// its third byte), lPitch in bytes; a run-sliced line goes through
// ScanLine24 (0x7CA240) as DrawLine16 through ScanLine16.
TEST_CASE("draw_line24", "[vid]")
{
    CVidInf mode;
    const CRect rSurface(0, 0, kDepthW, kDepthH);
    std::vector<BYTE> pixels(kDepthW * kDepthH * 3, 0);
    const DWORD color = 0x00ABCDEF;
    REQUIRE(mode.DrawLine24(5, 2, 0, 0, pixels.data(), kDepthW * 3, rSurface, color, TRUE));
    std::vector<BYTE> expected(kDepthW * kDepthH * 3, 0);
    auto put = [&](int x, int y) {
        BYTE* p = expected.data() + (y * kDepthW + x) * 3;
        p[0] = 0xEF;
        p[1] = 0xCD;
        p[2] = 0xAB;
    };
    for (const auto& [x, y] : std::vector<std::pair<int, int>> { { 0, 0 }, { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 } }) {
        put(x, y);
    }
    CHECK(pixels == expected);

    // Clipped by the function: a vertical line from above the surface.
    std::fill(pixels.begin(), pixels.end(), static_cast<BYTE>(0));
    std::fill(expected.begin(), expected.end(), static_cast<BYTE>(0));
    REQUIRE(mode.DrawLine24(6, -3, 6, 2, pixels.data(), kDepthW * 3, rSurface, color, FALSE));
    for (int y = 0; y <= 2; ++y) {
        put(6, y);
    }
    CHECK(pixels == expected);
}

// DrawThickLine16 (0x799280) and DrawThickLine24 (0x7CA330) draw a
// horizontal line a8 + 1 rows thick, starting half the thickness above and
// to the left of the first point.
TEST_CASE("draw_thick_line16_24", "[vid]")
{
    CVidInf mode;
    constexpr int kW = 12;
    constexpr int kH = 10;
    CRect rSurface(0, 0, kW, kH);

    std::vector<WORD> pixels16(kW * kH, 0);
    REQUIRE(mode.DrawThickLine16(3, 5, 7, 5, pixels16.data(), kW, rSurface, 2, 0x1234, TRUE));
    std::vector<WORD> expected16(kW * kH, 0);
    // a8 2: three rows from y 4, each starting at x 2 and one pixel later
    // per row (the original's row step), five pixels long.
    for (int row = 0; row < 3; ++row) {
        for (int x = 2 + row; x <= 6 + row; ++x) {
            expected16[(4 + row) * kW + x] = 0x1234;
        }
    }
    CHECK(pixels16 == expected16);

    std::vector<BYTE> pixels24(kW * kH * 3, 0);
    REQUIRE(mode.DrawThickLine24(3, 5, 7, 5, pixels24.data(), kW * 3, rSurface, 2, 0x00ABCDEF, TRUE));
    std::vector<BYTE> expected24(kW * kH * 3, 0);
    for (int i = 0; i < kW * kH; ++i) {
        if (expected16[i] != 0) {
            expected24[i * 3 + 0] = 0xEF;
            expected24[i * 3 + 1] = 0xCD;
            expected24[i * 3 + 2] = 0xAB;
        }
    }
    CHECK(pixels24 == expected24);
}

// DrawEllipse16 (0x796260) draws the outline through the four ends of its
// axes with the 16-bit colour (5-6-5 here), the same with the whole surface
// as the clip as without one, symmetric about the centre.
TEST_CASE("draw_ellipse16", "[vid]")
{
    CVidInf mode;
    mode.m_dwRBitShift = 11;
    mode.m_dwGBitShift = 5;
    mode.m_dwBBitShift = 0;
    mode.field_C2 = 3;
    mode.field_C6 = 2;
    mode.field_CA = 3;
    constexpr int kW = 24;
    constexpr int kH = 16;
    std::vector<WORD> plain(kW * kH, 0);
    std::vector<WORD> clipped(kW * kH, 0);
    DDSURFACEDESC ddsd = {};
    ddsd.lPitch = kW * 2;
    ddsd.lpSurface = plain.data();
    mode.DrawEllipse16(CPoint(11, 7), CSize(7, 4), ddsd, nullptr, RGB(0xF8, 0xFC, 0xF8));
    ddsd.lpSurface = clipped.data();
    const CRect rWhole(0, 0, kW, kH);
    mode.DrawEllipse16(CPoint(11, 7), CSize(7, 4), ddsd, &rWhole, RGB(0xF8, 0xFC, 0xF8));
    CHECK(plain == clipped);

    auto lit = [&](int x, int y) { return plain[y * kW + x] == 0xFFFF; };
    CHECK(lit(4, 7));
    CHECK(lit(18, 7));
    CHECK(lit(11, 3));
    CHECK(lit(11, 11));
    CHECK_FALSE(lit(11, 7));
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int mx = 22 - x;
            const int my = 14 - y;
            if (mx >= 0 && mx < kW && my >= 0 && my < kH) {
                INFO("pixel " << x << "," << y);
                CHECK(lit(x, y) == lit(mx, my));
            }
        }
    }
}

// DrawRecticle16 (0x7989B0) and DrawRecticle24 (0x7C9B10) draw the same
// pixels as DrawRecticle32 (0x7CB670) in their own formats.
TEST_CASE("draw_recticle16_24", "[vid]")
{
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    constexpr int kW = 64;
    constexpr int kH = 48;
    CVIDMODE_RECTICLE_DESCRIPTION desc = {};
    desc.ptCenter = CPoint(30, 22);
    desc.xAxis = 14;
    desc.yAxis = 9;
    desc.xGap = 3;
    desc.yGap = 2;
    desc.piePieceXOffset = 2;
    desc.piePieceYOffset = 1;
    desc.piePiecePtXOffset = 3;
    desc.piePiecePtYOffset = 2;
    const CRect rClip(5, 4, 50, 40);

    for (int withClip = 0; withClip < 2; ++withClip) {
        INFO("clip " << withClip);
        const CRect* pClip = withClip != 0 ? &rClip : nullptr;
        std::vector<DWORD> p32(kW * kH, 0);
        DDSURFACEDESC ddsd = {};
        ddsd.lPitch = kW * 4;
        ddsd.lpSurface = p32.data();
        mode.DrawRecticle32(desc, ddsd, pClip, RGB(0xF8, 0xFC, 0xF8));

        std::vector<BYTE> p24(kW * kH * 3, 0);
        ddsd.lPitch = kW * 3;
        ddsd.lpSurface = p24.data();
        mode.DrawRecticle24(desc, ddsd, pClip, RGB(0xF8, 0xFC, 0xF8));

        std::vector<WORD> p16(kW * kH, 0);
        ddsd.lPitch = kW * 2;
        ddsd.lpSurface = p16.data();
        mode.m_dwRBitShift = 11;
        mode.m_dwGBitShift = 5;
        mode.field_C2 = 3;
        mode.field_C6 = 2;
        mode.field_CA = 3;
        mode.DrawRecticle16(desc, ddsd, pClip, RGB(0xF8, 0xFC, 0xF8));
        mode.m_dwRBitShift = 16;
        mode.m_dwGBitShift = 8;
        mode.field_C2 = 0;
        mode.field_C6 = 0;
        mode.field_CA = 0;

        int drawn = 0;
        for (int i = 0; i < kW * kH; ++i) {
            const bool lit32 = p32[i] == 0x00F8FCF8;
            drawn += lit32 ? 1 : 0;
            INFO("pixel " << i);
            CHECK(lit32 == (p16[i] == 0xFFFF));
            CHECK(lit32 == (p24[i * 3] == 0xF8 && p24[i * 3 + 1] == 0xFC && p24[i * 3 + 2] == 0xF8));
        }
        CHECK(drawn > 40);
    }
}

// BltTile8To24 (0x7CDD00) writes each pixel as its palette entry's three low
// bytes, four at a time (a width not a multiple of four draws nothing).
// Dithered (0x8), even rows keep pixels 0 and 2 and odd rows 1 and 3, the
// others field_24, and the rows follow on from each other (no row jump, as
// the original). BltStencilTile8To16 (0x7B2440) puts field_24 where the
// stencil is clear in the pixel the row's parity drops.
TEST_CASE("blt_tile8to24_and_stencil16", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x00ABCDEF;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;
    for (int i = 0; i < 256; ++i) {
        CVidImage::rgbTempPal[i] = 0xEE000000u | static_cast<DWORD>(i) * 0x010101u;
    }
    auto* tileStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidTile)));
    std::memset(tileStorage, 0, sizeof(CVidTile));
    auto* tile = reinterpret_cast<CVidTile*>(tileStorage);

    BYTE data[12] = { 1, 2, 3, 4, 99, 99, 5, 6, 7, 8, 99, 99 };
    // An 8-pixel-wide 24-bit surface: 24 bytes a row.
    std::vector<BYTE> pixels(24 * 2, 0x55);
    REQUIRE(tile->BltTile8To24(reinterpret_cast<DWORD*>(pixels.data()), 24, data, CSize(4, 2), 2, 0));
    std::vector<BYTE> expected(24 * 2, 0x55);
    const BYTE rows[2][4] = { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } };
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            std::memset(expected.data() + y * 24 + x * 3, rows[y][x], 3);
        }
    }
    CHECK(pixels == expected);
    CHECK_FALSE(tile->BltTile8To24(reinterpret_cast<DWORD*>(pixels.data()), 24, data, CSize(3, 2), 2, 0));

    std::fill(pixels.begin(), pixels.end(), static_cast<BYTE>(0x55));
    REQUIRE(tile->BltTile8To24(reinterpret_cast<DWORD*>(pixels.data()), 24, data, CSize(4, 2), 2, 0x8));
    std::fill(expected.begin(), expected.end(), static_cast<BYTE>(0x55));
    // Row 0: data 1, key, data 3, key; row 1 straight after it, from data
    // index 4: key, data[5] (99), key, data[7] (6).
    const BYTE key[3] = { 0xEF, 0xCD, 0xAB };
    auto put = [&](int pixel, int index) { std::memset(expected.data() + pixel * 3, index, 3); };
    auto putKey = [&](int pixel) { std::memcpy(expected.data() + pixel * 3, key, 3); };
    put(0, 1);
    putKey(1);
    put(2, 3);
    putKey(3);
    putKey(4);
    put(5, 99);
    putKey(6);
    put(7, 6);
    CHECK(pixels == expected);

    BYTE stencil[12] = { 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0 };
    std::vector<WORD> words(8 * 2, 0x5555);
    REQUIRE(tile->BltStencilTile8To16(words.data(), 8 * 2, data, stencil, CSize(4, 2), 2, 0));
    // Row 0 (even): the second of each pair is field_24's low word where
    // its stencil is clear (index 1). Row 1 (odd): the first is the whole
    // field_24 ORed in where its stencil is clear (index 6), its high word
    // reaching the second pixel.
    CHECK(words == std::vector<WORD> { 0x0101, 0xCDEF, 0x0303, 0x0404, 0x5555, 0x5555, 0x5555, 0x5555, 0xCDEF, 0x0606 | 0x00AB, 0x0707, 0x0808, 0x5555, 0x5555, 0x5555, 0x5555 });
    CHECK_FALSE(tile->BltStencilTile8To16(words.data(), 8 * 2, data, stencil, CSize(3, 2), 2, 0));

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(tileStorage);
    ::operator delete(static_cast<void*>(engine));
}

// BltBmp8To24 (0x7CCB20) realizes the bitmap's colour table and writes each
// pixel's three low bytes, the rows going up from the destination (the
// bitmap's last row); BltBmp24To24 (0x7CCBF0) skips pure green and halves
// what is under black with 0x5, a drawn pixel's fourth byte landing on the
// next pixel's first (zero with shifts 16/8/0).
TEST_CASE("blt_bmp8to24_and_24to24", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    ActiveModeScope active(&mode);

    RGBQUAD colors[256];
    for (int i = 0; i < 256; ++i) {
        colors[i] = { static_cast<BYTE>(i), static_cast<BYTE>(255 - i), static_cast<BYTE>(i ^ 0x5A), 0 };
    }
    auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResBitmap)));
    std::memset(resStorage, 0, sizeof(CResBitmap));
    auto* res = reinterpret_cast<CResBitmap*>(resStorage);
    res->bParsed = TRUE;
    res->nColors = 256;
    res->pColorTable = colors;
    auto* bitmapStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(bitmapStorage, 0, sizeof(CVidBitmap));
    auto* bitmap = reinterpret_cast<CVidBitmap*>(bitmapStorage);
    static_cast<CResHelper<CResBitmap, 1>*>(bitmap)->pRes = res;

    // A 3x2 bitmap with a data jump of 1, into a 4-pixel-wide surface (12
    // bytes a row), drawn up from row 1.
    BYTE data[8] = { 10, 20, 30, 99, 40, 50, 60, 99 };
    std::vector<BYTE> surface(12 * 2, 0x77);
    REQUIRE(bitmap->BltBmp8To24(surface.data() + 12, 12, data, CSize(3, 2), 1, 0));
    std::vector<BYTE> expected(12 * 2, 0x77);
    auto put = [&](int row, int x, DWORD rgb) {
        BYTE* p = expected.data() + row * 12 + x * 3;
        p[0] = static_cast<BYTE>(rgb);
        p[1] = static_cast<BYTE>(rgb >> 8);
        p[2] = static_cast<BYTE>(rgb >> 16);
    };
    for (int x = 0; x < 3; ++x) {
        put(1, x, CVidImage::rgbTempPal[data[x]]);
        put(0, x, CVidImage::rgbTempPal[data[4 + x]]);
    }
    CHECK(surface == expected);

    // 24 to 24, keyed with shadows: pure green, black, a colour; one row.
    BYTE rgb[9] = { 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33 };
    std::vector<BYTE> row = { 0x40, 0x41, 0x42, 0x80, 0x82, 0x84, 0x90, 0x91, 0x92, 0x55, 0x55, 0x55 };
    REQUIRE(bitmap->BltBmp24To24(row.data(), 12, rgb, CSize(3, 1), 0, 0x5));
    // Pixel 0 skipped; pixel 1 halved (0x80 0x82 0x84 -> 0x40 0x41 0x42), its
    // fourth byte (0) on pixel 2's first; pixel 2 the colour (B 0x11, G
    // 0x22, R 0x33 as bytes 0, 1, 2), last of the row so three bytes.
    CHECK(row == std::vector<BYTE> { 0x40, 0x41, 0x42, 0x40, 0x41, 0x42, 0x11, 0x22, 0x33, 0x55, 0x55, 0x55 });

    ::operator delete(bitmapStorage);
    ::operator delete(resStorage);
}

// Regression tests for the render stream's fixes on the 32-bit path the game
// reaches (render-report.md section 4).
namespace {

// A resource whose data is in memory. CDimm::Demand (0x783D00) takes its
// loaded path (RES_FLAG_0x04 and RES_FLAG_0x10, no resource file) and
// returns m_pData; CRes::field_40, which CDimm::Demand raises and
// CDimm::Release (0x787BA0) lowers, is its demand count (GetDemands).
// field_44, a pending request, keeps the last release off CDimm's freed
// list. Empty bytes leave m_pData NULL, so a demand finds no data. The
// destructor takes the data back before ~CRes, whose Dump would free() it;
// it must run while a ChitinScope is installed.
template <typename T>
struct MemoryRes {
    explicit MemoryRes(std::vector<BYTE> bytes)
        : data(std::move(bytes))
    {
        res.m_pData = data.empty() ? nullptr : data.data();
        res.nSize = static_cast<DWORD>(data.size());
        res.dwFlags = CRes::RES_FLAG_0x04 | CRes::RES_FLAG_0x10;
        res.m_nID = 0xFC000000;
        res.field_44 = 1;
    }
    ~MemoryRes()
    {
        res.m_pData = nullptr;
        res.dwFlags = 0;
        res.field_40 = 0;
        res.field_44 = 0;
    }
    MemoryRes(const MemoryRes&) = delete;
    MemoryRes& operator=(const MemoryRes&) = delete;

    std::vector<BYTE> data;
    T res;
};

// A TIS tile's data: a grey palette, then 64 x 64 palette indexes.
std::vector<BYTE> TileBytes(BYTE index)
{
    std::vector<BYTE> bytes(1024 + 64 * 64, index);
    for (int i = 0; i < 256; ++i) {
        bytes[i * 4] = static_cast<BYTE>(i);
        bytes[i * 4 + 1] = static_cast<BYTE>(i);
        bytes[i * 4 + 2] = static_cast<BYTE>(i);
        bytes[i * 4 + 3] = 0;
    }
    return bytes;
}

// A w x h system-memory surface of the DirectDraw shim.
IDirectDrawSurface* MakeSurface(IDirectDraw* pDirectDraw, DWORD w, DWORD h)
{
    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
    desc.dwWidth = w;
    desc.dwHeight = h;
    IDirectDrawSurface* p = NULL;
    REQUIRE(pDirectDraw->CreateSurface(&desc, &p, NULL) == DD_OK);
    return p;
}

}

// CVidTile::Render with a stencil (0x7B1B30) demands both tiles (0x7B1BA8,
// 0x7B1BB6) before it reads either, and every way out after that gives both
// demands back: the clipped-away tile, the failed lock and the drawn tile.
// The tiles here are held by their owner (one demand each, parsed), as the
// tile set holds them. Ours released the stencil without demanding it, so
// each render dropped the owner's demand and, at 0, CResTile::Release
// (0x7807D0) cleared the stencil's parsed data: the drawn tile leaves the
// stencil's count at 0 and its data gone, which fails here (checked by
// removing the stencil's Demand).
//
// Missing tile or stencil data fails an assert (lines 6344 and 6351). Its
// tail (the original returns without releasing for the tile, releases both
// for the stencil) cannot run in iwd2r: CUtil::UtilAssert never returns. The
// test's chitin throws from ShutDown, so it checks the counts at the assert:
// both tiles demanded once more, neither released. Ours used to reach either
// assert with the stencil not demanded.
TEST_CASE("stencil_tile_render_balances_its_demands", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    ActiveModeScope active(&mode);
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    IDirectDrawSurface* pSurface = MakeSurface(pDirectDraw, 64, 64);
    IDirectDrawSurface* pSmall = MakeSurface(pDirectDraw, 16, 16);

    {
        MemoryRes<CResTile> main(TileBytes(5));
        MemoryRes<CResTile> stencil(TileBytes(1));
        REQUIRE(main.res.Demand() != nullptr);
        REQUIRE(stencil.res.Demand() != nullptr);
        auto held = [&](const char* step) {
            INFO(step);
            CHECK(main.res.GetDemands() == 1);
            CHECK(stencil.res.GetDemands() == 1);
            CHECK(main.res.GetTileData() != nullptr);
            CHECK(stencil.res.GetTileData() != nullptr);
        };

        CVidTile tile;
        tile.SetRes(&main.res);

        // Drawn: the blit's result.
        CHECK(tile.Render(pSurface, 0, 0, &stencil.res, CRect(0, 0, 64, 64), 0) == TRUE);
        held("drawn");
        CHECK(tile.Render(pSurface, 0, 0, &stencil.res, CRect(0, 0, 64, 64), 0) == TRUE);
        held("drawn twice");

        // An empty clip returns before either demand.
        CHECK(tile.Render(pSurface, 0, 0, &stencil.res, CRect(), 0) == FALSE);
        held("empty clip");

        // A tile wholly outside the clip: its rectangle clips to nothing.
        CHECK(tile.Render(pSurface, 100, 100, &stencil.res, CRect(0, 0, 32, 32), 0) == FALSE);
        held("clipped away");

        // The tile's rectangle runs past the 16 x 16 surface: Lock fails.
        CHECK(tile.Render(pSmall, 0, 0, &stencil.res, CRect(0, 0, 64, 64), 0) == FALSE);
        held("lock failed");

        // No tile data: the assert fires with both demanded.
        MemoryRes<CResTile> noTile(std::vector<BYTE> {});
        tile.SetRes(&noTile.res);
        CHECK_THROWS_AS(tile.Render(pSurface, 0, 0, &stencil.res, CRect(0, 0, 64, 64), 0), FailedAssert);
        CHECK(noTile.res.GetDemands() == 1);
        CHECK(stencil.res.GetDemands() == 2);
        stencil.res.Release();
        held("no tile data, the stencil's demand given back by the test");

        // No stencil data: likewise.
        MemoryRes<CResTile> noStencil(std::vector<BYTE> {});
        tile.SetRes(&main.res);
        CHECK_THROWS_AS(tile.Render(pSurface, 0, 0, &noStencil.res, CRect(0, 0, 64, 64), 0), FailedAssert);
        CHECK(main.res.GetDemands() == 2);
        CHECK(noStencil.res.GetDemands() == 1);
        main.res.Release();
        held("no stencil data, the tile's demand given back by the test");
    }

    pSmall->Release();
    pSurface->Release();
    pDirectDraw->Release();
}

// CVidCell::BltFromFX (0x7B06E0) copies the cell's frame from the FX surface
// to the destination, leaving the destination where the FX surface holds the
// colour key. Mirrored (dwFlags 0x10 left-right, 0x20 up-down) it goes
// through Blt with DDBLT_WAIT | DDBLT_KEYSRC | DDBLT_DDFX (push 0x1008800 at
// 0x7B090F); ours left out DDBLT_KEYSRC, which copied the key over the
// destination (fails here for 0x10, 0x20 and 0x30, checked by removing the
// flag). Unmirrored it is a keyed BltFast. A 4 x 2 frame at (0, 0); the
// mirrored destination rectangle is (destX, destY, width, height), as the
// original's, so it starts at the origin here.
TEST_CASE("cell_blt_from_fx_keeps_the_key_mirrored", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    IDirectDrawSurface* pBack = MakeSurface(pDirectDraw, 8, 4);
    IDirectDrawSurface* pFx = MakeSurface(pDirectDraw, 8, 4);
    constexpr DWORD kKey = 0x0000FF00;
    constexpr DWORD kBack = 0x00ABCDEF;
    DDCOLORKEY key = { kKey, kKey };
    REQUIRE(pFx->SetColorKey(DDCKEY_SRCBLT, &key) == DD_OK);
    IDirectDrawSurface* surfaces[3] = { pBack, NULL, pFx };
    mode.m_nSurfaces = 3;
    mode.pSurfaces = surfaces;
    ActiveModeScope active(&mode);

    // The frame in the FX surface: key pixels between colours.
    const DWORD frame[2][4] = {
        { 0x00000011, kKey, 0x00000033, kKey },
        { kKey, 0x00000022, kKey, 0x00000044 },
    };
    auto fill = [&](UINT nSurface, auto&& pixel) {
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(nSurface, &ddsd, CRect(0, 0, 8, 4)));
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 8; ++x) {
                reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch)[x] = pixel(x, y);
            }
        }
        mode.UnLockSurface(nSurface, ddsd.lpSurface);
    };
    fill(2, [&](int x, int y) { return x < 4 && y < 2 ? frame[y][x] : 0x00777777u; });

    CVidCell cell;
    FRAMEENTRY entry = {};
    entry.nWidth = 4;
    entry.nHeight = 2;
    cell.m_pFrame = &entry;
    cell.m_nFxSurface = 2;

    for (const DWORD dwFlags : { 0x0u, 0x10u, 0x20u, 0x30u }) {
        INFO("dwFlags 0x" << std::hex << dwFlags);
        fill(0, [&](int, int) { return kBack; });
        CHECK(cell.BltFromFX(pBack, 0, 0, CRect(0, 0, 8, 4), dwFlags));

        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 8, 4)));
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 8; ++x) {
                DWORD expected = kBack;
                if (x < 4 && y < 2) {
                    const DWORD source = frame[(dwFlags & 0x20) != 0 ? 1 - y : y][(dwFlags & 0x10) != 0 ? 3 - x : x];
                    if (source != kKey) {
                        expected = source;
                    }
                }
                INFO("x " << x << " y " << y);
                CHECK(reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch)[x] == expected);
            }
        }
        mode.UnLockSurface(0, ddsd.lpSurface);
    }

    cell.m_pFrame = NULL;
    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    pFx->Release();
    pBack->Release();
    pDirectDraw->Release();
}

// CVidBitmap::RenderDirect (0x7B3190) draws a 24-bit bitmap on a 32-bit
// surface with BltBmp24To32Tint when any flag of the high word is set
// (dwFlags & 0xFFFF0000, tested before each ApplyBrightnessContrast), or
// when the brightness and contrast move mid grey. With them neutral, ours
// tinted only for the latter, so the greyscale (0x80000) and tint (0x20000)
// flags were dropped and the pixels copied as they are (fails here, checked
// by removing the flag test). Keyed (0x1): each pixel is written with alpha
// 0xFF. A 2 x 1 bitmap (row padded to 8 bytes) drawn at (1, 0).
TEST_CASE("render_direct_tints_24bit_on_high_word_flags", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    IDirectDrawSurface* pSurface = MakeSurface(pDirectDraw, 4, 2);
    IDirectDrawSurface* surfaces[1] = { pSurface };
    mode.m_nSurfaces = 1;
    mode.pSurfaces = surfaces;
    ActiveModeScope active(&mode);

    BITMAPINFOHEADER info;
    std::memset(&info, 0, sizeof(info));
    info.biWidth = 2;
    info.biHeight = 1;
    info.biBitCount = 24;
    // B, G, R per pixel: (R 0x60, G 0x30, B 0x90) and (R 0x10, G 0x20, B 0x30).
    BYTE data[8] = { 0x90, 0x30, 0x60, 0x30, 0x20, 0x10, 0, 0 };
    auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResBitmap)));
    std::memset(resStorage, 0, sizeof(CResBitmap));
    auto* res = reinterpret_cast<CResBitmap*>(resStorage);
    res->bParsed = TRUE;
    res->pBitmapInfoHeader = &info;
    res->pBitmapData = data;
    auto* bitmapStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(bitmapStorage, 0, sizeof(CVidBitmap));
    auto* bitmap = reinterpret_cast<CVidBitmap*>(bitmapStorage);
    static_cast<CResHelper<CResBitmap, 1>*>(bitmap)->pRes = res;
    bitmap->m_paletteAffects.rgbTintColor = RGB(0x80, 0x40, 0xFF);

    auto pixel = [](DWORD r, DWORD g, DWORD b) { return 0xFF000000u | (r << 16) | (g << 8) | b; };
    struct Case {
        DWORD dwFlags;
        DWORD first;
        DWORD second;
    };
    const Case cases[] = {
        // No high-word flag: copied.
        { 0x1, pixel(0x60, 0x30, 0x90), pixel(0x10, 0x20, 0x30) },
        // Greyscale: (r + g + b) >> 2 in each channel.
        { 0x80001, pixel(0x48, 0x48, 0x48), pixel(0x18, 0x18, 0x18) },
        // Tint by rgbTintColor: c * t >> 8.
        { 0x20001, pixel(0x30, 0x0C, 0x8F), pixel(0x08, 0x08, 0x2F) },
    };
    for (const Case& c : cases) {
        INFO("dwFlags 0x" << std::hex << c.dwFlags);
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 4, 2)));
        for (int y = 0; y < 2; ++y) {
            std::memset(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 0, 4 * 4);
        }
        mode.UnLockSurface(0, ddsd.lpSurface);

        CHECK(bitmap->RenderDirect(0, 1, 0, CRect(0, 0, 4, 2), c.dwFlags, TRUE));

        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 4, 2)));
        const DWORD* row = reinterpret_cast<const DWORD*>(ddsd.lpSurface);
        const std::vector<DWORD> drawn(row, row + 4);
        const DWORD* below = reinterpret_cast<const DWORD*>(reinterpret_cast<const BYTE*>(ddsd.lpSurface) + ddsd.lPitch);
        const std::vector<DWORD> under(below, below + 4);
        mode.UnLockSurface(0, ddsd.lpSurface);
        CHECK(drawn == std::vector<DWORD> { 0, c.first, c.second, 0 });
        CHECK(under == std::vector<DWORD>(4, 0u));
    }

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    ::operator delete(bitmapStorage);
    ::operator delete(resStorage);
    pSurface->Release();
    pDirectDraw->Release();
}

namespace {

// A surface that only hands out device contexts, counting them. The shim's
// surfaces have none, so CVidFont::TextOutEx stops at GetDC on them.
class DcSurface final : public IDirectDrawSurface {
public:
    int getDCs = 0;
    int releaseDCs = 0;

    ULONG AddRef() override { return 1; }
    ULONG Release() override { return 1; }
    HRESULT Lock(LPRECT, LPDDSURFACEDESC, DWORD, HANDLE) override { return DDERR_UNSUPPORTED; }
    HRESULT Unlock(LPVOID) override { return DDERR_UNSUPPORTED; }
    HRESULT Blt(LPRECT, IDirectDrawSurface*, LPRECT, DWORD, LPDDBLTFX) override { return DDERR_UNSUPPORTED; }
    HRESULT BltFast(DWORD, DWORD, IDirectDrawSurface*, LPRECT, DWORD) override { return DDERR_UNSUPPORTED; }
    HRESULT Flip(IDirectDrawSurface*, DWORD) override { return DDERR_UNSUPPORTED; }
    HRESULT GetAttachedSurface(LPDDSCAPS, IDirectDrawSurface**) override { return DDERR_UNSUPPORTED; }
    HRESULT SetColorKey(DWORD, LPDDCOLORKEY) override { return DDERR_UNSUPPORTED; }
    HRESULT GetColorKey(DWORD, LPDDCOLORKEY) override { return DDERR_UNSUPPORTED; }
    HRESULT GetPixelFormat(LPDDPIXELFORMAT) override { return DDERR_UNSUPPORTED; }
    HRESULT SetClipper(IDirectDrawClipper*) override { return DDERR_UNSUPPORTED; }
    HRESULT SetPalette(IDirectDrawPalette*) override { return DDERR_UNSUPPORTED; }
    HRESULT GetPalette(IDirectDrawPalette**) override { return DDERR_UNSUPPORTED; }
    HRESULT GetDC(HDC* lphDC) override
    {
        ++getDCs;
        *lphDC = reinterpret_cast<HDC>(this);
        return DD_OK;
    }
    HRESULT ReleaseDC(HDC) override
    {
        ++releaseDCs;
        return DD_OK;
    }
    HRESULT Restore() override { return DD_OK; }
    HRESULT IsLost() override { return DD_OK; }
};

// A BAM of one sequence of one 5 x 7 frame: the header, the frame entry, the
// sequence entry, the frame list and a palette.
std::vector<BYTE> OneFrameBam()
{
    std::vector<BYTE> bytes(0x2C + 1024, 0);
    BAMHEADER header = {};
    std::memcpy(&header.nFileType, "BAM ", 4);
    std::memcpy(&header.nFileVersion, "V1  ", 4);
    header.nFrames = 1;
    header.nSequences = 1;
    header.nTableOffset = 0x18;
    header.nFrameListOffset = 0x28;
    header.nPaletteOffset = 0x2C;
    std::memcpy(bytes.data(), &header, sizeof(header));
    FRAMEENTRY frame = { 5, 7, 0, 0, 0x80000000 };
    std::memcpy(bytes.data() + 0x18, &frame, sizeof(frame));
    SEQUENCEENTRY sequence = { 1, 0 };
    std::memcpy(bytes.data() + 0x24, &sequence, sizeof(sequence));
    return bytes;
}

// A font that draws white on black, so TextOutEx reads no resource name for
// its colour, and whose TrueType font counts as set.
void PrepareFont(CVidFont& font)
{
    font.m_bFontSet = TRUE;
    font.m_rgbForegroundColor = RGB(255, 255, 255);
    font.m_rgbBackgroundColor = RGB(0, 0, 0);
}

}

// CVidFont::TextOutEx (0x793A70), the TrueType text path, returns TRUE once
// the text is out; ours returned FALSE, which BKTextOut and FXTextOut pass
// on. It measures the baseline with the caller's bDemanded (0x793CF5); ours
// passed TRUE. With no TrueType font named, the baseline is the font BAM's
// first frame height. That BAM starts parsed with no demand, a state only a
// demand and its release change (CResCell::Release clears the parse at 0):
// with bDemanded FALSE the text demands the BAM and gives it back, with TRUE
// it leaves it alone. Both fail on the old code (checked by restoring it).
TEST_CASE("font_text_out_ex_returns_true_and_passes_bdemanded", "[vid]")
{
    ChitinScope scope;
    g_pChitin->m_sFontName = "";
    CVidInf mode;
    DcSurface dc;
    IDirectDrawSurface* surfaces[1] = { &dc };
    mode.m_nSurfaces = 1;
    mode.pSurfaces = surfaces;
    ActiveModeScope active(&mode);

    {
        MemoryRes<CResCell> bam(OneFrameBam());
        bam.res.m_bCacheHeader = FALSE;
        CVidFont font;
        PrepareFont(font);
        static_cast<CResHelper<CResCell, 1000>&>(font).pRes = &bam.res;

        for (const BOOL bDemanded : { TRUE, FALSE }) {
            INFO("bDemanded " << bDemanded);
            REQUIRE(bam.res.Parse(bam.data.data()));
            dc.getDCs = 0;
            dc.releaseDCs = 0;
            CHECK(font.TextOutEx(0, CString("text"), 2, 20, CRect(0, 0, 64, 32), 0, bDemanded) == TRUE);
            CHECK(dc.getDCs == 1);
            CHECK(dc.releaseDCs == 1);
            CHECK(bam.res.GetDemands() == 0);
            CHECK(bam.res.m_bParsed == bDemanded);
        }
        static_cast<CResHelper<CResCell, 1000>&>(font).pRes = NULL;
    }

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
}

// CVidInf::FXTextOut (0x79D970) returns FALSE when the mode has no FX
// surface (GetFXSurface at 0x79DA7B) and draws nothing. Ours passed
// GetFXSurface's -1 on to TextOutEx, which took the surface pointer before
// the array; a guard sits in that slot here, so the old code draws on it and
// returns TRUE (checked by restoring it). With an FX surface the text goes
// there and TextOutEx's TRUE comes back.
TEST_CASE("fx_text_out_without_fx_surface_returns_false", "[vid]")
{
    ChitinScope scope;
    g_pChitin->m_sFontName = "Arial";
    g_pChitin->m_bUseMirrorFX = FALSE;
    CVidInf mode;
    DcSurface guard;
    DcSurface fx;
    IDirectDrawSurface* slots[1 + 9] = { &guard };
    mode.m_nSurfaces = 9;
    mode.pSurfaces = slots + 1;
    std::vector<DWORD> locked(4 * 4, 0);
    mode.m_SurfaceDesc.lpSurface = locked.data();
    mode.m_SurfaceDesc.lPitch = 4 * 4;
    ActiveModeScope active(&mode);

    CVidFont font;
    PrepareFont(font);
    font.m_nBaseLineHeight = 10;

    CHECK(mode.FXTextOut(&font, CString("fx"), 1, 12, CRect(0, 0, 32, 16), 0, TRUE) == FALSE);
    CHECK(guard.getDCs == 0);

    slots[1 + CVIDINF_SURFACE_2] = &fx;
    CHECK(mode.FXTextOut(&font, CString("fx"), 1, 12, CRect(0, 0, 32, 16), 0, TRUE) == TRUE);
    CHECK(fx.getDCs == 1);
    CHECK(fx.releaseDCs == 1);
    CHECK(guard.getDCs == 0);

    mode.m_SurfaceDesc.lpSurface = NULL;
    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
}

// CVidInf::BKRender(CParticle*) (0x79E060) returns TRUE after drawing the
// particle into the locked back buffer (ours fell through to FALSE, checked
// by restoring it), and FALSE when nothing is locked. A dot at (3, 3):
// x = posX >> 10, y = (3 * posY / 4 - posZ) >> 10.
TEST_CASE("bk_render_particle_returns_true", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    ActiveModeScope active(&mode);
    std::vector<DWORD> pixels(8 * 8, 0);
    mode.m_SurfaceDesc.lpSurface = pixels.data();
    mode.m_SurfaceDesc.lPitch = 8 * 4;

    CParticle particle(3 << 10, 4 << 10, 0, 0, 0, 0, RGB(1, 2, 3), 0, 0, CParticle::BLOB);
    CHECK(mode.BKRender(&particle, CRect(0, 0, 8, 8), CParticle::DOTS, 1) == TRUE);
    std::vector<DWORD> expected(8 * 8, 0);
    expected[3 * 8 + 3] = 0x00010203;
    CHECK(pixels == expected);

    mode.m_SurfaceDesc.lpSurface = NULL;
    CHECK(mode.BKRender(&particle, CRect(0, 0, 8, 8), CParticle::DOTS, 1) == FALSE);
}

// CParticle::Render's 16- and 24-bpp arms (0x7C36A0): the same walk as the
// 32-bit arms with the drawers' pitch units, the inlined dot writes and the
// two new blob helpers (0x7C3CC0, 0x7C3E20), whose plus grows with nBlobSize
// from the centre out (1 centre, 2 + down, 3 + left, 4 + right, 5 + up), each
// point clipped by the rect moved to its own origin.
TEST_CASE("particle_render_16_and_24", "[vid]")
{
    ChitinScope scope;
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    ActiveModeScope active(&mode);

    const COLORREF rgb = RGB(0x11, 0x22, 0x33);
    const DWORD color = mode.ReduceColor(rgb);
    const WORD color16 = static_cast<WORD>(color);
    const BYTE nThird = static_cast<BYTE>(color >> 16);

    for (const int bpp : { 16, 24, 32 }) {
        INFO("bpp " << bpp);
        g_pChitin->cVideo.m_nBpp = static_cast<WORD>(bpp);

        // DOTS: a particle at (3, 3) (y = (3 * posY / 4 - posZ) >> 10), one
        // loop, inside the clip at (1, 1): 16 bpp writes the WORD at
        // (y - top) * (lPitch >> 1) - left + x, 24 bpp three bytes at
        // (x - left) * 3 + (y - top) * lPitch.
        {
            CParticle particle(3 << 10, 4 << 10, 0, 0, 0, 0, rgb, 0, 0, CParticle::DOTS);
            if (bpp == 16) {
                std::vector<WORD> pixels(8 * 8, 0xAAAA);
                particle.Render(pixels.data(), 8 * 2, CRect(1, 1, 8, 8), CParticle::DOTS, 1);
                std::vector<WORD> expected(8 * 8, 0xAAAA);
                expected[(3 - 1) * 8 - 1 + 3] = color16;
                CHECK(pixels == expected);
            } else if (bpp == 24) {
                std::vector<BYTE> pixels(8 * 8 * 3, 0xAA);
                particle.Render(pixels.data(), 8 * 3, CRect(1, 1, 8, 8), CParticle::DOTS, 1);
                std::vector<BYTE> expected(8 * 8 * 3, 0xAA);
                BYTE* p = expected.data() + ((3 - 1) * 8 - 1 + 3) * 3;
                std::memcpy(p, &color16, 2);
                p[2] = nThird;
                CHECK(pixels == expected);
            } else {
                std::vector<DWORD> pixels(8 * 8, 0xAAAAAAAA);
                particle.Render(pixels.data(), 8 * 4, CRect(1, 1, 8, 8), CParticle::DOTS, 1);
                std::vector<DWORD> expected(8 * 8, 0xAAAAAAAA);
                expected[(3 - 1) * 8 - 1 + 3] = color;
                CHECK(pixels == expected);
            }
        }

        // BLOB: the plus at (3, 3) of the clip's own origin; each size adds
        // one point to the previous one's set.
        const CPoint blobPoints[] = { CPoint(3, 3), CPoint(3, 2), CPoint(2, 3), CPoint(4, 3), CPoint(3, 4) };
        for (USHORT nBlobSize = 1; nBlobSize <= 5; nBlobSize++) {
            INFO("blob size " << nBlobSize);
            CParticle particle(3 << 10, 4 << 10, 0, 0, 0, 0, rgb, 0, 0, CParticle::BLOB);
            if (bpp == 16) {
                std::vector<WORD> pixels(8 * 8, 0xAAAA);
                particle.Render(pixels.data(), 8 * 2, CRect(0, 0, 8, 8), CParticle::BLOB, nBlobSize);
                std::vector<WORD> expected(8 * 8, 0xAAAA);
                // Sizes 5,4,3,2,1 fall through to the smaller sets, so a size
                // draws the points of every case from itself down to 1.
                for (USHORT j = 1; j <= nBlobSize; j++) {
                    const CPoint& pt = blobPoints[j - 1];
                    expected[pt.y * 8 + pt.x] = color16;
                }
                CHECK(pixels == expected);
            } else if (bpp == 32) {
                std::vector<DWORD> pixels(8 * 8, 0xAAAAAAAA);
                particle.Render(pixels.data(), 8 * 4, CRect(0, 0, 8, 8), CParticle::BLOB, nBlobSize);
                std::vector<DWORD> expected(8 * 8, 0xAAAAAAAA);
                // The 32-bit sibling has no size switch: the five-point cross.
                for (const CPoint& pt : blobPoints) {
                    expected[pt.y * 8 + pt.x] = color;
                }
                CHECK(pixels == expected);
            } else {
                std::vector<BYTE> pixels(8 * 8 * 3, 0xAA);
                particle.Render(pixels.data(), 8 * 3, CRect(0, 0, 8, 8), CParticle::BLOB, nBlobSize);
                std::vector<BYTE> expected(8 * 8 * 3, 0xAA);
                for (USHORT j = 1; j <= nBlobSize; j++) {
                    const CPoint& pt = blobPoints[j - 1];
                    BYTE* p = expected.data() + (pt.y * 8 + pt.x) * 3;
                    std::memcpy(p, &color16, 2);
                    p[2] = nThird;
                }
                CHECK(pixels == expected);
            }
        }

        // BLOB at the clip's origin: the left, down and centre points at x or
        // y -1 fall outside and leave the destination.
        {
            CParticle particle(0, 0, 0, 0, 0, 0, rgb, 0, 0, CParticle::BLOB);
            if (bpp == 16) {
                std::vector<WORD> pixels(4 * 4, 0xAAAA);
                particle.Render(pixels.data(), 4 * 2, CRect(0, 0, 4, 4), CParticle::BLOB, 5);
                std::vector<WORD> expected(4 * 4, 0xAAAA);
                expected[0] = color16; // the centre
                expected[0 + 1] = color16; // right
                expected[1 * 4] = color16; // up
                CHECK(pixels == expected);
            } else if (bpp == 24) {
                std::vector<BYTE> pixels(4 * 4 * 3, 0xAA);
                particle.Render(pixels.data(), 4 * 3, CRect(0, 0, 4, 4), CParticle::BLOB, 5);
                std::vector<BYTE> expected(4 * 4 * 3, 0xAA);
                for (int i : { 0, 1, 4 }) {
                    BYTE* p = expected.data() + i * 3;
                    std::memcpy(p, &color16, 2);
                    p[2] = nThird;
                }
                CHECK(pixels == expected);
            } else {
                std::vector<DWORD> pixels(4 * 4, 0xAAAAAAAA);
                particle.Render(pixels.data(), 4 * 4, CRect(0, 0, 4, 4), CParticle::BLOB, 5);
                std::vector<DWORD> expected(4 * 4, 0xAAAAAAAA);
                for (int i : { 0, 1, 4 }) {
                    expected[i] = color;
                }
                CHECK(pixels == expected);
            }
        }

        // CONNECT: the line the arm hands to the mode's drawer, from
        // (pos - vel) to pos, clipped by the rect moved in one on its far
        // edges. The drawers are oracle-proven; this pins the wiring and the
        // pitch unit.
        {
            CParticle particle(5 << 10, 5 << 10, 0, 2 << 10, 4 << 10, 0, rgb, 0, 0, CParticle::CONNECT);
            const CRect clip(0, 0, 8, 8);
            const CRect drawClip(0, 0, 7, 7);
            if (bpp == 16) {
                std::vector<WORD> pixels(8 * 8, 0xAAAA);
                particle.Render(pixels.data(), 8 * 2, clip, CParticle::CONNECT, 1);
                std::vector<WORD> expected(8 * 8, 0xAAAA);
                // x1 = (5 - 2), y1 = (5 - 4) * 3 / 4, x2 = 5, y2 = 5 * 3 / 4.
                mode.DrawLine16(3, 0, 5, 3, expected.data(), 8, drawClip, color16, FALSE);
                CHECK(pixels == expected);
            } else if (bpp == 24) {
                std::vector<BYTE> pixels(8 * 8 * 3, 0xAA);
                particle.Render(pixels.data(), 8 * 3, clip, CParticle::CONNECT, 1);
                std::vector<BYTE> expected(8 * 8 * 3, 0xAA);
                mode.DrawLine24(3, 0, 5, 3, expected.data(), 8 * 3, drawClip, color, FALSE);
                CHECK(pixels == expected);
            } else {
                std::vector<DWORD> pixels(8 * 8, 0xAAAAAAAA);
                particle.Render(pixels.data(), 8 * 4, clip, CParticle::CONNECT, 1);
                std::vector<DWORD> expected(8 * 8, 0xAAAAAAAA);
                mode.DrawLine32(3, 0, 5, 3, expected.data(), 8, drawClip, color, FALSE);
                CHECK(pixels == expected);
            }
        }
    }

    g_pChitin->cVideo.m_nBpp = 32;
}

// CVisibilityMap::GetSubTileCode (0x5525C0) puts the corners that are not
// visible in the high nibble (`shl bl, 4` at 0x552668) and the unexplored
// ones in the low nibble: bit 0x1 the tile, 0x2 the one below, 0x4 below
// right, 0x8 right; a tile off the map counts as neither. Ours shifted by 8,
// which dropped the visibility bits from the BYTE, so the fog never showed
// what the party does not see.
TEST_CASE("visibility_sub_tile_code_keeps_the_unseen_corners", "[vid]")
{
    // GetSubTileCode reads only the map and its size; the constructor would
    // need the current video mode (sub_5518A0).
    alignas(CVisibilityMap) unsigned char storage[sizeof(CVisibilityMap)];
    std::memset(storage, 0, sizeof(storage));
    auto* map = reinterpret_cast<CVisibilityMap*>(storage);
    // 4x2 tiles: 0x80 explored, 0x3F the characters that see the tile.
    BYTE tiles[8] = {
        0x81,
        0x80,
        0x80,
        0x82,
        0x00,
        0x82,
        0x81,
        0x00,
    };
    map->m_pMap = tiles;
    map->m_nWidth = 4;
    map->m_nHeight = 2;
    map->m_nMapSize = 8;

    // Tile 0: seen; below (4) unseen and unexplored; below right (5) seen;
    // right (1) explored but unseen.
    CHECK(map->GetSubTileCode(0) == 0xA2);
    // Tile 2: itself explored but unseen; below right (7) neither seen nor
    // explored; below (6) and right (3) both.
    CHECK(map->GetSubTileCode(2) == 0x54);
    // Tile 3: below right (8) is off the map, and its right is tile 4, the
    // next row's first (nothing stops at the row's end).
    CHECK(map->GetSubTileCode(3) == 0xEE);
    // Every corner seen and explored.
    tiles[0] = tiles[1] = tiles[4] = tiles[5] = 0x81;
    CHECK(map->GetSubTileCode(0) == 0x00);
}

namespace {

// One of BltFogOWar's (0x551FF0) or BltDitherPattern's (0x551A90) blits:
// FOGOWAR's tile nTileRect (GetFogOWarTileRect: 0/4 top left, 1/5 top
// right, 2/6 bottom left, 3/7 bottom right of its 64x64 surface) from
// surface nSurface, mirrored as dwDDFX says, keyed on the surface's colour
// key.
struct FogBlit {
    BYTE nTileRect;
    int nSurface;
    DWORD dwDDFX;
};

}

// CVisibilityMap::BltFogOWar (0x551FF0) draws the fog of war into a 64x64
// tile surface, per 32x32 quadrant of the tile code: first BltDitherPattern
// (0x551A90) dithers the corners that are not visible (high nibble) unless
// the same corners are unexplored, then the unexplored corners (low nibble)
// get FOGOWAR's black edges, or a black fill when the whole quadrant is
// unexplored. The shapes, rectangles, surfaces and mirrors are the
// disassembly's (jump tables 0x551FB0 and 0x552558, rectangle ids at
// 0x85DE43-0x85DE4A). Ours did nothing, so no fog was ever drawn.
//
// The blits move whole 32-bit pixels: a keyed source pixel leaves the
// destination's X byte (a scalable-text tag) alone and a drawn one replaces
// it, and the black fill writes X 0, as any engine write does.
TEST_CASE("fog_of_war_blits_the_dither_and_the_edges", "[vid]")
{
    ChitinScope scope;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
    desc.dwWidth = 64;
    desc.dwHeight = 64;
    IDirectDrawSurface* surfaces[9] = {};
    for (IDirectDrawSurface*& surface : surfaces) {
        REQUIRE(pDirectDraw->CreateSurface(&desc, &surface, NULL) == DD_OK);
    }
    IDirectDrawSurface* target = surfaces[8];

    CVidInf mode;
    mode.m_nSurfaces = 8;
    mode.pSurfaces = surfaces;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    ActiveModeScope active(&mode);

    constexpr DWORD kKey = 0x0000FF00;
    constexpr DWORD kBackground = 0x55A0B0C0; // X 0x55, a text tag
    auto write = [](IDirectDrawSurface* surface, const std::vector<DWORD>& pixels) {
        DDSURFACEDESC ddsd;
        REQUIRE(surface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL) == DD_OK);
        for (int y = 0; y < 64; ++y) {
            std::memcpy(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, pixels.data() + y * 64, 64 * 4);
        }
        surface->Unlock(ddsd.lpSurface);
    };
    auto read = [](IDirectDrawSurface* surface) {
        std::vector<DWORD> pixels(64 * 64);
        DDSURFACEDESC ddsd;
        REQUIRE(surface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL) == DD_OK);
        for (int y = 0; y < 64; ++y) {
            std::memcpy(pixels.data() + y * 64, reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 64 * 4);
        }
        surface->Unlock(ddsd.lpSurface);
        return pixels;
    };

    // Surfaces 6 and 7 hold FOGOWAR's frames as LoadFogOWarSurfaces
    // (0x79E550) lays them out; here each pixel names its surface and
    // position, and a third of them are the colour key.
    std::vector<DWORD> fog[2];
    for (int nSurface = 6; nSurface <= 7; ++nSurface) {
        std::vector<DWORD>& pixels = fog[nSurface - 6];
        pixels.resize(64 * 64);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                pixels[y * 64 + x] = (x + 2 * y) % 3 == 0
                    ? kKey
                    : 0xFF000000 | (static_cast<DWORD>(nSurface) << 16) | (static_cast<DWORD>(y) << 8) | static_cast<DWORD>(x);
            }
        }
        write(surfaces[nSurface], pixels);
        DDCOLORKEY colorKey = { kKey, kKey };
        REQUIRE(surfaces[nSurface]->SetColorKey(DDCKEY_SRCBLT, &colorKey) == DD_OK);
    }

    constexpr DWORD kBoth = DDBLTFX_MIRRORLEFTRIGHT | DDBLTFX_MIRRORUPDOWN;
    // By the high nibble (BltDitherPattern), then the low (BltFogOWar).
    const std::pair<BYTE, FogBlit> dither[] = {
        { 0x3, { 5, 7, 0 } },
        { 0x6, { 4, 7, kBoth } },
        { 0x7, { 7, 7, kBoth } },
        { 0x9, { 4, 7, 0 } },
        { 0xB, { 6, 7, 0 } },
        { 0xC, { 5, 7, kBoth } },
        { 0xD, { 7, 7, 0 } },
        { 0xE, { 6, 7, kBoth } },
        { 0xF, { 3, 6, 0 } },
    };
    const std::pair<BYTE, FogBlit> edges[] = {
        { 0x3, { 1, 6, 0 } },
        { 0x6, { 0, 6, kBoth } },
        { 0x7, { 2, 6, DDBLTFX_MIRRORUPDOWN } },
        { 0x9, { 0, 6, 0 } },
        { 0xB, { 2, 6, 0 } },
        { 0xC, { 1, 6, kBoth } },
        { 0xD, { 2, 6, DDBLTFX_MIRRORLEFTRIGHT } },
        { 0xE, { 2, 6, kBoth } },
    };
    auto find = [](const auto& table, BYTE code) -> const FogBlit* {
        for (const auto& [nCode, blit] : table) {
            if (nCode == code) {
                return &blit;
            }
        }
        return nullptr;
    };
    auto apply = [&](std::vector<DWORD>& pixels, int ox, int oy, const FogBlit& blit) {
        const std::vector<DWORD>& src = fog[blit.nSurface - 6];
        int sx = (blit.nTileRect & 1) != 0 ? 32 : 0;
        int sy = (blit.nTileRect & 2) != 0 ? 32 : 0;
        for (int v = 0; v < 32; ++v) {
            for (int u = 0; u < 32; ++u) {
                int x = (blit.dwDDFX & DDBLTFX_MIRRORLEFTRIGHT) != 0 ? 31 - u : u;
                int y = (blit.dwDDFX & DDBLTFX_MIRRORUPDOWN) != 0 ? 31 - v : v;
                DWORD pixel = src[(sy + y) * 64 + sx + x];
                if (pixel != kKey) {
                    pixels[(oy + v) * 64 + ox + u] = pixel;
                }
            }
        }
    };

    const int offsets[4][2] = { { 0, 0 }, { 32, 0 }, { 0, 32 }, { 32, 32 } };
    for (int code = 0; code < 256; ++code) {
        BYTE codes[4] = {
            static_cast<BYTE>(code),
            static_cast<BYTE>(code + 37),
            static_cast<BYTE>(code + 101),
            static_cast<BYTE>(code + 211),
        };
        TILE_CODE tileCode = { codes[0], codes[1], codes[2], codes[3] };

        std::vector<DWORD> expected(64 * 64, kBackground);
        for (int q = 0; q < 4; ++q) {
            BYTE nUnseen = codes[q] >> 4;
            BYTE nUnexplored = codes[q] & 0xF;
            if (const FogBlit* blit = find(dither, nUnseen); blit != nullptr && nUnseen != nUnexplored) {
                apply(expected, offsets[q][0], offsets[q][1], *blit);
            }
        }
        for (int q = 0; q < 4; ++q) {
            BYTE nUnexplored = codes[q] & 0xF;
            if (const FogBlit* blit = find(edges, nUnexplored); blit != nullptr) {
                apply(expected, offsets[q][0], offsets[q][1], *blit);
            } else if (nUnexplored == 0xF) {
                for (int v = 0; v < 32; ++v) {
                    for (int u = 0; u < 32; ++u) {
                        expected[(offsets[q][1] + v) * 64 + offsets[q][0] + u] = 0;
                    }
                }
            }
        }

        write(target, std::vector<DWORD>(64 * 64, kBackground));
        CVisibilityMap::BltFogOWar(target, tileCode);
        INFO("tile code " << code);
        CHECK(read(target) == expected);
    }

    // The tags: a quadrant explored and seen keeps them, an unexplored one
    // loses them to the black, and keyed pixels of an edge keep them.
    write(target, std::vector<DWORD>(64 * 64, kBackground));
    CVisibilityMap::BltFogOWar(target, TILE_CODE { 0x00, 0xFF, 0x09, 0x00 });
    std::vector<DWORD> pixels = read(target);
    CHECK(pixels[0] == kBackground);
    CHECK(pixels[5 * 64 + 40] == 0);
    CHECK(pixels[32 * 64 + 0] == kBackground); // (0, 0) of FOGOWAR's frame 0 is the key
    CHECK(pixels[32 * 64 + 1] == 0xFF060001);

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    for (IDirectDrawSurface* surface : surfaces) {
        surface->Release();
    }
    pDirectDraw->Release();
}

// CInfinity::SetDawn (0x5D1890) and SetDusk (0x5D1A40), in an outdoor
// area (type 0x2), set the time-of-day light (+0x1B6) and the time-of-day
// rain colour (+0x1BA) from the doubled intensity n (a byte). Each value
// is read here from the disassembly's constants (0x5D193D-0x5D1A26 and
// 0x5D1B10-0x5D1BE4). From intensity 128 on, ours stored the rain colour
// into the light, so a dawn or dusk past its midpoint (the game starts in
// one) lit the world at about 110 grey: the walk_and_talk world shot was at
// 0.42 of the original's brightness.
TEST_CASE("infinity_dawn_and_dusk_keep_the_light_and_the_rain_apart", "[vid]")
{
    ChitinScope scope;
    auto pInfinity = std::make_unique<CInfinity>();
    pInfinity->m_areaType = 0x2;

    for (int intensity : { 0, 60, 127, 128, 150, 200, 255 }) {
        INFO("intensity " << intensity);
        const int n = (intensity * 2) & 0xFF;
        const auto rain = [&](int base, int baseBlue) { return RGB(base + (n * 0x14 >> 8), base + (n * 0x14 >> 8), baseBlue + (n * 10 >> 8)); };

        pInfinity->SetDawn(static_cast<BYTE>(intensity), FALSE);
        CHECK(pInfinity->m_dayLightIntensity == intensity);
        if (intensity < 128) {
            CHECK(pInfinity->m_rgbTimeOfDayGlobalLighting == RGB(0x64 + (n * 0x50 >> 8), 0x64 + (n * 100 >> 8), 0xC8 + (n * 0x37 >> 8)));
            CHECK(pInfinity->m_rgbTimeOfDayRainColor == rain(0x46, 0x5A));
        } else {
            CHECK(pInfinity->m_rgbTimeOfDayGlobalLighting == RGB(0xB4 + (n * 0x4B >> 8), 0xC8 + (n * 0x37 >> 8), 0xFF));
            CHECK(pInfinity->m_rgbTimeOfDayRainColor == rain(0x5A, 0x64));
        }

        pInfinity->SetDusk(static_cast<BYTE>(intensity), FALSE);
        CHECK(pInfinity->m_dayLightIntensity == intensity);
        if (intensity < 128) {
            CHECK(pInfinity->m_rgbTimeOfDayGlobalLighting == RGB(0x64 + (n * 0x9B >> 8), 0x64 + (n * 100 >> 8), 0xC8));
            CHECK(pInfinity->m_rgbTimeOfDayRainColor == rain(0x46, 0x5A));
        } else {
            CHECK(pInfinity->m_rgbTimeOfDayGlobalLighting == RGB(0xFF, 0xC8 + (n * 0x37 >> 8), 0xC8 + (n * 0x37 >> 8)));
            CHECK(pInfinity->m_rgbTimeOfDayRainColor == rain(0x5A, 0x64));
        }
    }
}

// CVidMode::DrawLightingEffect24 (0x7C8F70): a light pool on a 24-bpp
// surface, three bytes per pixel (R, G, B) with lPitch in bytes. No surface
// and no GPU: a plain buffer is enough, and the function reads only the
// global tint (and the arc list through the mode's vtable).
TEST_CASE("vid: DrawLightingEffect24 brightens inside the pool and leaves the rest", "[vid]")
{
    constexpr int kCanvas = 64;
    constexpr int kPitch = kCanvas * 3;
    constexpr BYTE kGrey = 0x80;

    CVidInf mode;
    mode.rgbGlobalTint = RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT);

    // The white light on a black ambient clamps every channel at 255 in the
    // middle of the pool; the edges only brighten part of the way.
    std::vector<BYTE> canvas(static_cast<size_t>(kPitch) * kCanvas + 4, kGrey);
    const std::vector<BYTE> before = canvas;
    mode.DrawLightingEffect24(canvas.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0xFFFFFF, 0x000000);

    const auto pixel = [&](int x, int y) { return &canvas[static_cast<size_t>(y) * kPitch + x * 3]; };
    // The middle band ramps up to x = 31 and down from x = 32.
    CHECK(pixel(31, 32)[0] == 0xFF);
    CHECK(pixel(31, 32)[1] == 0xFF);
    CHECK(pixel(31, 32)[2] == 0xFF);
    CHECK(pixel(24, 32)[0] > kGrey);
    CHECK(pixel(38, 32)[0] > kGrey);
    // Everything outside the pool is byte-identical, including the four
    // canary bytes after the buffer (the 24-bit write is three bytes).
    CHECK(pixel(0, 0)[0] == kGrey);
    CHECK(std::memcmp(pixel(32 + 12, 32), &before[static_cast<size_t>(32) * kPitch + 44 * 3], 3) == 0);
    CHECK(std::memcmp(pixel(32, 32 + 12), &before[static_cast<size_t>(44) * kPitch + 32 * 3], 3) == 0);
    CHECK(canvas[static_cast<size_t>(kPitch) * kCanvas] == kGrey);
    CHECK(canvas[static_cast<size_t>(kPitch) * kCanvas + 3] == kGrey);

    // Height 0 draws nothing (the original leaks the pixel list there), and
    // so does a light whose channels all end at zero (no light, a white
    // ambient): the buffer stays byte-identical.
    {
        std::vector<BYTE> flat = before;
        mode.DrawLightingEffect24(flat.data(), kPitch, CPoint(32, 32), CSize(8, 0), 0xFFFFFF, 0x000000);
        CHECK(flat == before);
    }
    {
        std::vector<BYTE> dark = before;
        mode.DrawLightingEffect24(dark.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0x000000, 0xFFFFFF);
        CHECK(dark == before);
    }

    // A global tint other than none scales the ambient first: a black tint
    // zeroes it, and the same white light still fills the middle of the pool.
    mode.rgbGlobalTint = 0x000000;
    std::vector<BYTE> tinted = before;
    mode.DrawLightingEffect24(tinted.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0xFFFFFF, 0x808080);
    CHECK(tinted[static_cast<size_t>(32) * kPitch + 31 * 3] == 0xFF);

    // The white light above is channel-symmetric, so it cannot tell the three
    // bytes apart. A light and an ambient with distinct channels move each
    // byte on its own: light RGB(0x00, 0x00, 0x80) on ambient
    // RGB(0x00, 0x40, 0x00) leaves the ramps' peak at R 240, G 176 and B
    // 255 (clamped). A port writing the channels in another order - R/B
    // swapped gives 255/176/240, G/B swapped 240/255/176 - fails here.
    mode.rgbGlobalTint = RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT);
    std::vector<BYTE> coloured = before;
    mode.DrawLightingEffect24(coloured.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0x800000, 0x004000);
    const auto colouredPixel = [&](int x, int y) { return &coloured[static_cast<size_t>(y) * kPitch + x * 3]; };
    CHECK(colouredPixel(31, 32)[0] == 240);
    CHECK(colouredPixel(31, 32)[1] == 176);
    CHECK(colouredPixel(31, 32)[2] == 0xFF);
}

// CVidMode::DrawLightingEffect16 (0x7968C0): the light pool on a 16-bpp
// surface, one WORD per pixel with lPitch in pixels. Each channel is taken
// through the mode's masks (+0x18/+0x1C/+0x20) and clamped at the mask's
// value rather than 255, so the case sets 5-6-5 up front. No surface and no
// GPU: a plain buffer is enough, and the function reads only the global tint
// (and the arc list through the mode's vtable).
TEST_CASE("vid: DrawLightingEffect16 brightens inside the pool and leaves the rest", "[vid]")
{
    constexpr int kCanvas = 64;
    constexpr int kPitch = kCanvas; // pixels
    constexpr WORD kGrey = 0x8410; // 5-6-5 mid grey

    CVidInf mode;
    mode.m_dwRBitMask = 0xF800;
    mode.m_dwGBitMask = 0x07E0;
    mode.m_dwBBitMask = 0x001F;
    mode.rgbGlobalTint = RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT);

    // The white light on a black ambient clamps every channel at its mask in
    // the middle of the pool; the edges only brighten part of the way.
    std::vector<WORD> canvas(static_cast<size_t>(kPitch) * kCanvas + 2, kGrey);
    const std::vector<WORD> before = canvas;
    mode.DrawLightingEffect16(canvas.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0xFFFFFF, 0x000000);

    const auto pixel = [&](int x, int y) -> WORD { return canvas[static_cast<size_t>(y) * kPitch + x]; };
    // The middle band ramps up to x = 31 and down from x = 32.
    CHECK(pixel(31, 32) == 0xFFFF);
    CHECK(pixel(24, 32) != kGrey);
    CHECK(pixel(38, 32) != kGrey);
    // The band's down ramp is nWidth - 1 pixels (0x797191), so x = 39 is
    // outside the pool. NOTE: the extra step a 24-bit-style nWidth ramp would
    // take lands on scale 64, which rewrites the pixel unchanged, so this pin
    // documents the extent but no pixel value distinguishes the two widths;
    // the exact ramp values below are what pin the sequence.
    CHECK(pixel(39, 32) == kGrey);
    // Everything outside the pool is byte-identical, including the two
    // canary words after the buffer (the 16-bit write is one WORD).
    CHECK(pixel(0, 0) == kGrey);
    CHECK(pixel(32 + 12, 32) == kGrey);
    CHECK(pixel(32, 32 + 12) == kGrey);
    CHECK(canvas[static_cast<size_t>(kPitch) * kCanvas] == kGrey);
    CHECK(canvas[static_cast<size_t>(kPitch) * kCanvas + 1] == kGrey);

    // A dim light never reaches the masks, so the ramp's arithmetic shows
    // exactly: 0x101010 on a black ambient gives (16 + 127), quantised to a
    // multiple of 8 above 64, i.e. 72 per channel and a step of 9; the middle
    // band's first two pixels get scales 73 and 82, turning the grey 5-6-5
    // channels (16, 32, 16) into (18, 36, 18) and (20, 41, 20).
    std::vector<WORD> dim = before;
    mode.DrawLightingEffect16(dim.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0x101010, 0x000000);
    CHECK(dim[static_cast<size_t>(32) * kPitch + 24] == 0x9492);
    CHECK(dim[static_cast<size_t>(32) * kPitch + 25] == 0xA534);

    // Height 0 draws nothing (the original leaks the pixel list there), and
    // so does a light whose channels all end at zero (no light, a white
    // ambient): the buffer stays byte-identical.
    {
        std::vector<WORD> flat = before;
        mode.DrawLightingEffect16(flat.data(), kPitch, CPoint(32, 32), CSize(8, 0), 0xFFFFFF, 0x000000);
        CHECK(flat == before);
    }
    {
        std::vector<WORD> dark = before;
        mode.DrawLightingEffect16(dark.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0x000000, 0xFFFFFF);
        CHECK(dark == before);
    }

    // A global tint other than none scales the ambient first: a black tint
    // zeroes it, and the same white light still fills the middle of the pool.
    mode.rgbGlobalTint = 0x000000;
    std::vector<WORD> tinted = before;
    mode.DrawLightingEffect16(tinted.data(), kPitch, CPoint(32, 32), CSize(8, 8), 0xFFFFFF, 0x808080);
    CHECK(tinted[static_cast<size_t>(32) * kPitch + 31] == 0xFFFF);
}

// CVidBitmap::BltBmp24To16 (0x7B37A0): a 24-bit BMP (three bytes, B then G
// then R) onto a 16-bit surface through the mode's channel shifts, rows
// bottom-up (lPitch in bytes). The function reads nothing from the bitmap
// object, so a plain buffer and a zeroed object are enough - no surface, no
// GPU.
TEST_CASE("vid: BltBmp24To16 converts 24-bit pixels into the mode's 16-bit fields", "[vid]")
{
    // A zeroed object: the port reads nothing from it (the oracle case does
    // the same).
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(storage, 0, sizeof(CVidBitmap));
    auto* bitmap = reinterpret_cast<CVidBitmap*>(storage);

    ChitinScope scope;
    CVidInf mode;
    ActiveModeScope active(&mode);
    const auto setMode16 = [&mode](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode.m_dwRBitMask = 0x1Fu << rShift;
        mode.m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode.m_dwBBitMask = 0x1F;
        mode.m_dwRBitShift = rShift;
        mode.m_dwGBitShift = 5;
        mode.m_dwBBitShift = 0;
        mode.field_C2 = 3;
        mode.field_C6 = 8 - gBits;
        mode.field_CA = 3;
    };

    constexpr int kW = 4;
    constexpr int kH = 4;
    constexpr size_t kPitch = kW * 2;
    const auto index = [](int x, int y) { return static_cast<size_t>(y) * kW + x; };

    // 5-6-5: pure blue, green and red bytes land in their fields; the rows go
    // up from the pointer handed in (row 2 then row 1).
    setMode16(true);
    std::vector<WORD> canvas(kW * kH, 0xFFFF);
    const BYTE data[12] = { 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x12, 0x34, 0x56 };
    CHECK(bitmap->BltBmp24To16(&canvas[index(1, 2)], static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(3, 2), 0, 0));
    CHECK(canvas[index(1, 2)] == 0x001F); // blue 0xFF -> 31
    CHECK(canvas[index(2, 2)] == 0x07E0); // green 0xFF -> 63 << 5
    CHECK(canvas[index(3, 2)] == 0xF800); // red 0xFF -> 31 << 11
    CHECK(canvas[index(1, 1)] == 0x51A2); // (0x12 >> 3) | ((0x34 >> 2) << 5) | ((0x56 >> 3) << 11)
    CHECK(canvas[index(0, 2)] == 0xFFFF); // the columns left of the blit
    CHECK(canvas[index(0, 1)] == 0xFFFF);

    // 5-5-5: the same bytes, five bits each.
    setMode16(false);
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    CHECK(bitmap->BltBmp24To16(&canvas[index(1, 2)], static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(3, 2), 0, 0));
    CHECK(canvas[index(1, 2)] == 0x001F);
    CHECK(canvas[index(2, 2)] == 0x03E0);
    CHECK(canvas[index(3, 2)] == 0x7C00);

    // The keyed form (0x1): pure green is transparent, so the pixel keeps the
    // destination; with 0x4 a black pixel is half of it.
    setMode16(true);
    std::fill(canvas.begin(), canvas.end(), 0xF800); // pure red destination
    const BYTE keyed[9] = { 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00 };
    CHECK(bitmap->BltBmp24To16(&canvas[index(0, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(keyed),
        CSize(3, 1), 0, 0x1));
    CHECK(canvas[index(0, 1)] == 0xF800); // the key leaves it
    CHECK(canvas[index(1, 1)] == 0x0000); // black written as black (only green is the key)
    CHECK(canvas[index(2, 1)] == 0x001F); // blue
    std::fill(canvas.begin(), canvas.end(), 0xF800);
    CHECK(bitmap->BltBmp24To16(&canvas[index(1, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(keyed + 3),
        CSize(1, 1), 0, 0x5));
    CHECK(canvas[index(1, 1)] == 0x7800); // half the red field (31 >> 1 = 15 << 11)

    // NOTE: the original's single-pixel path runs once even at width 0.
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    const BYTE one[6] = { 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00 };
    CHECK(bitmap->BltBmp24To16(&canvas[index(2, 2)], static_cast<LONG>(kPitch), const_cast<BYTE*>(one),
        CSize(0, 2), 0, 0));
    CHECK(canvas[index(2, 2)] == 0x001F);
    // The row step of that path is one pixel forward then a full row back.
    CHECK(canvas[index(3, 1)] == 0x07E0);
}

// CVidBitmap::BltBmp24To24Tint (0x7CCE80): a 24-bit BMP (three bytes, B then
// G then R) onto a 24-bit surface through the same tint, add and light passes
// as BltBmp24To32Tint, with its keyed form (pure green transparent; with 0x4
// a pure black pixel a shadow). Rows bottom-up (lPitch in bytes); a drawn
// pixel before a row's last writes a fourth byte over the next pixel's first.
TEST_CASE("vid: BltBmp24To24Tint tints, adds, lights and keys 24-bit pixels", "[vid]")
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(storage, 0, sizeof(CVidBitmap));
    auto* bitmap = reinterpret_cast<CVidBitmap*>(storage);

    ChitinScope scope;
    CVidInf mode;
    ActiveModeScope active(&mode);
    const auto setShifts = [&mode](DWORD r, DWORD g, DWORD b) {
        mode.m_dwRBitShift = r;
        mode.m_dwGBitShift = g;
        mode.m_dwBBitShift = b;
    };

    constexpr int kW = 4;
    constexpr int kH = 3;
    constexpr size_t kPitch = kW * 3;
    const auto index = [](int x, int y) { return static_cast<size_t>(y) * kW + x; };
    const auto rowBytes = [&](const std::vector<BYTE>& canvas, int x, int y, int n) {
        std::vector<BYTE> out(n);
        std::copy_n(canvas.begin() + static_cast<ptrdiff_t>(index(x, y) * 3), n, out.begin());
        return out;
    };

    // Plain (flags 0), shifts 16/8/0: the source's B, G, R bytes land at
    // bytes 0, 1, 2, the rows going up from the pointer handed in.
    const BYTE plain[18] = {
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x99,
        0x12,
        0x34,
        0x56,
        0x78,
        0x9A,
        0xBC,
        0xDE,
        0xF0,
        0x12,
    };
    setShifts(16, 8, 0);
    std::vector<BYTE> canvas(kW * kH * 3, 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 2) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(3, 2), 0, 0));
    CHECK(rowBytes(canvas, 0, 2, 9) == std::vector<BYTE> { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
    CHECK(rowBytes(canvas, 0, 1, 9) == std::vector<BYTE> { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x12 });
    CHECK(canvas[index(3, 2) * 3] == 0xEE); // the column right of the blit

    // Shifts 0/8/16: the same conversion with R at byte 0 and B at byte 2.
    setShifts(0, 8, 16);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(3, 1), 0, 0));
    CHECK(rowBytes(canvas, 0, 0, 9) == std::vector<BYTE> { 0x33, 0x22, 0x11, 0x66, 0x55, 0x44, 0x99, 0x88, 0x77 });

    // The tint (0x20000), 8-bit shift: channel * colour / 256. The first
    // pixel's fourth byte lands on the second's first, which its own write
    // overwrites again.
    bitmap->m_paletteAffects.rgbTintColor = RGB(0x80, 0x40, 0xFF);
    setShifts(16, 8, 0);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 1) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(2, 1), 0, 0x20000));
    CHECK(rowBytes(canvas, 0, 1, 6) == std::vector<BYTE> { 0x10, 0x08, 0x19, 0x43, 0x15, 0x33 });

    // The add (0x40000): ~((~(channel * (~colour)) >> 8)), an 8-bit shift.
    bitmap->m_paletteAffects.rgbAddColor = RGB(0x10, 0x20, 0x30);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(2, 1), 0, 0x40000));
    CHECK(rowBytes(canvas, 0, 0, 6) == std::vector<BYTE> { 0x3F, 0x3F, 0x41, 0x68, 0x6B, 0x71 });

    // The light (0x200000), scale 8 + the colour: the product above 255 keeps
    // its high bits (the original clamps only the low byte), so red 0x1FF
    // carries its bit 8 into green's byte with shifts 0/8/16.
    bitmap->m_paletteAffects.rgbLightColor = RGB(0x40, 0x20, 0x10);
    setShifts(0, 8, 16);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 2) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(2, 1), 0, 0x200000));
    CHECK(rowBytes(canvas, 0, 2, 6) == std::vector<BYTE> { 0xFF, 0xAB, 0x33, 0xFF, 0xFF, 0xCD });

    // Keyed (0x5), shifts 0/8/16: pure green is transparent, and a black
    // pixel is half the destination (the black level is 0 here). The first
    // pixel's fourth byte zeroes the green one's first, which stays zero.
    setShifts(0, 8, 16);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    canvas[index(0, 1) * 3] = 0x40;
    canvas[index(0, 1) * 3 + 1] = 0x80;
    canvas[index(0, 1) * 3 + 2] = 0xC0;
    canvas[index(1, 1) * 3] = 0x10;
    canvas[index(1, 1) * 3 + 1] = 0x20;
    canvas[index(1, 1) * 3 + 2] = 0x30;
    const BYTE shadowed[6] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00 };
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 1) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(shadowed), CSize(2, 1), 0, 0x5));
    CHECK(rowBytes(canvas, 0, 1, 6) == std::vector<BYTE> { 0x20, 0x40, 0x60, 0x00, 0x20, 0x30 });

    // Keyed (0x5), shifts 16/8/0: the row's last pixel reads a WORD for red
    // and green, so its red stays black and its blue halves the destination's
    // red byte; the drawn first pixel's fourth byte zeroes the word's low one.
    setShifts(16, 8, 0);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    canvas[index(0, 1) * 3] = 0x40;
    canvas[index(0, 1) * 3 + 1] = 0x80;
    canvas[index(0, 1) * 3 + 2] = 0xC0;
    canvas[index(1, 1) * 3] = 0x10;
    canvas[index(1, 1) * 3 + 1] = 0x20;
    canvas[index(1, 1) * 3 + 2] = 0x30;
    const BYTE shadowTail[6] = { 0x11, 0x22, 0x33, 0x00, 0x00, 0x00 };
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 1) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(shadowTail), CSize(2, 1), 0, 0x5));
    CHECK(rowBytes(canvas, 0, 1, 6) == std::vector<BYTE> { 0x11, 0x22, 0x33, 0x18, 0x10, 0x00 });

    // The keyed tint and grey of the same branch: a key leaves the pixel, the
    // tint truncates to bytes, and 0x80000 greys the source first.
    setShifts(16, 8, 0);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    const BYTE keyed[6] = { 0x00, 0xFF, 0x00, 0x11, 0x22, 0x33 };
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(keyed), CSize(2, 1), 0, 0x20001));
    CHECK(rowBytes(canvas, 0, 0, 6) == std::vector<BYTE> { 0xEE, 0xEE, 0xEE, 0x10, 0x08, 0x19 });
    const BYTE grey[3] = { 0x10, 0x20, 0x30 };
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(grey), CSize(1, 1), 0, 0x80001));
    CHECK(rowBytes(canvas, 0, 0, 3) == std::vector<BYTE> { 0x18, 0x18, 0x18 });
    // Black without 0x4 is written as black (only green is the key).
    const BYTE black[3] = { 0x00, 0x00, 0x00 };
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(black), CSize(1, 1), 0, 0x1));
    CHECK(rowBytes(canvas, 0, 0, 3) == std::vector<BYTE> { 0x00, 0x00, 0x00 });

    // The no-flag paths (the T58 oracle failure's shape on the 16-bit
    // sibling): GetAdd and GetLight answer the mode's brightness and gamma
    // with no flag bits at all, and 0x10000 tints with the mode's global
    // tint - here a different colour from the palette's, so the source shows.
    setShifts(16, 8, 0);

    // Flags 0x0, brightness 0x40: the add runs with ~0x40 = 191, shift 8
    // (((0x33 ^ 0xFF) * 191 >> 8) ^ 0xFF = 0x67, and so on).
    mode.m_nBrightnessCorrection = 0x40;
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(1, 1), 0, 0x0));
    CHECK(rowBytes(canvas, 0, 0, 3) == std::vector<BYTE> { 0x4E, 0x5B, 0x67 });

    // Flags 0x0, gamma 0x20: the light runs with 8 + 0x20 = 40 over the scale
    // of 3 (0x33 * 40 >> 3 = 0xFF).
    mode.m_nBrightnessCorrection = 0;
    mode.m_nGammaCorrection = 0x20;
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(1, 1), 0, 0x0));
    CHECK(rowBytes(canvas, 0, 0, 3) == std::vector<BYTE> { 0x55, 0xAA, 0xFF });

    // 0x10000 tints with the global tint (0x33 * 0x40 >> 8 = 0x0C), the
    // brightness add on top.
    mode.m_nGammaCorrection = 0;
    mode.m_nBrightnessCorrection = 0x40;
    mode.rgbGlobalTint = RGB(0x40, 0x20, 0x10);
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(1, 1), 0, 0x10000));
    CHECK(rowBytes(canvas, 0, 0, 3) == std::vector<BYTE> { 0x42, 0x44, 0x4A });

    // The same global tint with the gamma light instead.
    mode.m_nBrightnessCorrection = 0;
    mode.m_nGammaCorrection = 0x20;
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 0) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(plain), CSize(1, 1), 0, 0x10000));
    CHECK(rowBytes(canvas, 0, 0, 3) == std::vector<BYTE> { 0x05, 0x14, 0x3C });
    mode.m_nGammaCorrection = 0;
    mode.rgbGlobalTint = RGB(0xFF, 0xFF, 0xFF);

    // Keyed with shadows (0x5), brightness 0x40: the black level is
    // ApplyBrightnessContrast(0) = 0x41 a channel, the shadow half the
    // destination plus it; the key keeps its pixel bar its first byte, which
    // the shadow's fourth (zero) byte overwrote.
    mode.m_nBrightnessCorrection = 0x40;
    std::fill(canvas.begin(), canvas.end(), 0xEE);
    canvas[index(0, 1) * 3] = 0x10;
    canvas[index(0, 1) * 3 + 1] = 0x20;
    canvas[index(0, 1) * 3 + 2] = 0x30;
    canvas[index(1, 1) * 3] = 0x40;
    canvas[index(1, 1) * 3 + 1] = 0x50;
    canvas[index(1, 1) * 3 + 2] = 0x60;
    const BYTE shadowBright[6] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00 };
    CHECK(bitmap->BltBmp24To24Tint(canvas.data() + index(0, 1) * 3, static_cast<LONG>(kPitch),
        const_cast<BYTE*>(shadowBright), CSize(2, 1), 0, 0x5));
    CHECK(rowBytes(canvas, 0, 1, 6) == std::vector<BYTE> { 0x28, 0x30, 0x38, 0x00, 0x50, 0x60 });
    mode.m_nBrightnessCorrection = 0;

    ::operator delete(storage);
}

// CVidBitmap::BltBmp24To16Tint (0x7B3B70): the same 24-to-16 conversion with
// the image's tint, add and light multipliers and the high-word flags, i.e.
// the 16-bit counterpart of BltBmp24To32Tint. The bitmap is zeroed (no range
// tints); the mode's global tint, brightness and gamma drive GetTint, GetAdd
// and GetLight, so the maths is the shipped functions'.
TEST_CASE("vid: BltBmp24To16Tint applies the tint, add and light of the 24-to-16 blit", "[vid]")
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(storage, 0, sizeof(CVidBitmap));
    auto* bitmap = reinterpret_cast<CVidBitmap*>(storage);

    ChitinScope scope;
    CVidInf mode;
    ActiveModeScope active(&mode);
    const auto setMode16 = [&mode](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode.m_dwRBitMask = 0x1Fu << rShift;
        mode.m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode.m_dwBBitMask = 0x1F;
        mode.m_dwRBitShift = rShift;
        mode.m_dwGBitShift = 5;
        mode.m_dwBBitShift = 0;
        mode.field_C2 = 3;
        mode.field_C6 = 8 - gBits;
        mode.field_CA = 3;
        mode.rgbGlobalTint = RGB(255, 255, 255);
        mode.m_nBrightnessCorrection = 0;
        mode.m_nGammaCorrection = 0;
    };

    constexpr int kW = 4;
    constexpr int kH = 4;
    constexpr size_t kPitch = kW * 2;
    const auto index = [](int x, int y) { return static_cast<size_t>(y) * kW + x; };

    setMode16(true);

    // The global tint (0x10000) halves every channel: 255 * 128 >> 8 = 127,
    // 127 >> 3 = 15 in the 5-bit red field.
    const BYTE red[3] = { 0x00, 0x00, 0xFF };
    mode.rgbGlobalTint = RGB(128, 128, 128);
    std::vector<WORD> canvas(kW * kH, 0xFFFF);
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(1, 2)], static_cast<LONG>(kPitch), const_cast<BYTE*>(red),
        CSize(1, 1), 0, 0x10001));
    CHECK(canvas[index(1, 2)] == 0x7800); // r 127 -> 15 << 11

    // The transparent green is skipped before the tint: the pixel keeps the
    // destination.
    const BYTE green[6] = { 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF };
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(0, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(green),
        CSize(2, 1), 0, 0x10001));
    CHECK(canvas[index(0, 1)] == 0xFFFF);
    CHECK(canvas[index(1, 1)] == 0x7800); // red 255 -> 127 -> 15 << 11

    // The brightness correction (0x40000) adds through GetAdd: a black
    // source comes out 129 in every channel ((~255 * 32385) >> 16 -> ~126).
    mode.rgbGlobalTint = RGB(255, 255, 255);
    mode.m_nBrightnessCorrection = 128;
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    const BYTE black[3] = { 0x00, 0x00, 0x00 };
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(2, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(black),
        CSize(1, 1), 0, 0x40001));
    CHECK(canvas[index(2, 1)] == 0x8410); // 129 in each field: 16 | 32 << 5 | 16 << 11

    // The gamma correction (0x200000) doubles through GetLight (8 + 8 over
    // the light scale of 3) and clamps at 255.
    mode.m_nBrightnessCorrection = 0;
    mode.m_nGammaCorrection = 8;
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    const BYTE grey[6] = { 0x40, 0x40, 0x40, 0xFF, 0xFF, 0xFF };
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(0, 0)], static_cast<LONG>(kPitch), const_cast<BYTE*>(grey),
        CSize(2, 1), 0, 0x200001));
    CHECK(canvas[index(0, 0)] == 0x8410); // 64 -> 128 in each field
    CHECK(canvas[index(1, 0)] == 0xFFFF); // white stays white
    mode.m_nGammaCorrection = 0;

    // 0x80000 makes the pixel grey first: (0x30 + 0x60 + 0x90) >> 2 = 72.
    const BYTE mixed[3] = { 0x30, 0x60, 0x90 };
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(3, 0)], static_cast<LONG>(kPitch), const_cast<BYTE*>(mixed),
        CSize(1, 1), 0, 0x80001));
    CHECK(canvas[index(3, 0)] == 0x4A49); // 72: 9 | 18 << 5 | 9 << 11

    // The shadow (0x4) halves the destination channel with no tint at all:
    // 0xF800's red field (31) widened to 248 is 124, i.e. 15 in the field.
    std::fill(canvas.begin(), canvas.end(), 0xF800);
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(1, 2)], static_cast<LONG>(kPitch), const_cast<BYTE*>(black),
        CSize(1, 1), 0, 0x5));
    CHECK(canvas[index(1, 2)] == 0x7800);

    // The plain form (no 0x1) tints too, walks two rows upward through the
    // mode's shifts and steps nDataJump bytes between them.
    setMode16(false); // 5-5-5
    mode.rgbGlobalTint = RGB(128, 128, 128);
    std::fill(canvas.begin(), canvas.end(), 0xFFFF);
    const BYTE rows[9] = { 0x00, 0x00, 0xFF, 0x11, 0x88, 0x44, 0x22, 0x00, 0x00 };
    CHECK(bitmap->BltBmp24To16Tint(&canvas[index(1, 2)], static_cast<LONG>(kPitch), const_cast<BYTE*>(rows),
        CSize(1, 2), 1, 0x10000));
    CHECK(canvas[index(1, 2)] == 0x3C00); // red 127 -> 15 << 10
    CHECK(canvas[index(1, 1)] == 0x0888); // (0x88, 0x44, 0x22) halved: 8 | 4 << 5 | 2 << 10

    ::operator delete(storage);
}

// CVidBitmap::BltBmp24To16Tint's unkeyed pack (0x7B3B70's shared tail
// 0x7B3FF8): the light step's clamp replaces only the low byte of the
// register, so a channel whose lit value passes 255 keeps its second byte,
// and the 16-bit pack shifts it along with the first - the overflow lands in
// the neighbouring fields. These are the original's own words for the pixels
// (checked against the rig's vid/blt_bmp24to16_tint case), not the port's
// arithmetic, so a regression that masks each channel to a byte shows here.
TEST_CASE("vid: BltBmp24To16Tint packs the unkeyed light overflow into the neighbouring fields", "[vid]")
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(storage, 0, sizeof(CVidBitmap));
    auto* bitmap = reinterpret_cast<CVidBitmap*>(storage);

    ChitinScope scope;
    CVidInf mode;
    ActiveModeScope active(&mode);
    const auto setMode16 = [&mode](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode.m_dwRBitMask = 0x1Fu << rShift;
        mode.m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode.m_dwBBitMask = 0x1F;
        mode.m_dwRBitShift = rShift;
        mode.m_dwGBitShift = 5;
        mode.m_dwBBitShift = 0;
        mode.field_C2 = 3;
        mode.field_C6 = 8 - gBits;
        mode.field_CA = 3;
        mode.rgbGlobalTint = RGB(255, 255, 255);
        mode.m_nBrightnessCorrection = 55;
        mode.m_nGammaCorrection = 8;
    };

    constexpr int kW = 4;
    constexpr size_t kPitch = kW * 2;
    // (B, G, R) per pixel: green and blue overflow the light step, red does
    // not, so the green's second byte accents the red field.
    const BYTE data[12] = { 131, 250, 25, 100, 111, 50, 10, 20, 30, 0, 255, 0 };

    setMode16(true);
    std::vector<WORD> canvas(kW, 0x1111);
    CHECK(bitmap->BltBmp24To16Tint(canvas.data(), static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(4, 1), 0, 0));
    CHECK(canvas[0] == 0xBFFF);
    CHECK(canvas[1] == 0xBFFF);
    CHECK(canvas[2] == 0xA490);
    CHECK(canvas[3] == 0x77EE);

    // The same pixels through the odd-width tail (three of them).
    std::fill(canvas.begin(), canvas.end(), 0x1111);
    CHECK(bitmap->BltBmp24To16Tint(canvas.data(), static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(3, 1), 0, 0));
    CHECK(canvas[0] == 0xBFFF);
    CHECK(canvas[1] == 0xBFFF);
    CHECK(canvas[2] == 0xA490);
    CHECK(canvas[3] == 0x1111);

    // 5-5-5: five bits each, so the same overflow reaches the green field.
    setMode16(false);
    std::fill(canvas.begin(), canvas.end(), 0x1111);
    CHECK(bitmap->BltBmp24To16Tint(canvas.data(), static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(4, 1), 0, 0));
    CHECK(canvas[0] == 0x6FFF);
    CHECK(canvas[1] == 0x7FFF);
    CHECK(canvas[2] == 0x5250);
    CHECK(canvas[3] == 0x3BEE);

    ::operator delete(storage);
}

// CVidBitmap::BltBmp8To16, which has no address of its own: the original
// inlines it in RenderDirect (0x7B3190, the 16-bpp branch at
// 0x7B362D-0x7B3735). An 8-bit paletted bitmap is realized into
// CVidImage::rgbTempPal at 16 bpp and each index's word written to the
// surface, rows bottom-up; the inlined code stores two pixels per dword
// (second in the high word), writes an odd width's leading pixel before the
// pairs, and one pixel per row below width 2.
TEST_CASE("vid: BltBmp8To16 paints 8-bit indices through the palette into 16-bit fields", "[vid]")
{
    // Leaked on purpose: the 24-to-16 tests' harness keeps engine objects out
    // of destructors, and these own a resource. This path does realize a
    // palette, so the bitmap is constructed (not the zeroed block those tests
    // use).
    const auto& pal = CVidImage::rgbTempPal;

    RGBQUAD colors[4] = {};
    colors[0].rgbRed = 0xFF; // pure red
    colors[1].rgbGreen = 0xFF; // pure green
    colors[2].rgbBlue = 0xFF; // pure blue
    colors[3].rgbRed = colors[3].rgbGreen = colors[3].rgbBlue = 0x80; // grey
    auto* res = new CResBitmap;
    res->bParsed = TRUE;
    res->nColors = 4;
    res->pColorTable = colors;
    auto* bitmap = new CVidBitmap;
    bitmap->SetRes(res);

    ChitinScope scope;
    CVidInf mode;
    ActiveModeScope active(&mode);
    const auto setMode16 = [&mode](bool green6) {
        const int gBits = green6 ? 6 : 5;
        const DWORD rShift = 5 + gBits;
        mode.m_dwRBitMask = 0x1Fu << rShift;
        mode.m_dwGBitMask = ((1u << gBits) - 1) << 5;
        mode.m_dwBBitMask = 0x1F;
        mode.m_dwRBitShift = rShift;
        mode.m_dwGBitShift = 5;
        mode.m_dwBBitShift = 0;
        mode.field_C2 = 3;
        mode.field_C6 = 8 - gBits;
        mode.field_CA = 3;
        mode.rgbGlobalTint = RGB(255, 255, 255);
        mode.m_nBrightnessCorrection = 0;
        mode.m_nGammaCorrection = 0;
    };

    constexpr int kW = 4;
    constexpr size_t kPitch = kW * 2;
    const auto index = [](int x, int y) { return static_cast<size_t>(y) * kW + x; };
    // Row 0's indices are 0,1,2,3; row 1's are 3,2,1,0.
    const BYTE data[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };

    // 5-6-5, even width: pairs of indices, rows bottom-up from the pointer
    // handed in (row 1 first, then row 0). Each pixel is the low WORD of the
    // realized palette entry - the inlined code reads `word [pal + 4*i]`.
    setMode16(true);
    std::vector<WORD> canvas(kW * 2, 0x1111);
    CHECK(bitmap->BltBmp8To16(&canvas[index(0, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(4, 2), 0, 0));
    CHECK(canvas[index(0, 1)] == static_cast<WORD>(pal[0]));
    CHECK(canvas[index(1, 1)] == static_cast<WORD>(pal[1]));
    CHECK(canvas[index(2, 1)] == static_cast<WORD>(pal[2]));
    CHECK(canvas[index(3, 1)] == static_cast<WORD>(pal[3]));
    CHECK(canvas[index(0, 0)] == static_cast<WORD>(pal[3])); // the next source row, one row up
    CHECK(canvas[index(3, 0)] == static_cast<WORD>(pal[0]));
    CHECK(canvas[index(0, 1)] == 0xF800); // the pack itself: pure red

    // An odd width writes its leading pixel, then pairs through a dword
    // store; the pixel right of the blit keeps the destination.
    std::fill(canvas.begin(), canvas.end(), 0x1111);
    CHECK(bitmap->BltBmp8To16(&canvas[index(0, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(3, 2), 0, 0));
    CHECK(canvas[index(0, 1)] == static_cast<WORD>(pal[0]));
    CHECK(canvas[index(1, 1)] == static_cast<WORD>(pal[1]));
    CHECK(canvas[index(2, 1)] == static_cast<WORD>(pal[2]));
    CHECK(canvas[index(3, 1)] == 0x1111);
    // Row 0 continues where row 1 stopped: indices 3, 3, 2.
    CHECK(canvas[index(0, 0)] == static_cast<WORD>(pal[3]));
    CHECK(canvas[index(2, 0)] == static_cast<WORD>(pal[2]));

    // Below width 2 the original writes one pixel per row whatever the width.
    std::fill(canvas.begin(), canvas.end(), 0x1111);
    CHECK(bitmap->BltBmp8To16(&canvas[index(1, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(1, 2), 0, 0));
    CHECK(canvas[index(1, 1)] == static_cast<WORD>(pal[0]));
    CHECK(canvas[index(0, 1)] == 0x1111);
    CHECK(canvas[index(1, 0)] == static_cast<WORD>(pal[1]));

    // 5-5-5: the same palette in five bits each.
    setMode16(false);
    std::fill(canvas.begin(), canvas.end(), 0x1111);
    CHECK(bitmap->BltBmp8To16(&canvas[index(0, 1)], static_cast<LONG>(kPitch), const_cast<BYTE*>(data),
        CSize(4, 2), 0, 0));
    CHECK(canvas[index(0, 1)] == static_cast<WORD>(pal[0]));
    CHECK(canvas[index(1, 1)] == static_cast<WORD>(pal[1]));
    CHECK(canvas[index(2, 1)] == static_cast<WORD>(pal[2]));
    CHECK(canvas[index(3, 1)] == static_cast<WORD>(pal[3]));
    CHECK(canvas[index(0, 1)] == 0x7C00); // the pack: pure red, five bits
}
