#ifndef CPROJECTILESINGLETARGET_H_
#define CPROJECTILESINGLETARGET_H_

#include "CProjectile.h"
#include "CSound.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"
#include "IcewindCProjectileTargetMap.h"
#include "IcewindCVisualEffect.h"

#include <set>
#include <string>
#include <vector>

// The animated projectile that flies to one target (vtable 0x850CAC). It has
// the members of CProjectileBAM (0x52AD60) and an IcewindCVisualEffect; the
// spell hit and area projectiles derive from it.
class CProjectileSingleTarget : public CProjectile {
public:
    CProjectileSingleTarget();
    CProjectileSingleTarget(CResRef resRef);
    /* 0000 */ ~CProjectileSingleTarget() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0080 */ virtual DWORD GetField_E6();
    /* 0084 */ virtual void UpdateBasedOnTargetPoint(LONG x, LONG y);

    void CalculateFxRect(CRect& rFx, CPoint& ptReference, CVidCell* pVidCell);
    void SetSequenceDirection(CVidCell* pVidCell);

    /* 0192 */ CVidCell* m_animation;
    /* 0196 */ CVidCell* m_shadow;
    /* 019A */ CVidPalette m_palette;
    /* 01BE */ BOOL field_1BE;
    /* 01C2 */ BOOL field_1C2;
    /* 01C6 */ DWORD field_1C6;
    /* 01D4 */ DWORD field_1D4;
    /* 01D2 */ WORD field_1D2;
    // NOTE: 0x577E40 zeroes +0x1BE, +0x1D2, +0x1D8 and +0x1DC; 0x578110
    // leaves them to the subclasses, and at least ten set +0x1D8 (16 at
    // 0x57AFF0, 0x57C60C, 0x57D42D, 0x57F56F and 0x580321, 1 at 0x57F6C4).
    // Render reads +0x1D8. Bounded (ruling P4-10): ours also starts it at 0
    // for an object on 0x578110 that its creator leaves unset.
    /* 01D8 */ WORD field_1D8 = 0;
    /* 01DA */ WORD field_1DA;
    /* 01DC */ SHORT m_nDirection;
    /* 01DE */ BOOL field_1DE;
    /* 01E2 */ CVidBitmap field_1E2;
    /* 029C */ BYTE field_29C;
    /* 029D */ BOOLEAN m_bLimitedLifetime;
    /* 029E */ SHORT m_nLifetime;
    /* 02A0 */ BYTE field_2A0;
    /* 02A1 */ BYTE field_2A1;
    /* 02A2 */ IcewindCVisualEffect m_visualEffect;
};

// One puff of the cone's waves (vtable 0x850D54): a CProjectileSingleTarget
// that flies to a point; only its destructor and DeliverEffects slots differ,
// and both lead to the same functions.
class CProjectile850D54 : public CProjectileSingleTarget {
public:
    CProjectile850D54(CResRef resRef);
    /* 0078 */ void DeliverEffects() override;
};

// The cone (vtable 0x850DDC; effect 293, Beholder Dispel Magic). It flies
// to its target point like a CProjectileSingleTarget, sends waves of puffs
// to the points across its far end, and on arrival gives its effects to
// every AI object in the cone in line of sight from its source.
class CProjectile850DDC : public CProjectileSingleTarget {
public:
    CProjectile850DDC(const char* szAnimation, const char* szPuff);
    // 0x579DE0
    /* 0000 */ ~CProjectile850DDC() override;
    /* 000C */ void AIUpdate() override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0070 */ void OnArrival() override;
    /* 0078 */ void DeliverEffects() override;
    /* 0088 */ virtual void sub_88();

    // The corners: the two at the source, then the two at the far end.
    /* 02AE */ CPoint m_corners[4];
    /* 02CE */ LONG m_nWidth;
    /* 02D2 */ LONG m_nLength;
    /* 02D6 */ LONG m_nPuffs;
    /* 02DA */ LONG m_nHalfWidth;
    /* 02DE */ std::string m_sPuff;
    /* 02EE */ LONG field_2EE;
    /* 02F2 */ std::vector<CPoint> m_puffPoints;
    /* 0302 */ LONG m_nPuffSpacing;
    /* 0306 */ LONG m_nPuffSpeed;
    /* 030A */ LONG m_nPuffSpeedRandom;
    /* 030E */ LONG m_nFrames;
    /* 0312 */ LONG m_nWaveInterval;
    /* 0316 */ BYTE m_bRandomPuffFrame;
    /* 0317 */ BYTE field_317;
    /* 0318 */ CPoint m_ptSource;
    /* 0320 */ CPoint m_ptEnd;
    /* 0328 */ LONG m_nHeight;
    /* 032C */ WORD m_nType;
    /* 032E */ LONG m_nFrame;
    /* 0332 */ BOOLEAN m_bDelivered;
};

// The Icewind Dale II Scorcher (vtable 0x850E84). WillScarlettOhara models
// the family as IcewindCProjectileScorcher < IcewindCProjectileTravellingVFX
// < CProjectileTravelling; the exe ctor 0x57AEB0 bases it on
// CProjectileSingleTarget (0x578110) with CResRef("AScorcT"), which is the
// anchor used here. Its exe vtable overrides every base slot: dtor 0x57B080,
// AIUpdate 0x57B210, Render 0x57B780, Fire 0x57B1D0, and DeliverEffects —
// slot 0x0078 holds 0x799CA0, CProjectileArea85072C::OnArrival, an empty
// function MSVC ICF-folded with this class's equally empty DeliverEffects.
class IcewindCProjectileScorcher : public CProjectileSingleTarget {
public:
    // 0x57AEB0. Bases on CProjectileSingleTarget(CResRef("AScorcT")), then
    // tunes the wave: 3-tick strike / 60-tick clear cadence, TRA_M04 fire
    // sound, speed = frameHeight*3/4, life span 110, height enabled.
    IcewindCProjectileScorcher();
    // Slots 0x0000 and 0x000C are not declared: the dtor 0x57B080 and
    // AIUpdate 0x57B210 are parked (docs/wso/import-parked/57B080.json —
    // exercised by no scenario — and 57B210.json, whose life-span-0 exit is
    // `delete this` through the parked dtor), so the class inherits the base
    // virtuals for those slots; a declared-but-undefined dtor would become
    // the key function and leave the vtable unemitted, breaking the link
    // against the 0x57AEB0 ctor's vtable store. 0x57B080's scalar deleting
    // dtor calls operator_delete 0x7FC984; the body at 0x57B0A0 tears down
    // the std::set member.
    // 0x57B780.
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    // 0x57B1D0: marshals the seven arguments, calls the base Fire at
    // 0x5791D0, stores +0x2D2, then invokes slot 0x0078.
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    // 0x799CA0 as an ICF-folded alias; the slot is pinned by CProjectile.h.
    /* 0078 */ void DeliverEffects() override;

    // 0x57BBC0. Native static direction table.
    static void InitializeOffsets();

    // exe: _Myfirst/_Mylast/_Myend at 0x8E52D4/0x8E52D8/0x8E52DC; the vector
    // helper thunks (0x582210 size, 0x57AB70 insert, 0x582230 erase) are
    // called with the enclosing static base 0x8E52D0.
    static std::vector<CPoint> m_offsets;
    // 0x8E52E8
    static BYTE m_offsetsInitialized;

    /* 0088 */ virtual void GatherTargets();
    /* 008C */ virtual void Strike(LONG target, BYTE initial);

    /* 02AE */ BYTE m_lifeSpan;
    /* 02AF */ BYTE m_nSegments;
    /* 02B0 */ LONG m_deltaZ;
    /* 02B4 */ BYTE m_bGlow;

    // The ids this wave has already struck (exe std::set internals: _Myhead
    // 0x2BA, _Multi 0x2BE, NIL sentinel 0x8E28C8).
    /* 02B6 */ std::set<DWORD> m_struckTargets;

    /* 02C6 */ LONG m_tickCount;
    /* 02CA */ LONG m_strikePeriod;
    /* 02CE */ LONG m_clearPeriod;
    /* 02D2 */ LONG m_strikeTarget;
};

// The chain-lightning carrier (vtable 0x850F14): each strike of a chain
// lightning spawns one of these, which re-strikes a growing set of targets.
// WillScarlettOhara models the family as IcewindCProjectileChainLightning
// < IcewindCProjectileTravellingVFX < CProjectileTravelling with the
// struck-id std::set at +0x2AE; the exe ctor 0x57C510 bases it on
// CProjectileSingleTarget (0x578110) with CResRef("CLightT"), which is the
// anchor used here.
class IcewindCProjectileChainLightning : public CProjectileSingleTarget {
public:
    // 0x57C510. Copies the struck-target set (its sole argument), then
    // retunes the carrier: CLightT cell at sequence 0, 16 directions,
    // TRA_09 fire sound, speed doubled with a native WORD shift, and both
    // height gates cleared. DecodeProjectile's chain-lightning case
    // (0x5250B0) news 0x2BE bytes — the native class size — and hands the
    // ctor a set freshly built on its stack.
    explicit IcewindCProjectileChainLightning(const std::set<LONG>& struckTargets);
    // Slot 0x0078 is not declared: DeliverEffects (0x57C7B0) is parked
    // (docs/wso/import-parked/57C7B0.json), so the class inherits the base
    // virtual for that slot.
    // The ids this carrier has already struck (exe std::set internals: _Myhead
    // 0x2B2, _Multi 0x2B6, _Mysize 0x2BA, NIL sentinel 0x8D48B4, refcount
    // 0x8D48B0; native size 0x2BE). It is the last member, so the VC6-vs-ours
    // std::set size drift stays inside sizeof.
    /* 02AE */ std::set<LONG> m_struckTargets;
};

// The rebounding bolt (vtable 0x85101C, sizeof 0x2EC): the older,
// wall-rebounding LightnT carrier. DecodeProjectile's type 207 stops it at
// its first wall (the ctor starts it unbounced), and the searing light's
// Fire 0x530010 spawns one of these per rebound with its own count in
// m_nBounces. WillScarlettOhara models the family as CProjectileReboundingBolt
// < IcewindCProjectileTravellingVFX < CProjectileTravelling; the exe ctor
// 0x57D390 bases it on CProjectileSingleTarget (0x578110) with
// CResRef("LightnT"), which is the anchor used here (the same substitution
// as the classes above). The exported CProjectileBAM::MoveUpdate name at its
// AIUpdate 0x57D9F0 is stale: slot 3 of this leaf is AIUpdate, and +0x2B6 is
// a strike tracker, not a vid cell.
class CProjectileReboundingBolt : public CProjectileSingleTarget {
public:
    // 0x57D390. Binds LightnT through the base, constructs the strike
    // tracker at +0x2B6 (its ctor 0x55ACC0, the implicit member init), then
    // retunes the carrier: unbounced (-1 — the decoder and the searing
    // light's Fire stamp the real count), sequence 0, render flag 8, both
    // height gates cleared, 16 directions, TRA_09b fire sound, speed doubled
    // with a native WORD shift, a 60-tick duration, and the tracker armed:
    // owner this, strike interval 33, service period 2, range 50.
    CProjectileReboundingBolt();
    // 0x57D4C0. The deleting thunk over the scalar body 0x57D5D0, which is
    // the compiler's member teardown — the strike tracker's std::map at
    // +0x2B6 (its _Myhead read at +0x2C2 opens the body), then the base.
    // Empty braces spell it.
    /* 0000 */ ~CProjectileReboundingBolt() override;
    // Fire 0x57D700 and AIUpdate 0x57D9F0 stay parked
    // (docs/wso/import-parked/57D700.json, 57D9F0.json): undeclared, the
    // class inherits the base virtuals for those slots — a
    // declared-but-undefined virtual would become the key function and
    // leave the vtable unemitted.
    // The flight's duration in ticks (the ctor stamps 60).
    /* 02AE */ LONG m_nDuration;
    // Walls left: the ctor starts it unbounced (-1); the type-207 decode
    // stamps 1 and the searing light's Fire 0x530010 hands its own count
    // (1 or 6) down per rebound.
    /* 02B2 */ LONG m_nBounces;
    // The strike tracker the ctor arms (the same embed as the bolt's at
    // +0x2B2: owner +0x2BA, service period +0x2CE, strike interval +0x2D6,
    // range +0x2E8, decoder allocation 0x2EC).
    /* 02B6 */ IcewindCProjectileTargetMap m_targetMap;
};

// The lightning-bolt carrier (vtable 0x8513BC): pushes the picked target
// point out past the target so the bolt rakes the whole line. WillScarlettOhara
// models the family as CProjectileLightningBolt
// < IcewindCProjectileTravellingVFX < CProjectileTravelling; the exe Fire
// 0x580B30 closes with CProjectileSingleTarget::Fire (0x5791D0), which is
// the anchor used here (the same substitution as the two classes above).
class CProjectileLightningBolt : public CProjectileSingleTarget {
public:
    // 0x5806C0. Binds the spell's travel BAM through the base (the char*
    // argument folds through a CResRef temp), then constructs the strike
    // tracker at +0x2B2 (its ctor 0x55ACC0) and arms it: this projectile
    // owns it, it services every 2 ticks and re-strikes each victim every
    // 33 in-range passes. The rake length is stamped to 400 (the disruption
    // and breath entries retune to 600).
    CProjectileLightningBolt(const char* szResRef);
    // 0x580B30. Reads the caster's position, pushes the target point out to
    // m_beamRange past the target along the caster->target heading, then
    // launches through the base Fire.
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    // The dtor body 0x580770 stays parked (docs/wso/import-parked/580770.json
    // — it only destroys the strike tracker, which the implicit dtor does
    // here), as does slot 0x000C (AIUpdate 0x5808A0,
    // docs/wso/import-parked/5808A0.json — the strike tracker it walks is
    // that same map), so the class inherits the base virtual for that slot;
    // a declared-but-undefined dtor would become the key function and leave
    // the vtable unemitted, breaking the link.
    // Distance the shot is pushed past the picked target: the radius handed
    // to IcewindMisc::sub_5847B0 in Fire (a DWORD read at +0x2AE in the exe).
    /* 02AE */ LONG m_beamRange;
    // The strike tracker the ctor arms (its own vtable 0x84EEA8): the ids
    // inside the rake with their re-strike counters (exe embed at +0x2B2:
    // owner +0x2B6, service period +0x2CA, strike interval +0x2D2, range
    // +0x2E4, decoder allocation 0x2E8).
    /* 02B2 */ IcewindCProjectileTargetMap m_targetMap;
};

// The whirlwind (vtable 0x851444): a tornado that wanders the area on a
// seeded course, playing ARE_P23 over its whole life and striking the
// sprites it passes. WillScarlettOhara models the family as
// CProjectileWhirlwind < IcewindCProjectileTravellingVFX <
// CProjectileTravelling; the exe Fire 0x57FF80 closes with
// CProjectileSingleTarget::Fire (0x5791D0), which is the anchor used here
// (the same substitution as the classes above). The ctor 0x57F640 landed
// in task 81g (it wires the IcewindCProjectileTargetMap strike tracker at
// +0x2BA). Still parked: AIUpdate
// 0x57F8D0 (docs/wso/import-parked/57F8D0.json), PickWanderPoint 0x5800E0
// (docs/wso/import-parked/5800E0.json — the fork's 16-entry step table is
// mirrored against the exe's jump table at 0x58022C: the fork's case n is
// the exe's index (12-n) mod 16, while the rose that indexes it —
// CGameSprite::GetDirection, 0=south/4=west/8=north/12=east — matches the
// exe's order) and OnArrival 0x580270 (docs/wso/import-parked/580270.json —
// its only callee is that parked pick). The dtor 0x57F760 landed in batch
// i069; the parked slots stay undeclared — a declared-but-undefined virtual
// would become the key function and leave the vtable unemitted — so the
// class inherits the base virtuals for them.
class CProjectileWhirlwind : public CProjectileSingleTarget {
public:
    // 0x57F640. Binds the WhirlwX tornado through the base, constructs the
    // strike tracker at +0x2BA and the loop sound's CSound at +0x2F0 (the
    // implicit member inits), then retunes the wanderer: a 1000-tick
    // lifetime and leg budget, the finishing flag and the wander seed
    // cleared (the seed is MP-replicated into +0x356 later, through the
    // CMessageFireProjectile path), one wander direction, speed 5, the
    // tracker armed (owner this, 8 strikes total, gather radius 70,
    // re-strike interval 33, service period 3, never the caster),
    // copy-from-back on the visual effect, both height gates cleared,
    // and the EFF_P105 fire sound looped.
    CProjectileWhirlwind();
    // 0x57F760. Empty braces: the scalar body 0x57F780 is the compiler's
    // member teardown (the loop sound's CSound dtor first, then the strike
    // tracker map), which the members spell implicitly.
    /* 0000 */ ~CProjectileWhirlwind() override;

    // 0x57FF80. Points the loop sound at ARE_P23, makes it fire-and-forget
    // on channel 15 of the area, starts it at the tornado's position, then
    // launches through the base Fire with INVALID_INDEX for the target —
    // the tornado never homes.
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    // The strike tracker the ctor arms (the same embed as the bolt's at
    // +0x2B6: owner +0x2BE, service period +0x2D2, strike interval +0x2DA,
    // max total +0x2E2, skip-source +0x2EB, range +0x2EC, decoder
    // allocation 0x35A).
    /* 02AE */ LONG m_nLifetime;
    /* 02B2 */ LONG m_nLegBudget;
    /* 02B6 */ LONG field_2B6;
    /* 02BA */ IcewindCProjectileTargetMap m_targetMap;
    /* 02F0 */ CSound m_loopSound;
    /* 0354 */ BYTE m_bFinishing;
    /* 0356 */ LONG m_wanderSeed;
};

// The Wall of Moonlight carrier (vtable 0x8514CC, sizeof 0x374):
// DecodeProjectile's type 304 (WoMoonX, Moon Path's wall) builds one.
// Exactly four vtable slots differ from the base CProjectileSingleTarget
// (0x850CAC) over the family's 34 — slot 0x00's deleting dtor 0x5803C0
// (its scalar body 0x5803E0 is the compiler's member teardown: the loop
// sound's CSound dtor first at +0x310, then the wall tracker's map at
// +0x2B4, spelled implicitly by ours), slot 0x0C AIUpdate 0x580550, slot
// 0x6C Fire 0x580600, and slot 0x70 OnArrival 0x5820B0. Those three are
// not ported and none of the four is declared — a declared-but-undefined
// virtual would become the key function and leave the vtable unemitted —
// so the class inherits the base virtuals for them.
class CProjectileWallOfMoonlight : public CProjectileSingleTarget {
public:
    // 0x5802B0. Binds the WoMoonX wall through the base, constructs the
    // wall tracker at +0x2B4 (the derived IcewindCProjectileWallTargetMap
    // ctor 0x55AF40) and the loop sound's CSound at +0x310 (the implicit
    // member inits), then arms the wall in one straight line: a 500-tick
    // lifetime, the byte at +0x2B2 cleared, sixteen directions, speed
    // tripled with a native WORD imul (20 -> 60), the tracker armed (owner
    // this, one strike per victim, gather radius 70, service period 5,
    // wall length 20), copy-from-back on the visual effect, both height
    // gates cleared along with field_1DE and the byte at +0x2A0, and the
    // AFT_P21 fire sound looped.
    CProjectileWallOfMoonlight();

    // The wall tracker the ctor arms (the derived wall map, not the base:
    // owner +0x2B8, service period +0x2CC, per-victim cap +0x2D8, range
    // +0x2E6, wall length +0x2EA, decoder allocation 0x374).
    /* 02AE */ LONG m_nLifetime;
    /* 02B2 */ BYTE field_2B2;
    /* 02B4 */ IcewindCProjectileWallTargetMap m_targetMap;
    /* 0310 */ CSound m_loopSound;
};

// The MFMissT wanderer (vtable 0x851554, sizeof 0x2E4):
// DecodeProjectile's type 314 builds one — the last and leanest of the
// base-map wanderer family (the rebounding bolt, the two 0x57F390/0x57F640
// wanderers, the lightning bolt): a single-direction MFMissT traveler
// that arms the base strike tracker with just its owner and gather
// radius, leaving every cadence field at the map ctor's default. Only
// two vtable slots differ from the base CProjectileSingleTarget
// (0x850CAC) over the family's 34 — slot 0x00's deleting dtor 0x580CC0
// (its scalar body 0x580CE0 is the compiler's member teardown walking
// the base tracker map at +0x2AE, spelled implicitly by ours) and slot
// 0x70 OnArrival 0x580E10 (the same base slot the two walls override;
// not ported). Neither is declared — a declared-but-undefined virtual
// would become the key function and leave the vtable unemitted — so the
// class inherits the base virtuals for them.
class CProjectile851554 : public CProjectileSingleTarget {
public:
    // 0x580C00. Binds the MFMissT travel cell through the base, then
    // the retune: one direction at +0x1D8 (the walls seed sixteen, the
    // whirlwind one), speed doubled with a native WORD shift (20 -> 40),
    // the strike tracker armed with just two stores — owner this and
    // gather radius 70; service period 10, strike interval 10, both
    // caps 100000 and strikes allowed on the caster all keep the base
    // map ctor's defaults, since this ctor stores none of them —
    // copy-from-back on the visual effect, the height gate raised along
    // with the +0x1C2 flag, and the TRA_55 fire sound.
    CProjectile851554();

    // The base strike tracker the ctor arms (the only added member — no
    // loop sound's CSound in this class: owner +0x2B2, service period
    // +0x2C6, strike interval +0x2CE, per-victim cap +0x2D2, range
    // +0x2E0, decoder allocation 0x2E4).
    /* 02AE */ IcewindCProjectileTargetMap m_targetMap;
};

// The short AScorcT beam (vtable 0x8515DC, sizeof 0x2B6): a ten-tick
// sibling of the Scorcher above — the same base resref, fire sound and
// first four tail members, but no struck-target set and no strike cadence
// (the binary class stops at m_bGlow). Its Fire (0x581170) chains the base
// Fire and then calls DeliverEffects (slot 0x78, the inherited 0x52A1A0).
class CProjectile8515DC : public CProjectileSingleTarget {
public:
    // 0x581060. Binds the AScorcT beam through the base, zeroes the beam's
    // delta-Z and glow, pins frame 0 of the cell, clears the two color
    // fields and seeds 16 at +0x1D8, rebinds the TRA_M04 fire sound, takes
    // the beam's speed from three quarters of the cell's frame height,
    // disables the callback projectile, blanks the visual effect and
    // +0x182, raises the height gate and the +0x1C2 flag, then zeroes the
    // segment count and gives the beam its ten-tick lifespan.
    CProjectile8515DC();

    // Four slots differ from the base vtable — dtor 0x579B10 (slot 0),
    // AIUpdate 0x5811B0 (slot 3), Render 0x581520 (slot 19), Fire 0x581170
    // (slot 27) — and stay undeclared here (not ported), so the class
    // inherits the base's virtuals everywhere else.
    /* 02AE */ BYTE m_lifeSpan;
    /* 02AF */ BYTE m_nSegments;
    /* 02B0 */ LONG m_deltaZ;
    /* 02B4 */ BYTE m_bGlow;
};

// The Wall of Fire carrier (vtable 0x851664, sizeof 0x374): the moonlight
// wall's fire sibling — DecodeProjectile's type 370 (WoFireX, Wall of
// Fire's wall) builds one. Exactly four vtable slots differ from the base
// CProjectileSingleTarget (0x850CAC) over the family's 34 — slot 0x00's
// deleting dtor 0x581DB0 (its scalar body 0x581DD0 is the compiler's
// member teardown: the loop sound's CSound dtor first at +0x310, then the
// wall tracker's map walk at +0x2B4, spelled implicitly by ours), slot
// 0x0C AIUpdate 0x581F40, slot 0x6C Fire 0x581FF0, and slot 0x70
// OnArrival 0x5820B0 — the same shared override the moonlight wall
// carries. Those three are not ported and none of the four is declared —
// a declared-but-undefined virtual would become the key function and
// leave the vtable unemitted — so the class inherits the base virtuals
// for them.
class CProjectileWallOfFire : public CProjectileSingleTarget {
public:
    // 0x581CA0. Statement-for-statement the moonlight wall's ctor with
    // three deltas: the WoFireX resref, the tracker armed with a
    // 20-tick strike interval instead of the one-strike-per-victim cap
    // (the per-victim cap keeps the map's default), and no clear of the
    // byte at +0x2A0 (the moonlight wall zeroes it; this ctor leaves the
    // base's value standing). Everything else is the sibling's: 500-tick
    // lifetime, the byte at +0x2B2 cleared, sixteen directions, speed
    // tripled with a native WORD imul (20 -> 60), copy-from-back on the
    // visual effect, both height gates cleared along with field_1DE, and
    // the AFT_P21 fire sound looped.
    CProjectileWallOfFire();

    // The wall tracker the ctor arms (the derived wall map, not the base:
    // owner +0x2B8, service period +0x2CC, strike interval +0x2D4,
    // range +0x2E6, wall length +0x2EA, decoder allocation 0x374).
    /* 02AE */ LONG m_nLifetime;
    /* 02B2 */ BYTE field_2B2;
    /* 02B4 */ IcewindCProjectileWallTargetMap m_targetMap;
    /* 0310 */ CSound m_loopSound;
};

// The spell-school travel projectile (vtable 0x8511AC): DecodeProjectile's
// types 227-234 build one of these per school of magic — AbjurT, AlterT,
// InvocT, NecroT, ConjuT, EnchaT, IllusT, DivinT. Every vtable slot holds
// the base CProjectileSingleTarget function except slot 0, whose deleting
// dtor (0x579B10) is a plain wrapper over the base body 0x5783E0 through
// thunk 0x57E570; the ctor only retunes base members, so the class adds no
// fields — the decoder news the base size 0x2AE for it.
class CProjectile8511AC : public CProjectileSingleTarget {
public:
    // 0x57E370: the school's BAM through the base ctor (the char* argument
    // folds through a CString temp into the CResRef the base takes), then
    // the retune — the visual effect flagged for the flight, cell sequence
    // 0, speed doubled with a native WORD shift, the strike fields cleared,
    // no callback projectile, empty visual-effect name, height on, TRA_02
    // fire sound over a dead "" store, empty arrival sound.
    CProjectile8511AC(const char* szResRef);
};

#endif /* CPROJECTILESINGLETARGET_H_ */
