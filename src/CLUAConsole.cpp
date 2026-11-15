#include "CLUAConsole.h"

#include "CtrlAltDelete.h"

#include <cctype>

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

// 635 bytes (0x583E60-0x5840DA). thiscall: ecx is the console; the one
// by-value CString parameter is callee-cleaned (`ret 4` at 0x5840D8).
// Stores the raw word into field_4 (operator= 0x7FCD07 at 0x583E89),
// encodes a copy of it through the free scrambler (0x5844E0 at 0x583EA6,
// ecx = the console, which that callee ignores), then walks a strcmp chain
// (0x7E7A54, first compare at 0x583ED2) of the encoded word against the
// same transform baked into the literals at 0x8AF644+; a hit stores the
// command id into field_0 and jumps to the shared tail at 0x5840B8, the
// full miss (0x5840AB) leaves field_0 = -1 and reports through
// CommandNotFound (0x584320, called at 0x5840B3). The tree compares with
// == against EncodeCommandWord("WORD") per arm: the doubling map is
// injective over the letter range, so encoded(entered) == encoded(literal)
// is exactly the entered word, and re-encoding the plain literal at run
// time equals the baked bytes the exe compares against. Ids, in chain
// order: FIRSTAID 0xC, HANS 0xD, MIDAS 0xE, ADDGOLD 0x10, ADDSPELL 0x11,
// CHECKVAR 0x12, CREATEITEM 0x15, ENABLECHEATKEYS 0x18, EXPLOREAREA 0x19,
// MOVETOAREA 0x1A, SETCURRENTXP 0x1C, SETGLOBAL 0x1D, SETCHAPTER 0x25,
// JEFFKATTACKS 0x26, JEFFKDEFENDS 0x27. Ghidra: Get_CLUAConsole_Command_ID.
// 0x583E60
void CLUAConsole::ResolveCommand(CString sWord)
{
    field_4 = sWord;
    CString sEncoded = EncodeCommandWord(sWord);

    if (sEncoded == EncodeCommandWord("FIRSTAID"))
        field_0 = 0xC;
    else if (sEncoded == EncodeCommandWord("HANS"))
        field_0 = 0xD;
    else if (sEncoded == EncodeCommandWord("MIDAS"))
        field_0 = 0xE;
    else if (sEncoded == EncodeCommandWord("ADDGOLD"))
        field_0 = 0x10;
    else if (sEncoded == EncodeCommandWord("ADDSPELL"))
        field_0 = 0x11;
    else if (sEncoded == EncodeCommandWord("CHECKVAR"))
        field_0 = 0x12;
    else if (sEncoded == EncodeCommandWord("CREATEITEM"))
        field_0 = 0x15;
    else if (sEncoded == EncodeCommandWord("ENABLECHEATKEYS"))
        field_0 = 0x18;
    else if (sEncoded == EncodeCommandWord("EXPLOREAREA"))
        field_0 = 0x19;
    else if (sEncoded == EncodeCommandWord("MOVETOAREA"))
        field_0 = 0x1A;
    else if (sEncoded == EncodeCommandWord("SETCURRENTXP"))
        field_0 = 0x1C;
    else if (sEncoded == EncodeCommandWord("SETGLOBAL"))
        field_0 = 0x1D;
    else if (sEncoded == EncodeCommandWord("SETCHAPTER"))
        field_0 = 0x25;
    else if (sEncoded == EncodeCommandWord("JEFFKATTACKS"))
        field_0 = 0x26;
    else if (sEncoded == EncodeCommandWord("JEFFKDEFENDS"))
        field_0 = 0x27;
    else {
        field_0 = -1;
        CommandNotFound();
    }
}

// 96 bytes (0x5842C0-0x58431F). thiscall: ecx is the console; the one
// by-value CString parameter is callee-cleaned (`ret 4` at 0x584318). An
// SEH frame (trylevel at [esp+0x14] region: 0 at 0x5842E4, -1 at
// 0x5842FC) wraps the one real call. The append: the array's element
// count read straight from the member header — mov eax,[esi+0x2C] at
// 0x5842D8, the CArray m_nSize inside the token store at this+0x24 — is
// passed with the parameter (edx = [esp+0x14] at 0x5842DE) to
// CStringArray::SetAtGrow (0x7FBB6A at 0x5842EC, ecx = this+0x24);
// growing at the current size is an append. The token counter field_C is
// then bumped through [esi+0xC] (0x5842F1-0x5842F9) and the parameter is
// destructed in the callee (CString dtor 0x7FCC1A at 0x584304).
// 0x5842C0
void CLUAConsole::AddTextArg(CString sArg)
{
    field_24.SetAtGrow(field_24.GetSize(), sArg);
    field_C++;
}

// 475 bytes (0x5840E0-0x5842BA). thiscall: edi = the console (ecx); the
// one by-value CString parameter is callee-cleaned (`ret 4` at 0x5842B8).
// An SEH frame with the trylevel stepping 0 -> 1 (0x584111) -> 2 (0x584197)
// -> 3 (0x5841CD) -> 0 (0x58428D) -> -1 (0x58429B) spans the whole body.
//
// The parameter is trimmed in place first (TrimRight 0x7FAEFB at 0x584116,
// TrimLeft 0x7FAF47 at 0x58411F), then a pre-scan (0x584124-0x584150)
// locates the closing paren: a ')' as the first character jumps straight
// to the loop (0x58412C/0x58412F); otherwise it looks for the first ')'
// at sArgs[i+1] and rejects the line (-2 + CommandNotFound, 0x58427C) when
// i reaches 0x80 (0x584131) or len-1 (0x58413C-0x584142, no ')' before the
// end). The loop at 0x584152 splits the remainder on ",)" (literal
// 0x8AF714, Find 0x7FD0D0 at 0x584178): a hit n sets sToken = Left(n)
// (0x7FAA72 at 0x58418D) and sArgs = Right(len-n-1) (0x7FA9F6 at
// 0x5841C3), a miss leaves sArgs whole; then sToken.TrimRight (0x5841E9)
// and sArgs.TrimLeft (0x5841F2), an empty token loops (0x584200). A token
// opening with '"' (len >= 2, 0x584208-0x584210) must close with '"'
// (0x584217) or the line is rejected; the inner text goes to AddTextArg
// as Mid(1, len-2) (length re-read through the GetLength thunk 0x41E160
// at 0x58421D, Mid 0x7FA960 at 0x584234, call 0x5842C0 at 0x58423B).
// Otherwise the token must start with a digit (isdigit 0x7E93D9 at
// 0x584249) or '-' (0x584259): its atoi value (0x7E7B8E at 0x58425F) is
// grown into the value table at the current size ([this+0x18], SetAtGrow
// 0x7FB967 at 0x58426F) and field_8 counts it (0x584274). Any other first
// character — including a bare ')' tail — is the silent, uncounted return
// at 0x584289.
//
// NOTE: the split only consumes on a ",)" hit, so a trailing numeric token
// without one ("5)") never advances sArgs and re-appends forever — the
// exe's behavior when the line omits the conventional trailing comma. The
// port mirrors it exactly; the dispatcher 0x5838A0 always hands a
// ')'-terminated remainder.
// 0x5840E0
void CLUAConsole::ParseArgs(CString sArgs)
{
    sArgs.TrimRight();
    sArgs.TrimLeft();

    if (sArgs[0] != ')') {
        int nLength = sArgs.GetLength();
        for (int i = 0;; i++) {
            if (i == 0x80 || i == nLength - 1) {
                field_0 = -2;
                CommandNotFound();
                return;
            }
            if (sArgs[i + 1] == ')')
                break;
        }
    }

    for (;;) {
        if (sArgs.GetLength() == 0)
            return;

        CString sToken = sArgs;
        int n = sArgs.Find(",)");
        if (n >= 0) {
            sToken = sToken.Left(n);
            sArgs = sArgs.Right(sArgs.GetLength() - n - 1);
        }
        sToken.TrimRight();
        sArgs.TrimLeft();

        if (sToken.GetLength() == 0)
            continue;

        if (sToken[0] == '"' && sToken.GetLength() >= 2) {
            if (sToken[sToken.GetLength() - 1] != '"') {
                field_0 = -2;
                CommandNotFound();
                return;
            }
            AddTextArg(sToken.Mid(1, sToken.GetLength() - 2));
        } else if (isdigit(sToken[0]) || sToken[0] == '-') {
            field_10.SetAtGrow(field_10.GetSize(), atoi(sToken));
            field_8++;
        } else {
            return;
        }
    }
}
