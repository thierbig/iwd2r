#include "CProjectile84E9C0.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CUtil.h"

// 0x535F20
CProjectile84E9C0::CProjectile84E9C0(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue)
{
    m_animation.SetResRef(CResRef(sAnimation), FALSE, TRUE, TRUE);

    if (sPalette.GetLength() == 0) {
        m_bPalette = FALSE;
    } else {
        m_palette.SetResRef(CResRef(sPalette), TRUE, TRUE);
        m_palette.m_bDoubleSize = FALSE;
        m_bPalette = TRUE;
    }

    m_animation.SequenceSet(0);
    m_animation.FrameSet(0);

    m_sound.Stop();
    m_sound.SetResRef(CResRef(sSound), TRUE, TRUE);
    if (m_sound.m_nLooping == 0) {
        m_sound.SetFireForget(TRUE);
    }
    m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
    m_fireSoundRef = sSound;

    m_nTransValue = nTransValue;
    m_dwFlags = dwFlags;
    m_bHasHeight = TRUE;
}

// 0x5362A0
CProjectile84E9C0::~CProjectile84E9C0()
{
}

// 0x536380
void CProjectile84E9C0::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileBAM::AIUpdate, the time stop caster is compared
    // with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    if (m_animation.IsEndOfSequence(FALSE)) {
        OnArrival();
        return;
    }

    m_animation.FrameAdvance();
    m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
}

// 0x5363F0
void CProjectile84E9C0::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
    // __LINE__: 9833
    UTIL_ASSERT(pVidMode != NULL);

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    CPoint ptPos;
    ptPos.x = m_pos.x;
    ptPos.y = m_pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y - m_posZ;

    CPoint ptReference;
    CSize frameSize;
    m_animation.GetCurrentCenterPoint(ptReference, FALSE);
    m_animation.GetCurrentFrameSize(frameSize, FALSE);

    CRect rFx(0, 0, frameSize.cx, frameSize.cy);

    CRect rBounds;
    rBounds.left = ptPos.x - ptReference.x;
    rBounds.top = ptPos.y - ptReference.y;
    rBounds.right = rFx.right - rFx.left + rBounds.left;
    rBounds.bottom = rFx.bottom - rFx.top + rBounds.top;

    CRect rVisible;
    rVisible.IntersectRect(rBounds, rView);
    if (rVisible.IsRectEmpty()) {
        return;
    }

    // NOTE: The original keeps the copy-from-back flag in m_dwFlags.
    if (m_dwFlags != 0) {
        m_dwFlags |= CInfinity::FXPREP_COPYFROMBACK;
    }

    pInfinity->FXPrep(rFx, m_dwFlags, a3, ptPos, ptReference);
    if (!pInfinity->FXLock(rFx, m_dwFlags)) {
        return;
    }

    if (!m_bPalette) {
        pInfinity->FXRender(&m_animation, ptReference.x, ptReference.y, m_dwFlags, m_nTransValue);
    } else {
        m_palette.GetRes()->Demand();
        m_animation.SetPalette(m_palette.GetRes()->GetColorTable(),
            m_palette.GetRes()->GetColorCount(),
            CVidPalette::TYPE_RESOURCE);
        pInfinity->FXRender(&m_animation, ptReference.x, ptReference.y, m_dwFlags, m_nTransValue);
        m_palette.GetRes()->Release();
    }

    pInfinity->FXRenderClippingPolys(ptPos.x, ptPos.y, 0, ptReference, rBounds, FALSE, m_dwFlags);
    pInfinity->FXUnlock(m_dwFlags, NULL, CPoint(0, 0));
    pInfinity->FXBltFrom(a3, rFx, ptPos.x, ptPos.y, ptReference.x, ptReference.y, m_dwFlags);
}

// 0x5366F0
void CProjectile84E9C0::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)nType;

    m_sourceId = source;
    m_targetId = target;
    m_pArea = pArea;

    DeliverEffects();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CPoint ptTarget;
    if (m_targetId == CGameObjectArray::INVALID_INDEX) {
        ptTarget = targetPos;
    } else {
        GetSourcePosition(m_targetId, ptTarget, FALSE);
    }

    m_sound.Play(ptTarget.x, ptTarget.y, 0, FALSE);

    ptTarget.y++;
    CGameObject::AddToArea(pArea, ptTarget, nHeight, CGameObject::LIST_FRONT);
}

// NOTE: Inlined at the DecodeProjectile type-45 arm (0x5201C0): the arm
// news 0x32C bytes, runs the CProjectile84E9C0 constructor and then stamps
// this class's vtable (0x520223) - which is what a derived constructor
// emits anyway - before re-storing an empty m_fireSoundRef.
CProjectile84D838::CProjectile84D838(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue)
    : CProjectile84E9C0(sAnimation, sSound, sPalette, dwFlags, nTransValue)
{
}

// 0x5367C0
void CProjectile84D838::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)nType;

    m_sourceId = source;
    m_targetId = target;
    m_pArea = pArea;

    DeliverEffects();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    // The jet anchors at the position it was given (no target-object
    // branch like the base Fire) and its loop state comes from the
    // projectile's own fire-sound loop flag.
    CGameObject::AddToArea(pArea, targetPos, nHeight, CGameObject::LIST_FRONT);

    // NOTE: Inlined PlaySound in the original.
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);
}
