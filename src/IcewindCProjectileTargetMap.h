#ifndef ICEWINDCPROJECTILETARGETMAP_H_
#define ICEWINDCPROJECTILETARGETMAP_H_

#include <list>
#include <map>

#include "mfc.h"

#include "CGameObject.h"

// IWD2 wandering-projectile striking: the five wandering projectiles embed
// this map by value (constructors 0x57D390, 0x57F390, 0x57F640, 0x5806C0,
// 0x580C00; the Whirlwind swords carry it at +0x2BA). Offset comments give
// the exe's binary layout (vtable 0x84EEA8, two slots; sizeof 0x34 with
// VC6's std::map at +0x08) and are informational only: our std::map lays
// nodes out differently, so members drift by name rather than by address.

struct IcewindCProjectileTargetEntry {
    /* 0000 */ LONG m_nTicksInRange;
    /* 0004 */ LONG m_nStrikes;
};

class IcewindCProjectileTargetMap {
public:
    IcewindCProjectileTargetMap(); // 0x55ACC0
    virtual void Service(); // 0x55A610, vtable 0x84EEA8 slot 0
    virtual std::map<LONG, CPoint> GatherTargets(); // 0x55AD90, slot 1
    std::list<LONG> CollectDueStrikes(std::map<LONG, CPoint>& scan); // 0x55A890

private:
    void Strike(std::list<LONG>& due); // 0x55AB20
    void DeliverStrike(LONG targetId); // 0x55AB80

public:
    /* 0004 */ CGameObject* m_pOwner;
    /* 0008 */ std::map<LONG, IcewindCProjectileTargetEntry> m_targets;
    /* 0018 */ LONG m_servicePeriod;
    /* 001C */ LONG m_serviceCountdown;
    /* 0020 */ LONG m_strikeInterval;
    /* 0024 */ LONG m_maxStrikesPerTarget;
    /* 0028 */ LONG m_maxStrikesTotal;
    /* 002C */ LONG m_nStrikes;
    /* 0030 */ BOOLEAN m_bDone;
    /* 0031 */ BOOLEAN m_bSkipSource;
    /* 0032 */ WORD m_nRange;
};

#endif /* ICEWINDCPROJECTILETARGETMAP_H_ */
