#ifndef CPROJECTILEBAM_H_
#define CPROJECTILEBAM_H_

#include "CProjectile.h"
#include "CAIObjectType.h"
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
    // 0x5297B0. The exe ctor forwards the resref to the exploding
    // intermediate 0x52CCE0 — which bases on CProjectileBAM (0x52AD60),
    // stores vtable 0x84DBDC, and initializes the exploding fields our
    // anchor does not model (the CVidCells at +0x2FA and +0x3D8, the
    // CAIObjectType at +0x2B4, and +0x2A4 = 78) — then stores 0x84D938.
    // Ours forwards straight to CProjectileBAM; the resref is passed by
    // value in the exe (ret 0x8).
    CProjectileSparkleExplosion(CResRef resRef);
    /* 0088 */ virtual void Explode();
};

// The single travelling spell missile (vtable 0x84DB54, between
// CProjectileSparkleExplosion's 0x84D938 and the exploding intermediate's
// 0x84DBDC): the magic-missile bolt. DecodeProjectile's sparkle cases hand
// the ctor an optional palette recolour; the sparkle-stream factory cases
// swap the cell to "TRAVEL" afterwards. Doubled velocity, mirror bounds
// 15/11 and leaf render param 0x80 — the same tuning as the mirrored
// missiles — with the shadow, direction and lifetime fields cleared.
class CProjectileSparkle : public CProjectileBAM {
public:
    // 0x52CA10
    CProjectileSparkle(SHORT nPaletteType);
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

// The bolt projectile (vtable 0x84E14C): a CProjectileBAM on the BOLT
// animation with 16 directions and its speed times 5. It adds no members
// of its own — the ctor writes only inherited fields.
class CProjectileBolt : public CProjectileBAM {
public:
    CProjectileBolt();
};

// The boulder projectile (vtable 0x84E1D4): a CProjectileBAM on the BOULDER
// animation with 16 directions and its speed times 3/2. It adds no members
// of its own — the ctor writes only inherited fields.
class CProjectileBoulder : public CProjectileBAM {
public:
    CProjectileBoulder();
};

// The sling-stone projectile (vtable 0x84E2E4): a CProjectileBAM on the
// MAGICSTN animation with the full palette range SetRange(0, 27) off the
// master bitmap, one direction, and its speed times 5. It adds no members
// of its own — the ctor writes only inherited fields.
class CProjectileSlingStone : public CProjectileBAM {
public:
    CProjectileSlingStone();
};

// The axe projectile (vtable 0x84E36C): a CProjectileBAM on the AXE
// animation with 16 directions and its speed doubled. It adds no members
// of its own — the ctor writes only inherited fields.
class CProjectileAxe : public CProjectileBAM {
public:
    CProjectileAxe();
};

// The spear projectile (vtable 0x84E3F4): a CProjectileBAM on the SPEAR
// animation with 16 directions and its speed doubled. It adds no members
// of its own — the ctor writes only inherited fields.
class CProjectileSpear : public CProjectileBAM {
public:
    CProjectileSpear();
};

// The thrown-dagger projectile (vtable 0x84E47C): a CProjectileBAM on the
// DAGGER animation with the palette ranges SetRange(5, 27) and SetRange(4, 30)
// off the master bitmap and its speed doubled. It adds no members of its own —
// the ctor writes only inherited fields.
class CProjectileDagger : public CProjectileBAM {
public:
    CProjectileDagger();
};

// The acid-blob projectile (vtable 0x84E504): a CProjectileBAM on the
// ACIDBLOB animation with the full palette range SetRange(0, 73) off the
// master bitmap and 16 directions. It adds no members of its own — the ctor
// writes only inherited fields.
class CProjectileAcidBlob : public CProjectileBAM {
public:
    CProjectileAcidBlob();
};

// The SPBRNHND projectile (vtable 0x84E69C): a CProjectileBAM on the
// SPBRNHND animation with the full palette range SetRange(0, 71) off the
// master bitmap, the field_E6 brightest bit set, and a WORD lifetime of
// 55 at +0x2A0. The BAM ctor leaves +0x2A0 to the subclasses (it only
// writes +0x29D/+0x29E there), so — like CProjectile84E030's m_nCount —
// the derived class names the bytes.
class CProjectileSPBRNHND : public CProjectileBAM {
public:
    CProjectileSPBRNHND();

    /* 02A0 */ WORD m_animationLifetime;
};

// One cell of a spray area projectile (vtable 0x84E724): registers itself
// in the area's object list at the front, then plays its fire sound.
// NOTE: The exe vtable's slot 0x88 is AddVisualToArea (0x533AE0), overriding
// the slot CProjectileBAM fills with 0x529AB0; the vtable continues past it
// (0x8C -> 0x533DF0, 0x90 -> 0x45B680, 0x94 -> 0x4C7BE0, 0x98 -> 0x534140)
// and those virtuals are not part of batch i058, so ours declares none of
// them.
class CProjectileSprayCell : public CProjectileBAM {
public:
    /* 0088 */ virtual void AddVisualToArea(CGameArea* pArea, const CPoint& point, LONG height);
};

// The projectile that fires child projectiles of type m_childType + 1 on an
// interval until m_remainingShots runs out (vtable 0x84E7B0): Melf's Magic
// Missile and its kin are built on it.
// NOTE: The exe vtable overrides Fire (slot 0x6C -> 0x533F40) and AIUpdate
// (slot 0x0C -> 0x534140); neither is part of batch i058, so ours inherits
// CProjectileBAM's.
class CProjectileFollowUp : public CProjectileBAM {
public:
    CProjectileFollowUp(USHORT childType, CResRef resRef, SHORT remaining, SHORT interval, BOOL keepSource, CAIObjectType type);
    // 0x533DE0: the slot 0x7C callback AIUpdate waits on — it sets m_ready.
    /* 007C */ void CallBack() override;

    /* 02A0 */ WORD m_remainingShots;
    /* 02A2 */ WORD m_interval;
    /* 02A4 */ BOOL m_keepSource;
    /* 02A8 */ WORD m_childType;
    /* 02AA */ CProjectile* m_nextProjectile;
    /* 02AE */ BOOL m_ready;
    /* 02B2 */ CAIObjectType m_targetType;
    /* 02EE */ LONG m_launchHeight;
    /* 02F2 */ WORD m_launchKind;
    /* 02F4 */ DWORD m_checkLOS;
    /* 02F8 */ WORD m_delay;
};

// The Magic Missile barrage (vtable 0x84E838): a CProjectileFollowUp on the
// MAGMISS animation that keeps its source alive and launches one child of
// type 23 every 300 ticks until count runs out. The ctor mirrors the
// missile (field_1C6, mirror fields +0x1CA/+0x1CE = 10/7, one direction,
// leaf render param 0x80) and points m_targetType at the hostile side with
// LOS checks on.
class CProjectileMAGMISS : public CProjectileFollowUp {
public:
    CProjectileMAGMISS(SHORT count, USHORT /*unused*/);
};

// One magic missile, type 218 (vtable 0x8510A4).
class CProjectile8510A4 : public CProjectileBAM {
public:
    CProjectile8510A4(WORD nPaletteType);
};

#endif /* CPROJECTILEBAM_H_ */
