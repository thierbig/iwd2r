#include "CVidBitmap.h"

#include "CChitin.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CVideo3d.h"
#include "render/api.h"

// 0x7B2A40
CVidBitmap::CVidBitmap()
{
    m_nBitCount = 8;
    m_bDoubleSize = FALSE;
}

// 0x7B2DB0
CVidBitmap::~CVidBitmap()
{
}

// FIXME: Use reference.
//
// 0x7B2AD0
CVidBitmap::CVidBitmap(CResRef cNewResRef, BOOL bDoubleSize)
{
    // NOTE: Uninline.
    SetResRef(cNewResRef, TRUE, TRUE);

    m_nBitCount = 8;
    m_bDoubleSize = bDoubleSize;
}

// FIXME: Use reference.
//
// 0x7B2C40
CVidBitmap::CVidBitmap(CResRef cNewResRef, BOOL bDoubleSize, USHORT nBitCount)
{
    // NOTE: Uninline.
    SetResRef(cNewResRef, TRUE, TRUE);

    m_nBitCount = nBitCount;
    m_bDoubleSize = bDoubleSize;
}

// 0x7B2E90
SHORT CVidBitmap::GetBitCount(BOOL bDemanded)
{
    if (!bDemanded) {
        pRes->Demand();
    }

    SHORT nBitCount = pRes->GetBitDepth();

    if (!bDemanded) {
        pRes->Release();
    }

    return nBitCount;
}

// 0x7B2ED0
BOOL CVidBitmap::GetImageDimensions(CSize& size, BOOLEAN bDemanded)
{
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 7277
    UTIL_ASSERT(pRes != NULL);

    if (pRes == NULL) {
        return FALSE;
    }

    if (!bDemanded) {
        if (pRes->Demand() == NULL) {
            return FALSE;
        }
    }

    pRes->GetImageDimensions(size, m_bDoubleSize);

    if (!bDemanded) {
        pRes->Release();
    }

    return TRUE;
}

// 0x7B2F60
BOOL CVidBitmap::GetPixelColor(RGBQUAD& color, INT nX, INT nY, BOOL bLoaded)
{
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 7337
    UTIL_ASSERT(pRes != NULL);

    if (pRes == NULL) {
        return FALSE;
    }

    if (bLoaded) {
        pRes->GetPixelColor(nX, nY, color, m_bDoubleSize);
        return TRUE;
    }

    if (pRes->Demand()) {
        pRes->GetPixelColor(nX, nY, color, m_bDoubleSize);
        pRes->Release();
        return TRUE;
    }

    return FALSE;
}

// 0x7B3000
BOOL CVidBitmap::GetPixelValue(BYTE& value, INT x, INT y, BOOLEAN bDemanded)
{
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 7400
    UTIL_ASSERT((m_nBitCount == CVIDBITMAP_8BIT || m_nBitCount == CVIDBITMAP_4BIT));

    if (pRes == NULL) {
        return FALSE;
    }

    if (m_nBitCount == CVIDBITMAP_8BIT || m_nBitCount == CVIDBITMAP_4BIT) {
        if (bDemanded) {
            value = pRes->GetPixelValue(x, y, m_bDoubleSize);
            return TRUE;
        }

        if (pRes->Demand() != NULL) {
            value = pRes->GetPixelValue(x, y, m_bDoubleSize);
            pRes->Release();
            return TRUE;
        }
    }

    return FALSE;
}

// 0x7B30C0
BYTE CVidBitmap::GetPixelValue(INT x, INT y, BOOLEAN bDemanded)
{
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 7440
    UTIL_ASSERT((m_nBitCount == CVIDBITMAP_8BIT || m_nBitCount == CVIDBITMAP_4BIT));

    if (pRes == NULL) {
        return 0;
    }

    if (m_nBitCount == CVIDBITMAP_8BIT || m_nBitCount == CVIDBITMAP_4BIT) {
        if (bDemanded) {
            return pRes->GetPixelValue(x, y, m_bDoubleSize);
        }

        if (pRes->Demand() != NULL) {
            BYTE value = pRes->GetPixelValue(x, y, m_bDoubleSize);
            pRes->Release();
            return value;
        }
    }

    return 0;
}

// 0x7B3180
void CVidBitmap::SetRes(CResBitmap* pResBmp)
{
    pRes = pResBmp;
}

// 0x7B3190
BOOL CVidBitmap::RenderDirect(INT nSurface, INT x, INT y, const CRect& rClip, DWORD dwFlags, BOOLEAN bDemanded)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawBitmap(*this, { x, y }, dwFlags)) return TRUE;
    if (g_pChitin->cVideo.Is3dAccelerated()) {
        return RenderDirect3d(x, y, rClip, dwFlags, bDemanded);
    }

    if (pRes == NULL) {
        return FALSE;
    }

    if (rClip.IsRectEmpty()) {
        return FALSE;
    }

    if (!bDemanded) {
        pRes->Demand();
    }

    BYTE* pData = pRes->GetImageData(m_bDoubleSize);
    m_nBitCount = pRes->GetBitDepth();

    CSize bmpSize;
    pRes->GetImageDimensions(bmpSize, m_bDoubleSize);

    INT nBytesPerPixel;
    switch (m_nBitCount) {
    case 8:
        nBytesPerPixel = 1;
        break;
    case 24:
        nBytesPerPixel = 3;
        break;
    default:
        if (!bDemanded) {
            pRes->Release();
        }
        return FALSE;
    }

    LONG nDataJump = (-((bmpSize.cx * nBytesPerPixel) & 3)) & 3;

    CRect rSurface(CPoint(x, y), bmpSize);

    if (bmpSize.cy > rClip.Height()) {
        INT nSkipBottom = y + bmpSize.cy - rClip.bottom;
        if (nSkipBottom > 0) {
            pData += nSkipBottom * (nDataJump + bmpSize.cx * nBytesPerPixel);
            bmpSize.cy -= nSkipBottom;
            rSurface.bottom -= nSkipBottom;
        }

        INT nSkipTop = rClip.top - y;
        if (nSkipTop > 0) {
            bmpSize.cy -= nSkipTop;
            rSurface.top += nSkipTop;
        }
    }

    if (bmpSize.cx > rClip.Width()) {
        INT nSkipRight = x + bmpSize.cx - rClip.right;
        if (nSkipRight > 0) {
            nDataJump += nBytesPerPixel * nSkipRight;
            bmpSize.cx -= nSkipRight;
            rSurface.right -= nSkipRight;
        }

        INT nSkipLeft = rClip.left - x;
        if (nSkipLeft > 0) {
            nDataJump += nBytesPerPixel * nSkipLeft;
            pData += nBytesPerPixel * nSkipLeft;
            bmpSize.cx -= nSkipLeft;
            rSurface.left += nSkipLeft;
        }
    }

    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);

    BOOL bResult = FALSE;

    if (g_pChitin->GetCurrentVideoMode()->LockSurface(nSurface, &ddsd, rSurface)) {
        BYTE* pSurface = reinterpret_cast<BYTE*>(ddsd.lpSurface) + 4 * (bmpSize.cy - 1) * (ddsd.lPitch / 4);
        switch (m_nBitCount) {
        case 8:
            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                // NOTE: Uninline.
                bResult = BltBmp8To16(reinterpret_cast<WORD*>(pSurface),
                    ddsd.lPitch,
                    pData,
                    bmpSize,
                    nDataJump,
                    dwFlags);
                break;
            case 24:
                bResult = BltBmp8To24(reinterpret_cast<BYTE*>(pSurface),
                    ddsd.lPitch,
                    pData,
                    bmpSize,
                    nDataJump,
                    dwFlags);
                break;
            case 32:
                bResult = BltBmp8To32(reinterpret_cast<DWORD*>(pSurface),
                    ddsd.lPitch,
                    pData,
                    bmpSize,
                    nDataJump,
                    dwFlags);
                break;
            }
            break;
        case 24:
            // The tinting copy whenever a flag in the high word (tint, add,
            // light, ...) is set or the brightness and contrast move mid
            // grey (test of dwFlags & 0xFFFF0000 before each
            // ApplyBrightnessContrast).
            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                if ((dwFlags & 0xFFFF0000) != 0 || g_pChitin->GetCurrentVideoMode()->ApplyBrightnessContrast(RGB(128, 128, 128)) != RGB(128, 128, 128)) {
                    bResult = BltBmp24To16Tint(reinterpret_cast<WORD*>(pSurface),
                        ddsd.lPitch,
                        pData,
                        bmpSize,
                        nDataJump,
                        dwFlags);
                } else {
                    bResult = BltBmp24To16(reinterpret_cast<WORD*>(pSurface),
                        ddsd.lPitch,
                        pData,
                        bmpSize,
                        nDataJump,
                        dwFlags);
                }
                break;
            case 24:
                if ((dwFlags & 0xFFFF0000) != 0 || g_pChitin->GetCurrentVideoMode()->ApplyBrightnessContrast(RGB(128, 128, 128)) != RGB(128, 128, 128)) {
                    bResult = BltBmp24To24Tint(reinterpret_cast<BYTE*>(pSurface),
                        ddsd.lPitch,
                        pData,
                        bmpSize,
                        nDataJump,
                        dwFlags);
                } else {
                    bResult = BltBmp24To24(reinterpret_cast<BYTE*>(pSurface),
                        ddsd.lPitch,
                        pData,
                        bmpSize,
                        nDataJump,
                        dwFlags);
                }
                break;
            case 32:
                if ((dwFlags & 0xFFFF0000) != 0 || g_pChitin->GetCurrentVideoMode()->ApplyBrightnessContrast(RGB(128, 128, 128)) != RGB(128, 128, 128)) {
                    bResult = BltBmp24To32Tint(reinterpret_cast<DWORD*>(pSurface),
                        ddsd.lPitch,
                        pData,
                        bmpSize,
                        nDataJump,
                        dwFlags);
                } else {
                    bResult = BltBmp24To32(reinterpret_cast<DWORD*>(pSurface),
                        ddsd.lPitch,
                        pData,
                        bmpSize,
                        nDataJump,
                        dwFlags);
                }
                break;
            }
            break;
        }

        g_pChitin->GetCurrentVideoMode()->UnLockSurface(nSurface, ddsd.lpSurface);
    }

    if (!bDemanded) {
        pRes->Release();
    }

    return bResult;
}

// NOTE: Inlined. The original has no body of its own for this function:
// CVidBitmap::RenderDirect (0x7B3190) carries the 8-bpp to 16-bpp loop inside
// its own 16-bpp branch, the inlined arm at RenderDirect+0x49D (0x7B362D)
// through +0x5A5 (0x7B3735), so it has no address of its own to mark here:
// the address 0x7B362D is not a function entry, and a `// 0xADDR` comment
// there would fail the parity marker check. The port keeps it as a function -
// the inlining is
// the compiler's choice - and the three paths here are the inlined ones: pairs
// of pixels per dword store (second in the high word), an odd width's leading
// pixel before the pairs, and a sub-2 width's one pixel per row (a do-while
// there, so a zero-width blit still writes one pixel, as in BltBmp24To16).
// The palette goes through SetPalette/Realize exactly as BltBmp8To32 does
// (0x7B3660 and 0x7B3680 in the arm) and each pixel is the low word of the
// realized entry, `word [rgbTempPal + 4 * index]`. Its evidence is
// docs/evidence/7B362D.md, keyed by the arm's address.
//
// NOTE: The original halves the pair count of an even width with a byte shift
// (`shr byte ptr [ebp+0x14]` at 0x7B36B4) while the odd path shifts the whole
// dword (0x7B36E2). The two agree for every width below 256; at and above it
// the byte shift keeps the untouched high bytes and counts far too many
// pairs, reading past the source row and writing past the surface row. This
// halves the whole width instead - identical below 256, and the only widths
// the game blits - rather than reproduce the overrun.
BOOL CVidBitmap::BltBmp8To16(WORD* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    m_cPalette.SetPalette(pRes->GetColorTable(), pRes->GetColorCount(), CVidPalette::TYPE_RESOURCE);
    m_cPalette.Realize(rgbTempPal, 16, dwFlags | 0x800000, &m_paletteAffects, 255);

    for (LONG y = 0; y < bmpSize.cy; y++) {
        if (bmpSize.cx < 2) {
            *pSurface = static_cast<WORD>(rgbTempPal[*pData++]);
            pSurface += 1 - bmpSize.cx - lPitch / 2;
        } else {
            if ((bmpSize.cx & 1) != 0) {
                *pSurface++ = static_cast<WORD>(rgbTempPal[*pData++]);
            }

            for (LONG x = 0; x < bmpSize.cx / 2; x++) {
                WORD nFirst = static_cast<WORD>(rgbTempPal[pData[0]]);
                WORD nSecond = static_cast<WORD>(rgbTempPal[pData[1]]);
                *reinterpret_cast<DWORD*>(pSurface) = static_cast<DWORD>(nFirst)
                    | (static_cast<DWORD>(nSecond) << 16);
                pSurface += 2;
                pData += 2;
            }

            pSurface += -bmpSize.cx - lPitch / 2;
        }

        pData += nDataJump;
    }

    return TRUE;
}

// The 24-bit BMP (three bytes, B then G then R) onto a 16-bit surface,
// through the mode's channel shifts: the source shift (8 - the destination's
// bit count, field_C2/C6/CA) brings a byte down to the destination's width
// and the destination shift (m_dwRBitShift/...) places it. lPitch is in
// bytes and the rows go bottom-up, as the 32-bit sibling does.
//
// 0x7B37A0
BOOL CVidBitmap::BltBmp24To16(WORD* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    DWORD dwRSrcShift = pVidMode->field_C2;
    DWORD dwGSrcShift = pVidMode->field_C6;
    DWORD dwBSrcShift = pVidMode->field_CA;
    DWORD dwRBitShift = pVidMode->m_dwRBitShift;
    DWORD dwGBitShift = pVidMode->m_dwGBitShift;
    DWORD dwBBitShift = pVidMode->m_dwBBitShift;

    auto Convert = [&](const BYTE* p) {
        return static_cast<WORD>(((p[0] >> dwBSrcShift) << dwBBitShift)
            | ((p[1] >> dwGSrcShift) << dwGBitShift)
            | ((p[2] >> dwRSrcShift) << dwRBitShift));
    };

    if ((dwFlags & 0x1) == 0) {
        for (LONG y = 0; y < bmpSize.cy; y++) {
            if (bmpSize.cx < 2) {
                // NOTE: The original's single-pixel path (0x7B3940) runs at
                // least once, so a zero-width blit still writes one pixel and
                // consumes one source pixel per row; kept as it is.
                *pSurface = Convert(pData);
                pSurface += 1 - bmpSize.cx - lPitch / 2;
                pData += 3 + nDataJump;
            } else {
                for (LONG x = 0; x < bmpSize.cx; x++) {
                    *pSurface++ = Convert(pData);
                    pData += 3;
                }

                pSurface += -bmpSize.cx - lPitch / 2;
                pData += nDataJump;
            }
        }
    } else {
        // The keyed form the 32-bit sibling has: pure green is transparent,
        // and with 0x4 a pure black pixel is half the destination's colour
        // (the destination channel read back through its mask and shift).
        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                BYTE r = pData[2];
                BYTE g = pData[1];
                BYTE b = pData[0];

                if (r != 0 || g != 255 || b != 0) {
                    if ((dwFlags & 0x4) != 0 && r == 0 && g == 0 && b == 0) {
                        WORD nPixel = *pSurface;
                        r = static_cast<BYTE>((((nPixel & pVidMode->m_dwRBitMask) >> dwRBitShift) << dwRSrcShift) >> 1);
                        g = static_cast<BYTE>((((nPixel & pVidMode->m_dwGBitMask) >> dwGBitShift) << dwGSrcShift) >> 1);
                        b = static_cast<BYTE>((((nPixel & pVidMode->m_dwBBitMask) >> dwBBitShift) << dwBSrcShift) >> 1);
                    }

                    *pSurface = static_cast<WORD>(((b >> dwBSrcShift) << dwBBitShift)
                        | ((g >> dwGSrcShift) << dwGBitShift)
                        | ((r >> dwRSrcShift) << dwRBitShift));
                }

                pSurface++;
                pData += 3;
            }

            pSurface += -bmpSize.cx - lPitch / 2;
            pData += nDataJump;
        }
    }

    return TRUE;
}

// The tinted form of BltBmp24To16, as the 32-bit sibling BltBmp24To32Tint is:
// the same three-byte conversion, with the image's tint, add and light
// multipliers applied per channel, the high-word flags (0x80000 monochrome,
// 0x10000/0x20000 tint, 0x40000 add, 0x200000 light), the keyed (0x1) and
// shadow (0x4) handling, and the pixel packed through the mode's shifts.
//
// 0x7B3B70
BOOL CVidBitmap::BltBmp24To16Tint(WORD* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    DWORD nTintRed = 255;
    DWORD nTintGreen = 255;
    DWORD nTintBlue = 255;
    INT nTintShift = 0;

    DWORD nAddRed = 1;
    DWORD nAddGreen = 1;
    DWORD nAddBlue = 1;
    INT nAddShift = 0;

    INT nLightRed = 1 << CVidPalette::LIGHT_SCALE;
    INT nLightGreen = 1 << CVidPalette::LIGHT_SCALE;
    INT nLightBlue = 1 << CVidPalette::LIGHT_SCALE;

    BYTE nRSrcShift = static_cast<BYTE>(pVidMode->field_C2);
    BYTE nGSrcShift = static_cast<BYTE>(pVidMode->field_C6);
    BYTE nBSrcShift = static_cast<BYTE>(pVidMode->field_CA);
    BYTE nRShift = static_cast<BYTE>(pVidMode->m_dwRBitShift);
    BYTE nGShift = static_cast<BYTE>(pVidMode->m_dwGBitShift);
    BYTE nBShift = static_cast<BYTE>(pVidMode->m_dwBBitShift);

    BOOL bTint = GetTint(nTintRed, nTintGreen, nTintBlue, nTintShift, dwFlags);
    BOOL bAdd = GetAdd(nAddRed, nAddGreen, nAddBlue, nAddShift, dwFlags);
    BOOL bLight = GetLight(nLightRed, nLightGreen, nLightBlue, dwFlags);

    if (bmpSize.cy < 1) {
        return TRUE;
    }

    if ((dwFlags & 0x1) != 0) {
        COLORREF rgbBlack = pVidMode->ApplyBrightnessContrast(RGB(0, 0, 0));

        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                BYTE r = pData[2];
                BYTE g = pData[1];
                BYTE b = pData[0];

                // Pure green is the transparent colour.
                if (r != 0 || g != 255 || b != 0) {
                    if ((dwFlags & 0x4) != 0 && r == 0 && g == 0 && b == 0) {
                        // Shadow: half black over what is there, the
                        // destination channel read back through its mask and
                        // shift (and widened again by the source shift).
                        WORD nPixel = *pSurface;
                        r = static_cast<BYTE>(((((nPixel & pVidMode->m_dwRBitMask) >> nRShift) << nRSrcShift) + GetRValue(rgbBlack)) * 0x80 >> 8);
                        g = static_cast<BYTE>(((((nPixel & pVidMode->m_dwGBitMask) >> nGShift) << nGSrcShift) + GetGValue(rgbBlack)) * 0x80 >> 8);
                        b = static_cast<BYTE>(((((nPixel & pVidMode->m_dwBBitMask) >> nBShift) << nBSrcShift) + GetBValue(rgbBlack)) * 0x80 >> 8);
                    } else {
                        if ((dwFlags & 0x80000) != 0) {
                            r = static_cast<BYTE>((r + g + b) >> 2);
                            g = r;
                            b = r;
                        }

                        if (bTint) {
                            r = static_cast<BYTE>(static_cast<INT>(r * nTintRed) >> nTintShift);
                            g = static_cast<BYTE>(static_cast<INT>(g * nTintGreen) >> nTintShift);
                            b = static_cast<BYTE>(static_cast<INT>(b * nTintBlue) >> nTintShift);
                        }

                        if (bAdd) {
                            r = ~static_cast<BYTE>((static_cast<BYTE>(~r) * nAddRed) >> nAddShift);
                            g = ~static_cast<BYTE>((static_cast<BYTE>(~g) * nAddGreen) >> nAddShift);
                            b = ~static_cast<BYTE>((static_cast<BYTE>(~b) * nAddBlue) >> nAddShift);
                        }

                        if (bLight) {
                            INT nValue = (r * nLightRed) >> CVidPalette::LIGHT_SCALE;
                            r = nValue > 255 ? 255 : static_cast<BYTE>(nValue);
                            nValue = (g * nLightGreen) >> CVidPalette::LIGHT_SCALE;
                            g = nValue > 255 ? 255 : static_cast<BYTE>(nValue);
                            nValue = (b * nLightBlue) >> CVidPalette::LIGHT_SCALE;
                            b = nValue > 255 ? 255 : static_cast<BYTE>(nValue);

                            // Keep it off the transparent colour.
                            if (r == 0 && g == 255 && b == 0) {
                                g = 254;
                            }
                        }
                    }

                    *pSurface = static_cast<WORD>(((b >> nBSrcShift) << nBShift)
                        | ((g >> nGSrcShift) << nGShift)
                        | ((r >> nRSrcShift) << nRShift));
                }

                pSurface++;
                pData += 3;
            }

            pSurface += -bmpSize.cx - lPitch / 2;
            pData += nDataJump;
        }
    } else {
        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                // NOTE: The original keeps each channel in a whole register
                // (FUN_007b3ff8's shared tail, helper 0x7B3F66), and the
                // light step can leave a value above 255 there: its clamp
                // tests the second byte and only replaces the first
                // (`test ah, 0xff` / `mov al, 0xff`), so the second byte
                // survives. The pack then shifts the full 16-bit pair
                // (`shr al` leaves ah, `shl ax`), so an overflowing channel
                // leaks into the neighbouring fields - the accent in the
                // destination word. Masking each step to a byte loses that.
                INT r = pData[2];
                INT g = pData[1];
                INT b = pData[0];

                if ((dwFlags & 0x80000) != 0) {
                    r = (r + g + b) >> 2;
                    g = r;
                    b = r;
                }

                if (bTint) {
                    r = (r * static_cast<INT>(nTintRed)) >> nTintShift;
                    g = (g * static_cast<INT>(nTintGreen)) >> nTintShift;
                    b = (b * static_cast<INT>(nTintBlue)) >> nTintShift;
                }

                if (bAdd) {
                    // `not al` twice: only the low byte is inverted, and the
                    // multiply-shift cannot raise the register above 255.
                    r = (~((static_cast<BYTE>(~static_cast<BYTE>(r)) * nAddRed) >> nAddShift)) & 0xFF;
                    g = (~((static_cast<BYTE>(~static_cast<BYTE>(g)) * nAddGreen) >> nAddShift)) & 0xFF;
                    b = (~((static_cast<BYTE>(~static_cast<BYTE>(b)) * nAddBlue) >> nAddShift)) & 0xFF;
                }

                if (bLight) {
                    r = (r * nLightRed) >> CVidPalette::LIGHT_SCALE;
                    g = (g * nLightGreen) >> CVidPalette::LIGHT_SCALE;
                    b = (b * nLightBlue) >> CVidPalette::LIGHT_SCALE;

                    // The clamp leaves the second byte in place.
                    if ((r >> 8) != 0) {
                        r = (r & 0xFF00) | 0xFF;
                    }
                    if ((g >> 8) != 0) {
                        g = (g & 0xFF00) | 0xFF;
                    }
                    if ((b >> 8) != 0) {
                        b = (b & 0xFF00) | 0xFF;
                    }
                }

                // The 16-bit pack: the second byte of each value is shifted
                // along with the first, as the register pair is.
                const WORD nR = static_cast<WORD>(((r & 0xFF00) | ((r & 0xFF) >> nRSrcShift)) << nRShift);
                const WORD nG = static_cast<WORD>(((g & 0xFF00) | ((g & 0xFF) >> nGSrcShift)) << nGShift);
                const WORD nB = static_cast<WORD>(((b & 0xFF00) | ((b & 0xFF) >> nBSrcShift)) << nBShift);

                *pSurface = static_cast<WORD>(nB | nG | nR);

                pSurface++;
                pData += 3;
            }

            pSurface += -bmpSize.cx - lPitch / 2;
            pData += nDataJump;
        }
    }

    return TRUE;
}

namespace {

// A 24-bit pixel written as the original does: all four bytes of value,
// the fourth landing on the next pixel's first byte, except for a row's last
// pixel, which gets three (a WORD and a BYTE).
inline void PutBmpPixel24(BYTE* pPixel, DWORD value, bool bLast)
{
    pPixel[0] = static_cast<BYTE>(value);
    pPixel[1] = static_cast<BYTE>(value >> 8);
    pPixel[2] = static_cast<BYTE>(value >> 16);
    if (!bLast) {
        pPixel[3] = static_cast<BYTE>(value >> 24);
    }
}

}

// 0x7CCB20
BOOL CVidBitmap::BltBmp8To24(BYTE* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    m_cPalette.SetPalette(pRes->GetColorTable(), pRes->GetColorCount(), CVidPalette::TYPE_RESOURCE);
    m_cPalette.Realize(rgbTempPal, 24, dwFlags | 0x800000, &m_paletteAffects, 255);

    // NOTE: The original runs width - 1 pixels with a do-while, so a width
    // of 1 never ends there; RenderDirect passes wider bitmaps. The rows go
    // up the surface (the destination is the bitmap's last row).
    for (LONG y = bmpSize.cy; y > 0; y--) {
        BYTE* pPixel = pSurface;
        for (LONG x = 0; x < bmpSize.cx; x++) {
            PutBmpPixel24(pPixel, rgbTempPal[pData[x]], x == bmpSize.cx - 1);
            pPixel += 3;
        }

        pData += bmpSize.cx + nDataJump;
        pSurface -= lPitch;
    }

    return TRUE;
}

// 0x7CCBF0
BOOL CVidBitmap::BltBmp24To24(BYTE* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    BYTE nRShift = static_cast<BYTE>(pVidMode->m_dwRBitShift) & 0x1F;
    BYTE nGShift = static_cast<BYTE>(pVidMode->m_dwGBitShift) & 0x1F;
    BYTE nBShift = static_cast<BYTE>(pVidMode->m_dwBBitShift) & 0x1F;

    if ((dwFlags & 0x1) == 0) {
        // As BltBmp8To24: every pixel but a row's last written whole.
        for (LONG y = bmpSize.cy; y > 0; y--) {
            BYTE* pPixel = pSurface;
            for (LONG x = 0; x < bmpSize.cx; x++) {
                DWORD value = (static_cast<DWORD>(pData[2]) << nRShift)
                    | (static_cast<DWORD>(pData[1]) << nGShift)
                    | (static_cast<DWORD>(pData[0]) << nBShift);
                PutBmpPixel24(pPixel, value, x == bmpSize.cx - 1);
                pPixel += 3;
                pData += 3;
            }

            pData += nDataJump;
            pSurface -= lPitch;
        }
    } else {
        // Keyed: pure green is skipped, black with 0x4 halves what is there.
        // NOTE: A drawn pixel other than a row's last writes a fourth byte
        // over the next pixel's first, which a skipped pixel keeps; the
        // last pixel's shadow reads red and green from its first WORD and
        // blue from its third byte (shr al,1 on [edi+0x2] at 0x7CCD6E).
        for (LONG y = 0; y < bmpSize.cy; y++) {
            BYTE* pPixel = pSurface;
            for (LONG x = 0; x < bmpSize.cx; x++) {
                const bool bLast = x == bmpSize.cx - 1;
                DWORD r = pData[2];
                DWORD g = pData[1];
                DWORD b = pData[0];

                if (r != 0 || g != 0xFF || b != 0) {
                    if ((dwFlags & 0x4) != 0 && r == 0 && g == 0 && b == 0) {
                        if (bLast) {
                            DWORD nWord = pPixel[0] | (static_cast<DWORD>(pPixel[1]) << 8);
                            r = ((nWord >> nRShift) >> 1) & 0x7F;
                            g = ((nWord >> nGShift) >> 1) & 0x7F;
                            b = pPixel[2] >> 1;
                        } else {
                            DWORD nPixel = pPixel[0]
                                | (static_cast<DWORD>(pPixel[1]) << 8)
                                | (static_cast<DWORD>(pPixel[2]) << 16)
                                | (static_cast<DWORD>(pPixel[3]) << 24);
                            r = ((nPixel >> nRShift) >> 1) & 0x7F;
                            g = ((nPixel >> nGShift) >> 1) & 0x7F;
                            b = ((nPixel >> nBShift) >> 1) & 0x7F;
                        }
                    }

                    PutBmpPixel24(pPixel, (b << nBShift) | (g << nGShift) | (r << nRShift), bLast);
                }

                pPixel += 3;
                pData += 3;
            }

            pData += nDataJump;
            pSurface -= lPitch;
        }
    }

    return TRUE;
}

// The 24-bit BMP (B, G, R bytes) onto a 24-bit surface with the tint, add
// and light passes of BltBmp24To32Tint, plus its keyed form (pure green is
// transparent; with 0x4 a pure black pixel halves the destination, here
// against the mode's brightness/contrast black). Rows go bottom-up and
// lPitch is in bytes.
//
// 0x7CCE80
BOOL CVidBitmap::BltBmp24To24Tint(BYTE* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    DWORD nTintRed = 255;
    DWORD nTintGreen = 255;
    DWORD nTintBlue = 255;
    INT nTintShift = 0;

    DWORD nAddRed = 1;
    DWORD nAddGreen = 1;
    DWORD nAddBlue = 1;
    INT nAddShift = 0;

    INT nLightRed = 1 << CVidPalette::LIGHT_SCALE;
    INT nLightGreen = 1 << CVidPalette::LIGHT_SCALE;
    INT nLightBlue = 1 << CVidPalette::LIGHT_SCALE;

    BYTE nRShift = static_cast<BYTE>(pVidMode->m_dwRBitShift);
    BYTE nGShift = static_cast<BYTE>(pVidMode->m_dwGBitShift);
    BYTE nBShift = static_cast<BYTE>(pVidMode->m_dwBBitShift);

    BOOL bTint = GetTint(nTintRed, nTintGreen, nTintBlue, nTintShift, dwFlags);
    BOOL bAdd = GetAdd(nAddRed, nAddGreen, nAddBlue, nAddShift, dwFlags);
    BOOL bLight = GetLight(nLightRed, nLightGreen, nLightBlue, dwFlags);

    // NOTE: The original's unkeyed loop walks width - 1 pixels with a
    // do-while, so a width below two never ends, and its rows run body first,
    // so a height of 0 still draws one row. The keyed loop guards both (a
    // width below two draws just the row's last pixel - one per row at width
    // 0) and tests the height first. The loops below are the per-pixel form,
    // exact for every width of two and up; RenderDirect passes wider bitmaps
    // and the oracle case sweeps widths 2..32.
    if ((dwFlags & 0x1) == 0) {
        // NOTE: As BltBmp24To32Tint's unkeyed loop, the add step inverts
        // only the low byte on the whole register and the light step clamps
        // only the low byte, letting higher bits carry into the pixel.
        for (LONG y = bmpSize.cy; y > 0; y--) {
            BYTE* pPixel = pSurface;
            for (LONG x = 0; x < bmpSize.cx; x++) {
                DWORD r = pData[2];
                DWORD g = pData[1];
                DWORD b = pData[0];

                if ((dwFlags & 0x80000) != 0) {
                    r = (r + g + b) >> 2;
                    g = r;
                    b = r;
                }

                if (bTint) {
                    r = (r * nTintRed) >> nTintShift;
                    g = (g * nTintGreen) >> nTintShift;
                    b = (b * nTintBlue) >> nTintShift;
                }

                if (bAdd) {
                    r = (((r ^ 0xFF) * nAddRed) >> nAddShift) ^ 0xFF;
                    g = (((g ^ 0xFF) * nAddGreen) >> nAddShift) ^ 0xFF;
                    b = (((b ^ 0xFF) * nAddBlue) >> nAddShift) ^ 0xFF;
                }

                if (bLight) {
                    r = (r * nLightRed) >> 3;
                    g = (g * nLightGreen) >> 3;
                    b = (b * nLightBlue) >> 3;
                    if ((r & 0xFF00) != 0) {
                        r |= 0xFF;
                    }
                    if ((g & 0xFF00) != 0) {
                        g |= 0xFF;
                    }
                    if ((b & 0xFF00) != 0) {
                        b |= 0xFF;
                    }
                }

                PutBmpPixel24(pPixel, (b << nBShift) | (g << nGShift) | (r << nRShift), x == bmpSize.cx - 1);

                pPixel += 3;
                pData += 3;
            }

            pData += nDataJump;
            pSurface -= lPitch;
        }
    } else {
        COLORREF rgbBlack = pVidMode->ApplyBrightnessContrast(RGB(0, 0, 0));

        // NOTE: The original's last pixel of a row reads a WORD (red and green
        // through their shifts) and the third byte for blue, while the others
        // read a DWORD; with shifts 16/8/0 that leaves the last pixel's red at
        // the black level and its blue the destination's red byte. Kept.
        for (LONG y = 0; y < bmpSize.cy; y++) {
            BYTE* pPixel = pSurface;
            for (LONG x = 0; x < bmpSize.cx; x++) {
                const bool bLast = x == bmpSize.cx - 1;
                BYTE r = pData[2];
                BYTE g = pData[1];
                BYTE b = pData[0];

                if (r != 0 || g != 0xFF || b != 0) {
                    if ((dwFlags & 0x4) != 0 && r == 0 && g == 0 && b == 0) {
                        // Shadow: half of the destination plus the mode's
                        // brightness/contrast black.
                        DWORD nRed;
                        DWORD nGreen;
                        DWORD nBlue;
                        if (bLast) {
                            const DWORD nWord = pPixel[0] | (static_cast<DWORD>(pPixel[1]) << 8);
                            nRed = ((nWord >> nRShift) & 0xFF) + GetRValue(rgbBlack);
                            nGreen = ((nWord >> nGShift) & 0xFF) + GetGValue(rgbBlack);
                            nBlue = pPixel[2] + GetBValue(rgbBlack);
                        } else {
                            const DWORD nPixel = pPixel[0]
                                | (static_cast<DWORD>(pPixel[1]) << 8)
                                | (static_cast<DWORD>(pPixel[2]) << 16)
                                | (static_cast<DWORD>(pPixel[3]) << 24);
                            nRed = ((nPixel >> nRShift) & 0xFF) + GetRValue(rgbBlack);
                            nGreen = ((nPixel >> nGShift) & 0xFF) + GetGValue(rgbBlack);
                            nBlue = ((nPixel >> nBShift) & 0xFF) + GetBValue(rgbBlack);
                        }

                        r = static_cast<BYTE>((nRed << 7) >> 8);
                        g = static_cast<BYTE>((nGreen << 7) >> 8);
                        b = static_cast<BYTE>((nBlue << 7) >> 8);
                    } else {
                        if ((dwFlags & 0x80000) != 0) {
                            r = static_cast<BYTE>((r + g + b) >> 2);
                            g = r;
                            b = r;
                        }

                        if (bTint) {
                            r = static_cast<BYTE>((r * nTintRed) >> nTintShift);
                            g = static_cast<BYTE>((g * nTintGreen) >> nTintShift);
                            b = static_cast<BYTE>((b * nTintBlue) >> nTintShift);
                        }

                        if (bAdd) {
                            r = ~static_cast<BYTE>((static_cast<BYTE>(~r) * nAddRed) >> nAddShift);
                            g = ~static_cast<BYTE>((static_cast<BYTE>(~g) * nAddGreen) >> nAddShift);
                            b = ~static_cast<BYTE>((static_cast<BYTE>(~b) * nAddBlue) >> nAddShift);
                        }

                        if (bLight) {
                            INT nValue = (r * nLightRed) >> 3;
                            r = nValue > 255 ? 255 : static_cast<BYTE>(nValue);
                            nValue = (g * nLightGreen) >> 3;
                            g = nValue > 255 ? 255 : static_cast<BYTE>(nValue);
                            nValue = (b * nLightBlue) >> 3;
                            b = nValue > 255 ? 255 : static_cast<BYTE>(nValue);

                            // Keep it off the transparent colour.
                            if (r == 0 && g == 0xFF && b == 0) {
                                g = 254;
                            }
                        }
                    }

                    PutBmpPixel24(pPixel, (b << nBShift) | (g << nGShift) | (r << nRShift), bLast);
                }

                pPixel += 3;
                pData += 3;
            }

            pData += nDataJump;
            pSurface -= lPitch;
        }
    }

    return TRUE;
}

// 0x7CD5E0
BOOL CVidBitmap::BltBmp8To32(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    m_cPalette.SetPalette(pRes->GetColorTable(), pRes->GetColorCount(), CVidPalette::TYPE_RESOURCE);
    m_cPalette.Realize(rgbTempPal, 32, dwFlags | 0x800000, &m_paletteAffects, 255);

    for (LONG y = 0; y < bmpSize.cy; y++) {
        for (LONG x = 0; x < bmpSize.cx; x++) {
            *pSurface++ = rgbTempPal[*pData++];
        }

        pSurface += -bmpSize.cx - lPitch / 4;
        pData += nDataJump;
    }

    return TRUE;
}

// 0x7CD690
BOOL CVidBitmap::BltBmp24To32(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    DWORD dwRBitShift = pVidMode->m_dwRBitShift;
    DWORD dwGBitShift = pVidMode->m_dwGBitShift;
    DWORD dwBBitShift = pVidMode->m_dwBBitShift;

    if ((dwFlags & 0x1) != 0) {
        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                BYTE r = pData[2];
                BYTE g = pData[1];
                BYTE b = pData[0];
                BYTE a;

                if (r != 0 || g != 255 || b != 0) {
                    if ((dwFlags & 0x4) == 0 || r != 0 || g != 0 || b != 0) {
                        a = 255;
                    } else {
                        a = 128;
                        r = ((*pSurface >> dwRBitShift) >> 1) & 0x7F;
                        g = ((*pSurface >> dwGBitShift) >> 1) & 0x7F;
                        b = ((*pSurface >> dwBBitShift) >> 1) & 0x7F;
                    }

                    *pSurface = (a << 24) | (r << dwRBitShift) | (g << dwGBitShift) | (b << dwBBitShift);
                }

                pSurface++;
                pData += 3;
            }

            pSurface += -bmpSize.cx - lPitch / 4;
            pData += nDataJump;
        }
    } else {
        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                *pSurface++ = (pData[0] << dwBBitShift) | (pData[1] << dwGBitShift) | (pData[2] << dwRBitShift);
                pData += 3;
            }

            pSurface += -bmpSize.cx - lPitch / 4;
            pData += nDataJump;
        }
    }

    return TRUE;
}

// 0x7CD870
BOOL CVidBitmap::BltBmp24To32Tint(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& bmpSize, LONG nDataJump, DWORD dwFlags)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    DWORD nTintRed = 255;
    DWORD nTintGreen = 255;
    DWORD nTintBlue = 255;
    INT nTintShift = 0;

    DWORD nAddRed = 1;
    DWORD nAddGreen = 1;
    DWORD nAddBlue = 1;
    INT nAddShift = 0;

    INT nLightRed = 1 << CVidPalette::LIGHT_SCALE;
    INT nLightGreen = 1 << CVidPalette::LIGHT_SCALE;
    INT nLightBlue = 1 << CVidPalette::LIGHT_SCALE;

    BYTE nRShift = static_cast<BYTE>(pVidMode->m_dwRBitShift);
    BYTE nGShift = static_cast<BYTE>(pVidMode->m_dwGBitShift);
    BYTE nBShift = static_cast<BYTE>(pVidMode->m_dwBBitShift);

    BOOL bTint = GetTint(nTintRed, nTintGreen, nTintBlue, nTintShift, dwFlags);
    BOOL bAdd = GetAdd(nAddRed, nAddGreen, nAddBlue, nAddShift, dwFlags);
    BOOL bLight = GetLight(nLightRed, nLightGreen, nLightBlue, dwFlags);

    // NOTE: The original runs both loops body first (do-while); RenderDirect
    // never passes an empty size.
    if ((dwFlags & 0x1) != 0) {
        COLORREF rgbBlack = pVidMode->ApplyBrightnessContrast(RGB(0, 0, 0));

        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                BYTE r = pData[2];
                BYTE g = pData[1];
                BYTE b = pData[0];

                // Pure green is the transparent colour.
                if (r != 0 || g != 255 || b != 0) {
                    BYTE a;
                    if ((dwFlags & 0x4) != 0 && r == 0 && g == 0 && b == 0) {
                        // Shadow: half black over what is there.
                        a = 0x80; // 0x85E6F2
                        r = static_cast<BYTE>((((*pSurface >> nRShift) & 0xFF) + GetRValue(rgbBlack)) << 7 >> 8);
                        g = static_cast<BYTE>((((*pSurface >> nGShift) & 0xFF) + GetGValue(rgbBlack)) << 7 >> 8);
                        b = static_cast<BYTE>((((*pSurface >> nBShift) & 0xFF) + GetBValue(rgbBlack)) << 7 >> 8);
                    } else {
                        a = 0xFF;

                        if ((dwFlags & 0x80000) != 0) {
                            r = static_cast<BYTE>((r + g + b) >> 2);
                            g = r;
                            b = r;
                        }

                        if (bTint) {
                            r = static_cast<BYTE>(static_cast<INT>(r * nTintRed) >> nTintShift);
                            g = static_cast<BYTE>(static_cast<INT>(g * nTintGreen) >> nTintShift);
                            b = static_cast<BYTE>(static_cast<INT>(b * nTintBlue) >> nTintShift);
                        }

                        if (bAdd) {
                            r = ~static_cast<BYTE>((static_cast<BYTE>(~r) * nAddRed) >> nAddShift);
                            g = ~static_cast<BYTE>((static_cast<BYTE>(~g) * nAddGreen) >> nAddShift);
                            b = ~static_cast<BYTE>((static_cast<BYTE>(~b) * nAddBlue) >> nAddShift);
                        }

                        if (bLight) {
                            INT nValue = (r * nLightRed) >> CVidPalette::LIGHT_SCALE;
                            r = nValue > 255 ? 255 : static_cast<BYTE>(nValue);
                            nValue = (g * nLightGreen) >> CVidPalette::LIGHT_SCALE;
                            g = nValue > 255 ? 255 : static_cast<BYTE>(nValue);
                            nValue = (b * nLightBlue) >> CVidPalette::LIGHT_SCALE;
                            b = nValue > 255 ? 255 : static_cast<BYTE>(nValue);

                            // Keep it off the transparent colour.
                            if (r == 0 && g == 255 && b == 0) {
                                g = 254;
                            }
                        }
                    }

                    *pSurface = (b << nBShift) | (g << nGShift) | (r << nRShift) | (static_cast<DWORD>(a) << 24);
                }

                pSurface++;
                pData += 3;
            }

            pSurface -= (lPitch >> 2) + bmpSize.cx;
            pData += nDataJump;
        }
    } else {
        // NOTE: Inline assembly in the original, on whole registers: the
        // add step inverts only the low byte and the light step clamps only
        // the low byte, so higher bits carry into the pixel as there.
        for (LONG y = 0; y < bmpSize.cy; y++) {
            for (LONG x = 0; x < bmpSize.cx; x++) {
                DWORD r = pData[2];
                DWORD g = pData[1];
                DWORD b = pData[0];

                if ((dwFlags & 0x80000) != 0) {
                    r = (r + g + b) >> 2;
                    g = r;
                    b = r;
                }

                if (bTint) {
                    r = (r * nTintRed) >> nTintShift;
                    g = (g * nTintGreen) >> nTintShift;
                    b = (b * nTintBlue) >> nTintShift;
                }

                if (bAdd) {
                    r = (((r ^ 0xFF) * nAddRed) >> nAddShift) ^ 0xFF;
                    g = (((g ^ 0xFF) * nAddGreen) >> nAddShift) ^ 0xFF;
                    b = (((b ^ 0xFF) * nAddBlue) >> nAddShift) ^ 0xFF;
                }

                if (bLight) {
                    r = (r * nLightRed) >> 3;
                    g = (g * nLightGreen) >> 3;
                    b = (b * nLightBlue) >> 3;
                    if ((r & 0xFF00) != 0) {
                        r |= 0xFF;
                    }
                    if ((g & 0xFF00) != 0) {
                        g |= 0xFF;
                    }
                    if ((b & 0xFF00) != 0) {
                        b |= 0xFF;
                    }
                }

                *pSurface = (r << nRShift) | (g << nGShift) | (b << nBShift);

                pSurface++;
                pData += 3;
            }

            pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) - (lPitch + bmpSize.cx * 4));
            pData += nDataJump;
        }
    }

    return TRUE;
}

// 0x7C6800
void CVidBitmap::RenderTexture(INT x, INT y, const CSize& bmpSize, const CRect& rClip, DWORD dwFlags)
{
    if (x + bmpSize.cx < rClip.left
        || x > rClip.right
        || y + bmpSize.cy < rClip.top
        || y > rClip.bottom) {
        return;
    }

    float x1;
    float y1;
    float x2;
    float y2;
    float x3;
    float y3;
    float x4;
    float y4;

    x1 = static_cast<float>(x);
    y1 = static_cast<float>(y);
    x2 = static_cast<float>(x + bmpSize.cx);
    y2 = static_cast<float>(y + bmpSize.cy);
    x3 = 0.0f;
    y3 = 0.0f;
    x4 = static_cast<float>(bmpSize.cx) / static_cast<float>(CVidInf::FX_WIDTH);
    y4 = static_cast<float>(bmpSize.cy) / static_cast<float>(CVidInf::FX_HEIGHT);

    if (x < rClip.left) {
        x1 = static_cast<float>(rClip.left);
        x3 = static_cast<float>(rClip.left - x) / static_cast<float>(CVidInf::FX_WIDTH);
    }

    if (y < rClip.top) {
        y1 = static_cast<float>(rClip.top);
        y3 = static_cast<float>(rClip.top - y) / static_cast<float>(CVidInf::FX_HEIGHT);
    }

    if (x + bmpSize.cx > rClip.right) {
        x2 = static_cast<float>(rClip.right);
        x4 = static_cast<float>(rClip.right - x) / static_cast<float>(CVidInf::FX_WIDTH);
    }

    if (y + bmpSize.cy > rClip.bottom) {
        y2 = static_cast<float>(rClip.bottom);
        y4 = static_cast<float>(rClip.bottom - y) / static_cast<float>(CVidInf::FX_HEIGHT);
    }

    CVideo3d::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x1) != 0) {
        CVideo3d::glEnable(GL_BLEND);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    x1 += CVideo3d::SUB_PIXEL_SHIFT;
    y1 += CVideo3d::SUB_PIXEL_SHIFT;
    x2 += CVideo3d::SUB_PIXEL_SHIFT;
    y2 += CVideo3d::SUB_PIXEL_SHIFT;

    CVideo3d::glBegin(GL_TRIANGLE_STRIP);
    CVideo3d::glTexCoord2f(x3, y3);
    CVideo3d::glVertex3f(x1, y1, 0.0f);
    CVideo3d::glTexCoord2f(x3, y4);
    CVideo3d::glVertex3f(x1, y2, 0.0f);
    CVideo3d::glTexCoord2f(x4, y3);
    CVideo3d::glVertex3f(x2, y1, 0.0f);
    CVideo3d::glTexCoord2f(x4, y4);
    CVideo3d::glVertex3f(x2, y2, 0.0f);
    CVideo3d::glEnd();
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x1) != 0) {
        CVideo3d::glDisable(GL_BLEND);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }
}

// 0x7C6B80
BOOL CVidBitmap::Render3d(INT x, INT y, const CRect& rClip, DWORD dwFlags, BOOLEAN bDemanded)
{
    if (pRes == NULL || rClip.IsRectEmpty()) {
        return FALSE;
    }

    if (!bDemanded) {
        pRes->Demand();
    }

    BYTE* pData = pRes->GetImageData(m_bDoubleSize);
    m_nBitCount = pRes->GetBitDepth();

    CSize bmpSize;
    GetImageDimensions(bmpSize, m_bDoubleSize);

    INT nBytesPerPixel;
    switch (m_nBitCount) {
    case 8:
        nBytesPerPixel = 1;
        break;
    case 24:
        nBytesPerPixel = 3;
        break;
    default:
        if (!bDemanded) {
            pRes->Release();
        }
        return FALSE;
    }

    LONG nDataJump = (-((bmpSize.cx * nBytesPerPixel) & 3)) & 3;

    CVideo3d::glEnable(GL_TEXTURE_2D);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    g_pChitin->cVideo.field_13E = 2;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, 2);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CSize tileSize;
    LONG lTilePitch;
    LONG nTileDataJump = 0;

    BYTE* pTileRowData = pData;
    for (INT yOffset = 0; yOffset < bmpSize.cy; yOffset += CVidInf::FX_HEIGHT) {
        BYTE* pTileData = pTileRowData;
        for (INT xOffset = 0; xOffset < bmpSize.cx; xOffset += CVidInf::FX_WIDTH) {
            if ((dwFlags & 0x1) != 0) {
                CVidInf::FXClear(CVideo3d::texImageData, CVidInf::FX_WIDTH * CVidInf::FX_HEIGHT);
            }

            tileSize.cx = min(bmpSize.cx - xOffset, CVidInf::FX_WIDTH);
            tileSize.cy = min(bmpSize.cy - yOffset, CVidInf::FX_HEIGHT);

            if (bmpSize.cx > tileSize.cx) {
                nTileDataJump = nBytesPerPixel * (bmpSize.cx - tileSize.cx);
            }

            lTilePitch = g_pChitin->cVideo.field_13A
                ? CVidTile::BYTES_PER_TEXEL * CVIDINF_FX_WIDTH
                : CVidTile::BYTES_PER_TEXEL * tileSize.cx;

            BOOLEAN bResult = TRUE;
            switch (m_nBitCount) {
            case 8:
                bResult = BltBmp8To32(&(CVideo3d::texImageData[lTilePitch * (tileSize.cy - 1) / CVidTile::BYTES_PER_TEXEL]),
                    lTilePitch,
                    pTileData,
                    tileSize,
                    nDataJump + nTileDataJump,
                    dwFlags);
                break;
            case 24:
                if ((dwFlags & 0xFFFF0000) != 0
                    && g_pChitin->GetCurrentVideoMode()->ApplyBrightnessContrast(RGB(128, 128, 128)) != RGB(128, 128, 128)) {
                    bResult = BltBmp24To32Tint(&(CVideo3d::texImageData[lTilePitch * (tileSize.cy - 1) / CVidTile::BYTES_PER_TEXEL]),
                        lTilePitch,
                        pTileData,
                        tileSize,
                        nDataJump + nTileDataJump,
                        dwFlags);
                } else {
                    bResult = BltBmp24To32(&(CVideo3d::texImageData[(lTilePitch * (tileSize.cy - 1) / CVidTile::BYTES_PER_TEXEL)]),
                        lTilePitch,
                        pTileData,
                        tileSize,
                        nDataJump + nTileDataJump,
                        dwFlags);
                }
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
                // __LINE__: 2120
                UTIL_ASSERT(FALSE);
            }

            if (!bResult) {
                if (bDemanded) {
                    pRes->Release();
                }
                return FALSE;
            }

            if (g_pChitin->cVideo.field_13A) {
                CVideo3d::glTexImage2D(GL_TEXTURE_2D,
                    0,
                    GL_RGBA,
                    CVidInf::FX_WIDTH,
                    CVidInf::FX_HEIGHT,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    CVideo3d::texImageData);
                g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
            } else {
                CVideo3d::glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    tileSize.cx,
                    tileSize.cy,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    CVideo3d::texImageData);
                g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
            }

            RenderTexture(x + xOffset,
                y + bmpSize.cy - tileSize.cy - yOffset,
                tileSize,
                rClip,
                dwFlags);

            pTileData += CVidInf::FX_WIDTH * nBytesPerPixel;
        }
        pTileRowData += CVidInf::FX_HEIGHT * (nDataJump + bmpSize.cx * nBytesPerPixel);
    }

    if (!bDemanded) {
        pRes->Release();
    }

    return TRUE;
}

// 0x7C7070
BOOL CVidBitmap::RenderDirect3d(INT x, INT y, const CRect& rClip, DWORD dwFlags, BOOLEAN bDemanded)
{
    return Render3d(x, y, rClip, dwFlags, bDemanded);
}
