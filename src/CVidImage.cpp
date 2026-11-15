#include "CVidImage.h"

#include "CChitin.h"
#include "CResTile.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CVideo3d.h"
#include "render/api.h"

#include <cstdint>

// 0x85E838
const INT CVidTile::BYTES_PER_TEXEL = 4;

// 0xA09FC8
BYTE CVidTile::m_pPixels[16384];

// 0x9064F8
DWORD CVidImage::rgbTempPal[256];

// The dual-tile blitter's realized second palette. Its address sits
// between m_pBlendTable (0x9060F0) and rgbTempPal (0x9064F8); the 4-byte
// gap before rgbTempPal matches the exe's static layout.
//
// 0x9060F4
DWORD CVidImage::rgbDualTempPal[256];

// The 64x64x32 blend table: entry (k, m, i) is m and k mixed at weight
// i / 32, (m * (64 - 2i) + k * 2i) >> 6.
//
// 0x9060F0
BYTE* CVidImage::m_pBlendTable;

// 0x9068F8
INT CVidImage::m_nBlendTableRefs;

// Allocates and fills the shared blend table (0x20000 bytes) and returns
// TRUE, or returns FALSE and leaves the pointer NULL when the allocation
// fails. The original carries this body twice: inlined in the CVidImage
// constructor under the m_nBlendTableRefs count, and here as a standalone
// helper the 24- and 32-bpp tile blitters call when they find no table;
// the tree keeps the one copy.
//
// 0x7ACCB0
BOOL CVidImage::BuildBlendTable()
{
    m_pBlendTable = new BYTE[0x20000];
    if (m_pBlendTable == NULL) {
        return FALSE;
    }

    INT nWeight = 0;
    INT nStep = 64;
    do {
        INT nRowStart = 0;
        for (INT nColumn = 0; nColumn < 0x800; nColumn += 0x20) {
            INT nValue = nRowStart;
            INT nOffset = nColumn;
            for (INT nRow = 64; nRow != 0; nRow--) {
                m_pBlendTable[nOffset + nWeight] = static_cast<BYTE>(nValue >> 6);
                nOffset += 0x800;
                nValue += nWeight * 2;
            }
            nRowStart += nStep;
        }
        nStep -= 2;
        nWeight++;
    } while (nStep > 0);

    return TRUE;
}

// 0x7ACA60
CVidImage::CVidImage()
{
    m_nFxSurface = -1;
    m_paletteAffects.rgbTintColor = 0xFFFFFF;
    m_paletteAffects.rgbAddColor = 0;
    m_paletteAffects.rgbLightColor = (1 << CVidPalette::LIGHT_SCALE) | ((1 << CVidPalette::LIGHT_SCALE) << 8) | ((1 << CVidPalette::LIGHT_SCALE) << 16);
    memset(m_paletteAffects.pRangeTints, 0, sizeof(m_paletteAffects.pRangeTints[0]) * CVidPalette::NUM_RANGES);
    memset(m_paletteAffects.pRangeAdds, 0, sizeof(m_paletteAffects.pRangeAdds[0]) * CVidPalette::NUM_RANGES);
    memset(m_paletteAffects.pRangeLights, 0, sizeof(m_paletteAffects.pRangeLights[0]) * CVidPalette::NUM_RANGES);
    memset(m_paletteAffects.aRangeTintPeriods, 0, sizeof(m_paletteAffects.aRangeTintPeriods[0]) * CVidPalette::NUM_RANGES);
    memset(m_paletteAffects.aRangeAddPeriods, 0, sizeof(m_paletteAffects.aRangeAddPeriods[0]) * CVidPalette::NUM_RANGES);
    memset(m_paletteAffects.aRangeLightPeriods, 0, sizeof(m_paletteAffects.aRangeLightPeriods[0]) * CVidPalette::NUM_RANGES);
    m_paletteAffects.suppressTints = 0;

    // The first image builds the blend table.
    if (m_nBlendTableRefs++ == 0) {
        BuildBlendTable();
    }
}

// 0x7ACC40
CVidImage::~CVidImage()
{
    // The last image frees the blend table.
    if (--m_nBlendTableRefs == 0 && m_pBlendTable != NULL) {
        delete[] m_pBlendTable;
        m_pBlendTable = NULL;
    }
}

// #binary-identical
// 0x7ACD40
COLORREF CVidImage::GetTintColor()
{
    sizeof(m_cPalette);
    return m_paletteAffects.rgbTintColor;
}

// #binary-identical
// 0x7ACD50
void CVidImage::SetAddColor(COLORREF rgbNewColor)
{
    m_paletteAffects.rgbAddColor = rgbNewColor;
}

// #binary-identical
// 0x7ACD60
void CVidImage::SetTintColor(COLORREF rgbNewColor)
{
    m_paletteAffects.rgbTintColor = rgbNewColor;
}

// 0x7C40D0
void CVidImage::SetPolyColor(DWORD dwFlags)
{
    CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
}

// NOTE: The pulse phase of the range tints, adds and lights below starts
// from the image's address, as in the original (nAUCounter + this, a 32-bit
// add and a signed idiv). The add wraps in 32 bits as the original's does;
// on 64-bit only the address's low 32 bits count (docs/64bit-cast-sites.md).
static INT PulsePhase(const CVidImage* pImage, BYTE nPeriod)
{
    DWORD nSum = static_cast<DWORD>(g_pChitin->nAUCounter) + static_cast<DWORD>(reinterpret_cast<uintptr_t>(pImage));
    INT nPhase = static_cast<INT>(nSum) % static_cast<INT>(nPeriod * 2);
    if (static_cast<INT>(nPeriod) < nPhase) {
        nPhase = nPeriod * 2 - nPhase;
    }
    return nPhase;
}

// #guess
// 0x7B4230
BOOL CVidImage::GetTint(DWORD& nRed, DWORD& nGreen, DWORD& nBlue, INT& nShiftBack, DWORD dwFlags)
{
    INT nMaxValue = 255;
    BOOL bResult = FALSE;
    INT nLightPhase = 0;
    INT nAddPhase = 0;

    COLORREF rgbGlobalTintColor = g_pChitin->GetCurrentVideoMode()->GetGlobalTintColor();

    if ((dwFlags & 0x20000) != 0) {
        if ((dwFlags & 0x10000) != 0) {
            nMaxValue = 255 * 255;
            nRed = GetRValue(m_paletteAffects.rgbTintColor) * GetRValue(rgbGlobalTintColor);
            nGreen = GetGValue(m_paletteAffects.rgbTintColor) * GetGValue(rgbGlobalTintColor);
            nBlue = GetBValue(m_paletteAffects.rgbTintColor) * GetBValue(rgbGlobalTintColor);
            nShiftBack = 16;
        } else {
            nMaxValue = 255;
            nRed = GetRValue(m_paletteAffects.rgbTintColor);
            nGreen = GetGValue(m_paletteAffects.rgbTintColor);
            nBlue = GetBValue(m_paletteAffects.rgbTintColor);
            nShiftBack = 8;
        }
        bResult = TRUE;
    } else if ((dwFlags & 0x10000) != 0) {
        nMaxValue = 255;
        nRed = GetRValue(rgbGlobalTintColor);
        nGreen = GetGValue(rgbGlobalTintColor);
        nBlue = GetBValue(rgbGlobalTintColor);
        nShiftBack = 8;
        bResult = TRUE;
    }

    COLORREF* pRangeTint = m_paletteAffects.pRangeTints[0];
    if (pRangeTint != NULL) {
        BYTE nPeriod = m_paletteAffects.aRangeTintPeriods[0];
        if (nPeriod == 1) {
            nRed = GetRValue(*pRangeTint) * nRed;
            nGreen = GetGValue(*pRangeTint) * nGreen;
            nBlue = GetBValue(*pRangeTint) * nBlue;
        } else {
            INT nPhase = PulsePhase(this, nPeriod);
            nRed = (255 - static_cast<INT>((255 - GetRValue(*pRangeTint)) * nPhase) / nPeriod) * nRed;
            nGreen = (255 - static_cast<INT>((255 - GetGValue(*pRangeTint)) * nPhase) / nPeriod) * nGreen;
            nBlue = (255 - static_cast<INT>((255 - GetBValue(*pRangeTint)) * nPhase) / nPeriod) * nBlue;
        }
        bResult = TRUE;
        nShiftBack += 8;
        nMaxValue *= 255;
    }

    if (m_paletteAffects.pRangeAdds[0] != NULL) {
        BYTE nPeriod = m_paletteAffects.aRangeAddPeriods[0];
        if (nPeriod == 1 || nPeriod == 0) {
            nAddPhase = nPeriod;
        } else {
            nAddPhase = PulsePhase(this, nPeriod);
        }
    }

    if (m_paletteAffects.pRangeLights[0] != NULL) {
        BYTE nPeriod = m_paletteAffects.aRangeLightPeriods[0];
        if (nPeriod == 1) {
            nLightPhase = 1;
        } else {
            nLightPhase = PulsePhase(this, nPeriod);
        }
    }

    if ((m_paletteAffects.suppressTints & CVidPalette::m_SuppressTintMasks[0]) != 0
        && (nLightPhase != 0 || nAddPhase != 0)) {
        INT nPhase;
        BYTE nPeriod;
        if (nAddPhase < nLightPhase) {
            nPhase = nLightPhase;
            nPeriod = m_paletteAffects.aRangeLightPeriods[0];
        } else {
            nPhase = nAddPhase;
            nPeriod = m_paletteAffects.aRangeAddPeriods[0];
        }
        nRed = static_cast<INT>((nMaxValue - nRed) * nPhase) / nPeriod + nRed;
        nGreen = static_cast<INT>((nMaxValue - nGreen) * nPhase) / nPeriod + nGreen;
        nBlue = static_cast<INT>((nMaxValue - nBlue) * nPhase) / nPeriod + nBlue;
    }

    return bResult;
}

// #guess
// 0x7B4600
BOOL CVidImage::GetAdd(DWORD& nRed, DWORD& nGreen, DWORD& nBlue, INT& nShiftBack, DWORD dwFlags)
{
    BYTE nBrightnessCorrection = g_pChitin->GetCurrentVideoMode()->m_nBrightnessCorrection;
    BOOL bResult = FALSE;

    if ((dwFlags & 0x40000) != 0) {
        if (nBrightnessCorrection != 0) {
            DWORD nInv = static_cast<BYTE>(~nBrightnessCorrection);
            nRed = static_cast<BYTE>(~GetRValue(m_paletteAffects.rgbAddColor)) * nInv;
            nGreen = static_cast<BYTE>(~GetGValue(m_paletteAffects.rgbAddColor)) * nInv;
            nBlue = static_cast<BYTE>(~GetBValue(m_paletteAffects.rgbAddColor)) * nInv;
            nShiftBack = 16;
        } else {
            nRed = static_cast<BYTE>(~GetRValue(m_paletteAffects.rgbAddColor));
            nGreen = static_cast<BYTE>(~GetGValue(m_paletteAffects.rgbAddColor));
            nBlue = static_cast<BYTE>(~GetBValue(m_paletteAffects.rgbAddColor));
            nShiftBack = 8;
        }
        bResult = TRUE;
    } else if (nBrightnessCorrection != 0) {
        DWORD nInv = static_cast<BYTE>(~nBrightnessCorrection);
        nRed = nInv;
        nGreen = nInv;
        nBlue = nInv;
        nShiftBack = 8;
        bResult = TRUE;
    }

    COLORREF* pRangeAdd = m_paletteAffects.pRangeAdds[0];
    if (pRangeAdd != NULL) {
        BYTE nPeriod = m_paletteAffects.aRangeAddPeriods[0];
        BYTE nBlueAdd;
        if (nPeriod == 1 || nPeriod == 0) {
            nRed = static_cast<BYTE>(~GetRValue(*pRangeAdd)) * nRed;
            nGreen = static_cast<BYTE>(~GetGValue(*pRangeAdd)) * nGreen;
            nBlueAdd = GetBValue(*pRangeAdd);
        } else {
            INT nPhase = PulsePhase(this, nPeriod);
            nRed = static_cast<BYTE>(~static_cast<BYTE>(static_cast<INT>(GetRValue(*pRangeAdd) * nPhase) / nPeriod)) * nRed;
            nGreen = static_cast<BYTE>(~static_cast<BYTE>(static_cast<INT>(GetGValue(*pRangeAdd) * nPhase) / nPeriod)) * nGreen;
            nBlueAdd = static_cast<BYTE>(static_cast<INT>(GetBValue(*pRangeAdd) * nPhase) / nPeriod);
        }
        nBlue = static_cast<BYTE>(~nBlueAdd) * nBlue;
        nShiftBack += 8;
        return TRUE;
    }

    return bResult;
}

// #guess
// 0x7B4800
BOOL CVidImage::GetLight(INT& nRed, INT& nGreen, INT& nBlue, DWORD dwFlags)
{
    BYTE nGammaCorrection = g_pChitin->GetCurrentVideoMode()->m_nGammaCorrection;
    BOOL bResult = FALSE;

    if ((dwFlags & 0x200000) != 0) {
        nRed += GetRValue(m_paletteAffects.rgbLightColor) + nGammaCorrection;
        nGreen += GetGValue(m_paletteAffects.rgbLightColor) + nGammaCorrection;
        nBlue += GetBValue(m_paletteAffects.rgbLightColor) + nGammaCorrection;
        bResult = TRUE;
    } else if (nGammaCorrection != 0) {
        nRed += nGammaCorrection;
        nGreen += nGammaCorrection;
        nBlue += nGammaCorrection;
        bResult = TRUE;
    }

    COLORREF* pRangeLight = m_paletteAffects.pRangeLights[0];
    if (pRangeLight == NULL) {
        return bResult;
    }

    BYTE nPeriod = m_paletteAffects.aRangeLightPeriods[0];
    if (nPeriod != 1) {
        INT nPhase = PulsePhase(this, nPeriod);
        nRed += static_cast<INT>(GetRValue(*pRangeLight) * nPhase) / nPeriod;
        nGreen += static_cast<INT>(GetGValue(*pRangeLight) * nPhase) / nPeriod;
        nBlue += static_cast<INT>(GetBValue(*pRangeLight) * nPhase) / nPeriod;
        return TRUE;
    }

    nRed += GetRValue(*pRangeLight);
    nGreen += GetGValue(*pRangeLight);
    nBlue += GetBValue(*pRangeLight);
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x7B16E0
CVidTile::CVidTile()
{
    pRes = NULL;
    m_dwFlags = 0;
}

// 0x7B1700
CVidTile::~CVidTile()
{
}

// 0x7B1770
void CVidTile::SetRes(CResTile* pNewResTiles)
{
    pRes = pNewResTiles;
}

// 0x7B1780
BOOL CVidTile::Render(LPDIRECTDRAWSURFACE pSurface, INT x, INT y, const CRect& rClip, DWORD dwFlags)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawTile(*this, { x, y }, dwFlags)) return TRUE;
    LONG nDataJump = 0;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 6140
    UTIL_ASSERT(pSurface != NULL);

    if (pRes == NULL) {
        return FALSE;
    }

    if (rClip.IsRectEmpty()) {
        return FALSE;
    }

    pRes->Demand();

    BYTE* pData = pRes->GetTileData();
    if (pData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 6159
        UTIL_ASSERT_MSG(FALSE, "Tile data accessed when not loaded");

        // FALSE without releasing the demand (xor eax,eax at 0x7B182C).
        return FALSE;
    }

    INT nTileSize = pRes->GetTileSize();
    CSize blitSize(nTileSize, nTileSize);

    CRect rTileRect(x, y, x + nTileSize, y + nTileSize);

    INT nSkipTop = rClip.top - rTileRect.top;
    if (nSkipTop > 0) {
        rTileRect.top += nSkipTop;
        pData += nTileSize * nSkipTop;
        blitSize.cy -= nSkipTop;
    }

    INT nSkipBottom = rTileRect.bottom - rClip.bottom;
    if (nSkipBottom > 0) {
        blitSize.cy -= nSkipBottom;
        rTileRect.bottom -= nSkipBottom;
    }

    INT nSkipRight = rTileRect.right - rClip.right;
    if (nSkipRight > 0) {
        blitSize.cx -= nSkipRight;
        rTileRect.right -= nSkipRight;
    }

    INT nSkipLeft = rClip.left - rTileRect.left;
    if (nSkipLeft > 0) {
        blitSize.cx -= nSkipLeft;
        pData += nSkipLeft;
        rTileRect.left += nSkipLeft;
        nDataJump = nSkipLeft;
    }

    if (rTileRect.IsRectEmpty()) {
        pRes->Release();
        return FALSE;
    }

    DDSURFACEDESC ddsd;
    HRESULT hr;

    do {
        ddsd.dwSize = sizeof(ddsd);
        hr = pSurface->Lock(&rTileRect, &ddsd, DDLOCK_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            g_pChitin->GetCurrentVideoMode()->RestoreSurfaces();
        }
    } while (hr == DDERR_SURFACELOST || hr == DDERR_SURFACEBUSY);

    if (hr != DD_OK) {
        pRes->Release();
        return FALSE;
    }

    m_cPalette.SetPalette(pRes->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);

    BOOL bSuccess;

    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        m_cPalette.Realize(rgbTempPal,
            16,
            dwFlags | 0x400000,
            &m_paletteAffects,
            255);
        bSuccess = BltTile8To16(reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    case 24:
        m_cPalette.Realize(rgbTempPal,
            24,
            dwFlags | 0x400000,
            &m_paletteAffects,
            255);
        bSuccess = BltTile8To24(reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    case 32:
        m_cPalette.Realize(rgbTempPal,
            32,
            dwFlags | 0x400000,
            &m_paletteAffects,
            255);
        bSuccess = BltTile8To32(reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    default:
        bSuccess = FALSE;
        break;
    }

    pSurface->Unlock(ddsd.lpSurface);

    pRes->Release();

    // The blit's result (mov eax,esi at 0x7B1B1C).
    return bSuccess;
}

// 0x7B1B30
BOOL CVidTile::Render(LPDIRECTDRAWSURFACE pSurface, INT x, INT y, CResTile* pResStencil, const CRect& rClip, DWORD dwFlags)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawTile(*this, { x, y }, dwFlags)) return TRUE;
    LONG nDataJump = 0;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 6325
    UTIL_ASSERT(pSurface != NULL);

    if (pRes == NULL) {
        return FALSE;
    }

    if (rClip.IsRectEmpty()) {
        return FALSE;
    }

    // Both tiles are demanded (0x7B1BA8 and 0x7B1BB6); each is released
    // on the way out.
    pRes->Demand();
    pResStencil->Demand();

    BYTE* pData = pRes->GetTileData();
    if (pData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 6344
        UTIL_ASSERT_MSG(FALSE, "Tile data accessed when not loaded");

        // FALSE without releasing either demand, as the original.
        return FALSE;
    }

    BYTE* pStencilData = pResStencil->GetTileData();
    if (pStencilData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 6351
        UTIL_ASSERT_MSG(FALSE, "Tile stencil data accessed when not loaded");

        pRes->Release();
        pResStencil->Release();
        return FALSE;
    }

    INT nTileSize = pRes->GetTileSize();
    CSize blitSize(nTileSize, nTileSize);

    CRect rTileRect(x, y, x + nTileSize, y + nTileSize);

    INT nSkipTop = rClip.top - rTileRect.top;
    if (nSkipTop > 0) {
        rTileRect.top += nSkipTop;
        pData += nTileSize * nSkipTop;
        blitSize.cy -= nSkipTop;
    }

    INT nSkipBottom = rTileRect.bottom - rClip.bottom;
    if (nSkipBottom > 0) {
        blitSize.cy -= nSkipBottom;
        rTileRect.bottom -= nSkipBottom;
    }

    INT nSkipRight = rTileRect.right - rClip.right;
    if (nSkipRight > 0) {
        blitSize.cx -= nSkipRight;
        rTileRect.right -= nSkipRight;
    }

    INT nSkipLeft = rClip.left - rTileRect.left;
    if (nSkipLeft > 0) {
        blitSize.cx -= nSkipLeft;
        pData += nSkipLeft;
        rTileRect.left += nSkipLeft;
        nDataJump = nSkipLeft;
    }

    if (rTileRect.IsRectEmpty()) {
        pRes->Release();
        pResStencil->Release();
        return FALSE;
    }

    DDSURFACEDESC ddsd;
    HRESULT hr;

    do {
        ddsd.dwSize = sizeof(ddsd);
        hr = pSurface->Lock(&rTileRect, &ddsd, DDLOCK_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            g_pChitin->GetCurrentVideoMode()->RestoreSurfaces();
        }
    } while (hr == DDERR_SURFACELOST || hr == DDERR_SURFACEBUSY);

    if (hr != DD_OK) {
        pRes->Release();
        pResStencil->Release();
        return FALSE;
    }

    m_cPalette.SetPalette(pRes->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);

    BOOL bSuccess;

    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        m_cPalette.Realize(rgbTempPal,
            16,
            dwFlags | 0x400000,
            &m_paletteAffects,
            255);
        bSuccess = BltStencilTile8To16(reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            pStencilData,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    case 24:
        m_cPalette.Realize(rgbTempPal,
            24,
            dwFlags | 0x400000,
            &m_paletteAffects,
            255);
        bSuccess = BltStencilTile8To24(reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            pStencilData,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    case 32:
        m_cPalette.Realize(rgbTempPal,
            32,
            dwFlags | 0x400000,
            &m_paletteAffects,
            255);
        bSuccess = BltStencilTile8To32(reinterpret_cast<DWORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            pStencilData,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    default:
        // NOTE: The original releases only one tile here (unreachable: the
        // depth is 16, 24 or 32).
        bSuccess = FALSE;
        break;
    }

    pSurface->Unlock(ddsd.lpSurface);

    pRes->Release();
    pResStencil->Release();

    // The blit's result, as in the plain tile's Render.
    return bSuccess;
}

// 0x7B1F70
BOOL CVidTile::Render(LPDIRECTDRAWSURFACE pSurface, INT x, INT y, const CRect& rClip, CResTile* pResDualTile, BYTE nDitherSeed, DWORD dwFlags)
{
    LONG nDataJump = 0;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 6526
    UTIL_ASSERT(pSurface != NULL);

    if (pRes == NULL) {
        return FALSE;
    }

    if (rClip.IsRectEmpty()) {
        return FALSE;
    }

    pRes->Demand();
    pResDualTile->Demand();

    // Unlike the plain Render there is no assertion on the returned data:
    // the original fetches both tiles' data and blits whatever it gets.
    BYTE* pData = pRes->GetTileData();
    BYTE* pDualData = pResDualTile->GetTileData();

    INT nTileSize = pRes->GetTileSize();
    CSize blitSize(nTileSize, nTileSize);

    CRect rTileRect(x, y, x + nTileSize, y + nTileSize);

    // As in the plain Render, but every data skip moves BOTH tiles' data.
    INT nSkipTop = rClip.top - rTileRect.top;
    if (nSkipTop > 0) {
        rTileRect.top += nSkipTop;
        pData += nTileSize * nSkipTop;
        pDualData += nTileSize * nSkipTop;
        blitSize.cy -= nSkipTop;
    }

    INT nSkipBottom = rTileRect.bottom - rClip.bottom;
    if (nSkipBottom > 0) {
        blitSize.cy -= nSkipBottom;
        rTileRect.bottom -= nSkipBottom;
    }

    INT nSkipRight = rTileRect.right - rClip.right;
    if (nSkipRight > 0) {
        blitSize.cx -= nSkipRight;
        rTileRect.right -= nSkipRight;
    }

    INT nSkipLeft = rClip.left - rTileRect.left;
    if (nSkipLeft > 0) {
        blitSize.cx -= nSkipLeft;
        pData += nSkipLeft;
        pDualData += nSkipLeft;
        rTileRect.left += nSkipLeft;
        nDataJump = nSkipLeft;
    }

    if (rTileRect.IsRectEmpty()) {
        pRes->Release();
        pResDualTile->Release();
        return FALSE;
    }

    DDSURFACEDESC ddsd;
    HRESULT hr;

    do {
        ddsd.dwSize = sizeof(ddsd);
        hr = pSurface->Lock(&rTileRect, &ddsd, DDLOCK_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            g_pChitin->GetCurrentVideoMode()->RestoreSurfaces();
        }
    } while (hr == DDERR_SURFACELOST || hr == DDERR_SURFACEBUSY);

    if (hr != DD_OK) {
        // Only this tile's demand is released (0x7B2190); the dual tile's
        // stays held, as in the original.
        pRes->Release();
        return FALSE;
    }

    BOOL bSuccess;

    // Both palettes are set up and realized inside the 16-bpp blitter,
    // not here (0x7B279A-0x7B27F6).
    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        bSuccess = BltDualTile8To16(reinterpret_cast<WORD*>(ddsd.lpSurface),
            ddsd.lPitch,
            pData,
            pDualData,
            pResDualTile,
            nDitherSeed,
            blitSize,
            nDataJump,
            dwFlags);
        break;
    case 24:
        // TODO: 0x7B1F70 calls 0x7CE3D0 (not ported).
        bSuccess = FALSE;
        break;
    case 32:
        // TODO: 0x7B1F70 calls 0x7CE930 (not ported).
        bSuccess = FALSE;
        break;
    default:
        // An unknown bpp unlocks and releases only this tile's demand
        // (0x7B21CC-0x7B21E0); the dual tile's stays held, as in the
        // original.
        pSurface->Unlock(ddsd.lpSurface);
        pRes->Release();
        return FALSE;
    }

    pSurface->Unlock(ddsd.lpSurface);
    pRes->Release();
    pResDualTile->Release();

    // The blit's result (mov eax, esi at 0x7B22C2).
    return bSuccess;
}

// 0x7B22E0
BOOL CVidTile::BltTile8To16(WORD* pSurface, LONG lPitch, BYTE* pData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nDataIndex = 0;
    DWORD color = g_pChitin->GetCurrentVideoMode()->field_24;
    INT nSurfaceJump = (lPitch >> 1) - blitSize.cx;
    INT nPairs = blitSize.cx >> 1;

    // Two pixels are written at a time as one DWORD.
    DWORD* pDest = reinterpret_cast<DWORD*>(pSurface);

    if ((dwFlags & 0x8) == 0) {
        for (INT y = blitSize.cy; y > 0; y--) {
            for (INT x = nPairs; x > 0; x--) {
                *pDest++ = (rgbTempPal[pData[nDataIndex + 1]] << 16) | (rgbTempPal[pData[nDataIndex]] & 0xFFFF);
                nDataIndex += 2;
            }
            nDataIndex += nDataJump;
            pDest = reinterpret_cast<DWORD*>(reinterpret_cast<WORD*>(pDest) + nSurfaceJump);
        }
    } else {
        // Dithered with the video mode's colour: even rows keep the first
        // pixel of each pair, odd rows the second.
        // NOTE: The odd rows OR in the whole colour, so its high word
        // reaches the kept pixel, as in the original (0x7B23A1).
        for (INT y = 0; y < blitSize.cy; y++) {
            if ((y & 1) == 0) {
                for (INT x = nPairs; x > 0; x--) {
                    *pDest++ = (rgbTempPal[pData[nDataIndex]] & 0xFFFF) | (color << 16);
                    nDataIndex += 2;
                }
            } else {
                for (INT x = nPairs; x > 0; x--) {
                    *pDest++ = (rgbTempPal[pData[nDataIndex + 1]] << 16) | color;
                    nDataIndex += 2;
                }
            }
            pDest = reinterpret_cast<DWORD*>(reinterpret_cast<WORD*>(pDest) + nSurfaceJump);
            nDataIndex += nDataJump;
        }
    }

    return TRUE;
}

// 0x7B2440
BOOL CVidTile::BltStencilTile8To16(WORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nDataIndex = 0;
    DWORD color = g_pChitin->GetCurrentVideoMode()->field_24;

    // Pixel pairs written as one DWORD; an odd width draws nothing.
    if ((blitSize.cx & 1) != 0) {
        return FALSE;
    }

    INT nPairs = blitSize.cx >> 1;
    DWORD* pDest = reinterpret_cast<DWORD*>(pSurface);

    for (INT y = 0; y < blitSize.cy; y++) {
        if ((y & 1) == 0) {
            // Even rows keep the first pixel of each pair; the second is
            // the colour's low word where the stencil is clear.
            for (INT x = nPairs; x > 0; x--) {
                if (pStencilData[nDataIndex + 1] == CVidPalette::CLEAR_ENTRY) {
                    *pDest = (rgbTempPal[pData[nDataIndex]] & 0xFFFF) | (color << 16);
                } else {
                    *pDest = (rgbTempPal[pData[nDataIndex + 1]] << 16) | (rgbTempPal[pData[nDataIndex]] & 0xFFFF);
                }
                pDest++;
                nDataIndex += 2;
            }
        } else {
            // Odd rows keep the second pixel; the first is the colour where
            // the stencil is clear, ORed in whole as in BltTile8To16.
            for (INT x = nPairs; x > 0; x--) {
                if (pStencilData[nDataIndex] == CVidPalette::CLEAR_ENTRY) {
                    *pDest = (rgbTempPal[pData[nDataIndex + 1]] << 16) | color;
                } else {
                    *pDest = (rgbTempPal[pData[nDataIndex + 1]] << 16) | (rgbTempPal[pData[nDataIndex]] & 0xFFFF);
                }
                pDest++;
                nDataIndex += 2;
            }
        }

        nDataIndex += nDataJump;
        pDest = reinterpret_cast<DWORD*>(reinterpret_cast<WORD*>(pDest) + ((lPitch >> 1) - blitSize.cx));
    }

    return TRUE;
}

// 0x7B25E0
BOOL CVidTile::BltDualTile8To16(WORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pDualData, CResTile* pResDualTile, BYTE nDitherSeed, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nWidth = blitSize.cx;
    INT nHeight = blitSize.cy;

    // The dual tile's palette, realized separately from this tile's.
    CVidPalette cDualPalette;

    // Each channel field is read off the video mode with its own NULL
    // guard (0 if there is none); the shifts are read as bytes at
    // +0xAA/+0xAE/+0xB2.
    CVidMode* pVideoMode = g_pChitin->GetCurrentVideoMode();
    DWORD nRMask = pVideoMode != NULL ? pVideoMode->m_dwRBitMask : 0;
    DWORD nGMask = pVideoMode != NULL ? pVideoMode->m_dwGBitMask : 0;
    DWORD nBMask = pVideoMode != NULL ? pVideoMode->m_dwBBitMask : 0;
    DWORD nRShift = pVideoMode != NULL ? pVideoMode->m_dwRBitShift : 0;
    DWORD nGShift = pVideoMode != NULL ? pVideoMode->m_dwGBitShift : 0;
    DWORD nBShift = pVideoMode != NULL ? pVideoMode->m_dwBBitShift : 0;

    if (m_pBlendTable == NULL) {
        // The original inlines the fill loop here (0x7B26E9-0x7B2756);
        // the tree keeps the one copy in BuildBlendTable.
        if (!BuildBlendTable()) {
            return FALSE;
        }
    }

    // The seed shifted down by three picks the dither phase, 0-31.
    nDitherSeed >>= 3;

    m_cPalette.SetPalette(pRes->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);
    cDualPalette.SetPalette(pResDualTile->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);

    m_cPalette.Realize(rgbTempPal,
        16,
        dwFlags | 0x400000,
        &m_paletteAffects,
        255);
    cDualPalette.Realize(rgbDualTempPal,
        16,
        dwFlags | 0x400000,
        &m_paletteAffects,
        255);

    INT nSurfaceJump = (lPitch >> 1) - nWidth;

    // Checked after the palettes are realized, as in the original
    // (0x7B2A15).
    if (nHeight <= 0) {
        return TRUE;
    }

    // Each output pixel blends this tile's and the dual tile's pixels at
    // the dither phase: channel value x from this tile, y from the dual
    // tile, looked up as blend table entry (x << 11) | (y << 5) | phase —
    // the (k, m, i) = (m * (64 - 2i) + k * 2i) >> 6 cube built by
    // BuildBlendTable, with this tile as the primary k.
    WORD* pDest = pSurface;
    BYTE* pRowData = pData;
    BYTE* pRowDualData = pDualData;

    for (INT y = nHeight; y > 0; y--) {
        for (INT x = 0; x < nWidth; x++) {
            WORD cPrimary = static_cast<WORD>(rgbTempPal[pRowData[x]]);
            WORD cDual = static_cast<WORD>(rgbDualTempPal[pRowDualData[x]]);

            INT nRed = (((cPrimary & nRMask) >> nRShift) << 6) + ((cDual & nRMask) >> nRShift);
            INT nGreen = (((cPrimary & nGMask) >> nGShift) << 6) + ((cDual & nGMask) >> nGShift);
            INT nBlue = (((cPrimary & nBMask) >> nBShift) << 6) + ((cDual & nBMask) >> nBShift);

            *pDest++ = static_cast<WORD>((m_pBlendTable[(nRed << 5) + nDitherSeed] << nRShift)
                | (m_pBlendTable[(nGreen << 5) + nDitherSeed] << nGShift)
                | (m_pBlendTable[(nBlue << 5) + nDitherSeed] << nBShift));
        }

        pRowData += nWidth + nDataJump;
        pRowDualData += nWidth + nDataJump;
        pDest += nSurfaceJump;
    }

    return TRUE;
}

// 0x7CE3D0
BOOL CVidTile::BltDualTile8To24(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pDualData, CResTile* pResDualTile, BYTE nDitherSeed, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nWidth = blitSize.cx;
    INT nHeight = blitSize.cy;

    // The dual tile's palette, realized separately from this tile's.
    CVidPalette cDualPalette;

    // Each channel field is read off the video mode with its own NULL
    // guard (0 if there is none); the shifts are read as bytes at
    // +0xAA/+0xAE/+0xB2.
    CVidMode* pVideoMode = g_pChitin->GetCurrentVideoMode();
    DWORD nRMask = pVideoMode != NULL ? pVideoMode->m_dwRBitMask : 0;
    DWORD nGMask = pVideoMode != NULL ? pVideoMode->m_dwGBitMask : 0;
    DWORD nBMask = pVideoMode != NULL ? pVideoMode->m_dwBBitMask : 0;
    DWORD nRShift = pVideoMode != NULL ? pVideoMode->m_dwRBitShift : 0;
    DWORD nGShift = pVideoMode != NULL ? pVideoMode->m_dwGBitShift : 0;
    DWORD nBShift = pVideoMode != NULL ? pVideoMode->m_dwBBitShift : 0;

    if (m_pBlendTable == NULL) {
        // The shared builder is called directly here (0x7CE4A8), not
        // inlined as in the 16-bpp blitter.
        if (!BuildBlendTable()) {
            return FALSE;
        }
    }

    // The seed shifted down by three picks the dither phase, 0-31.
    nDitherSeed >>= 3;

    m_cPalette.SetPalette(pRes->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);
    cDualPalette.SetPalette(pResDualTile->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);

    m_cPalette.Realize(rgbTempPal,
        24,
        dwFlags | 0x400000,
        &m_paletteAffects,
        255);
    cDualPalette.Realize(rgbDualTempPal,
        24,
        dwFlags | 0x400000,
        &m_paletteAffects,
        255);

    // No /4*4 rounding here, unlike BltTile8To24: the jump is
    // lPitch - 3 * width in bytes (0x7CE569-0x7CE576).
    INT nSurfaceJump = lPitch - 3 * nWidth;

    // Checked after the palettes are realized, as in the original
    // (0x7CE57A).
    if (nHeight <= 0) {
        return TRUE;
    }

    // The per-pixel blend is the 16-bpp blitter's: channel value x from
    // this tile, y from the dual tile, looked up as blend table entry
    // (x << 11) | (y << 5) | phase, this tile as the primary k.
    BYTE* pDest = reinterpret_cast<BYTE*>(pSurface);
    BYTE* pRowData = pData;
    BYTE* pRowDualData = pDualData;

    for (INT y = nHeight; y > 0; y--) {
        // The first nWidth - 1 pixels are stored as overlapping DWORDs:
        // four bytes are written at the cursor and the cursor moves three
        // (mov [eax-3] at 0x7CE6CB), so each pixel's fourth byte is
        // overwritten by its successor.
        for (INT x = 0; x < nWidth - 1; x++) {
            DWORD cPrimary = rgbTempPal[pRowData[x]];
            DWORD cDual = rgbDualTempPal[pRowDualData[x]];

            INT nRed = (((cPrimary & nRMask) >> nRShift) << 6) + ((cDual & nRMask) >> nRShift);
            INT nGreen = (((cPrimary & nGMask) >> nGShift) << 6) + ((cDual & nGMask) >> nGShift);
            INT nBlue = (((cPrimary & nBMask) >> nBShift) << 6) + ((cDual & nBMask) >> nBShift);

            *reinterpret_cast<DWORD*>(pDest) = (m_pBlendTable[(nRed << 5) + nDitherSeed] << nRShift)
                | (m_pBlendTable[(nGreen << 5) + nDitherSeed] << nGShift)
                | (m_pBlendTable[(nBlue << 5) + nDitherSeed] << nBShift);
            pDest += 3;
        }

        // The last pixel of the row is written unconditionally as a word
        // plus its third byte (0x7CE7D4-0x7CE7DA). The row-head branch at
        // 0x7CE5D0 skips the fast loop for a width of one and comes
        // straight here; it also skips the row-base advance, so a width
        // of zero would read index 0 (Render never passes an empty rect,
        // so the width is at least one).
        INT nTail = nWidth > 1 ? nWidth - 1 : 0;

        DWORD cPrimary = rgbTempPal[pRowData[nTail]];
        DWORD cDual = rgbDualTempPal[pRowDualData[nTail]];

        INT nRed = (((cPrimary & nRMask) >> nRShift) << 6) + ((cDual & nRMask) >> nRShift);
        INT nGreen = (((cPrimary & nGMask) >> nGShift) << 6) + ((cDual & nGMask) >> nGShift);
        INT nBlue = (((cPrimary & nBMask) >> nBShift) << 6) + ((cDual & nBMask) >> nBShift);

        DWORD rgb = (m_pBlendTable[(nRed << 5) + nDitherSeed] << nRShift)
            | (m_pBlendTable[(nGreen << 5) + nDitherSeed] << nGShift)
            | (m_pBlendTable[(nBlue << 5) + nDitherSeed] << nBShift);
        *reinterpret_cast<WORD*>(pDest) = static_cast<WORD>(rgb);
        pDest[2] = static_cast<BYTE>(rgb >> 16);
        pDest += 3 + nSurfaceJump;

        // NOTE: The original advances the source rows one pixel short:
        // the row base takes nWidth - 1 at 0x7CE5E3 and nDataJump at
        // 0x7CE7F0, where the 16- and 32-bpp blitters advance by
        // nWidth + nDataJump. Each row after the first reads its data
        // one pixel further left; reproduced as-is.
        pRowData += (nWidth - 1) + nDataJump;
        pRowDualData += (nWidth - 1) + nDataJump;
    }

    return TRUE;
}

// 0x7CE930
BOOL CVidTile::BltDualTile8To32(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pDualData, CResTile* pResDualTile, BYTE nDitherSeed, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nWidth = blitSize.cx;
    INT nHeight = blitSize.cy;

    // The dual tile's palette, realized separately from this tile's.
    CVidPalette cDualPalette;

    // Each channel field is read off the video mode with its own NULL
    // guard (0 if there is none); the shifts are read as bytes at
    // +0xAA/+0xAE/+0xB2.
    CVidMode* pVideoMode = g_pChitin->GetCurrentVideoMode();
    DWORD nRMask = pVideoMode != NULL ? pVideoMode->m_dwRBitMask : 0;
    DWORD nGMask = pVideoMode != NULL ? pVideoMode->m_dwGBitMask : 0;
    DWORD nBMask = pVideoMode != NULL ? pVideoMode->m_dwBBitMask : 0;
    DWORD nRShift = pVideoMode != NULL ? pVideoMode->m_dwRBitShift : 0;
    DWORD nGShift = pVideoMode != NULL ? pVideoMode->m_dwGBitShift : 0;
    DWORD nBShift = pVideoMode != NULL ? pVideoMode->m_dwBBitShift : 0;

    if (m_pBlendTable == NULL) {
        // The shared builder is called directly here (0x7CEA14), not
        // inlined as in the 16-bpp blitter.
        if (!BuildBlendTable()) {
            return FALSE;
        }
    }

    // The seed shifted down by three picks the dither phase, 0-31.
    nDitherSeed >>= 3;

    m_cPalette.SetPalette(pRes->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);
    cDualPalette.SetPalette(pResDualTile->m_pPalette,
        256,
        CVidPalette::TYPE_RESOURCE);

    m_cPalette.Realize(rgbTempPal,
        32,
        dwFlags | 0x400000,
        &m_paletteAffects,
        255);
    cDualPalette.Realize(rgbDualTempPal,
        32,
        dwFlags | 0x400000,
        &m_paletteAffects,
        255);

    // The jump is counted in DWORDs, like the 16-bpp blitter's halfword
    // count (lPitch >> 2 at 0x7CEAE1).
    INT nSurfaceJump = (lPitch >> 2) - nWidth;

    // Checked after the palettes are realized, as in the original
    // (0x7CEAE6).
    if (nHeight <= 0) {
        return TRUE;
    }

    // The per-pixel blend is the 16-bpp blitter's: channel value x from
    // this tile, y from the dual tile, looked up as blend table entry
    // (x << 11) | (y << 5) | phase, this tile as the primary k. A row
    // with no pixels writes nothing and still moves on.
    DWORD* pDest = pSurface;
    BYTE* pRowData = pData;
    BYTE* pRowDualData = pDualData;

    for (INT y = nHeight; y > 0; y--) {
        for (INT x = 0; x < nWidth; x++) {
            DWORD cPrimary = rgbTempPal[pRowData[x]];
            DWORD cDual = rgbDualTempPal[pRowDualData[x]];

            INT nRed = (((cPrimary & nRMask) >> nRShift) << 6) + ((cDual & nRMask) >> nRShift);
            INT nGreen = (((cPrimary & nGMask) >> nGShift) << 6) + ((cDual & nGMask) >> nGShift);
            INT nBlue = (((cPrimary & nBMask) >> nBShift) << 6) + ((cDual & nBMask) >> nBShift);

            *pDest++ = (m_pBlendTable[(nRed << 5) + nDitherSeed] << nRShift)
                | (m_pBlendTable[(nGreen << 5) + nDitherSeed] << nGShift)
                | (m_pBlendTable[(nBlue << 5) + nDitherSeed] << nBShift);
        }

        pRowData += nWidth + nDataJump;
        pRowDualData += nWidth + nDataJump;
        pDest += nSurfaceJump;
    }

    return TRUE;
}

namespace {

// A 24-bit pixel: the three low bytes of a realized palette entry.
inline void PutTilePixel24(BYTE*& pDest, DWORD rgb)
{
    pDest[0] = static_cast<BYTE>(rgb);
    pDest[1] = static_cast<BYTE>(rgb >> 8);
    pDest[2] = static_cast<BYTE>(rgb >> 16);
    pDest += 3;
}

}

// 0x7CDD00
BOOL CVidTile::BltTile8To24(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nDataIndex = 0;
    DWORD color = g_pChitin->GetCurrentVideoMode()->field_24;

    // Four pixels are packed into three DWORDs at a time; a width that is
    // not a multiple of four draws nothing.
    if ((blitSize.cx & 3) != 0) {
        return FALSE;
    }

    INT nGroups = blitSize.cx >> 2;
    // The row jump rounds toward zero to whole DWORDs (cdq/sar at
    // 0x7CDD4E).
    INT nSurfaceJump = lPitch - blitSize.cx * 3;
    nSurfaceJump = nSurfaceJump / 4 * 4;
    BYTE* pDest = reinterpret_cast<BYTE*>(pSurface);

    if ((dwFlags & 0x8) == 0) {
        for (INT y = blitSize.cy; y > 0; y--) {
            for (INT x = nGroups; x > 0; x--) {
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex]]);
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 1]]);
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 2]]);
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 3]]);
                nDataIndex += 4;
            }
            nDataIndex += nDataJump;
            pDest += nSurfaceJump;
        }
    } else {
        // Dithered with the video mode's colour: even rows keep pixels 0
        // and 2 of each four, odd rows 1 and 3.
        // NOTE: The original moves neither to the next row nor past the
        // data jump here: the rows follow on from each other in the surface
        // and the data (the loop at 0x7CDD83 to 0x7CDEA7).
        for (INT y = 0; y < blitSize.cy; y++) {
            for (INT x = nGroups; x > 0; x--) {
                if ((y & 1) == 0) {
                    PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex]]);
                    PutTilePixel24(pDest, color);
                    PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 2]]);
                    PutTilePixel24(pDest, color);
                } else {
                    PutTilePixel24(pDest, color);
                    PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 1]]);
                    PutTilePixel24(pDest, color);
                    PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 3]]);
                }
                nDataIndex += 4;
            }
        }
    }

    return TRUE;
}

// 0x7CDF90
BOOL CVidTile::BltStencilTile8To24(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nDataIndex = 0;
    DWORD color = g_pChitin->GetCurrentVideoMode()->field_24;

    if ((blitSize.cx & 3) != 0) {
        return FALSE;
    }

    INT nGroups = blitSize.cx >> 2;
    INT nSurfaceJump = lPitch - blitSize.cx * 3;
    nSurfaceJump = nSurfaceJump / 4 * 4;
    BYTE* pDest = reinterpret_cast<BYTE*>(pSurface);

    for (INT y = 0; y < blitSize.cy; y++) {
        for (INT x = nGroups; x > 0; x--) {
            if ((y & 1) == 0) {
                // Even rows: pixels 1 and 3 are the colour where the
                // stencil is clear.
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex]]);
                PutTilePixel24(pDest, pStencilData[nDataIndex + 1] == CVidPalette::CLEAR_ENTRY ? color : rgbTempPal[pData[nDataIndex + 1]]);
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 2]]);
                PutTilePixel24(pDest, pStencilData[nDataIndex + 3] == CVidPalette::CLEAR_ENTRY ? color : rgbTempPal[pData[nDataIndex + 3]]);
            } else {
                // Odd rows: pixels 0 and 2.
                PutTilePixel24(pDest, pStencilData[nDataIndex] == CVidPalette::CLEAR_ENTRY ? color : rgbTempPal[pData[nDataIndex]]);
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 1]]);
                PutTilePixel24(pDest, pStencilData[nDataIndex + 2] == CVidPalette::CLEAR_ENTRY ? color : rgbTempPal[pData[nDataIndex + 2]]);
                PutTilePixel24(pDest, rgbTempPal[pData[nDataIndex + 3]]);
            }
            nDataIndex += 4;
        }

        nDataIndex += nDataJump;
        pDest += nSurfaceJump;
    }

    return TRUE;
}

// 0x7CE830
BOOL CVidTile::BltTile8To32(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nSurfaceJump = lPitch - 4 * blitSize.cx;
    DWORD color = g_pChitin->GetCurrentVideoMode()->field_24;
    INT nRows = blitSize.cy;
    INT nColumns;

    if ((blitSize.cx & 1) != 0) {
        return FALSE;
    }

    if ((dwFlags & 0x8) == 0) {
        do {
            for (nColumns = blitSize.cx; nColumns != 0; nColumns--) {
                *pSurface++ = rgbTempPal[*pData++];
            }
            pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nSurfaceJump);
            pData += nDataJump;
            nRows--;
        } while (nRows != 0);
    } else {
        // Dithered with the video mode's colour, two rows at a time: the
        // first keeps the even pixels, the second the odd ones.
        do {
            nColumns = blitSize.cx;
            do {
                pSurface[0] = rgbTempPal[pData[0]];
                if (nColumns == 1) {
                    break;
                }
                pSurface[1] = color;
                pData += 2;
                pSurface += 2;
                nColumns -= 2;
            } while (nColumns != 0);
            pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nSurfaceJump);
            pData += nDataJump;

            if (nRows == 1) {
                return TRUE;
            }

            nColumns = blitSize.cx;
            do {
                pSurface[0] = color;
                if (nColumns == 1) {
                    break;
                }
                pSurface[1] = rgbTempPal[pData[1]];
                pData += 2;
                pSurface += 2;
                nColumns -= 2;
            } while (nColumns != 0);
            pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nSurfaceJump);
            pData += nDataJump;
            nRows -= 2;
        } while (nRows != 0);
    }

    return TRUE;
}

// 0x7CE290
BOOL CVidTile::BltStencilTile8To32(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags)
{
    INT nDataIndex = 0;
    INT nSurfaceJump = lPitch - 4 * blitSize.cx;
    DWORD color = g_pChitin->GetCurrentVideoMode()->field_24;

    if ((blitSize.cx & 1) != 0) {
        return FALSE;
    }

    for (int y = 0; y < blitSize.cy; y++) {
        if ((y & 1) != 0) {
            for (int x = 0; x < blitSize.cx / 2; x++) {
                if (pStencilData[nDataIndex] == CVidPalette::CLEAR_ENTRY) {
                    pSurface[0] = color;
                } else {
                    pSurface[0] = rgbTempPal[pData[nDataIndex]];
                }

                pSurface[1] = rgbTempPal[pData[nDataIndex + 1]];

                pSurface += 2;
                nDataIndex += 2;
            }
        } else {
            for (int x = 0; x < blitSize.cx / 2; x++) {
                pSurface[0] = rgbTempPal[pData[nDataIndex]];

                if (pStencilData[nDataIndex + 1] == CVidPalette::CLEAR_ENTRY) {
                    pSurface[1] = color;
                } else {
                    pSurface[1] = rgbTempPal[pData[nDataIndex + 1]];
                }

                pSurface += 2;
                nDataIndex += 2;
            }
        }

        pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nSurfaceJump);
        // The jump moves the index into both the tile and the stencil
        // (add ecx,edx at 0x7CE3A0).
        nDataIndex += nDataJump;
    }

    return TRUE;
}

// NOTE: Inlined.
BOOL CVidTile::BltStencilTile8to32_3d(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwAlpha, DWORD dwFlags)
{
    INT nDataIndex = 0;
    INT nSurfaceJump = lPitch - 4 * blitSize.cx;

    if ((blitSize.cx & 1) != 0) {
        return FALSE;
    }

    for (int y = 0; y < blitSize.cy; y++) {
        for (int x = 0; x < blitSize.cx; x++) {
            *pSurface = rgbTempPal[pData[nDataIndex]];
            if (pStencilData[nDataIndex] == CVidPalette::CLEAR_ENTRY) {
                *pSurface &= (dwAlpha << 24) | 0xFFFFFF;
            }
            pSurface++;
            nDataIndex++;
        }

        pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nSurfaceJump);
        pData += nDataJump;
        pStencilData += nDataJump;
    }

    return TRUE;
}

// 0x7C5F60
BOOL CVidTile::ReadyTexture(INT nTextureId, DWORD dwFlags, DWORD dwAlpha)
{
    if (nTextureId < 9 && nTextureId != 2) {
        return FALSE;
    }

    if (pRes == NULL) {
        return FALSE;
    }

    pRes->Demand();

    BYTE* pTileData = pRes->GetTileData();
    if (pTileData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
        // __LINE__: 1465
        UTIL_ASSERT_MSG(FALSE, "Tile data accessed when not loaded");
    }

    m_dwFlags = dwFlags;

    GLuint textures[1];
    textures[0] = static_cast<GLuint>(nTextureId);

    CVideo3d::glDeleteTextures(1, textures);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    INT nTileSize = pRes->GetTileSize();
    CSize blitSize(nTileSize, nTileSize);

    m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags | 0x400000, &m_paletteAffects, dwAlpha);

    BltTile8To32(reinterpret_cast<DWORD*>(m_pPixels), 256, pTileData, blitSize, 0, dwFlags);

    g_pChitin->cVideo.field_13E = nTextureId;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, nTextureId);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexImage2D(GL_TEXTURE_2D,
        0,
        g_pChitin->cVideo.field_136,
        nTileSize,
        nTileSize,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        m_pPixels);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    pRes->Release();

    return TRUE;
}

// 0x7C61D0
BOOL CVidTile::ReadyTexture(INT nTextureId, CResTile* pResStencil, DWORD dwFlags, DWORD dwAlpha)
{
    if (nTextureId < 9 && nTextureId != 2) {
        return FALSE;
    }

    if (pRes == NULL) {
        return FALSE;
    }

    pRes->Demand();
    pResStencil->Demand();

    BYTE* pTileData = pRes->GetTileData();
    if (pTileData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
        // __LINE__: 1564
        UTIL_ASSERT_MSG(FALSE, "Tile data accessed when not loaded");
    }

    BYTE* pStencilData = pResStencil->GetTileData();
    if (pStencilData == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
        // __LINE__: 1573
        UTIL_ASSERT_MSG(FALSE, "Tile stencil data accessed when not loaded");
    }

    m_dwFlags = dwFlags;

    GLuint textures[1];
    textures[0] = static_cast<GLuint>(nTextureId);

    CVideo3d::glDeleteTextures(1, textures);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    INT nTileSize = pRes->GetTileSize();
    CSize blitSize(nTileSize, nTileSize);

    m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags | 0x400000, &m_paletteAffects, dwAlpha);

    // NOTE: Uninline.
    BltStencilTile8to32_3d(reinterpret_cast<DWORD*>(m_pPixels),
        256,
        pTileData,
        pStencilData,
        blitSize,
        0,
        dwAlpha,
        dwFlags);

    g_pChitin->cVideo.field_13E = nTextureId;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, nTextureId);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexImage2D(GL_TEXTURE_2D,
        0,
        g_pChitin->cVideo.field_136,
        nTileSize,
        nTileSize,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        m_pPixels);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    pRes->Release();
    pResStencil->Release();

    return TRUE;
}

// 0x7C64F0
void CVidTile::RenderTexture(INT nTextureId, const CRect& rDest, INT x, INT y, DWORD dwFlags)
{
    float x1;
    float y1;
    float x2;
    float y2;

    if (nTextureId < 9 && nTextureId != 2) {
        return;
    }

    x1 = static_cast<float>(x);
    y1 = static_cast<float>(y);
    x2 = static_cast<float>(x) + 64.0f;
    y2 = static_cast<float>(y) + 64.0f;

    CVideo3d::glEnable(GL_TEXTURE_2D);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    g_pChitin->cVideo.field_13E = nTextureId;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, nTextureId);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x3) != 0) {
        CVideo3d::glEnable(GL_BLEND);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    x1 += CVideo3d::SUB_PIXEL_SHIFT;
    y1 += CVideo3d::SUB_PIXEL_SHIFT;
    x2 += CVideo3d::SUB_PIXEL_SHIFT;
    y2 += CVideo3d::SUB_PIXEL_SHIFT;

    CVideo3d::glBegin(GL_TRIANGLE_STRIP);
    CVideo3d::glTexCoord2f(0.0f, 0.0f);
    CVideo3d::glVertex3f(x1, y1, 0.0f);
    CVideo3d::glTexCoord2f(0.0f, 1.0f);
    CVideo3d::glVertex3f(x1, y2, 0.0f);
    CVideo3d::glTexCoord2f(1.0f, 0.0f);
    CVideo3d::glVertex3f(x2, y1, 0.0f);
    CVideo3d::glTexCoord2f(1.0f, 1.0f);
    CVideo3d::glVertex3f(x2, y2, 0.0f);
    CVideo3d::glEnd();
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x3) != 0) {
        CVideo3d::glDisable(GL_BLEND);
    }
}
