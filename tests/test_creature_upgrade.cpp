#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

#include "CBaldurChitin.h"
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
