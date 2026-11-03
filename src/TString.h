#ifndef TSTRING_H_
#define TSTRING_H_

#include <cstring>
#include <stdexcept>

#include "mfc.h"

// BG2 PDB TString: VC6 Dinkumware's reference-counted
// std::basic_string<char>, reconstructed from the assign half - PrepareAssign
// (0x44BC20), Grow (0x44BE10), Tidy (0x448D50) - plus _Xlen (0x7F9AC2) and
// compare (0x4C5ED0). The object is 16 bytes: m_buf[4] at +0 is the union
// head a heap-owning TString never reads (CGameAnimationTypeEffect repurposes
// its byte 0 as a BYTE facing), m_pData at +4 points at the characters,
// m_nLen at +8, m_nRes at +0xC. A heap block starts one byte before the
// characters and that byte is the sharing refcount: 0 = sole owner,
// 0xFF = saturated (never shared, never freed), 1..0xFD = shared - a copy
// takes a ++ instead of duplicating, and any mutation detaches through
// Detach (0x44BD40). Grow rounds capacity |0x1F. This batch imports only the
// assign half (AssignString 0x45E870 with the helpers below), so the class
// declares no constructors or operators; ConstructCopy (0x44BA90), Compare
// (0x4C5ED0), Data/Length and the ctor/dtor/assignment family stay with the
// unimported halves. Offset comments are the exe's natural alignment and are
// informational.

class TString {
public:
    /* 0000 */ char m_buf[4];
    /* 0004 */ char* m_pData;
    /* 0008 */ int m_nLen;
    /* 000C */ int m_nRes;

    TString& AssignString(const TString& source, UINT start, UINT count); // 0x45E870

protected:
    void Detach(); // 0x44BD40
    void SetLength(UINT length); // 0x44BC00
    UINT MaxSize() const; // 0x45EA70
    BOOLEAN PrepareAssign(UINT nLen, BOOLEAN bTrim); // 0x44BC20
    void Grow(UINT nLen); // 0x44BE10
    void Tidy(BOOLEAN bDealloc); // 0x448D50

    static void Xlen() { throw std::length_error("string too long"); } // 0x7F9AC2
    static void Xran() { throw std::out_of_range("invalid string position"); } // 0x7F9C0E
};

#endif /* TSTRING_H_ */
