#include "IcewindCProjectileTargetMap.h"

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CProjectile.h"

// 0x55AB80
void IcewindCProjectileTargetMap::DeliverStrike(LONG targetId)
{
    CGameObject* pObject = NULL;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(targetId,
        CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }
    if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        CProjectile* pOwner = static_cast<CProjectile*>(m_pOwner);
        pSprite->m_posOld = pSprite->GetPos();
        if (pOwner->IsTargetImmune(pSprite) != 1) {
            POSITION pos = pOwner->m_effectList.GetHeadPosition();
            while (pos != NULL) {
                CGameEffect* pEffect = pOwner->m_effectList.GetNext(pos);
                CMessageAddEffect* message = new CMessageAddEffect(pEffect->Copy(), pOwner->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        } else {
            CGameEffect::FeedBackImmuneToResource(pSprite, pOwner->m_sourceRes);
        }
    }
    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(targetId,
        CGameObjectArray::THREAD_ASYNCH, INFINITE);
}

// 0x55AD90
// The gather pass: every object of any type within m_nRange of the owner,
// front list (from the owner's own vert-list position) and back list, with
// line of sight through the owner's terrain table; deduplicated by object id
// (the CPoint mapped value stays default -- the service path never reads it).
std::map<LONG, CPoint> IcewindCProjectileTargetMap::GatherTargets()
{
    std::map<LONG, CPoint> targets;
    CTypedPtrList<CPtrList, LONG*> inRange(10);

    CProjectile* pOwner = static_cast<CProjectile*>(m_pOwner);
    pOwner->GetArea()->GetAllInRange(pOwner->GetVertListPos(),
        pOwner->GetPos(),
        CAIObjectType::ANYONE,
        m_nRange,
        pOwner->m_terrainTable,
        inRange,
        TRUE,
        FALSE);
    pOwner->GetArea()->GetAllInRangeBack(pOwner->GetPos(),
        CAIObjectType::ANYONE,
        m_nRange,
        pOwner->m_terrainTable,
        inRange,
        TRUE,
        FALSE,
        FALSE);

    POSITION pos = inRange.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(inRange.GetNext(pos)));
        targets.insert(std::make_pair(nId, CPoint(0, 0)));
    }

    return targets;
}
