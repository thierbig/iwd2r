#ifndef CPROJECTILE84E9C0_H_
#define CPROJECTILE84E9C0_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"

// An animation played once at its target, in its own palette when one is
// given (vtable 0x84E9C0): the Visual Spell Hit effect's local projectile
// ("SHAir", "SHEarth", "SHWater" with the palettes "SHAir1".."SHAir7" and so
// on, "SPFDEATH") and many types of DecodeProjectile. It delivers its
// effects when fired and arrives at the end of its sequence.
class CProjectile84E9C0 : public CProjectile {
public:
    CProjectile84E9C0(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue);
    /* 0000 */ ~CProjectile84E9C0() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 0192 */ CVidCell m_animation;
    /* 026C */ CVidBitmap m_palette;
    /* 0326 */ DWORD m_dwFlags;
    /* 032A */ BOOLEAN m_bPalette;
    /* 032B */ BYTE m_nTransValue;
};

// The type-45 smoke jet (vtable 0x84D838): a memberless subclass whose
// construction is fully inlined in the DecodeProjectile type-45 arm
// (0x5201C0 - it news 0x32C bytes, runs the CProjectile84E9C0 constructor
// and stamps this class's vtable at 0x520223, then re-stores an empty
// m_fireSoundRef). It overrides only Fire (and the destructor the compiler
// generates for the memberless class, 0x529990/0x5299B0); everything else
// is the base class.
class CProjectile84D838 : public CProjectile84E9C0 {
public:
    CProjectile84D838(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue);
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
};

#endif /* CPROJECTILE84E9C0_H_ */
