// The video layer's pixel functions, the original's against ours, on plain
// buffers.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures (R10a, R10b). The buffers the original writes are blocks from its
// operator new, leaked; ours writes into a copy holding the same pixels.
// CVidPalette::RealizeRange (0x7BF870) reads the current video mode through
// g_pChitin (0x8CF6D8) -> pActiveEngine (0x3C4) -> pVidMode (4), the AU
// counter at 0x190E and the 3d flag at 0x91C; the chitin and the engine are
// zeroed blocks holding only those members. The video mode is a CVidInf from
// the original's constructor (0x79AF00), because the palette's SetType calls
// its GetTransparentColor through the vtable, with the members the function
// reads set afterwards. The palettes come from the original's CVidPalette
// constructor (0x7BEEA0). Ours gets the same: zeroed CChitin and CWarp
// storage, our CVidInf and CVidPalette from their constructors; g_pChitin is
// put back when the case ends. Original objects are never freed.
//
// The cell blends (sub_7D0950, sub_7D1190) run on a synthetic BAM: zeroed
// CVidCell and CResCell blocks holding only the members the functions read
// (R10b), the header, palette, frame entry and frame data in the original's
// memory on its side and in ours on ours. FillConvexPoly's polygon comes
// from the original's CVidPoly constructor (0x7C0DB0); its only global is
// g_pChitin->cVideo.m_nBpp, set in the zeroed chitin.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CChitin.h"
#include "CParticle.h"
#include "CResCell.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidInf.h"
#include "CVidPalette.h"
#include "CVidPoly.h"
#include "CWarp.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CVidPoly::DrawHLineShaded32 (0x7D6EB0) and DrawHLineShadedMirrored32
// (0x7D6FD0): (pSurface, xMin, xMax, dwColor, rSurface, ptRef), `ret 0x18`
// at 0x7D6FC3 and 0x7D70ED. Neither reads ecx; the mirrored one reads
// rSurface's left and right.
using HLineFn = void(__fastcall*)(void* self, void* edx, void* pSurface, int32_t xMin, int32_t xMax,
    uint32_t dwColor, const void* rSurface, const void* ptRef);
constexpr uintptr_t kDrawHLineShaded32 = 0x7D6EB0;
constexpr uintptr_t kDrawHLineShadedMirrored32 = 0x7D6FD0;
// CVidPoly: field_0, m_pVertices, m_nVertices, field_C, field_10,
// m_pDrawHLineFunction.
constexpr size_t kVidPolySize = 0x18;

// CVidPalette::RealizeRange(pDestPalette, nBpp, dwFlags, pAffectArgs,
// nTransVal) (0x7BF870), `ret 0x14` at 0x7C04AC.
using RealizeRangeFn = void(__fastcall*)(void* self, void* edx, uint32_t* pDest, int32_t nBpp, uint32_t dwFlags,
    void* pAffectArgs, uint32_t nTransVal);
constexpr uintptr_t kRealizeRange = 0x7BF870;
// CVidPalette::CVidPalette(USHORT nType) (0x7BEEA0), `ret 4`.
using PaletteCtorFn = void*(__fastcall*)(void* self, void* edx, uint32_t nType);
constexpr uintptr_t kPaletteCtor = 0x7BEEA0;
// CVidPalette: m_nAUCounter 0, m_nAUCounterBase 4, m_pPalette 8, m_nEntries
// 0xC, rgbGlobalTint 0x10, m_nType 0x14, m_bPaletteOwner 0x16,
// m_bSubRangesCalculated 0x18, m_rangeColors 0x1C.
constexpr size_t kPaletteSize = 0x24;
constexpr size_t kPaletteBaseOffset = 0x4;
constexpr size_t kPalettePaletteOffset = 0x8;
constexpr size_t kPaletteSubRangesOffset = 0x18;
// CVidInf::CVidInf() (0x79AF00), plain ret; operator new(0x73E) at 0x7951DC.
using VidInfCtorFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kVidInfCtor = 0x79AF00;
constexpr size_t kVidInfSize = 0x73E;
// CVidMode members RealizeRange and its callees read.
constexpr size_t kFadeOffset = 0x49;
constexpr size_t kGlobalTintOffset = 0xA4;
constexpr size_t kBrightnessOffset = 0xA8;
constexpr size_t kGammaOffset = 0xA9;
constexpr size_t kRShiftOffset = 0xAA;
constexpr size_t kGShiftOffset = 0xAE;
constexpr size_t kBShiftOffset = 0xB2;
// g_pChitin and its members.
constexpr uintptr_t kChitin = 0x8CF6D8;
constexpr size_t kChitinSize = 0x2000;
constexpr size_t kActiveEngineOffset = 0x3C4;
constexpr size_t kIs3dOffset = 0x91C;
constexpr size_t kAUCounterOffset = 0x190E;
// CChitin::cVideo (0x7EA) starts with m_nBpp, which
// CVidPoly::SetHLineFunction reads.
constexpr size_t kBppOffset = 0x7EA;
constexpr size_t kEngineSize = 0x10;
constexpr size_t kEngineVidModeOffset = 0x4;
// CVIDIMG_PALETTEAFFECT.
constexpr size_t kAffectSize = 0x78;

void* OriginalNew(size_t size)
{
    return reinterpret_cast<OriginalNewFn>(kOriginalNew)(size);
}

// A zeroed block from the original's operator new, leaked.
uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(OriginalNew(size));
    std::memset(at, 0, size);
    return at;
}

template <class T>
void Put(uint8_t* at, size_t offset, T value)
{
    std::memcpy(at + offset, &value, sizeof(value));
}

template <class T>
T Get(const uint8_t* at, size_t offset)
{
    T value;
    std::memcpy(&value, at + offset, sizeof(value));
    return value;
}

// Picks inputs from the run's --seed, apart from other streams by a salt.
struct Picks {
    Picks(uint64_t seed, uint64_t salt)
        : state(seed ^ salt)
        , bits(oracle::SplitMix64(&state))
    {
    }
    uint32_t Bits() { return *bits.Next(); }
    int32_t Between(int32_t lo, int32_t hi)
    {
        const uint32_t span = static_cast<uint32_t>(hi) - static_cast<uint32_t>(lo) + 1u;
        return static_cast<int32_t>(static_cast<uint32_t>(lo) + (span ? Bits() % span : Bits()));
    }

    uint64_t state;
    oracle::RandomGen<uint32_t> bits;
};

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", value);
    return text;
}

// Records the first differing entry of two arrays.
void CompareArrays(CaseContext& ctx, const uint32_t* original, const uint32_t* ours, int count)
{
    for (int i = 0; i < count; ++i) {
        if (original[i] != ours[i]) {
            ctx.Fail(ctx.Inputs() + ", entry " + std::to_string(i), Hex(original[i]), Hex(ours[i]));
            return;
        }
    }
}

constexpr int kLineWidth = 64;

// One DrawHLineShaded32 (or the mirrored one) on a 64x1 row of random
// pixels: xMin and xMax anywhere in the row (including empty spans), the
// surface rectangle as wide as the row allows.
void HLineShaded(CaseContext& ctx, uintptr_t address, bool mirrored, uint64_t salt)
{
    uint8_t* poly = nullptr;
    uint32_t* row = nullptr;
    uint8_t* rect = nullptr;
    uint8_t* ref = nullptr;
    ctx.SetInputs("building the original's buffers");
    ctx.CallOriginal([&] {
        poly = OriginalBlock(kVidPolySize);
        row = reinterpret_cast<uint32_t*>(OriginalBlock(kLineWidth * 4));
        rect = OriginalBlock(16);
        ref = OriginalBlock(8);
    });
    CVidPoly ours;
    Picks picks(ctx.seed, salt);

    auto check = [&](int32_t xMin, int32_t xMax, uint32_t color, int32_t left, int32_t right) {
        uint32_t pixels[kLineWidth];
        for (uint32_t& pixel : pixels) {
            // Dark pixels (sum below 32) and bright ones both occur.
            pixel = picks.Bits() >> picks.Between(0, 8);
        }
        std::memcpy(row, pixels, sizeof(pixels));
        const int32_t r[4] = { left, 0, right, 1 };
        std::memcpy(rect, r, sizeof(r));
        const int32_t pt[2] = { picks.Between(0, 7), picks.Between(0, 7) };
        std::memcpy(ref, pt, sizeof(pt));
        ctx.SetInputs((mirrored ? "DrawHLineShadedMirrored32(" : "DrawHLineShaded32(") + std::to_string(xMin)
            + ", " + std::to_string(xMax) + ", " + Hex(color) + ", rSurface " + std::to_string(left) + ".."
            + std::to_string(right) + ")");
        ctx.CallOriginal([&] {
            reinterpret_cast<HLineFn>(address)(poly, nullptr, row, xMin, xMax, color, rect, ref);
        });
        const CRect rSurface(left, 0, right, 1);
        const CPoint ptRef(pt[0], pt[1]);
        if (mirrored) {
            ours.DrawHLineShadedMirrored32(pixels, xMin, xMax, color, rSurface, ptRef);
        } else {
            ours.DrawHLineShaded32(pixels, xMin, xMax, color, rSurface, ptRef);
        }
        ctx.CountInputs(1);
        CompareArrays(ctx, row, pixels, kLineWidth);
    };

    // Every span of the row once, with the widest surface rectangle.
    for (int32_t xMin = 0; xMin < kLineWidth; ++xMin) {
        for (int32_t xMax = xMin - 1; xMax < kLineWidth; ++xMax) {
            check(xMin, xMax, picks.Bits() & 0xFFFFFF, 0, kLineWidth - 1);
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t xMin = picks.Between(0, kLineWidth - 1);
        const int32_t xMax = picks.Between(xMin - 2, kLineWidth - 1);
        // Mirrored: the span runs from right - left - xMin down to
        // right - left - xMax, which stays in the row.
        const int32_t width = picks.Between(xMax < xMin ? xMin : xMax, kLineWidth - 1);
        const int32_t left = picks.Between(-100, 100);
        check(xMin, xMax, picks.Bits(), left, left + width);
    }
}

void HLineShaded32(CaseContext& ctx)
{
    HLineShaded(ctx, kDrawHLineShaded32, false, 0x484C696E65530000ull); // "HLineS"
}

void HLineShadedMirrored32(CaseContext& ctx)
{
    HLineShaded(ctx, kDrawHLineShadedMirrored32, true, 0x484C696E654D0000ull); // "HLineM"
}

// CVidPoly::DrawHLineShaded16 (0x7C1FD0) and DrawHLineShadedMirrored16
// (0x7C2040), `ret 0x18` at 0x7C2037 and 0x7C20B6: they read the current
// video mode through g_pChitin (masks 0x18-0x20, shifts 0xAA-0xB2, the
// bits dropped at 0xC2-0xCA) for ScaleColor16 (0x79AD20) and
// GetIntensity16 (0x79AD90). DrawHLineShaded24 (0x7D6CA0) and
// DrawHLineShadedMirrored24 (0x7D6DA0), `ret 0x18` at 0x7D6D94 and
// 0x7D6EA2, read nothing but their arguments.
constexpr uintptr_t kDrawHLineShaded16 = 0x7C1FD0;
constexpr uintptr_t kDrawHLineShadedMirrored16 = 0x7C2040;
constexpr uintptr_t kDrawHLineShaded24 = 0x7D6CA0;
constexpr uintptr_t kDrawHLineShadedMirrored24 = 0x7D6DA0;
// CVidPoly::DrawHLine16 (0x7C1AA0), DrawHLineMirrored16 (0x7C1AF0),
// DrawHLine24 (0x7D6970) and DrawHLineMirrored24 (0x7D69B0), all `ret 0x18`
// and reading only their arguments.
constexpr uintptr_t kDrawHLine16 = 0x7C1AA0;
constexpr uintptr_t kDrawHLineMirrored16 = 0x7C1AF0;
constexpr uintptr_t kDrawHLine24 = 0x7D6970;
constexpr uintptr_t kDrawHLineMirrored24 = 0x7D69B0;
// The dithered spans: DrawHLineDithered16 (0x7C1B50), its mirror
// (0x7C1BB0), DrawHLineDithered24 (0x7D6A00), its mirror (0x7D6A70),
// DrawHLineDithered32 (0x7D6B60) and its mirror (0x7D6BF0), all `ret 0x18`;
// they read ptRef for the dither parity and the 32-bit ones the 3d flag
// (0x91C, 0 in the fixture).
constexpr uintptr_t kDrawHLineDithered16 = 0x7C1B50;
constexpr uintptr_t kDrawHLineDitheredMirrored16 = 0x7C1BB0;
constexpr uintptr_t kDrawHLineDithered24 = 0x7D6A00;
constexpr uintptr_t kDrawHLineDitheredMirrored24 = 0x7D6A70;
constexpr uintptr_t kDrawHLineDithered32 = 0x7D6B60;
constexpr uintptr_t kDrawHLineDitheredMirrored32 = 0x7D6BF0;
constexpr size_t kRMaskOffset = 0x18;
constexpr size_t kGMaskOffset = 0x1C;
constexpr size_t kBMaskOffset = 0x20;
constexpr size_t kRDropOffset = 0xC2;
constexpr size_t kGDropOffset = 0xC6;
constexpr size_t kBDropOffset = 0xCA;

// CVidMode::DrawThickLine32(nXFrom, nYFrom, nXTo, nYTo, pSurface, lPitch,
// rSurface, a8, color, bClipped) (0x7CBE70), `ret 0x28`. It reads no member;
// ecx goes on to the slice helper 0x7CC130, which ignores it.
using ThickLineFn = int32_t(__fastcall*)(void* self, void* edx, int32_t nXFrom, int32_t nYFrom, int32_t nXTo,
    int32_t nYTo, uint32_t* pSurface, int32_t lPitch, const void* rSurface, int32_t a8, uint32_t color,
    uint32_t bClipped);
constexpr uintptr_t kDrawThickLine32 = 0x7CBE70;
constexpr int kCanvas = 96;
constexpr int kMargin = 16;

// DrawThickLine32 (and its slice helper 0x7CC130) on a 96x96 canvas whose
// surface rectangle leaves a 16-pixel margin: vertical, horizontal,
// diagonal and general lines in every direction, thickness 0 to 9, clipped
// by the function (bClipped FALSE, endpoints anywhere on the canvas) or
// already inside the rectangle (bClipped TRUE). The whole canvas and the
// result are compared.
void ThickLine32(CaseContext& ctx)
{
    uint8_t* self = nullptr;
    uint32_t* canvas = nullptr;
    uint8_t* rect = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(kVidInfSize);
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(kCanvas * kCanvas * 4));
        rect = OriginalBlock(16);
    });
    const int32_t r[4] = { kMargin, kMargin, kCanvas - kMargin, kCanvas - kMargin };
    std::memcpy(rect, r, sizeof(r));
    CVidInf ours;
    CRect rSurface(r[0], r[1], r[2], r[3]);
    std::vector<DWORD> mine(kCanvas * kCanvas);
    Picks picks(ctx.seed, 0x546869636B4C0000ull); // "ThickL"
    int n = 0;

    auto check = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t a8, bool bClipped) {
        const uint32_t color = picks.Bits() | 1;
        std::memset(canvas, 0, kCanvas * kCanvas * 4);
        std::fill(mine.begin(), mine.end(), 0);
        ctx.SetInputs("input " + std::to_string(n++) + ": DrawThickLine32(" + std::to_string(x1) + ", "
            + std::to_string(y1) + ", " + std::to_string(x2) + ", " + std::to_string(y2) + ", a8 "
            + std::to_string(a8) + ", bClipped " + std::to_string(bClipped) + ")");
        // pSurface is the surface rectangle's top left (lPitch in pixels).
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            originalResult = reinterpret_cast<ThickLineFn>(kDrawThickLine32)(self, nullptr, x1, y1, x2, y2,
                canvas + kMargin * kCanvas + kMargin, kCanvas, rect, a8, color, bClipped ? 1u : 0u);
        });
        const BOOL oursResult = ours.DrawThickLine32(x1, y1, x2, y2, mine.data() + kMargin * kCanvas + kMargin,
            kCanvas, rSurface, a8, color, bClipped ? TRUE : FALSE);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, canvas, mine.data(), kCanvas * kCanvas);
        }
    };

    const int32_t lo = kMargin;
    const int32_t hi = kCanvas - kMargin - 1;
    for (int32_t a8 = 0; a8 <= 9; ++a8) {
        const int32_t half = (a8 + 1) >> 1;
        // Inside the rectangle shrunk by half the thickness, for bClipped.
        const int32_t in0 = lo + half;
        const int32_t in1 = hi - half;
        for (int i = 0; i < 8; ++i) {
            const int32_t x = picks.Between(in0, in1);
            const int32_t y = picks.Between(in0, in1);
            const int32_t len = picks.Between(0, 20);
            check(x, y, x, picks.Between(in0, in1), a8, true); // vertical
            check(x, y, picks.Between(in0, in1), y, a8, true); // horizontal
            const int32_t dx = picks.Between(0, 1) ? 1 : -1;
            const int32_t dy = picks.Between(0, 1) ? 1 : -1;
            int32_t x2 = x + dx * len;
            int32_t y2 = y + dy * len;
            if (x2 >= in0 && x2 <= in1 && y2 >= in0 && y2 <= in1) {
                check(x, y, x2, y2, a8, true); // diagonal
            }
            check(x, y, picks.Between(in0, in1), picks.Between(in0, in1), a8, true); // general
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t a8 = picks.Between(0, 9);
        const int32_t x1 = picks.Between(0, kCanvas - 1);
        const int32_t y1 = picks.Between(0, kCanvas - 1);
        const int32_t x2 = picks.Between(0, 3) == 0 ? x1 : picks.Between(0, kCanvas - 1);
        const int32_t y2 = picks.Between(0, 3) == 0 ? y1 : picks.Between(0, kCanvas - 1);
        check(x1, y1, x2, y2, a8, false);
    }
}

// CVidMode::DrawThickLine16 (0x799280) and DrawThickLine24 (0x7CA330),
// `ret 0x28`, with their slice helpers 0x79A950 and 0x7CA630; and
// DrawLine24 (0x7CA0D0), `ret 0x24`, with its run-slice helper 0x7CA240.
// None reads a member; lPitch is in pixels for 16 bits and in bytes for 24.
// Unreachable in iwd2r (32 bpp only).
using ThickLineAnyFn = int32_t(__fastcall*)(void* self, void* edx, int32_t nXFrom, int32_t nYFrom, int32_t nXTo,
    int32_t nYTo, void* pSurface, int32_t lPitch, const void* rSurface, int32_t a8, uint32_t color,
    uint32_t bClipped);
using DrawLine24Fn = int32_t(__fastcall*)(void* self, void* edx, int32_t nXFrom, int32_t nYFrom, int32_t nXTo,
    int32_t nYTo, void* pSurface, int32_t lPitch, const void* rSurface, uint32_t color, uint32_t bClipped);
constexpr uintptr_t kDrawThickLine16 = 0x799280;
constexpr uintptr_t kDrawThickLine24 = 0x7CA330;
constexpr uintptr_t kDrawLine24 = 0x7CA0D0;

// A 16- or 24-bit thick line (a8 0 to 9) or a 24-bit plain line (a8 -1) on
// the 96x96 canvas with a 16-pixel margin, as ThickLine32: vertical,
// horizontal, diagonal and general lines in every direction inside the
// rectangle (bClipped TRUE), then random ones clipped by the function. The
// whole canvas, byte by byte, and the result are compared.
void LineDepth(CaseContext& ctx, int nBpp, bool thick, uint64_t salt)
{
    const int bytes = nBpp / 8;
    const size_t size = static_cast<size_t>(kCanvas * kCanvas * bytes);
    uint8_t* self = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* rect = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(kVidInfSize);
        canvas = OriginalBlock(size);
        rect = OriginalBlock(16);
    });
    const int32_t r[4] = { kMargin, kMargin, kCanvas - kMargin, kCanvas - kMargin };
    std::memcpy(rect, r, sizeof(r));
    CVidInf ours;
    CRect rSurface(r[0], r[1], r[2], r[3]);
    std::vector<uint8_t> mine(size);
    Picks picks(ctx.seed, salt);
    const size_t origin = static_cast<size_t>((kMargin * kCanvas + kMargin) * bytes);
    const int32_t lPitch = nBpp == 16 ? kCanvas : kCanvas * 3;
    const std::string name = std::string(thick ? "DrawThickLine" : "DrawLine") + std::to_string(nBpp);
    int n = 0;

    auto check = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t a8, bool bClipped) {
        uint32_t color = picks.Bits() | 1;
        if (nBpp == 16) {
            color &= 0xFFFF;
        }
        std::memset(canvas, 0, size);
        std::fill(mine.begin(), mine.end(), static_cast<uint8_t>(0));
        ctx.SetInputs("input " + std::to_string(n++) + ": " + name + "(" + std::to_string(x1) + ", "
            + std::to_string(y1) + ", " + std::to_string(x2) + ", " + std::to_string(y2) + ", a8 "
            + std::to_string(a8) + ", color " + Hex(color) + ", bClipped " + std::to_string(bClipped) + ")");
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            if (thick) {
                originalResult = reinterpret_cast<ThickLineAnyFn>(nBpp == 16 ? kDrawThickLine16 : kDrawThickLine24)(
                    self, nullptr, x1, y1, x2, y2, canvas + origin, lPitch, rect, a8, color, bClipped ? 1u : 0u);
            } else {
                originalResult = reinterpret_cast<DrawLine24Fn>(kDrawLine24)(self, nullptr, x1, y1, x2, y2,
                    canvas + origin, lPitch, rect, color, bClipped ? 1u : 0u);
            }
        });
        BOOL oursResult;
        if (!thick) {
            oursResult = ours.DrawLine24(x1, y1, x2, y2, mine.data() + origin, lPitch, rSurface, color,
                bClipped ? TRUE : FALSE);
        } else if (nBpp == 16) {
            oursResult = ours.DrawThickLine16(x1, y1, x2, y2, reinterpret_cast<WORD*>(mine.data() + origin), lPitch,
                rSurface, a8, static_cast<WORD>(color), bClipped ? TRUE : FALSE);
        } else {
            oursResult = ours.DrawThickLine24(x1, y1, x2, y2, mine.data() + origin, lPitch, rSurface, a8, color,
                bClipped ? TRUE : FALSE);
        }
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            for (size_t i = 0; i < size; ++i) {
                if (canvas[i] != mine[i]) {
                    ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                    break;
                }
            }
        }
    };

    const int32_t lo = kMargin;
    const int32_t hi = kCanvas - kMargin - 1;
    for (int32_t a8 = thick ? 0 : -1; a8 <= (thick ? 9 : -1); ++a8) {
        const int32_t half = thick ? (a8 + 1) >> 1 : 0;
        const int32_t in0 = lo + half;
        const int32_t in1 = hi - half;
        for (int i = 0; i < (thick ? 8 : 64); ++i) {
            const int32_t x = picks.Between(in0, in1);
            const int32_t y = picks.Between(in0, in1);
            const int32_t len = picks.Between(0, 20);
            check(x, y, x, picks.Between(in0, in1), a8, true); // vertical
            check(x, y, picks.Between(in0, in1), y, a8, true); // horizontal
            const int32_t dx = picks.Between(0, 1) ? 1 : -1;
            const int32_t dy = picks.Between(0, 1) ? 1 : -1;
            const int32_t x2 = x + dx * len;
            const int32_t y2 = y + dy * len;
            if (x2 >= in0 && x2 <= in1 && y2 >= in0 && y2 <= in1) {
                check(x, y, x2, y2, a8, true); // diagonal
            }
            check(x, y, picks.Between(in0, in1), picks.Between(in0, in1), a8, true); // general
        }
    }
    for (int i = 0; i < (thick ? ctx.iterations : ctx.iterations / 4); ++i) {
        const int32_t a8 = thick ? picks.Between(0, 9) : -1;
        const int32_t x1 = picks.Between(0, kCanvas - 1);
        const int32_t y1 = picks.Between(0, kCanvas - 1);
        const int32_t x2 = picks.Between(0, 3) == 0 ? x1 : picks.Between(0, kCanvas - 1);
        const int32_t y2 = picks.Between(0, 3) == 0 ? y1 : picks.Between(0, kCanvas - 1);
        check(x1, y1, x2, y2, a8, false);
    }
}

void ThickLine16(CaseContext& ctx)
{
    LineDepth(ctx, 16, true, 0x546869636B313600ull); // "Thick16"
}

void ThickLine24(CaseContext& ctx)
{
    LineDepth(ctx, 24, true, 0x546869636B323400ull); // "Thick24"
}

void DrawLine24(CaseContext& ctx)
{
    LineDepth(ctx, 24, false, 0x4C696E6532340000ull); // "Line24"
}

// CVidMode::ApplyBrightnessContrast(rgb) (0x795E00), `ret 4` at 0x795F02:
// reads m_nBrightnessCorrection (0xA8) and m_nGammaCorrection (0xA9).
using BrightnessFn = uint32_t(__fastcall*)(void* self, void* edx, uint32_t rgb);
constexpr uintptr_t kApplyBrightnessContrast = 0x795E00;

// ApplyBrightnessContrast over every brightness and gamma byte with random
// colours (and black, which RealizeRange passes). The video mode is a
// CVidInf from the original's constructor.
void BrightnessContrast(CaseContext& ctx)
{
    uint8_t* mode = nullptr;
    ctx.SetInputs("building the original's video mode");
    ctx.CallOriginal([&] {
        mode = OriginalBlock(kVidInfSize);
        reinterpret_cast<VidInfCtorFn>(kVidInfCtor)(mode, nullptr);
    });
    CVidInf ours;
    Picks picks(ctx.seed, 0x4272696768740000ull); // "Bright"
    for (int brightness = 0; brightness < 256; ++brightness) {
        for (int gamma = 0; gamma < 256; ++gamma) {
            for (int i = 0; i < 4; ++i) {
                const uint32_t rgb = i == 0 ? 0u : (picks.Bits() & 0xFFFFFF);
                ctx.SetInputs("brightness " + std::to_string(brightness) + ", gamma " + std::to_string(gamma)
                    + ", rgb " + Hex(rgb));
                uint32_t original = 0;
                ctx.CallOriginal([&] {
                    Put<uint8_t>(mode, kBrightnessOffset, static_cast<uint8_t>(brightness));
                    Put<uint8_t>(mode, kGammaOffset, static_cast<uint8_t>(gamma));
                    original = reinterpret_cast<BrightnessFn>(kApplyBrightnessContrast)(mode, nullptr, rgb);
                });
                ours.m_nBrightnessCorrection = static_cast<BYTE>(brightness);
                ours.m_nGammaCorrection = static_cast<BYTE>(gamma);
                const uint32_t mine = ours.ApplyBrightnessContrast(rgb);
                ctx.CountInputs(1);
                if (original != mine) {
                    ctx.Fail(ctx.Inputs(), Hex(original), Hex(mine));
                }
            }
        }
    }
}

// CVidMode::DrawLightingEffect32(pSurface, lPitch, ptCenter, size, rgbLight,
// rgbAmbient) (0x7CAC60), `ret 0x18` at 0x7CB4FE. It reads rgbGlobalTint
// (0xA4) and calls GetEllipseArcPixelList through the vtable (0xC0).
using LightingFn = void(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch, const void* pt,
    const void* size, uint32_t a6, uint32_t a7);
constexpr uintptr_t kDrawLightingEffect32 = 0x7CAC60;
constexpr int kLightCanvas = 200;

// DrawLightingEffect32 on a 200x200 canvas of random pixels, centred, with
// half-axes 1 to 40 (height 0 draws nothing; width 0 divides by zero in the
// original, 0x7CB20C), random light and ambient
// colours, and the global tint none or random. The whole canvas is
// compared. The video mode is a CVidInf from the original's constructor.
void LightingEffect32(CaseContext& ctx)
{
    uint8_t* mode = nullptr;
    uint32_t* canvas = nullptr;
    uint8_t* pt = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's video mode and canvas");
    ctx.CallOriginal([&] {
        mode = OriginalBlock(kVidInfSize);
        reinterpret_cast<VidInfCtorFn>(kVidInfCtor)(mode, nullptr);
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(kLightCanvas * kLightCanvas * 4));
        pt = OriginalBlock(8);
        size = OriginalBlock(8);
    });
    CVidInf ours;
    std::vector<DWORD> mine(kLightCanvas * kLightCanvas);
    Picks picks(ctx.seed, 0x4C69676874000000ull); // "Light"
    const int count = 200 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        const int32_t cx = picks.Between(1, 40);
        const int32_t cy = n < 4 ? 0 : picks.Between(1, 40);
        const int32_t x = picks.Between(80, 120);
        const int32_t y = picks.Between(80, 120);
        const uint32_t a6 = picks.Bits() & 0xFFFFFF;
        const uint32_t a7 = picks.Bits() & 0xFFFFFF;
        const uint32_t tint = picks.Between(0, 1) ? 0xFFFFFFu : (picks.Bits() & 0xFFFFFF);
        for (DWORD& pixel : mine) {
            pixel = picks.Bits();
        }
        char inputs[160];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: centre (%d, %d), axes (%d, %d), light 0x%06X, ambient 0x%06X, global tint 0x%06X", n, x, y,
            cx, cy, a6, a7, tint);
        ctx.SetInputs(inputs);
        const int32_t p[2] = { x, y };
        const int32_t sz[2] = { cx, cy };
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size() * 4);
            std::memcpy(pt, p, sizeof(p));
            std::memcpy(size, sz, sizeof(sz));
            Put<uint32_t>(mode, kGlobalTintOffset, tint);
            reinterpret_cast<LightingFn>(kDrawLightingEffect32)(mode, nullptr, canvas, kLightCanvas, pt, size, a6,
                a7);
        });
        ours.rgbGlobalTint = tint;
        ours.DrawLightingEffect32(mine.data(), kLightCanvas, CPoint(x, y), CSize(cx, cy), a6, a7);
        ctx.CountInputs(1);
        CompareArrays(ctx, canvas, mine.data(), kLightCanvas * kLightCanvas);
    }
}

// CVidMode::DrawLightingEffect24(pSurface, lPitch, ptCenter, size, rgbLight,
// rgbAmbient) (0x7C8F70), `ret 0x18` at 0x7C997E. Three bytes per pixel, R at
// +0, G at +1, B at +2, lPitch in bytes; it reads rgbGlobalTint (0xA4) and
// calls GetEllipseArcPixelList through the vtable (0xC0), as the 32-bit
// sibling does.
using Lighting24Fn = void(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch, const void* pt,
    const void* size, uint32_t a6, uint32_t a7);
constexpr uintptr_t kDrawLightingEffect24 = 0x7C8F70;
constexpr int kLightCanvas24 = 200;
constexpr int kLightPitch24 = kLightCanvas24 * 3;

// DrawLightingEffect24 on a 200x200 canvas of random bytes (three per
// pixel, lPitch in bytes), centred, with half-axes 1 to 40 (height 0 draws
// nothing; width 0 divides by zero in the original, 0x7C9612), random light
// and ambient colours, and the global tint none or random. The whole canvas
// is compared, byte by byte. The video mode is a CVidInf from the original's
// constructor.
void LightingEffect24(CaseContext& ctx)
{
    uint8_t* mode = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* pt = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's video mode and canvas");
    ctx.CallOriginal([&] {
        mode = OriginalBlock(kVidInfSize);
        reinterpret_cast<VidInfCtorFn>(kVidInfCtor)(mode, nullptr);
        canvas = reinterpret_cast<uint8_t*>(OriginalBlock(static_cast<size_t>(kLightPitch24) * kLightCanvas24));
        pt = OriginalBlock(8);
        size = OriginalBlock(8);
    });
    CVidInf ours;
    std::vector<uint8_t> mine(static_cast<size_t>(kLightPitch24) * kLightCanvas24);
    Picks picks(ctx.seed, 0x4C69676874323400ull); // "Light24"
    const int count = 200 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        const int32_t cx = picks.Between(1, 40);
        const int32_t cy = n < 4 ? 0 : picks.Between(1, 40);
        const int32_t x = picks.Between(80, 120);
        const int32_t y = picks.Between(80, 120);
        const uint32_t a6 = picks.Bits() & 0xFFFFFF;
        const uint32_t a7 = picks.Bits() & 0xFFFFFF;
        const uint32_t tint = picks.Between(0, 1) ? 0xFFFFFFu : (picks.Bits() & 0xFFFFFF);
        for (uint8_t& byte : mine) {
            byte = static_cast<uint8_t>(picks.Bits());
        }
        char inputs[160];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: centre (%d, %d), axes (%d, %d), light 0x%06X, ambient 0x%06X, global tint 0x%06X", n, x, y,
            cx, cy, a6, a7, tint);
        ctx.SetInputs(inputs);
        const int32_t p[2] = { x, y };
        const int32_t sz[2] = { cx, cy };
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size());
            std::memcpy(pt, p, sizeof(p));
            std::memcpy(size, sz, sizeof(sz));
            Put<uint32_t>(mode, kGlobalTintOffset, tint);
            reinterpret_cast<Lighting24Fn>(kDrawLightingEffect24)(mode, nullptr, canvas, kLightPitch24, pt, size, a6,
                a7);
        });
        ours.rgbGlobalTint = tint;
        ours.DrawLightingEffect24(mine.data(), kLightPitch24, CPoint(x, y), CSize(cx, cy), a6, a7);
        ctx.CountInputs(1);
        for (size_t i = 0; i < mine.size(); ++i) {
            if (canvas[i] != mine[i]) {
                ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                break;
            }
        }
    }
}

// The video mode both sides read.
struct ModeValues {
    uint8_t fade;
    uint32_t globalTint;
    uint8_t brightness;
    uint8_t gamma;
};

// The chitin and current video mode both sides read through g_pChitin
// (header comment): the original's zeroed chitin and engine blocks and a
// CVidInf from its constructor, at 32 bpp with shifts 16/8/0; ours the same
// from zeroed CChitin and CWarp storage and our CVidInf. g_pChitin is put
// back when the fixture goes; our CVidInf is left alive (its destructor is
// not part of any case).
struct VideoFixture {
    explicit VideoFixture(CaseContext& ctx)
    {
        ctx.SetInputs("building the original's chitin and video mode");
        ctx.CallOriginal([&] {
            chitin = OriginalBlock(kChitinSize);
            engine = OriginalBlock(kEngineSize);
            mode = OriginalBlock(kVidInfSize);
            reinterpret_cast<VidInfCtorFn>(kVidInfCtor)(mode, nullptr);
            Put<uint32_t>(engine, kEngineVidModeOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mode)));
            Put<uint32_t>(chitin, kActiveEngineOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(engine)));
            Put<uint32_t>(chitin, kIs3dOffset, 0);
            Put<uint16_t>(chitin, kBppOffset, 32);
            Put<uint32_t>(mode, kRShiftOffset, 16);
            Put<uint32_t>(mode, kGShiftOffset, 8);
            Put<uint32_t>(mode, kBShiftOffset, 0);
            *reinterpret_cast<uint32_t*>(kChitin) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(chitin));
        });

        oursChitinStorage = static_cast<unsigned char*>(::operator new(sizeof(CChitin)));
        std::memset(oursChitinStorage, 0, sizeof(CChitin));
        oursEngineStorage = static_cast<unsigned char*>(::operator new(sizeof(CWarp)));
        std::memset(oursEngineStorage, 0, sizeof(CWarp));
        oursChitin = reinterpret_cast<CChitin*>(oursChitinStorage);
        auto* oursEngine = reinterpret_cast<CWarp*>(oursEngineStorage);
        oursMode = new CVidInf();
        oursMode->m_dwRBitShift = 16;
        oursMode->m_dwGBitShift = 8;
        oursMode->m_dwBBitShift = 0;
        oursEngine->pVidMode = oursMode;
        oursChitin->pActiveEngine = oursEngine;
        oursChitin->cVideo.m_nBpp = 32;
        g_pChitin = oursChitin;
    }
    ~VideoFixture()
    {
        g_pChitin = saved;
        ::operator delete(oursEngineStorage);
        ::operator delete(oursChitinStorage);
    }
    VideoFixture(const VideoFixture&) = delete;
    VideoFixture& operator=(const VideoFixture&) = delete;

    CChitin* saved = g_pChitin;
    uint8_t* chitin = nullptr;
    uint8_t* engine = nullptr;
    uint8_t* mode = nullptr;
    unsigned char* oursChitinStorage = nullptr;
    unsigned char* oursEngineStorage = nullptr;
    CChitin* oursChitin = nullptr;
    CVidInf* oursMode = nullptr;
};

// The current video mode on both sides as a 16-bit one: 5-6-5 (green6) or
// 5-5-5 masks, shifts and dropped bits as CVidInf::CreateSurfaces sets them.
void SetMode16(VideoFixture& video, CaseContext& ctx, bool green6)
{
    const uint32_t gBits = green6 ? 6 : 5;
    const uint32_t rShift = 5 + gBits;
    const uint32_t rMask = 0x1Fu << rShift;
    const uint32_t gMask = ((1u << gBits) - 1) << 5;
    const uint32_t bMask = 0x1F;
    ctx.CallOriginal([&] {
        Put<uint32_t>(video.mode, kRMaskOffset, rMask);
        Put<uint32_t>(video.mode, kGMaskOffset, gMask);
        Put<uint32_t>(video.mode, kBMaskOffset, bMask);
        Put<uint32_t>(video.mode, kRShiftOffset, rShift);
        Put<uint32_t>(video.mode, kGShiftOffset, 5);
        Put<uint32_t>(video.mode, kBShiftOffset, 0);
        Put<uint32_t>(video.mode, kRDropOffset, 3);
        Put<uint32_t>(video.mode, kGDropOffset, 8 - gBits);
        Put<uint32_t>(video.mode, kBDropOffset, 3);
        Put<uint16_t>(video.chitin, kBppOffset, 16);
    });
    video.oursMode->m_dwRBitMask = rMask;
    video.oursMode->m_dwGBitMask = gMask;
    video.oursMode->m_dwBBitMask = bMask;
    video.oursMode->m_dwRBitShift = rShift;
    video.oursMode->m_dwGBitShift = 5;
    video.oursMode->m_dwBBitShift = 0;
    video.oursMode->field_C2 = 3;
    video.oursMode->field_C6 = 8 - gBits;
    video.oursMode->field_CA = 3;
    video.oursChitin->cVideo.m_nBpp = 16;
}

// CVidMode::DrawLightingEffect16(pSurface, lPitch, ptCenter, size, rgbLight,
// rgbAmbient) (0x7968C0), `ret 0x18` at 0x7977C6. One WORD per pixel, lPitch
// in pixels; the channels come from the mode's masks (+0x18/+0x1C/+0x20) and
// are clamped at the mask's value, so the case runs 5-6-5 and 5-5-5 in turn
// (SetMode16). It reads rgbGlobalTint (0xA4) and calls
// GetEllipseArcPixelList through the vtable (0xC0), as the 32-bit sibling.
using Lighting16Fn = void(__fastcall*)(void* self, void* edx, uint16_t* pSurface, int32_t lPitch, const void* pt,
    const void* size, uint32_t a6, uint32_t a7);
constexpr uintptr_t kDrawLightingEffect16 = 0x7968C0;
constexpr int kLightCanvas16 = 200;

// DrawLightingEffect16 on a 200x200 canvas of random pixels, centred, with
// half-axes 1 to 40 (height 0 draws nothing; width 0 divides by zero in the
// original, 0x7971E0), random light and ambient colours, the global tint
// none or random, and the mode's masks alternating 5-6-5 / 5-5-5. The whole
// canvas is compared, word by word.
void LightingEffect16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint16_t* canvas = nullptr;
    uint8_t* pt = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        canvas = reinterpret_cast<uint16_t*>(OriginalBlock(kLightCanvas16 * kLightCanvas16 * 2));
        pt = OriginalBlock(8);
        size = OriginalBlock(8);
    });
    std::vector<uint16_t> mine(kLightCanvas16 * kLightCanvas16);
    Picks picks(ctx.seed, 0x4C69676874313600ull); // "Light16"
    const int count = 200 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        const bool green6 = picks.Between(0, 1) != 0;
        SetMode16(video, ctx, green6);
        const int32_t cx = picks.Between(1, 40);
        const int32_t cy = n < 4 ? 0 : picks.Between(1, 40);
        const int32_t x = picks.Between(80, 120);
        const int32_t y = picks.Between(80, 120);
        const uint32_t a6 = picks.Bits() & 0xFFFFFF;
        const uint32_t a7 = picks.Bits() & 0xFFFFFF;
        const uint32_t tint = picks.Between(0, 1) ? 0xFFFFFFu : (picks.Bits() & 0xFFFFFF);
        for (uint16_t& pixel : mine) {
            pixel = static_cast<uint16_t>(picks.Bits());
        }
        char inputs[192];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %s, centre (%d, %d), axes (%d, %d), light 0x%06X, ambient 0x%06X, global tint 0x%06X", n,
            green6 ? "5-6-5" : "5-5-5", x, y, cx, cy, a6, a7, tint);
        ctx.SetInputs(inputs);
        const int32_t p[2] = { x, y };
        const int32_t sz[2] = { cx, cy };
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size() * 2);
            std::memcpy(pt, p, sizeof(p));
            std::memcpy(size, sz, sizeof(sz));
            Put<uint32_t>(video.mode, kGlobalTintOffset, tint);
            reinterpret_cast<Lighting16Fn>(kDrawLightingEffect16)(video.mode, nullptr, canvas, kLightCanvas16, pt, size,
                a6, a7);
        });
        video.oursMode->rgbGlobalTint = tint;
        video.oursMode->DrawLightingEffect16(mine.data(), kLightCanvas16, CPoint(x, y), CSize(cx, cy), a6, a7);
        ctx.CountInputs(1);
        for (size_t i = 0; i < mine.size(); ++i) {
            if (canvas[i] != mine[i]) {
                ctx.Fail(ctx.Inputs() + ", pixel " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                break;
            }
        }
    }
}

// One 16-, 24- or 32-bit span function (solid, dithered, shaded, mirrored
// or not) on a 64x1 row of random pixels, as HLineShaded does for 32 bits:
// every span once with the widest surface rectangle, then random spans and
// rectangles, ptRef random for the dither parity. The 16-bit forms run on a
// 5-6-5 and a 5-5-5 video mode in turn; the colours are random 32-bit values
// (the 16-bit solid spans write pixel pairs from dwColor | dwColor << 16).
void HLineDepth(CaseContext& ctx, uintptr_t address, CVidPoly::DrawHLineFunc oursFn, const char* fnName, int nBpp,
    uint64_t salt)
{
    VideoFixture video(ctx);
    const int bytes = nBpp / 8;
    uint8_t* poly = nullptr;
    uint8_t* row = nullptr;
    uint8_t* rect = nullptr;
    uint8_t* ref = nullptr;
    ctx.SetInputs("building the original's buffers");
    ctx.CallOriginal([&] {
        poly = OriginalBlock(kVidPolySize);
        row = OriginalBlock(static_cast<size_t>(kLineWidth * bytes));
        rect = OriginalBlock(16);
        ref = OriginalBlock(8);
    });
    CVidPoly ours;
    Picks picks(ctx.seed, salt);
    const std::string name = fnName;

    auto check = [&](int32_t xMin, int32_t xMax, uint32_t color, int32_t left, int32_t right) {
        std::vector<uint8_t> pixels(static_cast<size_t>(kLineWidth * bytes));
        for (size_t i = 0; i < pixels.size(); ++i) {
            // Dark bytes (sum below 32 in the 24-bit form) and bright ones.
            pixels[i] = static_cast<uint8_t>(picks.Bits() >> picks.Between(0, 31));
        }
        std::memcpy(row, pixels.data(), pixels.size());
        const int32_t r[4] = { left, 0, right, 1 };
        std::memcpy(rect, r, sizeof(r));
        const int32_t pt[2] = { picks.Between(0, 7), picks.Between(0, 7) };
        std::memcpy(ref, pt, sizeof(pt));
        ctx.SetInputs(name + "(" + std::to_string(xMin) + ", " + std::to_string(xMax) + ", " + Hex(color)
            + ", rSurface " + std::to_string(left) + ".." + std::to_string(right) + ")");
        ctx.CallOriginal([&] {
            reinterpret_cast<HLineFn>(address)(poly, nullptr, row, xMin, xMax, color, rect, ref);
        });
        const CRect rSurface(left, 0, right, 1);
        const CPoint ptRef(pt[0], pt[1]);
        (ours.*oursFn)(pixels.data(), xMin, xMax, color, rSurface, ptRef);
        ctx.CountInputs(1);
        for (size_t i = 0; i < pixels.size(); ++i) {
            if (row[i] != pixels[i]) {
                ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(row[i]), Hex(pixels[i]));
                break;
            }
        }
    };

    for (int format = 0; format < (nBpp == 16 ? 2 : 1); ++format) {
        if (nBpp == 16) {
            SetMode16(video, ctx, format == 0);
        }
        for (int32_t xMin = 0; xMin < kLineWidth; ++xMin) {
            for (int32_t xMax = xMin - 1; xMax < kLineWidth; ++xMax) {
                check(xMin, xMax, picks.Bits(), 0, kLineWidth - 1);
            }
        }
        for (int i = 0; i < ctx.iterations; ++i) {
            const int32_t xMin = picks.Between(0, kLineWidth - 1);
            const int32_t xMax = picks.Between(xMin - 2, kLineWidth - 1);
            const int32_t width = picks.Between(xMax < xMin ? xMin : xMax, kLineWidth - 1);
            const int32_t left = picks.Between(-100, 100);
            check(xMin, xMax, picks.Bits(), left, left + width);
        }
    }
}

void HLineShaded16(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineShaded16, &CVidPoly::DrawHLineShaded16, "DrawHLineShaded16", 16,
        0x484C533136000000ull); // "HLS16"
}

void HLineShadedMirrored16(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineShadedMirrored16, &CVidPoly::DrawHLineShadedMirrored16, "DrawHLineShadedMirrored16", 16,
        0x484C4D3136000000ull); // "HLM16"
}

void HLineShaded24(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineShaded24, &CVidPoly::DrawHLineShaded24, "DrawHLineShaded24", 24,
        0x484C533234000000ull); // "HLS24"
}

void HLineShadedMirrored24(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineShadedMirrored24, &CVidPoly::DrawHLineShadedMirrored24, "DrawHLineShadedMirrored24", 24,
        0x484C4D3234000000ull); // "HLM24"
}

void HLine16(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLine16, &CVidPoly::DrawHLine16, "DrawHLine16", 16, 0x484C303136000000ull); // "HL016"
}

void HLineMirrored16(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineMirrored16, &CVidPoly::DrawHLineMirrored16, "DrawHLineMirrored16", 16,
        0x484C4D3136300000ull); // "HLM160"
}

void HLine24(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLine24, &CVidPoly::DrawHLine24, "DrawHLine24", 24, 0x484C303234000000ull); // "HL024"
}

void HLineMirrored24(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineMirrored24, &CVidPoly::DrawHLineMirrored24, "DrawHLineMirrored24", 24,
        0x484C4D3234300000ull); // "HLM240"
}

void HLineDithered16(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineDithered16, &CVidPoly::DrawHLineDithered16, "DrawHLineDithered16", 16,
        0x484C443136000000ull); // "HLD16"
}

void HLineDitheredMirrored16(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineDitheredMirrored16, &CVidPoly::DrawHLineDitheredMirrored16,
        "DrawHLineDitheredMirrored16", 16, 0x484C444D31360000ull); // "HLDM16"
}

void HLineDithered24(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineDithered24, &CVidPoly::DrawHLineDithered24, "DrawHLineDithered24", 24,
        0x484C443234000000ull); // "HLD24"
}

void HLineDitheredMirrored24(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineDitheredMirrored24, &CVidPoly::DrawHLineDitheredMirrored24,
        "DrawHLineDitheredMirrored24", 24, 0x484C444D32340000ull); // "HLDM24"
}

void HLineDithered32(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineDithered32, &CVidPoly::DrawHLineDithered32, "DrawHLineDithered32", 32,
        0x484C443332000000ull); // "HLD32"
}

void HLineDitheredMirrored32(CaseContext& ctx)
{
    HLineDepth(ctx, kDrawHLineDitheredMirrored32, &CVidPoly::DrawHLineDitheredMirrored32,
        "DrawHLineDitheredMirrored32", 32, 0x484C444D33320000ull); // "HLDM32"
}

// CVidBitmap::BltBmp24To24(pSurface, lPitch, pData, bmpSize, nDataJump,
// dwFlags) (0x7CCBF0), `ret 0x18`: reads the video mode's shifts only.
// Unreachable in iwd2r (32 bpp only).
using BltBmpFn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch, const uint8_t* pData,
    const void* bmpSize, int32_t nDataJump, uint32_t dwFlags);
constexpr uintptr_t kBltBmp24To24 = 0x7CCBF0;

// 24-bit bitmaps 2 to 32 wide (the unkeyed loop never ends for width 1 in
// the original) and 1 to 32 high, drawn upward from the last row of a
// 64x40 24-bit canvas of random bytes, with random data jumps, plain, keyed
// (0x1) and keyed with shadows (0x5), the data mixing pure green, black and
// random pixels, under shifts 16/8/0 and 0/8/16. The whole canvas and the
// result are compared.
void BltBmp24To24(CaseContext& ctx)
{
    VideoFixture video(ctx);
    constexpr int kW = 64;
    constexpr int kH = 40;
    constexpr size_t kSize = kW * kH * 3;
    constexpr size_t kDataSize = 64 * 32 * 3;
    uint8_t* self = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* data = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(0x100);
        canvas = OriginalBlock(kSize);
        data = OriginalBlock(kDataSize);
        size = OriginalBlock(8);
    });
    auto* oursStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(oursStorage, 0, sizeof(CVidBitmap));
    auto* ours = reinterpret_cast<CVidBitmap*>(oursStorage);
    std::vector<uint8_t> mine(kSize);
    std::vector<uint8_t> pixels(kDataSize);
    Picks picks(ctx.seed, 0x426D703234000000ull); // "Bmp24"
    const int count = 600 + ctx.iterations / 10;
    for (int n = 0; n < count; ++n) {
        if (n == 0 || n == count / 2) {
            const uint32_t r = n == 0 ? 16 : 0;
            const uint32_t b = n == 0 ? 0 : 16;
            ctx.CallOriginal([&] {
                Put<uint32_t>(video.mode, kRShiftOffset, r);
                Put<uint32_t>(video.mode, kBShiftOffset, b);
            });
            video.oursMode->m_dwRBitShift = r;
            video.oursMode->m_dwBBitShift = b;
        }
        const int32_t cx = picks.Between(2, 32);
        const int32_t cy = picks.Between(1, 32);
        const int32_t jump = picks.Between(0, 8);
        const int32_t x = picks.Between(0, kW - cx);
        const int32_t y = picks.Between(cy - 1, kH - 1);
        const uint32_t flags[3] = { 0u, 1u, 5u };
        const uint32_t dwFlags = flags[picks.Between(0, 2)];
        for (size_t i = 0; i < kDataSize; i += 3) {
            const int kind = picks.Between(0, 5);
            if (kind == 0) {
                pixels[i] = 0;
                pixels[i + 1] = 0xFF;
                pixels[i + 2] = 0;
            } else if (kind == 1) {
                pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
            } else {
                pixels[i] = static_cast<uint8_t>(picks.Bits());
                pixels[i + 1] = static_cast<uint8_t>(picks.Bits());
                pixels[i + 2] = static_cast<uint8_t>(picks.Bits());
            }
        }
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        char inputs[200];
        std::snprintf(inputs, sizeof(inputs), "input %d: %dx%d, last row at (%d,%d), jump %d, flags 0x%X", n, cx, cy,
            x, y, jump, dwFlags);
        ctx.SetInputs(inputs);
        const size_t offset = static_cast<size_t>((y * kW + x) * 3);
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), kSize);
            std::memcpy(data, pixels.data(), kDataSize);
            const int32_t sz[2] = { cx, cy };
            std::memcpy(size, sz, 8);
            originalResult = reinterpret_cast<BltBmpFn>(kBltBmp24To24)(self, nullptr, canvas + offset, kW * 3, data,
                size, jump, dwFlags);
        });
        const BOOL oursResult = ours->BltBmp24To24(mine.data() + offset, kW * 3, pixels.data(), CSize(cx, cy), jump,
            dwFlags);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            for (size_t i = 0; i < kSize; ++i) {
                if (canvas[i] != mine[i]) {
                    ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                    break;
                }
            }
        }
    }
    ::operator delete(oursStorage);
}

// CVidBitmap::BltBmp24To24Tint(pSurface, lPitch, pData, bmpSize,
// nDataJump, dwFlags) (0x7CCE80), `ret 0x18`: reads the video mode's shifts,
// global tint, brightness and gamma, and the bitmap's m_paletteAffects (in
// the original at 0x28 - its 32-bit layout - for the tint, add and light
// colours here only).
constexpr uintptr_t kBltBmp24To24Tint = 0x7CCE80;
constexpr size_t kOriginalAffectOffset = 0x28;

// 24-bit bitmaps 2 to 32 wide (the unkeyed loop never ends below 2 in the
// original) and 1 to 32 high, drawn upward from the last row of a 64x40
// 24-bit canvas of random bytes, with random data jumps; plain, keyed (0x1)
// and keyed with shadows (0x5) with the grey (0x80000), tint (0x30000), add
// (0x40000) and light (0x200000) flags mixed in; data mixing pure green,
// black and random pixels; under shifts 16/8/0 and 0/8/16, with random tint,
// add and light colours and the mode's brightness, gamma and global tint.
// The whole canvas and the result are compared.
void BltBmp24To24Tint(CaseContext& ctx)
{
    VideoFixture video(ctx);
    constexpr int kW = 64;
    constexpr int kH = 40;
    constexpr size_t kSize = kW * kH * 3;
    constexpr size_t kDataSize = 64 * 32 * 3;
    uint8_t* self = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* data = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(0x100);
        canvas = OriginalBlock(kSize);
        data = OriginalBlock(kDataSize);
        size = OriginalBlock(8);
    });
    auto* oursStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(oursStorage, 0, sizeof(CVidBitmap));
    auto* ours = reinterpret_cast<CVidBitmap*>(oursStorage);
    std::vector<uint8_t> mine(kSize);
    std::vector<uint8_t> pixels(kDataSize);
    Picks picks(ctx.seed, 0x426D703334000000ull); // "Bmp34"
    const int count = 600 + ctx.iterations / 10;
    for (int format = 0; format < 2; ++format) {
        const uint32_t r = format == 0 ? 16 : 0;
        const uint32_t b = format == 0 ? 0 : 16;
        ctx.CallOriginal([&] {
            Put<uint32_t>(video.mode, kRShiftOffset, r);
            Put<uint32_t>(video.mode, kBShiftOffset, b);
        });
        video.oursMode->m_dwRBitShift = r;
        video.oursMode->m_dwBBitShift = b;
        for (int n = 0; n < count; ++n) {
            const uint32_t tint = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
            const uint32_t add = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
            const uint32_t light = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
            const uint8_t brightness = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 255) : 0);
            const uint8_t gamma = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 40) : 0);
            const uint32_t globalTint = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
            uint32_t dwFlags = (n % 3 == 0) ? 0u : ((n % 3 == 1) ? 1u : 5u);
            const uint32_t extra[5] = { 0x80000, 0x10000, 0x20000, 0x40000, 0x200000 };
            for (const uint32_t bit : extra) {
                if (picks.Between(0, 2) == 0) {
                    dwFlags |= bit;
                }
            }
            const int32_t cx = picks.Between(2, 32);
            const int32_t cy = picks.Between(1, 32);
            const int32_t jump = picks.Between(0, 8);
            const int32_t x = picks.Between(0, kW - cx);
            const int32_t y = picks.Between(cy - 1, kH - 1);
            for (size_t i = 0; i < kDataSize; i += 3) {
                const int kind = picks.Between(0, 5);
                if (kind == 0) {
                    pixels[i] = 0;
                    pixels[i + 1] = 0xFF;
                    pixels[i + 2] = 0;
                } else if (kind == 1) {
                    pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
                } else {
                    pixels[i] = static_cast<uint8_t>(picks.Bits());
                    pixels[i + 1] = static_cast<uint8_t>(picks.Bits());
                    pixels[i + 2] = static_cast<uint8_t>(picks.Bits());
                }
            }
            for (uint8_t& pixel : mine) {
                pixel = static_cast<uint8_t>(picks.Bits());
            }
            char inputs[240];
            std::snprintf(inputs, sizeof(inputs),
                "input %d (%s): %dx%d, last row at (%d,%d), jump %d, flags 0x%X, tint 0x%X, add 0x%X, "
                "light 0x%X, brightness %u, gamma %u, global tint 0x%X",
                n, format == 0 ? "16/8/0" : "0/8/16", cx, cy, x, y, jump, dwFlags, tint, add, light, brightness,
                gamma, globalTint);
            ctx.SetInputs(inputs);
            const size_t offset = static_cast<size_t>((y * kW + x) * 3);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(canvas, mine.data(), kSize);
                std::memcpy(data, pixels.data(), kDataSize);
                const int32_t sz[2] = { cx, cy };
                std::memcpy(size, sz, 8);
                Put<uint32_t>(self, kOriginalAffectOffset + 0x0, tint);
                Put<uint32_t>(self, kOriginalAffectOffset + 0x4, add);
                Put<uint32_t>(self, kOriginalAffectOffset + 0x8, light);
                Put<uint32_t>(video.mode, kGlobalTintOffset, globalTint);
                Put<uint8_t>(video.mode, kBrightnessOffset, brightness);
                Put<uint8_t>(video.mode, kGammaOffset, gamma);
                originalResult = reinterpret_cast<BltBmpFn>(kBltBmp24To24Tint)(self, nullptr, canvas + offset,
                    kW * 3, data, size, jump, dwFlags);
            });
            ours->m_paletteAffects.rgbTintColor = tint;
            ours->m_paletteAffects.rgbAddColor = add;
            ours->m_paletteAffects.rgbLightColor = light;
            video.oursMode->rgbGlobalTint = globalTint;
            video.oursMode->m_nBrightnessCorrection = brightness;
            video.oursMode->m_nGammaCorrection = gamma;
            const BOOL oursResult = ours->BltBmp24To24Tint(mine.data() + offset, kW * 3, pixels.data(),
                CSize(cx, cy), jump, dwFlags);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                for (size_t i = 0; i < kSize; ++i) {
                    if (canvas[i] != mine[i]) {
                        ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                        break;
                    }
                }
            }
        }
    }
    ::operator delete(oursStorage);
}

// CVidBitmap::BltBmp24To16(pSurface, lPitch, pData, bmpSize, nDataJump,
// dwFlags) (0x7B37A0), `ret 0x18`: reads the video mode's shifts (and, for
// the keyed form, its masks) only.
constexpr uintptr_t kBltBmp24To16 = 0x7B37A0;

// 24-bit bitmaps 1 to 32 wide (0 hits the original's single-pixel path,
// which then writes one pixel per row) and 1 to 32 high, drawn upward from
// the last row of a 64x40 16-bit canvas of random bytes, with random data
// jumps, plain, keyed (0x1) and keyed with shadows (0x5), the data mixing
// pure green, black and random pixels, on 5-6-5 and 5-5-5. The whole canvas
// and the result are compared.
void BltBmp24To16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    constexpr int kW = 64;
    constexpr int kH = 40;
    constexpr size_t kSize = kW * kH * 2;
    constexpr size_t kDataSize = 64 * 32 * 3;
    uint8_t* self = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* data = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(0x100);
        canvas = OriginalBlock(kSize);
        data = OriginalBlock(kDataSize);
        size = OriginalBlock(8);
    });
    auto* oursStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(oursStorage, 0, sizeof(CVidBitmap));
    auto* ours = reinterpret_cast<CVidBitmap*>(oursStorage);
    std::vector<uint8_t> mine(kSize);
    std::vector<uint8_t> pixels(kDataSize);
    Picks picks(ctx.seed, 0x426D703136000000ull); // "Bmp16"
    const int count = 600 + ctx.iterations / 10;
    for (int format = 0; format < 2; ++format) {
        SetMode16(video, ctx, format == 0);
        for (int n = 0; n < count; ++n) {
            // Widths 0 and 1 take the original's single-pixel path, which
            // faults under the rig (the runner reported an access violation
            // right after the call for a 1-byte-wide input), so the case
            // starts at 2; the port keeps that path with a NOTE and the unit
            // test covers widths 0 and 1.
            const int32_t cx = picks.Between(2, 32);
            const int32_t cy = picks.Between(1, 32);
            const int32_t jump = picks.Between(0, 8);
            const int32_t x = picks.Between(0, kW - cx);
            const int32_t y = picks.Between(cy - 1, kH - 1);
            const uint32_t flags[3] = { 0u, 1u, 5u };
            const uint32_t dwFlags = flags[picks.Between(0, 2)];
            for (size_t i = 0; i < kDataSize; i += 3) {
                const int kind = picks.Between(0, 5);
                if (kind == 0) {
                    pixels[i] = 0;
                    pixels[i + 1] = 0xFF;
                    pixels[i + 2] = 0;
                } else if (kind == 1) {
                    pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
                } else {
                    pixels[i] = static_cast<uint8_t>(picks.Bits());
                    pixels[i + 1] = static_cast<uint8_t>(picks.Bits());
                    pixels[i + 2] = static_cast<uint8_t>(picks.Bits());
                }
            }
            for (uint8_t& b : mine) {
                b = static_cast<uint8_t>(picks.Bits());
            }
            char inputs[200];
            std::snprintf(inputs, sizeof(inputs), "input %d (%s): %dx%d, last row at (%d,%d), jump %d, flags 0x%X",
                n, format == 0 ? "5-6-5" : "5-5-5", cx, cy, x, y, jump, dwFlags);
            ctx.SetInputs(inputs);
            const size_t offset = static_cast<size_t>((y * kW + x) * 2);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(canvas, mine.data(), kSize);
                std::memcpy(data, pixels.data(), kDataSize);
                const int32_t sz[2] = { cx, cy };
                std::memcpy(size, sz, 8);
                originalResult = reinterpret_cast<BltBmpFn>(kBltBmp24To16)(self, nullptr, canvas + offset, kW * 2,
                    data, size, jump, dwFlags);
            });
            const BOOL oursResult = ours->BltBmp24To16(reinterpret_cast<WORD*>(mine.data() + offset), kW * 2,
                pixels.data(), CSize(cx, cy), jump, dwFlags);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                for (size_t i = 0; i < kSize; ++i) {
                    if (canvas[i] != mine[i]) {
                        ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                        break;
                    }
                }
            }
        }
    }
    ::operator delete(oursStorage);
}

// CVidBitmap::BltBmp8To16 has no address of its own: CVidBitmap::RenderDirect
// (0x7B3190) carries it inlined in its 16-bpp branch, 0x7B362D-0x7B3735
// (SetPalette at 0x7B3660, Realize at 0x7B3680, then the loop). The original
// is therefore reached by calling RenderDirect itself, with an 8-bit
// CResBitmap in a constructed original CVidBitmap and a fake video mode whose
// one surface is a fabricated IDirectDrawSurface: its Lock (+0x64) hands back
// the locked rect's first row in the canvas and the canvas pitch, its Unlock
// (+0x80) does nothing - all LockSurface (0x799FA0) and UnLockSurface
// (0x79A820) call. The resource is not demanded (bDemanded = TRUE), so Parse
// never runs and the case fills the members it would set.
constexpr uintptr_t kRenderDirect = 0x7B3190;
// CVidBitmap::CVidBitmap() (0x7B2A40), plain ret: it builds the palette and
// the palette-affect defaults the arm's SetPalette/Realize read.
constexpr uintptr_t kVidBitmapCtor = 0x7B2A40;
constexpr size_t kVidBitmapSize = 0x100;
constexpr size_t kVidBitmapResOffset = 0xA4;
constexpr size_t kVidBitmapDoubleSizeOffset = 0xB6;
// CResBitmap members Parse fills.
constexpr size_t kResColorsOffset = 0x50;
constexpr size_t kResParsedOffset = 0x58;
constexpr size_t kResDataOffset = 0x5C;
constexpr size_t kResBitmapInfoOffset = 0x64;
constexpr size_t kResColorTableOffset = 0x68;
constexpr size_t kResBlockSize = 0x80;
constexpr size_t kBitmapInfoSize = 0x28;
constexpr size_t kModeSurfacesOffset = 0x9C;
constexpr size_t kModeSurfaceArrayOffset = 0xA0;
// DDSURFACEDESC (DX3): lPitch at 0x10, lpSurface at 0x24.
constexpr size_t kDescPitchOffset = 0x10;
constexpr size_t kDescSurfaceOffset = 0x24;

using VidBitmapCtorFn = void*(__fastcall*)(void* self, void* edx);
using RenderDirectFn = int32_t(__fastcall*)(void* self, void* edx, int32_t nSurface, int32_t x, int32_t y,
    const int32_t* rClip, uint32_t dwFlags, uint8_t bDemanded);

// The canvas the fabricated surface locks, and its geometry: the locked rect
// starts at (left, top) and rows are lPitch bytes.
uint8_t* g_lockedCanvas = nullptr;
int32_t g_lockedStride = 0;
int32_t g_lockedPitch = 0;

// IDirectDrawSurface::Lock(rect, desc, flags, hEvent) and Unlock(lpSurface),
// __stdcall as COM spells them, returning DD_OK.
int32_t __stdcall FakeLock(void*, const int32_t* rect, void* desc, uint32_t, void*)
{
    auto* at = static_cast<uint8_t*>(desc);
    Put<int32_t>(at, kDescPitchOffset, g_lockedPitch);
    Put<uint32_t>(at, kDescSurfaceOffset,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_lockedCanvas)
            + (rect[1] * g_lockedStride + rect[0]) * 2));
    return 0;
}

int32_t __stdcall FakeUnlock(void*, void*) { return 0; }

// Slot +0x64 is Lock and +0x80 Unlock; no other slot is called.
uintptr_t g_fakeSurfaceVTable[0x84 / 4];

void ResetFakeSurfaceVTable()
{
    for (uintptr_t& slot : g_fakeSurfaceVTable) {
        slot = reinterpret_cast<uintptr_t>(&FakeUnlock);
    }
    g_fakeSurfaceVTable[0x64 / 4] = reinterpret_cast<uintptr_t>(&FakeLock);
}

// 8-bit bitmaps 1 to 32 wide - width 1 takes the original's single-pixel
// path, 2 the pairs, wider odd widths the leading pixel then pairs - and 1 to
// 32 high, drawn upward from the last row of a 64x40 16-bit canvas of random
// bytes, on 5-6-5 and 5-5-5 alternating. The source rows are as long as
// RenderDirect's own nDataJump makes them; the 256-entry palette is random
// apart from a pure red, green and blue, so the realized entries exercise the
// rounding as well as the packing. The whole canvas and the result compare.
void BltBmp8To16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    constexpr int kW = 64;
    constexpr int kH = 40;
    constexpr size_t kSize = kW * kH * 2;
    constexpr size_t kDataSize = 64 * 32;
    constexpr int kColors = 256;

    uint8_t* self = nullptr;
    uint8_t* res = nullptr;
    uint8_t* info = nullptr;
    uint8_t* colorTable = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* data = nullptr;
    uint8_t* surface = nullptr;
    uint8_t* surfaces = nullptr;
    ctx.SetInputs("building the original's bitmap, surface and canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(kVidBitmapSize);
        reinterpret_cast<VidBitmapCtorFn>(kVidBitmapCtor)(self, nullptr);
        res = OriginalBlock(kResBlockSize);
        info = OriginalBlock(kBitmapInfoSize);
        colorTable = OriginalBlock(kColors * 4);
        canvas = OriginalBlock(kSize);
        data = OriginalBlock(kDataSize);
        surface = OriginalBlock(4);
        surfaces = OriginalBlock(4);
        ResetFakeSurfaceVTable();
        Put<uint32_t>(surface, 0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_fakeSurfaceVTable)));
        Put<uint32_t>(surfaces, 0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(surface)));
        Put<uint32_t>(video.mode, kModeSurfacesOffset, 1);
        Put<uint32_t>(video.mode, kModeSurfaceArrayOffset,
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(surfaces)));
        Put<uint32_t>(res, kResParsedOffset, TRUE);
        Put<int32_t>(res, kResColorsOffset, kColors);
        Put<uint32_t>(res, kResDataOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)));
        Put<uint32_t>(res, kResBitmapInfoOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info)));
        Put<uint32_t>(res, kResColorTableOffset,
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(colorTable)));
        Put<uint16_t>(info, 0x0E, 8); // biBitCount
        Put<uint32_t>(self, kVidBitmapResOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(self, kVidBitmapDoubleSizeOffset, 0);
    });

    // Ours: the resource outlives the call and everything is leaked, as the
    // other vid cases do.
    auto* oursStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    auto* ours = new (oursStorage) CVidBitmap;
    auto* oursRes = new CResBitmap;
    oursRes->bParsed = TRUE;
    oursRes->nColors = kColors;
    RGBQUAD colors[kColors];
    oursRes->pColorTable = colors;
    ours->SetRes(oursRes);

    std::vector<uint8_t> mine(kSize);
    std::vector<uint8_t> pixels(kDataSize);
    Picks picks(ctx.seed, 0x426D703816000000ull); // "Bmp8"
    const int count = 600 + ctx.iterations / 10;

    // The same palette on both sides: three pure channels and 253 random.
    for (int i = 0; i < kColors; ++i) {
        const uint32_t bits = (i == 0 ? 0x0000FFu : i == 1 ? 0x00FF00u
                : i == 2                                   ? 0xFF0000u
                                                           : picks.Bits());
        std::memcpy(&colors[i], &bits, sizeof(colors[i]));
    }
    ctx.CallOriginal([&] { std::memcpy(colorTable, colors, sizeof(colors)); });

    for (int n = 0; n < count; ++n) {
        SetMode16(video, ctx, (n & 1) == 0);
        const int32_t cx = picks.Between(1, 32);
        const int32_t cy = picks.Between(1, 32);
        const int32_t x = picks.Between(0, kW - cx);
        const int32_t y = picks.Between(0, kH - cy);
        // RenderDirect's nDataJump for a 1-byte-per-pixel source.
        const int32_t jump = (-((cx * 1) & 3)) & 3;
        for (uint8_t& index : pixels) {
            index = static_cast<uint8_t>(picks.Bits());
        }
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d (%s): %dx%d at (%d,%d), data jump %d, palette entry 0 red", n,
            (n & 1) == 0 ? "5-6-5" : "5-5-5", cx, cy, x, y, jump);
        ctx.SetInputs(inputs);
        const int32_t rClip[4] = { 0, 0, kW, kH };
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), kSize);
            std::memcpy(data, pixels.data(), kDataSize);
            Put<int32_t>(info, 4, cx); // biWidth
            Put<int32_t>(info, 8, cy); // biHeight
            g_lockedCanvas = canvas;
            g_lockedStride = kW;
            g_lockedPitch = kW * 2;
            originalResult = reinterpret_cast<RenderDirectFn>(kRenderDirect)(
                self, nullptr, 0, x, y, rClip, 0, 1);
        });
        const BOOL oursResult = ours->BltBmp8To16(
            reinterpret_cast<WORD*>(mine.data() + ((y + cy - 1) * kW + x) * 2), kW * 2,
            pixels.data(), CSize(cx, cy), jump, 0);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            for (size_t i = 0; i < kSize; ++i) {
                if (canvas[i] != mine[i]) {
                    ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                    break;
                }
            }
        }
    }
    ::operator delete(oursStorage);
}

// CVidBitmap::BltBmp24To16Tint(pSurface, lPitch, pData, bmpSize,
// nDataJump, dwFlags) (0x7B3B70), `ret 0x18`: the tinted 24-to-16 blit. It
// reads the mode's shifts, masks, global tint (0xA4), brightness (0xA8) and
// gamma (0xA9); the bitmap's own palette-affect ranges stay zero, so the
// tint, add and light come from those mode fields, set on both sides.
using BltBmp24To16TintFn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch,
    const uint8_t* pData, const void* bmpSize, int32_t nDataJump, uint32_t dwFlags);
constexpr uintptr_t kBltBmp24To16Tint = 0x7B3B70;

// 24-bit bitmaps 2 to 32 wide (0 and 1 take the original's single-pixel
// path, which faults under the rig), 1 to 32 high, drawn upward from the last
// row of a 64x40 16-bit canvas of random bytes, with random data jumps, the
// eight flag combinations the family uses (plain, tint, keyed, keyed with
// add, light, monochrome or shadow), random global tint, brightness and gamma
// each iteration, and pixel data mixing pure green, black and random bytes,
// on 5-6-5 and 5-5-5. The whole canvas and the result are compared.
void BltBmp24To16Tint(CaseContext& ctx)
{
    VideoFixture video(ctx);
    constexpr int kW = 64;
    constexpr int kH = 40;
    constexpr size_t kSize = kW * kH * 2;
    constexpr size_t kDataSize = 64 * 32 * 3;
    uint8_t* self = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* data = nullptr;
    uint8_t* size = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(0x100);
        canvas = OriginalBlock(kSize);
        data = OriginalBlock(kDataSize);
        size = OriginalBlock(8);
    });
    auto* oursStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
    std::memset(oursStorage, 0, sizeof(CVidBitmap));
    auto* ours = reinterpret_cast<CVidBitmap*>(oursStorage);
    std::vector<uint8_t> mine(kSize);
    std::vector<uint8_t> pixels(kDataSize);
    Picks picks(ctx.seed, 0x426D703136540000ull); // "Bmp16T"
    const int count = 600 + ctx.iterations / 10;
    const uint32_t flags[8] = { 0u, 0x10000u, 0x10001u, 0x40001u, 0x200001u, 0x80001u, 0x5u, 0x10005u };
    for (int format = 0; format < 2; ++format) {
        SetMode16(video, ctx, format == 0);
        for (int n = 0; n < count; ++n) {
            const int32_t cx = picks.Between(2, 32);
            const int32_t cy = picks.Between(1, 32);
            const int32_t jump = picks.Between(0, 8);
            const int32_t x = picks.Between(0, kW - cx);
            const int32_t y = picks.Between(cy - 1, kH - 1);
            const uint32_t dwFlags = flags[picks.Between(0, 7)];
            const uint32_t tint = picks.Bits() & 0xFFFFFF;
            const uint8_t brightness = static_cast<uint8_t>(picks.Between(0, 80));
            const uint8_t gamma = static_cast<uint8_t>(picks.Between(0, 16));
            ctx.CallOriginal([&] {
                Put<uint32_t>(video.mode, kGlobalTintOffset, tint);
                Put<uint8_t>(video.mode, kBrightnessOffset, brightness);
                Put<uint8_t>(video.mode, kGammaOffset, gamma);
            });
            video.oursMode->rgbGlobalTint = tint;
            video.oursMode->m_nBrightnessCorrection = brightness;
            video.oursMode->m_nGammaCorrection = gamma;
            for (size_t i = 0; i < kDataSize; i += 3) {
                const int kind = picks.Between(0, 6);
                if (kind == 0) {
                    pixels[i] = 0;
                    pixels[i + 1] = 0xFF;
                    pixels[i + 2] = 0;
                } else if (kind == 1) {
                    pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
                } else if (kind == 2) {
                    pixels[i] = pixels[i + 1] = pixels[i + 2] = 0xFF;
                } else {
                    pixels[i] = static_cast<uint8_t>(picks.Bits());
                    pixels[i + 1] = static_cast<uint8_t>(picks.Bits());
                    pixels[i + 2] = static_cast<uint8_t>(picks.Bits());
                }
            }
            for (uint8_t& b : mine) {
                b = static_cast<uint8_t>(picks.Bits());
            }
            char inputs[240];
            std::snprintf(inputs, sizeof(inputs),
                "input %d (%s): %dx%d, last row at (%d,%d), jump %d, flags 0x%X, tint 0x%06X, brightness %u, gamma %u",
                n, format == 0 ? "5-6-5" : "5-5-5", cx, cy, x, y, jump, dwFlags, tint, brightness, gamma);
            ctx.SetInputs(inputs);
            const size_t offset = static_cast<size_t>((y * kW + x) * 2);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(canvas, mine.data(), kSize);
                std::memcpy(data, pixels.data(), kDataSize);
                const int32_t sz[2] = { cx, cy };
                std::memcpy(size, sz, 8);
                originalResult = reinterpret_cast<BltBmp24To16TintFn>(kBltBmp24To16Tint)(self, nullptr,
                    canvas + offset, kW * 2, data, size, jump, dwFlags);
            });
            const BOOL oursResult = ours->BltBmp24To16Tint(reinterpret_cast<WORD*>(mine.data() + offset), kW * 2,
                pixels.data(), CSize(cx, cy), jump, dwFlags);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                for (size_t i = 0; i < kSize; ++i) {
                    if (canvas[i] != mine[i]) {
                        ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                        break;
                    }
                }
            }
        }
    }
    ::operator delete(oursStorage);
}

// CVidPalette::RealizeRange (0x7BF870) at 32 bpp: a range palette of 256
// random entries, the seven ranges each with or without a tint, an add and a
// light (pulsing or steady), the suppress mask, the global tint, add and
// light flags, grey and the 0x2000000 shading, and the video mode's fade,
// brightness, gamma and global tint. All 256 destination entries are
// compared.
void RealizeRange(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* chitin = video.chitin;
    uint8_t* mode = video.mode;
    CChitin* oursChitin = video.oursChitin;
    CVidInf* oursMode = video.oursMode;
    Picks picks(ctx.seed, 0x5265616C52616E00ull); // "RealRan"

    const uint32_t kFlagBits[] = { 0x1, 0x8, 0x10000, 0x20000, 0x40000, 0x80000, 0x200000, 0x2000000 };

    auto check = [&](int n) {
        ModeValues values;
        values.fade = static_cast<uint8_t>(picks.Between(0, 3) == 0 ? picks.Between(0, CVidMode::NUM_FADE_FRAMES)
                                                                    : CVidMode::NUM_FADE_FRAMES);
        values.globalTint = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
        values.brightness = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 255) : 0);
        values.gamma = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 40) : 0);
        const int32_t auCounter = picks.Between(0, 100000);
        const int32_t auBase = picks.Between(1, 1 << 24);
        const uint32_t subRanges = static_cast<uint32_t>(picks.Between(0, 1));
        uint32_t dwFlags = 0;
        for (const uint32_t bit : kFlagBits) {
            if (picks.Between(0, 2) == 0) {
                dwFlags |= bit;
            }
        }
        const uint32_t nTransVal = picks.Between(0, 1) ? 0xFF000000u : 0u;

        RGBQUAD entries[256];
        for (RGBQUAD& entry : entries) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&entry, &bits, sizeof(entry));
        }

        // The affect arguments: colours, then per range a tint, an add and
        // a light, each absent, steady (period 1; 0 also for the add) or
        // pulsing (period 2 to 30).
        uint32_t colors[3];
        for (uint32_t& color : colors) {
            color = picks.Bits() & 0xFFFFFF;
        }
        uint32_t rangeColors[3][7] = {};
        bool present[3][7] = {};
        uint8_t periods[3][7] = {};
        for (int kind = 0; kind < 3; ++kind) {
            for (int range = 0; range < 7; ++range) {
                present[kind][range] = picks.Between(0, 2) != 0;
                rangeColors[kind][range] = picks.Bits() & 0xFFFFFF;
                const int32_t form = picks.Between(0, 2);
                if (form == 0) {
                    periods[kind][range] = static_cast<uint8_t>(kind == 1 && picks.Between(0, 1) ? 0 : 1);
                } else {
                    periods[kind][range] = static_cast<uint8_t>(picks.Between(2, 30));
                }
            }
        }
        // A range without an add keeps the previous range's add percentage,
        // and a suppressed range then divides by its own add period: with a
        // period of 0 the original divides by zero (0x7BFEC1). Inputs with
        // an add period of 0 suppress nothing.
        bool zeroAddPeriod = false;
        for (int range = 0; range < 7; ++range) {
            zeroAddPeriod = zeroAddPeriod || periods[1][range] == 0;
        }
        const uint8_t suppress = zeroAddPeriod ? 0 : static_cast<uint8_t>(picks.Bits());

        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: dwFlags 0x%X, nTransVal 0x%X, fade %u, tint 0x%X, brightness %u, gamma %u, AU %d+%d, "
            "subranges %u, suppress 0x%X",
            n, dwFlags, nTransVal, values.fade, values.globalTint, values.brightness, values.gamma, auBase,
            auCounter, subRanges, suppress);
        ctx.SetInputs(inputs);

        uint32_t* originalDest = nullptr;
        ctx.CallOriginal([&] {
            Put<uint8_t>(mode, kFadeOffset, values.fade);
            Put<uint32_t>(mode, kGlobalTintOffset, values.globalTint);
            Put<uint8_t>(mode, kBrightnessOffset, values.brightness);
            Put<uint8_t>(mode, kGammaOffset, values.gamma);
            Put<int32_t>(chitin, kAUCounterOffset, auCounter);

            uint8_t* palette = OriginalBlock(kPaletteSize);
            reinterpret_cast<PaletteCtorFn>(kPaletteCtor)(palette, nullptr, 1);
            Put<int32_t>(palette, kPaletteBaseOffset, auBase);
            Put<uint32_t>(palette, kPaletteSubRangesOffset, subRanges);
            auto* paletteEntries = reinterpret_cast<uint8_t*>(
                static_cast<uintptr_t>(Get<uint32_t>(palette, kPalettePaletteOffset)));
            std::memcpy(paletteEntries, entries, sizeof(entries));

            uint8_t* affect = OriginalBlock(kAffectSize);
            Put<uint32_t>(affect, 0x0, colors[0]);
            Put<uint32_t>(affect, 0x4, colors[1]);
            Put<uint32_t>(affect, 0x8, colors[2]);
            const size_t pointerOffsets[3] = { 0x0C, 0x30, 0x54 };
            const size_t periodOffsets[3] = { 0x28, 0x4C, 0x70 };
            uint8_t* targets = OriginalBlock(3 * 7 * 4);
            for (int kind = 0; kind < 3; ++kind) {
                for (int range = 0; range < 7; ++range) {
                    uint8_t* target = targets + (kind * 7 + range) * 4;
                    Put<uint32_t>(target, 0, rangeColors[kind][range]);
                    Put<uint32_t>(affect, pointerOffsets[kind] + range * 4,
                        present[kind][range] ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)) : 0u);
                    Put<uint8_t>(affect, periodOffsets[kind] + range, periods[kind][range]);
                }
            }
            Put<uint8_t>(affect, 0x77, suppress);

            originalDest = reinterpret_cast<uint32_t*>(OriginalBlock(256 * 4));
            reinterpret_cast<RealizeRangeFn>(kRealizeRange)(palette, nullptr, originalDest, 32, dwFlags, affect,
                nTransVal);
        });

        oursMode->m_nFade = values.fade;
        oursMode->rgbGlobalTint = values.globalTint;
        oursMode->m_nBrightnessCorrection = values.brightness;
        oursMode->m_nGammaCorrection = values.gamma;
        oursChitin->nAUCounter = auCounter;

        CVidPalette palette(CVidPalette::TYPE_RANGE);
        palette.m_nAUCounterBase = auBase;
        palette.m_bSubRangesCalculated = static_cast<BOOL>(subRanges);
        std::memcpy(palette.m_pPalette, entries, sizeof(entries));

        CVIDIMG_PALETTEAFFECT affect;
        std::memset(&affect, 0, sizeof(affect));
        affect.rgbTintColor = colors[0];
        affect.rgbAddColor = colors[1];
        affect.rgbLightColor = colors[2];
        COLORREF targets[3][7];
        for (int range = 0; range < 7; ++range) {
            for (int kind = 0; kind < 3; ++kind) {
                targets[kind][range] = rangeColors[kind][range];
            }
            affect.pRangeTints[range] = present[0][range] ? &targets[0][range] : nullptr;
            affect.aRangeTintPeriods[range] = periods[0][range];
            affect.pRangeAdds[range] = present[1][range] ? &targets[1][range] : nullptr;
            affect.aRangeAddPeriods[range] = periods[1][range];
            affect.pRangeLights[range] = present[2][range] ? &targets[2][range] : nullptr;
            affect.aRangeLightPeriods[range] = periods[2][range];
        }
        affect.suppressTints = suppress;

        DWORD oursDest[256] = {};
        palette.RealizeRange(oursDest, 32, dwFlags, &affect, nTransVal);
        ctx.CountInputs(1);
        CompareArrays(ctx, originalDest, oursDest, 256);
    };

    for (int i = 0; i < 64 + ctx.iterations; ++i) {
        check(i);
    }
}

// CVidPalette::RealizeResource(pDestPalette, nBpp, dwFlags, pAffectArgs)
// (0x7C04B0), `ret 0x10` at 0x7C0CB0.
using RealizeResourceFn = void(__fastcall*)(void* self, void* edx, uint32_t* pDest, int32_t nBpp, uint32_t dwFlags,
    void* pAffectArgs);
constexpr uintptr_t kRealizeResource = 0x7C04B0;
constexpr size_t kPaletteEntriesOffset = 0xC;

// CVidPalette::RealizeResource (0x7C04B0) at 32 bpp: a resource palette
// of 256 random entries (entry 0 sometimes 0xFF00, entry 1 sometimes black,
// for the reserved-entry flags 0x400000 and 0x800000), range 0's tint, add
// and light each absent, steady (period 1; 0 also for the add) or pulsing
// (period 2 to 30), the suppress mask, every flag bit the function and its
// callees read (including the add flag 0x40000 and the combined tint
// 0x30000), and the video mode's fade, brightness, gamma and global tint.
// The palette comes from the original's constructor (0x7BEEA0, type 0) with
// m_pPalette, m_nEntries and m_nAUCounterBase set; all 256 destination
// entries are compared.
void RealizeResource(CaseContext& ctx)
{
    VideoFixture video(ctx);
    Picks picks(ctx.seed, 0x5265616C52657300ull); // "RealRes"
    const uint32_t kFlagBits[] = { 0x8, 0x10000, 0x20000, 0x40000, 0x80000, 0x200000, 0x2000000, 0x400000,
        0x800000 };

    for (int n = 0; n < 200 + ctx.iterations; ++n) {
        ModeValues values;
        values.fade = static_cast<uint8_t>(picks.Between(0, 3) == 0 ? picks.Between(0, CVidMode::NUM_FADE_FRAMES)
                                                                    : CVidMode::NUM_FADE_FRAMES);
        values.globalTint = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
        values.brightness = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 255) : 0);
        values.gamma = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 40) : 0);
        const int32_t auCounter = picks.Between(0, 100000);
        const int32_t auBase = picks.Between(1, 1 << 24);
        uint32_t dwFlags = 0;
        for (const uint32_t bit : kFlagBits) {
            if (picks.Between(0, 2) == 0) {
                dwFlags |= bit;
            }
        }

        RGBQUAD entries[256];
        for (RGBQUAD& entry : entries) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&entry, &bits, sizeof(entry));
        }
        if (picks.Between(0, 1)) {
            const uint32_t green = 0xFF00;
            std::memcpy(&entries[0], &green, 4);
        }
        if (picks.Between(0, 1)) {
            std::memset(&entries[1], 0, 4);
        }

        uint32_t colors[3];
        for (uint32_t& color : colors) {
            color = picks.Bits() & 0xFFFFFF;
        }
        bool present[3];
        uint32_t rangeColors[3];
        uint8_t periods[3];
        for (int kind = 0; kind < 3; ++kind) {
            present[kind] = picks.Between(0, 2) != 0;
            rangeColors[kind] = picks.Bits() & 0xFFFFFF;
            const int32_t form = picks.Between(0, 2);
            if (form == 0) {
                periods[kind] = static_cast<uint8_t>(kind == 1 && picks.Between(0, 1) ? 0 : 1);
            } else {
                periods[kind] = static_cast<uint8_t>(picks.Between(2, 30));
            }
        }
        const uint8_t suppress = static_cast<uint8_t>(picks.Bits());

        char inputs[220];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: dwFlags 0x%X, fade %u, tint 0x%X, brightness %u, gamma %u, AU %d+%d, suppress 0x%X, "
            "range tint %d/%u add %d/%u light %d/%u",
            n, dwFlags, values.fade, values.globalTint, values.brightness, values.gamma, auBase, auCounter, suppress,
            present[0], periods[0], present[1], periods[1], present[2], periods[2]);
        ctx.SetInputs(inputs);

        uint32_t* originalDest = nullptr;
        ctx.CallOriginal([&] {
            Put<uint8_t>(video.mode, kFadeOffset, values.fade);
            Put<uint32_t>(video.mode, kGlobalTintOffset, values.globalTint);
            Put<uint8_t>(video.mode, kBrightnessOffset, values.brightness);
            Put<uint8_t>(video.mode, kGammaOffset, values.gamma);
            Put<int32_t>(video.chitin, kAUCounterOffset, auCounter);

            uint8_t* palette = OriginalBlock(kPaletteSize);
            reinterpret_cast<PaletteCtorFn>(kPaletteCtor)(palette, nullptr, 0);
            uint8_t* paletteEntries = OriginalBlock(sizeof(entries));
            std::memcpy(paletteEntries, entries, sizeof(entries));
            Put<uint32_t>(palette, kPalettePaletteOffset,
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(paletteEntries)));
            Put<int32_t>(palette, kPaletteEntriesOffset, 256);
            Put<int32_t>(palette, kPaletteBaseOffset, auBase);

            uint8_t* affect = OriginalBlock(kAffectSize);
            Put<uint32_t>(affect, 0x0, colors[0]);
            Put<uint32_t>(affect, 0x4, colors[1]);
            Put<uint32_t>(affect, 0x8, colors[2]);
            const size_t pointerOffsets[3] = { 0x0C, 0x30, 0x54 };
            const size_t periodOffsets[3] = { 0x28, 0x4C, 0x70 };
            uint8_t* targets = OriginalBlock(3 * 4);
            for (int kind = 0; kind < 3; ++kind) {
                uint8_t* target = targets + kind * 4;
                Put<uint32_t>(target, 0, rangeColors[kind]);
                Put<uint32_t>(affect, pointerOffsets[kind],
                    present[kind] ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)) : 0u);
                Put<uint8_t>(affect, periodOffsets[kind], periods[kind]);
            }
            Put<uint8_t>(affect, 0x77, suppress);

            originalDest = reinterpret_cast<uint32_t*>(OriginalBlock(256 * 4));
            reinterpret_cast<RealizeResourceFn>(kRealizeResource)(palette, nullptr, originalDest, 32, dwFlags, affect);
        });

        video.oursMode->m_nFade = values.fade;
        video.oursMode->rgbGlobalTint = values.globalTint;
        video.oursMode->m_nBrightnessCorrection = values.brightness;
        video.oursMode->m_nGammaCorrection = values.gamma;
        video.oursChitin->nAUCounter = auCounter;

        RGBQUAD oursEntries[256];
        std::memcpy(oursEntries, entries, sizeof(entries));
        CVidPalette palette(CVidPalette::TYPE_RESOURCE);
        palette.m_pPalette = oursEntries;
        palette.m_nEntries = 256;
        palette.m_nAUCounterBase = auBase;

        CVIDIMG_PALETTEAFFECT affect;
        std::memset(&affect, 0, sizeof(affect));
        affect.rgbTintColor = colors[0];
        affect.rgbAddColor = colors[1];
        affect.rgbLightColor = colors[2];
        COLORREF targets[3] = { rangeColors[0], rangeColors[1], rangeColors[2] };
        affect.pRangeTints[0] = present[0] ? &targets[0] : nullptr;
        affect.aRangeTintPeriods[0] = periods[0];
        affect.pRangeAdds[0] = present[1] ? &targets[1] : nullptr;
        affect.aRangeAddPeriods[0] = periods[1];
        affect.pRangeLights[0] = present[2] ? &targets[2] : nullptr;
        affect.aRangeLightPeriods[0] = periods[2];
        affect.suppressTints = suppress;

        DWORD oursDest[256] = {};
        palette.RealizeResource(oursDest, 32, dwFlags, &affect);
        palette.m_pPalette = nullptr;
        ctx.CountInputs(1);
        CompareArrays(ctx, originalDest, oursDest, 256);
    }
}

// CVidPoly::FillConvexPoly(pSurface, lPitch, rClip, dwColor, dwFlags, ptRef)
// (0x7C0F40), `ret 0x18`; CVidPoly::CVidPoly (0x7C0DB0), plain ret. It
// reads m_pVertices (4) and m_nVertices (8) and picks its span function from
// g_pChitin->cVideo.m_nBpp.
using FillConvexPolyFn = int32_t(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch,
    const void* rClip, uint32_t dwColor, uint32_t dwFlags, const void* ptRef);
constexpr uintptr_t kFillConvexPoly = 0x7C0F40;
using VidPolyCtorFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kVidPolyCtor = 0x7C0DB0;
constexpr int kPolyCanvasW = 64;
constexpr int kPolyCanvasH = 48;

// FillConvexPoly on a 64x48 canvas of random pixels, clipped to a rectangle
// inside it, the surface pointer at the clip's top left (as
// CVidInf::BKRenderConvexPoly passes it): triangles with fractional edges in
// both directions, flat tops and bottoms, polygons starting above or ending
// below the clip, convex quadrilaterals, and the axis-aligned rectangle with
// 0x2 (CUIPanel::RenderDither), each solid (0x1), dithered (0x2), shaded
// (0x10), mirrored (0x4) and flipped (0x8). The result and the whole canvas
// are compared.
void FillConvexPoly(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* poly = nullptr;
    uint32_t* canvas = nullptr;
    uint8_t* vertices = nullptr;
    uint8_t* clip = nullptr;
    uint8_t* ref = nullptr;
    ctx.SetInputs("building the original's polygon and canvas");
    ctx.CallOriginal([&] {
        poly = OriginalBlock(kVidPolySize);
        reinterpret_cast<VidPolyCtorFn>(kVidPolyCtor)(poly, nullptr);
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(kPolyCanvasW * kPolyCanvasH * 4));
        vertices = OriginalBlock(8 * 4);
        clip = OriginalBlock(16);
        ref = OriginalBlock(8);
    });
    CVidPoly ours;
    std::vector<DWORD> mine(kPolyCanvasW * kPolyCanvasH);
    Picks picks(ctx.seed, 0x46696C6C50000000ull); // "FillP"
    const uint32_t kFlags[] = { 0x1, 0x2, 0x10, 0x1 | 0x4, 0x2 | 0x4, 0x1 | 0x8, 0x2 | 0x8, 0x10 | 0x8 };
    int n = 0;

    auto check = [&](const std::vector<CVIDPOLY_VERTEX>& points, const CRect& rClip, uint32_t dwFlags) {
        const uint32_t color = picks.Bits() & 0xFFFFFF;
        for (DWORD& pixel : mine) {
            pixel = picks.Bits();
        }
        std::string text = "input " + std::to_string(n++) + ": flags " + Hex(dwFlags) + ", clip ("
            + std::to_string(rClip.left) + "," + std::to_string(rClip.top) + ")-(" + std::to_string(rClip.right)
            + "," + std::to_string(rClip.bottom) + "), vertices";
        for (const CVIDPOLY_VERTEX& v : points) {
            text += " (" + std::to_string(v.x) + "," + std::to_string(v.y) + ")";
        }
        ctx.SetInputs(text);
        const int32_t r[4] = { rClip.left, rClip.top, rClip.right, rClip.bottom };
        const int32_t pt[2] = { picks.Between(0, 7), picks.Between(0, 7) };
        const int32_t offset = rClip.top * kPolyCanvasW + rClip.left;
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size() * 4);
            std::memcpy(vertices, points.data(), points.size() * 4);
            std::memcpy(clip, r, sizeof(r));
            std::memcpy(ref, pt, sizeof(pt));
            Put<uint32_t>(poly, 4, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(vertices)));
            Put<int32_t>(poly, 8, static_cast<int32_t>(points.size()));
            originalResult = reinterpret_cast<FillConvexPolyFn>(kFillConvexPoly)(poly, nullptr, canvas + offset,
                kPolyCanvasW * 4, clip, color, dwFlags, ref);
        });
        std::vector<CVIDPOLY_VERTEX> copy(points);
        ours.SetPoly(copy.data(), static_cast<WORD>(copy.size()));
        const BOOL oursResult = ours.FillConvexPoly(reinterpret_cast<WORD*>(mine.data() + offset), kPolyCanvasW * 4,
            rClip, color, dwFlags, CPoint(pt[0], pt[1]));
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, canvas, mine.data(), kPolyCanvasW * kPolyCanvasH);
        }
    };

    auto vertex = [](int x, int y) { return CVIDPOLY_VERTEX { static_cast<WORD>(x), static_cast<WORD>(y) }; };
    const CRect kClips[] = { CRect(0, 0, kPolyCanvasW, kPolyCanvasH), CRect(4, 6, 60, 44), CRect(10, 12, 30, 20) };

    for (const uint32_t dwFlags : kFlags) {
        for (const CRect& rClip : kClips) {
            // Fixed shapes: fractional edges both ways, a flat top, a flat
            // bottom, one starting above the clip and ending below it, and
            // the dithered rectangle.
            check({ vertex(10, 0), vertex(13, 4), vertex(6, 4) }, rClip, dwFlags);
            check({ vertex(12, 1), vertex(20, 13), vertex(3, 13) }, rClip, dwFlags);
            check({ vertex(5, 3), vertex(40, 3), vertex(22, 30) }, rClip, dwFlags);
            check({ vertex(25, 2), vertex(50, 33), vertex(7, 33) }, rClip, dwFlags);
            check({ vertex(30, 0), vertex(55, 47), vertex(2, 40) }, rClip, dwFlags);
            check({ vertex(2, 3), vertex(8, 3), vertex(8, 7), vertex(2, 7) }, rClip, dwFlags);
            check({ vertex(0, 0), vertex(63, 0), vertex(63, 47), vertex(0, 47) }, rClip, dwFlags);
        }
    }
    for (int i = 0; i < 200 + ctx.iterations / 10; ++i) {
        const uint32_t dwFlags = kFlags[picks.Between(0, 7)];
        const CRect rClip = kClips[picks.Between(0, 2)];
        std::vector<CVIDPOLY_VERTEX> points;
        if (picks.Between(0, 1) == 0) {
            // A triangle, either winding, not all on one row.
            for (int k = 0; k < 3; ++k) {
                points.push_back(vertex(picks.Between(0, kPolyCanvasW - 1), picks.Between(0, kPolyCanvasH - 1)));
            }
            if (points[0].y == points[1].y && points[1].y == points[2].y) {
                points[2].y = static_cast<WORD>((points[2].y + 5) % kPolyCanvasH);
            }
        } else {
            // A convex quadrilateral: four points on an axis-aligned
            // ellipse, in angle order.
            const int cx = picks.Between(15, 48);
            const int cy = picks.Between(12, 36);
            const int rx = picks.Between(3, 15);
            const int ry = picks.Between(3, 11);
            static const int kCos[8] = { 100, 71, 0, -71, -100, -71, 0, 71 };
            static const int kSin[8] = { 0, 71, 100, 71, 0, -71, -100, -71 };
            int start = picks.Between(0, 1);
            for (int k = 0; k < 4; ++k) {
                const int a = start + 2 * k;
                points.push_back(vertex(cx + rx * kCos[a] / 100, cy + ry * kSin[a] / 100));
            }
            if (picks.Between(0, 1)) {
                std::reverse(points.begin(), points.end());
            }
        }
        check(points, rClip, dwFlags);
    }
}

// CVidPoly::FillPoly(pSurface, lPitch, rClip, dwColor, dwFlags, ptRef)
// (0x7C13A0), `ret 0x18`, with the same arguments as FillConvexPoly. It
// builds its edge table into a global (0xA07B20) and caches up to 16 tables
// keyed by the vertex list's address and count (0xA09F20; not for 0x2), so
// every input gets vertex lists at fresh addresses on both sides: a
// reused address with the same count would take a stale table, in the
// original as in ours.
using FillPolyFn = int32_t(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch,
    const void* rClip, uint32_t dwColor, uint32_t dwFlags, const void* ptRef);
constexpr uintptr_t kFillPoly = 0x7C13A0;

// FillPoly on the 64x48 canvas of random pixels: non-convex shapes (a
// notched square, an L, a U, a comb, a bow tie that crosses itself, a
// spiral-like zigzag), the convex triangle and rectangle, and random
// star-shaped polygons of 3 to 12 vertices, each solid, dithered, shaded,
// mirrored and flipped, three clip rectangles, each filled twice with the
// same vertex list so the second fill takes the cached table. The result
// and the whole canvas are compared.
void FillPoly(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* poly = nullptr;
    uint32_t* canvas = nullptr;
    uint8_t* clip = nullptr;
    uint8_t* ref = nullptr;
    ctx.SetInputs("building the original's polygon and canvas");
    ctx.CallOriginal([&] {
        poly = OriginalBlock(kVidPolySize);
        reinterpret_cast<VidPolyCtorFn>(kVidPolyCtor)(poly, nullptr);
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(kPolyCanvasW * kPolyCanvasH * 4));
        clip = OriginalBlock(16);
        ref = OriginalBlock(8);
    });
    CVidPoly ours;
    // Our vertex lists stay alive, so no address repeats.
    std::vector<std::vector<CVIDPOLY_VERTEX>> keep;
    std::vector<DWORD> mine(kPolyCanvasW * kPolyCanvasH);
    Picks picks(ctx.seed, 0x46696C6C506F6C79ull); // "FillPoly"
    const uint32_t kFlags[] = { 0x1, 0x2, 0x10, 0x1 | 0x4, 0x2 | 0x4, 0x1 | 0x8, 0x2 | 0x8, 0x10 | 0x8 };
    int n = 0;

    auto check = [&](const std::vector<CVIDPOLY_VERTEX>& points, const CRect& rClip, uint32_t dwFlags) {
        uint8_t* vertices = nullptr;
        ctx.CallOriginal([&] { vertices = OriginalBlock(points.size() * 4); });
        keep.push_back(points);
        std::vector<CVIDPOLY_VERTEX>& copy = keep.back();
        for (int pass = 0; pass < 2; ++pass) {
            const uint32_t color = picks.Bits() & 0xFFFFFF;
            for (DWORD& pixel : mine) {
                pixel = picks.Bits();
            }
            std::string text = "input " + std::to_string(n++) + ": pass " + std::to_string(pass) + ", flags "
                + Hex(dwFlags) + ", clip (" + std::to_string(rClip.left) + "," + std::to_string(rClip.top) + ")-("
                + std::to_string(rClip.right) + "," + std::to_string(rClip.bottom) + "), vertices";
            for (const CVIDPOLY_VERTEX& v : points) {
                text += " (" + std::to_string(v.x) + "," + std::to_string(v.y) + ")";
            }
            ctx.SetInputs(text);
            const int32_t r[4] = { rClip.left, rClip.top, rClip.right, rClip.bottom };
            const int32_t pt[2] = { picks.Between(0, 7), picks.Between(0, 7) };
            const int32_t offset = rClip.top * kPolyCanvasW + rClip.left;
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(canvas, mine.data(), mine.size() * 4);
                std::memcpy(vertices, points.data(), points.size() * 4);
                std::memcpy(clip, r, sizeof(r));
                std::memcpy(ref, pt, sizeof(pt));
                Put<uint32_t>(poly, 4, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(vertices)));
                Put<int32_t>(poly, 8, static_cast<int32_t>(points.size()));
                originalResult = reinterpret_cast<FillPolyFn>(kFillPoly)(poly, nullptr, canvas + offset,
                    kPolyCanvasW * 4, clip, color, dwFlags, ref);
            });
            ours.SetPoly(copy.data(), static_cast<WORD>(copy.size()));
            const BOOL oursResult = ours.FillPoly(reinterpret_cast<WORD*>(mine.data() + offset), kPolyCanvasW * 4,
                rClip, color, dwFlags, CPoint(pt[0], pt[1]));
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                CompareArrays(ctx, canvas, mine.data(), kPolyCanvasW * kPolyCanvasH);
            }
        }
    };

    auto vertex = [](int x, int y) { return CVIDPOLY_VERTEX { static_cast<WORD>(x), static_cast<WORD>(y) }; };
    const CRect kClips[] = { CRect(0, 0, kPolyCanvasW, kPolyCanvasH), CRect(4, 6, 60, 44), CRect(10, 12, 30, 20) };

    for (const uint32_t dwFlags : kFlags) {
        for (const CRect& rClip : kClips) {
            check({ vertex(2, 2), vertex(6, 6), vertex(10, 2), vertex(10, 10), vertex(2, 10) }, rClip, dwFlags);
            check({ vertex(5, 3), vertex(15, 3), vertex(15, 30), vertex(45, 30), vertex(45, 40), vertex(5, 40) },
                rClip, dwFlags);
            check({ vertex(4, 4), vertex(14, 4), vertex(17, 33), vertex(40, 35), vertex(43, 2), vertex(58, 6),
                      vertex(50, 45), vertex(9, 44) },
                rClip, dwFlags);
            check({ vertex(1, 1), vertex(9, 25), vertex(17, 3), vertex(25, 27), vertex(33, 5), vertex(41, 29),
                      vertex(49, 7), vertex(57, 31), vertex(60, 46), vertex(3, 46) },
                rClip, dwFlags);
            check({ vertex(3, 5), vertex(50, 40), vertex(50, 5), vertex(3, 40) }, rClip, dwFlags);
            check({ vertex(30, 0), vertex(63, 20), vertex(40, 24), vertex(55, 47), vertex(20, 30), vertex(2, 47),
                      vertex(10, 20), vertex(0, 8) },
                rClip, dwFlags);
            check({ vertex(10, 0), vertex(13, 4), vertex(6, 4) }, rClip, dwFlags);
            check({ vertex(0, 0), vertex(63, 0), vertex(63, 47), vertex(0, 47) }, rClip, dwFlags);
        }
    }
    for (int i = 0; i < 200 + ctx.iterations / 10; ++i) {
        const uint32_t dwFlags = kFlags[picks.Between(0, 7)];
        const CRect rClip = kClips[picks.Between(0, 2)];
        // A star-shaped polygon: points at increasing angles around a
        // centre with random radii, not all on one row.
        static const int kCos[12] = { 100, 87, 50, 0, -50, -87, -100, -87, -50, 0, 50, 87 };
        static const int kSin[12] = { 0, 50, 87, 100, 87, 50, 0, -50, -87, -100, -87, -50 };
        const int cx = picks.Between(20, 43);
        const int cy = picks.Between(16, 31);
        std::vector<CVIDPOLY_VERTEX> points;
        for (int k = 0; k < 12; ++k) {
            if (picks.Between(0, 2) == 0 && 12 - k > 3 - static_cast<int>(points.size())) {
                continue;
            }
            const int radius = picks.Between(2, 19);
            const int x = std::clamp(cx + radius * kCos[k] / 100, 0, kPolyCanvasW - 1);
            const int y = std::clamp(cy + radius * kSin[k] / 100, 0, kPolyCanvasH - 1);
            points.push_back(vertex(x, y));
        }
        bool flat = true;
        for (const CVIDPOLY_VERTEX& v : points) {
            flat = flat && v.y == points[0].y;
        }
        if (flat) {
            // The original reads through NULL when every edge is
            // horizontal (an empty edge table); not an input.
            continue;
        }
        if (picks.Between(0, 1)) {
            std::reverse(points.begin(), points.end());
        }
        check(points, rClip, dwFlags);
    }
}

// CVidCell::sub_7D0950(pSurface, lPitch, dwFlags, nTransVal) (0x7D0950),
// `ret 0x10`, and sub_7D1190(pSurface, lPitch, dwFlags) (0x7D1190), `ret 0xc`.
using CellTransFn = int32_t(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch, uint32_t dwFlags,
    int32_t nTransVal);
using CellBrightFn = int32_t(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch, uint32_t dwFlags);
constexpr uintptr_t kCellTrans = 0x7D0950;
constexpr uintptr_t kCellBright = 0x7D1190;
// The members they read: CVidCell m_cPalette (8), m_paletteAffects (0x2C),
// pRes (0xA8), m_bPaletteChanged (0xCC), m_pFrame (0xD0), m_bShadowOn (0xD4),
// m_bDoubleSize (0xD6); CResCell m_bParsed (0x50), m_pBamHeader (0x54),
// m_pPalette (0x6A), m_bCacheHeader (0x7E); the BAM header's
// nTransparentColor (0xB); the video mode's field_24 (the shadow colour).
constexpr size_t kCellSize = 0x100;
constexpr size_t kResCellSize = 0x100;
constexpr size_t kShadowColorOffset = 0x24;
constexpr int kCellCanvas = 64;
constexpr int kFrameDataOffset = 0x20;

// A frame of nWidth x nHeight palette indices.
std::vector<uint8_t> RunLengthEncode(const std::vector<uint8_t>& pixels, uint8_t transparent)
{
    std::vector<uint8_t> out;
    for (size_t i = 0; i < pixels.size();) {
        if (pixels[i] == transparent) {
            size_t run = 1;
            while (i + run < pixels.size() && pixels[i + run] == transparent && run < 256) {
                ++run;
            }
            out.push_back(transparent);
            out.push_back(static_cast<uint8_t>(run - 1));
            i += run;
        } else {
            out.push_back(pixels[i++]);
        }
    }
    return out;
}

// sub_7D0950 (translucent) and sub_7D1190 (brightness-blended) on a
// synthetic BAM: a zeroed CResCell holding only the parsed flag, the header
// (its transparent index) and the palette, one frame entry, and the frame
// data RLE compressed or raw (the frame entry's offset bit 31). The frame is
// drawn at (8, 8) of a 64x64 canvas of random pixels, each channel at least
// 8, with the cell's shadow on or off and the video mode's shadow colour
// random. Raw frames bring out the original's walks outside the frame (rows
// four times apart in sub_7D0950, an extra row and column in sub_7D1190);
// the canvas is large enough that they stay inside it, so ours, bounded to
// the canvas, must draw the same pixels. sub_7D1190 reads past the raw data
// into bytes that are 0 (a black palette entry), which the bright canvas
// keeps unchanged; ours skips those pixels (docs/evidence/7D1190.md).
//
// The functions write members of the zeroed cells: the embedded m_cPalette
// (plain data, no vtable), which SetPalette points at the resource palette
// and Realize reads back. Ruling P4-11: under R10b that is acceptable, since
// the written members are plain data and the case's comparison of the
// whole canvas covers what Realize makes of them.
// The cells' other 32-bit blits on the same fixture: Blt8To32 (0x7CFBC0,
// the FX form: the whole frame at the surface pointer, `ret 0x10`),
// Blt8To32ClearShadow (0x7D0470) and Blt8To32Brightest (0x7D0730), both
// (pSurface, lPitch, dwFlags), `ret 0xc`.
using CellBlt32Fn = int32_t(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch, uint32_t dwFlags,
    int32_t nTransVal);
using CellShadowFn = int32_t(__fastcall*)(void* self, void* edx, uint32_t* pSurface, int32_t lPitch, uint32_t dwFlags);
constexpr uintptr_t kCellBlt8To32 = 0x7CFBC0;
constexpr uintptr_t kCellClearShadow = 0x7D0470;
constexpr uintptr_t kCellBrightest = 0x7D0730;
enum class CellMode { Translucent,
    Brightness,
    Blt,
    ClearShadow,
    Brightest };

void CellBlend(CaseContext& ctx, CellMode mode, uint64_t salt)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint32_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(kCellCanvas * kCellCanvas * 4));
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<DWORD> mine(kCellCanvas * kCellCanvas);
    Picks picks(ctx.seed, salt);
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        const int width = picks.Between(1, 8);
        const int height = picks.Between(1, 4);
        const bool compressed = picks.Between(0, 1) != 0;
        const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
        const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
        const uint32_t shadow = picks.Bits() & 0xFFFFFF;
        const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
        const int32_t nTransVal = picks.Between(0, 256);
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
        for (uint8_t& p : pixels) {
            const int32_t k = picks.Between(0, 5);
            p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
        }
        const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
        std::vector<uint8_t> image(kBamSize, 0);
        image[0xB] = transparent;
        std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
        std::vector<RGBQUAD> colors(256);
        for (RGBQUAD& c : colors) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&c, &bits, 4);
        }
        std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
        // Light and dark colours both occur.
        for (int i = 0; i < 256; i += 3) {
            colors[i].rgbRed >>= 2;
            colors[i].rgbGreen >>= 2;
            colors[i].rgbBlue >>= 2;
        }
        FRAMEENTRY entry = {};
        entry.nWidth = static_cast<WORD>(width);
        entry.nHeight = static_cast<WORD>(height);
        entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
        for (DWORD& pixel : mine) {
            pixel = picks.Bits() | 0x080808;
        }

        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d %s, transparent %u, shadow %s 0x%06X, flags 0x%X, nTransVal %d", n, width, height,
            compressed ? "RLE" : "raw", transparent, shadowOn ? "on" : "off", shadow, dwFlags, nTransVal);
        ctx.SetInputs(inputs);
        const int32_t start = 8 * kCellCanvas + 8;
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(bam, image.data(), kBamSize);
            std::memcpy(palette, colors.data(), 256 * 4);
            std::memcpy(frame, &entry, sizeof(entry));
            std::memcpy(canvas, mine.data(), mine.size() * 4);
            Put<uint8_t>(cell, 0xD4, shadowOn);
            Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
            switch (mode) {
            case CellMode::Translucent:
                originalResult = reinterpret_cast<CellTransFn>(kCellTrans)(cell, nullptr, canvas + start,
                    kCellCanvas * 4, dwFlags, nTransVal);
                break;
            case CellMode::Brightness:
                originalResult = reinterpret_cast<CellBrightFn>(kCellBright)(cell, nullptr, canvas + start,
                    kCellCanvas * 4, dwFlags);
                break;
            case CellMode::Blt:
                originalResult = reinterpret_cast<CellBlt32Fn>(kCellBlt8To32)(cell, nullptr, canvas + start,
                    kCellCanvas * 4, dwFlags, nTransVal);
                break;
            case CellMode::ClearShadow:
                originalResult = reinterpret_cast<CellShadowFn>(kCellClearShadow)(cell, nullptr, canvas + start,
                    kCellCanvas * 4, dwFlags);
                break;
            case CellMode::Brightest:
                originalResult = reinterpret_cast<CellShadowFn>(kCellBrightest)(cell, nullptr, canvas + start,
                    kCellCanvas * 4, dwFlags);
                break;
            }
        });
        std::memcpy(oursBam.data(), image.data(), kBamSize);
        std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
        oursFrame = entry;
        oursCell->m_bShadowOn = shadowOn;
        video.oursMode->field_24 = shadow;
        const auto* begin = reinterpret_cast<const BYTE*>(mine.data());
        const CVidCell::SurfaceRange range = { begin, begin + mine.size() * 4 };
        BOOL oursResult;
        switch (mode) {
        case CellMode::Translucent:
            oursResult = oursCell->sub_7D0950(mine.data() + start, kCellCanvas * 4, dwFlags, nTransVal, range);
            break;
        case CellMode::Brightness:
            oursResult = oursCell->sub_7D1190(mine.data() + start, kCellCanvas * 4, dwFlags, range);
            break;
        case CellMode::Blt:
            oursResult = oursCell->Blt8To32(mine.data() + start, kCellCanvas * 4, dwFlags, nTransVal);
            break;
        case CellMode::ClearShadow:
            oursResult = oursCell->Blt8To32ClearShadow(mine.data() + start, kCellCanvas * 4, dwFlags);
            break;
        case CellMode::Brightest:
        default:
            oursResult = oursCell->Blt8To32Brightest(mine.data() + start, kCellCanvas * 4, dwFlags);
            break;
        }
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, canvas, mine.data(), kCellCanvas * kCellCanvas);
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::Blt8To16(pSurface, lPitch, pt, rClip, dwFlags, ptSource)
// (0x7AF3C0) and Blt8To24 (0x7CEEB0), both `ret 0x18`. Unreachable in
// iwd2r, which runs at 32 bpp only (the DirectDraw shim lists no 16- or
// 24-bit mode), and ported for the record. They read the cell's frame and
// resource like the blends above and the global realized palette
// CVidImage::rgbTempPal (0x9064F8): 16 bpp takes each entry's low word, 24
// bpp its three low bytes.
using CellPointBltFn = int32_t(__fastcall*)(void* self, void* edx, void* pSurface, int32_t lPitch, const void* pt,
    const void* rClip, uint32_t dwFlags, const void* ptSource);
constexpr uintptr_t kBlt8To16 = 0x7AF3C0;
// CVidCell::Blt8To32(pSurface, lPitch, pt, rClip, dwFlags, ptSource,
// nTransVal) (0x7CFDF0), `ret 0x1c`, the 32-bit form the cells' Render
// uses: rgbTempPal as is, and with nTransVal the alpha byte blended.
using CellPointBlt32Fn = int32_t(__fastcall*)(void* self, void* edx, void* pSurface, int32_t lPitch, const void* pt,
    const void* rClip, uint32_t dwFlags, const void* ptSource, int32_t nTransVal);
constexpr uintptr_t kBlt8To32Point = 0x7CFDF0;
constexpr uintptr_t kBlt8To24 = 0x7CEEB0;
constexpr uintptr_t kTempPal = 0x9064F8;
constexpr int kPointCanvas = 64;

// A random frame (up to 12x8, RLE or raw, transparent pixels mixed in) drawn
// at a random point (-6..40) from a random source point inside it, clipped
// to a random rectangle inside the 64x64 canvas of random bytes. The
// original's compressed walk draws rows down to rClip.bottom - pt.y counted
// from pt.y, so a frame starting above the clip may draw below it; the
// canvas holds that. The result and the whole canvas are compared.
void CellPointBlt(CaseContext& ctx, int bpp, uint64_t salt)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    uint8_t* args = nullptr;
    constexpr size_t kBamSize = 0x1000;
    const int bytes = bpp / 8;
    const int pitch = kPointCanvas * bytes;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(static_cast<size_t>(pitch * kPointCanvas));
        args = OriginalBlock(32);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint8_t> mine(static_cast<size_t>(pitch * kPointCanvas));
    Picks picks(ctx.seed, salt);
    const int count = 600 + ctx.iterations / 10;
    for (int n = 0; n < count; ++n) {
        const int width = picks.Between(1, 12);
        const int height = picks.Between(1, 8);
        const bool compressed = picks.Between(0, 1) != 0;
        const uint8_t transparent = static_cast<uint8_t>(picks.Between(0, 255));
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
        for (uint8_t& p : pixels) {
            p = picks.Between(0, 2) == 0 ? transparent : static_cast<uint8_t>(picks.Between(0, 255));
        }
        const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
        std::vector<uint8_t> image(kBamSize, 0);
        image[0xB] = transparent;
        std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
        FRAMEENTRY entry = {};
        entry.nWidth = static_cast<WORD>(width);
        entry.nHeight = static_cast<WORD>(height);
        entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
        uint32_t palette[256];
        for (uint32_t& c : palette) {
            c = picks.Bits();
        }
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        const int32_t left = picks.Between(0, 40);
        const int32_t top = picks.Between(0, 40);
        const int32_t r[4] = { left, top, picks.Between(left, 48), picks.Between(top, 48) };
        const int32_t pt[2] = { picks.Between(-6, 40), picks.Between(-6, 40) };
        const int32_t src[2] = { picks.Between(0, width - 1), picks.Between(0, height - 1) };

        char inputs[240];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d %s, transparent %u, pt (%d,%d), clip (%d,%d)-(%d,%d), source (%d,%d)", n, width,
            height, compressed ? "RLE" : "raw", transparent, pt[0], pt[1], r[0], r[1], r[2], r[3], src[0], src[1]);
        ctx.SetInputs(inputs);
        const int32_t nTransVal = bpp == 32 && picks.Between(0, 1) != 0 ? picks.Between(1, 255) : 0;
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(bam, image.data(), kBamSize);
            std::memcpy(frame, &entry, sizeof(entry));
            std::memcpy(canvas, mine.data(), mine.size());
            std::memcpy(reinterpret_cast<void*>(kTempPal), palette, sizeof(palette));
            std::memcpy(args, pt, 8);
            std::memcpy(args + 8, r, 16);
            std::memcpy(args + 24, src, 8);
            if (bpp == 32) {
                originalResult = reinterpret_cast<CellPointBlt32Fn>(kBlt8To32Point)(cell, nullptr, canvas, pitch,
                    args, args + 8, 0, args + 24, nTransVal);
            } else {
                originalResult = reinterpret_cast<CellPointBltFn>(bpp == 16 ? kBlt8To16 : kBlt8To24)(cell, nullptr,
                    canvas, pitch, args, args + 8, 0, args + 24);
            }
        });
        std::memcpy(oursBam.data(), image.data(), kBamSize);
        oursFrame = entry;
        std::memcpy(CVidImage::rgbTempPal, palette, sizeof(palette));
        BOOL oursResult;
        if (bpp == 32) {
            oursResult = oursCell->Blt8To32(reinterpret_cast<DWORD*>(mine.data()), pitch, CPoint(pt[0], pt[1]),
                CRect(r[0], r[1], r[2], r[3]), 0, CPoint(src[0], src[1]), nTransVal);
        } else if (bpp == 16) {
            oursResult = oursCell->Blt8To16(reinterpret_cast<WORD*>(mine.data()), pitch, CPoint(pt[0], pt[1]),
                CRect(r[0], r[1], r[2], r[3]), 0, CPoint(src[0], src[1]));
        } else {
            oursResult = oursCell->Blt8To24(mine.data(), pitch, CPoint(pt[0], pt[1]), CRect(r[0], r[1], r[2], r[3]),
                0, CPoint(src[0], src[1]));
        }
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            std::vector<uint32_t> a(mine.size());
            std::vector<uint32_t> b(mine.size());
            for (size_t i = 0; i < mine.size(); ++i) {
                a[i] = canvas[i];
                b[i] = mine[i];
            }
            CompareArrays(ctx, a.data(), b.data(), static_cast<int>(mine.size()));
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

void CellBlt8To16(CaseContext& ctx)
{
    CellPointBlt(ctx, 16, 0x426C743136000000ull); // "Blt16"
}

void CellBlt8To24(CaseContext& ctx)
{
    CellPointBlt(ctx, 24, 0x426C743234000000ull); // "Blt24"
}

// CVidCell::sub_7AF1C0 (0x7AF1C0): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags), `ret 0xc` - the plain 16-bit blit, one WORD a
// pixel, the transparent index skipped, the palette realized for 16 bpp with
// 255, rows bottom-up by lPitch in bytes. Unreachable in iwd2r, which runs at
// 32 bpp only (the DirectDraw shim lists no 16-bit mode), and ported for the
// record; it is the fast twin of the clipped Blt8To16 (0x7AF3C0) above, which
// takes a point and a clip. The raw walk reads one byte ahead of the frame
// (0x7AF384), unlike the 24- and 32-bit siblings, so raw frames land shifted
// by a pixel.
using CellFastBlt16Fn = int32_t(__fastcall*)(void* self, void* edx, uint16_t* pSurface, int32_t lPitch,
    uint32_t dwFlags);
constexpr uintptr_t kCellBlt8To16Fast = 0x7AF1C0;

// Random frames up to 8x4 (raw and RLE, transparent pixels mixed in), the
// palette flags, the shadow entry on and off, drawn at 8 rows and 8 pixels
// into a 64x64 16-bit canvas of random bytes, on 5-6-5 and 5-5-5 in turn (the
// palette realization reads the mode's masks). The pitch, the row step and
// the raw walk's one-byte lead all matter. The whole canvas and the result are
// compared.
void CellBlt8To16Fast(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 2;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x426C743136464100ull); // "Blt16FA"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 2;
    for (int format = 0; format < 2; ++format) {
        SetMode16(video, ctx, format == 0);
        for (int n = 0; n < count; ++n) {
            const int width = picks.Between(1, 8);
            const int height = picks.Between(1, 4);
            const bool compressed = picks.Between(0, 1) != 0;
            const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
            const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
            const uint32_t shadow = picks.Bits() & 0xFFFF;
            const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
            std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
            for (uint8_t& p : pixels) {
                const int32_t k = picks.Between(0, 5);
                p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
            }
            const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
            std::vector<uint8_t> image(kBamSize, 0);
            image[0xB] = transparent;
            std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
            std::vector<RGBQUAD> colors(256);
            for (RGBQUAD& c : colors) {
                const uint32_t bits = picks.Bits();
                std::memcpy(&c, &bits, 4);
            }
            std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
            for (int i = 0; i < 256; i += 3) {
                colors[i].rgbRed >>= 2;
                colors[i].rgbGreen >>= 2;
                colors[i].rgbBlue >>= 2;
            }
            FRAMEENTRY entry = {};
            entry.nWidth = static_cast<WORD>(width);
            entry.nHeight = static_cast<WORD>(height);
            entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
            for (uint32_t& word : mine) {
                word = picks.Bits();
            }

            char inputs[200];
            std::snprintf(inputs, sizeof(inputs),
                "input %d (%s): %dx%d %s, transparent %u, shadow %s 0x%04X, flags 0x%X", n,
                format == 0 ? "5-6-5" : "5-5-5", width, height, compressed ? "RLE" : "raw", transparent,
                shadowOn ? "on" : "off", shadow, dwFlags);
            ctx.SetInputs(inputs);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(bam, image.data(), kBamSize);
                std::memcpy(palette, colors.data(), 256 * 4);
                std::memcpy(frame, &entry, sizeof(entry));
                std::memcpy(canvas, mine.data(), kCanvasBytes);
                Put<uint8_t>(cell, 0xD4, shadowOn);
                Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
                originalResult = reinterpret_cast<CellFastBlt16Fn>(kCellBlt8To16Fast)(cell, nullptr,
                    reinterpret_cast<uint16_t*>(canvas + start), kCellCanvas * 2, dwFlags);
            });
            std::memcpy(oursBam.data(), image.data(), kBamSize);
            std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
            oursFrame = entry;
            oursCell->m_bShadowOn = shadowOn;
            video.oursMode->field_24 = static_cast<int>(shadow);
            const BOOL oursResult = oursCell->sub_7AF1C0(
                reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(mine.data()) + start), kCellCanvas * 2, dwFlags);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
            }
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::sub_7CECD0 (0x7CECD0): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags), `ret 0xc` - the plain 24-bit blit, three bytes
// a pixel, the transparent index skipped, the palette realized for 24 bpp with
// 255, rows bottom-up by lPitch in bytes. Unreachable in iwd2r, which runs at
// 32 bpp only (the DirectDraw shim lists no 24-bit mode), and ported for the
// record; it is the fast twin of the clipped Blt8To24 (0x7CEEB0) below, which
// takes a point and a clip.
using CellFastBlt24Fn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch,
    uint32_t dwFlags);
constexpr uintptr_t kCellBlt8To24Fast = 0x7CECD0;

// Random frames up to 8x4 (raw and RLE, transparent pixels mixed in), the
// palette flags, the shadow entry on and off, drawn at 8 rows and 8 pixels
// into a 96x96 24-bit canvas of random bytes: the pitch and the row step both
// matter. The whole canvas and the result are compared.
void CellBlt8To24Fast(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 3;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x426C743234464100ull); // "Blt24FA"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 3;
    for (int n = 0; n < count; ++n) {
        const int width = picks.Between(1, 8);
        const int height = picks.Between(1, 4);
        const bool compressed = picks.Between(0, 1) != 0;
        const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
        const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
        const uint32_t shadow = picks.Bits() & 0xFFFFFF;
        const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
        for (uint8_t& p : pixels) {
            const int32_t k = picks.Between(0, 5);
            p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
        }
        const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
        std::vector<uint8_t> image(kBamSize, 0);
        image[0xB] = transparent;
        std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
        std::vector<RGBQUAD> colors(256);
        for (RGBQUAD& c : colors) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&c, &bits, 4);
        }
        std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
        for (int i = 0; i < 256; i += 3) {
            colors[i].rgbRed >>= 2;
            colors[i].rgbGreen >>= 2;
            colors[i].rgbBlue >>= 2;
        }
        FRAMEENTRY entry = {};
        entry.nWidth = static_cast<WORD>(width);
        entry.nHeight = static_cast<WORD>(height);
        entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
        for (uint32_t& word : mine) {
            word = picks.Bits();
        }

        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d %s, transparent %u, shadow %s 0x%06X, flags 0x%X", n, width, height,
            compressed ? "RLE" : "raw", transparent, shadowOn ? "on" : "off", shadow, dwFlags);
        ctx.SetInputs(inputs);
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(bam, image.data(), kBamSize);
            std::memcpy(palette, colors.data(), 256 * 4);
            std::memcpy(frame, &entry, sizeof(entry));
            std::memcpy(canvas, mine.data(), kCanvasBytes);
            Put<uint8_t>(cell, 0xD4, shadowOn);
            Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
            originalResult = reinterpret_cast<CellFastBlt24Fn>(kCellBlt8To24Fast)(cell, nullptr, canvas + start,
                kCellCanvas * 3, dwFlags);
        });
        std::memcpy(oursBam.data(), image.data(), kBamSize);
        std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
        oursFrame = entry;
        oursCell->m_bShadowOn = shadowOn;
        video.oursMode->field_24 = shadow;
        const BOOL oursResult = oursCell->sub_7CECD0(reinterpret_cast<BYTE*>(mine.data()) + start,
            kCellCanvas * 3, dwFlags);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::sub_7CF3F0 (0x7CF3F0): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags), `ret 0xc` - the clear-shadow twin of
// sub_7CECD0: the same 24-bpp walk, the source's shadow entry drawn as the
// mode's shadow colour instead of the palette's (half of the destination
// pixel under it when the shadow is on, the mode's field_24 through the
// palette's shadow entry when it is off). Unreachable in iwd2r, which runs at
// 32 bpp only (the DirectDraw shim lists no 24-bit mode), and ported for the
// record.
using CellClearShadow24Fn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch,
    uint32_t dwFlags);
constexpr uintptr_t kCellClearShadow24 = 0x7CF3F0;

// Random frames up to 8x4 (raw and RLE, the transparent index, the shadow
// entry and palette index 0 mixed in), the palette flags, the shadow on and
// off with a random shadow colour, the 24-bit channel masks, drawn at 8 rows
// and 8 pixels into a 96x96 24-bit canvas of random bytes. The whole canvas
// and the result are compared.
void CellClearShadow24(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 3;
    constexpr uint32_t kRMask = 0x00FF0000;
    constexpr uint32_t kGMask = 0x0000FF00;
    constexpr uint32_t kBMask = 0x000000FF;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
        Put<uint32_t>(video.mode, kRMaskOffset, kRMask);
        Put<uint32_t>(video.mode, kGMaskOffset, kGMask);
        Put<uint32_t>(video.mode, kBMaskOffset, kBMask);
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;
    video.oursMode->m_dwRBitMask = kRMask;
    video.oursMode->m_dwGBitMask = kGMask;
    video.oursMode->m_dwBBitMask = kBMask;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x436C725368323400ull); // "ClrSh24"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 3;
    for (int n = 0; n < count; ++n) {
        const int width = picks.Between(1, 8);
        const int height = picks.Between(1, 4);
        const bool compressed = picks.Between(0, 1) != 0;
        const uint8_t transparent = static_cast<uint8_t>(picks.Between(0, 255));
        const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
        const uint32_t shadow = picks.Bits() & 0xFFFFFF;
        const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
        for (uint8_t& p : pixels) {
            const int32_t k = picks.Between(0, 6);
            p = k == 0 ? transparent : (k == 1 ? 1 : (k == 2 ? 0 : static_cast<uint8_t>(picks.Between(1, 255))));
        }
        const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
        std::vector<uint8_t> image(kBamSize, 0);
        image[0xB] = transparent;
        std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
        std::vector<RGBQUAD> colors(256);
        for (RGBQUAD& c : colors) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&c, &bits, 4);
        }
        std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
        for (int i = 0; i < 256; i += 3) {
            colors[i].rgbRed >>= 2;
            colors[i].rgbGreen >>= 2;
            colors[i].rgbBlue >>= 2;
        }
        FRAMEENTRY entry = {};
        entry.nWidth = static_cast<WORD>(width);
        entry.nHeight = static_cast<WORD>(height);
        entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
        for (uint32_t& word : mine) {
            word = picks.Bits();
        }

        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d %s, transparent %u, shadow %s 0x%06X, flags 0x%X", n, width, height,
            compressed ? "RLE" : "raw", transparent, shadowOn ? "on" : "off", shadow, dwFlags);
        ctx.SetInputs(inputs);
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(bam, image.data(), kBamSize);
            std::memcpy(palette, colors.data(), 256 * 4);
            std::memcpy(frame, &entry, sizeof(entry));
            std::memcpy(canvas, mine.data(), kCanvasBytes);
            Put<uint8_t>(cell, 0xD4, shadowOn);
            Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
            originalResult = reinterpret_cast<CellClearShadow24Fn>(kCellClearShadow24)(cell, nullptr, canvas + start,
                kCellCanvas * 3, dwFlags);
        });
        std::memcpy(oursBam.data(), image.data(), kBamSize);
        std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
        oursFrame = entry;
        oursCell->m_bShadowOn = shadowOn;
        video.oursMode->field_24 = shadow;
        const BOOL oursResult = oursCell->sub_7CF3F0(reinterpret_cast<BYTE*>(mine.data()) + start,
            kCellCanvas * 3, dwFlags);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::sub_7CF650 (0x7CF650): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags), `ret 0xc` - the brightest twin of sub_7CECD0:
// the same 24-bpp walk, each channel of a drawn pixel keeping the larger of
// the destination byte and the realized palette's (B, G, R compared
// separately), the transparent index skipped. Unreachable in iwd2r, which
// runs at 32 bpp only (the DirectDraw shim lists no 24-bit mode), and ported
// for the record.
using CellBrightest24Fn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch,
    uint32_t dwFlags);
constexpr uintptr_t kCellBrightest24 = 0x7CF650;

// Random frames up to 8x4 (raw and RLE, the transparent index and palette
// index 0 mixed in), the palette flags, the shadow on and off with a random
// shadow colour, drawn at 8 rows and 8 pixels into a 96x96 24-bit canvas of
// random bytes. The whole canvas and the result are compared.
void CellBrightest24(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 3;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x4272696732340000ull); // "Brig24"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 3;
    for (int n = 0; n < count; ++n) {
        const int width = picks.Between(1, 8);
        const int height = picks.Between(1, 4);
        const bool compressed = picks.Between(0, 1) != 0;
        const uint8_t transparent = static_cast<uint8_t>(picks.Between(0, 255));
        const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
        const uint32_t shadow = picks.Bits() & 0xFFFFFF;
        const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
        for (uint8_t& p : pixels) {
            const int32_t k = picks.Between(0, 6);
            p = k == 0 ? transparent : (k == 1 ? 1 : (k == 2 ? 0 : static_cast<uint8_t>(picks.Between(1, 255))));
        }
        const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
        std::vector<uint8_t> image(kBamSize, 0);
        image[0xB] = transparent;
        std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
        std::vector<RGBQUAD> colors(256);
        for (RGBQUAD& c : colors) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&c, &bits, 4);
        }
        std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
        for (int i = 0; i < 256; i += 3) {
            colors[i].rgbRed >>= 2;
            colors[i].rgbGreen >>= 2;
            colors[i].rgbBlue >>= 2;
        }
        FRAMEENTRY entry = {};
        entry.nWidth = static_cast<WORD>(width);
        entry.nHeight = static_cast<WORD>(height);
        entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
        for (uint32_t& word : mine) {
            word = picks.Bits();
        }

        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d %s, transparent %u, shadow %s 0x%06X, flags 0x%X", n, width, height,
            compressed ? "RLE" : "raw", transparent, shadowOn ? "on" : "off", shadow, dwFlags);
        ctx.SetInputs(inputs);
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(bam, image.data(), kBamSize);
            std::memcpy(palette, colors.data(), 256 * 4);
            std::memcpy(frame, &entry, sizeof(entry));
            std::memcpy(canvas, mine.data(), kCanvasBytes);
            Put<uint8_t>(cell, 0xD4, shadowOn);
            Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
            originalResult = reinterpret_cast<CellBrightest24Fn>(kCellBrightest24)(cell, nullptr, canvas + start,
                kCellCanvas * 3, dwFlags);
        });
        std::memcpy(oursBam.data(), image.data(), kBamSize);
        std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
        oursFrame = entry;
        oursCell->m_bShadowOn = shadowOn;
        video.oursMode->field_24 = shadow;
        const BOOL oursResult = oursCell->sub_7CF650(reinterpret_cast<BYTE*>(mine.data()) + start,
            kCellCanvas * 3, dwFlags);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::sub_7CF8D0 (0x7CF8D0): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags, nTransVal), `ret 0x10` — the translucent
// 24-bit blit: the fast blit's walk, each drawn pixel blended into the
// destination per byte through the realized 24-bit palette, (src * (256 -
// nTransVal) + dst * nTransVal) >> 8. A pixel whose realized colour is the
// mode's shadow colour is skipped in the compressed walk (not drawn opaque
// as in the 32-bit form); the raw walk skips on the palette entry's blue
// byte, tested against the whole shadow colour and the transparent index
// (the original's clobbered register), so the case picks the shadow colour
// in 0..255 a quarter of the time. Unreachable in iwd2r, which runs at
// 32 bpp only, and ported for the record.
using CellTrans24Fn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch,
    uint32_t dwFlags, int32_t nTransVal);
constexpr uintptr_t kCellTrans24 = 0x7CF8D0;

// Random frames up to 8x4 (raw and RLE, transparent pixels mixed in), the
// palette flags, the shadow entry on and off, nTransVal 0..256, drawn at
// 8 rows and 8 pixels into a 64x64 24-bit canvas of random bytes. The whole
// canvas and the result are compared.
void CellTranslucent24(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 3;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x43656C6C54323400ull); // "CellT24"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 3;
    for (int n = 0; n < count; ++n) {
        const int width = picks.Between(1, 8);
        const int height = picks.Between(1, 4);
        const bool compressed = picks.Between(0, 1) != 0;
        const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
        const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
        const uint32_t shadow = picks.Between(0, 3) == 0
            ? static_cast<uint32_t>(picks.Between(0, 255))
            : picks.Bits() & 0xFFFFFF;
        const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
        const int32_t nTransVal = picks.Between(0, 256);
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
        for (uint8_t& p : pixels) {
            const int32_t k = picks.Between(0, 5);
            p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
        }
        const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
        std::vector<uint8_t> image(kBamSize, 0);
        image[0xB] = transparent;
        std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
        std::vector<RGBQUAD> colors(256);
        for (RGBQUAD& c : colors) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&c, &bits, 4);
        }
        std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
        // Light and dark colours both occur.
        for (int i = 0; i < 256; i += 3) {
            colors[i].rgbRed >>= 2;
            colors[i].rgbGreen >>= 2;
            colors[i].rgbBlue >>= 2;
        }
        FRAMEENTRY entry = {};
        entry.nWidth = static_cast<WORD>(width);
        entry.nHeight = static_cast<WORD>(height);
        entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
        for (uint32_t& word : mine) {
            word = picks.Bits();
        }

        char inputs[240];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d %s, transparent %u, shadow %s 0x%06X, flags 0x%X, nTransVal %d", n, width, height,
            compressed ? "RLE" : "raw", transparent, shadowOn ? "on" : "off", shadow, dwFlags, nTransVal);
        ctx.SetInputs(inputs);
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(bam, image.data(), kBamSize);
            std::memcpy(palette, colors.data(), 256 * 4);
            std::memcpy(frame, &entry, sizeof(entry));
            std::memcpy(canvas, mine.data(), kCanvasBytes);
            Put<uint8_t>(cell, 0xD4, shadowOn);
            Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
            originalResult = reinterpret_cast<CellTrans24Fn>(kCellTrans24)(cell, nullptr, canvas + start,
                kCellCanvas * 3, dwFlags, nTransVal);
        });
        std::memcpy(oursBam.data(), image.data(), kBamSize);
        std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
        oursFrame = entry;
        oursCell->m_bShadowOn = shadowOn;
        video.oursMode->field_24 = shadow;
        const BOOL oursResult = oursCell->sub_7CF8D0(reinterpret_cast<BYTE*>(mine.data()) + start,
            kCellCanvas * 3, dwFlags, nTransVal);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

void CellBlt8To32Point(CaseContext& ctx)
{
    CellPointBlt(ctx, 32, 0x426C743332000000ull); // "Blt32"
}

// CVidCell::sub_7AF8A0 (0x7AF8A0): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags), `ret 0xc` - the clear-shadow 16-bit blit, one
// WORD a pixel, the transparent index skipped, the palette realized for 16 bpp
// with 255, rows bottom-up by lPitch in bytes. Unreachable in iwd2r, which
// runs at 32 bpp only (the DirectDraw shim lists no 16-bit mode), and ported
// for the record; it is the fast twin of the clipped Blt8To16 (0x7AF3C0)
// above. With the cell's shadow on, index 1 is half the destination per
// channel (through the mode's masks); the raw walk halves any index below two.
// With the shadow off the shadow entry is the mode's colour and the frame is
// drawn plainly.
using CellClearShadow16Fn = int32_t(__fastcall*)(void* self, void* edx, uint16_t* pSurface, int32_t lPitch,
    uint32_t dwFlags);
constexpr uintptr_t kCellClearShadow16 = 0x7AF8A0;

// Random frames up to 8x4 (raw and RLE, transparent pixels mixed in), the
// palette flags, the shadow entry, drawn at 8 rows and 8 pixels into a 64x64
// 16-bit canvas of random bytes, on 5-6-5 and 5-5-5 in turn (the realization
// and the shadow's half mask read the mode's masks). The pitch, the row step
// and the shadow/index-0 rules all matter. The whole canvas and the result are
// compared. Compressed frames always run with the cell's shadow on: with it
// off the original halves the palette with a half mask it only computes in the
// shadow-on path (an uninitialized read at 0x7AFA6C), so that path has no
// defined behaviour to compare.
void CellClearShadow16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 2;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x436C725331360000ull); // "ClrS16"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 2;
    for (int format = 0; format < 2; ++format) {
        SetMode16(video, ctx, format == 0);
        for (int n = 0; n < count; ++n) {
            const int width = picks.Between(1, 8);
            const int height = picks.Between(1, 4);
            const bool compressed = picks.Between(0, 1) != 0;
            const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
            const uint8_t shadowOn = compressed ? 1 : static_cast<uint8_t>(picks.Between(0, 1));
            const uint32_t shadow = picks.Bits() & 0xFFFF;
            const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
            std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
            for (uint8_t& p : pixels) {
                const int32_t k = picks.Between(0, 5);
                p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
            }
            const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
            std::vector<uint8_t> image(kBamSize, 0);
            image[0xB] = transparent;
            std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
            std::vector<RGBQUAD> colors(256);
            for (RGBQUAD& c : colors) {
                const uint32_t bits = picks.Bits();
                std::memcpy(&c, &bits, 4);
            }
            std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
            for (int i = 0; i < 256; i += 3) {
                colors[i].rgbRed >>= 2;
                colors[i].rgbGreen >>= 2;
                colors[i].rgbBlue >>= 2;
            }
            FRAMEENTRY entry = {};
            entry.nWidth = static_cast<WORD>(width);
            entry.nHeight = static_cast<WORD>(height);
            entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
            for (uint32_t& word : mine) {
                word = picks.Bits();
            }

            char inputs[200];
            std::snprintf(inputs, sizeof(inputs),
                "input %d (%s): %dx%d %s, transparent %u, shadow %s 0x%04X, flags 0x%X", n,
                format == 0 ? "5-6-5" : "5-5-5", width, height, compressed ? "RLE" : "raw", transparent,
                shadowOn ? "on" : "off", shadow, dwFlags);
            ctx.SetInputs(inputs);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(bam, image.data(), kBamSize);
                std::memcpy(palette, colors.data(), 256 * 4);
                std::memcpy(frame, &entry, sizeof(entry));
                std::memcpy(canvas, mine.data(), kCanvasBytes);
                Put<uint8_t>(cell, 0xD4, shadowOn);
                Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
                originalResult = reinterpret_cast<CellClearShadow16Fn>(kCellClearShadow16)(cell, nullptr,
                    reinterpret_cast<uint16_t*>(canvas + start), kCellCanvas * 2, dwFlags);
            });
            std::memcpy(oursBam.data(), image.data(), kBamSize);
            std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
            oursFrame = entry;
            oursCell->m_bShadowOn = shadowOn;
            video.oursMode->field_24 = static_cast<int>(shadow);
            const BOOL oursResult = oursCell->sub_7AF8A0(
                reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(mine.data()) + start), kCellCanvas * 2, dwFlags);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
            }
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::sub_7AFB50 (0x7AFB50): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags), `ret 0xc` - the brightest 16-bit blit, one WORD
// a pixel, the transparent index skipped, the palette realized for 16 bpp with
// 255, rows bottom-up by lPitch in bytes, each drawn pixel the per-channel
// brighter of the destination and the palette through the mode's masks.
// Unreachable in iwd2r, which runs at 32 bpp only (the DirectDraw shim lists
// no 16-bit mode), and ported for the record; the 16-bit twin of
// Blt8To32Brightest.
using CellBrightest16Fn = int32_t(__fastcall*)(void* self, void* edx, uint16_t* pSurface, int32_t lPitch,
    uint32_t dwFlags);
constexpr uintptr_t kCellBrightest16 = 0x7AFB50;

// Random frames up to 8x4 (raw and RLE, transparent pixels mixed in), the
// palette flags, the shadow entry on and off, drawn at 8 rows and 8 pixels
// into a 64x64 16-bit canvas of random bytes, on 5-6-5 and 5-5-5 in turn (the
// realization and the per-channel max read the mode's masks). The whole canvas
// and the result are compared.
void CellBrightest16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 2;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x436C723136427200ull); // "Clr16Br"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 2;
    for (int format = 0; format < 2; ++format) {
        SetMode16(video, ctx, format == 0);
        for (int n = 0; n < count; ++n) {
            const int width = picks.Between(1, 8);
            const int height = picks.Between(1, 4);
            const bool compressed = picks.Between(0, 1) != 0;
            const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
            const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
            const uint32_t shadow = picks.Bits() & 0xFFFF;
            const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
            std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
            for (uint8_t& p : pixels) {
                const int32_t k = picks.Between(0, 5);
                p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
            }
            const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
            std::vector<uint8_t> image(kBamSize, 0);
            image[0xB] = transparent;
            std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
            std::vector<RGBQUAD> colors(256);
            for (RGBQUAD& c : colors) {
                const uint32_t bits = picks.Bits();
                std::memcpy(&c, &bits, 4);
            }
            std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
            for (int i = 0; i < 256; i += 3) {
                colors[i].rgbRed >>= 2;
                colors[i].rgbGreen >>= 2;
                colors[i].rgbBlue >>= 2;
            }
            FRAMEENTRY entry = {};
            entry.nWidth = static_cast<WORD>(width);
            entry.nHeight = static_cast<WORD>(height);
            entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
            for (uint32_t& word : mine) {
                word = picks.Bits();
            }

            char inputs[200];
            std::snprintf(inputs, sizeof(inputs),
                "input %d (%s): %dx%d %s, transparent %u, shadow %s 0x%04X, flags 0x%X", n,
                format == 0 ? "5-6-5" : "5-5-5", width, height, compressed ? "RLE" : "raw", transparent,
                shadowOn ? "on" : "off", shadow, dwFlags);
            ctx.SetInputs(inputs);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(bam, image.data(), kBamSize);
                std::memcpy(palette, colors.data(), 256 * 4);
                std::memcpy(frame, &entry, sizeof(entry));
                std::memcpy(canvas, mine.data(), kCanvasBytes);
                Put<uint8_t>(cell, 0xD4, shadowOn);
                Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
                originalResult = reinterpret_cast<CellBrightest16Fn>(kCellBrightest16)(cell, nullptr,
                    reinterpret_cast<uint16_t*>(canvas + start), kCellCanvas * 2, dwFlags);
            });
            std::memcpy(oursBam.data(), image.data(), kBamSize);
            std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
            oursFrame = entry;
            oursCell->m_bShadowOn = shadowOn;
            video.oursMode->field_24 = static_cast<int>(shadow);
            const BOOL oursResult = oursCell->sub_7AFB50(
                reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(mine.data()) + start), kCellCanvas * 2, dwFlags);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
            }
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidCell::sub_7AFED0 (0x7AFED0): the whole frame at the surface pointer,
// (pSurface, lPitch, dwFlags, nTransVal), `ret 0x10` - the translucent 16-bit
// blit, one WORD a pixel, the palette realized for 16 bpp with 255, rows
// bottom-up by lPitch in bytes, each drawn pixel blended through the 64x64x32
// table at weight nTransVal >> 3 (the table's row is the destination channel,
// its column the palette's), a palette word equal to the mode's shadow colour
// left untouched. Unreachable in iwd2r, which runs at 32 bpp only (the
// DirectDraw shim lists no 16-bit mode), and ported for the record; the 16-bit
// twin of sub_7D0950 (Blt8To32Translucent). The raw walk draws height + 1 rows
// and does not skip the transparent index.
using CellTranslucent16Fn = int32_t(__fastcall*)(void* self, void* edx, uint16_t* pSurface, int32_t lPitch,
    uint32_t dwFlags, int32_t nTransVal);
constexpr uintptr_t kCellTranslucent16 = 0x7AFED0;

// Random frames up to 8x4 (raw and RLE, transparent pixels mixed in), the
// palette flags, the shadow entry on and off, the mode's shadow colour and
// nTransVal (including 0 and 256), drawn at 8 rows and 8 pixels into a 64x64
// 16-bit canvas of random bytes, on 5-6-5 and 5-5-5 in turn (the realization,
// the blend table and the masks all matter). The whole canvas and the result
// are compared.
void CellTranslucent16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* cell = nullptr;
    uint8_t* res = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* palette = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* canvas = nullptr;
    constexpr size_t kBamSize = 0x1000;
    constexpr int kCanvasBytes = kCellCanvas * kCellCanvas * 2;
    ctx.SetInputs("building the original's cell");
    ctx.CallOriginal([&] {
        cell = OriginalBlock(kCellSize);
        res = OriginalBlock(kResCellSize);
        bam = OriginalBlock(kBamSize);
        palette = OriginalBlock(256 * 4);
        frame = OriginalBlock(12);
        canvas = OriginalBlock(kCanvasBytes);
        Put<uint32_t>(res, 0x50, 1);
        Put<uint32_t>(res, 0x54, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bam)));
        Put<uint32_t>(res, 0x6A, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(palette)));
        Put<uint32_t>(cell, 0xA8, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<uint32_t>(cell, 0xD0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame)));
    });

    auto* oursCellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(oursCellStorage, 0, sizeof(CVidCell));
    auto* oursResStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(oursResStorage, 0, sizeof(CResCell));
    auto* oursCell = reinterpret_cast<CVidCell*>(oursCellStorage);
    auto* oursRes = reinterpret_cast<CResCell*>(oursResStorage);
    std::vector<uint8_t> oursBam(kBamSize);
    std::vector<RGBQUAD> oursPalette(256);
    FRAMEENTRY oursFrame = {};
    oursRes->m_bParsed = TRUE;
    oursRes->m_pBamHeader = reinterpret_cast<BAMHEADER*>(oursBam.data());
    oursRes->m_pPalette = oursPalette.data();
    static_cast<CResHelper<CResCell, 1000>*>(oursCell)->pRes = oursRes;
    oursCell->m_pFrame = &oursFrame;

    std::vector<uint32_t> mine(kCanvasBytes / 4);
    Picks picks(ctx.seed, 0x5472313600000000ull); // "Tr16"
    const uint32_t kFlags[] = { 0, 0x10000, 0x80000, 0x200000 };
    const int count = 400 + ctx.iterations / 20;
    const int start = (8 * kCellCanvas + 8) * 2;
    for (int format = 0; format < 2; ++format) {
        SetMode16(video, ctx, format == 0);
        for (int n = 0; n < count; ++n) {
            const int width = picks.Between(1, 8);
            const int height = picks.Between(1, 4);
            const bool compressed = picks.Between(0, 1) != 0;
            const uint8_t transparent = static_cast<uint8_t>(picks.Between(1, 255));
            const uint8_t shadowOn = static_cast<uint8_t>(picks.Between(0, 1));
            const uint32_t shadow = picks.Bits() & 0xFFFF;
            const uint32_t dwFlags = kFlags[picks.Between(0, 3)];
            const int32_t nTransVal = picks.Between(0, 3) == 0
                ? (picks.Between(0, 1) == 0 ? 0 : 256)
                : picks.Between(1, 255);
            std::vector<uint8_t> pixels(static_cast<size_t>(width * height));
            for (uint8_t& p : pixels) {
                const int32_t k = picks.Between(0, 5);
                p = k == 0 ? transparent : (k == 1 ? 1 : static_cast<uint8_t>(picks.Between(1, 255)));
            }
            const std::vector<uint8_t> data = compressed ? RunLengthEncode(pixels, transparent) : pixels;
            std::vector<uint8_t> image(kBamSize, 0);
            image[0xB] = transparent;
            std::memcpy(image.data() + kFrameDataOffset, data.data(), data.size());
            std::vector<RGBQUAD> colors(256);
            for (RGBQUAD& c : colors) {
                const uint32_t bits = picks.Bits();
                std::memcpy(&c, &bits, 4);
            }
            std::memset(&colors[0], 0, sizeof(RGBQUAD)); // the bytes after raw data
            for (int i = 0; i < 256; i += 3) {
                colors[i].rgbRed >>= 2;
                colors[i].rgbGreen >>= 2;
                colors[i].rgbBlue >>= 2;
            }
            FRAMEENTRY entry = {};
            entry.nWidth = static_cast<WORD>(width);
            entry.nHeight = static_cast<WORD>(height);
            entry.nOffset = static_cast<DWORD>(kFrameDataOffset) | (compressed ? 0u : 0x80000000u);
            for (uint32_t& word : mine) {
                word = picks.Bits();
            }

            char inputs[220];
            std::snprintf(inputs, sizeof(inputs),
                "input %d (%s): %dx%d %s, transparent %u, shadow %s 0x%04X, flags 0x%X, nTransVal %d", n,
                format == 0 ? "5-6-5" : "5-5-5", width, height, compressed ? "RLE" : "raw", transparent,
                shadowOn ? "on" : "off", shadow, dwFlags, nTransVal);
            ctx.SetInputs(inputs);
            int32_t originalResult = 0;
            ctx.CallOriginal([&] {
                std::memcpy(bam, image.data(), kBamSize);
                std::memcpy(palette, colors.data(), 256 * 4);
                std::memcpy(frame, &entry, sizeof(entry));
                std::memcpy(canvas, mine.data(), kCanvasBytes);
                Put<uint8_t>(cell, 0xD4, shadowOn);
                Put<uint32_t>(video.mode, kShadowColorOffset, shadow);
                originalResult = reinterpret_cast<CellTranslucent16Fn>(kCellTranslucent16)(cell, nullptr,
                    reinterpret_cast<uint16_t*>(canvas + start), kCellCanvas * 2, dwFlags, nTransVal);
            });
            std::memcpy(oursBam.data(), image.data(), kBamSize);
            std::memcpy(oursPalette.data(), colors.data(), 256 * 4);
            oursFrame = entry;
            oursCell->m_bShadowOn = shadowOn;
            video.oursMode->field_24 = static_cast<int>(shadow);
            const BOOL oursResult = oursCell->sub_7AFED0(
                reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(mine.data()) + start), kCellCanvas * 2, dwFlags,
                nTransVal);
            ctx.CountInputs(1);
            if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
                CompareArrays(ctx, reinterpret_cast<const uint32_t*>(canvas), mine.data(), kCanvasBytes / 4);
            }
        }
    }
    ::operator delete(oursResStorage);
    ::operator delete(oursCellStorage);
}

// CVidMode::DrawLine16(nXFrom, nYFrom, nXTo, nYTo, pSurface, lPitch,
// rSurface, color, bClipped) (0x797D30), `ret 0x24`, with its run-slice
// helper ScanLine16 (0x79A860). Unreachable in iwd2r (32 bpp only). lPitch
// is in pixels and pSurface the surface rectangle's top left, as DrawLine
// passes them.
using DrawLine16Fn = int32_t(__fastcall*)(void* self, void* edx, int32_t nXFrom, int32_t nYFrom, int32_t nXTo,
    int32_t nYTo, uint16_t* pSurface, int32_t lPitch, const void* rSurface, uint32_t color, uint32_t bClipped);
constexpr uintptr_t kDrawLine16 = 0x797D30;

// DrawLine16 on the 96x96 canvas with a 16-pixel margin, as ThickLine32:
// vertical, horizontal, diagonal and general lines in every direction,
// clipped by the function or already inside. The whole canvas and the
// result are compared.
void DrawLine16(CaseContext& ctx)
{
    uint8_t* self = nullptr;
    uint16_t* canvas = nullptr;
    uint8_t* rect = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(kVidInfSize);
        canvas = reinterpret_cast<uint16_t*>(OriginalBlock(kCanvas * kCanvas * 2));
        rect = OriginalBlock(16);
    });
    const int32_t r[4] = { kMargin, kMargin, kCanvas - kMargin, kCanvas - kMargin };
    std::memcpy(rect, r, sizeof(r));
    CVidInf ours;
    CRect rSurface(r[0], r[1], r[2], r[3]);
    std::vector<WORD> mine(kCanvas * kCanvas);
    Picks picks(ctx.seed, 0x4C696E6531360000ull); // "Line16"
    int n = 0;

    auto check = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool bClipped) {
        const uint16_t color = static_cast<uint16_t>(picks.Bits() | 1);
        std::memset(canvas, 0, kCanvas * kCanvas * 2);
        std::fill(mine.begin(), mine.end(), static_cast<WORD>(0));
        ctx.SetInputs("input " + std::to_string(n++) + ": DrawLine16(" + std::to_string(x1) + ", "
            + std::to_string(y1) + ", " + std::to_string(x2) + ", " + std::to_string(y2) + ", bClipped "
            + std::to_string(bClipped) + ")");
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            originalResult = reinterpret_cast<DrawLine16Fn>(kDrawLine16)(self, nullptr, x1, y1, x2, y2,
                canvas + kMargin * kCanvas + kMargin, kCanvas, rect, color, bClipped ? 1u : 0u);
        });
        const BOOL oursResult = ours.DrawLine16(x1, y1, x2, y2, mine.data() + kMargin * kCanvas + kMargin, kCanvas,
            rSurface, color, bClipped ? TRUE : FALSE);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            std::vector<uint32_t> a(canvas, canvas + kCanvas * kCanvas);
            std::vector<uint32_t> b(mine.begin(), mine.end());
            CompareArrays(ctx, a.data(), b.data(), kCanvas * kCanvas);
        }
    };

    const int32_t lo = kMargin;
    const int32_t hi = kCanvas - kMargin - 1;
    for (int i = 0; i < 64; ++i) {
        const int32_t x = picks.Between(lo, hi);
        const int32_t y = picks.Between(lo, hi);
        const int32_t len = picks.Between(0, 30);
        check(x, y, x, picks.Between(lo, hi), true);
        check(x, y, picks.Between(lo, hi), y, true);
        const int32_t dx = picks.Between(0, 1) ? 1 : -1;
        const int32_t dy = picks.Between(0, 1) ? 1 : -1;
        if (x + dx * len >= lo && x + dx * len <= hi && y + dy * len >= lo && y + dy * len <= hi) {
            check(x, y, x + dx * len, y + dy * len, true);
        }
        check(x, y, picks.Between(lo, hi), picks.Between(lo, hi), true);
    }
    for (int i = 0; i < ctx.iterations / 4; ++i) {
        const int32_t x1 = picks.Between(0, kCanvas - 1);
        const int32_t y1 = picks.Between(0, kCanvas - 1);
        const int32_t x2 = picks.Between(0, 3) == 0 ? x1 : picks.Between(0, kCanvas - 1);
        const int32_t y2 = picks.Between(0, 3) == 0 ? y1 : picks.Between(0, kCanvas - 1);
        check(x1, y1, x2, y2, false);
    }
}

using DrawLine32Fn = int32_t(__fastcall*)(void* self, void* edx, int32_t nXFrom, int32_t nYFrom, int32_t nXTo,
    int32_t nYTo, uint32_t* pSurface, int32_t lPitch, const void* rSurface, uint32_t color, uint32_t bClipped);
constexpr uintptr_t kDrawLine32 = 0x7CBC40;

// DrawLine32 (0x7CBC40) with ScanLine32 (0x7CBD80), as DrawLine16 on 32-bit
// pixels. DrawLine16 on the 96x96 canvas with a 16-pixel margin, as ThickLine32:
// vertical, horizontal, diagonal and general lines in every direction,
// clipped by the function or already inside. The whole canvas and the
// result are compared.
void DrawLine32(CaseContext& ctx)
{
    uint8_t* self = nullptr;
    uint32_t* canvas = nullptr;
    uint8_t* rect = nullptr;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        self = OriginalBlock(kVidInfSize);
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(kCanvas * kCanvas * 4));
        rect = OriginalBlock(16);
    });
    const int32_t r[4] = { kMargin, kMargin, kCanvas - kMargin, kCanvas - kMargin };
    std::memcpy(rect, r, sizeof(r));
    CVidInf ours;
    CRect rSurface(r[0], r[1], r[2], r[3]);
    std::vector<DWORD> mine(kCanvas * kCanvas);
    Picks picks(ctx.seed, 0x4C696E6533320000ull); // "Line32"
    int n = 0;

    auto check = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool bClipped) {
        const uint32_t color = picks.Bits() | 1;
        std::memset(canvas, 0, kCanvas * kCanvas * 4);
        std::fill(mine.begin(), mine.end(), 0u);
        ctx.SetInputs("input " + std::to_string(n++) + ": DrawLine32(" + std::to_string(x1) + ", "
            + std::to_string(y1) + ", " + std::to_string(x2) + ", " + std::to_string(y2) + ", bClipped "
            + std::to_string(bClipped) + ")");
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            originalResult = reinterpret_cast<DrawLine32Fn>(kDrawLine32)(self, nullptr, x1, y1, x2, y2,
                canvas + kMargin * kCanvas + kMargin, kCanvas, rect, color, bClipped ? 1u : 0u);
        });
        const BOOL oursResult = ours.DrawLine32(x1, y1, x2, y2, mine.data() + kMargin * kCanvas + kMargin, kCanvas,
            rSurface, color, bClipped ? TRUE : FALSE);
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            CompareArrays(ctx, canvas, mine.data(), kCanvas * kCanvas);
        }
    };

    const int32_t lo = kMargin;
    const int32_t hi = kCanvas - kMargin - 1;
    for (int i = 0; i < 64; ++i) {
        const int32_t x = picks.Between(lo, hi);
        const int32_t y = picks.Between(lo, hi);
        const int32_t len = picks.Between(0, 30);
        check(x, y, x, picks.Between(lo, hi), true);
        check(x, y, picks.Between(lo, hi), y, true);
        const int32_t dx = picks.Between(0, 1) ? 1 : -1;
        const int32_t dy = picks.Between(0, 1) ? 1 : -1;
        if (x + dx * len >= lo && x + dx * len <= hi && y + dy * len >= lo && y + dy * len <= hi) {
            check(x, y, x + dx * len, y + dy * len, true);
        }
        check(x, y, picks.Between(lo, hi), picks.Between(lo, hi), true);
    }
    for (int i = 0; i < ctx.iterations / 4; ++i) {
        const int32_t x1 = picks.Between(0, kCanvas - 1);
        const int32_t y1 = picks.Between(0, kCanvas - 1);
        const int32_t x2 = picks.Between(0, 3) == 0 ? x1 : picks.Between(0, kCanvas - 1);
        const int32_t y2 = picks.Between(0, 3) == 0 ? y1 : picks.Between(0, kCanvas - 1);
        check(x1, y1, x2, y2, false);
    }
}

// CVidMode::DrawEllipse24(ptCenter, axis, ddsd, rClip, rgbColor) (0x7C8B40),
// `ret 0x14`, with DrawEllipseHOctant24 (0x7C9990) and DrawEllipseVOctant24
// (0x7C8E10) through the vtable, and GetEllipseArcPixelList. Unreachable in
// iwd2r (32 bpp only). It reads the colour shifts and the surface's pitch
// (0x10) and pointer (0x24) from the DDSURFACEDESC.
using DrawEllipse24Fn = int32_t(__fastcall*)(void* self, void* edx, const void* ptCenter, const void* axis,
    const void* ddsd, const void* rClip, uint32_t rgbColor);
constexpr uintptr_t kDrawEllipse24 = 0x7C8B40;
constexpr int kEllipseCanvas = 96;

// Ellipses with half-axes 1 to 30 on a 96x96 24-bit canvas of random bytes:
// without a clip (the whole ellipse on the canvas, the surface at the
// canvas's origin), and with a random clip rectangle (the surface at the
// clip's top left, as DrawEllipse locks it; centres anywhere so that the
// clip cuts the ellipse). The whole canvas is compared.
void DrawEllipse24(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* canvas = nullptr;
    uint8_t* args = nullptr;
    uint8_t* ddsd = nullptr;
    const int pitch = kEllipseCanvas * 3;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        canvas = OriginalBlock(static_cast<size_t>(pitch * kEllipseCanvas));
        args = OriginalBlock(32);
        ddsd = OriginalBlock(0x6C);
    });
    std::vector<uint8_t> mine(static_cast<size_t>(pitch * kEllipseCanvas));
    Picks picks(ctx.seed, 0x456C6C3234000000ull); // "Ell24"
    const int count = 400 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        const int32_t ax = picks.Between(1, 30);
        const int32_t ay = picks.Between(1, 30);
        const bool clipped = picks.Between(0, 1) != 0;
        int32_t cx;
        int32_t cy;
        int32_t r[4] = { 0, 0, 0, 0 };
        if (clipped) {
            r[0] = picks.Between(0, 60);
            r[1] = picks.Between(0, 60);
            r[2] = picks.Between(r[0], 95);
            r[3] = picks.Between(r[1], 95);
            // Every point the octants walk stays on the canvas.
            const int32_t x0 = (std::max)(ax + 1, r[0] - ax);
            const int32_t y0 = (std::max)(ay + 1, r[1] - ay);
            cx = picks.Between(x0, (std::max)(x0, (std::min)(94 - ax, r[2] + ax)));
            cy = picks.Between(y0, (std::max)(y0, (std::min)(94 - ay, r[3] + ay)));
        } else {
            cx = picks.Between(ax + 1, 94 - ax);
            cy = picks.Between(ay + 1, 94 - ay);
        }
        const uint32_t rgb = picks.Bits() & 0xFFFFFF;
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        // The original asserts (UtilAssert faults in the runner) when an
        // arc is longer than the larger half-axis allows, as for long thin
        // ellipses; those are not inputs. The arc lengths come from ours,
        // whose GetEllipseArcPixelList the 32-bit case already exercises.
        std::vector<BYTE> arc(256);
        const LONG nArcH = video.oursMode->GetEllipseArcPixelList(ax, ay, arc.data());
        const LONG nArcV = video.oursMode->GetEllipseArcPixelList(ay, ax, arc.data());
        if (nArcH > (std::max)(ax, ay) || nArcV >= (std::max)(ax, ay)) {
            continue;
        }
        char inputs[200];
        std::snprintf(inputs, sizeof(inputs), "input %d: centre (%d,%d), axis (%d,%d), clip %s (%d,%d)-(%d,%d), rgb 0x%06X",
            n, cx, cy, ax, ay, clipped ? "on" : "off", r[0], r[1], r[2], r[3], rgb);
        ctx.SetInputs(inputs);
        const int32_t offset = clipped ? r[1] * pitch + r[0] * 3 : 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size());
            const int32_t pt[2] = { cx, cy };
            const int32_t size[2] = { ax, ay };
            std::memcpy(args, pt, 8);
            std::memcpy(args + 8, size, 8);
            std::memcpy(args + 16, r, 16);
            Put<int32_t>(ddsd, 0x10, pitch);
            Put<uint32_t>(ddsd, 0x24, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(canvas + offset)));
            reinterpret_cast<DrawEllipse24Fn>(kDrawEllipse24)(video.mode, nullptr, args, args + 8, ddsd,
                clipped ? args + 16 : nullptr, rgb);
        });
        DDSURFACEDESC desc = {};
        desc.lPitch = pitch;
        desc.lpSurface = mine.data() + offset;
        const CRect rClip(r[0], r[1], r[2], r[3]);
        video.oursMode->DrawEllipse24(CPoint(cx, cy), CSize(ax, ay), desc, clipped ? &rClip : nullptr, rgb);
        ctx.CountInputs(1);
        std::vector<uint32_t> a(canvas, canvas + mine.size());
        std::vector<uint32_t> b(mine.begin(), mine.end());
        CompareArrays(ctx, a.data(), b.data(), static_cast<int>(mine.size()));
    }
}

// CVidMode::DrawEllipse16(ptCenter, axis, ddsd, rClip, rgbColor)
// (0x796260), with its octants through the vtable (0x796550, 0x7966B0) and
// GetEllipseArcPixelList. Unreachable in iwd2r (32 bpp only).
constexpr uintptr_t kDrawEllipse16 = 0x796260;

// DrawEllipse24's inputs on a 96x96 16-bit canvas of random words, on a
// 5-6-5 and a 5-5-5 video mode in turn. The whole canvas is compared.
void DrawEllipse16(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* canvas = nullptr;
    uint8_t* args = nullptr;
    uint8_t* ddsd = nullptr;
    const int pitch = kEllipseCanvas * 2;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        canvas = OriginalBlock(static_cast<size_t>(pitch * kEllipseCanvas));
        args = OriginalBlock(32);
        ddsd = OriginalBlock(0x6C);
    });
    std::vector<uint8_t> mine(static_cast<size_t>(pitch * kEllipseCanvas));
    Picks picks(ctx.seed, 0x456C6C3136000000ull); // "Ell16"
    const int count = 400 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        if (n == 0 || n == count / 2) {
            SetMode16(video, ctx, n == 0);
        }
        const int32_t ax = picks.Between(1, 30);
        const int32_t ay = picks.Between(1, 30);
        const bool clipped = picks.Between(0, 1) != 0;
        int32_t cx;
        int32_t cy;
        int32_t r[4] = { 0, 0, 0, 0 };
        if (clipped) {
            r[0] = picks.Between(0, 60);
            r[1] = picks.Between(0, 60);
            r[2] = picks.Between(r[0], 95);
            r[3] = picks.Between(r[1], 95);
            const int32_t x0 = (std::max)(ax + 1, r[0] - ax);
            const int32_t y0 = (std::max)(ay + 1, r[1] - ay);
            cx = picks.Between(x0, (std::max)(x0, (std::min)(94 - ax, r[2] + ax)));
            cy = picks.Between(y0, (std::max)(y0, (std::min)(94 - ay, r[3] + ay)));
        } else {
            cx = picks.Between(ax + 1, 94 - ax);
            cy = picks.Between(ay + 1, 94 - ay);
        }
        const uint32_t rgb = picks.Bits() & 0xFFFFFF;
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        std::vector<BYTE> arc(256);
        const LONG nArcH = video.oursMode->GetEllipseArcPixelList(ax, ay, arc.data());
        const LONG nArcV = video.oursMode->GetEllipseArcPixelList(ay, ax, arc.data());
        if (nArcH > (std::max)(ax, ay) || nArcV >= (std::max)(ax, ay)) {
            continue;
        }
        char inputs[200];
        std::snprintf(inputs, sizeof(inputs), "input %d: centre (%d,%d), axis (%d,%d), clip %s (%d,%d)-(%d,%d), rgb 0x%06X",
            n, cx, cy, ax, ay, clipped ? "on" : "off", r[0], r[1], r[2], r[3], rgb);
        ctx.SetInputs(inputs);
        const int32_t offset = clipped ? r[1] * pitch + r[0] * 2 : 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size());
            const int32_t pt[2] = { cx, cy };
            const int32_t size[2] = { ax, ay };
            std::memcpy(args, pt, 8);
            std::memcpy(args + 8, size, 8);
            std::memcpy(args + 16, r, 16);
            Put<int32_t>(ddsd, 0x10, pitch);
            Put<uint32_t>(ddsd, 0x24, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(canvas + offset)));
            reinterpret_cast<DrawEllipse24Fn>(kDrawEllipse16)(video.mode, nullptr, args, args + 8, ddsd,
                clipped ? args + 16 : nullptr, rgb);
        });
        DDSURFACEDESC desc = {};
        desc.lPitch = pitch;
        desc.lpSurface = mine.data() + offset;
        const CRect rClip(r[0], r[1], r[2], r[3]);
        video.oursMode->DrawEllipse16(CPoint(cx, cy), CSize(ax, ay), desc, clipped ? &rClip : nullptr, rgb);
        ctx.CountInputs(1);
        std::vector<uint32_t> a(canvas, canvas + mine.size());
        std::vector<uint32_t> b(mine.begin(), mine.end());
        CompareArrays(ctx, a.data(), b.data(), static_cast<int>(mine.size()));
    }
}

// CVidTile::BltTile8To16(pSurface, lPitch, pData, blitSize, nDataJump,
// dwFlags) (0x7B22E0), `ret 0x18`. Unreachable in iwd2r (32 bpp only). It
// reads the realized palette (0x9064F8) and, for 0x8, the video mode's
// field_24, not the tile.
using BltTile16Fn = int32_t(__fastcall*)(void* self, void* edx, uint16_t* pSurface, int32_t lPitch,
    const uint8_t* pData, const void* blitSize, int32_t nDataJump, uint32_t dwFlags);
constexpr uintptr_t kBltTile8To16 = 0x7B22E0;

// Tiles of 1 to 32 by 1 to 32 pixels (odd widths too) with a random data
// jump, plain and dithered (0x8) with a random 32-bit field_24, into a
// 64x64 16-bit canvas of random words. The whole canvas is compared.
// CVidTile::BltTile8To32 (0x7CE830, `ret 0x18`, even widths only) and
// BltStencilTile8To32(pSurface, lPitch, pData, pStencilData, blitSize,
// nDataJump, dwFlags) (0x7CE290, `ret 0x1c`): the same fixture on a 32-bit
// canvas, the stencil random with a quarter of its bytes 0 (clear).
using BltStencilTile32Fn = int32_t(__fastcall*)(void* self, void* edx, uint8_t* pSurface, int32_t lPitch,
    const uint8_t* pData, const uint8_t* pStencilData, const void* blitSize, int32_t nDataJump, uint32_t dwFlags);
constexpr uintptr_t kBltTile8To32 = 0x7CE830;
constexpr uintptr_t kBltStencilTile8To32 = 0x7CE290;
// CVidTile::BltStencilTile8To16 (0x7B2440), BltTile8To24 (0x7CDD00) and
// BltStencilTile8To24 (0x7CDF90), `ret 0x18`/`ret 0x1C`, reading the
// realized palette and the video mode's field_24. Unreachable in iwd2r.
constexpr uintptr_t kBltStencilTile8To16 = 0x7B2440;
constexpr uintptr_t kBltTile8To24 = 0x7CDD00;
constexpr uintptr_t kBltStencilTile8To24 = 0x7CDF90;

void BltTile(CaseContext& ctx, int bpp, bool stencil, uint64_t salt)
{
    VideoFixture video(ctx);
    uint8_t* canvas = nullptr;
    uint8_t* stencilData = nullptr;
    uint8_t* data = nullptr;
    uint8_t* size = nullptr;
    uint8_t* tile = nullptr;
    constexpr int kTileCanvas = 64;
    constexpr size_t kDataSize = 64 * 96;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        canvas = OriginalBlock(kTileCanvas * kTileCanvas * 4);
        stencilData = OriginalBlock(kDataSize);
        data = OriginalBlock(kDataSize);
        size = OriginalBlock(8);
        tile = OriginalBlock(0x100);
    });
    auto* oursTileStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidTile)));
    std::memset(oursTileStorage, 0, sizeof(CVidTile));
    auto* oursTile = reinterpret_cast<CVidTile*>(oursTileStorage);
    const int bytes = bpp / 8;
    std::vector<uint8_t> mine(static_cast<size_t>(kTileCanvas * kTileCanvas * bytes));
    std::vector<uint8_t> pixels(kDataSize);
    std::vector<uint8_t> stencils(kDataSize);
    Picks picks(ctx.seed, salt);
    const int count = 600 + ctx.iterations / 10;
    for (int n = 0; n < count; ++n) {
        // The 32-bit blits take even widths only; the 24-bit ones draw
        // whole fours and return FALSE otherwise; the 16-bit stencil blit
        // returns FALSE for an odd width.
        int32_t cx;
        if (bpp == 32) {
            cx = 2 * picks.Between(1, 16);
        } else if (bpp == 24 && picks.Between(0, 3) != 0) {
            cx = 4 * picks.Between(1, 8);
        } else {
            cx = picks.Between(1, 32);
        }
        const int32_t cy = picks.Between(1, 32);
        const int32_t jump = picks.Between(0, 32);
        const uint32_t dwFlags = picks.Between(0, 1) ? 0x8u : 0u;
        const uint32_t color = picks.Bits();
        const int32_t x = picks.Between(0, kTileCanvas - cx);
        const int32_t y = picks.Between(0, kTileCanvas - cy);
        uint32_t palette[256];
        for (uint32_t& c : palette) {
            c = picks.Bits();
        }
        for (uint8_t& b : pixels) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        for (uint8_t& b : stencils) {
            b = picks.Between(0, 3) == 0 ? 0 : static_cast<uint8_t>(picks.Bits());
        }
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        char inputs[200];
        std::snprintf(inputs, sizeof(inputs), "input %d: %dx%d at (%d,%d), jump %d, flags 0x%X, field_24 0x%08X", n,
            cx, cy, x, y, jump, dwFlags, color);
        ctx.SetInputs(inputs);
        const int32_t offset = y * kTileCanvas + x;
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size());
            std::memcpy(data, pixels.data(), kDataSize);
            std::memcpy(stencilData, stencils.data(), kDataSize);
            const int32_t s[2] = { cx, cy };
            std::memcpy(size, s, 8);
            std::memcpy(reinterpret_cast<void*>(kTempPal), palette, sizeof(palette));
            Put<uint32_t>(video.mode, kShadowColorOffset, color);
            if (stencil) {
                const uintptr_t address = bpp == 16 ? kBltStencilTile8To16
                    : bpp == 24                     ? kBltStencilTile8To24
                                                    : kBltStencilTile8To32;
                originalResult = reinterpret_cast<BltStencilTile32Fn>(address)(tile, nullptr, canvas + offset * bytes,
                    kTileCanvas * bytes, data, stencilData, size, jump, dwFlags);
            } else {
                const uintptr_t address = bpp == 16 ? kBltTile8To16 : bpp == 24 ? kBltTile8To24
                                                                                : kBltTile8To32;
                originalResult = reinterpret_cast<BltTile16Fn>(address)(tile, nullptr,
                    reinterpret_cast<uint16_t*>(canvas + offset * bytes), kTileCanvas * bytes, data, size, jump,
                    dwFlags);
            }
        });
        std::memcpy(CVidImage::rgbTempPal, palette, sizeof(palette));
        video.oursMode->field_24 = color;
        BOOL oursResult;
        if (stencil && bpp == 16) {
            oursResult = oursTile->BltStencilTile8To16(reinterpret_cast<WORD*>(mine.data() + offset * bytes),
                kTileCanvas * bytes, pixels.data(), stencils.data(), CSize(cx, cy), jump, dwFlags);
        } else if (stencil && bpp == 24) {
            oursResult = oursTile->BltStencilTile8To24(reinterpret_cast<DWORD*>(mine.data() + offset * bytes),
                kTileCanvas * bytes, pixels.data(), stencils.data(), CSize(cx, cy), jump, dwFlags);
        } else if (stencil) {
            oursResult = oursTile->BltStencilTile8To32(reinterpret_cast<DWORD*>(mine.data() + offset * bytes),
                kTileCanvas * bytes, pixels.data(), stencils.data(), CSize(cx, cy), jump, dwFlags);
        } else if (bpp == 24) {
            oursResult = oursTile->BltTile8To24(reinterpret_cast<DWORD*>(mine.data() + offset * bytes),
                kTileCanvas * bytes, pixels.data(), CSize(cx, cy), jump, dwFlags);
        } else if (bpp == 16) {
            oursResult = oursTile->BltTile8To16(reinterpret_cast<WORD*>(mine.data() + offset * bytes),
                kTileCanvas * bytes, pixels.data(), CSize(cx, cy), jump, dwFlags);
        } else {
            oursResult = oursTile->BltTile8To32(reinterpret_cast<DWORD*>(mine.data() + offset * bytes),
                kTileCanvas * bytes, pixels.data(), CSize(cx, cy), jump, dwFlags);
        }
        ctx.CountInputs(1);
        if (oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult))) {
            std::vector<uint32_t> a(canvas, canvas + mine.size());
            std::vector<uint32_t> b(mine.begin(), mine.end());
            CompareArrays(ctx, a.data(), b.data(), static_cast<int>(mine.size()));
        }
    }
    ::operator delete(oursTileStorage);
}

void BltTile8To16(CaseContext& ctx)
{
    BltTile(ctx, 16, false, 0x54696C6531360000ull); // "Tile16"
}

void BltTile8To32(CaseContext& ctx)
{
    BltTile(ctx, 32, false, 0x54696C6533320000ull); // "Tile32"
}

void BltStencilTile8To32(CaseContext& ctx)
{
    BltTile(ctx, 32, true, 0x5374656E33320000ull); // "Sten32"
}

void BltStencilTile8To16(CaseContext& ctx)
{
    BltTile(ctx, 16, true, 0x5374656E31360000ull); // "Sten16"
}

void BltTile8To24(CaseContext& ctx)
{
    BltTile(ctx, 24, false, 0x54696C6532340000ull); // "Tile24"
}

void BltStencilTile8To24(CaseContext& ctx)
{
    BltTile(ctx, 24, true, 0x5374656E32340000ull); // "Sten24"
}

// CVidMode::DrawEllipse32 (0x7CA840) and DrawRecticle32 (0x7CB670), both
// `ret 0x10`/`ret 0x14` with the same DDSURFACEDESC reads as
// DrawEllipse24, through the vtable to the 32-bit octants and
// GetEllipseArcPixelList, and (the reticle) DrawLine32.
using DrawEllipse32Fn = int32_t(__fastcall*)(void* self, void* edx, const void* ptCenter, const void* axis,
    const void* ddsd, const void* rClip, uint32_t rgbColor);
using DrawRecticle32Fn = void(__fastcall*)(void* self, void* edx, const void* desc, const void* ddsd,
    const void* rClip, uint32_t rgbColor);
constexpr uintptr_t kDrawEllipse32 = 0x7CA840;
constexpr uintptr_t kDrawRecticle32 = 0x7CB670;
constexpr int kShapeCanvas = 128;

// Ellipses and reticles on a 128x128 32-bit canvas of random pixels, with
// and without a random clip (the surface at the clip's top left when there
// is one, as the callers lock it). Half-axes 2 to 30; a reticle's gaps stay
// below its arcs and its pie offsets small, so every point and line stays
// on the canvas. Inputs on which the original asserts are skipped, as in
// DrawEllipse24. The whole canvas is compared.
void Shapes32(CaseContext& ctx, bool recticle, uint64_t salt)
{
    VideoFixture video(ctx);
    uint32_t* canvas = nullptr;
    uint8_t* args = nullptr;
    uint8_t* ddsd = nullptr;
    const int pitch = kShapeCanvas * 4;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        canvas = reinterpret_cast<uint32_t*>(OriginalBlock(static_cast<size_t>(pitch * kShapeCanvas)));
        args = OriginalBlock(64);
        ddsd = OriginalBlock(0x6C);
    });
    std::vector<DWORD> mine(static_cast<size_t>(kShapeCanvas * kShapeCanvas));
    Picks picks(ctx.seed, salt);
    const int count = 500 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        const int32_t ax = picks.Between(2, 30);
        const int32_t ay = picks.Between(2, 30);
        std::vector<BYTE> arc(256);
        const LONG nArcH = video.oursMode->GetEllipseArcPixelList(ax, ay, arc.data());
        const LONG nArcV = video.oursMode->GetEllipseArcPixelList(ay, ax, arc.data());
        if (nArcH > (std::max)(ax, ay) || nArcV >= (std::max)(ax, ay) || nArcH < 3 || nArcV < 3) {
            continue;
        }
        CVIDMODE_RECTICLE_DESCRIPTION desc = {};
        desc.xAxis = static_cast<SHORT>(ax);
        desc.yAxis = static_cast<SHORT>(ay);
        desc.xGap = static_cast<SHORT>(picks.Between(0, nArcH - 3));
        desc.yGap = static_cast<SHORT>(picks.Between(0, nArcV - 2));
        desc.piePieceXOffset = static_cast<SHORT>(picks.Between(0, 6));
        desc.piePieceYOffset = static_cast<SHORT>(picks.Between(0, 6));
        desc.piePiecePtXOffset = static_cast<SHORT>(picks.Between(0, 6));
        desc.piePiecePtYOffset = static_cast<SHORT>(picks.Between(0, 6));
        const bool clipped = picks.Between(0, 1) != 0;
        int32_t r[4] = { 0, 0, 0, 0 };
        if (clipped) {
            r[0] = picks.Between(0, 90);
            r[1] = picks.Between(0, 90);
            r[2] = picks.Between(r[0] + 1, 127);
            r[3] = picks.Between(r[1] + 1, 127);
        }
        const int32_t cx = picks.Between(44, 83);
        const int32_t cy = picks.Between(44, 83);
        desc.ptCenter = CPoint(cx, cy);
        const uint32_t rgb = picks.Bits() & 0xFFFFFF;
        for (DWORD& pixel : mine) {
            pixel = picks.Bits();
        }
        char inputs[240];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: centre (%d,%d), axis (%d,%d), gap (%d,%d), pie (%d,%d) pt (%d,%d), clip %s (%d,%d)-(%d,%d)", n,
            cx, cy, ax, ay, desc.xGap, desc.yGap, desc.piePieceXOffset, desc.piePieceYOffset, desc.piePiecePtXOffset,
            desc.piePiecePtYOffset, clipped ? "on" : "off", r[0], r[1], r[2], r[3]);
        ctx.SetInputs(inputs);
        const int32_t offset = clipped ? r[1] * kShapeCanvas + r[0] : 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size() * 4);
            const int32_t pt[2] = { cx, cy };
            const int32_t size[2] = { ax, ay };
            std::memcpy(args, pt, 8);
            std::memcpy(args + 8, size, 8);
            std::memcpy(args + 16, r, 16);
            Put<int32_t>(args + 32, 0, cx);
            Put<int32_t>(args + 32, 4, cy);
            Put<int16_t>(args + 32, 8, desc.xAxis);
            Put<int16_t>(args + 32, 10, desc.yAxis);
            Put<int16_t>(args + 32, 12, desc.piePiecePtXOffset);
            Put<int16_t>(args + 32, 14, desc.piePiecePtYOffset);
            Put<int16_t>(args + 32, 16, desc.piePieceXOffset);
            Put<int16_t>(args + 32, 18, desc.piePieceYOffset);
            Put<int16_t>(args + 32, 20, desc.xGap);
            Put<int16_t>(args + 32, 22, desc.yGap);
            Put<int32_t>(ddsd, 0x10, pitch);
            Put<uint32_t>(ddsd, 0x24, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(canvas + offset)));
            if (recticle) {
                reinterpret_cast<DrawRecticle32Fn>(kDrawRecticle32)(video.mode, nullptr, args + 32, ddsd,
                    clipped ? args + 16 : nullptr, rgb);
            } else {
                reinterpret_cast<DrawEllipse32Fn>(kDrawEllipse32)(video.mode, nullptr, args, args + 8, ddsd,
                    clipped ? args + 16 : nullptr, rgb);
            }
        });
        DDSURFACEDESC surface = {};
        surface.lPitch = pitch;
        surface.lpSurface = mine.data() + offset;
        const CRect rClip(r[0], r[1], r[2], r[3]);
        if (recticle) {
            video.oursMode->DrawRecticle32(desc, surface, clipped ? &rClip : nullptr, rgb);
        } else {
            video.oursMode->DrawEllipse32(CPoint(cx, cy), CSize(ax, ay), surface, clipped ? &rClip : nullptr, rgb);
        }
        ctx.CountInputs(1);
        CompareArrays(ctx, canvas, mine.data(), kShapeCanvas * kShapeCanvas);
    }
}

// CVidMode::DrawRecticle16 (0x7989B0) and DrawRecticle24 (0x7C9B10), as
// DrawRecticle32 with the depth's octants and line. Unreachable in iwd2r.
constexpr uintptr_t kDrawRecticle16 = 0x7989B0;
constexpr uintptr_t kDrawRecticle24 = 0x7C9B10;

// Shapes32's reticles on a 128x128 16- or 24-bit canvas of random bytes
// (the 16-bit ones on a 5-6-5 and a 5-5-5 mode in turn). The whole canvas
// is compared byte by byte.
void RecticleDepth(CaseContext& ctx, int nBpp, uint64_t salt)
{
    VideoFixture video(ctx);
    const int bytes = nBpp / 8;
    uint8_t* canvas = nullptr;
    uint8_t* args = nullptr;
    uint8_t* ddsd = nullptr;
    const int pitch = kShapeCanvas * bytes;
    ctx.SetInputs("building the original's canvas");
    ctx.CallOriginal([&] {
        canvas = OriginalBlock(static_cast<size_t>(pitch * kShapeCanvas));
        args = OriginalBlock(64);
        ddsd = OriginalBlock(0x6C);
    });
    std::vector<uint8_t> mine(static_cast<size_t>(pitch * kShapeCanvas));
    Picks picks(ctx.seed, salt);
    const int count = 500 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        if (nBpp == 16 && (n == 0 || n == count / 2)) {
            SetMode16(video, ctx, n == 0);
        }
        const int32_t ax = picks.Between(2, 30);
        const int32_t ay = picks.Between(2, 30);
        std::vector<BYTE> arc(256);
        const LONG nArcH = video.oursMode->GetEllipseArcPixelList(ax, ay, arc.data());
        const LONG nArcV = video.oursMode->GetEllipseArcPixelList(ay, ax, arc.data());
        if (nArcH > (std::max)(ax, ay) || nArcV >= (std::max)(ax, ay) || nArcH < 3 || nArcV < 3) {
            continue;
        }
        CVIDMODE_RECTICLE_DESCRIPTION desc = {};
        desc.xAxis = static_cast<SHORT>(ax);
        desc.yAxis = static_cast<SHORT>(ay);
        desc.xGap = static_cast<SHORT>(picks.Between(0, nArcH - 3));
        desc.yGap = static_cast<SHORT>(picks.Between(0, nArcV - 2));
        desc.piePieceXOffset = static_cast<SHORT>(picks.Between(0, 6));
        desc.piePieceYOffset = static_cast<SHORT>(picks.Between(0, 6));
        desc.piePiecePtXOffset = static_cast<SHORT>(picks.Between(0, 6));
        desc.piePiecePtYOffset = static_cast<SHORT>(picks.Between(0, 6));
        const bool clipped = picks.Between(0, 1) != 0;
        int32_t r[4] = { 0, 0, 0, 0 };
        if (clipped) {
            r[0] = picks.Between(0, 90);
            r[1] = picks.Between(0, 90);
            r[2] = picks.Between(r[0] + 1, 127);
            r[3] = picks.Between(r[1] + 1, 127);
        }
        const int32_t cx = picks.Between(44, 83);
        const int32_t cy = picks.Between(44, 83);
        desc.ptCenter = CPoint(cx, cy);
        const uint32_t rgb = picks.Bits() & 0xFFFFFF;
        for (uint8_t& b : mine) {
            b = static_cast<uint8_t>(picks.Bits());
        }
        char inputs[240];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: centre (%d,%d), axis (%d,%d), gap (%d,%d), pie (%d,%d) pt (%d,%d), clip %s (%d,%d)-(%d,%d)", n,
            cx, cy, ax, ay, desc.xGap, desc.yGap, desc.piePieceXOffset, desc.piePieceYOffset, desc.piePiecePtXOffset,
            desc.piePiecePtYOffset, clipped ? "on" : "off", r[0], r[1], r[2], r[3]);
        ctx.SetInputs(inputs);
        const int32_t offset = clipped ? r[1] * pitch + r[0] * bytes : 0;
        ctx.CallOriginal([&] {
            std::memcpy(canvas, mine.data(), mine.size());
            std::memcpy(args + 16, r, 16);
            Put<int32_t>(args + 32, 0, cx);
            Put<int32_t>(args + 32, 4, cy);
            Put<int16_t>(args + 32, 8, desc.xAxis);
            Put<int16_t>(args + 32, 10, desc.yAxis);
            Put<int16_t>(args + 32, 12, desc.piePiecePtXOffset);
            Put<int16_t>(args + 32, 14, desc.piePiecePtYOffset);
            Put<int16_t>(args + 32, 16, desc.piePieceXOffset);
            Put<int16_t>(args + 32, 18, desc.piePieceYOffset);
            Put<int16_t>(args + 32, 20, desc.xGap);
            Put<int16_t>(args + 32, 22, desc.yGap);
            Put<int32_t>(ddsd, 0x10, pitch);
            Put<uint32_t>(ddsd, 0x24, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(canvas + offset)));
            reinterpret_cast<DrawRecticle32Fn>(nBpp == 16 ? kDrawRecticle16 : kDrawRecticle24)(video.mode, nullptr,
                args + 32, ddsd, clipped ? args + 16 : nullptr, rgb);
        });
        DDSURFACEDESC surface = {};
        surface.lPitch = pitch;
        surface.lpSurface = mine.data() + offset;
        const CRect rClip(r[0], r[1], r[2], r[3]);
        if (nBpp == 16) {
            video.oursMode->DrawRecticle16(desc, surface, clipped ? &rClip : nullptr, rgb);
        } else {
            video.oursMode->DrawRecticle24(desc, surface, clipped ? &rClip : nullptr, rgb);
        }
        ctx.CountInputs(1);
        for (size_t i = 0; i < mine.size(); ++i) {
            if (canvas[i] != mine[i]) {
                ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                break;
            }
        }
    }
}

void Recticle16(CaseContext& ctx)
{
    RecticleDepth(ctx, 16, 0x5265637431360000ull); // "Rect16"
}

void Recticle24(CaseContext& ctx)
{
    RecticleDepth(ctx, 24, 0x5265637432340000ull); // "Rect24"
}

void Ellipse32(CaseContext& ctx)
{
    Shapes32(ctx, false, 0x456C6C3332000000ull); // "Ell32"
}

void Recticle32(CaseContext& ctx)
{
    Shapes32(ctx, true, 0x5265637433320000ull); // "Rect32"
}

void CellTranslucent(CaseContext& ctx)
{
    CellBlend(ctx, CellMode::Translucent, 0x43656C6C54000000ull); // "CellT"
}

void CellBrightness(CaseContext& ctx)
{
    CellBlend(ctx, CellMode::Brightness, 0x43656C6C42000000ull); // "CellB"
}

void CellBlt32(CaseContext& ctx)
{
    CellBlend(ctx, CellMode::Blt, 0x43656C6C30000000ull); // "Cell0"
}

void CellClearShadow32(CaseContext& ctx)
{
    CellBlend(ctx, CellMode::ClearShadow, 0x43656C6C53000000ull); // "CellS"
}

void CellBrightest32(CaseContext& ctx)
{
    CellBlend(ctx, CellMode::Brightest, 0x43656C6C48000000ull); // "CellH"
}

// CVidMode::PackColor(rgbColor) (0x79ADF0), `ret 4` at 0x79AE3F and
// 0x79AE9E. It reads g_pChitin->cVideo.m_nBpp, the channel shifts and, at 16
// bpp and below, the dropped bits.
using PackColorFn = uint32_t(__fastcall*)(void* self, void* edx, uint32_t rgbColor);
constexpr uintptr_t kPackColor = 0x79ADF0;

// PackColor on random colours (and black and white) at 32 and 24 bpp with
// the shifts 16/8/0 and the reversed 0/8/16, then at 16 bpp in 5-6-5 and
// 5-5-5 (SetMode16), whose result keeps the low word only.
void PackColor(CaseContext& ctx)
{
    VideoFixture video(ctx);
    Picks picks(ctx.seed, 0x5061636B00000000ull); // "Pack"
    auto run = [&](const std::string& mode) {
        for (int i = 0; i < 2000; ++i) {
            const uint32_t rgb = i == 0 ? 0u : i == 1 ? 0xFFFFFFu
                                                      : picks.Bits();
            ctx.SetInputs(mode + ", rgb " + Hex(rgb));
            uint32_t original = 0;
            ctx.CallOriginal([&] {
                original = reinterpret_cast<PackColorFn>(kPackColor)(video.mode, nullptr, rgb);
            });
            const uint32_t mine = video.oursMode->PackColor(rgb);
            ctx.CountInputs(1);
            if (original != mine) {
                ctx.Fail(ctx.Inputs(), Hex(original), Hex(mine));
                return;
            }
        }
    };
    auto setDepth = [&](uint16_t nBpp, uint32_t rShift, uint32_t bShift) {
        ctx.CallOriginal([&] {
            Put<uint16_t>(video.chitin, kBppOffset, nBpp);
            Put<uint32_t>(video.mode, kRShiftOffset, rShift);
            Put<uint32_t>(video.mode, kGShiftOffset, 8);
            Put<uint32_t>(video.mode, kBShiftOffset, bShift);
        });
        video.oursChitin->cVideo.m_nBpp = nBpp;
        video.oursMode->m_dwRBitShift = rShift;
        video.oursMode->m_dwGBitShift = 8;
        video.oursMode->m_dwBBitShift = bShift;
    };
    setDepth(32, 16, 0);
    run("32 bpp, shifts 16/8/0");
    setDepth(32, 0, 16);
    run("32 bpp, shifts 0/8/16");
    setDepth(24, 16, 0);
    run("24 bpp, shifts 16/8/0");
    SetMode16(video, ctx, true);
    run("16 bpp, 5-6-5");
    SetMode16(video, ctx, false);
    run("16 bpp, 5-5-5");
}

// CParticle::Render (0x7C36A0) at 16, 24 and 32 bpp: CONNECT through the
// mode's DrawLine16/24/32 virtuals (ours against the original's), DOTS and
// BLOB as plain buffer writes, the two blob helpers (0x7C3CC0, 0x7C3E20) new
// with the 16/24 arms. A zeroed CParticle block with only the members Render
// reads - the timestamp and tail length at 0/2, the colour at 6, the position
// and velocity at 0x12/0x1E - the 3d flag off, and a canvas of random bytes;
// the clip rect, the flag and the blob size vary. The whole canvas is
// compared.
using ParticleRenderFn = void(__fastcall*)(void* self, void* edx, void* pSurface, int32_t lPitch,
    const void* rClipRect, uint32_t nFlag, uint32_t nBlobSize);
constexpr uintptr_t kParticleRender = 0x7C36A0;
constexpr size_t kParticleTimeStampOffset = 0x00;
constexpr size_t kParticleTailLengthOffset = 0x02;
constexpr size_t kParticleColorOffset = 0x06;
constexpr size_t kParticlePosOffset = 0x12;
constexpr size_t kParticleVelOffset = 0x1E;
constexpr int kParticleCanvas = 64;

void ParticleRender(CaseContext& ctx)
{
    VideoFixture video(ctx);
    uint8_t* particle = nullptr;
    uint8_t* rect = nullptr;
    uint8_t* canvas = nullptr;
    constexpr int kMaxCanvasBytes = kParticleCanvas * kParticleCanvas * 4;
    ctx.SetInputs("building the original's particle and canvas");
    ctx.CallOriginal([&] {
        particle = OriginalBlock(0x40);
        rect = OriginalBlock(16);
        canvas = OriginalBlock(kMaxCanvasBytes);
    });

    auto* oursStorage = static_cast<unsigned char*>(::operator new(sizeof(CParticle)));
    std::memset(oursStorage, 0, sizeof(CParticle));
    auto* ours = reinterpret_cast<CParticle*>(oursStorage);
    std::vector<uint8_t> mine(kMaxCanvasBytes);

    Picks picks(ctx.seed, 0x5061727469630000ull); // "Partic"
    const int count = 400 + ctx.iterations / 20;
    const int32_t kPositions[] = { -2, -1, 0, 1, 3, 7, 30, 62, 65 };

    const auto setDepth = [&](uint16_t nBpp, uint32_t rShift, uint32_t gShift, uint32_t bShift) {
        ctx.CallOriginal([&] {
            Put<uint16_t>(video.chitin, kBppOffset, nBpp);
            Put<uint32_t>(video.mode, kRShiftOffset, rShift);
            Put<uint32_t>(video.mode, kGShiftOffset, gShift);
            Put<uint32_t>(video.mode, kBShiftOffset, bShift);
        });
        video.oursChitin->cVideo.m_nBpp = nBpp;
        video.oursMode->m_dwRBitShift = rShift;
        video.oursMode->m_dwGBitShift = gShift;
        video.oursMode->m_dwBBitShift = bShift;
    };

    for (const int bpp : { 16, 24, 32 }) {
        const int bytes = bpp / 8;
        const int32_t pitch = kParticleCanvas * bytes;
        const size_t canvasBytes = static_cast<size_t>(pitch) * kParticleCanvas;
        if (bpp == 16) {
            // 5-6-5, as CVidInf::CreateSurfaces sets it.
            ctx.CallOriginal([&] {
                Put<uint32_t>(video.mode, kRMaskOffset, 0xF800);
                Put<uint32_t>(video.mode, kGMaskOffset, 0x07E0);
                Put<uint32_t>(video.mode, kBMaskOffset, 0x001F);
                Put<uint32_t>(video.mode, kRDropOffset, 3);
                Put<uint32_t>(video.mode, kGDropOffset, 2);
                Put<uint32_t>(video.mode, kBDropOffset, 3);
                Put<uint16_t>(video.chitin, kBppOffset, 16);
                Put<uint32_t>(video.mode, kRShiftOffset, 11);
                Put<uint32_t>(video.mode, kGShiftOffset, 5);
                Put<uint32_t>(video.mode, kBShiftOffset, 0);
            });
            video.oursMode->m_dwRBitMask = 0xF800;
            video.oursMode->m_dwGBitMask = 0x07E0;
            video.oursMode->m_dwBBitMask = 0x001F;
            video.oursMode->field_C2 = 3;
            video.oursMode->field_C6 = 2;
            video.oursMode->field_CA = 3;
            video.oursMode->m_dwRBitShift = 11;
            video.oursMode->m_dwGBitShift = 5;
            video.oursMode->m_dwBBitShift = 0;
            video.oursChitin->cVideo.m_nBpp = 16;
        } else {
            setDepth(static_cast<uint16_t>(bpp), 16, 8, 0);
        }

        for (int n = 0; n < count; n++) {
            const int16_t nTimeStamp = static_cast<int16_t>(picks.Between(0, 6));
            const int16_t nTailLength = static_cast<int16_t>(picks.Between(0, 6));
            const uint32_t rgb = picks.Bits() & 0xFFFFFF;
            const int32_t x = (kPositions[picks.Between(0, 8)] << 10) | static_cast<int32_t>(picks.Bits() & 0x3FF);
            const int32_t y = (kPositions[picks.Between(0, 8)] << 10) | static_cast<int32_t>(picks.Bits() & 0x3FF);
            const int32_t z = picks.Between(-3000, 3000);
            const int32_t vx = picks.Between(-4000, 4000);
            const int32_t vy = picks.Between(-4000, 4000);
            const int32_t vz = picks.Between(-2000, 2000);
            const int32_t left = picks.Between(0, kParticleCanvas - 1);
            const int32_t top = picks.Between(0, kParticleCanvas - 1);
            const int32_t right = picks.Between(left, kParticleCanvas);
            const int32_t bottom = picks.Between(top, kParticleCanvas);
            const uint32_t nFlag = static_cast<uint32_t>(picks.Between(0, 2));
            const uint32_t nBlobSize = static_cast<uint32_t>(picks.Between(1, 5));
            for (size_t i = 0; i < canvasBytes; i++) {
                mine[i] = static_cast<uint8_t>(picks.Bits());
            }

            char inputs[240];
            std::snprintf(inputs, sizeof(inputs),
                "%d bpp: input %d, timestamp %d, tail %d, colour 0x%06X, pos (%d,%d,%d), vel (%d,%d,%d), "
                "clip (%d,%d,%d,%d), flag %u, blob %u",
                bpp, n, nTimeStamp, nTailLength, rgb, x, y, z, vx, vy, vz, left, top, right, bottom, nFlag, nBlobSize);
            ctx.SetInputs(inputs);
            ctx.CallOriginal([&] {
                Put<int16_t>(particle, kParticleTimeStampOffset, nTimeStamp);
                Put<int16_t>(particle, kParticleTailLengthOffset, nTailLength);
                Put<uint32_t>(particle, kParticleColorOffset, rgb);
                Put<int32_t>(particle, kParticlePosOffset, x);
                Put<int32_t>(particle, kParticlePosOffset + 4, y);
                Put<int32_t>(particle, kParticlePosOffset + 8, z);
                Put<int32_t>(particle, kParticleVelOffset, vx);
                Put<int32_t>(particle, kParticleVelOffset + 4, vy);
                Put<int32_t>(particle, kParticleVelOffset + 8, vz);
                const int32_t r[4] = { left, top, right, bottom };
                std::memcpy(rect, r, 16);
                std::memcpy(canvas, mine.data(), canvasBytes);
                reinterpret_cast<ParticleRenderFn>(kParticleRender)(particle, nullptr, canvas, pitch, rect, nFlag,
                    nBlobSize);
            });
            ours->m_nTimeStamp = nTimeStamp;
            ours->m_nTailLength = nTailLength;
            ours->m_rgbColor = rgb;
            ours->m_pos.x = x;
            ours->m_pos.y = y;
            ours->m_pos.z = z;
            ours->m_vel.x = vx;
            ours->m_vel.y = vy;
            ours->m_vel.z = vz;
            ours->Render(mine.data(), pitch, CRect(left, top, right, bottom), static_cast<USHORT>(nFlag),
                static_cast<USHORT>(nBlobSize));
            ctx.CountInputs(1);
            for (size_t i = 0; i < canvasBytes; i++) {
                if (canvas[i] != mine[i]) {
                    ctx.Fail(ctx.Inputs() + ", byte " + std::to_string(i), Hex(canvas[i]), Hex(mine[i]));
                    break;
                }
            }
        }
    }

    ::operator delete(oursStorage);
}

} // namespace

ORACLE_CASE("vid/hline_shaded32", HLineShaded32);
ORACLE_CASE("vid/hline_shaded_mirrored32", HLineShadedMirrored32);
ORACLE_CASE("vid/hline_shaded16", HLineShaded16);
ORACLE_CASE("vid/hline_shaded_mirrored16", HLineShadedMirrored16);
ORACLE_CASE("vid/hline_shaded24", HLineShaded24);
ORACLE_CASE("vid/hline_shaded_mirrored24", HLineShadedMirrored24);
ORACLE_CASE("vid/hline16", HLine16);
ORACLE_CASE("vid/hline_mirrored16", HLineMirrored16);
ORACLE_CASE("vid/hline24", HLine24);
ORACLE_CASE("vid/hline_mirrored24", HLineMirrored24);
ORACLE_CASE("vid/hline_dithered16", HLineDithered16);
ORACLE_CASE("vid/hline_dithered_mirrored16", HLineDitheredMirrored16);
ORACLE_CASE("vid/hline_dithered24", HLineDithered24);
ORACLE_CASE("vid/hline_dithered_mirrored24", HLineDitheredMirrored24);
ORACLE_CASE("vid/hline_dithered32", HLineDithered32);
ORACLE_CASE("vid/hline_dithered_mirrored32", HLineDitheredMirrored32);
ORACLE_CASE("vid/realize_range", RealizeRange);
ORACLE_CASE("vid/realize_resource", RealizeResource);
ORACLE_CASE("vid/thick_line32", ThickLine32);
ORACLE_CASE("vid/thick_line16", ThickLine16);
ORACLE_CASE("vid/thick_line24", ThickLine24);
ORACLE_CASE("vid/draw_line24", DrawLine24);
ORACLE_CASE("vid/brightness_contrast", BrightnessContrast);
ORACLE_CASE("vid/lighting_effect32", LightingEffect32);
ORACLE_CASE("vid/lighting_effect24", LightingEffect24);
ORACLE_CASE("vid/lighting_effect16", LightingEffect16);
ORACLE_CASE("vid/fill_convex_poly", FillConvexPoly);
ORACLE_CASE("vid/fill_poly", FillPoly);
ORACLE_CASE("vid/cell_translucent32", CellTranslucent);
ORACLE_CASE("vid/cell_brightness32", CellBrightness);
ORACLE_CASE("vid/cell_blt8to32", CellBlt32);
ORACLE_CASE("vid/cell_clear_shadow32", CellClearShadow32);
ORACLE_CASE("vid/cell_brightest32", CellBrightest32);
ORACLE_CASE("vid/cell_blt8to16", CellBlt8To16);
ORACLE_CASE("vid/cell_blt8to16_fast", CellBlt8To16Fast);
ORACLE_CASE("vid/cell_blt8to24", CellBlt8To24);
ORACLE_CASE("vid/cell_blt8to24_fast", CellBlt8To24Fast);
ORACLE_CASE("vid/cell_clear_shadow16", CellClearShadow16);
ORACLE_CASE("vid/cell_clear_shadow24", CellClearShadow24);
ORACLE_CASE("vid/cell_brightest16", CellBrightest16);
ORACLE_CASE("vid/cell_brightest24", CellBrightest24);
ORACLE_CASE("vid/cell_translucent24", CellTranslucent24);
ORACLE_CASE("vid/cell_translucent16", CellTranslucent16);
ORACLE_CASE("vid/cell_blt8to32_point", CellBlt8To32Point);
ORACLE_CASE("vid/draw_line16", DrawLine16);
ORACLE_CASE("vid/draw_line32", DrawLine32);
ORACLE_CASE("vid/draw_ellipse24", DrawEllipse24);
ORACLE_CASE("vid/draw_ellipse16", DrawEllipse16);
ORACLE_CASE("vid/blt_tile8to16", BltTile8To16);
ORACLE_CASE("vid/blt_tile8to32", BltTile8To32);
ORACLE_CASE("vid/blt_stencil_tile8to32", BltStencilTile8To32);
ORACLE_CASE("vid/blt_stencil_tile8to16", BltStencilTile8To16);
ORACLE_CASE("vid/blt_tile8to24", BltTile8To24);
ORACLE_CASE("vid/blt_stencil_tile8to24", BltStencilTile8To24);
ORACLE_CASE("vid/blt_bmp24to24", BltBmp24To24);
ORACLE_CASE("vid/blt_bmp24to24_tint", BltBmp24To24Tint);
ORACLE_CASE("vid/blt_bmp8to16", BltBmp8To16);
ORACLE_CASE("vid/blt_bmp24to16", BltBmp24To16);
ORACLE_CASE("vid/blt_bmp24to16_tint", BltBmp24To16Tint);
ORACLE_CASE("vid/draw_ellipse32", Ellipse32);
ORACLE_CASE("vid/draw_recticle32", Recticle32);
ORACLE_CASE("vid/draw_recticle16", Recticle16);
ORACLE_CASE("vid/draw_recticle24", Recticle24);
ORACLE_CASE("vid/pack_color", PackColor);
ORACLE_CASE("vid/particle_render", ParticleRender);
