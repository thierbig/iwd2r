#ifndef CVIDIMAGE_H_
#define CVIDIMAGE_H_

#include "mfc.h"

#include "platform/ddraw_shim.h"

#include "CVidPalette.h"

#define CVIDIMG_TRANSLUCENT 0x2

class CResTile;

class CVidImage {
public:
    static DWORD rgbTempPal[256];
    static DWORD rgbDualTempPal[256];
    static BYTE* m_pBlendTable;
    static INT m_nBlendTableRefs;

    static BOOL BuildBlendTable();

    CVidImage();
    ~CVidImage();
    COLORREF GetTintColor();
    void SetAddColor(COLORREF rgbNewColor);
    void SetTintColor(COLORREF rgbNewColor);

    void SetPolyColor(DWORD dwFlags);

    BOOL GetTint(DWORD& nRed, DWORD& nGreen, DWORD& nBlue, INT& nShiftBack, DWORD dwFlags);
    BOOL GetAdd(DWORD& nRed, DWORD& nGreen, DWORD& nBlue, INT& nShiftBack, DWORD dwFlags);
    BOOL GetLight(INT& nRed, INT& nGreen, INT& nBlue, DWORD dwFlags);

    /* 0000 */ INT m_nFxSurface;
    /* 0004 */ CVidPalette m_cPalette;
    /* 0028 */ CVIDIMG_PALETTEAFFECT m_paletteAffects;
};

class CVidTile : public CVidImage {
public:
    static const INT BYTES_PER_TEXEL;
    static BYTE m_pPixels[16384];

    CVidTile();
    ~CVidTile();
    void SetRes(CResTile* pNewResTiles);
    BOOL Render(LPDIRECTDRAWSURFACE pSurface, INT x, INT y, const CRect& rClip, DWORD dwFlags);
    BOOL Render(LPDIRECTDRAWSURFACE pSurface, INT x, INT y, CResTile* pResStencil, const CRect& rClip, DWORD dwFlags);

    BOOL BltTile8To16(WORD* pSurface, LONG lPitch, BYTE* pData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);
    BOOL BltStencilTile8To16(WORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);

    BOOL BltDualTile8To16(WORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pDualData, CResTile* pResDualTile, BYTE nDitherSeed, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);

    BOOL BltTile8To24(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);
    BOOL BltStencilTile8To24(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);

    BOOL BltTile8To32(DWORD* pSurface, LONG lPitch, BYTE* pData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);
    BOOL BltStencilTile8To32(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwFlags);

    BOOL BltStencilTile8to32_3d(DWORD* pSurface, LONG lPitch, BYTE* pData, BYTE* pStencilData, const CSize& blitSize, LONG nDataJump, DWORD dwAlpha, DWORD dwFlags);
    BOOL ReadyTexture(INT nTextureId, DWORD dwFlags, DWORD dwAlpha);
    BOOL ReadyTexture(INT nTextureId, CResTile* pResStencil, DWORD dwFlags, DWORD dwAlpha);
    void RenderTexture(INT nTextureId, const CRect& rDest, INT x, INT y, DWORD dwFlags);

    /* 00A0 */ CResTile* pRes;
    /* 00A4 */ DWORD m_dwFlags;
};

#endif /* CVIDIMAGE_H_ */
