#ifndef CPROJECTILE_H_
#define CPROJECTILE_H_

#include "CGameEffectList.h"
#include "CGameObject.h"
#include "CSound.h"

class CGameAIBase;
class CGameArea;
class CGameSprite;

class CProjectile : public CGameObject {
public:
    CProjectile();
    /* 0000 */ ~CProjectile() override;
    /* 002C */ BOOLEAN CompressTime(DWORD deltaTime) override;
    /* 0068 */ BOOLEAN IsProjectile() override;
    /* 006C */ virtual void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType);
    /* 0070 */ virtual void OnArrival();
    /* 0074 */ virtual void RemoveSelf();
    /* 0078 */ virtual void DeliverEffects();
    /* 007C */ virtual void CallBack();

    static CProjectile* DecodeProjectile(WORD nProjectileType, CGameAIBase* pSprite, BYTE nUnknown);
    // NOTE: It returns NULL, after an assert, only for a type it does not
    // know; the original's callers (0x482073, 0x56024E, 0x5759FC, 0x586E1C,
    // 0x762526) use the result without a NULL check.
    static CProjectile* DecodeSpellHitProjectile(DWORD nType, CGameSprite* pSprite, BOOLEAN bPriest);

    void AddEffect(CGameEffect* pEffect);
    void AddEffect2(CGameEffect* pEffect);
    void ClearEffects();
    LONG DetermineHeight(CGameSprite* pSprite);
    SHORT GetDirection(CPoint target);
    BOOL IsTargetImmune(CGameSprite* pSprite);
    void PlaySound(CResRef resRef, BOOL loop, BOOL fireAndForget);
    static void GetSourcePosition(LONG nObjectId, CPoint& pt, BOOLEAN bLaunchOffset);
    static SHORT GetLaunchOffset(CGameSprite* pSprite);

    /* 006E */ WORD m_projectileType;
    /* 0070 */ SHORT m_speed;
    /* 0072 */ LONG m_sourceId;
    /* 0076 */ LONG m_targetId;
    /* 007A */ LONG m_callBackProjectile;
    /* 007E */ CGameEffectList m_effectList;
    /* 009A */ WORD m_nSparkleColour;
    /* 009C */ LONG m_posExactX;
    /* 00A0 */ LONG m_posExactY;
    /* 00A4 */ LONG m_deltaX;
    /* 00A8 */ LONG m_deltaY;
    /* 00AC */ LONG m_driftX;
    /* 00B0 */ LONG m_driftY;
    /* 00B4 */ LONG m_randomMinX;
    /* 00B8 */ LONG m_randomMinY;
    /* 00BC */ LONG m_randomMaxX;
    /* 00C0 */ LONG m_randomMaxY;
    /* 00C4 */ BOOL m_bTrackTargetHeight;
    /* 00C8 */ CPoint m_targetPos;
    /* 00D0 */ BYTE m_terrainTable[16];
    /* 00E0 */ WORD m_driftDecay;
    /* 00E2 */ BOOL m_bSparkle;
    /* 00E6 */ DWORD field_E6;
    /* 00EA */ CGameArea* m_pArea;
    /* 00EE */ CSound m_sound;
    /* 0152 */ CResRef m_fireSoundRef;
    /* 015A */ BOOL m_loopFireSound;
    /* 015E */ CResRef m_arrivalSoundRef;
    /* 0166 */ BOOL m_loopArrivalSound;
    /* 016A */ BOOLEAN m_bHasHeight;
    /* 016C */ SHORT m_nStartHeight;
    /* 016E */ SHORT m_nCurrentHeight;
    /* 0170 */ LONG m_nStartDistance;
    /* 0174 */ LONG field_174;
    /* 0178 */ LONG field_178;
    /* 017C */ BYTE field_17C;
    /* 017E */ CString m_sVisualEffect;
    /* 0182 */ LONG field_182;
    /* 0186 */ DWORD m_nSpellLevel;
    /* 018A */ CResRef m_sourceRes;
};

// The projectile DecodeProjectile builds for type 1 and, as its fallback, for
// a type it does not know: it has no animation and delivers its effects the
// moment it is fired (vtable 0x84D8B8).
class CProjectile84D8B8 : public CProjectile {
public:
    CProjectile84D8B8();
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
};

#endif /* CPROJECTILE_H_ */
