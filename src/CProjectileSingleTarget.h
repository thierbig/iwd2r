#ifndef CPROJECTILESINGLETARGET_H_
#define CPROJECTILESINGLETARGET_H_

#include "CProjectile.h"
#include "CSound.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"
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

// The lightning-bolt carrier (vtable 0x8513BC): pushes the picked target
// point out past the target so the bolt rakes the whole line. WillScarlettOhara
// models the family as CProjectileLightningBolt
// < IcewindCProjectileTravellingVFX < CProjectileTravelling; the exe Fire
// 0x580B30 closes with CProjectileSingleTarget::Fire (0x5791D0), which is
// the anchor used here (the same substitution as the two classes above).
class CProjectileLightningBolt : public CProjectileSingleTarget {
public:
    // 0x580B30. Reads the caster's position, pushes the target point out to
    // m_beamRange past the target along the caster->target heading, then
    // launches through the base Fire.
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    // The ctor 0x5806C0 and the dtor (vtable slot 0, exe entry 0x580750)
    // are not part of the batch, and slot 0x000C (AIUpdate 0x5808A0) is
    // parked (docs/wso/import-parked/5808A0.json — the strike tracker it
    // walks is the fork-only IcewindCProjectileTargetMap at +0x2BE), so the
    // class inherits the base virtuals for those slots; a declared-but-
    // undefined dtor would become the key function and leave the vtable
    // unemitted, breaking the link.
    // Distance the shot is pushed past the picked target: the radius handed
    // to IcewindMisc::sub_5847B0 in Fire (a DWORD read at +0x2AE in the exe).
    /* 02AE */ LONG m_beamRange;
};

// The whirlwind (vtable 0x851444): a tornado that wanders the area on a
// seeded course, playing ARE_P23 over its whole life and striking the
// sprites it passes. WillScarlettOhara models the family as
// CProjectileWhirlwind < IcewindCProjectileTravellingVFX <
// CProjectileTravelling; the exe Fire 0x57FF80 closes with
// CProjectileSingleTarget::Fire (0x5791D0), which is the anchor used here
// (the same substitution as the classes above). The ctor 0x57F640 is parked
// (docs/wso/import-parked/57F640.json — it wires the fork-only
// IcewindCProjectileTargetMap strike tracker at +0x2BA), as are AIUpdate
// 0x57F8D0 (docs/wso/import-parked/57F8D0.json), PickWanderPoint 0x5800E0
// (docs/wso/import-parked/5800E0.json — the fork's 16-entry step table is
// mirrored against the exe's jump table at 0x58022C: the fork's case n is
// the exe's index (12-n) mod 16, while the rose that indexes it —
// CGameSprite::GetDirection, 0=south/4=west/8=north/12=east — matches the
// exe's order) and OnArrival 0x580270 (docs/wso/import-parked/580270.json —
// its only callee is that parked pick). The fork's dtor (0x57F760) has not
// been queued. None of those slots is declared — a declared-but-undefined
// virtual would become the key function and leave the vtable unemitted —
// so the class inherits the base virtuals for them.
class CProjectileWhirlwind : public CProjectileSingleTarget {
public:
    // 0x57FF80. Points the loop sound at ARE_P23, makes it fire-and-forget
    // on channel 15 of the area, starts it at the tornado's position, then
    // launches through the base Fire with INVALID_INDEX for the target —
    // the tornado never homes.
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    // +0x2BA..+0x2EF is the strike tracker the parked ctor builds (the
    // fork-only IcewindCProjectileTargetMap); kept as raw bytes until that
    // type is imported.
    /* 02AE */ LONG m_nLifetime;
    /* 02B2 */ LONG m_nLegBudget;
    /* 02B6 */ LONG field_2B6;
    /* 02BA */ BYTE field_2BA[0x36];
    /* 02F0 */ CSound m_loopSound;
    /* 0354 */ BYTE m_bFinishing;
    /* 0356 */ LONG m_wanderSeed;
};

#endif /* CPROJECTILESINGLETARGET_H_ */
