#ifndef CVISUALEFFECT_H_
#define CVISUALEFFECT_H_

#include "mfc.h"

#include "CGameObject.h"
#include "CResRef.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"

class CVEFEvent;

// IWD2 visual effects (VEF). Offset comments give the exe's packed (2-byte)
// layout and are informational only: this tree compiles without #pragma pack,
// so members drift by name rather than by address. The fork's CVEFHeader,
// CVEFEvent and CVisualEffectFile companions belong to the loader half
// (0x5544C0/0x5545E0/0x554D20) and are not imported with this batch.

class CVisualEffectBase {
public:
    /* 0000 */ DWORD m_nTimedEventOffset;
    /* 0004 */ DWORD m_nTimedEventCount;
    /* 0008 */ DWORD m_nTriggeredEventOffset;
    /* 000C */ DWORD m_nTriggeredEventCount;
    /* 0010 */ DWORD m_nTargetType;
    /* 0014 */ LONG m_nSpeed;
    /* 0018 */ LONG m_nDuration;
    /* 001C */ CPoint m_ptStartDeltaV;
    /* 0024 */ LONG m_nDeltaVDec;
    /* 0028 */ LONG m_nHeight;
    /* 002C */ DWORD m_flags;
    /* 0030 */ DWORD m_extra[100];
};

class CVEFVidCellBase {
public:
    /* 0000 */ CResRef m_cVidCellRef;
    /* 0008 */ CResRef m_cShadowVidCellRef;
    /* 0010 */ DWORD m_bltFlags;
    /* 0014 */ DWORD m_bltInfo;
    /* 0018 */ DWORD m_bltFlagsExtra;
    /* 001C */ DWORD m_bltInfoExtra;
    /* 0020 */ CPoint m_offset;
    /* 0028 */ DWORD m_offsetFlags;
    /* 002C */ LONG m_animationSpeed;
    /* 0030 */ LONG m_numDirections;
    /* 0034 */ LONG m_direction;
    /* 0038 */ DWORD m_directionFlags;
    /* 003C */ CResRef m_cNewPaletteRef;
    /* 0044 */ LONG m_height;
    /* 0048 */ CPoint m_glowSize;
    /* 0050 */ DWORD m_glowIntensity;
    /* 0054 */ LONG m_nDuration;
    /* 0058 */ CResRef m_cVVCResRes;
    /* 0060 */ LONG m_nStartSequence;
    /* 0064 */ LONG m_nLoopSequence;
    /* 0068 */ LONG m_nCurrentSequence;
    /* 006C */ DWORD m_sequenceFlags;
    /* 0070 */ CResRef m_cStartSoundRef;
    /* 0078 */ CResRef m_cLoopSoundRef;
    /* 0080 */ CResRef m_cAlphaBamRef;
    /* 0088 */ LONG m_nEndSequence;
    /* 008C */ CResRef m_cEndSoundRef;
    /* 0094 */ DWORD m_extra[84];
};

class CVEFVidCell : public CGameObject, public CVEFVidCellBase {
public:
    static const SHORT OFFSET_FROMX_X[16];
    static const SHORT OFFSET_FROMX_Y[16];
    static const SHORT OFFSET_FROMY_X[16];
    static const SHORT OFFSET_FROMY_Y[16];

    void FrameAdvance(LONG nCounter);
    void CalculatePos(const CPoint& pos);
    void SetDirection(const CPoint& pos, const CPoint& oldPos, const CPoint& targetPos, SHORT nDirection);
    static SHORT GetDirection(const CPoint& pos, const CPoint& targetPos, SHORT nDirection);

    /* 0252 */ CVidCell* m_cVidCell;
    /* 0256 */ CVidCell* m_cShadowVidCell;
    /* 025A */ CVidPalette m_palette;
    /* 027E */ CVidBitmap m_newPalette;
    /* 0338 */ BOOL m_done;
    /* 033C */ LONG m_newDirection;
};

class CVisualEffect : public CGameObject, public CVisualEffectBase {
public:
    static const LONG MAX_FRAME_RATE;

    void AIUpdate() override;
    void Move(const CPoint& targetPos);

    /* 022E */ CList<LONG, LONG> m_activeAnimations;
    /* 024A */ CTypedPtrList<CPtrList, CVEFEvent*> m_timedEvents;
    /* 0266 */ CTypedPtrList<CPtrList, CVEFEvent*> m_triggeredEvents;
    /* 0282 */ CPoint m_ptTargetPos;
    /* 028A */ LONG m_nTargetId;
    /* 028E */ CPoint m_posExact;
    /* 0296 */ CPoint m_posOld;
    /* 029E */ LONG m_nExistanceCounter;
    /* 02A2 */ LONG m_nDirection;
    /* 02A6 */ LONG m_nNewDirection;
    /* 02AA */ CPoint m_posDelta;
    /* 02B2 */ BYTE m_terrainTable[16];
};

#endif /* CVISUALEFFECT_H_ */
