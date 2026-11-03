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
