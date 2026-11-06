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
    // 0x5295D0 (the resref is passed by value in the exe — ret 0x8)
    void SetVidCell(CResRef resRef);
    // 0x529660 (the exe builds a CString from the literal and hands it
    // here; ours takes the name directly)
    void SetAltPalette(const char* sName);

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

// The exploding-projectile intermediate (vtable 0x84DBDC): the delayed-burst
// carrier. The ctor arms a child projectile (type 78) with an anyone filter
// and a 0x100 strike and pre-check range, and readies two CVidCell explosion
// slots; the parked virtuals (Render 0x52CF80, AIUpdate 0x52DD60, Fire
// 0x52D9F0, OnArrival 0x52D7F0, AreaEffect 0x52D430 — all in
// docs/wso/import-parked/) decode and fire the child in bursts around the
// hit. WillScarlettOhara models it as CProjectileExploding <
// CProjectileTravelling < CProjectileBAM; the exe ctor 0x52CCE0 bases
// directly on CProjectileBAM (0x52AD60 — the resref by value, ret 0x8),
// which is the anchor used here. None of the fork's virtual set (nor its
// slot 0x0088 Explode override, COMDAT-folded with 0x78E730 in the exe) is
// declared: a declared-but-undefined virtual would become the key function
// and leave the vtable unemitted, so ours inherits the base slots.
class CProjectileExploding : public CProjectileBAM {
public:
    // 0x52CCE0 (the resref is passed by value in the exe — ret 0x8)
    CProjectileExploding(CResRef resRef);

    // Native exe offsets; ours lay out after sizeof(CProjectileBAM) = 0x2A2
    // under the reduced packing — the same drift as CProjectile84E030's
    // WORD m_nCount at +0x2A0, which spans the base's field_2A0/field_2A1.
    /* 02A0 */ SHORT m_strikeRange;
    /* 02A2 */ SHORT m_preCheckRange;
    /* 02A4 */ SHORT m_childProjectileType;
    /* 02A6 */ SHORT m_strikesLeft;
    /* 02A8 */ int m_nState;
    /* 02AC */ SHORT m_lingerPeriod;
    /* 02AE */ SHORT m_lingerCountdown;
    /* 02B0 */ int m_bPreScan;
    /* 02B4 */ CAIObjectType m_targetType;
    /* 02F0 */ BYTE m_bBurstPending;
    /* 02F1 */ BYTE m_burstType;
    /* 02F2 */ int m_bCheckNonSprites;
    /* 02F6 */ int m_bExplodeCell1Active;
    /* 02FA */ CVidCell m_explodeCell1;
    /* 03D4 */ int m_bExplodeCell2Active;
    /* 03D8 */ CVidCell m_explodeCell2;
};

// The sparkle-burst projectile: Explode decodes the child projectile its
// sparkle colour maps to (types 196 to 203) and fires it at the target
// (vtable 0x84D938, which sits immediately before CProjectileBAM's).
// NOTE: The exe's vtable for this class overrides Fire too (slot 0x6C
// -> 0x52D9F0, shared with the exploding-projectile intermediate — the
// direct base here, whose ctor 0x52CCE0 this class's ctor 0x5297B0
// calls). That Fire override is not part of batch i058, so ours inherits
// CProjectileBAM::Fire.
class CProjectileSparkleExplosion : public CProjectileExploding {
public:
    // 0x5297B0. The exe ctor forwards the resref to the exploding
    // intermediate 0x52CCE0 — which bases on CProjectileBAM (0x52AD60),
    // stores vtable 0x84DBDC, and initializes the exploding fields (the
    // CVidCells at +0x2FA and +0x3D8, the CAIObjectType at +0x2B4, and
    // +0x2A4 = 78) — then stores 0x84D938. Ours bases on the ported
    // CProjectileExploding ctor, so the chaining matches the exe; the
    // resref is passed by value in the exe (ret 0x8).
    CProjectileSparkleExplosion(CResRef resRef);
    /* 0088 */ virtual void Explode();
};

// The exploding weapon missile (vtable 0x84DC68): Explode (slot 0x0088) is
// the fireball burst (CGameObject85BFE8, 0x7105B0) at the missile position —
// the strike range around, expanding at half the flight speed — coloured by
// the leaf's own explosion colour range byte. The exe vtable also overrides
// slots 0x000C (AIUpdate 0x52E4D0, docs/wso/import-parked/52E4D0.json) and
// +0x98 (0x52EC80, docs/wso/import-parked/52EC80.json); both stay parked, so
// ours inherits the base virtuals for them and declares only Explode.
class CProjectileExplodingWeapon : public CProjectileExploding {
public:
    // 0x52E230: the SPFIREBL burst carrier on the exploding intermediate,
    // then the retune — cell sequence 0, palette range 0 recoloured 0x43 off
    // the master bitmap and handed to the cell, mirror bounds 15/11, the
    // same 0x43 as the explosion colour, mirroring on with the leaf render
    // param 0x80 and 16 directions, doubled speed, the empty fire sound, all
    // seven trail colours 'A' with the trail tick cleared, and child type 78
    // again (the base ctor already stored it).
    CProjectileExplodingWeapon();
    /* 0088 */ virtual void Explode();

    // Native exe offsets, past the tail of m_explodeCell2; ours lay out after
    // the base under the reduced packing.
    /* 04B2 */ BYTE m_trailColorRanges[7];
    /* 04B9 */ BYTE m_trailTick;
    /* 04BA */ BYTE m_explodeColorRange;
};

// The exploding flame missile (vtable 0x84DCF4): Explode (slot 0x0088) is
// the same fireball burst with the colour ranges hardwired to 0x43. The exe
// vtable also overrides slot 0x000C (AIUpdate 0x52EC80, parked); ours
// inherits the base virtual there. The trail members are the weapon
// missile's two, minus its explosion colour byte.
class CProjectileExplodingFlame : public CProjectileExploding {
public:
    // 0x52E9F0: the SPFLMARR burst carrier on the exploding intermediate,
    // then the retune — cell sequence 0, a two-range palette recolour
    // (range 0 to 0x43, range 2 to 0x39) off the master bitmap handed to
    // the cell, the mirror fields cleared with 16 directions, tripled
    // speed, the empty fire sound, all seven trail colours 'A' with the
    // trail tick cleared, and child type 78 again (the base ctor already
    // stored it).
    CProjectileExplodingFlame();

    /* 0088 */ virtual void Explode();

    // Native exe offsets, past the tail of m_explodeCell2; ours lay out after
    // the base under the reduced packing.
    /* 04B2 */ BYTE m_trailColorRanges[7];
    /* 04B9 */ BYTE m_trailTick;
};

// The skull trap missile (vtable 0x84DD80): Explode (slot 0x0088) is the
// ShSkull burst sound fire-and-forget on channel 14 followed by the bone
// chunk spray the shatter effect throws (CGameChunk, sized by the trap's
// own child type). AIUpdate (0x52F9E0) stays parked from i065 on this
// class, so that slot inherits the base virtual.
class CProjectileSkullTrap : public CProjectileExploding {
public:
    // 0x52F260 (ret 0xC — the resrefs pass by reference in the exe): binds
    // the spell's travel BAM through the exploding base, then makes the
    // burst BAM a non-double-sized shadow cell, sequence 0 on both cells,
    // clears the mirror fields, one strike of a 100-tick linger with the
    // pre-scan on, child type 78 again (the base ctor already stored it),
    // speed doubled with a native WORD shift, and arms both explosion
    // cells — cell 1 with the travel BAM, cell 2 with the burst BAM, each
    // flagged active only when its ref is non-empty. DecodeProjectile's
    // types 96 (Skull Trap) and 100 (Glyph of Warding) build this.
    CProjectileSkullTrap(const CResRef& cMissileRef, const CResRef& cExplodeRef,
        SHORT nType);
    /* 0088 */ virtual void Explode();

    // Native exe offsets, past the tail of m_explodeCell2; ours lay out after
    // the base under the reduced packing.
    /* 04B2 */ SHORT m_nType;
    // The exe member is the 4-byte VC6 CString: the ctor's nil-sentinel
    // store at +0x4B4, the case bodies' CString::operator= assigns into it,
    // and the decoder's operator-new size 0x4B8 is this member plus 4.
    /* 04B4 */ CString m_explodeSound;
};

// The ACOWG1 cow shot (vtable 0x84DA4C): DecodeProjectile's type 189. The
// ctor is a pure pass-through — the decoder hands it the cell's resref by
// value (ret 0x8) straight to the base, and the class adds no members
// (op-new 0x2A0, the CProjectileBAM base exactly). Against the
// CProjectileBAM vtable (0x84D9C4) the class overrides only the deleting
// dtor (0x529AB0 over the base's 0x52B010 — ours is implicit), AIUpdate
// 0x536D40 and Fire 0x5369A0; neither body is in-tree, so both stay
// undeclared (IcewindCProjectileScorcher precedent) and ours inherits the
// base virtuals.
class CProjectile84DA4C : public CProjectileBAM {
public:
    // 0x529A90
    CProjectile84DA4C(CResRef resRef);
};

// The plain SPFIREBL flame jet (vtable 0x84DE0C): DecodeProjectile's types
// 80 and 105 — the jet as cast, and the same jet with its palette range
// widened one entry (0x44 over the ctor's 0x43). It adds no members of its
// own: the ctor writes only inherited fields. Against the CProjectileBAM
// vtable (0x84D9C4) the exe overrides only the deleting dtor (0x52FD50 over
// the base's 0x52B010 — ours is implicit; an earlier reading diffed against
// CProjectile's 0x84D7B8 and counted the base's own AIUpdate 0x52B900,
// Render 0x52B190, Fire 0x52C050, GetField_E6 0x5297D0 and
// UpdateBasedOnTargetPoint 0x52BD20 as this class's — all inherited
// unchanged), so nothing is declared here and ours inherits the base
// virtuals throughout.
class CProjectile84DE0C : public CProjectileBAM {
public:
    // 0x52FC20 (ret 0x4 — the cell's name arrives as a char* and folds
    // through a CResRef temp into the base): sequence 0, palette range 0
    // recoloured 0x43 off the master bitmap and handed to the animation
    // (SetRange before SetPalette, the reverse of the decoder's tints),
    // mirror bounds 15/11 with mirroring on, field_1D2 = 0x80, 16
    // directions, speed times 3, and the no-callback / no-visual-effect
    // stamps.
    CProjectile84DE0C(const char* szResRef);
};

// The SPLightB searing-light shot (vtable 0x84DE94): DecodeProjectile's
// types 206 and 380 — the same flight either as a single pass or with six
// rebounds, per the bounce count the decoder stamps at +0x2A0. Its Fire
// (0x530010) spawns a rebounding-bolt child (ctor 0x57D390, type 207) and
// hands that count over as the child's m_nBounces (+0x2B2). Against the
// CProjectileBAM vtable (0x84D9C4) the exe overrides exactly two slots —
// the deleting dtor 0x52FED0 (over the base's 0x52B010; ours is implicit)
// and Fire 0x530010 (the bounce hand-off above; not in-tree, undeclared
// per the IcewindCProjectileScorcher precedent — a declared-but-undefined
// virtual would become the key function and leave the vtable unemitted).
// An earlier reading diffed against CProjectile's 0x84D7B8 and counted
// the base's own AIUpdate 0x52B900, Render 0x52B190, GetField_E6
// 0x5297D0 and UpdateBasedOnTargetPoint 0x52BD20 as this class's too;
// those are inherited unchanged, so ours inherits the base virtuals in
// every slot but Fire.
class CProjectile84DE94 : public CProjectileBAM {
public:
    // Native exe offset; ours lays out after sizeof(CProjectileBAM) = 0x2A2
    // under the reduced packing — the same drift as CProjectileExploding's
    // tail and CProjectile84E030's WORD m_nCount, whose DWORD-at-0x2A0 also
    // spans the base's field_2A0/field_2A1.
    /* 02A0 */ LONG m_nBounces;
    // 0x52FE90 (plain ret — no stack args; the cell is hardcoded in the
    // ctor body rather than passed by the decoder)
    CProjectile84DE94();
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

// Magic Missile, types 68 to 78 (vtable 0x84E0C4): 1 to 11 missiles of type
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

// The dart projectile (vtable 0x84E25C): a CProjectileBAM on the DART
// animation with 16 directions and its speed tripled. It adds no members of
// its own — the ctor writes only inherited fields.
class CProjectileDart : public CProjectileBAM {
public:
    CProjectileDart();
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

// The arrow projectile (vtable 0x84E58C): a CProjectileBAM on the ARARROW
// animation with the palette ranges SetRange(5, 27), SetRange(4, 23) and
// SetRange(2, 46) off the master bitmap, tinted, 16 directions and its speed
// quintupled. It adds no members of its own — the ctor writes only inherited
// fields.
class CProjectileArrow : public CProjectileBAM {
public:
    CProjectileArrow();
};

// The SPFLMARR projectile (vtable 0x84E614): a CProjectileBAM on the SPFLMARR
// flame-arrow animation with the palette ranges SetRange(0, 67) and
// SetRange(1, 46) off the master bitmap, 16 directions and its speed
// quintupled. It names the seven trail colour-range bytes at +0x2A0 (all
// stamped 'A') and the trail tick at +0x2A7 — the BAM ctor leaves +0x2A0 to
// the subclasses (it only writes +0x29D/+0x29E there), so — like
// CProjectileSPBRNHND's m_animationLifetime — the derived class names the
// bytes. Its AIUpdate (0x5329A0, parked from i052) animates the trail.
class CProjectileSPFLMARR : public CProjectileBAM {
public:
    CProjectileSPFLMARR();

    /* 02A0 */ BYTE m_trailColorRanges[7];
    /* 02A7 */ BYTE m_trailTick;
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
