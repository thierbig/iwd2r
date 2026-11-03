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

// 0x44BE10
void TString::Grow(UINT nLen)
{
    UINT nCap = nLen | 0x1f;
    if (nCap > 0xFFFFFFFD) {
        nCap = nLen;
    }
    char* pBuf;
    try {
        LONG size = static_cast<LONG>(nCap + 2);
        pBuf = new char[size < 0 ? 0 : size];
    } catch (...) {
        nCap = nLen;
        LONG size = static_cast<LONG>(nCap + 2);
        pBuf = new char[size < 0 ? 0 : size];
    }
    UINT nKeep = static_cast<UINT>(m_nLen) < nCap ? static_cast<UINT>(m_nLen) : nCap;
    if (nKeep != 0) {
        memcpy(pBuf + 1, m_pData, nKeep);
    }
    Tidy(TRUE);
    pBuf[0] = 0;
    m_pData = pBuf + 1;
    m_nRes = static_cast<INT>(nCap);
    m_nLen = static_cast<INT>(nKeep);
    m_pData[nKeep] = 0;
}
