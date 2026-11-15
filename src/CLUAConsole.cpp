#include "CLUAConsole.h"

#include "CtrlAltDelete.h"

// 0x4EDA50 CLUAConsole::~CLUAConsole() — 90 bytes (0x4EDA50-0x4EDAAA), a
// caller-cleaned thiscall (plain `ret` at 0x4EDAA9; there is no deleting
// wrapper because the object is a stack local of the Enter handler 0x77BF30).
// SEH prologue with the state byte stepping 1 -> 0 -> -1 across the three
// member calls.
//
// NOTE: The original spells the member destruction explicitly, in reverse
// declaration order: the token store at +0x24 (lea ecx,[esi+0x24] 0x4EDA6D,
// the CStringArray dtor 0x7FB9C1 at 0x4EDA78 — its element loop 0x7FB9FF
// runs the CString dtor per token, then frees the buffer), the value table
// at +0x10 (0x4EDA7D, the CArray dtor 0x7FB93A at 0x4EDA85 — buffer free
// only, the DWORD elements are trivial), then the CString at +0x4 (0x4EDA8A,
// CString dtor 0x7FCC1A at 0x4EDA95). The DWORD members at +0/+8/+0xC/
// +0x38/+0x3C need no call. The members carry that destruction here. No
// vftable is stored: the class has no base and no virtuals.
CLUAConsole::~CLUAConsole()
{
}

// 32 bytes (0x584400-0x58441F). thiscall with a by-value CString return:
// the caller pushes the hidden return buffer, the body runs the CString copy
// ctor (0x7FC98F) with that buffer as its this and the member at +0x4 as the
// source, and `ret 4` drops the buffer pointer.
//
// The accessor behind the console's unknown-word report: 0x584320's -1 arm
// concatenates it after the "Command not found : " prefix (0x8AF7DC).
// 0x584400
CString CLUAConsole::GetCommandWord()
{
    return field_4;
}

// 213 bytes (0x584320-0x5843F4). thiscall: ecx is the console. The console's
// shared error reporter, keyed on the command id at +0x0: -3 prints the
// "I don't know how you go here, try again!" literal (0x8AF718), -2 the
// "Wrong Parameters : ..." literal (0x8AF744, double spaces and the
// "seperate" typo as shipped), and -1 the "Command not found : " prefix
// (0x8AF7DC) with GetCommandWord() appended; every other id prints
// nothing. All arms fall through the store at 0x5843D2 leaving the id at
// -1. The print call hands &this+0x38 to CtrlAltDelete_DisplayText
// (0x4EC1C0) as its ecx, which that callee never reads.
// 0x584320
void CLUAConsole::CommandNotFound()
{
    switch (static_cast<int>(field_0)) {
    case -3:
        CtrlAltDelete_DisplayText("I don't know how you go here, try again!");
        break;
    case -2:
        CtrlAltDelete_DisplayText("Wrong Parameters : Check the parameter list.  Strings must be in quotes.  Make sure to enclose parameters in () and seperate parameters with a comma.");
        break;
    case -1: {
        CString sText = "Command not found : ";
        sText += GetCommandWord();
        CtrlAltDelete_DisplayText(sText);
        break;
    }
    default:
        break;
    }

    field_0 = -1;
}
