#ifndef CWARP_H_
#define CWARP_H_

#include "mfc.h"

#include "CChitin.h"
#include "CDimm.h"
#include "CSoundMixer.h"
#include "CVidMode.h"
#include "CVideo.h"

class CKeyInfo;
class CWarp;

// #guess: the timer CWarp::lTimers holds. Nothing in iwd2.exe ever constructs
// one and neither IEex's names nor the exports name the class, so the name,
// the virtual's name and every field's meaning are ours; only the layout is
// the binary's - CWarp::TimerUpdate (0x7B5550) reads exactly these offsets:
// +0x04 set fires the timer on every update, +0x08/+0x0C/+0x10/+0x14 gate the
// self-removal (+0x10 is the owning CWarp, whose lTimers sits at +0xC, and
// +0x14 is the timer's own POSITION in it), +0x18 is the period and +0x1C the
// accumulator of 10000 / CChitin::TIMER_UPDATES_PER_SECOND. In release MFC
// CObject has three virtuals, so the class's first own virtual is at
// vtable+0xC there; our compat CObject has one (the destructor), so it sits at
// +0x4 here - every call is by name, so behaviour is the same.
class CWarpTimer : public CObject {
public:
    /* 0004 */ BOOL field_4; // #guess: fires every update
    /* 0008 */ LONG field_8; // #guess: non-zero allows the self-removal
    /* 000C */ LONG field_C; // #guess: cleared when the timer removes itself
    /* 0010 */ CWarp* field_10; // #guess: the owning engine
    /* 0014 */ POSITION field_14; // #guess: its POSITION in the owner's lTimers
    /* 0018 */ LONG field_18; // the period, in units of 10000 / updates per second
    /* 001C */ LONG field_1C; // the accumulator

    // #guess: pure because nothing in the binary implements or constructs a
    // timer; the class is a declared layout, and a real timer supplies this.
    virtual void Update() = 0; // #guess: vtable+0xC in the original
};

class CWarp : public CObject {
public:
    static CSoundMixer* pSoundMixer;
    static CDimm* pDimm;
    static CVideo* pVideo;
    static CChitin* pChitin;

    CWarp();
    /* 0000 */ /* GetRuntimeClass */
    /* 0004 */ ~CWarp() override;
    /* 0008 */ /* Serialize */
    /* 000C */ virtual void InvalidateCursorRect(const CRect& rect);
    /* 0010 */ virtual void NormalizePanelRect(DWORD nID, CRect& rect);
    /* 0014 */ virtual void EngineActivated();
    /* 0018 */ virtual void EngineDeactivated();
    /* 001C */ virtual void EngineDestroyed();
    /* 0020 */ virtual void EngineInitialized();
    /* 0024 */ virtual void EngineGameInit();
    /* 0028 */ virtual void EngineGameUninit();
    /* 002C */ virtual void SelectEngine(CWarp* pWarp);
    /* 0030 */ virtual BOOL CheckSystemKeyCtrl();
    /* 0034 */ virtual void SetSystemKeyCtrl(BOOLEAN bValue);
    /* 0038 */ virtual BOOL GetCtrlKey();
    /* 003C */ virtual BOOL CheckSystemKeyShift();
    /* 0040 */ virtual void SetSystemKeyShift(BOOL bValue);
    /* 0044 */ virtual BOOL GetShiftKey();
    /* 0048 */ virtual BOOL CheckSystemKeyMenu();
    /* 004C */ virtual void SetSystemKeyMenu(BOOL bValue);
    /* 0050 */ virtual BOOL CheckSystemKeyCapsLock();
    /* 0054 */ virtual void SetSystemKeyCapsLock(BOOL bValue);
    /* 0058 */ virtual BOOL GetCapsLockKey();
    /* 005C */ virtual BOOL GetMenuKey();
    /* 0060 */ virtual BOOL CheckMouseMove();
    /* 0064 */ virtual void OnMouseMove(CPoint pt);
    /* 0068 */ virtual BOOL CheckMouseLButton();
    /* 006C */ virtual void OnLButtonDblClk(CPoint pt);
    /* 0070 */ virtual void OnLButtonDown(CPoint pt);
    /* 0074 */ virtual void OnLButtonUp(CPoint pt);
    /* 0078 */ virtual BOOL CheckMouseMButton();
    /* 007C */ virtual void OnMButtonDblClk(CPoint pt);
    /* 0080 */ virtual void OnMButtonDown(CPoint pt);
    /* 0084 */ virtual void OnMButtonUp(CPoint pt);
    /* 0088 */ virtual BOOL CheckMouseRButton();
    /* 008C */ virtual void OnRButtonDblClk(CPoint pt);
    /* 0090 */ virtual void OnRButtonDown(CPoint pt);
    /* 0094 */ virtual void OnRButtonUp(CPoint pt);
    /* 0098 */ virtual BOOL CheckMouseWheel();
    /* 009C */ virtual void OnMouseWheel(BOOL bForward, LONG nTicks, DWORD nLines, WORD wFlags);
    /* 00A0 */ virtual SHORT GetNumVirtualKeys();
    /* 00A4 */ virtual CKeyInfo* GetVirtualKeys();
    /* 00A8 */ virtual BYTE* GetVirtualKeysFlags();
    /* 00AC */ virtual void OnKeyDown(SHORT a2);
    /* 00B0 */ virtual void EnableEditKeys(SHORT a2);
    /* 00B4 */ virtual void DisableEditKeys();
    /* 00B8 */ virtual void ResetControls();
    /* 00BC */ virtual void OnVideoChange();
    /* 00C0 */ virtual void TimerAsynchronousUpdate();
    /* 00C4 */ virtual void TimerSynchronousUpdate();
    /* 00C8 */ virtual void TimerUpdate();

    BOOL SetVideoMode(int a1);

    /* 0004 */ CVidMode* pVidMode; // #guess
    /* 0008 */ CWarp* pLastEngine;
    /* 000C */ CObList lTimers;
};

#endif /* CWARP_H_ */
