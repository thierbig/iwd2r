#ifndef CPROJECTILEAREA_H_
#define CPROJECTILEAREA_H_

#include "CAIObjectType.h"
#include "CProjectileSingleTarget.h"

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

// An animation and a sound the explosion object of an area projectile plays
// (built at 0x56FDC0 and 0x56FE30 for the second and third).
class CProjectileAreaExplosion {
public:
    /* 0000 */ std::string m_animation;
    /* 0010 */ std::string m_sound;
    /* 0020 */ IcewindCVisualEffect m_visualEffect;
};

class CProjectileAreaExplosion2 : public CProjectileAreaExplosion {
public:
    /* 002C */ BYTE field_2C;
    /* 002E */ LONG field_2E;
};

// The points an area projectile's explosion burned (the third
// description's, 0x14 bytes): the explosion adds one for each try at a
// lingering piece, taken (m_bTaken) where a piece was made; a wandering
// piece moves to a free one when its sequence ends. The pieces share it
// and the last one deletes it (m_nRefCount).
class CProjectileAreaPoints {
public:
    class Point {
    public:
        Point(LONG x, LONG y, BYTE bTaken);

        /* 0000 */ LONG x;
        /* 0004 */ LONG y;
        /* 0008 */ BYTE m_bTaken;
    };

    CProjectileAreaPoints();

    /* 0004 */ std::vector<Point> m_points;
    /* 0010 */ LONG m_nRefCount;
};

class CProjectileAreaExplosion3 : public CProjectileAreaExplosion {
public:
    LONG GetChanceRange();

    /* 002C */ CProjectileAreaPoints* m_pPoints;
    /* 0030 */ LONG field_30;
    /* 0034 */ BYTE field_34;
    /* 0035 */ BYTE field_35;
    /* 0036 */ BYTE field_36;
    /* 0038 */ LONG field_38;
    /* 003C */ LONG field_3C;
    /* 0040 */ LONG field_40;
    /* 0044 */ BYTE field_44;
};

// The projectile that affects everything in a radius around the point it
// reaches (vtable 0x84F1C4). Once it has exploded it fires its secondary
// projectile (type m_nSecondaryType + 1) with a copy of its effects at each
// target found, every m_nHitModulus-th time a target is found.
class CProjectileArea : public CProjectileSingleTarget {
public:
    CProjectileArea(WORD nRadius);
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0070 */ void OnArrival() override;
    /* 0088 */ virtual void sub_88();
    /* 008C */ virtual LONG GetDuration(BYTE nUnknown);
    /* 0090 */ virtual std::list<LONG> GetTargets();
    /* 0094 */ virtual void ApplyToTargets(std::list<LONG>& targets);
    /* 0098 */ virtual void CreateSecondary(LONG nTargetId);

    void TargetEnemy(CGameAIBase* pCaster);
    void TargetFriend(CGameAIBase* pCaster);
    void SetTravelAnimation(const CResRef& resRef);

    /* 02AE */ WORD m_nRadius;
    /* 02B2 */ WORD m_nSecondaryType;
    /* 02B4 */ WORD field_2B4;
    /* 02B6 */ BOOL m_bExploded;
    /* 02BA */ WORD field_2BA;
    /* 02BC */ WORD field_2BC;
    /* 02BE */ CAIObjectType m_targetType;
    /* 02FA */ DWORD field_2FA;
    /* 02FE */ BYTE field_2FE;
    /* 02FF */ BYTE field_2FF;
    /* 0300 */ BOOL m_bCheckForNonSprites;
    /* 0304 */ BOOL m_bAnimation1;
    /* 0308 */ CVidCell m_animation1;
    /* 03E2 */ BOOL m_bAnimation2;
    /* 03E6 */ CVidCell m_animation2;
    /* 04C0 */ LONG m_nDuration;
    /* 04C4 */ std::map<LONG, LONG> m_targetHits;
    /* 04D4 */ LONG m_nTriggerInterval;
    /* 04D8 */ LONG m_nTriggerCountdown;
    /* 04DC */ LONG m_nHitModulus;
    /* 04E0 */ BOOLEAN m_bTravels;
    /* 04E2 */ CProjectileAreaExplosion m_explosion1;
    /* 050E */ CProjectileAreaExplosion2 m_explosion2;
    /* 0540 */ CProjectileAreaExplosion3 m_explosion3;
    /* 0586 */ CSound m_explosionSound;
    /* 05EA */ CSound m_loopingSound;
    /* 064E */ std::set<LONG> m_excludedTargets;
};

// Fireball (projectile type 38, vtable 0x84F580): it flies as FirebaT to
// the target point and explodes there in FirebaX, throwing up to 20
// FirebaR flames and leaving FirebaA embers that wander over the burned
// points (CGameObject84F0DC), 200 around.
class CProjectileArea84F580 : public CProjectileArea {
public:
    CProjectileArea84F580();
};

// Cloudkill (projectile type 187, vtable 0x84F310): CloudKX, up to 13
// CloudKR clouds and lingering CloudKA ones, 150 around, 1000 ticks.
class CProjectileArea84F310 : public CProjectileArea {
public:
    CProjectileArea84F310();
};

// Acid Fog (projectile type 212, vtable 0x84F3AC): DFogX, up to 20 DFogR
// clouds and lingering DFogA ones, 150 around, 1100 ticks. Its secondary
// projectiles need the caster in the game and go to dead and excluded
// sprites too.
class CProjectileArea84F3AC : public CProjectileArea {
public:
    CProjectileArea84F3AC();
    /* 0098 */ void CreateSecondary(LONG nTargetId) override;
};

// Entangle (projectile type 235, vtable 0x84F4E4): EntangX and lingering
// EntangA vines, 200 around, 1000 ticks.
class CProjectileArea84F4E4 : public CProjectileArea {
public:
    CProjectileArea84F4E4();
};

// Fire Seeds, Flaming Oil (projectile type 270, vtable 0x84F61C): flies as
// MagicStn and bursts in FSeedsX, 70 around.
class CProjectileArea84F61C : public CProjectileArea {
public:
    CProjectileArea84F61C();
};

// Fire Storm, Lightning Bolt (projectile type 92, vtable 0x84F6B8): FStormX
// and lingering FStormA flames, 200 around, 105 ticks.
class CProjectileArea84F6B8 : public CProjectileArea {
public:
    CProjectileArea84F6B8();
};

// Flame Strike (projectile type 67, vtable 0x84F754): a FStrikH column on the
// target point, then the area, 140 around.
class CProjectileArea84F754 : public CProjectileArea {
public:
    CProjectileArea84F754();
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
};

// Grease (projectile type 101, vtable 0x84F7F0): GreaseX and lingering
// GreaseA, 100 around, 1000 ticks.
class CProjectileArea84F7F0 : public CProjectileArea {
public:
    CProjectileArea84F7F0();
};

// Hold Person (projectile type 190, vtable 0x84F9C4): EnchanX, 150 around;
// it holds every sprite found up to 1 to 4 humanoids.
class CProjectileArea84F9C4 : public CProjectileArea {
public:
    CProjectileArea84F9C4();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// Sleep (projectile type 42, vtable 0x84FEA4): EnchanX, 150 around; it puts
// to sleep creatures below level 5, up to 2 to 8 levels.
class CProjectileArea84FEA4 : public CProjectileArea {
public:
    CProjectileArea84FEA4();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// Spike Stones (projectile type 213, vtable 0x84FFDC): SStoneA spikes and
// lingering SStoneA ones, 150 around, 1200 ticks.
class CProjectileArea84FFDC : public CProjectileArea {
public:
    CProjectileArea84FFDC();
};

// Stinking Cloud (projectile type 95, vtable 0x8501B0): SCloudX, up to 13
// SCloudR clouds and lingering SCloudA ones, 100 around, 1000 ticks.
class CProjectileArea8501B0 : public CProjectileArea {
public:
    CProjectileArea8501B0();
};

// Web (projectile type 64, vtable 0x8502E8): WebX and lingering WebA
// strands, 150 around, 1500 ticks.
class CProjectileArea8502E8 : public CProjectileArea {
public:
    CProjectileArea8502E8();
};

// A breath (projectile type 349, vtable 0x85072C): it arrives on its
// seventh tick, DBreatH and lingering DBreatA, 200 around.
class CProjectileArea85072C : public CProjectileArea {
public:
    CProjectileArea85072C();
    /* 000C */ void AIUpdate() override;
    /* 0070 */ void OnArrival() override;

    /* 065E */ LONG m_nTicks;
};

// Turn Undead (projectile type 376, vtable 0x850A38): 300 around; it turns
// or destroys the undead the caster's turning check beats.
class CProjectileArea850A38 : public CProjectileArea {
public:
    CProjectileArea850A38();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// Dispel Magic (projectile type 246, vtable 0x850C0C): AbjuraX, 150 around;
// it removes a sprite's Globe of Invulnerability (effect 450) before the
// secondary projectile.
class CProjectileArea850C0C : public CProjectileArea {
public:
    CProjectileArea850C0C();
    /* 0098 */ void CreateSecondary(LONG nTargetId) override;
};

// The area projectile of Static Charge (effect 264, vtable 0x850078): it
// strikes the first target found, else the caster's spell ends.
class CProjectileArea850078 : public CProjectileArea {
public:
    CProjectileArea850078();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// The area projectile of Call Lightning (effect 449, vtable 0x850114): it
// strikes the first target found outdoors, else the caster's spell ends.
class CProjectileArea850114 : public CProjectileArea {
public:
    CProjectileArea850114();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// An area projectile that leaves its source out (Cloak of Fear, effect 265;
// vtable 0x8504BC).
class CProjectileArea8504BC : public CProjectileArea {
public:
    CProjectileArea8504BC(WORD nRadius);
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// The area projectile of Mold Touch's tick (CPersistantEffect84C484, vtable
// 0x8505F4): it strikes the first sprite not already in the Mold Touch state.
class CProjectileArea8505F4 : public CProjectileArea {
public:
    CProjectileArea8505F4(WORD nRadius);
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// The area projectile of Shroud of Flame's tick (CPersistantEffect84C4D4,
// vtable 0x850690): it strikes every sprite not in the Shroud of Flame state.
class CProjectileArea850690 : public CProjectileArea {
public:
    CProjectileArea850690(WORD nRadius);
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

#endif /* CPROJECTILEAREA_H_ */
