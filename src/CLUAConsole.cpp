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

// 140 bytes (0x5844E0-0x58456B). Free function, not a member: the resolver
// 0x583E60 calls it with ecx = the console (0x583EA6) but the body never
// reads ecx; the only inputs are the by-value CString argument and the
// hidden return buffer (callee-cleaned `ret 8` at 0x584569). The length
// check is inlined as the CString header read [eax-8] (0x584503, re-read
// each iteration at 0x584526); the per-byte work at 0x584514-0x58451F is
// register-only — al = sText[i], al += 8, al <<= 1, byte-truncated — and
// SetAt (0x7FD114, called at 0x584521) stores it back. The copy ctor
// (0x7FC98F at 0x58453D) then moves the transformed string to the return
// buffer and the dtor (0x7FCC1A at 0x584553) drops the argument. Ghidra:
// Encode_CString_Weirdness.
// 0x5844E0
CString EncodeCommandWord(CString sText)
{
    for (int i = 0; i < sText.GetLength(); i++)
        sText.SetAt(i, static_cast<char>((sText[i] + 8) * 2));

    return sText;
}

// 130 bytes (0x584420-0x5844A1). thiscall: ecx is the console; plain `ret`
// (caller-cleaned, esi saved, no SEH frame). The dispatcher's argument-count
// check: field_0 - 12 indexes the 28-entry shape table at 0x5844C4 (an id
// outside 12..39 takes the unsigned-above at 0x58442B), and the byte picks
// one of eight jump-table arms at 0x5844A4:
//   0 (0x58449E) return 0;                 1 (0x58443C) field_8==1 ? 1 : 0;
//   2 (0x58445A) field_C==1 ? field_8+1 : 0;   3 (0x58444B) field_C==2 ? 2 : 0;
//   4 (0x584468) field_C==1 ? 1 : 0;       5 (0x584477) field_C==2 && field_8==1 ? 3 : 0;
//   6 (0x58448C) return field_8;           7 (0x584493) field_0 = -3,
// CommandNotFound (0x584499, ecx unchanged), return 0.
// 0x584420
int CLUAConsole::CountArgs()
{
    static const BYTE bArgShape[28] = {
        0, 0, 0, 7, 1, 2, 3, 7, 7, 2, 7, 7, 1, 0, 4,
        7, 1, 5, 7, 7, 7, 7, 7, 7, 7, 6, 0, 0
    };

    DWORD nId = field_0 - 12;
    int nArm = (nId <= 0x1b) ? bArgShape[nId] : 7;
    switch (nArm) {
    case 1:
        return (field_8 == 1) ? 1 : 0;
    case 2:
        return (field_C == 1) ? static_cast<int>(field_8) + 1 : 0;
    case 3:
        return (field_C == 2) ? 2 : 0;
    case 4:
        return (field_C == 1) ? 1 : 0;
    case 5:
        return (field_C == 2 && field_8 == 1) ? 3 : 0;
    case 6:
        return static_cast<int>(field_8);
    case 7:
        field_0 = -3;
        CommandNotFound();
        return 0;
    default:
        return 0;
    }
}
