#ifndef CVISUALEFFECT_H_
#define CVISUALEFFECT_H_

#include "mfc.h"

#include <cstring>

#include "CGameObject.h"
#include "CRes.h"
#include "CResBinary.h"
#include "CResRef.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"

// IWD2 reads this 0x24-byte prefix; event offsets are from the resource start.
struct CVEFHeader {
    /* 0000 */ char signature[4];
    /* 0004 */ char version[4];
    /* 0008 */ DWORD timedOffset;
    /* 000C */ DWORD timedCount;
    /* 0010 */ DWORD triggeredOffset;
    /* 0014 */ DWORD triggeredCount;
    /* 0018 */ DWORD targetType;
    /* 001C */ LONG speed;
    /* 0020 */ LONG duration;
};

class CVEFEvent {
public:
    CVEFEvent() { memset(this, 0, sizeof(CVEFEvent)); }
    /* 0x0000 */ LONG m_nTimeFrame;
    /* 0x0004 */ DWORD m_dwFlags;
    /* 0x0008 */ LONG m_nRepeatRate;
    /* 0x000C */ LONG m_nResType;
    /* 0x0010 */ CResRef m_res;
    /* 0x0018 */ DWORD m_specialFlags;
    /* 0x001C */ DWORD m_pad[49];
};

class CVisualEffectBase {
public:
    /* 0x0000 */ DWORD m_nTimedEventOffset;
    /* 0x0004 */ DWORD m_nTimedEventCount;
    /* 0x0008 */ DWORD m_nTriggeredEventOffset;
    /* 0x000C */ DWORD m_nTriggeredEventCount;
    /* 0x0010 */ DWORD m_nTargetType;
    /* 0x0014 */ LONG m_nSpeed;
    /* 0x0018 */ LONG m_nDuration;
    /* 0x001C */ CPoint m_ptStartDeltaV;
    /* 0x0024 */ LONG m_nDeltaVDec;
    /* 0x0028 */ LONG m_nHeight;
    /* 0x002C */ DWORD m_flags;
    /* 0x0030 */ DWORD m_extra[100];
};

class CVEFVidCellBase {
public:
    /* 0x0000 */ CResRef m_cVidCellRef;
    /* 0x0008 */ CResRef m_cShadowVidCellRef;
    /* 0x0010 */ DWORD m_bltFlags;
    /* 0x0014 */ DWORD m_bltInfo;
    /* 0x0018 */ DWORD m_bltFlagsExtra;
    /* 0x001C */ DWORD m_bltInfoExtra;
    /* 0x0020 */ CPoint m_offset;
    /* 0x0028 */ DWORD m_offsetFlags;
    /* 0x002C */ LONG m_animationSpeed;
    /* 0x0030 */ LONG m_numDirections;
    /* 0x0034 */ LONG m_direction;
    /* 0x0038 */ DWORD m_directionFlags;
    /* 0x003C */ CResRef m_cNewPaletteRef;
    /* 0x0044 */ LONG m_height;
    /* 0x0048 */ CPoint m_glowSize;
    /* 0x0050 */ DWORD m_glowIntensity;
    /* 0x0054 */ LONG m_nDuration;
    /* 0x0058 */ CResRef m_cVVCResRes;
    /* 0x0060 */ LONG m_nStartSequence;
    /* 0x0064 */ LONG m_nLoopSequence;
    /* 0x0068 */ LONG m_nCurrentSequence;
    /* 0x006C */ DWORD m_sequenceFlags;
    /* 0x0070 */ CResRef m_cStartSoundRef;
    /* 0x0078 */ CResRef m_cLoopSoundRef;
    /* 0x0080 */ CResRef m_cAlphaBamRef;
    /* 0x0088 */ LONG m_nEndSequence;
    /* 0x008C */ CResRef m_cEndSoundRef;
    /* 0x0094 */ DWORD m_extra[84];
};

// Recovered prefix and the animation tail through m_newDirection; loading,
// rendering and sound have not been recovered yet.
class CVEFVidCell : public CGameObject, public CVEFVidCellBase {
public:
    static const SHORT OFFSET_FROMX_X[16];
    static const SHORT OFFSET_FROMX_Y[16];
    static const SHORT OFFSET_FROMY_X[16];
    static const SHORT OFFSET_FROMY_Y[16];

    void FrameAdvance(LONG nCounter); // 0x5513F0
    void CalculatePos(const CPoint& pos); // 0x551490
    void SetDirection(const CPoint& pos, const CPoint& oldPos, const CPoint& targetPos, SHORT nDirection); // 0x551520
    static SHORT GetDirection(const CPoint& pos, const CPoint& targetPos, SHORT nDirection); // 0x551670

    /* 0x0252 */ CVidCell* m_cVidCell;
    /* 0x0256 */ CVidCell* m_cShadowVidCell;
    /* 0x025A */ CVidPalette m_palette;
    /* 0x027E */ CVidBitmap m_newPalette;
    /* 0x0338 */ BOOL m_done;
    /* 0x033C */ LONG m_newDirection;
};

class CVisualEffect : public CGameObject, public CVisualEffectBase {
public:
    static const LONG MAX_FRAME_RATE;

    CVisualEffect(); // 0x554870
    ~CVisualEffect() override; // 0x5549D0
    void AIUpdate() override; // 0x554B70
    void SetAllDelete(); // 0x555200
    void Move(const CPoint& targetPos); // 0x554F00
    static LONG Load(CString res, CGameArea* pArea, const CPoint& pos,
        LONG targetId, const CPoint& targetPos, LONG height); // 0x554D20

    /* 0x022E */ CList<LONG, LONG> m_activeAnimations;
    /* 0x024A */ CTypedPtrList<CPtrList, CVEFEvent*> m_timedEvents;
    /* 0x0266 */ CTypedPtrList<CPtrList, CVEFEvent*> m_triggeredEvents;
    /* 0x0282 */ CPoint m_ptTargetPos;
    /* 0x028A */ LONG m_nTargetId;
    /* 0x028E */ CPoint m_posExact;
    /* 0x0296 */ CPoint m_posOld;
    /* 0x029E */ LONG m_nExistanceCounter;
    /* 0x02A2 */ LONG m_nDirection;
    /* 0x02A6 */ LONG m_nNewDirection;
    /* 0x02AA */ CPoint m_posDelta;
    /* 0x02B2 */ BYTE m_terrainTable[16];
};

class CVisualEffectFile : public CResHelper<CResBinary, 1020> {
public:
    CVisualEffectFile() = default;
    explicit CVisualEffectFile(CResRef res); // 0x5544C0
    CVisualEffect* GetVEF(); // 0x5545E0
};

#endif /* CVISUALEFFECT_H_ */
