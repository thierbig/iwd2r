#include "CProjectileArea.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameObject84D624.h"
#include "CGameObject84F0DC.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CParticle.h"
#include "CProjectileShaft.h"
#include "CUtil.h"
#include "IcewindCGameEffects.h"
#include "IcewindMisc.h"

#include <math.h>

// 0x56EDD0
CProjectileArea::CProjectileArea(WORD nRadius)
    : CProjectileSingleTarget(CResRef("SPMAGMIS"))
{
    m_explosion2.field_2C = 0;
    m_explosion2.field_2E = 0x7FFFFFFF;
    m_explosion3.m_pPoints = NULL;
    m_explosion3.field_30 = 0;
    m_explosion3.field_34 = 0;
    m_explosion3.field_35 = 0;
    m_explosion3.field_36 = 0;
    m_explosion3.field_38 = 250;
    m_explosion3.field_3C = 6;
    m_explosion3.field_40 = 30;
    m_explosion3.field_44 = 0;
    field_2B4 = 0;
    field_2BA = 0;
    field_2BC = 0;
    field_2FA = 0;
    field_2FE = 0;
    field_2FF = 0;
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
    m_nSecondaryType = 78;
    m_bExploded = FALSE;
    m_targetType.Set(CAIObjectType::ANYONE);
    m_nRadius = nRadius;
    m_bAnimation1 = FALSE;
    m_bAnimation2 = FALSE;
    m_bCheckForNonSprites = FALSE;
    m_nTriggerCountdown = 0;
    m_bTravels = FALSE;
    field_1DE = FALSE;
    m_nDuration = 45;
    m_nTriggerInterval = 10000;
    m_nHitModulus = 10;
}

// 0x56FAF0
void CProjectileArea::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileSingleTarget::AIUpdate, the time stop caster is
    // compared with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    if (!m_bExploded) {
        if (!m_bTravels) {
            m_pos.x = m_targetPos.x;
            m_pos.y = m_targetPos.y;
            OnArrival();
            return;
        }

        m_animation->FrameAdvance();

        LONG dy = m_targetPos.y - m_pos.y;
        LONG dx = m_targetPos.x - m_pos.x;
        LONG nStep = m_speed + 1;
        if (dy * dy * 16 / 9 + dx * dx <= nStep * nStep) {
            OnArrival();
            return;
        }

        UpdateBasedOnTargetPoint(m_targetPos.x, m_targetPos.y);

        if (m_bSparkle) {
            // A puff of sparkles at the projectile.
            CGameObject84D624* pSparkle = new CGameObject84D624(m_nSparkleColour,
                CGameObject84D624::TYPE_SPARKLE,
                CGameObject84D624::EXPLOSION_PUFFS);
            pSparkle->AddToArea(m_pArea, m_pos, m_posZ, CGameObject::LIST_FRONT);
        }

        m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
        return;
    }

    if (m_bAnimation1) {
        m_animation1.FrameAdvance();
    }

    if (m_bAnimation2) {
        m_animation2.FrameAdvance();
    }

    if (--m_nTriggerCountdown <= 0) {
        std::list<LONG> targets = GetTargets();
        ApplyToTargets(targets);
        m_nTriggerCountdown = m_nTriggerInterval;
    }

    if (!m_explosion2.m_sound.empty() && !m_loopingSound.IsSoundPlaying()) {
        m_loopingSound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }

    if (--m_nDuration <= 0) {
        RemoveSelf();
    }
}

// 0x56F3F0
void CProjectileArea::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    CProjectileSingleTarget::Render(pArea, pVidMode, a3);
}

// 0x56F820
void CProjectileArea::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)nHeight;
    (void)nType;

    if (pArea == NULL) {
        return;
    }

    m_sourceId = source;
    m_targetId = target;
    m_pArea = pArea;

    CGameObject* pSource;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (!m_effectList.IsEmpty()) {
        CGameEffect* pEffect = m_effectList.GetTail();
        if (pEffect != NULL) {
            m_nDuration = GetDuration(static_cast<BYTE>(pEffect->m_casterLevel));
        }
    }

    CPoint ptStart;
    GetSourcePosition(m_sourceId, ptStart, TRUE);
    if (!m_bTravels) {
        ptStart = targetPos;
    }

    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        LONG nSourceId = m_sourceId;
        delete this;

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nSourceId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }

    CGameObject::AddToArea(pArea,
        ptStart,
        DetermineHeight(static_cast<CGameSprite*>(pSource)),
        CGameObject::LIST_FRONT);
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);

    m_targetPos.x = targetPos.x;
    m_pos.x = ptStart.x;
    m_posExactX = ptStart.x << CParticle::RESOLUTION_INC;
    m_posExactY = ((ptStart.y << CParticle::RESOLUTION_INC) * 4) / 3;
    m_targetPos.y = targetPos.y;
    m_pos.y = ptStart.y;

    LONG dy = (m_targetPos.y * 4) / 3 - (m_pos.y * 4) / 3;
    LONG dx = m_targetPos.x - m_pos.x;
    LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

    if (nDistance == 0) {
        OnArrival();
    } else {
        m_deltaX = (dx << CParticle::RESOLUTION_INC) * m_speed / nDistance;
        m_deltaY = (dy << CParticle::RESOLUTION_INC) * m_speed / nDistance;
        m_nDirection = GetDirection(m_targetPos);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x56F410
void CProjectileArea::OnArrival()
{
    if (m_callBackProjectile != CGameObjectArray::INVALID_INDEX) {
        CProjectile* pProjectile;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_callBackProjectile,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        pProjectile->CallBack();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_callBackProjectile,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    m_bExploded = TRUE;
    field_1DE = FALSE;

    PlaySound(m_arrivalSoundRef, m_loopArrivalSound, TRUE);

    // The points the explosion's lingering pieces burn at. NOTE: The
    // original never deletes them here; the last piece does (none made, they
    // stay).
    if (!m_explosion3.m_animation.empty()) {
        m_explosion3.m_pPoints = new CProjectileAreaPoints();
    }

    if (!m_explosion2.m_animation.empty()
        || !m_explosion3.m_animation.empty()
        || !m_explosion1.m_animation.empty()) {
        // Its particles stop at walls.
        new CGameObject84F0DC(m_explosion1,
            m_explosion2,
            m_explosion3,
            m_pArea,
            m_pos,
            static_cast<SHORT>(m_nRadius),
            static_cast<BYTE>(m_speed),
            CGameObject84F0DC::WALL_STOP,
            static_cast<WORD>(m_nDuration));
    }

    if (!m_explosion1.m_sound.empty()) {
        CString sSound(m_explosion1.m_sound.c_str());
        m_explosionSound.SetResRef(CResRef(sSound), TRUE, TRUE);
        m_explosionSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
        m_explosionSound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }

    if (!m_explosion2.m_sound.empty()) {
        CString sSound(m_explosion2.m_sound.c_str());
        m_loopingSound.SetResRef(CResRef(sSound), TRUE, TRUE);
        m_loopingSound.SetLoopingFlag(TRUE);
        m_loopingSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
    }
}

// 0x78E730
void CProjectileArea::sub_88()
{
}

// 0x5703E0
LONG CProjectileArea::GetDuration(BYTE nUnknown)
{
    (void)nUnknown;
    return m_nDuration;
}

// 0x56FED0
std::list<LONG> CProjectileArea::GetTargets()
{
    CTypedPtrList<CPtrList, LONG*> found;

    m_pArea->GetAllInRange(m_posVertList,
        m_pos,
        m_targetType,
        m_nRadius,
        m_terrainTable,
        found,
        TRUE,
        m_bCheckForNonSprites);
    m_pArea->GetAllInRangeBack(m_pos,
        m_targetType,
        m_nRadius,
        m_terrainTable,
        found,
        TRUE,
        FALSE,
        m_bCheckForNonSprites);

    // Forget the targets that are no longer in range.
    std::map<LONG, LONG>::iterator it = m_targetHits.begin();
    while (it != m_targetHits.end()) {
        BOOL bFound = FALSE;
        POSITION pos = found.GetHeadPosition();
        while (pos != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(found.GetNext(pos)));
            if (nId == it->first) {
                bFound = TRUE;
                break;
            }
        }

        if (bFound) {
            ++it;
        } else {
            std::map<LONG, LONG>::iterator next = it;
            ++next;
            m_targetHits.erase(it);
            it = next;
        }
    }

    std::list<LONG> targets;

    POSITION pos = found.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(found.GetNext(pos)));

        if (m_targetHits.find(nId) == m_targetHits.end()) {
            m_targetHits[nId] = 0;
        }

        if (m_targetHits[nId] % m_nHitModulus == 0) {
            targets.push_back(nId);
        }

        m_targetHits[nId]++;
    }

    return targets;
}

// 0x5701B0
void CProjectileArea::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CreateSecondary(*it);
    }
}

// 0x5701E0
void CProjectileArea::CreateSecondary(LONG nTargetId)
{
    CGameObject* pTarget;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTargetId,
        CGameObjectArray::THREAD_ASYNCH,
        &pTarget,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (m_excludedTargets.find(pTarget->GetId()) != m_excludedTargets.end()) {
        // NOTE: The original returns without releasing the share
        // (0x57024A).
        return;
    }

    if ((pTarget->GetObjectType() & CGameObject::TYPE_AIBASE) != 0 || m_bCheckForNonSprites) {
        if (pTarget->GetObjectType() != CGameObject::TYPE_SPRITE) {
            pTarget->GetPos();
        }

        if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE
            && IcewindMisc::IsDead(static_cast<CGameSprite*>(pTarget)) == TRUE) {
            // NOTE: The original returns without releasing the share
            // (0x5702A6).
            return;
        }

        if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
            static_cast<CGameSprite*>(pTarget)->m_posOld = pTarget->GetPos();
        }

        CProjectile* pProjectile = CProjectile::DecodeProjectile(m_nSecondaryType + 1, NULL, 0);
        pProjectile->m_sourceRes = m_sourceRes;
        pProjectile->m_nSpellLevel = m_nSpellLevel;
        pProjectile->m_fireSoundRef = CResRef("");

        POSITION pos = m_effectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = m_effectList.GetNext(pos);
            pProjectile->AddEffect(pEffect->Copy());
        }

        pProjectile->Fire(m_pArea, m_id, nTargetId, pTarget->GetPos(), 50, 0);

        // NOTE: The original releases a share of the callback projectile
        // here (0x570384), which it never took; the target's share is
        // released below.
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_callBackProjectile,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x5704E0
void CProjectileArea::TargetEnemy(CGameAIBase* pCaster)
{
    if (pCaster != NULL) {
        if (pCaster->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ENEMY;
            m_targetType.Set(type);
        } else {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ALLY;
            m_targetType.Set(type);
        }
    }
}

// 0x5703F0
void CProjectileArea::TargetFriend(CGameAIBase* pCaster)
{
    if (pCaster != NULL) {
        if (pCaster->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ALLY;
            m_targetType.Set(type);
        } else {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ENEMY;
            m_targetType.Set(type);
        }
    }
}

// -----------------------------------------------------------------------------

// 0x571E80
CProjectileArea84F580::CProjectileArea84F580()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 200.
    : CProjectileArea(256)
{
    // NOTE: Inlined: the travel animation of a name that is not empty.
    const char* szTravel = "FirebaT";
    if (strlen(szTravel) != 0) {
        field_1DE = TRUE;
        SetTravelAnimation(CResRef(szTravel));
        m_bTravels = TRUE;
    } else {
        field_1DE = FALSE;
        m_bTravels = FALSE;
    }

    m_fireSoundRef = "TRA_06";
    m_visualEffect.sub_586A90(TRUE);

    m_explosion1.m_animation = "FirebaX";
    m_explosion1.m_sound = "RNG_M03";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion2.m_animation = "FirebaR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 20;

    m_explosion3.m_animation = "FirebaA";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerCountdown = 0;
    m_speed <<= 1;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_36 = TRUE;
    m_nTriggerInterval = 10000;
    m_nHitModulus = 10;
    m_nDuration = 45;
    field_1D8 = 16;
    m_nRadius = 200;
}

// 0x572080
void CProjectileArea::SetTravelAnimation(const CResRef& resRef)
{
    if (m_animation != NULL) {
        delete m_animation;
    }

    m_animation = new CVidCell(resRef, FALSE);
}

// 0x571310
CProjectileArea84F310::CProjectileArea84F310()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "CloudKX";
    m_explosion1.m_sound = "RNG_M01";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion2.m_animation = "CloudKR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 13;

    m_explosion3.m_animation = "CloudKA";
    m_explosion3.m_sound = "ARE_M02";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 500;
    m_nTriggerCountdown = 0;
    m_nDuration = 1000;
    field_1D8 = 1;
    m_nRadius = 150;
}

// -----------------------------------------------------------------------------

// 0x5714E0
CProjectileArea84F3AC::CProjectileArea84F3AC()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "DFogX";
    m_explosion1.m_sound = "RNG_M01";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion2.m_animation = "DFogR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 20;

    m_explosion3.m_animation = "DFogA";
    m_explosion3.m_sound = "ARE_M02";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerCountdown = 0;
    m_nDuration = 1100;
    field_1D8 = 1;
    m_nRadius = 150;
}

// 0x5716B0
void CProjectileArea84F3AC::CreateSecondary(LONG nTargetId)
{
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();

    CGameObject* pSource;
    if (pArray->GetShare(m_sourceId, CGameObjectArray::THREAD_ASYNCH, &pSource, INFINITE) != CGameObjectArray::SUCCESS) {
        return;
    }

    CGameObject* pTarget;
    if (pArray->GetShare(nTargetId, CGameObjectArray::THREAD_ASYNCH, &pTarget, INFINITE) != CGameObjectArray::SUCCESS) {
        pArray->ReleaseShare(m_sourceId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return;
    }

    if (pTarget->GetObjectType() != CGameObject::TYPE_SPRITE) {
        pArray->ReleaseShare(m_sourceId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pArray->ReleaseShare(nTargetId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return;
    }

    static_cast<CGameSprite*>(pTarget)->m_posOld = pTarget->GetPos();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(m_nSecondaryType + 1, NULL, 0);
    pProjectile->m_sourceRes = m_sourceRes;
    pProjectile->m_nSpellLevel = m_nSpellLevel;
    pProjectile->m_fireSoundRef = CResRef("");

    POSITION pos = m_effectList.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* pEffect = m_effectList.GetNext(pos);
        pProjectile->AddEffect(pEffect->Copy());
    }

    // NOTE: The original looks the target up in the hit counts here
    // (0x571885) and drops the result.
    pProjectile->Fire(m_pArea, m_id, nTargetId, pTarget->GetPos(), 50, 0);

    // NOTE: As in CProjectileArea::CreateSecondary, the original releases a
    // share of the callback projectile it never took (0x5718D4).
    pArray->ReleaseShare(m_callBackProjectile, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    pArray->ReleaseShare(nTargetId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    pArray->ReleaseShare(m_sourceId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
}

// -----------------------------------------------------------------------------

// 0x571CE0
CProjectileArea84F4E4::CProjectileArea84F4E4()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 200.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "EntangX";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion3.m_animation = "EntangA";
    m_explosion3.m_sound = "ARE_P01";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 450;
    m_nTriggerCountdown = 0;
    m_nDuration = 1000;
    m_nRadius = 200;
}

// -----------------------------------------------------------------------------

// 0x572110
CProjectileArea84F61C::CProjectileArea84F61C()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 70.
    : CProjectileArea(256)
{
    // NOTE: Inlined: the travel animation of a name that is not empty.
    const char* szTravel = "MagicStn";
    if (strlen(szTravel) != 0) {
        field_1DE = TRUE;
        SetTravelAnimation(CResRef(szTravel));
        m_bTravels = TRUE;
    } else {
        field_1DE = FALSE;
        m_bTravels = FALSE;
    }

    m_explosion1.m_animation = "FSeedsX";
    m_explosion1.m_sound = "EFF_P45";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    field_1D8 = 16;
    m_nRadius = 70;
}

// -----------------------------------------------------------------------------

// 0x572290
CProjectileArea84F6B8::CProjectileArea84F6B8()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 200.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "FStormX";
    m_explosion1.m_sound = "EFF_P45";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion3.m_animation = "FStormA";
    m_explosion3.m_sound = "ARE_P03";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerCountdown = 0;
    m_nDuration = 105;
    field_1D8 = 1;
    m_nRadius = 200;
}

// -----------------------------------------------------------------------------

// 0x572470
CProjectileArea84F754::CProjectileArea84F754()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 140.
    : CProjectileArea(256)
{
    m_explosion1.m_sound = "EFF_P16";

    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 140;
}

// 0x572540
void CProjectileArea84F754::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    // A column of flame at the point, then the area.
    CProjectileShaft* pShaft = new CProjectileShaft(CString("FStrikH"), CString(""), 0xFF, FALSE, FALSE, 28);
    pShaft->Fire(pArea, source, target, targetPos, nHeight, nType);

    CProjectileArea::Fire(pArea, source, target, targetPos, nHeight, nType);
}

// -----------------------------------------------------------------------------

// 0x572670
CProjectileArea84F7F0::CProjectileArea84F7F0()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 100.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "GreaseX";
    m_explosion1.m_sound = "EFF_M31b";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion3.m_animation = "GreaseA";
    m_explosion3.m_sound = "ARE_M01";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_38 = 1000;
    m_nDuration = 1000;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerCountdown = 0;
    field_1D8 = 1;
    m_nRadius = 100;
}

// -----------------------------------------------------------------------------

// 0x572E60
CProjectileArea84F9C4::CProjectileArea84F9C4()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "EnchanX";
    m_explosion1.m_sound = "ARE_M21";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 150;
}

// 0x572F90
void CProjectileArea84F9C4::ApplyToTargets(std::list<LONG>& targets)
{
    // The sprites in the list up to the 1st to 4th humanoid.
    std::list<LONG> held;
    INT nHumanoids = rand() % 4;
    INT nFound = 0;

    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            held.push_back(*it);
            if (IcewindMisc::IsHumanoid(static_cast<CGameSprite*>(pObject)) == TRUE
                && ++nFound == nHumanoids + 1) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                break;
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    CProjectileArea::ApplyToTargets(held);
}

// -----------------------------------------------------------------------------

// 0x574300
CProjectileArea84FEA4::CProjectileArea84FEA4()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "EnchanX";
    m_explosion1.m_sound = "ARE_M21";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 150;
}

// 0x574430
void CProjectileArea84FEA4::ApplyToTargets(std::list<LONG>& targets)
{
    // Sprites below level 5, in order, while their levels fit in 2 to 8
    // (rand() % 4 twice, plus 2).
    std::list<LONG> asleep;
    INT nFirst = rand() % 4;
    INT nSecond = rand() % 4;
    INT nLevels = nSecond + 2 + nFirst;

    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            BYTE nLevel = static_cast<CGameSprite*>(pObject)->m_derivedStats.GetLevel();
            if (nLevel < 5 && nLevels - nLevel >= 0) {
                asleep.push_back(*it);
                nLevels -= nLevel;
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    CProjectileArea::ApplyToTargets(asleep);
}

// -----------------------------------------------------------------------------

// 0x5747A0
CProjectileArea84FFDC::CProjectileArea84FFDC()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "SStoneA";
    m_explosion1.m_sound = "EFF_P48";

    m_explosion3.m_animation = "SStoneA";
    m_explosion3.m_sound = "ARE_P04";

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 500;
    m_nTriggerCountdown = 0;
    m_nDuration = 1200;
    m_nRadius = 150;
}

// -----------------------------------------------------------------------------

// 0x574B80
CProjectileArea8501B0::CProjectileArea8501B0()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 100.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "SCloudX";
    m_explosion1.m_sound = "RNG_M01";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion2.m_animation = "SCloudR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 13;

    m_explosion3.m_animation = "SCloudA";
    m_explosion3.m_sound = "ARE_M02";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_38 = 1000;
    m_nDuration = 1000;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerCountdown = 0;
    field_1D8 = 1;
    m_nRadius = 100;
}

// -----------------------------------------------------------------------------

// 0x574EB0
CProjectileArea8502E8::CProjectileArea8502E8()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_explosion1.m_animation = "WebX";
    m_explosion1.m_sound = "EFF_M19";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion3.m_animation = "WebA";
    m_explosion3.m_sound = "ARE_M03";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 450;
    m_nTriggerCountdown = 0;
    m_nDuration = 1500;
    m_nRadius = 150;
}

// -----------------------------------------------------------------------------

// 0x575EA0
CProjectileArea85072C::CProjectileArea85072C()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 200.
    : CProjectileArea(256)
{
    m_nTicks = 0;

    m_explosion1.m_animation = "DBreatH";
    m_explosion1.m_sound = "ARE_M21";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion3.m_animation = "DBreatA";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerCountdown = 0;
    field_1DE = FALSE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_36 = TRUE;
    m_explosion3.field_38 = 500;
    m_nTriggerInterval = 10000;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 200;
}

// 0x576040
void CProjectileArea85072C::AIUpdate()
{
    // It arrives on its seventh tick and works as an area from then on.
    m_nTicks++;
    if (m_nTicks == 7) {
        field_1DE = TRUE;
        CProjectileArea::OnArrival();
    }

    if (m_nTicks > 6) {
        CProjectileArea::AIUpdate();
    }
}

// 0x799CA0
void CProjectileArea85072C::OnArrival()
{
    // NOTE: An empty function; its own AIUpdate makes it arrive.
}

// 0x576D40
// NOTE: The same file-static helper IcewindCGameEffects.cpp has for the turn
// undead effect (the original inlines it into ApplyToTargets below).
static INT CalculateTurnUndeadFinalSourceRoll(CGameSprite* pSprite)
{
    INT nModifier = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetAbilityScoreModifier(pSprite->GetDerivedStats()->m_nCHR);
    INT nRoll = nModifier + 1 + CUtil::UtilRandInt(20, pSprite->GetDerivedStats()->m_nLuck);
    SHORT nTurnUndeadLevel = pSprite->GetTurnUndeadLevel();
    if (nRoll - 10 >= 0) {
        return (nRoll - 10) / 3 + nTurnUndeadLevel;
    }
    return (nRoll - 9) / 3 - 1 + nTurnUndeadLevel;
}

// 0x577380
// NOTE: A file-static of the original; it rewrites the list in place to the
// sprites whose level is above -1.
static void FilterTurnUndeadTargets(std::list<LONG>& targets)
{
    std::list<LONG> kept;

    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (pSprite->GetLevel() <= -1) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        // NOTE: The original calls GetLevel a second time here and drops the
        // result.
        pSprite->GetLevel();

        kept.push_back(*it);
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    targets = kept;
}

// 0x576CF0
CProjectileArea850A38::CProjectileArea850A38()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 300.
    : CProjectileArea(256)
{
    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 300;
}

// 0x576DC0
void CProjectileArea850A38::ApplyToTargets(std::list<LONG>& targets)
{
    CGameObject* pSourceObject;
    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
            CGameObjectArray::THREAD_ASYNCH,
            &pSourceObject,
            INFINITE)
        != CGameObjectArray::SUCCESS) {
        // NOTE: The original releases the share it failed to take.
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }

    if (pSourceObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }
    CGameSprite* pSource = static_cast<CGameSprite*>(pSourceObject);

    // The turn roll, then the destroying threshold: a d20, then two d6.
    INT nTurnRoll = CalculateTurnUndeadFinalSourceRoll(pSource);
    INT nThreshold = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetAbilityScoreModifier(pSource->GetDerivedStats()->m_nCHR)
        + CUtil::UtilRandInt(6, pSource->GetDerivedStats()->m_nLuck)
        + CUtil::UtilRandInt(6, pSource->GetDerivedStats()->m_nLuck)
        + 2;
    if (pSource->sub_763150(CGAMESPRITE_FEAT_IMPROVED_TURNING) == 1) {
        nThreshold += 2;
    }

    FilterTurnUndeadTargets(targets);

    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }
        CGameSprite* pTarget = static_cast<CGameSprite*>(pObject);

        if (IcewindMisc::IsUndead(pTarget) != TRUE) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        if (pTarget->GetDerivedStats()->m_spellStates[SPLSTATE_TURNED]) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        LONG nLevel = pTarget->GetLevel();
        if (nLevel > nTurnRoll || nLevel > nThreshold) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            continue;
        }

        SHORT nTurnUndeadLevel = pSource->GetTurnUndeadLevel();
        const char* szResRef;
        if (IcewindMisc::IsEvil(pSource) == TRUE) {
            szResRef = (nTurnUndeadLevel >= 2 * nLevel) ? "EffTU1" : "EffTU2";
        } else {
            szResRef = (nTurnUndeadLevel >= 2 * nLevel) ? "EffTU3" : "EffTU4";
        }

        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
            reinterpret_cast<const BYTE*>(szResRef),
            1);
        for (std::list<CGameEffect*>::iterator effectIt = effects.begin(); effectIt != effects.end(); ++effectIt) {
            (*effectIt)->m_casterLevel = nTurnUndeadLevel;
            if ((*effectIt)->m_effectID == 280) {
                (*effectIt)->m_numDice = m_sourceId;
            }
            CMessage* pMessage = new CMessageAddEffect(*effectIt, m_sourceId, *it);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// -----------------------------------------------------------------------------

// 0x577B20
CProjectileArea850C0C::CProjectileArea850C0C()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 150.
    : CProjectileArea(256)
{
    m_fireSoundRef = CResRef("TRA_04A");
    m_arrivalSoundRef = CResRef("CAS_P02");

    m_explosion1.m_animation = "AbjuraX";
    m_explosion1.m_sound = "ARE_M20";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_nRadius = 150;
}

// 0x577C70
void CProjectileArea850C0C::CreateSecondary(LONG nTargetId)
{
    CGameObject* pObject;
    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTargetId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE)
        != CGameObjectArray::SUCCESS) {
        return;
    }

    if (pObject->GetObjectType() != CGameObject::TYPE_SPRITE) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }

    // NOTE: Inlined CGameObject::InControl.
    if (pObject->InControl()) {
        CGameEffect* pEffect = IcewindMisc::CreateEffectRemoveAllOfType(pObject,
            ICEWIND_CGAMEEFFECT_GLOBEOFINVULNERABILITY,
            0);
        static_cast<CGameSprite*>(pObject)->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    CProjectileArea::CreateSecondary(nTargetId);
}

// -----------------------------------------------------------------------------

// 0x574970
CProjectileArea850078::CProjectileArea850078()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then the WORD at
    // 0x84F07C (275).
    : CProjectileArea(256)
{
    m_nRadius = 275;
}

// 0x5749A0
void CProjectileArea850078::ApplyToTargets(std::list<LONG>& targets)
{
    if (!targets.empty()) {
        CreateSecondary(targets.front());
        return;
    }

    CGameObject* pSource;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pSource);
        // NOTE: The sprite effect and intensity are the bytes at 0x85BB4B
        // (2) and 0x85BB4E (0).
        pSprite->StartSpriteEffect(2, 0, 30, FALSE);
        pSprite->FeedBack(CGameSprite::FEEDBACK_STATICCHARGEEND, 0, 0, 0, -1, 0, 0);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// -----------------------------------------------------------------------------

// 0x574A70
CProjectileArea850114::CProjectileArea850114()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then the WORD at
    // 0x84F084 (275).
    : CProjectileArea(256)
{
    m_nRadius = 275;
}

// 0x574AA0
void CProjectileArea850114::ApplyToTargets(std::list<LONG>& targets)
{
    CGameObject* pSource;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    CGameSprite* pSprite = NULL;
    if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
        pSprite = static_cast<CGameSprite*>(pSource);
    }

    if ((pSource->GetArea()->m_header.m_areaType & 0x1) != 0) {
        if (!targets.empty()) {
            CreateSecondary(targets.front());
        } else if (pSprite != NULL) {
            // NOTE: The sprite effect and intensity are the bytes at
            // 0x85BB4B (2) and 0x85BB4E (0).
            pSprite->StartSpriteEffect(2, 0, 30, FALSE);
            pSprite->FeedBack(CGameSprite::FEEDBACK_CALLLIGHTNINGEND, 0, 0, 0, -1, 0, 0);
        }
    } else {
        // NOTE: Indoors the original calls FeedBack on the source without
        // checking that it is a sprite (0x574B36). Bounded (ruling P4-10):
        // a source that is not a sprite gets no feedback.
        if (pSprite != NULL) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_CALLLIGHTNINGEND, 0, 0, 0, -1, 0, 0);
        }
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// -----------------------------------------------------------------------------

// 0x575B00
CProjectileArea8504BC::CProjectileArea8504BC(WORD nRadius)
    : CProjectileArea(nRadius)
{
}

// 0x575B30
void CProjectileArea8504BC::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        if (*it == m_sourceId) {
            targets.erase(it);
            break;
        }
    }

    CProjectileArea::ApplyToTargets(targets);
}

// -----------------------------------------------------------------------------

// 0x575CD0
CProjectileArea8505F4::CProjectileArea8505F4(WORD nRadius)
    : CProjectileArea(nRadius)
{
}

// 0x575CF0
void CProjectileArea8505F4::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            // NOTE: The original returns here, leaving the other targets
            // (0x575D36).
            return;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
            && !static_cast<CGameSprite*>(pObject)->GetDerivedStats()->m_spellStates.test(SPLSTATE_MOLD_TOUCH)) {
            CreateSecondary(*it);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            // One target only.
            return;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x575DD0
CProjectileArea850690::CProjectileArea850690(WORD nRadius)
    : CProjectileArea(nRadius)
{
}

// 0x575DF0
void CProjectileArea850690::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                && !static_cast<CGameSprite*>(pObject)->GetDerivedStats()->m_spellStates.test(SPLSTATE_SHROUD_OF_FLAME)) {
                CreateSecondary(*it);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// -----------------------------------------------------------------------------

// 0x5868C0
CProjectileAreaPoints::Point::Point(LONG x, LONG y, BYTE bTaken)
{
    this->x = x;
    this->y = y;
    m_bTaken = bTaken;
}

// 0x5868E0
CProjectileAreaPoints::CProjectileAreaPoints()
{
    m_nRefCount = 0;
    m_points.reserve(512);
}

// -----------------------------------------------------------------------------

// 0x56FEC0
LONG CProjectileAreaExplosion3::GetChanceRange()
{
    return 10000;
}
