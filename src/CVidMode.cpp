#include "CVidMode.h"

#include "CChitin.h"
#include "CUtil.h"
#include "CVidCell.h"
#include "CVideo3d.h"
#include "render/api.h"

typedef struct CVIDMODE_CLIP_OUTCODE {
    unsigned int all;
    unsigned int left : 4;
    unsigned int right : 4;
    unsigned int bottom : 4;
    unsigned int top : 4;
} CVIDMODE_CLIP_OUTCODE;

namespace {

// The thick-line slices of DrawThickLine16/24 (0x79A950, 0x7CA630): the
// algorithm of DrawThickLine32's sub_7CC130 on pixels of type T, written by
// put. The adjustments are in units of T (bytes for 24 bits).
template <class T, class Put>
void ThickLineSlices(T* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, UINT nThickness, Put put)
{
    UINT nWholeStep = nMajorAxis / nMinorAxis;
    INT nAdjUp = (nMajorAxis % nMinorAxis) * 2;
    INT nAdjDown = nMinorAxis * 2;
    INT nErrorTerm = (nAdjUp >> 1) - nAdjDown;
    UINT nFinalPixelCount = (nWholeStep >> 1) + 1;
    UINT nInitialPixelCount = nFinalPixelCount;

    if (nAdjUp == 0 && (nWholeStep & 1) == 0) {
        nInitialPixelCount--;
    }

    if ((nWholeStep & 1) != 0) {
        nErrorTerm += nMinorAxis;
    }

    INT nTrailErrorTerm = nErrorTerm;
    UINT nTrail = 0;
    UINT nTrailRun = nInitialPixelCount;
    UINT nLead = (nThickness & ~1) + nInitialPixelCount;

    for (UINT n = nLead; n != 0; n--) {
        put(pSurface);
        pSurface += nMajorAxisAdjust;
    }
    pSurface += nMinorAxisAdjust - static_cast<INT>(nLead) * nMajorAxisAdjust;

    UINT nWait = nThickness - 2;
    for (UINT nSlice = nMinorAxis - 1; nSlice != 0; nSlice--) {
        UINT nRunLength = nWholeStep;
        nErrorTerm += nAdjUp;
        if (nErrorTerm > 0) {
            nRunLength = nWholeStep + 1;
            nErrorTerm -= nAdjDown;
        }

        nLead += nRunLength;
        if (nTrail < nLead) {
            for (UINT n = nLead - nTrail; n != 0; n--) {
                put(pSurface);
                pSurface += nMajorAxisAdjust;
            }
        }
        pSurface -= static_cast<INT>(nLead - nTrail) * nMajorAxisAdjust;

        if (nWait == 0) {
            pSurface += static_cast<INT>(nTrailRun) * nMajorAxisAdjust;
            nTrail += nTrailRun;
            nTrailRun = nWholeStep;
            nTrailErrorTerm += nAdjUp;
            if (nTrailErrorTerm > 0) {
                nTrailRun = nWholeStep + 1;
                nTrailErrorTerm -= nAdjDown;
            }
        } else {
            nWait--;
        }

        pSurface += nMinorAxisAdjust;
    }

    nLead += nFinalPixelCount;
    for (UINT nSlice = 0; nSlice < nThickness; nSlice++) {
        if (nTrail < nLead) {
            for (UINT n = nLead - nTrail; n != 0; n--) {
                put(pSurface);
                pSurface += nMajorAxisAdjust;
            }
        }
        pSurface -= static_cast<INT>(nLead - nTrail) * nMajorAxisAdjust;

        if (nSlice == nThickness - 1) {
            nTrailRun = nFinalPixelCount;
        }

        pSurface += static_cast<INT>(nTrailRun) * nMajorAxisAdjust;
        nTrail += nTrailRun;
        nTrailRun = nWholeStep;
        nTrailErrorTerm += nAdjUp;
        if (nTrailErrorTerm > 0) {
            nTrailRun = nWholeStep + 1;
            nTrailErrorTerm -= nAdjDown;
        }

        pSurface += nMinorAxisAdjust;
    }
}

// DrawRecticle16/24 (0x7989B0, 0x7C9B10): DrawRecticle32's body (0x7CB670)
// with the depth's octants and line, lPitch in the units they take.
template <class T, class HOctant, class VOctant, class Line>
void Recticle(CVidMode* pMode, const CVIDMODE_RECTICLE_DESCRIPTION& rd, T* pSurface, LONG lPitch, const CRect* rClip, HOctant hOctant, VOctant vOctant, Line line)
{
    BYTE* pPixelList = new BYTE[max(rd.xAxis, rd.yAxis)];

    INT v1 = rd.xAxis + rd.piePieceXOffset;
    INT v2 = rd.yAxis + rd.piePieceYOffset;
    INT v3;

    CRect rLineClip;
    BOOLEAN bLineClipped;

    // The lines are clipped to the clip less its last row and column when
    // there is one, and drawn unclipped when there is none.
    if (rClip != NULL) {
        rLineClip.SetRect(rClip->left, rClip->top, rClip->right - 1, rClip->bottom - 1);
        bLineClipped = FALSE;
    } else {
        rLineClip.SetRect(0, 0, 0, 0);
        bLineClipped = TRUE;
    }

    LONG nArcLength = pMode->GetEllipseArcPixelList(rd.xAxis, rd.yAxis, pPixelList);

    nArcLength -= 1;

    UTIL_ASSERT(nArcLength < max(rd.xAxis, rd.yAxis));

    v3 = hOctant(rd.ptCenter.x, rd.ptCenter.y - v2, nArcLength - rd.xGap, pSurface, lPitch, FALSE, pPixelList, rClip);
    line(rd.ptCenter.x + rd.xGap - nArcLength, v3, rd.ptCenter.x, rd.ptCenter.y + rd.piePiecePtYOffset - v2, pSurface, lPitch, rLineClip, bLineClipped);

    v3 = hOctant(rd.ptCenter.x + 1, rd.ptCenter.y + pPixelList[0] - v2, nArcLength - rd.xGap - 1, pSurface, lPitch, TRUE, pPixelList + 1, rClip);
    line(rd.ptCenter.x - rd.xGap + nArcLength, v3, rd.ptCenter.x, rd.ptCenter.y + rd.piePiecePtYOffset - v2, pSurface, lPitch, rLineClip, bLineClipped);

    v3 = hOctant(rd.ptCenter.x, rd.ptCenter.y + v2, nArcLength - rd.xGap, pSurface, -lPitch, FALSE, pPixelList, rClip);
    line(rd.ptCenter.x + rd.xGap - nArcLength, v3, rd.ptCenter.x, rd.ptCenter.y - rd.piePiecePtYOffset + v2, pSurface, lPitch, rLineClip, bLineClipped);

    v3 = hOctant(rd.ptCenter.x + 1, rd.ptCenter.y - pPixelList[0] + v2, nArcLength - rd.xGap - 1, pSurface, -lPitch, TRUE, pPixelList + 1, rClip);
    line(rd.ptCenter.x - rd.xGap + nArcLength, v3, rd.ptCenter.x, rd.ptCenter.y - rd.piePiecePtYOffset + v2, pSurface, lPitch, rLineClip, bLineClipped);

    nArcLength = pMode->GetEllipseArcPixelList(rd.yAxis, rd.xAxis, pPixelList);

    UTIL_ASSERT(nArcLength < max(rd.xAxis, rd.yAxis));

    v3 = vOctant(rd.ptCenter.x - v1, rd.ptCenter.y, nArcLength - rd.yGap, pSurface, -lPitch, TRUE, pPixelList, rClip);
    line(v3, rd.ptCenter.y + rd.yGap - nArcLength, rd.ptCenter.x + rd.piePiecePtXOffset - v1, rd.ptCenter.y, pSurface, lPitch, rLineClip, bLineClipped);

    v3 = vOctant(rd.ptCenter.x + pPixelList[0] - v1, rd.ptCenter.y + 1, nArcLength - rd.yGap - 1, pSurface, lPitch, TRUE, pPixelList + 1, rClip);
    line(v3, rd.ptCenter.y - rd.yGap + nArcLength, rd.ptCenter.x + rd.piePiecePtXOffset - v1, rd.ptCenter.y, pSurface, lPitch, rLineClip, bLineClipped);

    v3 = vOctant(rd.ptCenter.x + v1, rd.ptCenter.y, nArcLength - rd.yGap, pSurface, -lPitch, FALSE, pPixelList, rClip);
    line(v3, rd.ptCenter.y + rd.yGap - nArcLength, rd.ptCenter.x - rd.piePiecePtXOffset + v1, rd.ptCenter.y, pSurface, lPitch, rLineClip, bLineClipped);

    v3 = vOctant(rd.ptCenter.x - pPixelList[0] + v1, rd.ptCenter.y + 1, nArcLength - rd.yGap - 1, pSurface, lPitch, FALSE, pPixelList + 1, rClip);
    line(v3, rd.ptCenter.y - rd.yGap + nArcLength, rd.ptCenter.x - rd.piePiecePtXOffset + v1, rd.ptCenter.y, pSurface, lPitch, rLineClip, bLineClipped);

    delete[] pPixelList;
}

// DrawThickLine16/24 (0x799280, 0x7CA330): DrawThickLine32's body on pixels
// of type T, nUnit Ts to a pixel (3 bytes for 24 bits, else 1), lPitch in
// Ts, nHalf half the thickness rounded up. slices draws the general case.
template <class T, class Put, class Slices>
BOOL ThickLine(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, T* pSurface, LONG lPitch, const CRect& rSurface, int a8, INT nHalf, INT nUnit, BOOLEAN bClipped, Put put, Slices slices)
{
    INT nCount = a8 + 1;
    INT nStep = nUnit;
    INT nDeltaX;
    INT nDeltaY;

    if (!bClipped) {
        CRect rClip;
        rClip.left = rSurface.left + nHalf;
        rClip.top = rSurface.top + nHalf;
        rClip.right = rSurface.right - nHalf;
        rClip.bottom = rSurface.bottom - nHalf;
        if (!CVidMode::ClipLine(nXFrom, nYFrom, nXTo, nYTo, rClip)) {
            return FALSE;
        }
    }

    if (nYTo < nYFrom) {
        INT t;

        t = nXTo;
        nXTo = nXFrom;
        nXFrom = t;

        t = nYTo;
        nYTo = nYFrom;
        nYFrom = t;
    }

    nDeltaX = nXTo - nXFrom;
    if (nDeltaX < 0) {
        nDeltaX = nXFrom - nXTo;
        nStep = -nUnit;
    }

    nDeltaY = nYTo - nYFrom;

    // The line's first pixel moved back half the thickness across and up.
    pSurface += lPitch * (nYFrom - rSurface.top) + (nXFrom - rSurface.left) * nUnit;
    if (nStep > 0) {
        pSurface -= nHalf * nUnit;
    } else {
        pSurface += nHalf * nUnit;
    }
    pSurface -= nHalf * lPitch;

    if (nDeltaX == 0) {
        // Each column starts a row below the previous one.
        if (a8 >= 0) {
            do {
                if (nDeltaY >= 0) {
                    INT n = nDeltaY + 1;
                    do {
                        put(pSurface);
                        pSurface += lPitch;
                        n--;
                    } while (n != 0);
                }
                pSurface += nUnit - nDeltaY * lPitch;
                nCount--;
            } while (nCount != 0);
            return TRUE;
        }
    } else if (nDeltaY == 0) {
        // Each row starts a column after the previous one.
        if (a8 >= 0) {
            do {
                if (nDeltaX >= 0) {
                    INT n = nDeltaX + 1;
                    do {
                        put(pSurface);
                        pSurface += nStep;
                        n--;
                    } while (n != 0);
                }
                nCount--;
                pSurface += lPitch - nDeltaX * nStep;
            } while (nCount != 0);
            return TRUE;
        }
    } else if (nDeltaX == nDeltaY) {
        INT nRow = 0;
        INT nRun = a8 - 1;
        if (a8 > 0) {
            do {
                nRun += nRow < nDeltaX ? 1 : 0;
                for (INT n = nRun; n > 0; n--) {
                    put(pSurface);
                    pSurface += nStep;
                }
                nRow++;
                pSurface += lPitch - nRun * nStep;
            } while (nRow < a8);
        }

        pSurface += nStep;

        INT nRows = nDeltaY - ((a8 & 1) != 0 ? a8 : a8 - 1);
        if (nRows > 0) {
            do {
                for (INT n = nRun; n > 0; n--) {
                    put(pSurface);
                    pSurface += nStep;
                }
                nRows--;
                pSurface += lPitch - (nRun - 1) * nStep;
            } while (nRows != 0);
        }

        if (a8 > 0) {
            INT nAdvance = (1 - nRun) * nStep + lPitch;
            INT nLeft = a8;
            do {
                for (INT n = nRun; n > 0; n--) {
                    put(pSurface);
                    pSurface += nStep;
                }
                pSurface += nAdvance;
                nRun--;
                nAdvance += nStep;
                nLeft--;
            } while (nLeft != 0);
            return TRUE;
        }
    } else {
        if (nDeltaY > nDeltaX) {
            slices(pSurface, nDeltaY, nDeltaX, lPitch, nStep, a8);
        } else {
            slices(pSurface, nDeltaX, nDeltaY, nStep, lPitch, a8);
        }
    }

    return TRUE;
}

}

// 0x8FB964
const CString CVidMode::SCREEN_SHOT_EXTENSION("bmp");

// 0x8FB95C
const CString CVidMode::SCREEN_SHOT_DIR_NAME(".\\ScrnShot");

// 0x8BA320
short CVidMode::word_8BA320 = 100;

// 0x8BA322
BYTE CVidMode::NUM_FADE_FRAMES = 20;

// #guess
// 0x8FB968
BOOL CVidMode::bInitialized;

// 0x799C70
BOOLEAN CVidMode::SaveScreen()
{
    return FALSE;
}

// 0x799C80
BOOL CVidMode::RenderPointer()
{
    return TRUE;
}

// #binary-identical
// 0x799CB0
CVidMode::CVidMode()
{
    m_dwRBitMask = 0;
    m_dwGBitMask = 0;
    m_dwBBitMask = 0;
    field_24 = 0;
    field_98 = 0;
    field_C2 = 0;
    field_C6 = 0;
    field_CA = 0;
    field_D2 = 0;
    field_DC = 0;
    m_bPointerEnabled = FALSE;
    m_bPointerInside = TRUE;
    m_nColorDepth = 0;
    field_E0 = 0;
    m_nSurfaces = 0;
    nTickCount = GetTickCount();
    field_94 = 0;
    m_pPointerVidCell = NULL;
    m_bPointerAnimating = FALSE;
    pSurfaces = NULL;
    m_rPointerStorage.left = 0;
    m_rPointerStorage.top = 0;
    m_rPointerStorage.right = 0;
    m_rPointerStorage.bottom = 0;
    rgbGlobalTint = 0xFFFFFF;
    m_nPrintFile = 0;
    m_bPrintScreen = FALSE;
    m_nBrightnessCorrection = 0;
    m_nGammaCorrection = 0;
    m_nPointerNumber = 0;
    m_dwCursorRenderFlags = 0;
    field_10 = 1;
    field_14 = 0;
    m_bFadeTo = 0;
    m_nFade = NUM_FADE_FRAMES;

    bInitialized = TRUE;
}

// 0x799E10
BOOLEAN CVidMode::SetClipper(IDirectDrawClipper* lpDirectDrawClipper)
{
    return FALSE;
}

// #binary-identical
// 0x799EB0
CVidMode::~CVidMode()
{
    bInitialized = FALSE;
}

// #binary-identical
// 0x799E50
COLORREF CVidMode::GetTransparentColor()
{
    return 65280;
}

// #binary-identical
// 0x799E70
void CVidMode::PrintScreen()
{
    m_bPrintScreen = TRUE;
}

// 0x799E80
int CVidMode::GetPointerNumber()
{
    return m_nPointerNumber;
}

// #binary-identical
// 0x799F40
BOOL CVidMode::CheckResults(HRESULT hr)
{
    if (hr != DD_OK) {
        switch (hr) {
        case DDERR_SURFACELOST:
            RestoreSurfaces();
            break;
        }

        return FALSE;
    }

    return TRUE;
}

// 0x799F70
COLORREF CVidMode::GetGlobalTintColor()
{
    return rgbGlobalTint;
}

// 0x795980
BOOL CVidMode::ClipLine(INT& nXFrom, INT& nYFrom, INT& nXTo, INT& nYTo, const CRect& rClip)
{
    CVIDMODE_CLIP_OUTCODE fromCode;
    CVIDMODE_CLIP_OUTCODE toCode;
    CVIDMODE_CLIP_OUTCODE finalCode;
    INT x;
    INT y;

    // The right edge is tested before the left one (0x7959E4), which only
    // matters for a rectangle whose left is past its right.
    while (1) {
        memset(&fromCode, 0, sizeof(fromCode));
        memset(&toCode, 0, sizeof(toCode));

        if (nYFrom < rClip.top) {
            fromCode.top |= 1;
            fromCode.all += 8;
        } else if (nYFrom > rClip.bottom) {
            fromCode.bottom |= 1;
            fromCode.all += 4;
        }

        if (nXFrom > rClip.right) {
            fromCode.right |= 1;
            fromCode.all += 2;
        } else if (nXFrom < rClip.left) {
            fromCode.left |= 1;
            fromCode.all += 1;
        }

        if (nYTo < rClip.top) {
            toCode.top |= 1;
            toCode.all += 8;
        } else if (nYTo > rClip.bottom) {
            toCode.bottom |= 1;
            toCode.all += 4;
        }

        if (nXTo > rClip.right) {
            toCode.right |= 1;
            toCode.all += 2;
        } else if (nXTo < rClip.left) {
            toCode.left |= 1;
            toCode.all += 1;
        }

        if (fromCode.all == 0 && toCode.all == 0) {
            return TRUE;
        }

        if ((fromCode.all & toCode.all) != 0) {
            return FALSE;
        }

        finalCode = fromCode.all != 0 ? fromCode : toCode;

        if (finalCode.top) {
            y = rClip.top;
            x = nXFrom + (nXTo - nXFrom) * (y - nYFrom) / (nYTo - nYFrom);
        } else if (finalCode.bottom) {
            y = rClip.bottom;
            x = nXFrom + (nXTo - nXFrom) * (y - nYFrom) / (nYTo - nYFrom);
        } else if (finalCode.right) {
            x = rClip.right;
            y = nYFrom + (nYTo - nYFrom) * (x - nXFrom) / (nXTo - nXFrom);
        } else if (finalCode.left) {
            x = rClip.left;
            y = nYFrom + (nYTo - nYFrom) * (x - nXFrom) / (nXTo - nXFrom);
        }

        if (finalCode.all == fromCode.all) {
            nXFrom = x;
            nYFrom = y;
        } else {
            nXTo = x;
            nYTo = y;
        }
    }
}

// #binary-identical
// 0x795BB0
BOOL CVidMode::CreateSurface(IDirectDrawSurface** lplpDirectDrawSurface, const CSize& cSize)
{
    DDCOLORKEY ddck;
    ddck.dwColorSpaceLowValue = m_dwGBitMask;
    ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;

    DDSURFACEDESC ddsd = { 0 };
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDCAPS_BLT;
    ddsd.dwWidth = cSize.cx;
    ddsd.dwHeight = cSize.cy;

    if (g_pChitin->cVideo.m_pDirectDraw2->CreateSurface(&ddsd, lplpDirectDrawSurface, NULL) != DD_OK) {
        *lplpDirectDrawSurface = NULL;
        return FALSE;
    }

    (*lplpDirectDrawSurface)->SetColorKey(DDCKEY_SRCBLT, &ddck);

    return TRUE;
}

// 0x795C50
void CVidMode::DestroySurface(IDirectDrawSurface** lplpDirectDrawSurface)
{
    if (lplpDirectDrawSurface != NULL) {
        if (*lplpDirectDrawSurface != NULL) {
            (*lplpDirectDrawSurface)->Release();
            *lplpDirectDrawSurface = NULL;
        }
    }
}

// 0x795C70
void CVidMode::DisplayFrameRate(UINT nSurface)
{
    if (g_pChitin->field_1A2A) {
        CString sFrameRate;
        sFrameRate.Format("%02d %02d %d ",
            g_pChitin->m_nRenderPerSec,
            g_pChitin->m_nAIPerSec,
            GetNumVRamSurfaces());

        CString v3;
        DoTextOut(nSurface, sFrameRate, (CVideo::SCREENWIDTH >> 1) - 240, 4, 255);
    }
}

// 0x795D50
DWORD CVidMode::ConvertToSurfaceRGB(COLORREF rgb)
{
    if (g_pChitin->cVideo.m_nBpp == 16) {
        return ((GetRValue(rgb) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgb) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgb) >> field_CA) << m_dwBBitShift);
    } else {
        return (GetRValue(rgb) << m_dwRBitShift) | (GetGValue(rgb) << m_dwGBitShift) | (GetBValue(rgb) << m_dwBBitShift);
    }
}

// 0x795E00
COLORREF CVidMode::ApplyBrightnessContrast(COLORREF rgb)
{
    // NOTE: The complements are bytes (0x795E10).
    if (m_nBrightnessCorrection != 0) {
        BYTE v1 = ~m_nBrightnessCorrection;
        rgb = RGB(~static_cast<BYTE>((v1 * static_cast<BYTE>(~GetRValue(rgb))) >> 8),
            ~static_cast<BYTE>((v1 * static_cast<BYTE>(~GetGValue(rgb))) >> 8),
            ~static_cast<BYTE>((v1 * static_cast<BYTE>(~GetBValue(rgb))) >> 8));
    }

    // NOTE: The scale is an int, so a correction above 247 does not wrap
    // (0x795E87).
    if (m_nGammaCorrection != 0) {
        INT v2 = m_nGammaCorrection + (1 << CVidPalette::LIGHT_SCALE);
        rgb = RGB(min((v2 * GetRValue(rgb)) >> CVidPalette::LIGHT_SCALE, 255),
            min((v2 * GetGValue(rgb)) >> CVidPalette::LIGHT_SCALE, 255),
            min((v2 * GetBValue(rgb)) >> CVidPalette::LIGHT_SCALE, 255));
    }

    return rgb;
}

// 0x795F10
void CVidMode::ApplyFadeAmount(CVIDPALETTE_COLOR* pColor)
{
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return;
    }

    pColor->rgbRed = pColor->rgbRed * m_nFade / NUM_FADE_FRAMES;

    // NOTE: Strange check for 3D, probably result of some inlining
    pColor->rgbGreen = g_pChitin->cVideo.m_bIs3dAccelerated
        ? pColor->rgbGreen
        : pColor->rgbGreen * m_nFade / NUM_FADE_FRAMES;

    // NOTE: Same as above.
    pColor->rgbBlue = g_pChitin->cVideo.m_bIs3dAccelerated
        ? pColor->rgbBlue
        : pColor->rgbBlue * m_nFade / NUM_FADE_FRAMES;
}

// 0x795FB0
COLORREF CVidMode::ApplyFadeAmount(COLORREF rgb)
{
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return rgb;
    }

    return RGB(GetRValue(rgb) * m_nFade / NUM_FADE_FRAMES,
        GetGValue(rgb) * m_nFade / NUM_FADE_FRAMES,
        GetBValue(rgb) * m_nFade / NUM_FADE_FRAMES);
}

// 0x796020
INT CVidMode::ApplyFadeAmount(INT nColor)
{
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return nColor;
    }

    return nColor * m_nFade / NUM_FADE_FRAMES;
}

// #binary-identical
// 0x796050
BOOL CVidMode::DrawEllipse(const CPoint& ptCenter, const CSize& axis, const CRect& rClip, COLORREF rgbColor)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawEllipse(ptCenter, axis, rgbColor)) return TRUE;
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return DrawEllipse3d(ptCenter, axis, rClip, rgbColor);
    }

    CRect r;
    r.SetRect(ptCenter.x - axis.cx - 1,
        ptCenter.y - axis.cy - 1,
        ptCenter.x + axis.cx + 1,
        ptCenter.y + axis.cy + 1);

    CRect rClipped;
    if (rClipped.IntersectRect(&r, rClip)) {
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);

        if (!LockSurface(0, &ddsd, rClipped)) {
            return FALSE;
        }

        CRect* pTemp;
        CRect rTemp;
        if (rClip.EqualRect(r)) {
            pTemp = NULL;
        } else {
            // NOTE: Looks like inlined OffsetRect.
            rTemp.left = rClipped.left - r.left;
            rTemp.top = rClipped.top - r.top;
            rTemp.right = rClipped.right - r.left;
            rTemp.bottom = rClipped.bottom - r.top;
            pTemp = &rTemp;
        }

        DrawEllipse(CPoint(axis.cx, axis.cy), axis, ddsd, pTemp, rgbColor);

        // NOTE: Uninline.
        UnLockSurface(0, ddsd.lpSurface);
    }

    return TRUE;
}

// #binary-identical
// 0x7961D0
void CVidMode::DrawEllipse(const CPoint& ptCenter, const CSize& axis, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        DrawEllipse16(ptCenter, axis, ddsd, rClip, rgbColor);
        break;
    case 24:
        DrawEllipse24(ptCenter, axis, ddsd, rClip, rgbColor);
        break;
    case 32:
        DrawEllipse32(ptCenter, axis, ddsd, rClip, rgbColor);
        break;
    }
}

// 0x796260
void CVidMode::DrawEllipse16(const CPoint& ptCenter, const CSize& axis, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    // As DrawEllipse24 (0x7C8B40) with 16-bit pixels: the pitch in pixels
    // (sar at 0x796292) and the colour with the dropped bits.
    BYTE* pPixelList = new BYTE[max(axis.cx, axis.cy)];
    LONG lPitch = ddsd.lPitch >> 1;
    WORD color = static_cast<WORD>(((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift)
        | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift)
        | ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift));
    LONG nArcLength = GetEllipseArcPixelList(axis.cx, axis.cy, pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 1506
    UTIL_ASSERT(nArcLength <= max(axis.cx, axis.cy));

    DrawEllipseHOctant16(ptCenter.x,
        ptCenter.y - axis.cy,
        nArcLength,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseHOctant16(ptCenter.x + 1,
        ptCenter.y + pPixelList[0] - axis.cy,
        nArcLength - 1,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawEllipseHOctant16(ptCenter.x,
        ptCenter.y + axis.cy,
        nArcLength,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        -lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseHOctant16(ptCenter.x + 1,
        ptCenter.y - pPixelList[0] + axis.cy,
        nArcLength - 1,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        -lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    nArcLength = GetEllipseArcPixelList(axis.cy, axis.cx, pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 1529
    UTIL_ASSERT(nArcLength < max(axis.cx, axis.cy));

    DrawEllipseVOctant16(ptCenter.x - axis.cx,
        ptCenter.y,
        nArcLength,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        -lPitch,
        TRUE,
        pPixelList,
        rClip,
        color);

    DrawEllipseVOctant16(ptCenter.x + pPixelList[0] - axis.cx,
        ptCenter.y + 1,
        nArcLength - 1,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawEllipseVOctant16(ptCenter.x + axis.cx,
        ptCenter.y,
        nArcLength,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        -lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseVOctant16(ptCenter.x - pPixelList[0] + axis.cx,
        ptCenter.y + 1,
        nArcLength - 1,
        reinterpret_cast<WORD*>(ddsd.lpSurface),
        lPitch,
        FALSE,
        pPixelList + 1,
        rClip,
        color);

    delete[] pPixelList;
}

// 0x796550
LONG CVidMode::DrawEllipseHOctant16(LONG x, LONG y, INT nArcLength, WORD* pSurface, LONG lPitch, BOOLEAN clockwise, unsigned char* pArcData, const CRect* rClip, WORD color)
{
    INT nStep;

    if (rClip == NULL) {
        if (lPitch < 0) {
            pSurface += x - y * lPitch;
            nStep = -1;
        } else {
            pSurface += x + y * lPitch;
            nStep = 1;
        }

        while (nArcLength != 0) {
            *pSurface = color;

            if (*pArcData++ != 0) {
                pSurface += lPitch;
                y += nStep;
            }

            if (clockwise) {
                pSurface++;
            } else {
                pSurface--;
            }

            nArcLength--;
        }

        return y;
    }

    if (lPitch < 0) {
        pSurface += (x - (y - rClip->top) * lPitch) - rClip->left;
        nStep = -1;
    } else {
        pSurface += ((y - rClip->top) * lPitch - rClip->left) + x;
        nStep = 1;
    }

    while (nArcLength != 0) {
        if (rClip->PtInRect(CPoint(x, y))) {
            *pSurface = color;
        }

        if (*pArcData++ != 0) {
            pSurface += lPitch;
            y += nStep;
        }

        if (clockwise) {
            pSurface++;
            x++;
        } else {
            pSurface--;
            x--;
        }

        nArcLength--;
    }

    return y;
}

// 0x7966B0
LONG CVidMode::DrawEllipseVOctant16(LONG x, LONG y, INT nArcLength, WORD* pSurface, LONG lPitch, BOOLEAN clockwise, unsigned char* pArcData, const CRect* rClip, WORD color)
{
    if (rClip == NULL) {
        if (lPitch < 0) {
            pSurface += x - y * lPitch;
        } else {
            pSurface += x + y * lPitch;
        }

        while (nArcLength != 0) {
            *pSurface = color;

            if (*pArcData++ != 0) {
                if (clockwise) {
                    pSurface++;
                    x++;
                } else {
                    pSurface--;
                    x--;
                }
            }

            pSurface += lPitch;
            nArcLength--;
        }

        return x;
    }

    INT nStep;
    if (lPitch < 0) {
        pSurface += (x - (y - rClip->top) * lPitch) - rClip->left;
        nStep = -1;
    } else {
        pSurface += ((y - rClip->top) * lPitch - rClip->left) + x;
        nStep = 1;
    }

    while (nArcLength != 0) {
        if (rClip->PtInRect(CPoint(x, y))) {
            *pSurface = color;
        }

        if (*pArcData++ != 0) {
            if (clockwise) {
                pSurface++;
                x++;
            } else {
                pSurface--;
                x--;
            }
        }

        pSurface += lPitch;
        y += nStep;
        nArcLength--;
    }

    return x;
}

// #binary-identical
// 0x7967F0
void CVidMode::DrawLightingEffect(const DDSURFACEDESC& ddsd, const CPoint& a3, const CSize& a4, unsigned long a5, unsigned long a6)
{
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return;
    }

    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        DrawLightingEffect16(reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 1,
            a3,
            a4,
            a5,
            a6);
        break;
    case 24:
        DrawLightingEffect24(reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            a3,
            a4,
            a5,
            a6);
        break;
    case 32:
        DrawLightingEffect32(reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 2,
            a3,
            a4,
            a5,
            a6);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
        // __LINE__: 1826
        UTIL_ASSERT(FALSE);
        break;
    }
}

// 0x7968C0
void CVidMode::DrawLightingEffect16(WORD* pSurface, LONG lPitch, const CPoint& a4, const CSize& a5, unsigned long a6, unsigned long a7)
{
    // The scale starts at 64 and a pixel is scaled by it >> 6 (.rdata
    // 0x85DE38, 0x85DE3A), as in the 32- and 24-bit siblings.
    const BYTE nBase = 64;
    const BYTE nShift = 6;

    LONG nHeight = a5.cy;
    if (nHeight == 0) {
        // NOTE: The original allocates the pixel list first and leaks it
        // here (0x7968E6); ours allocates after this test.
        return;
    }

    BYTE* pPixelBuffer = new BYTE[a5.cx > a5.cy ? a5.cx : a5.cy];
    BYTE* pPixelList = pPixelBuffer;

    DWORD rgbAmbient;
    if (rgbGlobalTint != RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT)) {
        rgbAmbient = (((((rgbGlobalTint >> 16) & 0xFF) * ((a7 >> 16) & 0xFF)) >> 8) & 0xFF) << 16
            | ((((a7 >> 8) & 0xFF) * ((rgbGlobalTint >> 8) & 0xFF)) & 0xFF00)
            | ((((a7 & 0xFF) * (rgbGlobalTint & 0xFF)) >> 8) & 0xFF);
    } else {
        rgbAmbient = a7;
    }

    // Each channel brightens to the light's colour plus half the ambient's
    // shortfall, rounded down to a multiple of the height above the base.
    BYTE nRed = 255;
    if (static_cast<INT>(a6 & 0xFF) + ((255 - static_cast<INT>(rgbAmbient & 0xFF)) >> 1) <= 255) {
        nRed = static_cast<BYTE>((a6 & 0xFF) + ((255 - static_cast<INT>(rgbAmbient & 0xFF)) >> 1));
    }

    BYTE nGreen = 255;
    if (static_cast<INT>((a6 >> 8) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 8) & 0xFF)) >> 1) <= 255) {
        nGreen = static_cast<BYTE>(((a6 >> 8) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 8) & 0xFF)) >> 1));
    }

    BYTE nBlue = 255;
    if (static_cast<INT>((a6 >> 16) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 16) & 0xFF)) >> 1) <= 255) {
        nBlue = static_cast<BYTE>(((a6 >> 16) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 16) & 0xFF)) >> 1));
    }

    nRed = nRed > nBase ? static_cast<BYTE>((nRed - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;
    nGreen = nGreen > nBase ? static_cast<BYTE>((nGreen - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;
    nBlue = nBlue > nBase ? static_cast<BYTE>((nBlue - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;

    if (nRed == 0 && nGreen == 0 && nBlue == 0) {
        // NOTE: The original leaks the pixel list here (0x796A28); ours frees
        // it.
        delete[] pPixelBuffer;
        return;
    }

    // Writes one pixel scaled by (nScaleRed, nScaleGreen, nScaleBlue) >> 6.
    // Each channel is taken through its mask (+0x18/+0x1C/+0x20), clamped at
    // the mask's value rather than 255, re-masked and OR'd back together; the
    // original writes two pixels per 32-bit store (0x796D30-0x796D4B), which
    // the per-pixel step below reproduces value for value.
    auto Scale = [this, nShift](WORD* pPixel, BYTE nScaleRed, BYTE nScaleGreen, BYTE nScaleBlue) {
        DWORD nPixel = *pPixel;
        DWORD nScaledRed = ((nPixel & m_dwRBitMask) * nScaleRed) >> nShift;
        if (nScaledRed >= m_dwRBitMask) {
            nScaledRed = m_dwRBitMask;
        }
        DWORD nScaledGreen = ((nPixel & m_dwGBitMask) * nScaleGreen) >> nShift;
        if (nScaledGreen >= m_dwGBitMask) {
            nScaledGreen = m_dwGBitMask;
        }
        DWORD nScaledBlue = ((nPixel & m_dwBBitMask) * nScaleBlue) >> nShift;
        if (nScaledBlue >= m_dwBBitMask) {
            nScaledBlue = m_dwBBitMask;
        }
        *pPixel = static_cast<WORD>((nScaledRed & m_dwRBitMask)
            | (nScaledGreen & m_dwGBitMask)
            | (nScaledBlue & m_dwBBitMask));
    };

    // The upper and lower caps: rows from the top (and the bottom) toward
    // the centre, each a ramp up and down that widens by a pixel to the
    // left as the arc steps.
    LONG nArc = GetEllipseArcPixelList(a5.cx, a5.cy, pPixelList);
    WORD* pRow = pSurface + (a4.y - nHeight) * lPitch + a4.x;
    BYTE nStepRed = static_cast<BYTE>(nRed / nHeight);
    BYTE nStepGreen = static_cast<BYTE>(nGreen / nHeight);
    BYTE nStepBlue = static_cast<BYTE>(nBlue / nHeight);

    for (SHORT nHalf = 0; nHalf < 2; nHalf++) {
        BYTE nRowRed = nStepRed;
        BYTE nRowGreen = nStepGreen;
        BYTE nRowBlue = nStepBlue;

        for (INT nWidth = 1; nWidth - 1 < nArc; nWidth++) {
            if (pPixelList[nWidth - 1] != 0) {
                WORD* pPixel = pRow;
                BYTE nDeltaRed = static_cast<BYTE>(nRowRed / nWidth);
                BYTE nDeltaGreen = static_cast<BYTE>(nRowGreen / nWidth);
                BYTE nDeltaBlue = static_cast<BYTE>(nRowBlue / nWidth);
                BYTE nScaleRed = nBase;
                BYTE nScaleGreen = nBase;
                BYTE nScaleBlue = nBase;

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed += nDeltaRed;
                    nScaleGreen += nDeltaGreen;
                    nScaleBlue += nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                for (SHORT x = 0; x < nWidth - 1; x++) {
                    nScaleRed -= nDeltaRed;
                    nScaleGreen -= nDeltaGreen;
                    nScaleBlue -= nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                pRow += lPitch;
                nRowRed += nStepRed;
                nRowGreen += nStepGreen;
                nRowBlue += nStepBlue;
            }
            pRow--;
        }

        if (nHalf == 0) {
            pRow = pSurface + (a4.y + nHeight) * lPitch + a4.x;
        }

        lPitch = -lPitch;
    }

    // The middle band: rows from the centre outward, a ramp up across the
    // full width and down across nWidth - 1 pixels (the bound at 0x797191 is
    // `dec` of nWidth; the 32-bit sibling shifts it and the 24-bit one does
    // not), narrowing as the arc steps.
    nArc = GetEllipseArcPixelList(a5.cy, a5.cx, pPixelList);
    LONG nWidth = a5.cx;
    pRow = pSurface + a4.y * lPitch - nWidth + a4.x;

    for (SHORT nHalf = 0; nHalf < 2; nHalf++) {
        BYTE nRowRed = nRed;
        BYTE nRowGreen = nGreen;
        BYTE nRowBlue = nBlue;

        if (nArc > 0) {
            for (INT nIndex = 0; nIndex < nArc; nIndex++) {
                // NOTE: The original divides by zero here once the band has
                // no width left (0x7971E0); ours stops drawing.
                if (nWidth == 0) {
                    delete[] pPixelBuffer;
                    return;
                }

                WORD* pPixel = pRow;
                BYTE nDeltaRed = static_cast<BYTE>(nRowRed / nWidth);
                BYTE nDeltaGreen = static_cast<BYTE>(nRowGreen / nWidth);
                BYTE nDeltaBlue = static_cast<BYTE>(nRowBlue / nWidth);
                BYTE nScaleRed = nBase;
                BYTE nScaleGreen = nBase;
                BYTE nScaleBlue = nBase;

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed += nDeltaRed;
                    nScaleGreen += nDeltaGreen;
                    nScaleBlue += nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                for (SHORT x = 0; x < nWidth - 1; x++) {
                    nScaleRed -= nDeltaRed;
                    nScaleGreen -= nDeltaGreen;
                    nScaleBlue -= nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                if (pPixelList[nIndex] != 0) {
                    pRow++;
                    nWidth--;
                }

                pRow -= lPitch;
                nRowRed -= nStepRed;
                nRowGreen -= nStepGreen;
                nRowBlue -= nStepBlue;
            }
        }

        if (nHalf == 0) {
            // The lower half starts a row below the centre, without the
            // first arc step.
            pRow = pSurface + (a4.y + 1) * lPitch - a5.cx + a4.x + pPixelList[0];
            nWidth = a5.cx - pPixelList[0];
            nArc--;
            pPixelList++;
            lPitch = -lPitch;
        }
    }

    delete[] pPixelBuffer;
}

// 0x7977D0
BOOL CVidMode::DrawPoints(CPoint* pPoints, INT nPoints, WORD* pSurface, LONG lPitch, const CRect& rSurface, COLORREF rgbColor, BOOL bClipped)
{
    // The points are surface coordinates; unless bClipped, a point outside
    // rSurface is skipped. lPitch is in bytes.
    DWORD color = ReduceColor(rgbColor);

    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        if (!bClipped) {
            for (; nPoints > 0; nPoints--) {
                if (rSurface.PtInRect(*pPoints)) {
                    pSurface[pPoints->y * (lPitch >> 1) + pPoints->x] = static_cast<WORD>(color);
                }
                pPoints++;
            }
        } else {
            for (; nPoints > 0; nPoints--) {
                pSurface[pPoints->y * (lPitch >> 1) + pPoints->x] = static_cast<WORD>(color);
                pPoints++;
            }
        }
        break;
    case 24:
        if (!bClipped) {
            for (; nPoints > 0; nPoints--) {
                if (rSurface.PtInRect(*pPoints)) {
                    BYTE* pPixel = reinterpret_cast<BYTE*>(pSurface) + pPoints->x * 3 + pPoints->y * lPitch;
                    *reinterpret_cast<WORD*>(pPixel) = static_cast<WORD>(color);
                    pPixel[2] = static_cast<BYTE>(color >> 16);
                }
                pPoints++;
            }
        } else {
            for (; nPoints > 0; nPoints--) {
                BYTE* pPixel = reinterpret_cast<BYTE*>(pSurface) + pPoints->x * 3 + pPoints->y * lPitch;
                *reinterpret_cast<WORD*>(pPixel) = static_cast<WORD>(color);
                pPixel[2] = static_cast<BYTE>(color >> 16);
                pPoints++;
            }
        }
        break;
    case 32:
        if (!bClipped) {
            for (; nPoints > 0; nPoints--) {
                if (rSurface.PtInRect(*pPoints)) {
                    reinterpret_cast<DWORD*>(pSurface)[pPoints->y * (lPitch >> 2) + pPoints->x] = color;
                }
                pPoints++;
            }
        } else {
            for (; nPoints > 0; nPoints--) {
                reinterpret_cast<DWORD*>(pSurface)[pPoints->y * (lPitch >> 2) + pPoints->x] = color;
                pPoints++;
            }
        }
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

// #binary-identical
// 0x797A30
BOOL CVidMode::DrawLine(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, UINT nSurface, const CRect& rSurface, COLORREF rgbColor)
{
    if (auto* b = render::Active(); b && b->Clipped(rSurface).DrawLine({ nXFrom, nYFrom }, { nXTo, nYTo }, rgbColor)) return TRUE;
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return DrawLine3d(nXFrom, nYFrom, nXTo, nYTo, rSurface, rgbColor);
    }

    if (!ClipLine(nXFrom, nYFrom, nXTo, nYTo, rSurface)) {
        return FALSE;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 2391
    UTIL_ASSERT(nSurface != -1);

    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);

    if (!LockSurface(nSurface, &ddsd, rSurface)) {
        return FALSE;
    }

    BOOL rc = DrawLine(nXFrom, nYFrom, nXTo, nYTo, ddsd, rSurface, rgbColor, TRUE);

    // NOTE: Uninline.
    UnLockSurface(nSurface, ddsd.lpSurface);

    return rc;
}

// #binary-identical
// 0x797B80
BOOL CVidMode::DrawLine(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, const DDSURFACEDESC& ddsd, const CRect& rSurface, COLORREF rgbColor, BOOLEAN bClipped)
{
    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        return DrawLine16(nXFrom,
            nYFrom,
            nXTo,
            nYTo,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 1,
            rSurface,
            ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift),
            bClipped);
    case 24:
        return DrawLine24(nXFrom,
            nYFrom,
            nXTo,
            nYTo,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            rSurface,
            (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift),
            bClipped);
    case 32:
        return DrawLine32(nXFrom,
            nYFrom,
            nXTo,
            nYTo,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 2,
            rSurface,
            (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift),
            bClipped);
    default:
        return FALSE;
    }
}

// 0x797D30
BOOL CVidMode::DrawLine16(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, WORD* pSurface, LONG lPitch, const CRect& rSurface, WORD color, BOOLEAN bClipped)
{
    INT nDeltaX;
    INT nDeltaY;
    INT nStep = 1;

    if (!bClipped) {
        if (!ClipLine(nXFrom, nYFrom, nXTo, nYTo, rSurface)) {
            return FALSE;
        }
    }

    if (nYTo < nYFrom) {
        INT t;

        t = nXTo;
        nXTo = nXFrom;
        nXFrom = t;

        t = nYTo;
        nYTo = nYFrom;
        nYFrom = t;
    }

    nDeltaX = nXTo - nXFrom;
    if (nDeltaX < 0) {
        nDeltaX = nXFrom - nXTo;
        nStep = -1;
    }

    nDeltaY = nYTo - nYFrom;

    pSurface += (nYFrom - rSurface.top) * lPitch - rSurface.left + nXFrom;

    if (nDeltaX == 0) {
        while (nDeltaY >= 0) {
            *pSurface = color;
            pSurface += lPitch;
            nDeltaY--;
        }
    } else if (nDeltaY == 0) {
        while (nDeltaX >= 0) {
            *pSurface = color;
            pSurface += nStep;
            nDeltaX--;
        }
    } else if (nDeltaX == nDeltaY) {
        while (nDeltaX >= 0) {
            *pSurface = color;
            pSurface += nStep + lPitch;
            nDeltaX--;
        }
    } else if (nDeltaX < nDeltaY) {
        ScanLine16(pSurface, nDeltaY, nDeltaX, lPitch, nStep, color);
    } else {
        ScanLine16(pSurface, nDeltaX, nDeltaY, nStep, lPitch, color);
    }

    return TRUE;
}

// 0x797E70
//
// Halves every pixel of the rect on the surface: the 50% dark box floating
// text is drawn over.
BOOL CVidMode::DarkenRect(const CRect& r, UINT nSurface, const CRect& rClip)
{
    CRect rDst;
    if (!IntersectRect(&rDst, &rClip, &r)) {
        return FALSE;
    }

    // The intersected rect clamped against the clip again. rDst is already
    // inside rClip (it is r n rClip), so this is the same rect and the
    // offsets below are 0; the original does both (0x797EB0-0x797F70).
    CRect rLock(max(rDst.left, rClip.left),
        max(rDst.top, rClip.top),
        min(rDst.right, rClip.right),
        min(rDst.bottom, rClip.bottom));

    INT dx = rDst.left - rLock.left;
    INT dy = rDst.top - rLock.top;
    // The original counts (rDst.right - rLock.left) - dx and
    // (rDst.bottom - rLock.top) - dy, which is the rect's own width and
    // height.
    INT nCols = rDst.right - rDst.left;
    INT nRows = rDst.bottom - rDst.top;

    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);

    if (!LockSurface(nSurface, &ddsd, rLock)) {
        return FALSE;
    }

    // Each channel's own bits with its top bit dropped, summed
    // (0x797F3E-0x797F66): 0x7F7F7F for an 8-8-8 format.
    DWORD dwHalveMask = ((ddsd.ddpfPixelFormat.dwRBitMask >> 1) & ddsd.ddpfPixelFormat.dwRBitMask)
        + ((ddsd.ddpfPixelFormat.dwGBitMask >> 1) & ddsd.ddpfPixelFormat.dwGBitMask)
        + ((ddsd.ddpfPixelFormat.dwBBitMask >> 1) & ddsd.ddpfPixelFormat.dwBBitMask);

    switch (g_pChitin->cVideo.GetBitsPerPixels()) {
    case 16: {
        BYTE* pPixel = reinterpret_cast<BYTE*>(ddsd.lpSurface) + ddsd.lPitch * dy + dx * 2;
        INT nRowJump = ddsd.lPitch - nCols * 2;
        for (INT y = 0; y < nRows; y++) {
            for (INT x = 0; x < nCols; x++) {
                WORD* pValue = reinterpret_cast<WORD*>(pPixel);
                *pValue = static_cast<WORD>((*pValue >> 1) & dwHalveMask);
                pPixel += 2;
            }
            pPixel += nRowJump;
        }
        break;
    }
    case 24: {
        BYTE* pPixel = reinterpret_cast<BYTE*>(ddsd.lpSurface) + ddsd.lPitch * dy + dx * 3;
        INT nRowJump = ddsd.lPitch - nCols * 3;
        for (INT y = 0; y < nRows; y++) {
            for (INT x = 0; x < nCols; x++) {
                // A 4-byte store over 3-byte pixels: it also writes the next
                // pixel's first byte with this pixel's top byte, which is 0
                // once the halve mask is applied, so every pixel after a
                // row's first reads its low byte as 0 (0x797FEB-0x798031).
                DWORD dwPixel = (pPixel[1] << 16) | (pPixel[2] << 8) | pPixel[0];
                *reinterpret_cast<DWORD*>(pPixel) = (dwPixel >> 1) & dwHalveMask;
                pPixel += 3;
            }
            pPixel += nRowJump;
        }
        break;
    }
    case 32: {
        BYTE* pPixel = reinterpret_cast<BYTE*>(ddsd.lpSurface) + ddsd.lPitch * dy + dx * 4;
        INT nRowJump = ddsd.lPitch - nCols * 4;
        for (INT y = 0; y < nRows; y++) {
            for (INT x = 0; x < nCols; x++) {
                DWORD* pValue = reinterpret_cast<DWORD*>(pPixel);
                *pValue = (*pValue >> 1) & dwHalveMask;
                pPixel += 4;
            }
            pPixel += nRowJump;
        }
        break;
    }
    default:
        break;
    }

    // NOTE: The original inlines the unlock for surface 0 whatever nSurface
    // was locked (0x7980AA-0x7980CA); its one caller, CInfinity::DarkenRect,
    // passes 0.
    UnLockSurface(0, ddsd.lpSurface);

    return TRUE;
}

// 0x7980E0
BOOL CVidMode::DrawRect(const CRect& r, UINT nSurface, const CRect& rClip, COLORREF rgbColor)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawRect(r, rgbColor, false)) return TRUE;
    if (g_pChitin->cVideo.Is3dAccelerated()) {
        return DrawRect3d(r, rClip, rgbColor);
    }

    CRect rLock;
    rLock.left = max(r.left, rClip.left);
    rLock.top = max(r.top, rClip.top);
    rLock.right = min(r.right, rClip.right);
    rLock.bottom = min(r.bottom, rClip.bottom);

    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);

    WORD color16;
    DWORD color;

    if (!LockSurface(nSurface, &ddsd, rLock)) {
        return FALSE;
    }

    INT x1 = r.left - rLock.left;
    INT y1 = r.top - rLock.top;
    INT x2 = r.right - rLock.left;
    INT y2 = r.bottom - rLock.top;

    switch (g_pChitin->cVideo.GetBitsPerPixels()) {
    case 16:
        color16 = ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift);
        DrawLine16(x1,
            y1,
            x2,
            y1,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch / 2,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color16,
            FALSE);
        DrawLine16(x1,
            y2,
            x2,
            y2,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch / 2,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color16,
            FALSE);
        DrawLine16(x1,
            y1,
            x1,
            y2,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch / 2,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color16,
            FALSE);
        DrawLine16(x2,
            y1,
            x2,
            y2,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch / 2,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color16,
            FALSE);
        break;
    case 24:
        color = (GetRValue(rgbColor) << m_dwRBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift);
        DrawLine24(x1,
            y1,
            x2,
            y1,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        DrawLine24(x1,
            y2,
            x2,
            y2,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        DrawLine24(x1,
            y1,
            x1,
            y2,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        DrawLine24(x2,
            y1,
            x2,
            y2,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        break;
    case 32:
        color = (GetRValue(rgbColor) << m_dwRBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift);
        DrawLine32(x1,
            y1,
            x2,
            y1,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch / 4,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        DrawLine32(x1,
            y2,
            x2,
            y2,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch / 4,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        DrawLine32(x1,
            y1,
            x1,
            y2,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch / 4,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        DrawLine32(x2,
            y1,
            x2,
            y2,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch / 4,
            CRect(0, 0, rLock.Width(), rLock.Height()),
            color,
            FALSE);
        break;
    }

    // NOTE: Uninline. The original unlocks surface 0 whatever nSurface is
    // (mov eax,[ecx] at 0x79874C); its one caller, CInfinity::DrawHighlightRect,
    // passes 0. Ours unlocks the surface it locked.
    UnLockSurface(nSurface, ddsd.lpSurface);

    return TRUE;
}

// #binary-identical
// 0x798780
BOOL CVidMode::DrawRecticle(const CVIDMODE_RECTICLE_DESCRIPTION& rd, const CRect& rClip, COLORREF rgbColor)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawRecticle(rd, rgbColor)) return TRUE;
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return DrawRecticle3d(rd, rClip, rgbColor);
    }

    CRect r;
    r.SetRect(rd.ptCenter.x - rd.xAxis - rd.piePieceXOffset - 1,
        rd.ptCenter.y - rd.yAxis - rd.piePieceYOffset - 1,
        rd.ptCenter.x + rd.xAxis + rd.piePieceXOffset + 1,
        rd.ptCenter.y + rd.yAxis + rd.piePieceYOffset + 1);

    CRect rClipped;
    if (rClipped.IntersectRect(r, rClip)) {
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);

        if (!LockSurface(0, &ddsd, rClipped)) {
            return FALSE;
        }

        CVIDMODE_RECTICLE_DESCRIPTION copy;
        memcpy(&copy, &rd, sizeof(copy));
        copy.ptCenter.x = rd.xAxis + rd.piePieceXOffset;
        copy.ptCenter.y = rd.yAxis + rd.piePieceYOffset;

        CRect* pTemp;
        CRect rTemp;
        if (rClip.EqualRect(r)) {
            pTemp = NULL;
        } else {
            // NOTE: Looks like inlined OffsetRect.
            rTemp.left = rClipped.left - r.left;
            rTemp.top = rClipped.top - r.top;
            rTemp.right = rClipped.right - r.left;
            rTemp.bottom = rClipped.bottom - r.top;
            pTemp = &rTemp;
        }

        DrawRecticle(copy, ddsd, pTemp, rgbColor);

        // NOTE: Uninline.
        UnLockSurface(0, ddsd.lpSurface);
    }

    return TRUE;
}

// #binary-identical
// 0x798930
void CVidMode::DrawRecticle(const CVIDMODE_RECTICLE_DESCRIPTION& rd, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        DrawRecticle16(rd, ddsd, rClip, rgbColor);
        break;
    case 24:
        DrawRecticle24(rd, ddsd, rClip, rgbColor);
        break;
    case 32:
        DrawRecticle32(rd, ddsd, rClip, rgbColor);
        break;
    }
}

// 0x7989B0
void CVidMode::DrawRecticle16(const CVIDMODE_RECTICLE_DESCRIPTION& rd, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    // As DrawRecticle32 (0x7CB670) with the 16-bit octants and line, the
    // pitch in pixels and the colour with the dropped bits.
    WORD color = static_cast<WORD>(((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift)
        | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift)
        | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift));

    Recticle(this, rd, reinterpret_cast<WORD*>(ddsd.lpSurface), ddsd.lPitch >> 1, rClip,
        [this, color](LONG x, LONG y, INT n, WORD* p, LONG lPitch, BOOLEAN clockwise, BYTE* pArc, const CRect* pClip) {
            return DrawEllipseHOctant16(x, y, n, p, lPitch, clockwise, pArc, pClip, color);
        },
        [this, color](LONG x, LONG y, INT n, WORD* p, LONG lPitch, BOOLEAN clockwise, BYTE* pArc, const CRect* pClip) {
            return DrawEllipseVOctant16(x, y, n, p, lPitch, clockwise, pArc, pClip, color);
        },
        [this, color](INT x1, INT y1, INT x2, INT y2, WORD* p, LONG lPitch, const CRect& r, BOOLEAN bClipped) {
            DrawLine16(x1, y1, x2, y2, p, lPitch, r, color, bClipped);
        });
}

// #binary-identical
// 0x798F90
BOOL CVidMode::DrawThickLine(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, UINT nSurface, CRect& rSurface, int a8, COLORREF rgbColor)
{
    SHORT v1 = (a8 + 1) >> 1;

    CRect rClip;
    rClip.left = rSurface.left + v1;
    rClip.top = rSurface.top + v1;
    rClip.right = rSurface.right - v1;
    rClip.bottom = rSurface.bottom - v1;

    if (!ClipLine(nXFrom, nYFrom, nXTo, nYTo, rClip)) {
        return FALSE;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 3191
    UTIL_ASSERT(nSurface != -1);

    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);

    if (!LockSurface(nSurface, &ddsd, rSurface)) {
        return FALSE;
    }

    BOOL rc = DrawThickLine(nXFrom, nYFrom, nXTo, nYTo, ddsd, rSurface, a8, rgbColor, TRUE);

    // NOTE: Uninline.
    UnLockSurface(nSurface, ddsd.lpSurface);

    return rc;
}

// #binary-identical
// 0x7990D0
BOOL CVidMode::DrawThickLine(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, const DDSURFACEDESC& ddsd, CRect& rSurface, int a8, COLORREF rgbColor, BOOLEAN bClipped)
{
    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        return DrawThickLine16(nXFrom,
            nYFrom,
            nXTo,
            nYTo,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 1,
            rSurface,
            a8,
            ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift),
            bClipped);
    case 24:
        return DrawThickLine24(nXFrom,
            nYFrom,
            nXTo,
            nYTo,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            rSurface,
            a8,
            (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift),
            bClipped);
    case 32:
        return DrawThickLine32(nXFrom,
            nYFrom,
            nXTo,
            nYTo,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 2,
            rSurface,
            a8,
            (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift),
            bClipped);
    default:
        return FALSE;
    }
}

// 0x799280
BOOL CVidMode::DrawThickLine16(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, WORD* pSurface, LONG lPitch, CRect& rSurface, int a8, WORD color, BOOLEAN bClipped)
{
    // As DrawThickLine32 (0x7CBE70) on 16-bit pixels; half the thickness
    // is taken as a short (movsx at the start).
    return ThickLine(nXFrom, nYFrom, nXTo, nYTo, pSurface, lPitch, rSurface, a8,
        static_cast<SHORT>((a8 + 1) >> 1), 1, bClipped,
        [color](WORD* p) { *p = color; },
        [this, color](WORD* p, UINT nMajor, UINT nMinor, INT nMajorAdjust, INT nMinorAdjust, int nThickness) {
            sub_79A950(p, nMajor, nMinor, nMajorAdjust, nMinorAdjust, nThickness, color);
        });
}

// The 16-bit thick-line slices of DrawThickLine16: sub_7CC130 on 16-bit
// pixels.
//
// 0x79A950
void CVidMode::sub_79A950(WORD* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, UINT nThickness, WORD color)
{
    ThickLineSlices(pSurface, nMajorAxis, nMinorAxis, nMajorAxisAdjust, nMinorAxisAdjust, nThickness,
        [color](WORD* p) { *p = color; });
}

// 0x799540
BOOL CVidMode::OutlinePoly(const CPoint* pPoly, SHORT nVertices, const CRect& rSurface, COLORREF rgbColor, const CPoint& ptOffset)
{
    if (auto* b = render::Active(); b && b->Clipped(rSurface).DrawPoly(render::Minus(pPoly, nVertices, ptOffset).data(), nVertices, rgbColor, false, 0)) return TRUE;
    CRect rLock(0, 0, rSurface.right - rSurface.left, rSurface.bottom - rSurface.top);
    INT nOffsetX = rSurface.left + ptOffset.x;
    INT nOffsetY = ptOffset.y + rSurface.top;
    SHORT nIndex;

    if (nVertices < 1) {
        return FALSE;
    }

    if (g_pChitin->cVideo.Is3dAccelerated()) {
        return OutlinePoly3d(pPoly, nVertices, rSurface, rgbColor, ptOffset);
    }

    if (rSurface.IsRectEmpty()) {
        return FALSE;
    }

    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);

    BOOL bLocked = LockSurface(0, &ddsd, rSurface);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 3466
    UTIL_ASSERT(ddsd.lpSurface != NULL);

    if (!bLocked) {
        return FALSE;
    }

    WORD color16;
    DWORD color;

    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        color16 = static_cast<WORD>(((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift));
        for (nIndex = 0; nIndex < nVertices - 1; nIndex++) {
            DrawLine16(pPoly[nIndex].x - nOffsetX,
                pPoly[nIndex].y - nOffsetY,
                pPoly[nIndex + 1].x - nOffsetX,
                pPoly[nIndex + 1].y - nOffsetY,
                reinterpret_cast<WORD*>(ddsd.lpSurface),
                ddsd.lPitch >> 1,
                rLock,
                color16,
                FALSE);
        }
        DrawLine16(pPoly[nVertices - 1].x - nOffsetX,
            pPoly[nVertices - 1].y - nOffsetY,
            pPoly[0].x - nOffsetX,
            pPoly[0].y - nOffsetY,
            reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 1,
            rLock,
            color16,
            FALSE);
        break;
    case 24:
        color = (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift) | (GetRValue(rgbColor) << m_dwRBitShift);
        for (nIndex = 0; nIndex < nVertices - 1; nIndex++) {
            DrawLine24(pPoly[nIndex].x - nOffsetX,
                pPoly[nIndex].y - nOffsetY,
                pPoly[nIndex + 1].x - nOffsetX,
                pPoly[nIndex + 1].y - nOffsetY,
                reinterpret_cast<BYTE*>(ddsd.lpSurface),
                ddsd.lPitch,
                rLock,
                color,
                FALSE);
        }
        DrawLine24(pPoly[nVertices - 1].x - nOffsetX,
            pPoly[nVertices - 1].y - nOffsetY,
            pPoly[0].x - nOffsetX,
            pPoly[0].y - nOffsetY,
            reinterpret_cast<BYTE*>(ddsd.lpSurface),
            ddsd.lPitch,
            rLock,
            color,
            FALSE);
        break;
    case 32:
        color = (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift) | (GetRValue(rgbColor) << m_dwRBitShift);
        for (nIndex = 0; nIndex < nVertices - 1; nIndex++) {
            DrawLine32(pPoly[nIndex].x - nOffsetX,
                pPoly[nIndex].y - nOffsetY,
                pPoly[nIndex + 1].x - nOffsetX,
                pPoly[nIndex + 1].y - nOffsetY,
                reinterpret_cast<DWORD*>(ddsd.lpSurface),
                ddsd.lPitch >> 2,
                rLock,
                color,
                FALSE);
        }
        DrawLine32(pPoly[nVertices - 1].x - nOffsetX,
            pPoly[nVertices - 1].y - nOffsetY,
            pPoly[0].x - nOffsetX,
            pPoly[0].y - nOffsetY,
            reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch >> 2,
            rLock,
            color,
            FALSE);
        break;
    }

    // NOTE: Uninline.
    UnLockSurface(0, ddsd.lpSurface);

    return TRUE;
}

// #binary-identical
// 0x7999B0
BOOL CVidMode::EraseScreen(UINT nSurface, COLORREF rgbColor)
{
    if (auto* b = render::Active(); b && b->Clipped(CRect(0, 0, CVideo::SCREENWIDTH, CVideo::SCREENHEIGHT)).DrawRect(CRect(0, 0, CVideo::SCREENWIDTH, CVideo::SCREENHEIGHT), rgbColor, true)) return TRUE;
    if (nSurface >= m_nSurfaces) {
        return FALSE;
    }

    IDirectDrawSurface* pSurface = pSurfaces[nSurface];
    if (pSurface == NULL) {
        return FALSE;
    }

    DDBLTFX fx;
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift);

    HRESULT hr;
    do {
        hr = g_pChitin->cVideo.cVidBlitter.Blt(pSurface, NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);
        CheckResults(hr);
    } while (hr == DDERR_SURFACELOST || hr == DDERR_WASSTILLDRAWING);

    return TRUE;
}

// 0x799A90
int CVidMode::GetColorDepth()
{
    return m_nColorDepth;
}

// 0x799AA0
LONG CVidMode::GetEllipseArcPixelList(LONG nMajorAxis, LONG nMinorAxis, BYTE* pPixelList)
{
    INT nMajorAxisSqrd = nMajorAxis * nMajorAxis;
    INT nMinorAxisSqrd = nMinorAxis * nMinorAxis;
    INT nMajorAxisAdjust = 0;
    INT nMinorAxisAdjust = nMinorAxis * nMajorAxisSqrd * 2;
    INT nThreshold = nMajorAxisSqrd / 4 - nMinorAxis * nMajorAxisSqrd;
    LONG nArcLength = 0;

    do {
        nThreshold += nMajorAxisAdjust + nMinorAxisSqrd;
        if (nThreshold < 0) {
            pPixelList[nArcLength] = 0;
        } else {
            pPixelList[nArcLength] = 1;
            nMinorAxisAdjust -= 2 * nMajorAxisSqrd;
            nThreshold -= nMinorAxisAdjust;
        }

        nMajorAxisAdjust += 2 * nMinorAxisSqrd;

        nArcLength++;
    } while (nMajorAxisAdjust < nMinorAxisAdjust);

    return nArcLength;
}

// #binary-identical
// 0x799B00
BOOL CVidMode::GetCursorSurfaceSize(CSize& size)
{
    size.cx = 0;
    size.cy = 0;
    return FALSE;
}

// #binary-identical
// 0x799B00
BOOL CVidMode::GetFXSize(CSize& size)
{
    size.cx = 0;
    size.cy = 0;
    return FALSE;
}

// #binary-identical
// 0x799B20
BOOL CVidMode::GetFXSurface(INT& nSurface, DWORD dwFlags)
{
    nSurface = 0;
    return FALSE;
}

// #binary-identical
// 0x799B30
int CVidMode::GetType()
{
    return 255;
}

// #binary-identical
// 0x799B40
BOOL CVidMode::RenderPoly(UINT nSurface, CRect& a3, POINT* apt, int cpt, COLORREF rgbColor)
{
    if (nSurface >= m_nSurfaces) {
        return FALSE;
    }

    IDirectDrawSurface* pSurface = pSurfaces[nSurface];
    if (pSurface == NULL) {
        return FALSE;
    }

    HRESULT hr;
    HDC hdc;

    do {
        hr = pSurface->GetDC(&hdc);
        CheckResults(hr);
    } while (hr == DDERR_SURFACELOST || hr == DDERR_WASSTILLDRAWING);

    if (hr != DD_OK) {
        return FALSE;
    }

    HPEN hPen = CreatePen(PS_SOLID, 1, rgbColor);
    HBRUSH hBrush = CreateSolidBrush(rgbColor);
    HGDIOBJ hPrevPen = SelectObject(hdc, hPen);
    HGDIOBJ hPrevBrush = SelectObject(hdc, hBrush);
    Polygon(hdc, apt, cpt);
    SelectObject(hdc, hPrevPen);
    SelectObject(hdc, hPrevBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);

    do {
        hr = pSurface->ReleaseDC(hdc);
        CheckResults(hr);
    } while (hr == DDERR_SURFACELOST || hr == DDERR_WASSTILLDRAWING);

    return TRUE;
}

// 0x799C60
BOOLEAN CVidMode::PrintSurfaceToBmp(LPBYTE& data, int a3, const CRect& r, LONG& size, short a6)
{
    return FALSE;
}

// 0x799CA0
void CVidMode::DestroySurfaces()
{
}

// 0x799E30
void CVidMode::DoTextOut(UINT nSurface, const CString& sText, int x, int y, COLORREF color)
{
}

// 0x799E40
void CVidMode::DoTextOut(UINT nSurface, const CString& sText, int x, int y, COLORREF color, int height)
{
}

// 0x799E60
void CVidMode::LoadFogOWarSurfaces(const CString& a2)
{
}

// 0x799CA0
void CVidMode::RestoreSurfaces()
{
}

// 0x799F90
CVidCell* CVidMode::GetPointer()
{
    return m_pPointerVidCell;
}

// #binary-identical
// 0x799FA0
BOOL CVidMode::LockSurface(UINT nIndex, LPDDSURFACEDESC pSurfaceDesc, const CRect& rect)
{
    RECT r = static_cast<RECT>(rect);

    if (nIndex >= m_nSurfaces) {
        return FALSE;
    }

    IDirectDrawSurface* pSurface = pSurfaces[nIndex];
    if (pSurface == NULL) {
        return FALSE;
    }

    HRESULT hr;
    do {
        while (TRUE) {
            pSurfaceDesc->dwSize = sizeof(*pSurfaceDesc);
            hr = pSurface->Lock(&r, pSurfaceDesc, DDLOCK_WAIT, NULL);
            if (hr != DDERR_SURFACELOST) {
                break;
            }
            RestoreSurfaces();
        }

        CheckResults(hr);
    } while (hr == DDERR_SURFACEBUSY);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 5071
    UTIL_ASSERT(g_pChitin->cVideo.GetBitsPerPixels() == 24 || !(static_cast<LONG>(reinterpret_cast<INT_PTR>(pSurfaceDesc->lpSurface)) & 0x00000001));

    return hr == DD_OK;
}

// 0x79A080
BOOL CVidMode::PolyLine(INT nSurface, CRect& rSurface, LPPOINT lpPoints, int nCount, COLORREF rgbColor, int nThickness)
{
    if (auto* b = render::Active(); b && b->Clipped(rSurface).DrawPolyLine(lpPoints, nCount, rgbColor, nThickness)) return TRUE;
    BOOL bResult;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo.cpp
    // __LINE__: 5104
    UTIL_ASSERT(nSurface != -1 && (nThickness > 0));

    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);
    // NOTE: The lock's result is returned when no segment is drawn.
    bResult = LockSurface(nSurface, &ddsd, rSurface);
    if (!bResult) {
        return FALSE;
    }

    switch (g_pChitin->cVideo.GetBitsPerPixels()) {
    case 16:
        if (nThickness == 1) {
            while (nCount - 1 > 0) {
                bResult = DrawLine16(lpPoints[0].x,
                    lpPoints[0].y,
                    lpPoints[1].x,
                    lpPoints[1].y,
                    reinterpret_cast<WORD*>(ddsd.lpSurface),
                    ddsd.lPitch >> 1,
                    rSurface,
                    static_cast<WORD>(((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift)),
                    FALSE);
                lpPoints++;
                nCount--;
            }
        } else {
            while (nCount - 1 > 0) {
                bResult = DrawThickLine16(lpPoints[0].x,
                    lpPoints[0].y,
                    lpPoints[1].x,
                    lpPoints[1].y,
                    reinterpret_cast<WORD*>(ddsd.lpSurface),
                    ddsd.lPitch >> 1,
                    rSurface,
                    nThickness,
                    static_cast<WORD>(((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift)),
                    FALSE);
                lpPoints++;
                nCount--;
            }
        }
        break;
    case 24:
        if (nThickness == 1) {
            while (nCount - 1 > 0) {
                bResult = DrawLine24(lpPoints[0].x,
                    lpPoints[0].y,
                    lpPoints[1].x,
                    lpPoints[1].y,
                    reinterpret_cast<BYTE*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    rSurface,
                    (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift),
                    FALSE);
                lpPoints++;
                nCount--;
            }
        } else {
            while (nCount - 1 > 0) {
                bResult = DrawThickLine24(lpPoints[0].x,
                    lpPoints[0].y,
                    lpPoints[1].x,
                    lpPoints[1].y,
                    reinterpret_cast<BYTE*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    rSurface,
                    nThickness,
                    (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift),
                    FALSE);
                lpPoints++;
                nCount--;
            }
        }
        break;
    case 32:
        if (nThickness == 1) {
            while (nCount - 1 > 0) {
                bResult = DrawLine32(lpPoints[0].x,
                    lpPoints[0].y,
                    lpPoints[1].x,
                    lpPoints[1].y,
                    reinterpret_cast<DWORD*>(ddsd.lpSurface),
                    ddsd.lPitch >> 2,
                    rSurface,
                    (GetRValue(rgbColor) << m_dwRBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift),
                    FALSE);
                lpPoints++;
                nCount--;
            }
        } else {
            while (nCount - 1 > 0) {
                bResult = DrawThickLine32(lpPoints[0].x,
                    lpPoints[0].y,
                    lpPoints[1].x,
                    lpPoints[1].y,
                    reinterpret_cast<DWORD*>(ddsd.lpSurface),
                    ddsd.lPitch >> 2,
                    rSurface,
                    nThickness,
                    (GetRValue(rgbColor) << m_dwRBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift),
                    FALSE);
                lpPoints++;
                nCount--;
            }
        }
        break;
    }

    // NOTE: Uninline.
    UnLockSurface(nSurface, ddsd.lpSurface);

    return bResult;
}

// #binary-identical
// 0x79A670
BOOL CVidMode::SetPointer(CVidCell* pVidCell, BOOLEAN bAnimating, INT nPointerNumber)
{
    CSingleLock lock(&m_csRenderPointer, FALSE);
    lock.Lock(INFINITE);

    m_nPointerNumber = nPointerNumber;
    m_pPointerVidCell = pVidCell;
    m_bPointerAnimating = bAnimating;

    lock.Unlock();

    return TRUE;
}

// 0x79A700
BOOL CVidMode::SetPointer(CVidCell* pVidCell, CResRef cResRef, BOOLEAN bAnimating, INT nPointerNumber)
{
    CSingleLock lock(&m_csRenderPointer, FALSE);
    lock.Lock(INFINITE);

    // NOTE: Uninline.
    pVidCell->SetResRef(cResRef, FALSE, TRUE, TRUE);

    if (pVidCell->pRes == NULL) {
        lock.Unlock();
        return FALSE;
    }

    m_bPointerAnimating = bAnimating;
    m_nPointerNumber = nPointerNumber;
    m_pPointerVidCell = pVidCell;

    lock.Unlock();

    return TRUE;
}

// #binary-identical
// 0x79A820
BOOL CVidMode::UnLockSurface(UINT nIndex, LPVOID lpSurfaceData)
{
    if (nIndex >= m_nSurfaces) {
        return FALSE;
    }

    IDirectDrawSurface* pSurface = pSurfaces[nIndex];
    if (pSurface == NULL) {
        return FALSE;
    }

    return pSurface->Unlock(lpSurfaceData) == DD_OK;
}

// 0x79A860
void CVidMode::ScanLine16(WORD* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, WORD color)
{
    // A run-sliced line: runs of whole steps along the major axis, the
    // first and last half runs, one minor step between runs.
    UINT nWholeStep = nMajorAxis / nMinorAxis;
    INT nAdjUp = (nMajorAxis % nMinorAxis) * 2;
    INT nErrorTerm = (nAdjUp >> 1) - nMinorAxis * 2;
    UINT nFinalRun = (nWholeStep >> 1) + 1;
    UINT nInitialRun = nFinalRun;
    UINT nRun;

    if (nAdjUp == 0 && (nWholeStep & 1) == 0) {
        nInitialRun = nWholeStep >> 1;
    }

    if ((nWholeStep & 1) != 0) {
        nErrorTerm += nMinorAxis;
    }

    for (nRun = nInitialRun; nRun != 0; nRun--) {
        *pSurface = color;
        pSurface += nMajorAxisAdjust;
    }

    UINT nRows = nMinorAxis;
    while (true) {
        nRows--;
        pSurface += nMinorAxisAdjust;
        if (nRows == 0) {
            break;
        }

        nErrorTerm += nAdjUp;
        nRun = nWholeStep;
        if (nErrorTerm > 0) {
            nRun = nWholeStep + 1;
            nErrorTerm -= nMinorAxis * 2;
        }

        for (; nRun != 0; nRun--) {
            *pSurface = color;
            pSurface += nMajorAxisAdjust;
        }
    }

    for (nRun = nFinalRun; nRun != 0; nRun--) {
        *pSurface = color;
        pSurface += nMajorAxisAdjust;
    }
}

// The 16-bit pixel color with each channel scaled by nScale / 256.
//
// 0x79AD20
DWORD CVidMode::ScaleColor16(DWORD color, INT nScale)
{
    color &= 0xFFFF;

    return ((((m_dwRBitMask & color) >> (m_dwRBitShift & 0x1F)) * nScale >> 8) << (m_dwRBitShift & 0x1F))
        | ((((m_dwGBitMask & color) >> (m_dwGBitShift & 0x1F)) * nScale >> 8) << (m_dwGBitShift & 0x1F))
        | ((((m_dwBBitMask & color) >> (m_dwBBitShift & 0x1F)) * nScale >> 8) << (m_dwBBitShift & 0x1F));
}

// The 16-bit pixel's channels brought to 8 bits and summed, green twice.
//
// 0x79AD90
DWORD CVidMode::GetIntensity16(DWORD color)
{
    color &= 0xFFFF;

    return (((m_dwBBitMask & color) >> (m_dwBBitShift & 0x1F)) << (field_CA & 0x1F))
        + (((m_dwGBitMask & color) >> (m_dwGBitShift & 0x1F)) << (field_C6 & 0x1F)) * 2
        + (((m_dwRBitMask & color) >> (m_dwRBitShift & 0x1F)) << (field_C2 & 0x1F));
}

// The colour as a pixel of the current depth: each channel at its shift,
// above 16 bpp whole, else less the bits the 16-bit format drops and cut to
// the low word.
//
// 0x79ADF0
DWORD CVidMode::PackColor(COLORREF rgbColor)
{
    if (g_pChitin->cVideo.m_nBpp > 16) {
        return (GetRValue(rgbColor) << (m_dwRBitShift & 0x1F))
            | (GetGValue(rgbColor) << (m_dwGBitShift & 0x1F))
            | (GetBValue(rgbColor) << (m_dwBBitShift & 0x1F));
    }

    return ((GetGValue(rgbColor) >> (field_C6 & 0x1F)) << (m_dwGBitShift & 0x1F)
               | (GetBValue(rgbColor) >> (field_CA & 0x1F)) << (m_dwBBitShift & 0x1F)
               | (GetRValue(rgbColor) >> (field_C2 & 0x1F)) << (m_dwRBitShift & 0x1F))
        & 0xFFFF;
}

// #binary-identical
// 0x79AB50
DWORD CVidMode::ReduceColor(COLORREF rgbColor)
{
    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return rgbColor | 0xFF000000;
    }

    if (g_pChitin->cVideo.m_nBpp == 16) {
        return ((GetRValue(rgbColor) >> field_C2) << m_dwRBitShift) | ((GetGValue(rgbColor) >> field_C6) << m_dwGBitShift) | ((GetBValue(rgbColor) >> field_CA) << m_dwBBitShift);
    } else {
        return (GetBValue(rgbColor) << m_dwBBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetRValue(rgbColor) << m_dwRBitShift);
    }
}

// 0x79AC20
void CVidMode::SetWindowedMode(HWND hWnd)
{
    LONG windowStyle = GetWindowLongA(hWnd, GWL_STYLE);
    windowStyle &= ~(WS_POPUP | WS_CAPTION | WS_MINIMIZEBOX);
    windowStyle |= (WS_CAPTION | WS_MINIMIZEBOX);
    SetWindowLongA(hWnd, GWL_STYLE, windowStyle);

    RECT windowRect;
    SetRect(&windowRect, 0, 0, CVideo::SCREENWIDTH, CVideo::SCREENHEIGHT);

    AdjustWindowRectEx(&windowRect,
        GetWindowLongA(hWnd, GWL_STYLE),
        GetMenu(hWnd) != NULL,
        GetWindowLongA(hWnd, GWL_EXSTYLE));

    SetWindowPos(hWnd,
        HWND_TOP,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);

    SetWindowPos(hWnd,
        HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    RECT workRect;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workRect, 0);

    GetWindowRect(hWnd, &windowRect);

    if (windowRect.left < workRect.left) {
        windowRect.left = workRect.left;
    }

    if (windowRect.top < workRect.top) {
        windowRect.top = workRect.top;
    }

    SetWindowPos(hWnd,
        HWND_TOP,
        windowRect.left,
        windowRect.top,
        0,
        0,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
}

// 0x7BC100
BOOL CVidMode::ActivateVideoMode(CVidMode* pPrevVidMode, HWND hWnd, BOOLEAN bFullscreen)
{
    return TRUE;
}

// 0x7C8B40
LONG CVidMode::DrawEllipse24(const CPoint& ptCenter, const CSize& axis, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    BYTE* pPixelList = new BYTE[max(axis.cx, axis.cy)];
    DWORD color = (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift) | (GetRValue(rgbColor) << m_dwRBitShift);
    LONG nArcLength = GetEllipseArcPixelList(axis.cx, axis.cy, pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo24.cpp
    // __LINE__: 68
    UTIL_ASSERT(nArcLength <= max(axis.cx, axis.cy));

    // The pitch stays in bytes: the octants step three bytes a pixel.
    DrawEllipseHOctant24(ptCenter.x,
        ptCenter.y - axis.cy,
        nArcLength,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        ddsd.lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseHOctant24(ptCenter.x + 1,
        ptCenter.y + pPixelList[0] - axis.cy,
        nArcLength - 1,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        ddsd.lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawEllipseHOctant24(ptCenter.x,
        ptCenter.y + axis.cy,
        nArcLength,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        -ddsd.lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseHOctant24(ptCenter.x + 1,
        ptCenter.y - pPixelList[0] + axis.cy,
        nArcLength - 1,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        -ddsd.lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    nArcLength = GetEllipseArcPixelList(axis.cy, axis.cx, pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo24.cpp
    // __LINE__: 91
    UTIL_ASSERT(nArcLength < max(axis.cx, axis.cy));

    DrawEllipseVOctant24(ptCenter.x - axis.cx,
        ptCenter.y,
        nArcLength,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        -ddsd.lPitch,
        TRUE,
        pPixelList,
        rClip,
        color);

    DrawEllipseVOctant24(ptCenter.x + pPixelList[0] - axis.cx,
        ptCenter.y + 1,
        nArcLength - 1,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        ddsd.lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawEllipseVOctant24(ptCenter.x + axis.cx,
        ptCenter.y,
        nArcLength,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        -ddsd.lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseVOctant24(ptCenter.x - pPixelList[0] + axis.cx,
        ptCenter.y + 1,
        nArcLength - 1,
        reinterpret_cast<BYTE*>(ddsd.lpSurface),
        ddsd.lPitch,
        FALSE,
        pPixelList + 1,
        rClip,
        color);

    delete[] pPixelList;

    return 0;
}

// 0x7C8E10
LONG CVidMode::DrawEllipseVOctant24(LONG x, LONG y, INT nArcLength, BYTE* pSurface, LONG lPitch, BOOLEAN clockwise, unsigned char* pArcData, const CRect* rClip, DWORD color)
{
    if (rClip == NULL) {
        if (lPitch < 0) {
            pSurface += x * 3 - y * lPitch;
        } else {
            pSurface += y * lPitch + x * 3;
        }

        while (nArcLength != 0) {
            pSurface[2] = static_cast<BYTE>(color >> 16);
            *reinterpret_cast<WORD*>(pSurface) = static_cast<WORD>(color);

            if (*pArcData++ != 0) {
                if (clockwise) {
                    pSurface += 3;
                    x++;
                } else {
                    pSurface -= 3;
                    x--;
                }
            }

            pSurface += lPitch;
            nArcLength--;
        }
    } else {
        INT nStep;
        if (lPitch < 0) {
            pSurface += (x - rClip->left) * 3 - (y - rClip->top) * lPitch;
            nStep = -1;
        } else {
            pSurface += (x - rClip->left) * 3 + (y - rClip->top) * lPitch;
            nStep = 1;
        }

        while (nArcLength != 0) {
            if (rClip->PtInRect(CPoint(x, y))) {
                *reinterpret_cast<WORD*>(pSurface) = static_cast<WORD>(color);
                pSurface[2] = static_cast<BYTE>(color >> 16);
            }

            if (*pArcData++ != 0) {
                if (clockwise) {
                    pSurface += 3;
                    x++;
                } else {
                    pSurface -= 3;
                    x--;
                }
            }

            pSurface += lPitch;
            y += nStep;
            nArcLength--;
        }
    }

    return x;
}

// A light pool on a 24-bpp surface: an ellipse of half-axes a5 centred on a4
// whose pixels are scaled by a factor rising from the edge toward the centre.
// a6 is the light's colour, a7 the ambient colour (tinted by the global tint).
// Pixels are three bytes, R at +0, G at +1, B at +2; lPitch is in bytes.
//
// 0x7C8F70
void CVidMode::DrawLightingEffect24(BYTE* pSurface, LONG lPitch, const CPoint& a4, const CSize& a5, unsigned long a6, unsigned long a7)
{
    // The scale starts at 64 and a pixel is scaled by it >> 6 (.rdata
    // 0x85DE38, 0x85DE3A), as in the 32-bit sibling.
    const BYTE nBase = 64;
    const BYTE nShift = 6;

    LONG nHeight = a5.cy;
    if (nHeight == 0) {
        // NOTE: The original allocates the pixel list first and leaks it
        // here (0x7C8F8E); ours allocates after this test.
        return;
    }

    BYTE* pPixelBuffer = new BYTE[a5.cx > a5.cy ? a5.cx : a5.cy];
    BYTE* pPixelList = pPixelBuffer;

    DWORD rgbAmbient;
    if (rgbGlobalTint != RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT)) {
        rgbAmbient = (((((rgbGlobalTint >> 16) & 0xFF) * ((a7 >> 16) & 0xFF)) >> 8) & 0xFF) << 16
            | ((((a7 >> 8) & 0xFF) * ((rgbGlobalTint >> 8) & 0xFF)) & 0xFF00)
            | ((((a7 & 0xFF) * (rgbGlobalTint & 0xFF)) >> 8) & 0xFF);
    } else {
        rgbAmbient = a7;
    }

    // Each channel brightens to the light's colour plus half the ambient's
    // shortfall, rounded down to a multiple of the height above the base.
    BYTE nRed = 255;
    if (static_cast<INT>(a6 & 0xFF) + ((255 - static_cast<INT>(rgbAmbient & 0xFF)) >> 1) <= 255) {
        nRed = static_cast<BYTE>((a6 & 0xFF) + ((255 - static_cast<INT>(rgbAmbient & 0xFF)) >> 1));
    }

    BYTE nGreen = 255;
    if (static_cast<INT>((a6 >> 8) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 8) & 0xFF)) >> 1) <= 255) {
        nGreen = static_cast<BYTE>(((a6 >> 8) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 8) & 0xFF)) >> 1));
    }

    BYTE nBlue = 255;
    if (static_cast<INT>((a6 >> 16) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 16) & 0xFF)) >> 1) <= 255) {
        nBlue = static_cast<BYTE>(((a6 >> 16) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 16) & 0xFF)) >> 1));
    }

    nRed = nRed > nBase ? static_cast<BYTE>((nRed - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;
    nGreen = nGreen > nBase ? static_cast<BYTE>((nGreen - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;
    nBlue = nBlue > nBase ? static_cast<BYTE>((nBlue - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;

    if (nRed == 0 && nGreen == 0 && nBlue == 0) {
        // NOTE: The original leaks the pixel list here (0x7C90A9); ours frees
        // it.
        delete[] pPixelBuffer;
        return;
    }

    // Writes one pixel scaled by (nScaleRed, nScaleGreen, nScaleBlue) >> 6,
    // each channel at most 255; the byte after it is not touched.
    auto Scale = [nShift](BYTE* pPixel, BYTE nScaleRed, BYTE nScaleGreen, BYTE nScaleBlue) {
        INT r = (static_cast<INT>(pPixel[0]) * nScaleRed) >> nShift;
        if (r >= 255) {
            r = 255;
        }
        INT g = (static_cast<INT>(pPixel[1]) * nScaleGreen) >> nShift;
        if (g >= 255) {
            g = 255;
        }
        INT b = (static_cast<INT>(pPixel[2]) * nScaleBlue) >> nShift;
        if (b >= 255) {
            b = 255;
        }
        pPixel[0] = static_cast<BYTE>(r);
        pPixel[1] = static_cast<BYTE>(g);
        pPixel[2] = static_cast<BYTE>(b);
    };

    // The upper and lower caps: rows from the top (and the bottom) toward
    // the centre, each a ramp up and down that widens by a pixel to the
    // left as the arc steps.
    LONG nArc = GetEllipseArcPixelList(a5.cx, a5.cy, pPixelList);
    BYTE* pRow = pSurface + (a4.y - nHeight) * lPitch + a4.x * 3;
    BYTE nStepRed = static_cast<BYTE>(nRed / nHeight);
    BYTE nStepGreen = static_cast<BYTE>(nGreen / nHeight);
    BYTE nStepBlue = static_cast<BYTE>(nBlue / nHeight);

    for (SHORT nHalf = 0; nHalf < 2; nHalf++) {
        BYTE nRowRed = nStepRed;
        BYTE nRowGreen = nStepGreen;
        BYTE nRowBlue = nStepBlue;

        for (INT nWidth = 1; nWidth - 1 < nArc; nWidth++) {
            if (pPixelList[nWidth - 1] != 0) {
                BYTE* pPixel = pRow;
                BYTE nDeltaRed = static_cast<BYTE>(nRowRed / nWidth);
                BYTE nDeltaGreen = static_cast<BYTE>(nRowGreen / nWidth);
                BYTE nDeltaBlue = static_cast<BYTE>(nRowBlue / nWidth);
                BYTE nScaleRed = nBase;
                BYTE nScaleGreen = nBase;
                BYTE nScaleBlue = nBase;

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed += nDeltaRed;
                    nScaleGreen += nDeltaGreen;
                    nScaleBlue += nDeltaBlue;
                    Scale(pPixel, nScaleRed, nScaleGreen, nScaleBlue);
                    pPixel += 3;
                }

                for (SHORT x = 0; x < nWidth - 1; x++) {
                    nScaleRed -= nDeltaRed;
                    nScaleGreen -= nDeltaGreen;
                    nScaleBlue -= nDeltaBlue;
                    Scale(pPixel, nScaleRed, nScaleGreen, nScaleBlue);
                    pPixel += 3;
                }

                pRow += lPitch;
                nRowRed += nStepRed;
                nRowGreen += nStepGreen;
                nRowBlue += nStepBlue;
            }
            pRow -= 3;
        }

        if (nHalf == 0) {
            pRow = pSurface + (a4.y + nHeight) * lPitch + a4.x * 3;
        }

        lPitch = -lPitch;
    }

    // The middle band: rows from the centre outward, a ramp up across the
    // full width and down across the full width again, narrowing as the arc
    // steps. (The 32-bit sibling ramps down only (nWidth - 1) >> 1 pixels
    // here; the original's 24-bit code draws nWidth down steps, the last one
    // peeled out of the loop - 0x7C96F8, 0x7C97E0. Kept as it is.)
    nArc = GetEllipseArcPixelList(a5.cy, a5.cx, pPixelList);
    LONG nWidth = a5.cx;
    pRow = pSurface + a4.y * lPitch - nWidth * 3 + a4.x * 3;

    for (SHORT nHalf = 0; nHalf < 2; nHalf++) {
        BYTE nRowRed = nRed;
        BYTE nRowGreen = nGreen;
        BYTE nRowBlue = nBlue;

        if (nArc > 0) {
            for (INT nIndex = 0; nIndex < nArc; nIndex++) {
                // NOTE: The original divides by zero here once the band has
                // no width left (0x7C9612); ours stops drawing.
                if (nWidth == 0) {
                    delete[] pPixelBuffer;
                    return;
                }

                BYTE* pPixel = pRow;
                BYTE nDeltaRed = static_cast<BYTE>(nRowRed / nWidth);
                BYTE nDeltaGreen = static_cast<BYTE>(nRowGreen / nWidth);
                BYTE nDeltaBlue = static_cast<BYTE>(nRowBlue / nWidth);
                BYTE nScaleRed = nBase;
                BYTE nScaleGreen = nBase;
                BYTE nScaleBlue = nBase;

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed += nDeltaRed;
                    nScaleGreen += nDeltaGreen;
                    nScaleBlue += nDeltaBlue;
                    Scale(pPixel, nScaleRed, nScaleGreen, nScaleBlue);
                    pPixel += 3;
                }

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed -= nDeltaRed;
                    nScaleGreen -= nDeltaGreen;
                    nScaleBlue -= nDeltaBlue;
                    Scale(pPixel, nScaleRed, nScaleGreen, nScaleBlue);
                    pPixel += 3;
                }

                if (pPixelList[nIndex] != 0) {
                    pRow += 3;
                    nWidth--;
                }

                pRow -= lPitch;
                nRowRed -= nStepRed;
                nRowGreen -= nStepGreen;
                nRowBlue -= nStepBlue;
            }
        }

        if (nHalf == 0) {
            // The lower half starts a row below the centre, without the
            // first arc step.
            pRow = pSurface + (a4.y + 1) * lPitch - a5.cx * 3 + a4.x * 3 + pPixelList[0] * 3;
            nWidth = a5.cx - pPixelList[0];
            nArc--;
            pPixelList++;
            lPitch = -lPitch;
        }
    }

    delete[] pPixelBuffer;
}

// 0x7C9990
LONG CVidMode::DrawEllipseHOctant24(LONG x, LONG y, INT nArcLength, BYTE* pSurface, LONG lPitch, BOOLEAN clockwise, unsigned char* pArcData, const CRect* rClip, DWORD color)
{
    INT nStep;

    if (rClip != NULL) {
        if (lPitch < 0) {
            nStep = -1;
            pSurface += (x - rClip->left) * 3 - (y - rClip->top) * lPitch;
        } else {
            nStep = 1;
            pSurface += (x - rClip->left) * 3 + (y - rClip->top) * lPitch;
        }

        while (nArcLength != 0) {
            if (rClip->PtInRect(CPoint(x, y))) {
                *reinterpret_cast<WORD*>(pSurface) = static_cast<WORD>(color);
                pSurface[2] = static_cast<BYTE>(color >> 16);
            }

            if (*pArcData++ != 0) {
                pSurface += lPitch;
                y += nStep;
            }

            if (clockwise) {
                pSurface += 3;
                x++;
            } else {
                pSurface -= 3;
                x--;
            }

            nArcLength--;
        }

        return y;
    }

    if (lPitch < 0) {
        pSurface += x * 3 - y * lPitch;
        nStep = -1;
    } else {
        pSurface += y * lPitch + x * 3;
        nStep = 1;
    }

    while (nArcLength != 0) {
        *reinterpret_cast<WORD*>(pSurface) = static_cast<WORD>(color);
        pSurface[2] = static_cast<BYTE>(color >> 16);

        if (*pArcData != 0) {
            pSurface += lPitch;
            y += nStep;
        }

        if (clockwise) {
            pSurface += 3;
        } else {
            pSurface -= 3;
        }

        nArcLength--;
        pArcData++;
    }

    return y;
}

// 0x7C9B10
void CVidMode::DrawRecticle24(const CVIDMODE_RECTICLE_DESCRIPTION& rd, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    // As DrawRecticle32 (0x7CB670) with the 24-bit octants and line, the
    // pitch in bytes.
    DWORD color = (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift) | (GetRValue(rgbColor) << m_dwRBitShift);

    Recticle(this, rd, reinterpret_cast<BYTE*>(ddsd.lpSurface), ddsd.lPitch, rClip,
        [this, color](LONG x, LONG y, INT n, BYTE* p, LONG lPitch, BOOLEAN clockwise, BYTE* pArc, const CRect* pClip) {
            return DrawEllipseHOctant24(x, y, n, p, lPitch, clockwise, pArc, pClip, color);
        },
        [this, color](LONG x, LONG y, INT n, BYTE* p, LONG lPitch, BOOLEAN clockwise, BYTE* pArc, const CRect* pClip) {
            return DrawEllipseVOctant24(x, y, n, p, lPitch, clockwise, pArc, pClip, color);
        },
        [this, color](INT x1, INT y1, INT x2, INT y2, BYTE* p, LONG lPitch, const CRect& r, BOOLEAN bClipped) {
            DrawLine24(x1, y1, x2, y2, p, lPitch, r, color, bClipped);
        });
}

// 0x7CA0D0
BOOL CVidMode::DrawLine24(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, BYTE* pSurface, LONG lPitch, const CRect& rSurface, DWORD color, BOOLEAN bClipped)
{
    INT nDeltaX;
    INT nDeltaY;
    INT nStep = 3;

    if (!bClipped) {
        if (!ClipLine(nXFrom, nYFrom, nXTo, nYTo, rSurface)) {
            return FALSE;
        }
    }

    if (nYTo < nYFrom) {
        INT t;

        t = nXTo;
        nXTo = nXFrom;
        nXFrom = t;

        t = nYTo;
        nYTo = nYFrom;
        nYFrom = t;
    }

    nDeltaX = nXTo - nXFrom;
    if (nDeltaX < 0) {
        nStep = -3;
        nDeltaX = nXFrom - nXTo;
    }

    nDeltaY = nYTo - nYFrom;

    // lPitch is in bytes; each pixel is the colour's low word and its third
    // byte.
    pSurface += (nYFrom - rSurface.top) * lPitch + (nXFrom - rSurface.left) * 3;

    if (nDeltaX == 0) {
        while (nDeltaY >= 0) {
            PutPixel24(pSurface, color);
            pSurface += lPitch;
            nDeltaY--;
        }
    } else if (nDeltaY == 0) {
        while (nDeltaX >= 0) {
            PutPixel24(pSurface, color);
            pSurface += nStep;
            nDeltaX--;
        }
    } else if (nDeltaX == nDeltaY) {
        while (nDeltaX >= 0) {
            PutPixel24(pSurface, color);
            pSurface += nStep + lPitch;
            nDeltaX--;
        }
    } else if (nDeltaX < nDeltaY) {
        ScanLine24(pSurface, nDeltaY, nDeltaX, lPitch, nStep, color);
    } else {
        ScanLine24(pSurface, nDeltaX, nDeltaY, nStep, lPitch, color);
    }

    return TRUE;
}

// The 24-bit run-sliced line: ScanLine16 (0x79A860) on 3-byte pixels, the
// adjustments in bytes.
//
// 0x7CA240
void CVidMode::ScanLine24(BYTE* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, DWORD color)
{
    UINT nWholeStep = nMajorAxis / nMinorAxis;
    INT nAdjUp = (nMajorAxis % nMinorAxis) * 2;
    INT nErrorTerm = (nAdjUp >> 1) - nMinorAxis * 2;
    UINT nFinalRun = (nWholeStep >> 1) + 1;
    UINT nInitialRun = nFinalRun;
    UINT nRun;

    if (nAdjUp == 0 && (nWholeStep & 1) == 0) {
        nInitialRun = nWholeStep >> 1;
    }

    if ((nWholeStep & 1) != 0) {
        nErrorTerm += nMinorAxis;
    }

    for (nRun = nInitialRun; nRun != 0; nRun--) {
        PutPixel24(pSurface, color);
        pSurface += nMajorAxisAdjust;
    }

    UINT nRows = nMinorAxis;
    while (true) {
        nRows--;
        pSurface += nMinorAxisAdjust;
        if (nRows == 0) {
            break;
        }

        nErrorTerm += nAdjUp;
        nRun = nWholeStep;
        if (nErrorTerm > 0) {
            nRun = nWholeStep + 1;
            nErrorTerm -= nMinorAxis * 2;
        }

        for (; nRun != 0; nRun--) {
            PutPixel24(pSurface, color);
            pSurface += nMajorAxisAdjust;
        }
    }

    for (nRun = nFinalRun; nRun != 0; nRun--) {
        PutPixel24(pSurface, color);
        pSurface += nMajorAxisAdjust;
    }
}

// 0x7CA330
BOOL CVidMode::DrawThickLine24(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, BYTE* pSurface, LONG lPitch, CRect& rSurface, int a8, DWORD color, BOOLEAN bClipped)
{
    // As DrawThickLine32 (0x7CBE70) on 3-byte pixels, lPitch in bytes.
    return ThickLine(nXFrom, nYFrom, nXTo, nYTo, pSurface, lPitch, rSurface, a8, (a8 + 1) >> 1, 3, bClipped,
        [color](BYTE* p) { PutPixel24(p, color); },
        [this, color](BYTE* p, UINT nMajor, UINT nMinor, INT nMajorAdjust, INT nMinorAdjust, int nThickness) {
            sub_7CA630(p, nMajor, nMinor, nMajorAdjust, nMinorAdjust, nThickness, color);
        });
}

// The 24-bit thick-line slices of DrawThickLine24: sub_7CC130 on 3-byte
// pixels, the adjustments in bytes.
//
// 0x7CA630
void CVidMode::sub_7CA630(BYTE* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, UINT nThickness, DWORD color)
{
    ThickLineSlices(pSurface, nMajorAxis, nMinorAxis, nMajorAxisAdjust, nMinorAxisAdjust, nThickness,
        [color](BYTE* p) { PutPixel24(p, color); });
}

// 0x7CA840
LONG CVidMode::DrawEllipse32(const CPoint& ptCenter, const CSize& axis, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    BYTE* pPixelList = new BYTE[max(axis.cx, axis.cy)];
    LONG lPitch = ddsd.lPitch / 4;
    DWORD color = (GetRValue(rgbColor) << m_dwRBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift);
    LONG nArcLength = GetEllipseArcPixelList(axis.cx, axis.cy, pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo24.cpp
    // __LINE__: 1474
    UTIL_ASSERT(nArcLength <= max(axis.cx, axis.cy));

    DrawEllipseHOctant32(ptCenter.x,
        ptCenter.y - axis.cy,
        nArcLength,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseHOctant32(ptCenter.x + 1,
        ptCenter.y + pPixelList[0] - axis.cy,
        nArcLength - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawEllipseHOctant32(ptCenter.x,
        ptCenter.y + axis.cy,
        nArcLength,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseHOctant32(ptCenter.x + 1,
        ptCenter.y - pPixelList[0] + axis.cy,
        nArcLength - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    nArcLength = GetEllipseArcPixelList(axis.cy, axis.cx, pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo24.cpp
    // __LINE__: 1497
    UTIL_ASSERT(nArcLength < max(axis.cx, axis.cy));

    DrawEllipseVOctant32(ptCenter.x - axis.cx,
        ptCenter.y,
        nArcLength,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        TRUE,
        pPixelList,
        rClip,
        color);

    DrawEllipseVOctant32(ptCenter.x + pPixelList[0] - axis.cx,
        ptCenter.y + 1,
        nArcLength - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawEllipseVOctant32(ptCenter.x + axis.cx,
        ptCenter.y,
        nArcLength,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawEllipseVOctant32(ptCenter.x - pPixelList[0] + axis.cx,
        ptCenter.y + 1,
        nArcLength - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        FALSE,
        pPixelList + 1,
        rClip,
        color);

    delete[] pPixelList;

    return 0;
}

// 0x7CAB20
LONG CVidMode::DrawEllipseVOctant32(LONG x, LONG y, INT nArcLength, DWORD* pSurface, LONG lPitch, BOOLEAN clockwise, BYTE* pPixelList, const CRect* rClip, DWORD color)
{
    INT nStep;

    if (rClip != NULL) {
        if (lPitch < 0) {
            pSurface += -lPitch * (y - rClip->top) - rClip->left + x;
            nStep = -1;
        } else {
            pSurface += lPitch * (y - rClip->top) - rClip->left + x;
            nStep = 1;
        }

        while (nArcLength != 0) {
            if (rClip->PtInRect({ x, y })) {
                *pSurface = color;
            }

            if (*pPixelList++ != 0) {
                if (clockwise) {
                    pSurface++;
                    x++;
                } else {
                    pSurface--;
                    x--;
                }
            }

            pSurface += lPitch;
            y += nStep;

            nArcLength--;
        }
    } else {
        if (lPitch < 0) {
            pSurface += -lPitch * y + x;
        } else {
            pSurface += lPitch * y + x;
        }

        while (nArcLength != 0) {
            *pSurface = color;

            if (*pPixelList++ != 0) {
                if (clockwise) {
                    pSurface++;
                    x++;
                } else {
                    pSurface--;
                    x--;
                }
            }

            pSurface += lPitch;
            nArcLength--;
        }
    }

    return x;
}

// A light pool: an ellipse of half-axes a5 centred on a4 whose pixels are
// scaled by a factor rising from the edge toward the centre. a6 is the
// light's colour, a7 the ambient colour (tinted by the global tint).
//
// 0x7CAC60
void CVidMode::DrawLightingEffect32(DWORD* pSurface, LONG lPitch, const CPoint& a4, const CSize& a5, unsigned long a6, unsigned long a7)
{
    // The scale starts at 64 and a pixel is scaled by it >> 6 (.rdata
    // 0x85DE38, 0x85DE3A).
    const BYTE nBase = 64;
    const BYTE nShift = 6;

    LONG nHeight = a5.cy;
    if (nHeight == 0) {
        // NOTE: The original allocates the pixel list first and leaks it
        // here (0x7CAC8E); ours allocates after this test.
        return;
    }

    BYTE* pPixelBuffer = new BYTE[a5.cx > a5.cy ? a5.cx : a5.cy];
    BYTE* pPixelList = pPixelBuffer;

    DWORD rgbAmbient;
    if (rgbGlobalTint != RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT)) {
        rgbAmbient = (((((rgbGlobalTint >> 16) & 0xFF) * ((a7 >> 16) & 0xFF)) >> 8) & 0xFF) << 16
            | ((((a7 >> 8) & 0xFF) * ((rgbGlobalTint >> 8) & 0xFF)) & 0xFF00)
            | ((((a7 & 0xFF) * (rgbGlobalTint & 0xFF)) >> 8) & 0xFF);
    } else {
        rgbAmbient = a7;
    }

    // Each channel brightens to the light's colour plus half the ambient's
    // shortfall, rounded down to a multiple of the height above the base.
    BYTE nRed = 255;
    if (static_cast<INT>(a6 & 0xFF) + ((255 - static_cast<INT>(rgbAmbient & 0xFF)) >> 1) <= 255) {
        nRed = static_cast<BYTE>((a6 & 0xFF) + ((255 - static_cast<INT>(rgbAmbient & 0xFF)) >> 1));
    }

    BYTE nGreen = 255;
    if (static_cast<INT>((a6 >> 8) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 8) & 0xFF)) >> 1) <= 255) {
        nGreen = static_cast<BYTE>(((a6 >> 8) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 8) & 0xFF)) >> 1));
    }

    BYTE nBlue = 255;
    if (static_cast<INT>((a6 >> 16) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 16) & 0xFF)) >> 1) <= 255) {
        nBlue = static_cast<BYTE>(((a6 >> 16) & 0xFF) + ((255 - static_cast<INT>((rgbAmbient >> 16) & 0xFF)) >> 1));
    }

    nRed = nRed > nBase ? static_cast<BYTE>((nRed - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;
    nGreen = nGreen > nBase ? static_cast<BYTE>((nGreen - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;
    nBlue = nBlue > nBase ? static_cast<BYTE>((nBlue - nBase) / nHeight * static_cast<BYTE>(nHeight)) : 0;

    if (nRed == 0 && nGreen == 0 && nBlue == 0) {
        // NOTE: The original leaks the pixel list here (0x7CAE2C); ours
        // frees it.
        delete[] pPixelBuffer;
        return;
    }

    // Writes one pixel scaled by (nScaleRed, nScaleGreen, nScaleBlue) >> 6,
    // each channel at most 255; the top byte ends up clear.
    auto Scale = [nShift](DWORD* pPixel, BYTE nScaleRed, BYTE nScaleGreen, BYTE nScaleBlue) {
        DWORD nPixel = *pPixel;
        INT r = static_cast<INT>((nPixel & 0xFF) * nScaleRed) >> nShift;
        if (r >= 255) {
            r = 255;
        }
        INT g = static_cast<INT>(((nPixel >> 8) & 0xFF) * nScaleGreen) >> nShift;
        if (g >= 255) {
            g = 255;
        }
        INT b = static_cast<INT>(((nPixel >> 16) & 0xFF) * nScaleBlue) >> nShift;
        if (b >= 255) {
            b = 255;
        }
        *pPixel = ((static_cast<DWORD>(b) << 8 | static_cast<DWORD>(g)) << 8) | static_cast<DWORD>(r);
    };

    // The upper and lower caps: rows from the top (and the bottom) toward
    // the centre, each a ramp up and down that widens by a pixel to the
    // left as the arc steps.
    LONG nArc = GetEllipseArcPixelList(a5.cx, a5.cy, pPixelList);
    DWORD* pRow = pSurface + (a4.y - nHeight) * lPitch + a4.x;
    BYTE nStepRed = static_cast<BYTE>(nRed / nHeight);
    BYTE nStepGreen = static_cast<BYTE>(nGreen / nHeight);
    BYTE nStepBlue = static_cast<BYTE>(nBlue / nHeight);

    for (SHORT nHalf = 0; nHalf < 2; nHalf++) {
        BYTE nRowRed = nStepRed;
        BYTE nRowGreen = nStepGreen;
        BYTE nRowBlue = nStepBlue;

        for (INT nWidth = 1; nWidth - 1 < nArc; nWidth++) {
            if (pPixelList[nWidth - 1] != 0) {
                DWORD* pPixel = pRow;
                BYTE nDeltaRed = static_cast<BYTE>(nRowRed / nWidth);
                BYTE nDeltaGreen = static_cast<BYTE>(nRowGreen / nWidth);
                BYTE nDeltaBlue = static_cast<BYTE>(nRowBlue / nWidth);
                BYTE nScaleRed = nBase;
                BYTE nScaleGreen = nBase;
                BYTE nScaleBlue = nBase;

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed += nDeltaRed;
                    nScaleGreen += nDeltaGreen;
                    nScaleBlue += nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                for (SHORT x = 0; x < nWidth - 1; x++) {
                    nScaleRed -= nDeltaRed;
                    nScaleGreen -= nDeltaGreen;
                    nScaleBlue -= nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                pRow += lPitch;
                nRowRed += nStepRed;
                nRowGreen += nStepGreen;
                nRowBlue += nStepBlue;
            }
            pRow--;
        }

        if (nHalf == 0) {
            pRow = pSurface + (a4.y + nHeight) * lPitch + a4.x;
        }

        lPitch = -lPitch;
    }

    // The middle band: rows from the centre outward, a ramp up across the
    // full width and down across half of it, narrowing as the arc steps.
    nArc = GetEllipseArcPixelList(a5.cy, a5.cx, pPixelList);
    LONG nWidth = a5.cx;
    pRow = pSurface + a4.y * lPitch - nWidth + a4.x;

    for (SHORT nHalf = 0; nHalf < 2; nHalf++) {
        BYTE nRowRed = nRed;
        BYTE nRowGreen = nGreen;
        BYTE nRowBlue = nBlue;

        if (nArc > 0) {
            LONG nFalling = nWidth - 1;
            for (INT nIndex = 0; nIndex < nArc; nIndex++) {
                // NOTE: The original divides by zero here once the band has
                // no width left (0x7CB20C); ours stops drawing.
                if (nWidth == 0) {
                    delete[] pPixelBuffer;
                    return;
                }

                DWORD* pPixel = pRow;
                BYTE nDeltaRed = static_cast<BYTE>(nRowRed / nWidth);
                BYTE nDeltaGreen = static_cast<BYTE>(nRowGreen / nWidth);
                BYTE nDeltaBlue = static_cast<BYTE>(nRowBlue / nWidth);
                BYTE nScaleRed = nBase;
                BYTE nScaleGreen = nBase;
                BYTE nScaleBlue = nBase;

                for (SHORT x = 0; x < nWidth; x++) {
                    nScaleRed += nDeltaRed;
                    nScaleGreen += nDeltaGreen;
                    nScaleBlue += nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                LONG nDown = nFalling >> 1;
                for (SHORT x = 0; x < nDown; x++) {
                    nScaleRed -= nDeltaRed;
                    nScaleGreen -= nDeltaGreen;
                    nScaleBlue -= nDeltaBlue;
                    Scale(pPixel++, nScaleRed, nScaleGreen, nScaleBlue);
                }

                if (pPixelList[nIndex] != 0) {
                    pRow++;
                    nWidth--;
                    nFalling--;
                }

                pRow -= lPitch;
                nRowRed -= nStepRed;
                nRowGreen -= nStepGreen;
                nRowBlue -= nStepBlue;
            }
        }

        if (nHalf == 0) {
            // The lower half starts a row below the centre, without the
            // first arc step.
            pRow = pSurface + (a4.y + 1) * lPitch - a5.cx + a4.x + pPixelList[0];
            nWidth = a5.cx - pPixelList[0];
            nArc--;
            pPixelList++;
            lPitch = -lPitch;
        }
    }

    delete[] pPixelBuffer;
}

// 0x7CB510
LONG CVidMode::DrawEllipseHOctant32(LONG x, LONG y, INT nArcLength, DWORD* pSurface, LONG lPitch, BOOLEAN clockwise, BYTE* pPixelList, const CRect* rClip, DWORD color)
{
    INT nStep;

    if (rClip != NULL) {
        if (lPitch < 0) {
            pSurface += -lPitch * (y - rClip->top) - rClip->left + x;
            nStep = -1;
        } else {
            pSurface += lPitch * (y - rClip->top) - rClip->left + x;
            nStep = 1;
        }

        while (nArcLength != 0) {
            if (rClip->PtInRect({ x, y })) {
                *pSurface = color;
            }

            if (*pPixelList++ != 0) {
                pSurface += lPitch;
                y += nStep;
            }

            if (clockwise) {
                pSurface++;
                x++;
            } else {
                pSurface--;
                x--;
            }

            nArcLength--;
        }
    } else {
        if (lPitch < 0) {
            pSurface += -lPitch * y + x;
            nStep = -1;
        } else {
            pSurface += lPitch * y + x;
            nStep = 1;
        }

        while (nArcLength != 0) {
            *pSurface = color;

            if (*pPixelList++ != 0) {
                pSurface += lPitch;
                y += nStep;
            }

            if (clockwise) {
                pSurface++;
            } else {
                pSurface--;
            }

            nArcLength--;
        }
    }

    return y;
}

// 0x7CB670
void CVidMode::DrawRecticle32(const CVIDMODE_RECTICLE_DESCRIPTION& recticleDesc, const DDSURFACEDESC& ddsd, const CRect* rClip, COLORREF rgbColor)
{
    BYTE* pPixelList = new BYTE[max(recticleDesc.xAxis, recticleDesc.yAxis)];
    LONG lPitch = ddsd.lPitch / 4;
    DWORD color = (GetRValue(rgbColor) << m_dwRBitShift) | (GetGValue(rgbColor) << m_dwGBitShift) | (GetBValue(rgbColor) << m_dwBBitShift);

    INT v1 = recticleDesc.xAxis + recticleDesc.piePieceXOffset;
    INT v2 = recticleDesc.yAxis + recticleDesc.piePieceYOffset;
    INT v3;

    CRect rLineClip;
    BOOLEAN bLineClipped;

    // The lines are clipped to the clip less its last row and column when
    // there is one, and drawn unclipped when there is none (sete at
    // 0x7CB7D3: bClipped is TRUE when there is no clip).
    if (rClip != NULL) {
        rLineClip.SetRect(rClip->left, rClip->top, rClip->right - 1, rClip->bottom - 1);
        bLineClipped = FALSE;
    } else {
        rLineClip.SetRect(0, 0, 0, 0);
        bLineClipped = TRUE;
    }

    LONG nArcLength = GetEllipseArcPixelList(recticleDesc.xAxis,
        recticleDesc.yAxis,
        pPixelList);

    nArcLength -= 1;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo24.cpp
    // __LINE__: 2097
    UTIL_ASSERT(nArcLength < max(recticleDesc.xAxis, recticleDesc.yAxis));

    v3 = DrawEllipseHOctant32(recticleDesc.ptCenter.x,
        recticleDesc.ptCenter.y - v2,
        nArcLength - recticleDesc.xGap,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawLine32(recticleDesc.ptCenter.x + recticleDesc.xGap - nArcLength,
        v3,
        recticleDesc.ptCenter.x,
        recticleDesc.ptCenter.y + recticleDesc.piePiecePtYOffset - v2,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    v3 = DrawEllipseHOctant32(recticleDesc.ptCenter.x + 1,
        recticleDesc.ptCenter.y + pPixelList[0] - v2,
        nArcLength - recticleDesc.xGap - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawLine32(recticleDesc.ptCenter.x - recticleDesc.xGap + nArcLength,
        v3,
        recticleDesc.ptCenter.x,
        recticleDesc.ptCenter.y + recticleDesc.piePiecePtYOffset - v2,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    v3 = DrawEllipseHOctant32(recticleDesc.ptCenter.x,
        recticleDesc.ptCenter.y + v2,
        nArcLength - recticleDesc.xGap,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawLine32(recticleDesc.ptCenter.x + recticleDesc.xGap - nArcLength,
        v3,
        recticleDesc.ptCenter.x,
        recticleDesc.ptCenter.y - recticleDesc.piePiecePtYOffset + v2,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    v3 = DrawEllipseHOctant32(recticleDesc.ptCenter.x + 1,
        recticleDesc.ptCenter.y - pPixelList[0] + v2,
        nArcLength - recticleDesc.xGap - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawLine32(recticleDesc.ptCenter.x - recticleDesc.xGap + nArcLength,
        v3,
        recticleDesc.ptCenter.x,
        recticleDesc.ptCenter.y - recticleDesc.piePiecePtYOffset + v2,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    nArcLength = GetEllipseArcPixelList(recticleDesc.yAxis,
        recticleDesc.xAxis,
        pPixelList);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVideo24.cpp
    // __LINE__: 2129
    UTIL_ASSERT(nArcLength < max(recticleDesc.xAxis, recticleDesc.yAxis));

    v3 = DrawEllipseVOctant32(recticleDesc.ptCenter.x - v1,
        recticleDesc.ptCenter.y,
        nArcLength - recticleDesc.yGap,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        TRUE,
        pPixelList,
        rClip,
        color);

    DrawLine32(v3,
        recticleDesc.ptCenter.y + recticleDesc.yGap - nArcLength,
        recticleDesc.ptCenter.x + recticleDesc.piePiecePtXOffset - v1,
        recticleDesc.ptCenter.y,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    v3 = DrawEllipseVOctant32(recticleDesc.ptCenter.x + pPixelList[0] - v1,
        recticleDesc.ptCenter.y + 1,
        nArcLength - recticleDesc.yGap - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        TRUE,
        pPixelList + 1,
        rClip,
        color);

    DrawLine32(v3,
        recticleDesc.ptCenter.y - recticleDesc.yGap + nArcLength,
        recticleDesc.ptCenter.x + recticleDesc.piePiecePtXOffset - v1,
        recticleDesc.ptCenter.y,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    v3 = DrawEllipseVOctant32(recticleDesc.ptCenter.x + v1,
        recticleDesc.ptCenter.y,
        nArcLength - recticleDesc.yGap,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        -lPitch,
        FALSE,
        pPixelList,
        rClip,
        color);

    DrawLine32(v3,
        recticleDesc.ptCenter.y + recticleDesc.yGap - nArcLength,
        recticleDesc.ptCenter.x - recticleDesc.piePiecePtXOffset + v1,
        recticleDesc.ptCenter.y,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    v3 = DrawEllipseVOctant32(recticleDesc.ptCenter.x - pPixelList[0] + v1,
        recticleDesc.ptCenter.y + 1,
        nArcLength - recticleDesc.yGap - 1,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        FALSE,
        pPixelList + 1,
        rClip,
        color);

    DrawLine32(v3,
        recticleDesc.ptCenter.y - recticleDesc.yGap + nArcLength,
        recticleDesc.ptCenter.x - recticleDesc.piePiecePtXOffset + v1,
        recticleDesc.ptCenter.y,
        reinterpret_cast<DWORD*>(ddsd.lpSurface),
        lPitch,
        rLineClip,
        color,
        bLineClipped);

    delete[] pPixelList;
}

// 0x7CBC40
BOOL CVidMode::DrawLine32(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, DWORD* pSurface, LONG lPitch, const CRect& rSurface, DWORD color, BOOLEAN bClipped)
{
    INT nDeltaX;
    INT nDeltaY;
    INT nStep;

    if (!bClipped) {
        if (!ClipLine(nXFrom, nYFrom, nXTo, nYTo, rSurface)) {
            return FALSE;
        }
    }

    if (nYFrom > nYTo) {
        INT t;

        t = nXTo;
        nXTo = nXFrom;
        nXFrom = t;

        t = nYTo;
        nYTo = nYFrom;
        nYFrom = t;
    }

    if (nXTo - nXFrom < 0) {
        nDeltaX = nXFrom - nXTo;
        nStep = -1;
    } else {
        nDeltaX = nXTo - nXFrom;
        nStep = 1;
    }

    nDeltaY = nYTo - nYFrom;

    pSurface += lPitch * (nYFrom - rSurface.top) - rSurface.left + nXFrom;

    if (nDeltaX == 0) {
        while (nDeltaY >= 0) {
            *pSurface = color;
            pSurface += lPitch;
            nDeltaY--;
        }
        return TRUE;
    }

    if (nDeltaY == 0) {
        while (nDeltaX >= 0) {
            *pSurface = color;
            pSurface += nStep;
            nDeltaX--;
        }
        return TRUE;
    }

    if (nDeltaX == nDeltaY) {
        while (nDeltaX >= 0) {
            *pSurface = color;
            pSurface += lPitch + nStep;
            nDeltaX--;
        }
        return TRUE;
    }

    if (nDeltaX < nDeltaY) {
        ScanLine32(pSurface, nDeltaY, nDeltaX, lPitch, nStep, color);
    } else {
        ScanLine32(pSurface, nDeltaX, nDeltaY, nStep, lPitch, color);
    }

    return TRUE;
}

// 0x7CBD80
void CVidMode::ScanLine32(DWORD* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, DWORD color)
{
    int v1 = nMajorAxis / nMinorAxis;
    int v2 = 2 * nMinorAxis;
    int v3 = 2 * (nMajorAxis % nMinorAxis);
    int v4 = v3 / 2 - v2;
    int v5 = v1 / 2 + 1;
    int v6 = nMinorAxis - 1;
    int v7 = v1 / 2 + 1;

    if (v3 != 0) {
        if ((v1 & 1) != 0) {
            v4 += nMinorAxis;
        }
    } else {
        if ((v1 & 1) != 0) {
            v4 += nMinorAxis;
        } else {
            v5 -= 1;
        }
    }

    while (v5 > 0) {
        *pSurface = color;
        pSurface += nMajorAxisAdjust;
        v5--;
    }

    pSurface += nMinorAxisAdjust;
    while (v6 > 0) {
        v4 += v3;

        int v8 = v1;
        if (v4 > 0) {
            v8 += 1;
            v4 -= v2;
        }

        while (v8 > 0) {
            *pSurface = color;
            pSurface += nMajorAxisAdjust;
            v8--;
        }

        pSurface += nMinorAxisAdjust;

        v6--;
    }

    while (v7 > 0) {
        *pSurface = color;
        pSurface += nMajorAxisAdjust;
        v7--;
    }
}

// 0x7CBE70
BOOL CVidMode::DrawThickLine32(INT nXFrom, INT nYFrom, INT nXTo, INT nYTo, DWORD* pSurface, LONG lPitch, CRect& rSurface, int a8, DWORD color, BOOLEAN bClipped)
{
    INT nCount = a8 + 1;
    INT nHalf = nCount >> 1;
    INT nStep = 1;
    INT nDeltaX;
    INT nDeltaY;

    if (!bClipped) {
        CRect rClip;
        rClip.left = rSurface.left + nHalf;
        rClip.top = rSurface.top + nHalf;
        rClip.right = rSurface.right - nHalf;
        rClip.bottom = rSurface.bottom - nHalf;
        if (!ClipLine(nXFrom, nYFrom, nXTo, nYTo, rClip)) {
            return FALSE;
        }
    }

    if (nYFrom > nYTo) {
        INT t;

        t = nXTo;
        nXTo = nXFrom;
        nXFrom = t;

        t = nYTo;
        nYTo = nYFrom;
        nYFrom = t;
    }

    nDeltaX = nXTo - nXFrom;
    if (nDeltaX < 0) {
        nDeltaX = nXFrom - nXTo;
        nStep = -1;
    }

    nDeltaY = nYTo - nYFrom;

    // The line's first pixel moved back half the thickness across and up.
    pSurface += lPitch * (nYFrom - rSurface.top) - rSurface.left + nXFrom;
    if (nStep > 0) {
        pSurface -= nHalf;
    } else {
        pSurface += nHalf;
    }
    pSurface -= nHalf * lPitch;

    if (nDeltaX == 0) {
        // NOTE: Each column starts a row below the previous one.
        if (a8 >= 0) {
            do {
                if (nDeltaY >= 0) {
                    INT n = nDeltaY + 1;
                    do {
                        *pSurface = color;
                        pSurface += lPitch;
                        n--;
                    } while (n != 0);
                }
                pSurface += 1 - nDeltaY * lPitch;
                nCount--;
            } while (nCount != 0);
            return TRUE;
        }
    } else if (nDeltaY == 0) {
        // NOTE: Each row starts a column after the previous one.
        if (a8 >= 0) {
            do {
                if (nDeltaX >= 0) {
                    INT n = nDeltaX + 1;
                    do {
                        *pSurface = color;
                        pSurface += nStep;
                        n--;
                    } while (n != 0);
                }
                nCount--;
                pSurface += lPitch - nDeltaX * nStep;
            } while (nCount != 0);
            return TRUE;
        }
    } else if (nDeltaX == nDeltaY) {
        INT nRow = 0;
        INT nRun = a8 - 1;
        if (a8 > 0) {
            do {
                nRun += nRow < nDeltaX ? 1 : 0;
                for (INT n = nRun; n > 0; n--) {
                    *pSurface = color;
                    pSurface += nStep;
                }
                nRow++;
                pSurface += lPitch - nRun * nStep;
            } while (nRow < a8);
        }

        pSurface += nStep;

        INT nRows = nDeltaY - ((a8 & 1) != 0 ? a8 : a8 - 1);
        if (nRows > 0) {
            do {
                for (INT n = nRun; n > 0; n--) {
                    *pSurface = color;
                    pSurface += nStep;
                }
                nRows--;
                pSurface += lPitch - (nRun - 1) * nStep;
            } while (nRows != 0);
        }

        if (a8 > 0) {
            INT nAdvance = (1 - nRun) * nStep + lPitch;
            INT nLeft = a8;
            do {
                for (INT n = nRun; n > 0; n--) {
                    *pSurface = color;
                    pSurface += nStep;
                }
                pSurface += nAdvance;
                nRun--;
                nAdvance += nStep;
                nLeft--;
            } while (nLeft != 0);
            return TRUE;
        }
    } else {
        if (nDeltaY > nDeltaX) {
            sub_7CC130(pSurface, nDeltaY, nDeltaX, lPitch, nStep, a8, color);
        } else {
            sub_7CC130(pSurface, nDeltaX, nDeltaY, nStep, lPitch, a8, color);
        }
    }

    return TRUE;
}

// A run-length sliced line nThickness pixels wide (Abrash's): runs along the
// major axis, nMinorAxis + 1 slices across it.
//
// 0x7CC130
void CVidMode::sub_7CC130(DWORD* pSurface, UINT nMajorAxis, UINT nMinorAxis, INT nMajorAxisAdjust, INT nMinorAxisAdjust, UINT nThickness, DWORD color)
{
    UINT nWholeStep = nMajorAxis / nMinorAxis;
    INT nAdjUp = (nMajorAxis % nMinorAxis) * 2;
    INT nAdjDown = nMinorAxis * 2;
    INT nErrorTerm = (nAdjUp >> 1) - nAdjDown;
    UINT nFinalPixelCount = (nWholeStep >> 1) + 1;
    UINT nInitialPixelCount = nFinalPixelCount;

    if (nAdjUp == 0 && (nWholeStep & 1) == 0) {
        nInitialPixelCount--;
    }

    if ((nWholeStep & 1) != 0) {
        nErrorTerm += nMinorAxis;
    }

    // The leading edge of the slices runs nLead pixels from the start, the
    // trailing edge nTrail; the trailing edge starts moving once the first
    // nThickness - 1 slices are down.
    INT nTrailErrorTerm = nErrorTerm;
    UINT nTrail = 0;
    UINT nTrailRun = nInitialPixelCount;
    UINT nLead = (nThickness & ~1) + nInitialPixelCount;

    for (UINT n = nLead; n != 0; n--) {
        *pSurface = color;
        pSurface += nMajorAxisAdjust;
    }
    pSurface += nMinorAxisAdjust - static_cast<INT>(nLead) * nMajorAxisAdjust;

    UINT nWait = nThickness - 2;
    for (UINT nSlice = nMinorAxis - 1; nSlice != 0; nSlice--) {
        UINT nRunLength = nWholeStep;
        nErrorTerm += nAdjUp;
        if (nErrorTerm > 0) {
            nRunLength = nWholeStep + 1;
            nErrorTerm -= nAdjDown;
        }

        nLead += nRunLength;
        if (nTrail < nLead) {
            for (UINT n = nLead - nTrail; n != 0; n--) {
                *pSurface = color;
                pSurface += nMajorAxisAdjust;
            }
        }
        pSurface -= static_cast<INT>(nLead - nTrail) * nMajorAxisAdjust;

        if (nWait == 0) {
            pSurface += static_cast<INT>(nTrailRun) * nMajorAxisAdjust;
            nTrail += nTrailRun;
            nTrailRun = nWholeStep;
            nTrailErrorTerm += nAdjUp;
            if (nTrailErrorTerm > 0) {
                nTrailRun = nWholeStep + 1;
                nTrailErrorTerm -= nAdjDown;
            }
        } else {
            nWait--;
        }

        pSurface += nMinorAxisAdjust;
    }

    nLead += nFinalPixelCount;
    for (UINT nSlice = 0; nSlice < nThickness; nSlice++) {
        if (nTrail < nLead) {
            for (UINT n = nLead - nTrail; n != 0; n--) {
                *pSurface = color;
                pSurface += nMajorAxisAdjust;
            }
        }
        pSurface -= static_cast<INT>(nLead - nTrail) * nMajorAxisAdjust;

        if (nSlice == nThickness - 1) {
            nTrailRun = nFinalPixelCount;
        }

        pSurface += static_cast<INT>(nTrailRun) * nMajorAxisAdjust;
        nTrail += nTrailRun;
        nTrailRun = nWholeStep;
        nTrailErrorTerm += nAdjUp;
        if (nTrailErrorTerm > 0) {
            nTrailRun = nWholeStep + 1;
            nTrailErrorTerm -= nAdjDown;
        }

        pSurface += nMinorAxisAdjust;
    }
}

// 0x799E20
BOOL CVidMode::CreateSurfaces(BOOLEAN bFullscreen)
{
    return FALSE;
}

// 0x799C90
BOOL CVidMode::DeactivateVideoMode(CVidMode* pNextVidMode)
{
    return TRUE;
}

// 0x799E20
IDirectDrawSurface* CVidMode::GetFXSurfacePtr(DWORD dwFlags)
{
    return NULL;
}

// 0x7ABB90
RGBQUAD* CVidMode::GetPalette()
{
    return NULL;
}

// 0x78E6E0
int CVidMode::GetNumVRamSurfaces()
{
    return 0;
}

// 0x7B5450
void CVidMode::RenderFlash(UINT nSurface, COLORREF rgbColor, unsigned char a4, const CRect& rClip)
{
}

// 0x799C90
BOOL CVidMode::RenderPointer(UINT nSurface)
{
    return TRUE;
}

// 0x799C90
BOOL CVidMode::Flip(BOOL bRenderCursor)
{
    return TRUE;
}

// 0x799E60
void CVidMode::ParsePixelFormat(const DDPIXELFORMAT& ddpf)
{
}

// NOTE: Inlined.
BOOL CVidMode::GetFullyFaded()
{
    return m_nFade == 0;
}

// NOTE: Inlined.
void CVidMode::UpdateFade()
{
    if (m_bFadeTo) {
        if (m_nFade > 0) {
            m_nFade--;
        }
    } else {
        if (m_nFade < NUM_FADE_FRAMES) {
            m_nFade++;
        }
    }
    if (auto* b = render::Active()) b->Fade(m_nFade);
}

// FIXME: `rgbFade` is misleading (both type and meaning). This function is
// usually called via inlined non-static `CInfinity::SetFade`, but I'm not sure
// how `CInfinity` instance is obtained in the first place.
//
// NOTE: Inlined.
void CVidMode::SetFade(BOOL bFadeTo, COLORREF rgbFade)
{
    if (rgbFade != 0) {
        NUM_FADE_FRAMES = static_cast<BYTE>(rgbFade);
    }

    m_bFadeTo = bFadeTo;

    if (m_bFadeTo) {
        m_nFade = NUM_FADE_FRAMES;
    } else {
        m_nFade = 0;
    }
    if (auto* b = render::Active()) b->Fade(m_nFade);
}
