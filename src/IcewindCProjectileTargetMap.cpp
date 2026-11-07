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

// 0x55A610
// Vtable slot 0.
// The service tick. The local scan map and due list destruct implicitly
// where the binary inlines their teardown.
void IcewindCProjectileTargetMap::Service()
{
    if (m_bDone == 1) {
        return;
    }
    if (m_serviceCountdown < 1) {
        std::map<LONG, CPoint> scan = GatherTargets();
        std::list<LONG> due = CollectDueStrikes(scan);
        Strike(due);
        m_serviceCountdown = m_servicePeriod;
    }
    m_serviceCountdown--;
}

// 0x55A890
// Merge the fresh scan into m_targets and collect the victims due a strike.
// Tracked entries that left the scan reset their in-range tick count; every
// scanned id is inserted on first sight ({0, 0} entry) and is due when its
// tick count hits a multiple of m_strikeInterval -- so on first contact --
// while it has strikes left; the tick count always advances.
std::list<LONG> IcewindCProjectileTargetMap::CollectDueStrikes(std::map<LONG, CPoint>& scan)
{
    std::list<LONG> due;

    std::map<LONG, IcewindCProjectileTargetEntry>::iterator it;
    for (it = m_targets.begin(); it != m_targets.end(); ++it) {
        if (scan.find(it->first) == scan.end()) {
            it->second.m_nTicksInRange = 0;
        }
    }

    std::map<LONG, CPoint>::iterator scanIt;
    for (scanIt = scan.begin(); scanIt != scan.end(); ++scanIt) {
        IcewindCProjectileTargetEntry& entry = m_targets[scanIt->first];
        if (entry.m_nStrikes < m_maxStrikesPerTarget
            && entry.m_nTicksInRange % m_strikeInterval == 0) {
            due.push_back(scanIt->first);
            entry.m_nStrikes++;
        }
        entry.m_nTicksInRange++;
    }

    return due;
}

// 0x55AB20
// Deliver the due strikes: skip the owner's source when m_bSkipSource, count
// every other one against the total cap (overflow latches m_bDone and stops
// servicing for good), then post the strike.
void IcewindCProjectileTargetMap::Strike(std::list<LONG>& due)
{
    CProjectile* pOwner = static_cast<CProjectile*>(m_pOwner);
    std::list<LONG>::iterator it;
    for (it = due.begin(); it != due.end(); ++it) {
        if (m_bSkipSource == 1 && *it == pOwner->m_sourceId) {
            continue;
        }
        m_nStrikes++;
        if (m_maxStrikesTotal < m_nStrikes) {
            m_bDone = 1;
            return;
        }
        DeliverStrike(*it);
    }
}

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

// 0x55ACC0
// The binary opens with the inlined VC6 std::map construction (shared _Nil
// sentinel at 0x8E28C0, its refcount at 0x8E28BC, _Lockit guard calls); our
// m_targets default construction stands for all of it. m_pOwner and m_nRange
// are left uninitialized, faithfully -- every embedding projectile ctor fills
// them right after this runs (the Whirlwind stores itself, range 70, period 3,
// interval 33, total cap 8, skip-source).
IcewindCProjectileTargetMap::IcewindCProjectileTargetMap()
{
    m_servicePeriod = 10;
    m_serviceCountdown = 0;
    m_strikeInterval = 10;
    m_maxStrikesPerTarget = 100000;
    m_maxStrikesTotal = 100000;
    m_nStrikes = 0;
    m_bDone = FALSE;
    m_bSkipSource = FALSE;
}

// 0x55AF40
// The wall subclass's ctor: byte-for-byte the base stream above (the
// inlined VC6 map construction stands for our m_targets the same way) plus
// the one store the base lacks -- the wall byte at +0x3A cleared -- and its
// own vtable stamp 0x84EEB0. m_pOwner, m_nRange, the wall length at +0x36
// and the corner offsets all stay uninitialized here; both wall ctors fill
// the first three right after (Wall of Moonlight 0x5802B0 and Wall of Fire
// 0x581CA0 stamp the length to 20), and the corners are only read by the
// wall's own gather 0x55B1B0, which arms them elsewhere.
IcewindCProjectileWallTargetMap::IcewindCProjectileWallTargetMap()
    : IcewindCProjectileTargetMap()
{
    field_3A = 0;
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
