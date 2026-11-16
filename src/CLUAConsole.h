#ifndef CLUACONSOLE_H_
#define CLUACONSOLE_H_

#include "mfc.h"

// The Lua debug console's command buffer: the stack object the dispatcher
// 0x5838A0 (Execute, "CLUAConsole" in docs/followups.md's original note)
// parses each entered "CtrlAltDelete:" line into. The Lua console edit's
// Enter handler (0x77BF30, CUIControlEditLua::OnEditReturn) constructs one
// per line and destroys it at scope end (the destructor 0x4EDA50 through
// its SEH unwind pad). Layout from those two listings; the offsets are the
// exe's, the field names are the tree's.
class CLUAConsole {
public:
    // 0x77C350
    // The Enter handler's inlined per-line construction: the empty-string
    // store for +0x4 at 0x77C350, the array ctors for +0x10 (0x7FB907 at
    // 0x77C366) and +0x24 (0x7FB98E at 0x77C377), and the five zero stores
    // at 0x77C37C-0x77C38C. A body so the handler (and the tests) build
    // one the way the exe does; the members with ctors carry their own.
    CLUAConsole();

    ~CLUAConsole();

    // 0x584400
    CString GetCommandWord();

    // 0x584320
    void CommandNotFound();

    // 0x584420
    int CountArgs();

    // 0x583E60
    void ResolveCommand(CString sWord);

    // 0x5842C0
    void AddTextArg(CString sArg);

    // 0x5840E0
    void ParseArgs(CString sArgs);

    // 0x5838A0
    void Execute(CString sInput);

    /* 0000 */ DWORD field_0;
    /* 0004 */ CString field_4;
    /* 0008 */ DWORD field_8;
    /* 000C */ DWORD field_C;
    // The parsed argument value table: the dispatch arms read their DWORD
    // arguments through the CArray data pointer at this+0x14 (SETGLOBAL's
    // dwValue, fetched in the arm at 0x583D22 before calling 0x4ECAD0).
    /* 0010 */ CDWordArray field_10;
    // The command token store: the arms fetch their text arguments through
    // 0x65F8F0 with this+0x24 as the store (0x583D3D in the SETGLOBAL arm).
    /* 0024 */ CStringArray field_24;
    // The dispatcher hands &field_38 (this+0x40 for the FIRSTAID/HANS/MIDAS
    // arms) to the handlers in ecx; every ported handler ignores it (see
    // src/CtrlAltDelete.h).
    /* 0038 */ DWORD field_38;
    /* 003C */ DWORD field_3C;
};

// 0x5844E0
CString EncodeCommandWord(CString sText);

#endif /* CLUACONSOLE_H_ */
