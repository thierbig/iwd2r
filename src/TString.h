#ifndef TSTRING_H_
#define TSTRING_H_

#include "mfc.h"

#include <cstring>
#include <stdexcept>

// TString -- the engine's string class (BG2 PDB: TString, union sizeof 16). It is
// the VC6 Dinkumware reference-counted std::basic_string<char>: a 16-byte object
// laid out { <union head>[4], char* m_pData @4, int m_nLen @8, int m_nRes @0xc }
// whose heap buffer carries a one-byte reference count immediately ahead of m_pData
// (m_pData[-1]) and whose capacity is rounded up with | 0x1f. Reconstructed from the
// storage/grow/tidy helpers 0x44BC20 / 0x44BE10 / 0x448D50 and the _Xlen throw
// 0x7F9AC2.
//
// Copies share a nonempty buffer while its BYTE reference count is below FE;
// saturated/frozen buffers copy into owned storage. Mutation detaches shared data.
class TString {
public:
    /* 0000 */ char m_buf[4];   // union head; unused once heap-allocated. A caller may
                                // repurpose byte 0 (CGameAnimationTypeEffect stores its
                                // BYTE facing there -- the string ops never touch +0).
    /* 0004 */ char* m_pData;   // heap contents; m_pData[-1] holds the reference count
    /* 0008 */ int m_nLen;
    /* 000C */ int m_nRes;      // capacity, rounded up | 0x1f

    TString()
    {
        m_buf[0] = 0;
        m_buf[1] = 0;
        m_buf[2] = 0;
        m_buf[3] = 0;
        m_pData = NULL;
        m_nLen = 0;
        m_nRes = 0;
    }

    ~TString()
    {
        Tidy(TRUE);
    }

    TString& operator=(const char* s)
    {
        return Assign(s, static_cast<int>(strlen(s)));
    }

    TString(const TString& other)
    {
        ConstructCopy(this, &other);
    }

    // Copy assignment is declared without a definition: the native operator routes
    // through AssignString (0x45E870, not imported), and declaring it suppresses the
    // implicit memberwise assignment the refcounted representation must not get.
    TString& operator=(const TString& other);

private:
    TString& Assign(const char* s, int nLen)
    {
        if (PrepareAssign(static_cast<UINT>(nLen), TRUE)) {
            memcpy(m_pData, s, nLen);
            SetLength(static_cast<UINT>(nLen));
        }
        return *this;
    }

protected:
    static void ConstructCopy(TString* target, const TString* source); // 0x44BA90
    void Detach(); // 0x44BD40
    void SetLength(UINT length); // 0x44BC00
    BOOLEAN PrepareAssign(UINT nLen, BOOLEAN bTrim); // 0x44BC20
    void Grow(UINT nLen); // 0x44BE10
    void Tidy(BOOLEAN bDealloc); // 0x448D50

    // 0x7F9AC2: std::_String_base::_Xlen.
    static void Xlen()
    {
        throw std::length_error("string too long");
    }

    // 0x7F9C0E: std::_String_base::_Xran.
    static void Xran()
    {
        throw std::out_of_range("invalid string position");
    }
};

#endif /* TSTRING_H_ */
