#ifndef CPROJECTILEBAM_H_
#define CPROJECTILEBAM_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"

// The animated projectile that flies to its target (vtable 0x84D9C4).
// CProjectileSingleTarget (0x578110) is a copy of it with an
// IcewindCVisualEffect.
class CProjectileBAM : public CProjectile {
public:
    CProjectileBAM(CResRef resRef);
    /* 0000 */ ~CProjectileBAM() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0080 */ virtual DWORD GetField_E6();
    /* 0084 */ virtual void UpdateBasedOnTargetPoint(LONG x, LONG y);

    void CalculateFxRect(CRect& rFx, CPoint& ptReference, CVidCell* pVidCell);
    void SetColorRange(USHORT nRange, USHORT nColor);
    void SetSequenceDirection(CVidCell* pVidCell);

    /* 0192 */ CVidCell* m_animation;
    /* 0196 */ CVidCell* m_shadow;
    /* 019A */ CVidPalette m_palette;
    // NOTE: The constructor (0x52AD60) leaves +0x1BE, +0x1C2, +0x1D2 and
    // +0x1D8 to the subclasses, and not all of them set every one:
    // CProjectile84DFA4 sets +0x1BE, +0x1C2 and +0x1D8, CProjectile84DF1C
    // only +0x1D8, and CProjectile8510A4 (0x57E030, type 218) all but
    // +0x1D8, which Render reads, so the original mirrors that missile by
    // direction whenever the heap garbage there is 2 to 0x7FFF (the WORD
    // compare is signed). Bounded (ruling P4-10): ours starts all four at
    // 0, so it never mirrors.
    /* 01BE */ DWORD field_1BE = 0;
    /* 01C2 */ BOOL field_1C2 = FALSE;
    /* 01C6 */ DWORD field_1C6;
    /* 01CA */ DWORD field_1CA;
    /* 01CE */ DWORD field_1CE;
    /* 01D2 */ WORD field_1D2 = 0;
    /* 01D4 */ DWORD field_1D4;
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
};

// The sparkle-burst projectile: Explode decodes the child projectile its
// sparkle colour maps to (types 196 to 203) and fires it at the target
// (vtable 0x84D938, which sits immediately before CProjectileBAM's).
// NOTE: The exe's vtable for this class overrides Fire too (slot 0x6C
// -> 0x52D9F0, shared with the sibling class at vtable 0x84DC68 — the
// exploding-projectile intermediate whose ctor 0x52CCE0 this class's
// ctor 0x5297B0 calls). That Fire override is not part of batch i058,
// so ours inherits CProjectileBAM::Fire.
class CProjectileSparkleExplosion : public CProjectileBAM {
public:
    /* 0088 */ virtual void Explode();
};

// The invisible projectile an area projectile fires at each target, type 79
// (vtable 0x84DF1C).
class CProjectile84DF1C : public CProjectileBAM {
public:
    CProjectile84DF1C();
};

// Several projectiles of one type fired together (vtable 0x84E030): the
// constructor makes nCount of type nType + 1 (DecodeProjectile); Fire fires
// them all at the target, each with copies of this projectile's effects and
// its speed changed by up to 10 either way, spreads them in pairs to either
// side of the line of flight, and deletes this one.
class CProjectile84E030 : public CProjectileBAM {
public:
    CProjectile84E030(WORD nType, CResRef resRef, WORD nCount, WORD nPaletteType);
    /* 0000 */ ~CProjectile84E030() override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 02A0 */ WORD m_nCount;
    /* 02A2 */ CTypedPtrList<CPtrList, CProjectile*> m_projectiles;
    /* 02BE */ WORD m_nType;
};

// Magic Missile, types 68 to 72 (vtable 0x84E0C4): 1 to 5 missiles of type
// 218.
class CProjectile84E0C4 : public CProjectile84E030 {
public:
    CProjectile84E0C4(WORD nCount, WORD nPaletteType);
};

// The thrown-dagger projectile (vtable 0x84E47C): a CProjectileBAM on the
// DAGGER animation with the palette ranges SetRange(5, 27) and SetRange(4, 30)
// off the master bitmap and its speed doubled. It adds no members of its own —
// the ctor writes only inherited fields.
class CProjectileDagger : public CProjectileBAM {
public:
    CProjectileDagger();
};

// One magic missile, type 218 (vtable 0x8510A4).
class CProjectile8510A4 : public CProjectileBAM {
public:
    CProjectile8510A4(WORD nPaletteType);
};

#endif /* CPROJECTILEBAM_H_ */
