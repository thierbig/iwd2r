#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CResRef.h"
#include "engine_world.h"

// UpgradeCreatureFileV21ToV22 (0x5A5060): the "CRE V2.1" -> "CRE V2.2"
// creature-file converter. It rebuilds the two V2.1 spell blocks (the flat
// memorized list and the grouped known list) into the V2.2 shape (7 per-class
// grouped lists x 9 levels, a domain list, the innate/song/shapeshift
// extras), rewrites the CCreatureFileOffsets table and relocates the
// equipment/item/effect tail. The buffers below are hand-built V2.1 images;
// every expected offset is the layout the converter is required to produce.

namespace {

using engine_world::DataDir;
using engine_world::StartEngine;

// The standard prologue: skips without an install and starts the engine
// once per process. No NewGame — the converter only needs the live CInfGame
// (its spell tables and GetSpellcasterIndex).
CInfGame* GameOrSkip()
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    REQUIRE(pGame != NULL);
    return pGame;
}

// Replaces a game spell table's contents for the duration of the guard and
// restores the original list afterwards (the game destructor frees m_pList).
class TableGuard {
public:
    TableGuard(CSpellResRefList& table, std::vector<CResRef> entries)
        : m_table(table)
        , m_entries(entries)
        , m_oldList(table.m_pList)
        , m_oldCount(table.m_nCount)
    {
        m_table.m_pList = m_entries.data();
        m_table.m_nCount = static_cast<UINT>(m_entries.size());
    }

    ~TableGuard()
    {
        m_table.m_pList = m_oldList;
        m_table.m_nCount = m_oldCount;
    }

private:
    CSpellResRefList& m_table;
    std::vector<CResRef> m_entries;
    CResRef* m_oldList;
    UINT m_oldCount;
};

// A deterministic, non-zero fill so untouched bytes can be told apart from
// the converter's zero-fill and from stale source bytes.
BYTE PatternByte(size_t offset)
{
    return static_cast<BYTE>((offset * 13 + 7) & 0xFF);
}

void FillPattern(BYTE* buffer, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        buffer[i] = PatternByte(i);
    }
}

void PutByte(BYTE* buffer, size_t offset, BYTE value)
{
    buffer[offset] = value;
}

void PutWord(BYTE* buffer, size_t offset, WORD value)
{
    std::memcpy(buffer + offset, &value, sizeof(value));
}

void PutDword(BYTE* buffer, size_t offset, DWORD value)
{
    std::memcpy(buffer + offset, &value, sizeof(value));
}

DWORD GetDword(const BYTE* buffer, size_t offset)
{
    DWORD value = 0;
    std::memcpy(&value, buffer + offset, sizeof(value));
    return value;
}

// RESREFs are 8 bytes, space padded like the engine writes them.
void PutResref(BYTE* buffer, size_t offset, const char* resref)
{
    std::memset(buffer + offset, ' ', 8);
    std::memcpy(buffer + offset, resref, std::strlen(resref));
}

bool ResrefEquals(const BYTE* buffer, size_t offset, const char* resref)
{
    return std::memcmp(buffer + offset, resref, std::strlen(resref)) == 0;
}

// A V2.1 flat memorized-list entry: resref[8], spellLevel@+8, type@+10
// (stride 0xC).
void PutFlatEntry(BYTE* buffer, size_t offset, const char* resref, WORD level, WORD type)
{
    PutResref(buffer, offset, resref);
    PutWord(buffer, offset + 8, level);
    PutWord(buffer, offset + 10, type);
}

// A V2.1 known-list header: level@0, type@6, entryStart@8, count@+0xC
// (stride 0x10).
void PutKnownHeader(BYTE* buffer, size_t offset, WORD level, WORD type, DWORD start, DWORD count)
{
    PutWord(buffer, offset, level);
    PutWord(buffer, offset + 2, 0);
    PutWord(buffer, offset + 4, 0);
    PutWord(buffer, offset + 6, type);
    PutDword(buffer, offset + 8, start);
    PutDword(buffer, offset + 12, count);
}

// A V2.1 known-list pool entry: resref[8], memorized flag byte@+8 (stride
// 0xC).
void PutKnownEntry(BYTE* buffer, size_t offset, const char* resref, BYTE flag)
{
    PutResref(buffer, offset, resref);
    PutByte(buffer, offset + 8, flag);
}

// A V2.1 image skeleton: header, last-block flag, class byte and the tail
// dwords. The spell blocks themselves are test-specific.
void PutV21Skeleton(BYTE* buffer, size_t size, BYTE classByte, DWORD equip, DWORD items, DWORD itemCount, DWORD effects, DWORD effectCount, const char* dialog)
{
    FillPattern(buffer, size);
    std::memcpy(buffer, "CRE V2.1", 8);
    PutByte(buffer, 0x33, 0); // last-block flag: effects are 0x30 bytes each
    PutByte(buffer, 0x387, classByte);
    PutDword(buffer, 0x3D2, equip);
    PutDword(buffer, 0x3D6, items);
    PutDword(buffer, 0x3DA, itemCount);
    PutDword(buffer, 0x3DE, effects);
    PutDword(buffer, 0x3E2, effectCount);
    PutResref(buffer, 0x3E6, dialog);
}

} // namespace

TEST_CASE("upgrade: a V2.1 creature becomes a V2.2 file", "[creature-upgrade][data]")
{
    CInfGame* pGame = GameOrSkip();

    // m_spells: ZZTSTAA1 (known + memorized), ZZTSTFF6 (in the known list
    // only, and absent from every Find — the loop-2 gate test). m_songs:
    // ZZTSTBB2 (both loops), ZZTSTEE5 (found in the table but never added).
    TableGuard spellsGuard(pGame->m_spells, { CResRef("ZZTSTAA1"), CResRef("ZZTSTFF6") });
    TableGuard songsGuard(pGame->m_songs, { CResRef("ZZTSTBB2"), CResRef("ZZTSTEE5") });

    // Flat list at 0x3EE (3 entries), known headers at 0x412 (2), pool at
    // 0x432 (5). H1 (level 1, type 0) holds pool 0..3, H2 (level 3, type 2)
    // pool 3..5.
    // The old spell bytes run 3x0xC + 2x0x10 + (3+2)x0xC = 0x80, so the tail
    // begins at 0x3EE + 0x80 = 0x46E, right behind the pool's last entry.
    std::vector<BYTE> oldBuf(0x52E);
    PutV21Skeleton(oldBuf.data(), oldBuf.size(), 5, 0x46E, 0x4D6, 2, 0x4FE, 1, "DLGTST01");
    PutDword(oldBuf.data(), 0x3BA, 0x3EE);
    PutDword(oldBuf.data(), 0x3BE, 3);
    PutFlatEntry(oldBuf.data(), 0x3EE, "ZZTSTAA1", 1, 0); // grouped[0] level 1
    PutFlatEntry(oldBuf.data(), 0x3FA, "ZZTSTBB2", 0, 2); // songs
    PutFlatEntry(oldBuf.data(), 0x406, "ZZTSTCC3", 2, 0); // obsolete
    PutDword(oldBuf.data(), 0x3C2, 0x412);
    PutDword(oldBuf.data(), 0x3C6, 2);
    PutKnownHeader(oldBuf.data(), 0x412, 1, 0, 0, 3);
    PutKnownHeader(oldBuf.data(), 0x422, 3, 2, 3, 2);
    PutDword(oldBuf.data(), 0x3CA, 0x432);
    PutKnownEntry(oldBuf.data(), 0x432, "ZZTSTAA1", 1);
    PutKnownEntry(oldBuf.data(), 0x43E, "ZZTSTD44", 0);
    PutKnownEntry(oldBuf.data(), 0x44A, "ZZTSTFF6", 1);
    PutKnownEntry(oldBuf.data(), 0x456, "ZZTSTBB2", 1);
    PutKnownEntry(oldBuf.data(), 0x462, "ZZTSTEE5", 0);

    // The caller's side of the contract: a fresh buffer, *plSize already
    // set (oldSize 0x52E + 0x240 + delta 0x1F8), and a fourth word the
    // function must never read.
    BYTE* pCreature = new BYTE[0x966];
    std::memset(pCreature, 0xAA, 0x966);
    LONG size = 0x966;
    LONG unusedWord = 0x12345678;

    CHECK(UpgradeCreatureFileV21ToV22(&pCreature, &size, oldBuf.data(), &unusedWord) == TRUE);

    // Read-only *plSize and untouched fourth argument.
    CHECK(size == 0x966);
    CHECK(unusedWord == 0x12345678);

    const BYTE* out = pCreature;

    CHECK(std::memcmp(out, "CRE V2.2", 8) == 0);
    CHECK(out[0x387] == 5); // class byte preserved (5 is no caster)

    // The 0xDF-dword bulk copy survives below 0x384 (sample bytes, the
    // last-block flag among them); 0x3B5 is the table's pad and stays zero.
    CHECK(std::memcmp(out + 8, oldBuf.data() + 8, 0x37C) == 0);
    CHECK(out[0x33] == oldBuf[0x33]);
    CHECK(out[0x3B4] == oldBuf[0x3B4]);
    CHECK(out[0x3B5] == 0);
    CHECK(GetDword(out, 0x3B6) == GetDword(oldBuf.data(), 0x3B6));
    CHECK(out[0x388] == oldBuf[0x388]);
    CHECK(out[0x389] == oldBuf[0x389]);

    // ZZTSTAA1 was appended by loop 1 and accumulated by loop 2 (its known
    // flag was 1): one entry in grouped[0] level 1 with field_4/field_8 = 2.
    // ZZTSTFF6 found in m_spells but not in the list — the Find gate left
    // it out entirely.
    CHECK(GetDword(out, 0x3BA) == 0x62E); // grouped[0] level 0 offset
    CHECK(GetDword(out, 0x4B6) == 0); // count
    CHECK(GetDword(out, 0x3BE) == 0x636); // grouped[0] level 1 offset
    CHECK(GetDword(out, 0x4BA) == 1); // count
    CHECK(GetDword(out, 0x636) == 0); // m_nID
    CHECK(GetDword(out, 0x63A) == 2); // field_4
    CHECK(GetDword(out, 0x63E) == 2); // field_8
    CHECK(GetDword(out, 0x642) == 0); // field_C
    CHECK(GetDword(out, 0x646) == 0); // field_14
    CHECK(GetDword(out, 0x64A) == 0); // field_18

    // The empty lists' slots chain the running offset: class 1 starts at
    // 0x686, class 6 level 8's pair ends the grouped block at 0x836.
    CHECK(GetDword(out, 0x3DE) == 0x686); // grouped[1] level 0 offset
    CHECK(GetDword(out, 0x4B2) == 0x82E); // grouped[6] level 8 offset

    CHECK(GetDword(out, 0x5B2) == 0x836); // domain level 0 offset
    CHECK(GetDword(out, 0x5D6) == 0); // domain level 0 count

    CHECK(GetDword(out, 0x5FA) == 0x87E); // innate offset
    CHECK(GetDword(out, 0x5FE) == 0); // innate count

    // ZZTSTBB2 went to the song list through both loops; ZZTSTEE5 was in
    // the table but its extra Find missed, so it never landed.
    CHECK(GetDword(out, 0x602) == 0x886); // songs offset
    CHECK(GetDword(out, 0x606) == 1); // songs count
    CHECK(GetDword(out, 0x886) == 0); // m_nID
    CHECK(GetDword(out, 0x88A) == 2); // field_4
    CHECK(GetDword(out, 0x88E) == 2); // field_8
    CHECK(GetDword(out, 0x892) == 0); // field_C

    CHECK(GetDword(out, 0x60A) == 0x89E); // shapeshifts offset
    CHECK(GetDword(out, 0x60E) == 0); // shapeshifts count

    // Tail: the three offsets shift by 0x240 + delta 0x1F8 = 0x438, the
    // counts and the dialog copy verbatim, and the last block (equipment
    // 0x68 + 2 items + 1 effect = 0xC0) lands at the new spell end.
    CHECK(GetDword(out, 0x612) == 0x8A6); // equipment offset
    CHECK(GetDword(out, 0x616) == 0x90E); // item offset
    CHECK(GetDword(out, 0x61A) == 2); // item count
    CHECK(GetDword(out, 0x61E) == 0x936); // effect offset
    CHECK(GetDword(out, 0x622) == 1); // effect count
    CHECK(ResrefEquals(out, 0x626, "DLGTST01"));
    CHECK(std::memcmp(out + 0x8A6, oldBuf.data() + 0x46E, 0xC0) == 0);

    delete[] pCreature;
}

TEST_CASE("upgrade: an empty V2.1 creature upgrades in place", "[creature-upgrade][data]")
{
    CInfGame* pGame = GameOrSkip();

    TableGuard spellsGuard(pGame->m_spells, std::vector<CResRef>());

    // No spells at all; the caller's no-realloc path passes the old buffer
    // itself (pOldCreature NULL). oldSize = 0x3EE + 0x68, newSize = oldSize
    // + 0x240 + 0x258 = 0x8EE.
    BYTE* pCreature = new BYTE[0x8EE];
    PutV21Skeleton(pCreature, 0x456, 0, 0x3EE, 0x456, 0, 0x456, 0, "DLGEMP01");
    PutDword(pCreature, 0x3BA, 0x3EE);
    PutDword(pCreature, 0x3BE, 0);
    PutDword(pCreature, 0x3C2, 0x3EE);
    PutDword(pCreature, 0x3C6, 0);
    PutDword(pCreature, 0x3CA, 0x3EE);
    std::memset(pCreature + 0x456, 0xBB, 0x8EE - 0x456);
    // The upgrade writes back over this buffer, so keep a copy of the old
    // image to compare the relocated tail against.
    const std::vector<BYTE> oldSnapshot(pCreature, pCreature + 0x456);
    LONG size = 0x8EE;
    LONG unusedWord = 0x12345678;

    CHECK(UpgradeCreatureFileV21ToV22(&pCreature, &size, NULL, &unusedWord) == TRUE);

    CHECK(size == 0x8EE);
    CHECK(unusedWord == 0x12345678);

    const BYTE* out = pCreature;

    CHECK(std::memcmp(out, "CRE V2.2", 8) == 0);

    // The prologue normalized the zero class byte to 5 before the build
    // phase re-checked it, so 5 (not 2) is what the file carries.
    CHECK(out[0x387] == 5);

    // Every list is empty: the slots walk 0x62E..0x886 in 8-byte pairs.
    CHECK(GetDword(out, 0x3BA) == 0x62E); // grouped[0] level 0
    CHECK(GetDword(out, 0x4B6) == 0);
    CHECK(GetDword(out, 0x5B2) == 0x826); // domain level 0
    CHECK(GetDword(out, 0x5FA) == 0x86E); // innate
    CHECK(GetDword(out, 0x602) == 0x876); // songs
    CHECK(GetDword(out, 0x60A) == 0x87E); // shapeshifts
    CHECK(GetDword(out, 0x60E) == 0);

    // The empty equipment block (0x68) is the whole last block; the empty
    // item and effect lists now point at the buffer end.
    CHECK(GetDword(out, 0x612) == 0x886);
    CHECK(GetDword(out, 0x616) == 0x8EE);
    CHECK(GetDword(out, 0x61A) == 0);
    CHECK(GetDword(out, 0x61E) == 0x8EE);
    CHECK(GetDword(out, 0x622) == 0);
    CHECK(ResrefEquals(out, 0x626, "DLGEMP01"));
    CHECK(std::memcmp(out + 0x886, oldSnapshot.data() + 0x3EE, 0x68) == 0);

    delete[] pCreature;
}

TEST_CASE("upgrade: a cleric's spells land in the cleric block", "[creature-upgrade][data]")
{
    CInfGame* pGame = GameOrSkip();

    // GetSpellcasterIndex maps CLERIC (3) to block 1.
    UINT classIndex = 0;
    const BYTE cleric = 3;
    classIndex = pGame->GetSpellcasterIndex(cleric);
    CHECK(classIndex == 1);

    TableGuard spellsGuard(pGame->m_spells, { CResRef("ZZTSTGG7") });

    // One flat entry, level 2, no known list: oldSize = 0x3EE + 0xC + 0x68
    // = 0x462, newSize = 0x62E + 0x268 + 0x68 = 0x8FE.
    std::vector<BYTE> oldBuf(0x462);
    PutV21Skeleton(oldBuf.data(), oldBuf.size(), cleric, 0x3FA, 0x462, 0, 0x462, 0, "DLGCLR01");
    PutDword(oldBuf.data(), 0x3BA, 0x3EE);
    PutDword(oldBuf.data(), 0x3BE, 1);
    PutFlatEntry(oldBuf.data(), 0x3EE, "ZZTSTGG7", 2, 0);
    PutDword(oldBuf.data(), 0x3C2, 0x3FA);
    PutDword(oldBuf.data(), 0x3C6, 0);
    PutDword(oldBuf.data(), 0x3CA, 0x3FA);

    BYTE* pCreature = new BYTE[0x8FE];
    std::memset(pCreature, 0xAA, 0x8FE);
    LONG size = 0x8FE;
    LONG unusedWord = 0x12345678;

    CHECK(UpgradeCreatureFileV21ToV22(&pCreature, &size, oldBuf.data(), &unusedWord) == TRUE);

    CHECK(size == 0x8FE);
    CHECK(unusedWord == 0x12345678);

    const BYTE* out = pCreature;

    CHECK(std::memcmp(out, "CRE V2.2", 8) == 0);
    CHECK(out[0x387] == 3);

    // The wizard block (0) stays empty; the spell sits in grouped[1]
    // level 2, whose slot is 0x3BA + (9 + 2) * 4 = 0x3E6 and whose count
    // slot is 0x4B6 + 0x2C = 0x4E2.
    CHECK(GetDword(out, 0x3BA) == 0x62E);
    CHECK(GetDword(out, 0x4B6) == 0);
    CHECK(GetDword(out, 0x3E6) == 0x686);
    CHECK(GetDword(out, 0x4E2) == 1);
    CHECK(GetDword(out, 0x686) == 0); // m_nID
    CHECK(GetDword(out, 0x68A) == 1); // field_4
    CHECK(GetDword(out, 0x68E) == 1); // field_8
    CHECK(GetDword(out, 0x692) == 0); // field_C

    // The spell block ends at 0x896, where the equipment lands.
    CHECK(GetDword(out, 0x612) == 0x896);
    CHECK(std::memcmp(out + 0x896, oldBuf.data() + 0x3FA, 0x68) == 0);

    delete[] pCreature;
}

// ComputeCreatureFileV21ToV22Delta (0x5A69B0): the sizing helper the
// "CRE V2.1" arm of CGameSprite::Unmarshal calls before
// UpgradeCreatureFileV21ToV22. The delta must reproduce the destination
// sizes the three converter tests above already upgrade into
// (0x966 / 0x8EE / 0x8FE). The version-gate reject arm is not drivable
// here: its UTIL_ASSERT(FALSE) shuts the engine down and aborts
// (CUtil::UtilAssert, src/CUtil.cpp:43).
TEST_CASE("upgrade: the V2.1 sizing delta matches the converter's caller contract", "[creature-upgrade][data]")
{
    CInfGame* pGame = GameOrSkip();

    // The TEST A layout: 63 grouped lists of which one is populated (1
    // entry), one populated song list, 9 empty domain levels — nTotal
    // 0x278 against 0x80 old spell bytes.
    {
        TableGuard spellsGuard(pGame->m_spells, { CResRef("ZZTSTAA1"), CResRef("ZZTSTFF6") });
        TableGuard songsGuard(pGame->m_songs, { CResRef("ZZTSTBB2"), CResRef("ZZTSTEE5") });

        std::vector<BYTE> oldBuf(0x52E);
        PutV21Skeleton(oldBuf.data(), oldBuf.size(), 5, 0x46E, 0x4D6, 2, 0x4FE, 1, "DLGTST01");
        PutDword(oldBuf.data(), 0x3BA, 0x3EE);
        PutDword(oldBuf.data(), 0x3BE, 3);
        PutFlatEntry(oldBuf.data(), 0x3EE, "ZZTSTAA1", 1, 0);
        PutFlatEntry(oldBuf.data(), 0x3FA, "ZZTSTBB2", 0, 2);
        PutFlatEntry(oldBuf.data(), 0x406, "ZZTSTCC3", 2, 0);
        PutDword(oldBuf.data(), 0x3C2, 0x412);
        PutDword(oldBuf.data(), 0x3C6, 2);
        PutKnownHeader(oldBuf.data(), 0x412, 1, 0, 0, 3);
        PutKnownHeader(oldBuf.data(), 0x422, 3, 2, 3, 2);
        PutDword(oldBuf.data(), 0x3CA, 0x432);
        PutKnownEntry(oldBuf.data(), 0x432, "ZZTSTAA1", 1);
        PutKnownEntry(oldBuf.data(), 0x43E, "ZZTSTD44", 0);
        PutKnownEntry(oldBuf.data(), 0x44A, "ZZTSTFF6", 1);
        PutKnownEntry(oldBuf.data(), 0x456, "ZZTSTBB2", 1);
        PutKnownEntry(oldBuf.data(), 0x462, "ZZTSTEE5", 0);

        LONG nDelta = ComputeCreatureFileV21ToV22Delta(oldBuf.data());
        CHECK(nDelta == 0x1F8);

        // The caller contract: newSize = oldSize + 0x240 + delta, the size
        // TEST A upgrades into. Drive the converter with the helper-derived
        // size to prove the delta is exactly what 0x5A5060 needs.
        DWORD nNewSize = static_cast<DWORD>(oldBuf.size()) + 0x240 + static_cast<DWORD>(nDelta);
        CHECK(nNewSize == 0x966);

        BYTE* pCreature = new BYTE[nNewSize];
        std::memset(pCreature, 0xAA, nNewSize);
        LONG size = static_cast<LONG>(nNewSize);
        LONG unusedWord = 0x12345678;
        CHECK(UpgradeCreatureFileV21ToV22(&pCreature, &size, oldBuf.data(), &unusedWord) == TRUE);
        CHECK(std::memcmp(pCreature, "CRE V2.2", 8) == 0);
        delete[] pCreature;
    }

    // The TEST B layout: an empty spellbook. Class 0 is normalized to 5
    // in the old buffer; nTotal 0x258 with no old spell bytes.
    {
        TableGuard spellsGuard(pGame->m_spells, std::vector<CResRef>());

        std::vector<BYTE> oldBuf(0x456);
        PutV21Skeleton(oldBuf.data(), oldBuf.size(), 0, 0x3EE, 0x456, 0, 0x456, 0, "DLGEMP01");
        PutDword(oldBuf.data(), 0x3BA, 0x3EE);
        PutDword(oldBuf.data(), 0x3BE, 0);
        PutDword(oldBuf.data(), 0x3C2, 0x3EE);
        PutDword(oldBuf.data(), 0x3C6, 0);
        PutDword(oldBuf.data(), 0x3CA, 0x3EE);

        CHECK(oldBuf[0x387] == 0); // blank class before the call
        LONG nDelta = ComputeCreatureFileV21ToV22Delta(oldBuf.data());
        CHECK(nDelta == 0x258);
        CHECK(oldBuf[0x387] == 5); // normalized in the old buffer, as written
        CHECK(static_cast<DWORD>(oldBuf.size()) + 0x240 + static_cast<DWORD>(nDelta) == 0x8EE);
    }

    // The TEST C layout: one cleric spell at level 2. nTotal 0x268 against
    // 0xC old spell bytes; a real class byte is left alone.
    {
        TableGuard spellsGuard(pGame->m_spells, { CResRef("ZZTSTGG7") });

        std::vector<BYTE> oldBuf(0x462);
        PutV21Skeleton(oldBuf.data(), oldBuf.size(), 3, 0x3FA, 0x462, 0, 0x462, 0, "DLGCLR01");
        PutDword(oldBuf.data(), 0x3BA, 0x3EE);
        PutDword(oldBuf.data(), 0x3BE, 1);
        PutFlatEntry(oldBuf.data(), 0x3EE, "ZZTSTGG7", 2, 0);
        PutDword(oldBuf.data(), 0x3C2, 0x3FA);
        PutDword(oldBuf.data(), 0x3C6, 0);
        PutDword(oldBuf.data(), 0x3CA, 0x3FA);

        LONG nDelta = ComputeCreatureFileV21ToV22Delta(oldBuf.data());
        CHECK(nDelta == 0x25C);
        CHECK(oldBuf[0x387] == 3); // a set class byte is untouched
        CHECK(static_cast<DWORD>(oldBuf.size()) + 0x240 + static_cast<DWORD>(nDelta) == 0x8FE);
    }
}

// UpgradeCreatureFileV20ToV21 (0x5A3DB0): the "CRE V2.0" -> "CRE V2.1"
// step of the same upgrade chain (0x5A7740 dispatches on the signature and
// hands "CRE V2.0" here first; the converter above then takes it to V2.2).
// Unlike that one it touches no engine state — its body is operator
// new[]/delete[] plus memcpy/memset — so these cases run without GameOrSkip
// and without the [data] tag. The caller's buffer is both source and
// destination: Unmarshal grows it by 0xCA before the call, so the buffers
// below are V2.1-sized with a V2.0 image at the front, and *plSize is
// never written.

TEST_CASE("upgrade: a V2.0 fighter becomes a V2.1 file", "[creature-upgrade]")
{
    // A well-formed V2.0 image whose tail sections tile the buffer
    // exactly: one flat memorized spell (12 bytes) at 0x33C, one
    // known-spell header (0x10 bytes, its +0xC count naming 2 spells) at
    // 0x348, two pool entries (0xC each) at 0x358, the equipment header at
    // 0x370, one item slot at equip+0x68 and two 48-byte effects ending at
    // 0x44C. The buffer is 0x44C + 0xCA bytes — what Unmarshal grows it to.
    const LONG nV20Size = 0x44C;
    const LONG nNewSize = 0x516;
    std::vector<BYTE> buffer(nNewSize);
    BYTE* pCreature = buffer.data();
    FillPattern(pCreature, nNewSize);
    std::memcpy(pCreature, "CRE V2.0", 8);
    PutDword(pCreature, 0x14, 0xFA1); // XP: one step past the level-9 bound
    PutByte(pCreature, 0x33, 0); // last-block flag clear -> 48-byte effects
    PutByte(pCreature, 0x2DB, 2); // BG1 FIGHTER
    PutByte(pCreature, 0x234, 5); // level

    // V2.0 spellbook sub-header at 0x308..0x33B (the last two dwords are
    // the dialog resref).
    PutDword(pCreature, 0x308, 0x33C);
    PutDword(pCreature, 0x30C, 1);
    PutDword(pCreature, 0x310, 0x348);
    PutDword(pCreature, 0x314, 1);
    PutDword(pCreature, 0x318, 0x358);
    PutDword(pCreature, 0x31C, 2);
    PutDword(pCreature, 0x320, 0x370);
    PutDword(pCreature, 0x324, 0x3D8);
    PutDword(pCreature, 0x328, 1);
    PutDword(pCreature, 0x32C, 0x3EC);
    PutDword(pCreature, 0x330, 2);
    PutResref(pCreature, 0x334, "DRAGDG01");
    PutDword(pCreature, 0x354, 2); // the known header's own spell count

    const std::vector<BYTE> oldImage(buffer.begin(), buffer.begin() + nV20Size);
    const BYTE* oldBytes = oldImage.data();

    BYTE* pCreatureArg = pCreature;
    LONG nSize = nNewSize;
    CHECK(UpgradeCreatureFileV20ToV21(&pCreatureArg, &nSize) == TRUE);
    CHECK(nSize == nNewSize); // *plSize is read-only
    CHECK(pCreatureArg == pCreature); // written back in place, not reallocated

    const BYTE* out = pCreature;

    // Signature rewrite and the header moves.
    CHECK(std::memcmp(out, "CRE V2.1", 8) == 0);
    CHECK(std::memcmp(out + 8, oldBytes + 8, 0x10) == 0); // incl. the XP dword
    CHECK(std::memcmp(out + 0x18, oldBytes + 0x18, 0x2D) == 0);
    CHECK(std::memcmp(out + 0x46, oldBytes + 0x48, 0xF) == 0);
    CHECK(out[0x25A] == 9); // XP ladder: 0xFA1 > 0xFA0
    CHECK(out[0x21E] == oldBytes[0x45] / 10);
    CHECK(out[0x226] == oldBytes[0x231] / 10);

    // Fighter arm (BG1 id 2): FIGHTER mask, class byte 5, level slots.
    CHECK(GetDword(out, 0x3B6) == 0x10); // CLASSMASK_FIGHTER
    CHECK(out[0x8F] == 5);
    CHECK(out[0x387] == 5); // CAIOBJECTTYPE_C_FIGHTER
    CHECK(out[0x8A] == 5);
    CHECK(out[0x3B4] == 5); // mirror of the class byte
    CHECK(GetDword(out, 0x270) == 0); // no kit

    // Seeds and the 0x2DC block.
    CHECK(out[0x384] == oldBytes[0x2D8]);
    CHECK(out[0x385] == oldBytes[0x2D9]);
    CHECK(out[0x386] == oldBytes[0x2DA]);
    CHECK(std::memcmp(out + 0x388, oldBytes + 0x2DC, 0x2C) == 0);

    // The 13-dword V2.1 spellbook sub-header: the six offsets re-based by
    // the 0xB2 header growth, the counts and the dialog resref unadjusted.
    CHECK(GetDword(out, 0x3BA) == 0x3EE);
    CHECK(GetDword(out, 0x3BE) == 1);
    CHECK(GetDword(out, 0x3C2) == 0x3FA);
    CHECK(GetDword(out, 0x3C6) == 1);
    CHECK(GetDword(out, 0x3CA) == 0x40A);
    CHECK(GetDword(out, 0x3CE) == 2);
    CHECK(GetDword(out, 0x3D2) == 0x422);
    CHECK(GetDword(out, 0x3D6) == 0x48A);
    CHECK(GetDword(out, 0x3DA) == 1);
    CHECK(GetDword(out, 0x3DE) == 0x49E);
    CHECK(GetDword(out, 0x3E2) == 2);
    CHECK(ResrefEquals(out, 0x3E6, "DRAGDG01"));

    // Memorization: 12 (flat entry) + 12*2 (the header's two spells) + 16
    // (the header record) = 52 bytes, copied verbatim to 0x3EE.
    CHECK(std::memcmp(out + 0x3EE, oldBytes + 0x33C, 52) == 0);

    // Item slots and effects, verbatim.
    CHECK(std::memcmp(out + 0x48A, oldBytes + 0x3D8, 20) == 0);
    CHECK(std::memcmp(out + 0x49E, oldBytes + 0x3EC, 96) == 0);

    // Equipment header: 0x68 bytes at 0x422 — first 0x1A 1:1, 0x1A..0x4B
    // shifted +8, the word pair at 0x4C shifted +0x18, 0xFF elsewhere.
    CHECK(std::memcmp(out + 0x422, oldBytes + 0x370, 0x1A) == 0);
    CHECK(std::memcmp(out + 0x444, oldBytes + 0x38A, 0x32) == 0);
    CHECK(std::memcmp(out + 0x486, oldBytes + 0x3BC, 4) == 0);
    for (size_t i = 0x43C; i < 0x444; i++) {
        CHECK(out[i] == 0xFF);
    }
    for (size_t i = 0x476; i < 0x486; i++) {
        CHECK(out[i] == 0xFF);
    }

    CHECK(out[0x4FE] == 0); // past the last section: still the zero fill
}

namespace {

// Runs the converter on a minimal V2.0 image — every section count zero,
// the equipment header at 0x33C the only tail block (its rebuilt 0x68-byte
// V2.1 block lands at 0x3EE..0x456, exactly the end of the buffer) — and
// returns the in-place V2.1 result.
std::vector<BYTE> UpgradeMinimalV20Class(BYTE nClass, BYTE nFirst, BYTE nSecond, BYTE nThird)
{
    const LONG nSize = 0x456;
    std::vector<BYTE> buffer(nSize);
    BYTE* pCreature = buffer.data();
    FillPattern(pCreature, nSize);
    std::memcpy(pCreature, "CRE V2.0", 8);
    PutByte(pCreature, 0x2DB, nClass);
    PutByte(pCreature, 0x234, nFirst);
    PutByte(pCreature, 0x235, nSecond);
    PutByte(pCreature, 0x236, nThird);

    PutDword(pCreature, 0x308, 0x33C);
    PutDword(pCreature, 0x30C, 0);
    PutDword(pCreature, 0x310, 0x33C);
    PutDword(pCreature, 0x314, 0);
    PutDword(pCreature, 0x318, 0x33C);
    PutDword(pCreature, 0x31C, 0);
    PutDword(pCreature, 0x320, 0x33C);
    PutDword(pCreature, 0x324, 0x33C);
    PutDword(pCreature, 0x328, 0);
    PutDword(pCreature, 0x32C, 0x33C);
    PutDword(pCreature, 0x330, 0);
    PutResref(pCreature, 0x334, "DLGTEST");

    BYTE* pCreatureArg = pCreature;
    LONG nSizeArg = nSize;
    CHECK(UpgradeCreatureFileV20ToV21(&pCreatureArg, &nSizeArg) == TRUE);
    return buffer;
}

} // namespace

TEST_CASE("upgrade: V2.0 class ids map to V2.1 class slots", "[creature-upgrade]")
{
    // The BG1 class.ids byte at 0x2DB drives a dispatch whose arms fill
    // the class-mask dword at 0x3B6, the primary-class byte at 0x387, the
    // combined level at 0x8A and the alphabetical level slots (0x8B
    // barbarian ... 0x95 wizard). Dual classes halve each source level
    // (<=3 floors at 1), triple classes take max(v/3, 1).

    {
        // MAGE (1), and its IWD2-beta alias 202: wizard kit dword.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(1, 7, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x400); // CLASSMASK_WIZARD
        CHECK(out[0x95] == 7);
        CHECK(out[0x387] == 0xB); // CAIOBJECTTYPE_C_WIZARD
        CHECK(out[0x8A] == 7);
        CHECK(GetDword(out.data(), 0x270) == 0x4000);

        const std::vector<BYTE> alias = UpgradeMinimalV20Class(202, 4, 0, 0);
        CHECK(GetDword(alias.data(), 0x3B6) == 0x400);
        CHECK(alias[0x95] == 4);
        CHECK(alias[0x387] == 0xB);
    }

    {
        // FIGHTER (2); level 0 floors at 1 before the arm runs.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(2, 5, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x10);
        CHECK(out[0x8F] == 5);
        CHECK(out[0x387] == 5);
        CHECK(out[0x8A] == 5);
        CHECK(GetDword(out.data(), 0x270) == 0);

        const std::vector<BYTE> floored = UpgradeMinimalV20Class(2, 0, 0, 0);
        CHECK(floored[0x8F] == 1);
        CHECK(floored[0x8A] == 1);
    }

    {
        // Blank classes normalize to FIGHTER, and unknown ids fall to the
        // default FIGHTER arm.
        for (BYTE nClass : { BYTE { 0 }, BYTE { 0xFF }, BYTE { 250 } }) {
            const std::vector<BYTE> out = UpgradeMinimalV20Class(nClass, 6, 0, 0);
            CHECK(GetDword(out.data(), 0x3B6) == 0x10);
            CHECK(out[0x387] == 5);
            CHECK(out[0x8F] == 6);
            CHECK(out[0x8A] == 6);
        }
    }

    {
        // THIEF (4): rogue slot, mask 0x100.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(4, 6, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x100); // CLASSMASK_ROGUE
        CHECK(out[0x93] == 6);
        CHECK(out[0x387] == 9); // CAIOBJECTTYPE_C_ROGUE
        CHECK(out[0x8A] == 6);
    }

    {
        // PALADIN (6): paladin slot, mask 0x40, kit dword 1.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(6, 8, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x40); // CLASSMASK_PALADIN
        CHECK(out[0x91] == 8);
        CHECK(out[0x387] == 7); // CAIOBJECTTYPE_C_PALADIN
        CHECK(out[0x8A] == 8);
        CHECK(GetDword(out.data(), 0x270) == 1);
    }

    {
        // F/M (7) 12/3: both halved to 6/1; the exe stores 0x8F twice and
        // the second (halved second class) wins — kept literal — and 0x95
        // is never stored at all.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(7, 12, 3, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x410); // FIGHTER | WIZARD
        CHECK(out[0x8F] == 1);
        CHECK(out[0x95] == 0);
        CHECK(out[0x387] == 5);
        CHECK(out[0x8A] == 7);
    }

    {
        // C/M (14) 10/4: halved to 5/2, CLERIC class byte, levels summed.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(14, 10, 4, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x404); // CLERIC | WIZARD
        CHECK(out[0x8D] == 5);
        CHECK(out[0x95] == 2);
        CHECK(out[0x387] == 3); // CAIOBJECTTYPE_C_CLERIC
        CHECK(out[0x8A] == 7);
    }

    {
        // F/M/T (10) 9/9/9: thirds 3/3/3 summed to 9.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(10, 9, 9, 9);
        CHECK(GetDword(out.data(), 0x3B6) == 0x510); // FIGHTER | ROGUE | WIZARD
        CHECK(out[0x8F] == 3);
        CHECK(out[0x95] == 3);
        CHECK(out[0x93] == 3);
        CHECK(out[0x387] == 5);
        CHECK(out[0x8A] == 9);
    }

    {
        // F/M/C (17) 12/12/12: thirds 4/4/4 summed to 12.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(17, 12, 12, 12);
        CHECK(GetDword(out.data(), 0x3B6) == 0x414); // FIGHTER | CLERIC | WIZARD
        CHECK(out[0x8F] == 4);
        CHECK(out[0x95] == 4);
        CHECK(out[0x8D] == 4);
        CHECK(out[0x387] == 5);
        CHECK(out[0x8A] == 12);
    }

    {
        // DRUID (11) 8: halved to 4, and the level sum is the halved
        // first level, not a pair sum.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(11, 8, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x8); // CLASSMASK_DRUID
        CHECK(out[0x8E] == 4);
        CHECK(out[0x387] == 4); // CAIOBJECTTYPE_C_DRUID
        CHECK(out[0x8A] == 4);
    }

    {
        // RANGER (12) 9: no halving.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(12, 9, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x80); // CLASSMASK_RANGER
        CHECK(out[0x92] == 9);
        CHECK(out[0x387] == 8); // CAIOBJECTTYPE_C_RANGER
        CHECK(out[0x8A] == 9);
    }

    {
        // SORCERER (19) 6: no kit dword.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(19, 6, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x200); // CLASSMASK_SORCERER
        CHECK(out[0x94] == 6);
        CHECK(out[0x387] == 0xA); // CAIOBJECTTYPE_C_SORCERER
        CHECK(out[0x8A] == 6);
        CHECK(GetDword(out.data(), 0x270) == 0);
    }

    {
        // MONK (20) 3: kit dword 8.
        const std::vector<BYTE> out = UpgradeMinimalV20Class(20, 3, 0, 0);
        CHECK(GetDword(out.data(), 0x3B6) == 0x20); // CLASSMASK_MONK
        CHECK(out[0x90] == 3);
        CHECK(out[0x387] == 6); // CAIOBJECTTYPE_C_MONK
        CHECK(out[0x8A] == 3);
        CHECK(GetDword(out.data(), 0x270) == 8);
    }
}

TEST_CASE("upgrade: the V2.0 effects stride follows the last-block flag", "[creature-upgrade]")
{
    // Same layout as the fighter case, but the last-block flag at 0x33 is
    // set and there is one effect: the section copies 0x108 bytes per
    // entry instead of 0x30, so it ends at 0x4F4 and the buffer grows to
    // 0x4F4 + 0xCA.
    const LONG nV20Size = 0x4F4;
    const LONG nNewSize = 0x5BE;
    std::vector<BYTE> buffer(nNewSize);
    BYTE* pCreature = buffer.data();
    FillPattern(pCreature, nNewSize);
    std::memcpy(pCreature, "CRE V2.0", 8);
    PutDword(pCreature, 0x14, 0xFA1);
    PutByte(pCreature, 0x33, 1); // last-block flag set -> 264-byte effects
    PutByte(pCreature, 0x2DB, 2);
    PutByte(pCreature, 0x234, 5);

    PutDword(pCreature, 0x308, 0x33C);
    PutDword(pCreature, 0x30C, 1);
    PutDword(pCreature, 0x310, 0x348);
    PutDword(pCreature, 0x314, 1);
    PutDword(pCreature, 0x318, 0x358);
    PutDword(pCreature, 0x31C, 2);
    PutDword(pCreature, 0x320, 0x370);
    PutDword(pCreature, 0x324, 0x3D8);
    PutDword(pCreature, 0x328, 1);
    PutDword(pCreature, 0x32C, 0x3EC);
    PutDword(pCreature, 0x330, 1);
    PutResref(pCreature, 0x334, "DRAGDG01");
    PutDword(pCreature, 0x354, 2);

    const std::vector<BYTE> oldImage(buffer.begin(), buffer.begin() + nV20Size);
    const BYTE* oldBytes = oldImage.data();

    BYTE* pCreatureArg = pCreature;
    LONG nSize = nNewSize;
    CHECK(UpgradeCreatureFileV20ToV21(&pCreatureArg, &nSize) == TRUE);

    const BYTE* out = pCreature;

    CHECK(std::memcmp(out, "CRE V2.1", 8) == 0);
    CHECK(GetDword(out, 0x3DE) == 0x49E);
    CHECK(GetDword(out, 0x3E2) == 1);
    CHECK(std::memcmp(out + 0x49E, oldBytes + 0x3EC, 264) == 0);

    // The other sections keep their strides and placements.
    CHECK(GetDword(out, 0x3D6) == 0x48A);
    CHECK(std::memcmp(out + 0x48A, oldBytes + 0x3D8, 20) == 0);
    CHECK(std::memcmp(out + 0x3EE, oldBytes + 0x33C, 52) == 0);
    CHECK(std::memcmp(out + 0x422, oldBytes + 0x370, 0x1A) == 0);
    CHECK(out[0x5A6] == 0); // past the widened section: still the zero fill
}

// ---------------------------------------------------------------------------
// CGameSprite::Unmarshal's upgrade ladder (0x70CF90): a "CRE V2.0" (or the
// V9.2 image 0x5A7740 normalizes to one) is grown by 0xCA and brought to
// V2.1; a "CRE V2.1" image is re-sized by ComputeCreatureFileV21ToV22Delta
// + 0x240 and rewritten by 0x5A7940; the sprite then parses the result
// exactly as a native V2.2 file. These tests drive the ladder through the
// real CGameSprite constructor, the way a saved-game or area load does.

namespace {

// CHARBASE.CRE's bytes through the engine's resource system: a known-good
// V2.2 image whose header supplies real stats (name strref, animation,
// colors, portraits) for the band Unmarshal memcpys into m_baseStats.
std::vector<BYTE> LoadCharBaseBytes()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

// A sprite made the way StartCreateChar makes one (0x610540): the ctor's
// Unmarshal call is the ladder's caller.
CGameSprite* MakeSprite(std::vector<BYTE>& bytes)
{
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

} // namespace

TEST_CASE("unmarshal: a V2.0 and a V2.1 creature load as V2.2", "[creature-upgrade][data]")
{
    GameOrSkip();

    const std::vector<BYTE> real = LoadCharBaseBytes();
    REQUIRE(real.size() > 0x384);
    REQUIRE(std::memcmp(real.data(), "CRE V2.2", 8) == 0);
    WORD nRealAnimationWord = 0;
    std::memcpy(&nRealAnimationWord, real.data() + 0x28, sizeof(nRealAnimationWord));
    const short nRealAnimation = static_cast<short>(nRealAnimationWord);

    SECTION("a V2.1 image runs the 0x5A7940 arm")
    {
        std::vector<BYTE> v21(0x456);
        // Every section empty: the rebuilt equipment header at 0x3EE is
        // the only tail block, so the spellbook tables are never consulted.
        PutV21Skeleton(v21.data(), v21.size(), 2, 0x3EE, 0x456, 0, 0x456, 0, "DLGTST01");
        // Real stats for the whole +8..+0x384 band, which the converter
        // copies verbatim and Unmarshal memcpys into m_baseStats. The
        // equipment header stays zeroed: Unmarshal re-equips from its
        // selected-weapon fields after the parse, so garbage there would
        // index weapon sets the creature does not have.
        std::memset(v21.data() + 0x3EE, 0, 0x68);
        std::memcpy(v21.data() + 8, real.data() + 8, 0x37C);
        // Spellbook pointers into the empty equipment header with zero
        // counts, so the upgrade chain's delta walks no entries (the
        // skeleton helper leaves these dwords as fill pattern).
        PutDword(v21.data(), 0x3BA, 0x3EE);
        PutDword(v21.data(), 0x3BE, 0);
        PutDword(v21.data(), 0x3C2, 0x3EE);
        PutDword(v21.data(), 0x3C6, 0);
        PutDword(v21.data(), 0x3CA, 0x3EE);

        CGameSprite* pSprite = MakeSprite(v21);
        REQUIRE(pSprite != NULL);

        // The parse ran on the upgraded V2.2 buffer: the grafted header
        // landed in m_baseStats and the derived abilities followed it —
        // the same observables the native-V2.2 CHARBASE test checks.
        CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
        CHECK(pBStats->m_animationType == nRealAnimation);
        CHECK(pBStats->m_STRBase == real[0x266]);
        CHECK(pSprite->m_derivedStats.m_nSTR == real[0x266]);
        CHECK(pSprite->m_derivedStats.m_nCHR == real[0x26B]);
    }

    SECTION("a V2.0 image runs the 0x5A7740 arm then the 0x5A7940 arm")
    {
        // The V2.0 image tiles exactly: 0x33C bytes of header plus the
        // 0x50-byte V2.0 equipment header (the only tail block). The
        // tiling is load-bearing — the Unmarshal ladder grows the buffer
        // by 0xCA (0xB2 header growth + 0x18 equipment growth) and
        // 0x5A3DB0 writes every section at +0xB2, so a tiled V2.0 file
        // lands exactly at the grown size and 0x5A5060's final
        // size assertion holds.
        std::vector<BYTE> v20(0x38C);
        FillPattern(v20.data(), v20.size());
        std::memcpy(v20.data(), "CRE V2.0", 8);
        PutByte(v20.data(), 0x2DB, 2); // BG1 class.ids: FIGHTER
        PutByte(v20.data(), 0x234, 1);
        PutByte(v20.data(), 0x235, 0);
        PutByte(v20.data(), 0x236, 0);
        // The empty V2.0 spellbook sub-header, exactly as
        // UpgradeMinimalV20Class builds it: the equipment header at 0x33C
        // is the only tail block.
        PutDword(v20.data(), 0x308, 0x33C);
        PutDword(v20.data(), 0x30C, 0);
        PutDword(v20.data(), 0x310, 0x33C);
        PutDword(v20.data(), 0x314, 0);
        PutDword(v20.data(), 0x318, 0x33C);
        PutDword(v20.data(), 0x31C, 0);
        PutDword(v20.data(), 0x320, 0x33C);
        PutDword(v20.data(), 0x324, 0x33C);
        PutDword(v20.data(), 0x328, 0);
        PutDword(v20.data(), 0x32C, 0x33C);
        PutDword(v20.data(), 0x330, 0);
        PutResref(v20.data(), 0x334, "DLGTEST");
        // A zeroed V2.0 equipment header (0x50 bytes at 0x33C, the only
        // tail block): 0x5A3DB0 rebuilds it into the 0x68-byte V2.1 form
        // (0xFF fill plus the source's 0x50 bytes re-based), and Unmarshal
        // re-equips from its selected-weapon fields, so zero keeps the
        // parse benign.
        std::memset(v20.data() + 0x33C, 0, 0x50);
        // Real stats for the +8..+0x45 band — the header band both
        // converters copy verbatim. Beyond it the pattern fill stands in
        // for old-creature data and survives both in-place conversions.
        std::memcpy(v20.data() + 8, real.data() + 8, 0x3D);

        CGameSprite* pSprite = MakeSprite(v20);
        REQUIRE(pSprite != NULL);

        // Same proof, split by source: the grafted band carries the real
        // animation, and 0x5A3DB0's move list pins each ability to its
        // V2.0 source byte — STRBase@0x266 <- 0x238, INTBase@0x267 <-
        // 0x23A, CHRBase@0x26B <- 0x23E (inside the 0x267<-0x23A
        // seven-byte move) — before 0x5A5060 copies the header band
        // verbatim into the V2.2 image.
        CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
        CHECK(pBStats->m_animationType == nRealAnimation);
        CHECK(pBStats->m_STRBase == PatternByte(0x238));
        CHECK(pBStats->m_INTBase == PatternByte(0x23A));
        CHECK(pBStats->m_CHRBase == PatternByte(0x23E));
    }
}
