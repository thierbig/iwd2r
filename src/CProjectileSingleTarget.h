#ifndef CPROJECTILESINGLETARGET_H_
#define CPROJECTILESINGLETARGET_H_

#include "CProjectile.h"
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
// AIUpdate and Render are parked i047 rows 0x57B210/0x57B780, so the base
// overrides are declarations only; nothing instantiates this class, which
// keeps them link-safe.
class IcewindCProjectileScorcher : public CProjectileSingleTarget {
public:
    // 0x57B080. Scalar deleting dtor: the delete branch calls
    // operator_delete 0x7FC984, the body at 0x57B0A0 tears down the
    // std::set member.
    /* 0000 */ ~IcewindCProjectileScorcher() override;
    // 0x57B210 (parked i047).
    /* 000C */ void AIUpdate() override;
    // 0x57B780 (parked i047).
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

    // The ids this wave has already struck (exe std::set internals: _Myhead
    // 0x2BA, _Multi 0x2BE, NIL sentinel 0x8E28C8).
    /* 02B6 */ std::set<DWORD> m_struckTargets;
};

#endif /* CPROJECTILESINGLETARGET_H_ */
