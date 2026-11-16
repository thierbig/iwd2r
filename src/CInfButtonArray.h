#ifndef CINFBUTTONARRAY_H_
#define CINFBUTTONARRAY_H_

#include "CVidCell.h"

class CButtonData;
class CGameButtonList;
class CGameSprite;

class CInfButtonSettings {
public:
    CInfButtonSettings();
    ~CInfButtonSettings();

    /* 0000 */ int field_0; // shown
    /* 0004 */ int field_4; // shows the icon `field_14`
    /* 0008 */ int field_8; // shows a frame of GUIBTACT
    /* 000C */ int field_C; // GUIBTACT frame
    /* 0010 */ int field_10; // GUIBTACT frame when selected
    /* 0014 */ CVidCell field_14; // icon
    /* 00EE */ CVidCell field_EE; // launcher icon
    /* 01C8 */ int field_1C8;
    /* 01CC */ int field_1CC; // selected
    /* 01D0 */ int field_1D0; // the quick weapon belongs to the active weapon set
    // NOTE: Never set by the original (not even its constructor); read once
    // by UpdateButtons. Ours starts at 0.
    /* 01D4 */ int field_1D4;
    /* 01D8 */ int field_1D8; // count
    /* 01DC */ BOOL m_bGreyOut;
};

class CInfButtonArray {
public:
    static const BYTE STATE_NONE;

    static CGameButtonList* dword_8E6820;

    CInfButtonArray();
    ~CInfButtonArray();

    static void GetSelectedQuickWeaponData(CButtonData& cButtonData);
    static BYTE GetSelectedModalMode();
    BYTE GetButtonId(INT buttonType);
    BOOL ResetState();
    void UpdateState();
    BOOL SetState(INT nState, BYTE bPush);
    BOOL UpdateButtons();
    BOOL OnLButtonPressed(int buttonID);
    BOOL OnRButtonPressed(int buttonID);
    void UpdateCustomizableButtonTypes(INT* pButtonList);
    void UpdateSelectedWeaponSet(BYTE nIndex);

    void SetSelectedButton(INT nSelectedButton);

    static void sub_587BD0();
    static CGameButtonList* sub_587C20(INT nIndex, INT nType, BYTE& nClass, DWORD nDomain, BOOLEAN bItemUsages);
    static CGameButtonList* sub_587DF0(const CResRef& cResRef);
    static void sub_587F80(BYTE buttonNum, CButtonData& buttonData, INT nType);
    static void sub_588300(BYTE buttonNum, CButtonData& buttonData);
    static BOOLEAN sub_5884B0(CButtonData* pButtonData, BOOL bFlag);
    static void sub_588570(SHORT buttonNum, INT nType);
    static BOOLEAN sub_5886A0(CButtonData* pButtonData, BOOL bFlag);
    static BOOLEAN sub_588760(CButtonData* pButtonData, BOOL bFlag);
    BOOLEAN sub_588820(CButtonData* pButtonData, BOOL bFlag);
    static void sub_588CB0(CButtonData* pButtonData, BYTE buttonNum, INT nType);
    void sub_589FF0(BYTE bPush, BYTE bAll);
    BOOL sub_594170(INT nButtonType);
    void sub_594280(INT nButtonType, CGameSprite* pSprite);
    BOOL sub_5950F0(const CPoint& pt, const CRect& rClip, BOOL bPressed, INT nButton);
    BOOL sub_5957C0(const CPoint& pt, const CRect& rClip, BOOL bPressed, INT nButton);
    static BOOL sub_5959A0(INT nButtonType);
    static BOOL sub_595C80();
    static BOOL sub_595D60(const BYTE& nClass);
    static BOOLEAN sub_595EB0(CGameSprite* pSprite);
    INT sub_595FB0(CGameButtonList* pButtonList);
    INT sub_596040(CGameButtonList* pButtonList);
    void sub_5960E0();

    /* 0000 */ CInfButtonSettings m_buttonArray[12];
    /* 16B0 */ INT m_buttonTypes[12];
    /* 16E0 */ int field_16E0;
    /* 16E4 */ int field_16E4;
    /* 16E8 */ CVidCell field_16E8; // GUIBTBUT
    /* 17C2 */ CVidCell field_17C2; // GUIBTACT
    /* 189C */ CVidCell field_189C;
    /* 1976 */ int field_1976; // quick slot being customized
    /* 197A */ int field_197A; // first list entry shown
    /* 197E */ INT m_nSelectedButton;
    /* 1982 */ INT m_nState;
    // The original's std::deque<INT> (0x1986), used as a stack at its front;
    // CGameArea reads its size at 0x19B2 as `field_19B2`, kept equal to
    // `m_stateStack.GetCount()`.
    /* 1986 */ CList<INT, INT> m_stateStack;
    /* 19B2 */ int field_19B2;
    /* 19B6 */ INT m_customizableButtonTypes[9];
    /* 19DA */ BYTE m_nCurrentSelectedSpellClass;
    /* 19DC */ DWORD field_19DC; // domain of the selected cleric list
    /* 19E0 */ CResRef field_19E0; // the feat ability whose list is shown
    /* 19E8 */ BYTE m_nSelectedWeaponSet;
};

#endif /* CINFBUTTONARRAY_H_ */
