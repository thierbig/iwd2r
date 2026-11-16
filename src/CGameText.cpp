#include "CGameText.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameAnimationType.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CUtil.h"
#include "CVidMode.h"
#include "CWarp.h"

// 0x8D6570
const CSize CGameText::PADDING(0, 0);

// 0x4CB340
CGameText::CGameText(CGameArea* pArea, const CPoint& pt, BYTE nDuration, BYTE nBeginFade, const CString& sText)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameText.cpp
    // __LINE__: 110
    UTIL_ASSERT(pArea != NULL);

    m_nDuration = 0;
    m_nBeginFade = 0;
    m_nMaxLines = 0;
    m_objectType = TYPE_TEXT;
    m_szLine = NULL;
    m_nLines = 0;

    m_textFont.SetResRef(CResRef("INFOFONT"), FALSE, TRUE);
    m_vidCell.SequenceSet(0);
    m_vidCell.FrameSet(0);

    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE) == CGameObjectArray::SUCCESS) {
        AddToArea(pArea, pt, 0, LIST_FLIGHT);
        SetText(pt, nDuration, nBeginFade, sText);
        field_1C = 0;
    } else {
        // FIXME: Doesn't look cool.
        delete this;
    }
}

// 0x4CB4B0
void CGameText::SetText(const CPoint& pt, BYTE nDuration, BYTE nBeginFade, const CString& sText)
{
    CString sMutableText(sText);
    sMutableText.TrimRight();

    CSize size;
    if (m_vidCell.GetRes() != NULL) {
        m_vidCell.GetCurrentFrameSize(size, FALSE);
    } else {
        g_pBaldurChitin->GetCurrentVideoMode()->GetFXSize(size);
        size.cx = size.cx / 2 - 1;
        size.cy = size.cy / 2 - 1;
    }

    if (m_textFont.GetRes()->Demand() != NULL) {
        m_textFont.SetColor(RGB(255, 255, 255), RGB(0, 0, 0), TRUE);

        m_nMaxLines = static_cast<BYTE>((size.cy - 2 * PADDING.cy) / m_textFont.GetFontHeight(TRUE));

        if (m_szLine != NULL) {
            delete[] m_szLine;
        }

        m_szLine = new CString[m_nMaxLines];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameText.cpp
        // __LINE__: 222
        UTIL_ASSERT(m_szLine != NULL);

        m_nLines = CUtil::SplitString(&m_textFont,
            sMutableText,
            static_cast<WORD>(size.cx - 2 * PADDING.cx),
            m_nMaxLines,
            m_szLine,
            FALSE,
            TRUE,
            TRUE,
            -1);

        m_textFont.GetRes()->Release();
    }

    m_nDuration = 5 * nDuration * m_nLines;

    if (m_nDuration < 45) {
        m_nDuration = 65;
    }

    if (sText.GetLength() > 255) {
        m_nDuration = 500;
    }

    m_nBeginFade = nBeginFade;
    m_pos = pt;
}

// 0x4CBB20
void CGameText::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    CInfinity* pInfinity = &m_pArea->m_cInfinity;
    DWORD dwRenderFlags = CInfinity::FXPREP_CLEARFILL | 0x20001;
    BOOL bFadeOut = FALSE;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\
    // __LINE__: 550
    UTIL_ASSERT(pVidMode != NULL);

    if (m_nLines == 0) {
        return;
    }

    CRect rFXRect;
    CPoint ptReference;

    if (m_vidCell.GetRes() == NULL) {
        // No backing frame: the text box is half the FX surface, and a single
        // line is narrowed to the width it actually occupies.
        CSize size;
        pVidMode->GetFXSize(size);
        size.cy = size.cy / 2;
        size.cx = size.cx / 2;
        SetRect(&rFXRect, 0, 0, size.cx - 1, size.cy - 1);

        LONG nWidth = size.cx;
        if (m_nLines == 1) {
            nWidth = m_textFont.GetStringLength(m_szLine[0], FALSE);
        }

        ptReference.x = nWidth / 2;
        ptReference.y = (m_nLines * size.cy / m_nMaxLines) / 2;
    } else {
        bFadeOut = m_nDuration < m_nBeginFade;

        CSize frameSize;
        m_vidCell.GetCurrentCenterPoint(ptReference, FALSE);
        m_vidCell.GetCurrentFrameSize(frameSize, FALSE);
        SetRect(&rFXRect, 0, 0, frameSize.cx, frameSize.cy);
    }

    CRect rClip(PADDING.cx,
        PADDING.cy,
        rFXRect.right - PADDING.cx,
        rFXRect.bottom - PADDING.cy);

    // Cull against the world rect the viewport currently shows.
    CRect rText(m_pos.x - ptReference.x,
        m_pos.y - m_posZ - ptReference.y,
        m_pos.x + ptReference.x,
        m_pos.y - m_posZ + ptReference.y);

    CRect rView(pInfinity->nCurrentX,
        pInfinity->nCurrentY,
        pInfinity->nCurrentX + (pInfinity->rViewPort.right - pInfinity->rViewPort.left),
        pInfinity->nCurrentY + (pInfinity->rViewPort.bottom - pInfinity->rViewPort.top));

    if (!IntersectRect(&rView, &rText, &rView)) {
        return;
    }

    // Re-anchor on the owner every frame so the text tracks a moving sprite.
    // The vertical offset is in line units, biased by the owner's footprint.
    if (field_1C != 0) {
        CGameObject* pOwner;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(field_1C,
                CGameObjectArray::THREAD_ASYNCH,
                &pOwner,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        CPoint ptOwner = pOwner->GetPos();

        if (pOwner->GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pOwner);

            // __FILE__: .\Include\ObjAnimation.h
            // __LINE__: 2115
            UTIL_ASSERT(pSprite->m_animation.m_animation != NULL);

            if (pSprite->m_animation.m_animation->m_animationID == ANIM_BEETLE_RHINOCEROUS) {
                m_pos.x = ptOwner.x;
                m_pos.y = (m_nLines - 20) * 5 + ptOwner.y;
            } else {
                BYTE nPersonalSpace = pSprite->m_animation.GetPersonalSpace();
                m_pos.x = ptOwner.x;
                m_pos.y = (m_nLines - nPersonalSpace * 7) * 5 + ptOwner.y;
            }
        } else if (pOwner->GetObjectType() != CGameObject::TYPE_TRIGGER) {
            m_pos.x = ptOwner.x;
            m_pos.y = (-3 - m_nLines) * 5 + ptOwner.y;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(field_1C,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    // The 50% dark box behind the text: the text bounds padded 4 wide / 2
    // tall around the reference point (0x4CBED1-0x4CBF22).
    CRect rDarken(m_pos.x - ptReference.x - 4,
        m_pos.y - m_posZ - ptReference.y - 2,
        m_pos.x + ptReference.x + 4,
        m_pos.y - m_posZ + ptReference.y + 2);
    pInfinity->DarkenRect(rDarken);

    CPoint ptPos(m_pos.x, m_pos.y - m_posZ);
    pInfinity->FXPrep(rFXRect, dwRenderFlags, a3, ptPos, ptReference);

    if (pInfinity->FXLock(rFXRect, dwRenderFlags)) {
        if (m_vidCell.GetRes() != NULL) {
            pInfinity->FXRender(&m_vidCell, ptReference.x, ptReference.y, dwRenderFlags, 0);
        }

        if (m_textFont.GetRes()->Demand() != NULL) {
            for (BYTE nLine = 0; nLine < m_nLines; nLine++) {
                SHORT nFontHeight = m_textFont.GetFontHeight(TRUE);
                SHORT nBaseLine = m_textFont.GetBaseLineHeight(TRUE);

                pInfinity->FXTextOut(&m_textFont,
                    m_szLine[nLine],
                    PADDING.cx,
                    nBaseLine + PADDING.cy + nFontHeight * nLine,
                    rClip,
                    dwRenderFlags,
                    TRUE);
            }

            m_textFont.GetRes()->Release();
        }

        if (bFadeOut) {
            pInfinity->FXUnlock(dwRenderFlags,
                &rFXRect,
                CPoint(m_pos.x + ptReference.x, m_pos.y + ptReference.y));
        } else {
            pInfinity->FXUnlock(dwRenderFlags, NULL, CPoint(0, 0));
        }

        pInfinity->FXBltFrom(a3,
            rFXRect,
            m_pos.x,
            m_pos.y - m_posZ,
            ptReference.x,
            ptReference.y,
            dwRenderFlags | 1);
    }
}

// 0x4CB820
//
// Moves the text to pt, then by the amount it sticks out of the viewport.
void CGameText::SetPositionInView(const CPoint& pt)
{
    CVidMode* pVidMode = g_pBaldurChitin->pActiveEngine != NULL ? g_pBaldurChitin->pActiveEngine->pVidMode : NULL;
    const CInfinity& cInfinity = m_pArea->m_cInfinity;

    m_pos = pt;

    // The half extents: the frame's centre with a picture, else from the FX
    // size and the lines.
    CPoint ptHalf;
    if (m_vidCell.GetRes() != NULL) {
        CSize frameSize;
        m_vidCell.GetCurrentCenterPoint(ptHalf, FALSE);
        m_vidCell.GetCurrentFrameSize(frameSize, FALSE);
    } else {
        CSize size;
        pVidMode->GetFXSize(size);
        size.cx /= 2;
        size.cy /= 2;
        if (static_cast<BYTE>(m_nLines) == 1) {
            size.cx = m_textFont.GetStringLength(m_szLine[0], FALSE);
        }
        ptHalf.x = size.cx / 2;
        ptHalf.y = static_cast<BYTE>(m_nLines) * size.cy / m_nMaxLines / 2;
    }

    LONG nY = m_pos.y - m_posZ;
    LONG nLeft = m_pos.x - ptHalf.x;
    LONG nRight = m_pos.x + ptHalf.x;
    LONG nTop = nY - ptHalf.y;
    LONG nBottom = nY + ptHalf.y;

    LONG nViewLeft = cInfinity.nCurrentX;
    LONG nViewTop = cInfinity.nCurrentY;
    LONG nViewRight = cInfinity.rViewPort.right - cInfinity.rViewPort.left + nViewLeft;
    LONG nViewBottom = cInfinity.rViewPort.bottom - cInfinity.rViewPort.top + nViewTop;

    LONG dx = nLeft < nViewLeft ? nViewLeft - nLeft : 0;
    if (nRight > nViewRight) {
        dx = nViewRight - nRight;
    }

    // NOTE: A text below the viewport is moved further down (the original
    // adds the overhang, 0x4CB97C-0x4CB982); kept.
    LONG dy = 0;
    if (nBottom > nViewBottom) {
        dy = nBottom - nViewBottom;
    }
    if (nTop < nViewTop) {
        dy = nViewTop - nTop;
    }

    m_pos.x += dx;
    m_pos.y += dy;
}

// 0x4CB9B0
//
// Whether the text's box meets the viewport.
BOOL CGameText::IsOnScreen()
{
    if (static_cast<BYTE>(m_nLines) == 0) {
        return FALSE;
    }

    CVidMode* pVidMode = g_pBaldurChitin->pActiveEngine->pVidMode;
    const CInfinity& cInfinity = m_pArea->m_cInfinity;

    CPoint ptHalf;
    if (m_vidCell.GetRes() != NULL) {
        CSize frameSize;
        m_vidCell.GetCurrentCenterPoint(ptHalf, FALSE);
        m_vidCell.GetCurrentFrameSize(frameSize, FALSE);
    } else {
        CSize size;
        pVidMode->GetFXSize(size);
        size.cx /= 2;
        size.cy /= 2;
        LONG nWidth = size.cx;
        if (static_cast<BYTE>(m_nLines) == 1) {
            nWidth = m_textFont.GetStringLength(m_szLine[0], FALSE);
        }
        ptHalf.x = nWidth / 2;
        ptHalf.y = static_cast<BYTE>(m_nLines) * size.cy / m_nMaxLines / 2;
    }

    LONG nY = m_pos.y - m_posZ;
    CRect rText(m_pos.x - ptHalf.x, nY - ptHalf.y, m_pos.x + ptHalf.x, nY + ptHalf.y);
    CRect rView(cInfinity.nCurrentX,
        cInfinity.nCurrentY,
        cInfinity.rViewPort.right - cInfinity.rViewPort.left + cInfinity.nCurrentX,
        cInfinity.rViewPort.bottom - cInfinity.rViewPort.top + cInfinity.nCurrentY);
    return IntersectRect(&rView, &rText, &rView) != FALSE;
}

// 0x4CB700
BOOLEAN CGameText::DoAIUpdate(BOOLEAN active, LONG counter)
{
    return (counter & m_AISpeed) == (m_AISpeed & m_id);
}

// 0x4CB720
void CGameText::AIUpdate()
{
    if (m_nDuration == 0) {
        RemoveFromArea();
        return;
    }

    if (m_nDuration > 0) {
        m_nDuration--;
    }

    if (m_vidCell.GetRes() == NULL) {
        if (m_nDuration < m_nBeginFade) {
            LONG nScale = 144 * m_nDuration / m_nBeginFade + 100;
            m_textFont.SetColor(RGB(nScale, nScale, nScale), 0, TRUE);
        }
    }

    m_vidCell.FrameAdvance();
}

// 0x4CB7A0
void CGameText::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    // NOTE: This implementation is slightly different from other `CGameObject`
    // subclasses (looping until success).
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
            CGameObjectArray::THREAD_ASYNCH,
            NULL,
            INFINITE);
    } while (rc != CGameObjectArray::SUCCESS);

    delete this;
}

// 0x4CC0D0
CGameText::~CGameText()
{
    if (m_szLine != NULL) {
        delete[] m_szLine;
    }

    if (field_1C != 0) {
        CGameObject* pObject;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(field_1C,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pObject->field_1C = 0;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(field_1C,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x47C830
BOOLEAN CGameText::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}
