#include "CParticle.h"

#include "CChitin.h"
#include "CUtil.h"
#include "CVidMode.h"
#include "CVideo3d.h"

// 0x85EB60
const USHORT CParticle::CONNECT = 0;

// 0x85EB62
const USHORT CParticle::DOTS = 1;

// 0x85EB64
const USHORT CParticle::BLOB = 2;

// 0x85EB66
const USHORT CParticle::RESOLUTION_INC = 10;

// 0x85EB68
const USHORT CParticle::GRAVITY = 1024;

// 0x85EB6A
const WORD CParticle::CONSTANT_VELOCITY = 1;

// 0x85EB6C
const WORD CParticle::NORMAL_VELOCITY = 2;

// 0x85EB6E
const BYTE CParticle::AIRBORN = 0;

// 0x85EB6F
const BYTE CParticle::GROUNDED = 1;

// 0x85EB70
const BYTE CParticle::DEAD = 2;

// 0x85EB74
//
// NOTE: 0x7FFFFFFF in the original. Not `LONG_MAX`, which is 64-bit on
// LP64 targets and became -1 in a 32-bit `LONG`.
const LONG CParticle::LIFESPAN_INFINITE = 0x7FFFFFFF;

// 0x7C3500
CParticle::CParticle()
{
    m_pos.x = 0;
    m_pos.y = 0;
    m_pos.z = -1;
    m_vel.x = 0;
    m_vel.y = 0;
    m_vel.z = 0;
    m_nTimeStamp = 0;
    m_nTailLength = 0;
    m_nRenderTime = 0;
    m_bTag = FALSE;
    m_nGravity = GRAVITY;
    m_wType = BLOB;
    m_nLifeSpan = LIFESPAN_INFINITE;
    m_rgbColor = 0;
}

// 0x7C3540
CParticle::CParticle(LONG posX, LONG posY, LONG posZ, LONG velX, LONG velY, LONG velZ, COLORREF rgbInitColor, SHORT nTailLength, LONG nGravity, WORD wType)
{
    m_pos.x = posX;
    m_pos.y = posY;
    m_pos.z = posZ;
    m_vel.x = velX;
    m_vel.y = velY;
    m_vel.z = velZ;
    m_rgbColor = rgbInitColor;
    m_nTimeStamp = 0;
    m_nRenderTime = 0;
    m_nTailLength = nTailLength;
    m_bTag = FALSE;
    m_nGravity = nGravity;
    m_wType = wType;
    m_nLifeSpan = LIFESPAN_INFINITE;
}

// 0x7C35B0
CParticle::CParticle(const CParticle& particle)
{
    m_pos = particle.m_pos;
    m_vel = particle.m_vel;
    m_rgbColor = particle.m_rgbColor;
    m_nTimeStamp = particle.m_nTimeStamp;
    m_nTailLength = particle.m_nTailLength;
    m_nRenderTime = particle.m_nRenderTime;
    m_bTag = FALSE;
    m_nGravity = particle.m_nGravity;
    m_wType = particle.m_wType;
    m_nLifeSpan = particle.m_nLifeSpan;
}

// 0x7C3620
BYTE CParticle::AsynchronousUpdate()
{
    LONG nTmpZVel = 0;

    if (m_nLifeSpan--) {
        if ((m_wType & 0x1) != 0) {
            m_pos.z = max(m_pos.z - m_nGravity, 0);
        } else {
            nTmpZVel = m_vel.z - m_nGravity;
            m_pos.z = max(m_pos.z + nTmpZVel, 0);
        }

        if (m_pos.z > 0) {
            m_nTimeStamp++;
            m_pos.x += m_vel.x;
            m_pos.y += m_vel.y;
            m_vel.z = nTmpZVel;
            return AIRBORN;
        } else {
            if (m_nTailLength != 0) {
                m_nTailLength--;
                return GROUNDED;
            }
        }
    }

    return DEAD;
}

// 0x7C36A0
void CParticle::Render(LPVOID pSurface, LONG lPitch, const CRect& rClipRect, USHORT nFlag, USHORT nBlobSize)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();
    DWORD color = pVidMode->ReduceColor(m_rgbColor);
    LONG nXPrev = m_pos.x;
    LONG nYPrev = m_pos.y;
    LONG nZPrev = m_pos.z;
    LONG nZPrevVel = m_vel.z;
    SHORT nLoops = min(m_nTailLength, m_nTimeStamp) + 1;
    SHORT nBpp = g_pChitin->cVideo.Is3dAccelerated() ? 32 : g_pChitin->cVideo.GetBitsPerPixels();

    switch (nFlag) {
    case CONNECT:
        switch (nBpp) {
        case 16:
            if (nLoops > 0) {
                CRect rDrawClip(rClipRect.left, rClipRect.top, rClipRect.right - 1, rClipRect.bottom - 1);
                for (SHORT i = nLoops; i > 0; i--) {
                    pVidMode->DrawLine16((nXPrev - m_vel.x) >> RESOLUTION_INC,
                        (((nYPrev - m_vel.y) * 3 >> 2) - nZPrev + nZPrevVel) >> RESOLUTION_INC,
                        nXPrev >> RESOLUTION_INC,
                        ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC,
                        reinterpret_cast<WORD*>(pSurface),
                        lPitch >> 1,
                        rDrawClip,
                        static_cast<WORD>(color),
                        FALSE);
                    nXPrev -= m_vel.x;
                    nYPrev -= m_vel.y;
                    nZPrev -= nZPrevVel;
                    nZPrevVel += GRAVITY;
                }
            }
            break;
        case 24:
            if (nLoops > 0) {
                CRect rDrawClip(rClipRect.left, rClipRect.top, rClipRect.right - 1, rClipRect.bottom - 1);
                for (SHORT i = nLoops; i > 0; i--) {
                    pVidMode->DrawLine24((nXPrev - m_vel.x) >> RESOLUTION_INC,
                        (((nYPrev - m_vel.y) * 3 >> 2) - nZPrev + nZPrevVel) >> RESOLUTION_INC,
                        nXPrev >> RESOLUTION_INC,
                        ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC,
                        reinterpret_cast<BYTE*>(pSurface),
                        lPitch,
                        rDrawClip,
                        color,
                        FALSE);
                    nXPrev -= m_vel.x;
                    nYPrev -= m_vel.y;
                    nZPrev -= nZPrevVel;
                    nZPrevVel += GRAVITY;
                }
            }
            break;
        case 32:
            if (nLoops > 0) {
                CRect rDrawClip(rClipRect.left, rClipRect.top, rClipRect.right - 1, rClipRect.bottom - 1);
                pVidMode->DrawLine32((nXPrev - m_vel.x) >> RESOLUTION_INC,
                    (((nYPrev - m_vel.y) * 3 >> 2) - nZPrev + nZPrevVel) >> RESOLUTION_INC,
                    nXPrev >> RESOLUTION_INC,
                    ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC,
                    reinterpret_cast<DWORD*>(pSurface),
                    lPitch / 4,
                    rDrawClip,
                    color,
                    FALSE);
            }
            break;
        }
        break;
    case DOTS:
        switch (nBpp) {
        case 16:
            while (nLoops > 0) {
                CPoint pt(nXPrev >> RESOLUTION_INC, ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC);
                if (rClipRect.PtInRect(pt)) {
                    reinterpret_cast<WORD*>(pSurface)[(pt.y - rClipRect.top) * (lPitch >> 1) - rClipRect.left + pt.x]
                        = static_cast<WORD>(color);
                }
                nXPrev -= m_vel.x;
                nYPrev -= m_vel.y;
                nZPrev -= nZPrevVel;
                nZPrevVel += GRAVITY;
                nLoops--;
            }
            break;
        case 24:
            while (nLoops > 0) {
                CPoint pt(nXPrev >> RESOLUTION_INC, ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC);
                if (rClipRect.PtInRect(pt)) {
                    BYTE* pPixel = reinterpret_cast<BYTE*>(pSurface)
                        + (pt.x - rClipRect.left) * 3 + (pt.y - rClipRect.top) * lPitch;
                    *reinterpret_cast<WORD*>(pPixel) = static_cast<WORD>(color);
                    pPixel[2] = static_cast<BYTE>(color >> 16);
                }
                nXPrev -= m_vel.x;
                nYPrev -= m_vel.y;
                nZPrev -= nZPrevVel;
                nZPrevVel += GRAVITY;
                nLoops--;
            }
            break;
        case 32:
            while (nLoops > 0) {
                RenderDot32(reinterpret_cast<DWORD*>(pSurface),
                    lPitch / 4,
                    nXPrev >> RESOLUTION_INC,
                    (3 * nYPrev / 4 - nZPrev) >> RESOLUTION_INC,
                    rClipRect,
                    color);
                nXPrev -= m_vel.x;
                nYPrev -= m_vel.y;
                nZPrev -= nZPrevVel;
                nZPrevVel += GRAVITY;
                nLoops--;
            }
            break;
        }
        break;
    case BLOB:
        switch (nBpp) {
        case 16:
            while (nLoops > 0) {
                RenderBlob16(reinterpret_cast<WORD*>(pSurface),
                    lPitch >> 1,
                    nXPrev >> RESOLUTION_INC,
                    ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC,
                    rClipRect,
                    nBlobSize,
                    color);
                nXPrev -= m_vel.x;
                nYPrev -= m_vel.y;
                nZPrev -= nZPrevVel;
                nZPrevVel += GRAVITY;
                nLoops--;
            }
            break;
        case 24:
            while (nLoops > 0) {
                RenderBlob24(reinterpret_cast<BYTE*>(pSurface),
                    lPitch,
                    nXPrev >> RESOLUTION_INC,
                    ((nYPrev * 3) >> 2) - nZPrev >> RESOLUTION_INC,
                    rClipRect,
                    nBlobSize,
                    color);
                nXPrev -= m_vel.x;
                nYPrev -= m_vel.y;
                nZPrev -= nZPrevVel;
                nZPrevVel += GRAVITY;
                nLoops--;
            }
            break;
        case 32:
            while (nLoops > 0) {
                RenderBlob32(reinterpret_cast<DWORD*>(pSurface),
                    lPitch / 4,
                    nXPrev >> RESOLUTION_INC,
                    (3 * nYPrev / 4 - nZPrev) >> RESOLUTION_INC,
                    rClipRect,
                    nBlobSize,
                    color);
                nXPrev -= m_vel.x;
                nYPrev -= m_vel.y;
                nZPrev -= nZPrevVel;
                nZPrevVel += GRAVITY;
                nLoops--;
            }
            break;
        }
        break;
    }
}

// 0x7C3CC0
void CParticle::RenderBlob16(WORD* pSurface, LONG lPitch, LONG lX, LONG lY, const CRect& rClipRect, USHORT nBlobSize, DWORD dwColor)
{
    CRect rBlob;
    rBlob.left = 0;
    rBlob.top = 0;
    rBlob.right = rClipRect.right - rClipRect.left;
    rBlob.bottom = rClipRect.bottom - rClipRect.top;

    const LONG lXPos = lX - rClipRect.left;
    const LONG lYPos = lY - rClipRect.top;
    const WORD nColor = static_cast<WORD>(dwColor);

    switch (nBlobSize) {
    case 5:
        if (rBlob.PtInRect(CPoint(lXPos, lYPos + 1))) {
            pSurface[(lYPos + 1) * lPitch + lXPos] = nColor;
        }
        // fall through
    case 4:
        if (rBlob.PtInRect(CPoint(lXPos + 1, lYPos))) {
            pSurface[lYPos * lPitch + lXPos + 1] = nColor;
        }
        // fall through
    case 3:
        if (rBlob.PtInRect(CPoint(lXPos - 1, lYPos))) {
            pSurface[lYPos * lPitch + lXPos - 1] = nColor;
        }
        // fall through
    case 2:
        if (rBlob.PtInRect(CPoint(lXPos, lYPos - 1))) {
            pSurface[(lYPos - 1) * lPitch + lXPos] = nColor;
        }
        // fall through
    case 1:
        if (rBlob.PtInRect(CPoint(lXPos, lYPos))) {
            pSurface[lYPos * lPitch + lXPos] = nColor;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChParticle.cpp
        // __LINE__: 615
        UTIL_ASSERT(FALSE);
        break;
    }
}

// 0x7C3E20
void CParticle::RenderBlob24(BYTE* pSurface, LONG lPitch, LONG lX, LONG lY, const CRect& rClipRect, USHORT nBlobSize, DWORD dwColor)
{
    CRect rBlob;
    rBlob.left = 0;
    rBlob.top = 0;
    rBlob.right = rClipRect.right - rClipRect.left;
    rBlob.bottom = rClipRect.bottom - rClipRect.top;

    const LONG lXPos = lX - rClipRect.left;
    const LONG lYPos = lY - rClipRect.top;
    const BYTE nThird = static_cast<BYTE>(dwColor >> 16);
    const WORD nColor = static_cast<WORD>(dwColor);

    switch (nBlobSize) {
    case 5:
        if (rBlob.PtInRect(CPoint(lXPos, lYPos + 1))) {
            BYTE* p = pSurface + (lYPos + 1) * lPitch + lXPos * 3;
            *reinterpret_cast<WORD*>(p) = nColor;
            p[2] = nThird;
        }
        // fall through
    case 4:
        if (rBlob.PtInRect(CPoint(lXPos + 1, lYPos))) {
            BYTE* p = pSurface + lYPos * lPitch + (lXPos + 1) * 3;
            *reinterpret_cast<WORD*>(p) = nColor;
            p[2] = nThird;
        }
        // fall through
    case 3:
        if (rBlob.PtInRect(CPoint(lXPos - 1, lYPos))) {
            BYTE* p = pSurface + lYPos * lPitch + (lXPos - 1) * 3;
            *reinterpret_cast<WORD*>(p) = nColor;
            p[2] = nThird;
        }
        // fall through
    case 2:
        if (rBlob.PtInRect(CPoint(lXPos, lYPos - 1))) {
            BYTE* p = pSurface + (lYPos - 1) * lPitch + lXPos * 3;
            *reinterpret_cast<WORD*>(p) = nColor;
            p[2] = nThird;
        }
        // fall through
    case 1:
        if (rBlob.PtInRect(CPoint(lXPos, lYPos))) {
            BYTE* p = pSurface + lYPos * lPitch + lXPos * 3;
            *reinterpret_cast<WORD*>(p) = nColor;
            p[2] = nThird;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChParticle.cpp
        // __LINE__: 670
        UTIL_ASSERT(FALSE);
        break;
    }
}

// 0x7C3FD0
void CParticle::RenderBlob32(DWORD* pSurface, LONG lPitch, LONG lX, LONG lY, const CRect& rClipRect, USHORT nFlag, DWORD color)
{
    CRect rBlob;
    rBlob.left = 0;
    rBlob.top = 0;
    rBlob.right = rClipRect.right - rClipRect.left;
    rBlob.bottom = rClipRect.bottom - rClipRect.top;

    CPoint pt1;
    pt1.x = lX - rClipRect.left;
    pt1.y = lY - rClipRect.top + 1;
    if (rBlob.PtInRect(pt1)) {
        pSurface[pt1.x + lPitch * pt1.y] = color;
    }

    CPoint pt2;
    pt2.x = lX - rClipRect.left + 1;
    pt2.y = lY - rClipRect.top;
    if (rBlob.PtInRect(pt2)) {
        pSurface[pt2.x + lPitch * pt2.y] = color;
    }

    CPoint pt3;
    pt3.x = lX - rClipRect.left - 1;
    pt3.y = lY - rClipRect.top;
    if (rBlob.PtInRect(pt3)) {
        pSurface[pt3.x + lPitch * pt3.y] = color;
    }

    CPoint pt4;
    pt4.x = lX - rClipRect.left;
    pt4.y = lY - rClipRect.top - 1;
    if (rBlob.PtInRect(pt4)) {
        pSurface[pt4.x + lPitch * pt4.y] = color;
    }

    CPoint pt5;
    pt5.x = lX - rClipRect.left;
    pt5.y = lY - rClipRect.top;
    if (rBlob.PtInRect(pt5)) {
        pSurface[pt5.x + lPitch * pt5.y] = color;
    }
}

// 0x7CC340
void CParticle::Render3d(const CRect& rClipRect, const CRect& rLockedRect, USHORT nFlag, USHORT nBlobSize)
{
    LONG v1 = rClipRect.left - rLockedRect.left;
    LONG v2 = rClipRect.top - rLockedRect.top;
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();
    DWORD color = pVidMode->ReduceColor(m_rgbColor);
    LONG nXPrev = m_pos.x;
    LONG nYPrev = m_pos.y;
    LONG nZPrev = m_pos.z;
    LONG nZPrevVel = m_vel.z;
    SHORT nLoops = min(m_nTailLength, m_nTimeStamp) + 1;
    CPoint pt;
    INT nPointSize;

    switch (nFlag) {
    case CONNECT:
        pVidMode->DrawLine3d(((nXPrev - m_vel.x) >> RESOLUTION_INC) - v1,
            ((3 * (nYPrev - m_vel.y) / 4 - nZPrev + nZPrevVel) >> RESOLUTION_INC) - v2,
            nXPrev >> RESOLUTION_INC,
            (3 * nYPrev / 4 - nZPrev) >> RESOLUTION_INC,
            rLockedRect,
            m_rgbColor);
        break;
    case DOTS:
        CVideo3d::glDisable(GL_BLEND);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glDisable(GL_TEXTURE_2D);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glPointSize(static_cast<float>(1));
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glColor4f(static_cast<float>(GetRValue(m_rgbColor)) / 255.0f,
            static_cast<float>(GetGValue(m_rgbColor)) / 255.0f,
            static_cast<float>(GetBValue(m_rgbColor)) / 255.0f,
            1.0f);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glBegin(GL_POINTS);
        while (nLoops > 0) {
            pt.x = nXPrev >> RESOLUTION_INC;
            pt.y = ((3 * nYPrev) / 4 - nZPrev) >> RESOLUTION_INC;
            if (rClipRect.PtInRect(pt)) {
                // NOTE: Original code adds `CVideo3d::SUB_PIXEL_SHIFT`, however
                // it's value (0.2) is too small and due to rounding all points
                // (snowflakes) appear exactly between pixels (which results in
                // no snow at all).
                CVideo3d::glVertex3f(static_cast<float>(pt.x - v1) + 0.5f,
                    static_cast<float>(pt.y - v2) + 0.5f,
                    0.0f);
            }

            nXPrev -= m_vel.x;
            nYPrev -= m_vel.y;
            nZPrev -= nZPrevVel;
            nZPrevVel += GRAVITY;
            nLoops--;
        }
        CVideo3d::glEnd();
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
        break;
    case BLOB:
        CVideo3d::glDisable(GL_BLEND);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glDisable(GL_TEXTURE_2D);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glColor4f(static_cast<float>(GetRValue(m_rgbColor)) / 255.0f,
            static_cast<float>(GetGValue(m_rgbColor)) / 255.0f,
            static_cast<float>(GetBValue(m_rgbColor)) / 255.0f,
            1.0f);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        switch (nBlobSize) {
        case 3:
        case 4:
            nPointSize = 2;
            break;
        case 5:
            nPointSize = 3;
            break;
        default:
            nPointSize = 1;
            break;
        }

        CVideo3d::glPointSize(static_cast<float>(nPointSize));
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glBegin(GL_POINTS);
        while (nLoops > 0) {
            pt.x = nXPrev >> RESOLUTION_INC;
            pt.y = ((3 * nYPrev) / 4 - nZPrev) >> RESOLUTION_INC;
            if (rClipRect.PtInRect(pt)) {
                // NOTE: See above, but in case of blobs its only meaningful
                // for point size 1. Point sizes 2 and 3 are always visible.
                CVideo3d::glVertex3f(static_cast<float>(pt.x - v1) + 0.5f,
                    static_cast<float>(pt.y - v2) + 0.5f,
                    0.0f);
            }

            nXPrev -= m_vel.x;
            nYPrev -= m_vel.y;
            nZPrev -= nZPrevVel;
            nZPrevVel += GRAVITY;
            nLoops--;
        }
        CVideo3d::glEnd();
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
        break;
    }
}

// NOTE: Convenience.
void CParticle::RenderDot32(DWORD* pSurface, LONG lPitch, LONG lX, LONG lY, const CRect& rClipRect, DWORD color)
{
    CPoint pt;
    pt.x = lX;
    pt.y = lY;
    if (rClipRect.PtInRect(pt)) {
        pSurface[pt.x + lPitch * (pt.y - rClipRect.top) - rClipRect.left] = color;
    }
}
