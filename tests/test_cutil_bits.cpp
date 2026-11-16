#include <catch2/catch_test_macros.hpp>

#include "CUtil.h"

// CUtil::RotateFieldRight (0x781630): rotates the low nBits-wide field of
// nValue right by nLowBits — the field's top nLowBits bits wrap to the bottom
// (0x7816E7: value >> (nBits - nLowBits), masked to nLowBits one-bits), the
// rest shifts up (0x7816ED). The shift-up is masked back to the field width
// only when nBits < 4 (0x7816F2).
TEST_CASE("CUtil::RotateFieldRight guards", "[util]")
{
    // value 0, nBits 0, nLowBits 0 and nLowBits >= nBits all return the value
    // unchanged (0x781640-0x781668).
    CHECK(CUtil::RotateFieldRight(0, 18, 1) == 0);
    CHECK(CUtil::RotateFieldRight(7, 0, 1) == 7);
    CHECK(CUtil::RotateFieldRight(7, 18, 0) == 7);
    CHECK(CUtil::RotateFieldRight(5, 3, 3) == 5);
    CHECK(CUtil::RotateFieldRight(5, 3, 4) == 5);
}

TEST_CASE("CUtil::RotateFieldRight rotates the field", "[util]")
{
    // The CGameSound ctor's shape: an 18-bit field rotated by one bit.
    // A lone low bit moves up one place (nothing wraps).
    CHECK(CUtil::RotateFieldRight(1, 18, 1) == 2);
    // A lone top bit (bit 17) wraps to the bottom; the unmasked shift-up
    // also copies it one past the field (bit 18).
    CHECK(CUtil::RotateFieldRight(1 << 17, 18, 1) == ((1 << 18) | 1));
    // A contiguous block walks up by one and gains the wrapped top bit.
    CHECK(CUtil::RotateFieldRight(0x3FFC0, 18, 1) == 0x7FF81);
    // The ctor remap input: 0x1FFE0 rotates onto the compared constant
    // 0x3FFC0, which the ctor then rewrites to 0x3FFF80. The other compared
    // constant ~0x3FFC0 is unreachable: its bit 0 is 1, which needs the
    // input's bit 17 set, but that bit also leaks to the result's bit 18
    // (the unmasked shift-up), which ~0x3FFC0 has clear.
    CHECK(CUtil::RotateFieldRight(0x1FFE0, 18, 1) == 0x3FFC0);
}

TEST_CASE("CUtil::RotateFieldRight narrow-field mask", "[util]")
{
    // nBits < 4 masks the shift-up back to the field: the 3-bit field 0b011
    // of 0b1011 rotates right to 0b110.
    CHECK(CUtil::RotateFieldRight(0b1011, 3, 1) == 0b110);
    // nBits >= 4 leaves the shift-up unmasked, so bits above the field
    // survive (0x7816F2 skips the mask).
    CHECK(CUtil::RotateFieldRight(0b110100, 6, 2) == 0b11010011);
}
