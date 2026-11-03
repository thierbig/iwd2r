#include "TString.h"

#include <cstring>

// 0x448D50
void TString::Tidy(BOOLEAN bDealloc)
{
    if (bDealloc && m_pData != NULL) {
        BYTE rc = static_cast<BYTE>(m_pData[-1]);
        if (rc == 0 || rc == 0xFF) {
            delete[] (m_pData - 1);
        } else {
            m_pData[-1] = static_cast<char>(rc - 1);
        }
    }
    m_pData = NULL;
    m_nLen = 0;
    m_nRes = 0;
}

// 0x44BC00
void TString::SetLength(UINT length)
{
    m_nLen = static_cast<INT>(length);
    m_pData[length] = 0;
}

// 0x45EA70
UINT TString::MaxSize() const
{
    return 0xFFFFFFFD;
}
