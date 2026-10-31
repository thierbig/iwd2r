#include "CUtil.h"

#include <zlib.h>

#include "CChitin.h"
#include "CVidFont.h"

// #guess
// 0x8FB8E8
CCriticalSection CUtil::ZLIB_CRITICAL_SECTION;

// 0x780AD0
int CUtil::Compress(BYTE* pDest, DWORD* nDestSize, const BYTE* pSrc, DWORD nSrcSize, int nLevel)
{
    CSingleLock compressionLock(&ZLIB_CRITICAL_SECTION, FALSE);
    compressionLock.Lock(INFINITE);

    uLongf nDestLen = *nDestSize;
    int err = compress2(pDest, &nDestLen, pSrc, nSrcSize, nLevel);
    *nDestSize = static_cast<DWORD>(nDestLen);

    compressionLock.Unlock();

    return err;
}

// 0x780B60
int CUtil::Uncompress(BYTE* dest, DWORD* destLen, const BYTE* source, DWORD sourceLen)
{
    CSingleLock decompressionLock(&ZLIB_CRITICAL_SECTION, FALSE);
    decompressionLock.Lock(INFINITE);

    uLongf nDestLen = *destLen;
    int err = uncompress(dest, &nDestLen, source, sourceLen);
    *destLen = static_cast<DWORD>(nDestLen);

    decompressionLock.Unlock();

    return err;
}

// 0x780C00
void CUtil::UtilAssert(int nLineNumber, const char* szFileName, const char* pExp, const char* pText)
{
    // NOTE: Portability: the original takes the name after the last '\\'
    // only; GCC and Clang spell __FILE__ with '/', where that would be a bad
    // pointer, so the last separator of either kind is used.
    const char* fileName = strrchr(szFileName, '\\');
    const char* slash = strrchr(szFileName, '/');
    if (fileName == NULL || (slash != NULL && slash > fileName)) {
        fileName = slash;
    }
    fileName = fileName != NULL ? fileName + 1 : szFileName;
    if (pText == NULL) {
        pText = "no msg.";
    }

    char buffer[1024];
    sprintf(buffer, "ASSERTION FAILED!\nFile: %s\nLine: %d\n\nExp: %s\n\nMsg: %s\nRun Debugger?", fileName, nLineNumber, pExp, pText);

    g_pChitin->SuspendThreads();
    g_pChitin->ShutDown(nLineNumber, fileName, pText);

    abort();
}

// 0x780C80
LONG CUtil::UtilRandInt(LONG nRange, LONG nWeight)
{
    LONG nValue = nWeight + ((nRange * rand()) >> 15);

    if (nValue >= nRange) {
        nValue = nRange - 1;
    }

    if (nValue < 0) {
        nValue = 0;
    }

    return nValue;
}

// 0x780CB0
int CUtil::GetCurrentBitsPerPixels()
{
    HDC dc = GetDC(NULL);
    int bpp = GetDeviceCaps(dc, PLANES) * GetDeviceCaps(dc, BITSPIXEL);
    ReleaseDC(NULL, dc);
    return bpp;
}

// 0x780CF0
BYTE CUtil::SplitString(CVidFont* pTextFont, const CString& sSource, WORD nLineLength, BYTE nMaxStrings, CString* pStringsOut, BOOL bDivideWords, BOOL bDemanded, BOOL bCheckRemainderForNewLine, WORD nSuggestedSplitLength)
{
    if (sSource.Compare("") == 0) {
        return 0;
    }

    if (pStringsOut == NULL || nMaxStrings == 0 || nLineLength == 0) {
        return 0;
    }

    if (!bDemanded) {
        if (pTextFont->pRes->Demand() == NULL) {
            return 0;
        }
    }

    pStringsOut[0] = sSource;
    for (BYTE nNext = 1; nNext < nMaxStrings; nNext++) {
        pStringsOut[nNext] = "";
    }

    BYTE nCurrentLine = 0;
    while (nCurrentLine < nMaxStrings) {
        if (pStringsOut[nCurrentLine].IsEmpty()) {
            break;
        }

        int nSpace = -1;
        int nIndex = 0;
        while (nIndex < pStringsOut[nCurrentLine].GetLength()) {
            char ch = pStringsOut[nCurrentLine][nIndex];
            int nNext = nIndex;

            // With Double Byte Character Support a lead byte takes its trail
            // byte with it: the pair is measured as one unit and the trail
            // byte is never tested for a line break (0x780DF8-0x780E25).
            if (g_pChitin->field_1A0 != 0 && IsDBCSLeadByte(ch)) {
                nNext++;
            } else {
                if (ch == '\n' || ch == '\r') {
                    if (nCurrentLine + 1 < nMaxStrings) {
                        pStringsOut[nCurrentLine + 1] = pStringsOut[nCurrentLine].Mid(nIndex + 1);
                    }

                    if (nCurrentLine + 1 != nMaxStrings || bCheckRemainderForNewLine) {
                        pStringsOut[nCurrentLine] = pStringsOut[nCurrentLine].Left(nIndex);
                    }

                    break;
                }

                if (ch == ' ') {
                    nSpace = nIndex;
                }
            }

            nNext++;

            LONG nSubStringWidth = pTextFont->GetStringLength(pStringsOut[nCurrentLine].Left(nNext), TRUE);

            // NOTE: Signed compare. The DBCS build budgets 10 pixels less per
            // line (0x780E60-0x780E80).
            int nLineLimit = static_cast<int>(nLineLength) - (g_pChitin->field_1A0 != 0 ? 10 : 0);
            if (nSubStringWidth > nLineLimit
                || (nSubStringWidth >= nSuggestedSplitLength
                    && nSpace == nIndex)) {
                if (nCurrentLine + 1 < nMaxStrings) {
                    if (!bDivideWords && nSpace != -1) {
                        pStringsOut[nCurrentLine + 1] = pStringsOut[nCurrentLine].Mid(nSpace + 1);
                        pStringsOut[nCurrentLine] = pStringsOut[nCurrentLine].Left(nSpace);
                    } else {
                        pStringsOut[nCurrentLine + 1] = pStringsOut[nCurrentLine].Mid(nIndex);
                        pStringsOut[nCurrentLine] = pStringsOut[nCurrentLine].Left(nIndex);
                    }
                }

                break;
            }

            nIndex = nNext;
        }

        nCurrentLine++;
    }

    if (!bDemanded) {
        pTextFont->pRes->Release();
    }

    return nCurrentLine;
}

// 0x781100
int CUtil::Find(const CString& sString, char ch, int start)
{
    int nIndex = start;
    while (nIndex < sString.GetLength()) {
        if (g_pChitin->field_1A0 && IsDBCSLeadByte(sString[nIndex])) {
            nIndex++;
        } else {
            if (sString[nIndex] == ch) {
                return nIndex;
            }
        }

        nIndex++;
    }

    return -1;
}

// 0x781180
int CUtil::FindOneOf(const CString& sString, const CString& sSeparators, int start)
{
    CString sMutableSeparators(sSeparators);

    if (g_pChitin->field_1A0) {
        // NOTE: The original bounds this loop by the separators' length
        // before any removal (0x7811CD, reloaded at 0x781261), so after a
        // removal it reads past their end; ours stops at their end.
        int nIndex = 0;
        while (nIndex < sMutableSeparators.GetLength()) {
            if (IsDBCSLeadByte(sMutableSeparators[nIndex])) {
                sMutableSeparators = sMutableSeparators.Left(nIndex) + sMutableSeparators.Mid(nIndex + 1);
                nIndex--;
            }
            nIndex++;
        }

        if (sMutableSeparators.IsEmpty()) {
            return -1;
        }
    }

    // A double-byte character of the searched string is skipped whole
    // (0x78128D-0x7812B9).
    int nIndex = start;
    while (nIndex < sString.GetLength()) {
        if (g_pChitin->field_1A0 && IsDBCSLeadByte(sString[nIndex])) {
            nIndex++;
        } else {
            for (int pos = 0; pos < sMutableSeparators.GetLength(); pos++) {
                if (sString[nIndex] == sMutableSeparators[pos]) {
                    return nIndex;
                }
            }
        }

        nIndex++;
    }

    return -1;
}

// 0x781330
void CUtil::MakeReverse(CString& sString)
{
    CString sReversed;

    int nIndex = 0;
    while (nIndex < sString.GetLength()) {
        if (g_pChitin->field_1A0 && IsDBCSLeadByte(sString[nIndex])) {
            sReversed = sString.Mid(nIndex, 2) + sReversed;
            nIndex++;
        } else {
            sReversed = sString.Mid(nIndex, 1) + sReversed;
        }

        nIndex++;
    }

    sString = sReversed;
}

// 0x7815D0
BOOL CUtil::IsDBCSTrailByte(const CString& sString, int end)
{
    // NOTE: Looks odd.
    int v1 = -2;

    for (int index = 0; index < end; index++) {
        if (IsDBCSLeadByte(sString[index])) {
            v1 = index++;
        }
    }

    return end - v1 == 1;
}

// 0x781470
void CUtil::TrimLeft(CString& sString)
{
    int nIndex = 0;
    while (nIndex < sString.GetLength()) {
        if (g_pChitin->field_1A0 && IsDBCSLeadByte(sString[nIndex])) {
            break;
        }

        if (sString[nIndex] != ' '
            && sString[nIndex] != '\t'
            && sString[nIndex] != '\n') {
            break;
        }

        nIndex++;
    }

    sString = sString.Mid(nIndex);
}

// 0x781520
void CUtil::TrimRight(CString& sString)
{
    int nEnd = -1;
    int nIndex = 0;
    while (nIndex < sString.GetLength()) {
        if (g_pChitin->field_1A0 && IsDBCSLeadByte(sString[nIndex])) {
            nIndex++;
            nEnd = nIndex;
        } else {
            if (sString[nIndex] != ' '
                && sString[nIndex] != '\t'
                && sString[nIndex] != '\n') {
                nEnd = nIndex;
            }
        }

        nIndex++;
    }

    sString = sString.Left(nEnd + 1);
}
