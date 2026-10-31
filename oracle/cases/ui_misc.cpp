// The ui-misc stream's pure functions, the original's against ours:
// CGameSprite's dexterity bonus to the armour class (0x723EA0),
// CUtil::FindOneOf (0x781180) and CRuleTables' hit points per level from
// constitution (0x542EC0).
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures:
// - 0x723EA0 reads, with no armour (+0x4ADC): DEX (+0x97A), the derived
//   general state (+0x920, STATE_BLIND) and the feats (+0x75C, Blind Fight
//   through 0x763150); GetAbilityScoreModifier (0x542510) is called on
//   g_pBaldurChitin (0x8CF6DC) -> m_pObjectGame (0x1C54) and reads nothing
//   of it. As combat/armor_class: the original's sprite is a zeroed block
//   from its operator new with those members set (R10b), g_pBaldurChitin a
//   zeroed chitin holding a zeroed game; ours zeroed storage of our classes'
//   sizes with the same members, g_pBaldurChitin put back to null at the
//   end.
// - FindOneOf reads g_pChitin (0x8CF6D8) + 0x1A0 (Double Byte Character
//   Support) and calls IsDBCSLeadByte. The original's is the system's (the
//   host's ANSI code page); ours is the shim's stub (platform/
//   win32_stubs.cpp), FALSE for every byte unless a test installs a lead
//   byte table. So the case covers the single-byte behaviour only: with
//   the flag on, no separator is removed and no byte skipped (the input's
//   high bytes are not lead bytes on the host, or the two would differ).
//   The double-byte paths have the unit test (tests/test_ui_misc.cpp).
//   Both strings are original CStrings from its constructor (0x7FCC88) in
//   leaked memory (R10a); the original copies the separators (0x7FC98F, the
//   copy constructor) into their argument's slot (0x7811A5).
// - 0x542EC0 reads neither `this` nor the class (zeroed blocks).
//
// Calling conventions (objdump of the vanilla exe): 0x723EA0 __thiscall
// without arguments, plain ret; FindOneOf __cdecl (const CString&, const
// CString&, int), plain ret; 0x542EC0
// __thiscall with four stack arguments, `ret 0x10`.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CChitin.h"
#include "CGameSprite.h"
#include "CImm.h"
#include "CInfGame.h"
#include "CResCell.h"
#include "CResMosaic.h"
#include "CRuleTables.h"
#include "CUIControlScrollBar.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVidFont.h"
#include "CVidMosaic.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CString::CString(LPCSTR) (0x7FCC88): this in ecx, the text at [esp+4],
// ret 4.
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

using DexArmorClassFn = int32_t(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kDexArmorClass = 0x723EA0;

using FindOneOfFn = int32_t(__cdecl*)(const void* sString, const void* sSeparators, int32_t start);
constexpr uintptr_t kFindOneOf = 0x781180;

using HpConBonusFn = void(__fastcall*)(const void* self, void* edx, uint32_t nClass, uint32_t nCON, int32_t* bonus,
    int32_t* rolls);
constexpr uintptr_t kHpConBonus = 0x542EC0;

constexpr uintptr_t kBaldurChitin = 0x8CF6DC;
constexpr uintptr_t kChitin = 0x8CF6D8;
constexpr size_t kObjectGameOffset = 0x1C54;
constexpr size_t kDbcsOffset = 0x1A0;

constexpr size_t kSpriteBlockSize = 0x7800;
constexpr size_t kGeneralStateOffset = 0x920;
constexpr size_t kDexOffset = 0x97A;
constexpr size_t kFeatsOffset = 0x75C;
constexpr size_t kRuleTablesSize = 0x1B58;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

void SetOriginalPointer(uintptr_t global, const void* value)
{
    const uint32_t pointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
    std::memcpy(reinterpret_cast<void*>(global), &pointer, 4);
}

struct OurGlobalsReset {
    ~OurGlobalsReset()
    {
        g_pBaldurChitin = nullptr;
        g_pChitin = nullptr;
    }
};

// 0x723EA0 with DEX -5..60, blind or not, with or without Blind Fight and
// other feats set; --iterations random states.
void DexArmorClass(CaseContext& ctx)
{
    OurGlobalsReset reset;
    uint8_t* original = nullptr;
    ctx.SetInputs("building the original's sprite block and g_pBaldurChitin");
    ctx.CallOriginal([&] {
        original = OriginalBlock(kSpriteBlockSize);
        uint8_t* chitin = OriginalBlock(kObjectGameOffset + 4);
        uint8_t* game = OriginalBlock(0x100);
        const uint32_t gamePointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(game));
        std::memcpy(chitin + kObjectGameOffset, &gamePointer, 4);
        SetOriginalPointer(kBaldurChitin, chitin);
    });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameSprite)));
    std::memset(storage, 0, sizeof(CGameSprite));
    auto* ours = reinterpret_cast<CGameSprite*>(storage);
    auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CBaldurChitin)));
    std::memset(chitin, 0, sizeof(CBaldurChitin));
    auto* game = static_cast<unsigned char*>(::operator new(sizeof(CInfGame)));
    std::memset(game, 0, sizeof(CInfGame));
    reinterpret_cast<CBaldurChitin*>(chitin)->m_pObjectGame = reinterpret_cast<CInfGame*>(game);
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(chitin);

    uint64_t state = ctx.seed ^ 0x723EA0ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };

    auto check = [&](int16_t dex, uint32_t generalState, uint32_t feats) {
        ctx.SetInputs("DEX " + std::to_string(dex) + ", general state " + std::to_string(generalState) + ", feats "
            + std::to_string(feats));
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(original + kGeneralStateOffset, &generalState, 4);
            std::memcpy(original + kDexOffset, &dex, 2);
            std::memcpy(original + kFeatsOffset, &feats, 4);
            originalResult = reinterpret_cast<DexArmorClassFn>(kDexArmorClass)(original, nullptr);
        });
        ours->m_derivedStats.m_generalState = generalState;
        ours->m_derivedStats.m_nDEX = dex;
        ours->m_baseStats.m_feats[0] = feats;
        ctx.CountInputs(1);
        oracle::Compare(ctx, originalResult, static_cast<int32_t>(ours->sub_723EA0()));
    };

    const uint32_t states[] = { 0, STATE_BLIND, STATE_BLIND | STATE_INVISIBLE, STATE_INVISIBLE, 0xFFFFFFFFu };
    const uint32_t featSets[] = { 0, 1u << CGAMESPRITE_FEAT_BLIND_FIGHT, 0xFFFFFFFFu, 0xFFFFFFFFu & ~(1u << CGAMESPRITE_FEAT_BLIND_FIGHT) };
    for (int16_t dex = -5; dex <= 60; dex++) {
        for (uint32_t generalState : states) {
            for (uint32_t feats : featSets) {
                check(dex, generalState, feats);
            }
        }
    }
    for (long long n = 0; n < ctx.iterations; n++) {
        check(static_cast<int16_t>(next() % 81) - 10, next(), next());
    }
}

// FindOneOf over strings of letters, separators and high bytes, every start
// from -1 to past the end, with Double Byte Character Support off and on
// (no lead bytes on either side; see the fixtures); then --iterations
// random cases.
void FindOneOf(CaseContext& ctx)
{
    OurGlobalsReset reset;
    uint8_t* originalChitin = nullptr;
    ctx.SetInputs("building the original's g_pChitin");
    ctx.CallOriginal([&] {
        originalChitin = OriginalBlock(kDbcsOffset + 4);
        SetOriginalPointer(kChitin, originalChitin);
    });
    auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CChitin)));
    std::memset(chitin, 0, sizeof(CChitin));
    g_pChitin = reinterpret_cast<CChitin*>(chitin);

    uint64_t state = ctx.seed ^ 0x781180ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };
    const char alphabet[] = "ab,.; :\xA1\xE9\x82";

    auto check = [&](const std::string& sString, const std::string& sSeparators, int32_t start, int32_t dbcs) {
        ctx.SetInputs("FindOneOf(\"" + sString + "\", \"" + sSeparators + "\", " + std::to_string(start) + "), DBCS "
            + std::to_string(dbcs));
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(originalChitin + kDbcsOffset, &dbcs, 4);
            void* original = reinterpret_cast<OriginalNewFn>(kOriginalNew)(4);
            reinterpret_cast<OriginalCStringFn>(kOriginalCString)(original, nullptr, sString.c_str());
            void* originalSeparators = reinterpret_cast<OriginalNewFn>(kOriginalNew)(4);
            reinterpret_cast<OriginalCStringFn>(kOriginalCString)(originalSeparators, nullptr, sSeparators.c_str());
            originalResult = reinterpret_cast<FindOneOfFn>(kFindOneOf)(original, originalSeparators, start);
        });
        g_pChitin->field_1A0 = dbcs;
        ctx.CountInputs(1);
        oracle::Compare(ctx,
            originalResult,
            static_cast<int32_t>(CUtil::FindOneOf(CString(sString.c_str()), CString(sSeparators.c_str()), start)));
    };

    const char* strings[] = { "", "a", ",", "a,b.c", "abc", "abcdefghijklmnopqrstuvwxyz;", "\xA1,b,c", "a b:c" };
    const char* separators[] = { "", ",", ",.", ";", " :", "xyz", "\xA1" };
    for (const char* sString : strings) {
        for (const char* sSeparators : separators) {
            const int32_t length = static_cast<int32_t>(std::strlen(sString));
            for (int32_t start = -1; start <= length + 1; start++) {
                check(sString, sSeparators, start, 0);
                check(sString, sSeparators, start, 1);
            }
        }
    }
    for (long long n = 0; n < ctx.iterations; n++) {
        std::string sString;
        std::string sSeparators;
        const uint32_t stringLength = next() % 40;
        const uint32_t separatorLength = next() % 5;
        for (uint32_t i = 0; i < stringLength; i++) {
            sString += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        for (uint32_t i = 0; i < separatorLength; i++) {
            sSeparators += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        check(sString, sSeparators, static_cast<int32_t>(next() % (stringLength + 2)), static_cast<int32_t>(next() % 2));
    }
}

// 0x542EC0 for every CON byte with classes 0..12; --iterations random pairs.
void HpConBonus(CaseContext& ctx)
{
    void* original = nullptr;
    ctx.SetInputs("building the original's CRuleTables block");
    ctx.CallOriginal([&] { original = OriginalBlock(kRuleTablesSize); });
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    const CRuleTables* ours = reinterpret_cast<const CRuleTables*>(storage);

    auto check = [&](uint32_t nClass, uint32_t nCON) {
        ctx.SetInputs("sub_542EC0(" + std::to_string(nClass) + ", " + std::to_string(nCON) + ")");
        int32_t originalBonus = 0x55555555;
        int32_t originalRolls = 0x55555555;
        ctx.CallOriginal([&] {
            reinterpret_cast<HpConBonusFn>(kHpConBonus)(original, nullptr, nClass, nCON, &originalBonus, &originalRolls);
        });
        INT oursBonus = 0x55555555;
        INT oursRolls = 0x55555555;
        ours->sub_542EC0(static_cast<BYTE>(nClass), static_cast<BYTE>(nCON), oursBonus, oursRolls);
        ctx.CountInputs(1);
        oracle::Compare(ctx,
            std::to_string(originalBonus) + " " + std::to_string(originalRolls),
            std::to_string(oursBonus) + " " + std::to_string(oursRolls));
    };

    for (uint32_t nClass = 0; nClass <= 12; nClass++) {
        for (uint32_t nCON = 0; nCON <= 255; nCON++) {
            check(nClass, nCON);
        }
    }
    uint64_t state = ctx.seed ^ 0x542EC0ull;
    for (long long n = 0; n < ctx.iterations; n++) {
        const uint32_t nClass = static_cast<uint32_t>(oracle::SplitMix64(&state));
        const uint32_t nCON = static_cast<uint32_t>(oracle::SplitMix64(&state));
        check(nClass & 0xFF, nCON & 0xFF);
    }
}

// CImm::sub_7C3020 (0x7C3020), the composition window's rect (report-T80.md
// §2.10; WSO calls it GetCompositionWindowRect): the mosaic's size, three
// lines of the edit's font plus the padding either side clamped to the
// mosaic's height, placed against the panel and the edit. It reads the mosaic
// through CVidMosaic::GetSize, whose Demand/Release go through the chitin's
// resource manager: the synthetic CResMosaic is flagged in memory (see the
// side-specific flags below) with a high m_nID, so CDimm::InternalDemand
// hands back m_pData without a file, and a 0x40-byte MOS blob (the magic, the
// version and the sizes Parse reads) is all the resource needs. The original's static PADDING
// (0xA09FC0, 3/3, its startup's initializer at 0x7C2630) is written here:
// the runner does not run the exe's static initializers.
//
// The original is __thiscall with its CRect return by hidden pointer: `this`
// in ecx and the pointer first on the stack ([ebp+4], then the five
// arguments). A __thiscall function-pointer type cannot express that - MSVC's
// own struct-return ABI for it puts the pointer in ecx and `this` on the
// stack, as the built oracle's call site showed - so the call keeps the
// house __fastcall shape (this in ecx, edx unused) with the rect pointer as
// an explicit stack argument, which reproduces the original's layout.
using CompositionWindowRectFn = void(__fastcall*)(void* self, void* edx, CRect* pRect, const CPoint* ptPanel,
    const CSize* panelSize, const CPoint* ptEdit, const CSize* editSize, int16_t nFontHeight);
constexpr uintptr_t kCompositionWindowRect = 0x7C3020;
constexpr uintptr_t kPadding = 0xA09FC0;
constexpr size_t kImmBlockSize = 0x140;
constexpr size_t kImmMosaicOffset = 0x13C;
constexpr size_t kMosaicBlockSize = 0xC0;
constexpr size_t kMosaicResOffset = 0xA4;
constexpr size_t kMosaicDoubleOffset = 0xB0;
constexpr size_t kResBlockSize = 0x80;
constexpr size_t kResFlagsOffset = 0x04;
constexpr size_t kResDataOffset = 0x08;
constexpr size_t kResSizeOffset = 0x14;
constexpr size_t kResField18Offset = 0x18;
constexpr size_t kResField44Offset = 0x44;
constexpr size_t kResIdOffset = 0x4C;
constexpr size_t kMosSize = 0x40;
constexpr size_t kChitinSize = 0x2000;

// The mosaic's size in the MOS header (0x7805D0 reads it).
void PutMosaicSize(uint8_t* at, uint16_t nWidth, uint16_t nHeight)
{
    std::memcpy(at + 8, &nWidth, 2);
    std::memcpy(at + 0xA, &nHeight, 2);
}

// Mosaic sizes, panel and edit points/sizes and the font height swept in
// the branches' directions (the mosaic smaller or larger than the panel, the
// edit reaching past the panel or not, negative edit offsets); then
// --iterations random ones. The four edges are compared.
void CompositionWindowRect(CaseContext& ctx)
{
    uint8_t* chitin = nullptr;
    uint8_t* imm = nullptr;
    uint8_t* mosaic = nullptr;
    uint8_t* res = nullptr;
    uint8_t* mos = nullptr;
    ctx.SetInputs("building the original's chitin, CImm, mosaic and resource");
    ctx.CallOriginal([&] {
        chitin = OriginalBlock(kChitinSize);
        imm = OriginalBlock(kImmBlockSize);
        mosaic = OriginalBlock(kMosaicBlockSize);
        res = OriginalBlock(kResBlockSize);
        mos = OriginalBlock(kMosSize);
        SetOriginalPointer(kChitin, chitin);
        const uint32_t mosaicPointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mosaic));
        std::memcpy(imm + kImmMosaicOffset, &mosaicPointer, 4);
        const uint32_t resPointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res));
        std::memcpy(mosaic + kMosaicResOffset, &resPointer, 4);
        const uint32_t mosPointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mos));
        std::memcpy(res + kResDataOffset, &mosPointer, 4);
        // The original's InternalDemand (0x783EF9) takes its in-memory
        // shortcut for (dwFlags & 0x04) && !(dwFlags & 0x10), and skips the
        // OnResourceServiced vtable call when (dwFlags & 0x60) is set; our
        // port (#guess) tests 0x10 the other way round (a pre-existing
        // divergence, in the report), so each side gets the flags its own
        // code wants for the same shortcut.
        const uint32_t flags = 0x04 | 0x20;
        const uint32_t size = kMosSize;
        const uint32_t one = 1;
        const uint32_t id = 0xFF000000u;
        const uint32_t zero = 0;
        std::memcpy(res + kResFlagsOffset, &flags, 4);
        std::memcpy(res + kResSizeOffset, &size, 4);
        std::memcpy(res + kResField18Offset, &zero, 4);
        std::memcpy(res + kResField44Offset, &one, 4);
        std::memcpy(res + kResIdOffset, &id, 4);
        std::memcpy(mos, "MOS ", 4);
        std::memcpy(mos + 4, "V1  ", 4);
        const uint16_t oneTile = 1;
        const uint32_t paletteOffset = 0x18;
        std::memcpy(mos + 0xC, &oneTile, 2);
        std::memcpy(mos + 0xE, &oneTile, 2);
        std::memcpy(mos + 0x14, &paletteOffset, 4);
        const uint32_t padding = 3;
        std::memcpy(reinterpret_cast<void*>(kPadding), &padding, 4);
        std::memcpy(reinterpret_cast<void*>(kPadding + 4), &padding, 4);
    });

    std::unique_ptr<unsigned char[]> chitinStorage(new unsigned char[sizeof(CChitin)]());
    std::unique_ptr<unsigned char[]> immStorage(new unsigned char[sizeof(CImm)]());
    std::unique_ptr<unsigned char[]> mosaicStorage(new unsigned char[sizeof(CVidMosaic)]());
    std::unique_ptr<unsigned char[]> resStorage(new unsigned char[sizeof(CResMosaic)]());
    auto* oursImmPtr = reinterpret_cast<CImm*>(immStorage.get());
    auto* oursMosaic = reinterpret_cast<CVidMosaic*>(mosaicStorage.get());
    auto* oursRes = reinterpret_cast<CResMosaic*>(resStorage.get());
    std::vector<uint8_t> oursMos(kMosSize, 0);
    std::memcpy(oursMos.data(), "MOS ", 4);
    std::memcpy(oursMos.data() + 4, "V1  ", 4);
    const uint16_t oneTile = 1;
    const uint32_t paletteOffset = 0x18;
    std::memcpy(oursMos.data() + 0xC, &oneTile, 2);
    std::memcpy(oursMos.data() + 0xE, &oneTile, 2);
    std::memcpy(oursMos.data() + 0x14, &paletteOffset, 4);
    oursRes->dwFlags = CRes::RES_FLAG_0x04 | CRes::RES_FLAG_0x10;
    oursRes->m_pData = oursMos.data();
    oursRes->nSize = kMosSize;
    oursRes->field_18 = 0;
    oursRes->field_44 = 1;
    oursRes->m_nID = 0xFF000000;
    oursMosaic->pRes = oursRes;
    oursMosaic->m_bDoubleSize = FALSE;
    oursImmPtr->field_13C = oursMosaic;
    CChitin* savedChitin = g_pChitin;
    g_pChitin = reinterpret_cast<CChitin*>(chitinStorage.get());

    uint64_t state = ctx.seed ^ 0x7C3020ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };

    auto check = [&](uint16_t mosaicWidth, uint16_t mosaicHeight, const CPoint& ptPanel, const CSize& panelSize,
                     const CPoint& ptEdit, const CSize& editSize, int16_t nFontHeight) {
        char inputs[240];
        std::snprintf(inputs, sizeof(inputs), // NOLINT
            "mosaic %ux%u, panel (%d,%d) %dx%d, edit (%d,%d) %dx%d, font %d", mosaicWidth, mosaicHeight, ptPanel.x,
            ptPanel.y, panelSize.cx, panelSize.cy, ptEdit.x, ptEdit.y, editSize.cx, editSize.cy, nFontHeight);
        ctx.SetInputs(inputs);
        CRect originalRect;
        ctx.CallOriginal([&] {
            PutMosaicSize(mos, mosaicWidth, mosaicHeight);
            reinterpret_cast<CompositionWindowRectFn>(kCompositionWindowRect)(
                imm, nullptr, &originalRect, &ptPanel, &panelSize, &ptEdit, &editSize, nFontHeight);
        });
        PutMosaicSize(oursMos.data(), mosaicWidth, mosaicHeight);
        const CRect oursRect = oursImmPtr->sub_7C3020(ptPanel, panelSize, ptEdit, editSize, nFontHeight);
        ctx.CountInputs(1);
        oracle::Compare(ctx, originalRect.left, oursRect.left);
        oracle::Compare(ctx, originalRect.top, oursRect.top);
        oracle::Compare(ctx, originalRect.right, oursRect.right);
        oracle::Compare(ctx, originalRect.bottom, oursRect.bottom);
    };

    // Panels and edits in each branch's direction: the mosaic smaller than
    // the panel or not, the edit reaching past it or not, negative offsets.
    const CPoint panels[] = { CPoint(0, 0), CPoint(10, 20), CPoint(-40, -30), CPoint(300, 200) };
    const CSize panelSizes[] = { CSize(10, 10), CSize(200, 150), CSize(50, 100), CSize(1200, 900) };
    const CPoint edits[] = { CPoint(0, 0), CPoint(30, 5), CPoint(0, 50), CPoint(-25, -60), CPoint(90, 40) };
    const CSize editSizes[] = { CSize(0, 0), CSize(60, 20), CSize(10, 20), CSize(60, 60), CSize(400, 300) };
    const uint16_t mosaicSizes[][2] = { { 40, 12 }, { 40, 8 }, { 60, 90 }, { 1, 1 }, { 400, 300 } };
    const int16_t fonts[] = { 0, 2, 10, 40 };
    for (const auto& mosaicSize : mosaicSizes) {
        for (const CPoint& panel : panels) {
            for (const CSize& panelSize : panelSizes) {
                check(mosaicSize[0], mosaicSize[1], panel, panelSize, edits[1], editSizes[1], 2);
            }
        }
    }
    for (const CPoint& panel : panels) {
        for (const CSize& panelSize : panelSizes) {
            for (const CPoint& edit : edits) {
                for (const CSize& editSize : editSizes) {
                    for (int16_t font : fonts) {
                        check(40, 12, panel, panelSize, edit, editSize, font);
                    }
                }
            }
        }
    }
    const int count = 400 + ctx.iterations / 20;
    for (int n = 0; n < count; n++) {
        const uint16_t mosaicWidth = static_cast<uint16_t>(1 + next() % 400);
        const uint16_t mosaicHeight = static_cast<uint16_t>(1 + next() % 400);
        const CPoint panel(static_cast<int32_t>(next() % 501), static_cast<int32_t>(next() % 501));
        const CSize panelSize(static_cast<int32_t>(next() % 500), static_cast<int32_t>(next() % 500));
        const CPoint edit(static_cast<int32_t>(next() % 601) - 100, static_cast<int32_t>(next() % 601) - 100);
        const CSize editSize(static_cast<int32_t>(next() % 500), static_cast<int32_t>(next() % 500));
        check(mosaicWidth, mosaicHeight, panel, panelSize, edit, editSize, static_cast<int16_t>(next() % 41));
    }

    g_pChitin = savedChitin;
}

// Writes a value into an original block at a byte offset.
template <class T>
void Put(uint8_t* at, size_t offset, T value)
{
    std::memcpy(at + offset, &value, sizeof(value));
}

// CBaldurEngine::OnMouseWheel (0x4289C0), the tail after the scroll-bar loop
// (report-T95.md section 2.6): with no current scroll bar the two remembered
// bars at 0xFE/0x102 take the wheel - both must be non-null, then bForward
// selects the forward one and the other direction the backward one - each tick
// calling its vtable +0x78 twice. The exe calls that slot with no argument (the
// slot's function is `ret 4`, so the shipped path would unbalance the stack; it
// never runs because nothing assigns 0xFE/0x102). The case builds synthetic
// bars, their panels and their vtables on both sides: the original's at the
// binary's packed offsets (vptr +0, panel +6, active +0x1E, panel active
// +0xF4), ours through the members, and puts host callbacks in the vtable slots
// the function uses (+0x78 the button, +0x7C/+0x80 the page calls) to record
// which bar was called, which operation and the page count. The full call
// sequences are compared; no engine global or surface is involved.
using OnMouseWheelFn = void(__fastcall*)(void* self, void* edx, int32_t bForward, int32_t nTicks, uint32_t nLines,
    uint32_t wFlags);
constexpr uintptr_t kOnMouseWheel = 0x4289C0;

constexpr uint32_t kEngineBarCurrent = 0xFA;
constexpr uint32_t kEngineBarForward = 0xFE;
constexpr uint32_t kEngineBarBackward = 0x102;
constexpr uint32_t kOriginalBarPanel = 0x6;
constexpr uint32_t kOriginalBarActive = 0x1E;
constexpr uint32_t kOriginalPanelActive = 0xF4;
constexpr size_t kOriginalEngineSize = 0x140;
constexpr size_t kOriginalBarSize = 0x40;
constexpr size_t kOriginalPanelSize = 0x100;

constexpr int kVtableBytes = 0x84;
constexpr int kMaxCalls = 16;

// Which bar (0 forward, 1 backward, 2 current, 3 unknown) in bits 16+, the
// operation (0 button, 1 page up, 2 page down) in bits 8+ and the page count in
// the low byte.
enum {
    kCallButton = 0,
    kCallPageUp = 1,
    kCallPageDown = 2,
};

int32_t sSequence[kMaxCalls];
int sSequenceLength;
void* sBarForward;
void* sBarBackward;
void* sBarCurrent;

void Record(void* self, int32_t op, uint32_t nLines)
{
    const int32_t which = self == sBarForward ? 0 : (self == sBarBackward ? 1 : (self == sBarCurrent ? 2 : 3));
    if (sSequenceLength < kMaxCalls) {
        sSequence[sSequenceLength] = (which << 16) + (op << 8) + static_cast<int32_t>(nLines & 0xFF);
    }
    sSequenceLength++;
}

// The exe's +0x78 call passes no argument; ours passes the BYTE our header's
// slot declares. The page calls take the DWORD nLines on both sides.
void __fastcall RecordButtonOriginal(void* self, void* edx)
{
    (void)edx;
    Record(self, kCallButton, 0);
}

void __fastcall RecordButtonOurs(void* self, void* edx, unsigned char nDirection)
{
    (void)edx;
    (void)nDirection;
    Record(self, kCallButton, 0);
}

void __fastcall RecordPageUpOriginal(void* self, void* edx, uint32_t nLines)
{
    (void)edx;
    Record(self, kCallPageUp, nLines);
}

void __fastcall RecordPageUpOurs(void* self, void* edx, uint32_t nLines)
{
    (void)edx;
    Record(self, kCallPageUp, nLines);
}

void __fastcall RecordPageDownOriginal(void* self, void* edx, uint32_t nLines)
{
    (void)edx;
    Record(self, kCallPageDown, nLines);
}

void __fastcall RecordPageDownOurs(void* self, void* edx, uint32_t nLines)
{
    (void)edx;
    Record(self, kCallPageDown, nLines);
}

uint32_t sOriginalVtable[kVtableBytes / 4];
uint32_t sOursVtable[kVtableBytes / 4];

// OnMouseWheel over synthetic bars: the current bar (0xFA) and the two
// remembered ones (0xFE/0x102), their panels' and their own active flags,
// bForward, nTicks and nLines. The head, the tail and their separation are all
// compared through the recorded sequences.
void EngineOnMouseWheel(CaseContext& ctx)
{
    for (int i = 0; i < kVtableBytes / 4; i++) {
        sOriginalVtable[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordButtonOriginal));
        sOursVtable[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordButtonOurs));
    }
    sOriginalVtable[0x78 / 4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordButtonOriginal));
    sOriginalVtable[0x7C / 4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordPageUpOriginal));
    sOriginalVtable[0x80 / 4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordPageDownOriginal));
    sOursVtable[0x78 / 4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordButtonOurs));
    sOursVtable[0x7C / 4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordPageUpOurs));
    sOursVtable[0x80 / 4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RecordPageDownOurs));

    uint8_t* originalEngine = nullptr;
    uint8_t* originalCurrent = nullptr;
    uint8_t* originalForward = nullptr;
    uint8_t* originalBackward = nullptr;
    uint8_t* originalCurrentPanel = nullptr;
    uint8_t* originalForwardPanel = nullptr;
    uint8_t* originalBackwardPanel = nullptr;
    ctx.SetInputs("building the original's blocks");
    ctx.CallOriginal([&] {
        originalEngine = OriginalBlock(kOriginalEngineSize);
        originalCurrent = OriginalBlock(kOriginalBarSize);
        originalForward = OriginalBlock(kOriginalBarSize);
        originalBackward = OriginalBlock(kOriginalBarSize);
        originalCurrentPanel = OriginalBlock(kOriginalPanelSize);
        originalForwardPanel = OriginalBlock(kOriginalPanelSize);
        originalBackwardPanel = OriginalBlock(kOriginalPanelSize);
    });

    std::vector<unsigned char> oursEngineStorage(sizeof(CBaldurEngine), 0);
    std::vector<unsigned char> oursCurrentStorage(sizeof(CUIControlScrollBar), 0);
    std::vector<unsigned char> oursForwardStorage(sizeof(CUIControlScrollBar), 0);
    std::vector<unsigned char> oursBackwardStorage(sizeof(CUIControlScrollBar), 0);
    std::vector<unsigned char> oursCurrentPanelStorage(sizeof(CUIPanel), 0);
    std::vector<unsigned char> oursForwardPanelStorage(sizeof(CUIPanel), 0);
    std::vector<unsigned char> oursBackwardPanelStorage(sizeof(CUIPanel), 0);
    auto* oursEngine = reinterpret_cast<CBaldurEngine*>(oursEngineStorage.data());
    auto* oursCurrent = reinterpret_cast<CUIControlScrollBar*>(oursCurrentStorage.data());
    auto* oursForward = reinterpret_cast<CUIControlScrollBar*>(oursForwardStorage.data());
    auto* oursBackward = reinterpret_cast<CUIControlScrollBar*>(oursBackwardStorage.data());
    auto* oursCurrentBase = reinterpret_cast<CUIControlBase*>(oursCurrent);
    auto* oursForwardBase = reinterpret_cast<CUIControlBase*>(oursForward);
    auto* oursBackwardBase = reinterpret_cast<CUIControlBase*>(oursBackward);
    auto* oursCurrentPanel = reinterpret_cast<CUIPanel*>(oursCurrentPanelStorage.data());
    auto* oursForwardPanel = reinterpret_cast<CUIPanel*>(oursForwardPanelStorage.data());
    auto* oursBackwardPanel = reinterpret_cast<CUIPanel*>(oursBackwardPanelStorage.data());

    uint64_t state = ctx.seed ^ 0x4289C0ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };

    auto check = [&](bool currentPresent, bool currentPanelActive, bool currentBarActive, bool forwardPresent,
                     bool forwardPanelActive, bool forwardBarActive, bool backwardPresent, bool backwardPanelActive,
                     bool backwardBarActive, int32_t bForwardDir, int32_t nTicks, int32_t nLines) {
        char inputs[260];
        std::snprintf(inputs, sizeof(inputs),
            "current %d/%d/%d, forward %d/%d/%d, backward %d/%d/%d, bForward %d, nTicks %d, nLines %d",
            currentPresent ? 1 : 0, currentPanelActive ? 1 : 0, currentBarActive ? 1 : 0, forwardPresent ? 1 : 0,
            forwardPanelActive ? 1 : 0, forwardBarActive ? 1 : 0, backwardPresent ? 1 : 0,
            backwardPanelActive ? 1 : 0, backwardBarActive ? 1 : 0, bForwardDir, nTicks, nLines);
        ctx.SetInputs(inputs);

        int32_t originalSequence[kMaxCalls];
        int originalLength = 0;
        sSequenceLength = 0;
        sBarCurrent = currentPresent ? originalCurrent : nullptr;
        sBarForward = forwardPresent ? originalForward : nullptr;
        sBarBackward = backwardPresent ? originalBackward : nullptr;
        ctx.CallOriginal([&] {
            *reinterpret_cast<uint32_t*>(originalCurrent) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sOriginalVtable));
            *reinterpret_cast<uint32_t*>(originalForward) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sOriginalVtable));
            *reinterpret_cast<uint32_t*>(originalBackward) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sOriginalVtable));
            Put<uint32_t>(originalCurrent, kOriginalBarPanel, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalCurrentPanel)));
            Put<uint32_t>(originalForward, kOriginalBarPanel, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalForwardPanel)));
            Put<uint32_t>(originalBackward, kOriginalBarPanel, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalBackwardPanel)));
            Put<uint8_t>(originalCurrentPanel, kOriginalPanelActive, currentPanelActive);
            Put<uint8_t>(originalForwardPanel, kOriginalPanelActive, forwardPanelActive);
            Put<uint8_t>(originalBackwardPanel, kOriginalPanelActive, backwardPanelActive);
            Put<uint8_t>(originalCurrent, kOriginalBarActive, currentBarActive);
            Put<uint8_t>(originalForward, kOriginalBarActive, forwardBarActive);
            Put<uint8_t>(originalBackward, kOriginalBarActive, backwardBarActive);
            Put<uint32_t>(originalEngine, kEngineBarCurrent,
                currentPresent ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalCurrent)) : 0);
            Put<uint32_t>(originalEngine, kEngineBarForward,
                forwardPresent ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalForward)) : 0);
            Put<uint32_t>(originalEngine, kEngineBarBackward,
                backwardPresent ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalBackward)) : 0);
            reinterpret_cast<OnMouseWheelFn>(kOnMouseWheel)(originalEngine, nullptr, bForwardDir, nTicks,
                static_cast<uint32_t>(nLines), 0);
        });
        std::memcpy(originalSequence, sSequence, sizeof(originalSequence));
        originalLength = sSequenceLength;

        sSequenceLength = 0;
        sBarCurrent = currentPresent ? oursCurrent : nullptr;
        sBarForward = forwardPresent ? oursForward : nullptr;
        sBarBackward = backwardPresent ? oursBackward : nullptr;
        *reinterpret_cast<uint32_t*>(oursCurrentStorage.data()) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sOursVtable));
        *reinterpret_cast<uint32_t*>(oursForwardStorage.data()) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sOursVtable));
        *reinterpret_cast<uint32_t*>(oursBackwardStorage.data()) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sOursVtable));
        oursCurrentBase->m_pPanel = oursCurrentPanel;
        oursForwardBase->m_pPanel = oursForwardPanel;
        oursBackwardBase->m_pPanel = oursBackwardPanel;
        oursCurrentPanel->m_bActive = currentPanelActive;
        oursForwardPanel->m_bActive = forwardPanelActive;
        oursBackwardPanel->m_bActive = backwardPanelActive;
        oursCurrentBase->m_bActive = currentBarActive;
        oursForwardBase->m_bActive = forwardBarActive;
        oursBackwardBase->m_bActive = backwardBarActive;
        oursEngine->m_pCurrentScrollBar = currentPresent ? oursCurrent : nullptr;
        oursEngine->field_FE = forwardPresent ? oursForward : nullptr;
        oursEngine->field_102 = backwardPresent ? oursBackward : nullptr;
        oursEngine->CBaldurEngine::OnMouseWheel(bForwardDir, nTicks, static_cast<uint32_t>(nLines), 0);

        ctx.CountInputs(1);
        oracle::Compare(ctx, originalLength, sSequenceLength);
        const int common = originalLength < sSequenceLength ? originalLength : sSequenceLength;
        for (int i = 0; i < common; i++) {
            oracle::Compare(ctx, originalSequence[i], sSequence[i]);
        }
    };

    for (int32_t nLines = 0; nLines <= 3; nLines++) {
        for (int32_t nTicks = -2; nTicks <= 3; nTicks++) {
            for (int32_t bForward = 0; bForward <= 1; bForward++) {
                check(false, false, false, true, true, true, true, true, true, bForward, nTicks, nLines);
                check(false, false, false, true, false, true, true, true, true, bForward, nTicks, nLines);
                check(false, false, false, true, true, false, true, true, true, bForward, nTicks, nLines);
                check(false, false, false, true, true, true, true, false, true, bForward, nTicks, nLines);
                check(false, false, false, true, true, true, true, true, false, bForward, nTicks, nLines);
                check(false, false, false, true, true, true, false, false, false, bForward, nTicks, nLines);
                check(false, false, false, false, false, false, true, true, true, bForward, nTicks, nLines);
                check(false, false, false, false, false, false, false, false, false, bForward, nTicks, nLines);
                check(true, true, true, true, true, true, true, true, true, bForward, nTicks, nLines);
                check(true, false, true, true, true, true, true, true, true, bForward, nTicks, nLines);
                check(true, true, false, true, true, true, true, true, true, bForward, nTicks, nLines);
            }
        }
    }
    const int count = 400 + ctx.iterations / 20;
    for (int n = 0; n < count; n++) {
        check((next() & 1) != 0, (next() & 1) != 0, (next() & 1) != 0, (next() & 1) != 0, (next() & 1) != 0,
            (next() & 1) != 0, (next() & 1) != 0, (next() & 1) != 0, (next() & 1) != 0,
            static_cast<int32_t>(next() & 1), static_cast<int32_t>(next() % 6) - 2, static_cast<int32_t>(next() % 4));
    }
}

} // namespace

ORACLE_CASE("combat/dex_armor_class", DexArmorClass);

// CUtil::SplitString (0x780CF0): the line loop, including its double-byte
// handling. __cdecl, nine arguments pushed right to left (the suggested split
// length, the remainder flag, bDemanded, bDivideWords, the output array, the
// maximum count, the line length, the source CString and the font).
//
// The font is a synthetic one: a zeroed CVidFont whose pRes (+0xA8) points at
// a zeroed CResCell whose parsed flag (+0x50), BAM header (+0x54), frames
// (+0x5C), sequences (+0x60) and frame list (+0x64) describe one 5x7 frame, so
// every character is 5 px wide (CVidFont::GetStringLength 0x792D40 reads the
// frame). The chitin's font name (+0x2E8) points at an empty string so the
// font takes the BAM path (strcmp at 0x792D5A). bDemanded is TRUE so nothing
// goes through the resource manager.
//
// Only the single-byte arm is oracle-tested: IsDBCSLeadByte (the host's) uses
// the ANSI code page, so with the flag on a Western rig the original would
// treat no byte as a lead byte while our shim returns FALSE for every byte;
// the double-byte branch has the unit test (tests/test_ui_misc.cpp, whose
// lead-byte table makes a pair straddle the budget).
using SplitStringFn = uint8_t(__cdecl*)(const void* pFont, const void* sSource, uint16_t nLineLength,
    uint8_t nMaxStrings, void* pStringsOut, int32_t bDivideWords, int32_t bDemanded,
    int32_t bCheckRemainderForNewLine, uint16_t nSuggestedSplitLength);
constexpr uintptr_t kSplitString = 0x780CF0;
constexpr size_t kFontNameOffset = 0x2E8; // CChitin::m_sFontName
constexpr size_t kFontResOffset = 0xA8; // CVidCell::pRes
constexpr size_t kCellParsedOffset = 0x50;
constexpr size_t kCellBamHeaderOffset = 0x54;
constexpr size_t kCellFramesOffset = 0x5C;
constexpr size_t kCellSequencesOffset = 0x60;
constexpr size_t kCellFrameListOffset = 0x64;
constexpr size_t kCellFrameListSizeOffset = 0x68;
constexpr size_t kCellCacheHeaderOffset = 0x7E;

// The original CString: the buffer pointer is the member, the length word at
// data - 8, the bytes at the pointer (messages_wire's ReadCString).
std::string ReadCStringAt(const uint8_t* member)
{
    const char* data = nullptr;
    std::memcpy(&data, member, sizeof(data));
    if (data == nullptr) {
        return std::string();
    }
    int32_t length = 0;
    std::memcpy(&length, data - 8, sizeof(length));
    return length > 0 ? std::string(data, static_cast<size_t>(length)) : std::string();
}

void CaseSplitString(CaseContext& ctx)
{
    OurGlobalsReset reset;
    uint8_t* chitin = nullptr;
    uint8_t* font = nullptr;
    uint8_t* cell = nullptr;
    uint8_t* bam = nullptr;
    uint8_t* frame = nullptr;
    uint8_t* sequence = nullptr;
    uint8_t* frameList = nullptr;
    uint8_t* empty = nullptr;
    ctx.SetInputs("building the original's chitin and font");
    ctx.CallOriginal([&] {
        chitin = OriginalBlock(0x1000);
        font = OriginalBlock(0x1000);
        cell = OriginalBlock(0x100);
        bam = OriginalBlock(0x20);
        frame = OriginalBlock(0x10);
        sequence = OriginalBlock(8);
        frameList = OriginalBlock(8);
        empty = OriginalBlock(1);
        SetOriginalPointer(kChitin, chitin);

        auto putPointer = [](uint8_t* at, size_t offset, const void* value) {
            const uint32_t pointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
            std::memcpy(at + offset, &pointer, 4);
        };
        putPointer(chitin, kFontNameOffset, empty);
        const uint16_t noDbcs = 0;
        std::memcpy(chitin + kDbcsOffset, &noDbcs, 2);
        putPointer(font, kFontResOffset, cell);
        const uint8_t parsed = 1;
        std::memcpy(cell + kCellParsedOffset, &parsed, 1);
        putPointer(cell, kCellBamHeaderOffset, bam);
        putPointer(cell, kCellFramesOffset, frame);
        putPointer(cell, kCellSequencesOffset, sequence);
        putPointer(cell, kCellFrameListOffset, frameList);
        const uint16_t frameListSize = 1;
        std::memcpy(cell + kCellFrameListSizeOffset, &frameListSize, 2);
        const uint8_t cacheHeader = 0;
        std::memcpy(cell + kCellCacheHeaderOffset, &cacheHeader, 1);

        // BAMHEADER: nFrames (8), nSequences (10).
        const uint16_t oneFrame = 1;
        std::memcpy(bam + 8, &oneFrame, 2);
        const uint8_t oneSequence = 1;
        std::memcpy(bam + 10, &oneSequence, 1);
        // FRAMEENTRY: nWidth (0) 5, nHeight (2) 7.
        const uint16_t five = 5;
        const uint16_t seven = 7;
        std::memcpy(frame, &five, 2);
        std::memcpy(frame + 2, &seven, 2);
        // SEQUENCEENTRY: nFrames (0) 1, nStartingFrame (2) 0.
        std::memcpy(sequence, &oneFrame, 2);
        // The frame list's single index 0.
        const uint16_t zero = 0;
        std::memcpy(frameList, &zero, 2);
    });

    std::vector<uint8_t> ourChitinStorage(sizeof(CChitin), 0);
    auto* ourChitin = reinterpret_cast<CChitin*>(ourChitinStorage.data());
    // SplitString's font reads the chitin's font name, a CString (a bare
    // zeroed CChitin cannot hold one), so construct just that member.
    new (&ourChitin->m_sFontName) CString();
    struct FontNameReset {
        CString* pName;
        ~FontNameReset() { pName->~CString(); }
    } nameReset { &ourChitin->m_sFontName };
    ourChitin->m_sFontName = "";
    ourChitin->field_1A0 = 0;
    g_pChitin = ourChitin;

    std::vector<uint8_t> ourFontStorage(sizeof(CVidFont), 0);
    std::vector<uint8_t> ourCellStorage(sizeof(CResCell), 0);
    auto* ourFont = reinterpret_cast<CVidFont*>(ourFontStorage.data());
    auto* ourCell = reinterpret_cast<CResCell*>(ourCellStorage.data());
    BAMHEADER ourBam = {};
    ourBam.nFrames = 1;
    ourBam.nSequences = 1;
    FRAMEENTRY ourFrame = {};
    ourFrame.nWidth = 5;
    ourFrame.nHeight = 7;
    SEQUENCEENTRY ourSequence = { 1, 0 };
    WORD ourFrameList = 0;
    ourCell->m_bParsed = TRUE;
    ourCell->m_pBamHeader = &ourBam;
    ourCell->m_pFrames = &ourFrame;
    ourCell->m_pSequences = &ourSequence;
    ourCell->m_pFrameList = &ourFrameList;
    ourCell->m_nFrameListSize = 1;
    ourCell->m_bCacheHeader = FALSE;
    static_cast<CResHelper<CResCell, 1000>&>(*ourFont).pRes = ourCell;

    uint64_t state = ctx.seed ^ 0x780CF0ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };
    const char alphabet[] = "ab c\r\n\x81\xE9";

    auto checkInput = [&](const std::string& source, uint16_t lineLength, uint8_t maxStrings, int32_t divideWords,
                          int32_t checkRemainder, uint16_t suggested) {
        char inputs[160];
        std::snprintf(inputs, sizeof(inputs), "\"%s\" len %u max %u divide %d rem %d suggested %u",
            source.c_str(), lineLength, maxStrings, divideWords, checkRemainder, suggested);
        ctx.SetInputs(inputs);

        std::vector<std::string> originalLines(8);
        uint8_t originalResult = 0;
        ctx.CallOriginal([&] {
            void* sSource = reinterpret_cast<OriginalNewFn>(kOriginalNew)(4);
            reinterpret_cast<OriginalCStringFn>(kOriginalCString)(sSource, nullptr, source.c_str());
            uint8_t* out = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(4 * 8));
            for (int i = 0; i < 8; i++) {
                reinterpret_cast<OriginalCStringFn>(kOriginalCString)(out + i * 4, nullptr, "");
            }
            originalResult = reinterpret_cast<SplitStringFn>(kSplitString)(font, sSource, lineLength, maxStrings,
                out, divideWords, TRUE, checkRemainder, suggested);
            for (int i = 0; i < originalResult && i < 8; i++) {
                originalLines[i] = ReadCStringAt(out + i * 4);
            }
        });

        CString ourLines[8];
        const BYTE ourResult = CUtil::SplitString(ourFont, CString(source.c_str()), lineLength, maxStrings, ourLines,
            divideWords, TRUE, checkRemainder, suggested);
        ctx.CountInputs(1);

        oracle::Compare(ctx, static_cast<int32_t>(originalResult), static_cast<int32_t>(ourResult));
        for (int i = 0; i < originalResult && i < 8; i++) {
            oracle::Compare(ctx, originalLines[i], std::string(static_cast<const char*>(ourLines[i])));
        }
    };

    const char* fixed[] = { "", "a", "ab", "abc", "abcdef", "ab cd", "ab\ncd", "a\r\nb", "   ", "\x81"
                                                                                                "A",
        "a\xE9"
        "b\x81",
        "abcdefghij" };
    const uint16_t lengths[] = { 0, 1, 5, 10, 15, 20, 40 };
    const uint8_t maxima[] = { 0, 1, 2, 3, 5 };
    for (const char* source : fixed) {
        for (uint16_t lineLength : lengths) {
            for (uint8_t maxStrings : maxima) {
                checkInput(source, lineLength, maxStrings, 1, 0, 0);
                checkInput(source, lineLength, maxStrings, 0, 1, 10);
            }
        }
    }
    for (long long n = 0; n < ctx.iterations; n++) {
        std::string source;
        const uint32_t length = next() % 26;
        for (uint32_t i = 0; i < length; i++) {
            source += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        checkInput(source, static_cast<uint16_t>(next() % 41), static_cast<uint8_t>(next() % 6),
            static_cast<int32_t>(next() % 2), static_cast<int32_t>(next() % 2), static_cast<uint16_t>(next() % 41));
    }
}

ORACLE_CASE("util/find_one_of", FindOneOf);
ORACLE_CASE("util/split_string", CaseSplitString);
ORACLE_CASE("rules/hp_con_bonus", HpConBonus);
ORACLE_CASE("ui/ime_composition_rect", CompositionWindowRect);
ORACLE_CASE("ui/on_mouse_wheel", EngineOnMouseWheel);
