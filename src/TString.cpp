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

// 0x44BC20
BOOLEAN TString::PrepareAssign(UINT nLen, BOOLEAN bTrim)
{
    if (nLen > 0xFFFFFFFD) {
        Xlen();
    }
    if (m_pData != NULL && static_cast<BYTE>(m_pData[-1]) != 0 && static_cast<BYTE>(m_pData[-1]) != 0xFF) {
        if (nLen == 0) {
            m_pData[-1] = static_cast<char>(static_cast<BYTE>(m_pData[-1]) - 1);
            Tidy(FALSE);
            return FALSE;
        }
        Grow(nLen);
    } else if (nLen == 0) {
        if (bTrim) {
            Tidy(TRUE);
        } else if (m_pData != NULL) {
            m_nLen = 0;
            m_pData[0] = 0;
        }
        return FALSE;
    } else if (bTrim && !(static_cast<UINT>(m_nRes) < 0x20 && nLen <= static_cast<UINT>(m_nRes))) {
        Tidy(TRUE);
        Grow(nLen);
    } else if (static_cast<UINT>(m_nRes) < nLen) {
        Grow(nLen);
    }
    return TRUE;
}

// 0x44BD40
void TString::Detach()
{
    char* data = m_pData;
    if (data == NULL) {
        return;
    }
    BYTE references = static_cast<BYTE>(data[-1]);
    if (references == 0 || references == 0xFF) {
        return;
    }
    data[-1] = static_cast<char>(references - 1);
    Tidy(FALSE);
    UINT length = static_cast<UINT>(strlen(data));
    if (PrepareAssign(length, TRUE)) {
        memcpy(m_pData, data, length);
        SetLength(length);
    }
}

// 0x45E870
TString& TString::AssignString(const TString& source, UINT start, UINT count)
{
    if (static_cast<UINT>(source.m_nLen) < start) {
        Xran();
    }
    UINT length = static_cast<UINT>(source.m_nLen) - start;
    if (count < length) {
        length = count;
    }
    if (this == &source) {
        UINT end = start + length;
        if (static_cast<UINT>(m_nLen) < end) {
            Xran();
        }
        Detach();
        UINT tail = static_cast<UINT>(m_nLen) - end;
        if (tail != 0) {
            memmove(m_pData + end, m_pData + end + tail, static_cast<UINT>(m_nLen) - end - tail);
            UINT remaining = static_cast<UINT>(m_nLen) - tail;
            if (PrepareAssign(remaining, FALSE)) {
                SetLength(remaining);
            }
        }
        Detach();
        UINT prefix = start < static_cast<UINT>(m_nLen) ? start : static_cast<UINT>(m_nLen);
        if (prefix != 0) {
            memmove(m_pData, m_pData + prefix, static_cast<UINT>(m_nLen) - prefix);
            UINT remaining = static_cast<UINT>(m_nLen) - prefix;
            if (PrepareAssign(remaining, FALSE)) {
                SetLength(remaining);
            }
        }
        return *this;
    }
    const char* data = source.m_pData != NULL ? source.m_pData : "";
    if (length != 0 && length == static_cast<UINT>(source.m_nLen) && static_cast<BYTE>(data[-1]) < 0xFE) {
        Tidy(TRUE);
        m_pData = source.m_pData != NULL ? source.m_pData : const_cast<char*>("");
        m_nLen = source.m_nLen;
        m_nRes = source.m_nRes;
        m_pData[-1] = static_cast<char>(static_cast<BYTE>(m_pData[-1]) + 1);
    } else {
        if (MaxSize() < length) {
            Xlen();
        }
        if (PrepareAssign(length, TRUE)) {
            data = source.m_pData != NULL ? source.m_pData : "";
            memcpy(m_pData, data + start, length);
            SetLength(length);
        }
    }
    return *this;
}
