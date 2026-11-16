#ifndef ICEWINDCPROJECTILETARGETMAP_H_
#define ICEWINDCPROJECTILETARGETMAP_H_

#include <list>
#include <map>

#include "mfc.h"

#include "CGameObject.h"

// IWD2 wandering-projectile striking: the five wandering projectiles embed
// this map by value (constructors 0x57D390, 0x57F390, 0x57F640, 0x5806C0,
// 0x580C00; the Whirlwind swords carry it at +0x2BA), and the two
// stationary walls embed the derived wall variant below instead (Wall of
// Moonlight 0x5802B0 and Wall of Fire 0x581CA0, both at +0x2B4). Offset
// comments give the exe's binary layout (vtable 0x84EEA8, two slots;
// sizeof 0x34 with VC6's std::map at +0x08) and are informational only:
// our std::map lays nodes out differently, so members drift by name
// rather than by address.

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

// The wall variant: the stationary walls (Wall of Moonlight, ctor 0x5802B0,
// and Wall of Fire, 0x581CA0) embed this at +0x2B4 where the wanderers
// carry the base. Its ctor 0x55AF40 is the base's stream above plus one
// store the base lacks (the wall byte at +0x3A cleared) and its own vtable
// stamp 0x84EEB0, which keeps the base Service 0x55A610 in slot 0 and puts
// the wall's own gather 0x55B1B0 in slot 1 — that override walks the four
// corner offsets at +0x3C..+0x5C instead of ranging a radius. Both wall
// ctors arm the wall length at +0x36 to 20 after construction. The corner
// setup 0x55B010 (which would write +0x3C..+0x5C and set +0x3A) has no
// caller or reference anywhere in the exe, so the corners keep whatever the
// ctor state has; the gather reads them as the original does. Offsets
// informational as above (vtable 0x84EEB0, sizeof 0x5C in the exe: 0x2B4 +
// 0x5C = 0x310, exactly the loop sound's CSound that follows the embed).
class IcewindCProjectileWallTargetMap : public IcewindCProjectileTargetMap {
public:
    IcewindCProjectileWallTargetMap(); // 0x55AF40
    std::map<LONG, CPoint> GatherTargets() override; // 0x55B1B0, vtable 0x84EEB0 slot 1

    /* 0034 */ WORD field_34; // untouched by both wall ctors
    /* 0036 */ LONG field_36; // armed 20 by both wall ctors (the wall length)
    /* 003A */ BYTE field_3A; // cleared by this ctor
    /* 003C */ CPoint m_corners[4]; // the four corner offsets the gather walks
};

#endif /* ICEWINDCPROJECTILETARGETMAP_H_ */
