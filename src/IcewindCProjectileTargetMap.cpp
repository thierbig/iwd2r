#include "IcewindCProjectileTargetMap.h"

#include "CBaldurChitin.h"
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
