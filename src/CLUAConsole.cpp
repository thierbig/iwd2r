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
// end). The loop at 0x584152 splits the remainder at any of ",)" (literal
// 0x8AF714, FindOneOf 0x7FD0D0 at 0x584178): a hit n sets sToken = Left(n)
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
// NOTE: the split is FindOneOf (any ',' or ')'), so each token is consumed
// with its delimiter: "5,6)" files 5 then 6 and ends on the empty remainder.
// A miss leaves sArgs whole, so a numeric token after the closing paren with
// no further delimiter ("5)7") re-appends forever — the exe's behavior; the
// pre-scan only guarantees a ')' somewhere past the first character.
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
        int n = sArgs.FindOneOf(",)");
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

// 1,364 bytes (0x5838A0-0x583DEE, `ret 4` at 0x583DF1; the dispatch tables
// sit at 0x583E34 and 0x583DF4). thiscall: esi is the console (ecx); the one
// by-value CString parameter is callee-cleaned. An SEH frame spans the whole
// body (trylevel 0 at 0x5838D7, -1 at the exits at 0x58399B and 0x583DD2),
// ebx holds 0 throughout, and ebp is the saved parameter pointer until
// 0x5839FD sets it to -1, the value both the -1 id store and the final
// trylevel store then use. Ghidra: CLUAConsole_Input.
//
// The full-line driver. The parameter is copied into a local sLine (copy
// ctor 0x7FCC88 at 0x5838C7); an empty line is the silent return (inline
// [eax-8] length read at 0x5838D0, branch at 0x5838DB). Otherwise
// sLine.MakeUpper() (0x7FD0F0) and nColon = sLine.Find(':') (0x7FD095 with
// 0x3A at 0x5838F0); nColon < 1 (cmp edi,1 at 0x5838F7) — no colon, or a
// colon at the very front — prints "Nice try cheater!" (0x8AF700 through
// DisplayText 0x4EC1C0 at 0x583996, ecx = this+0x38) and returns. The word
// left of the colon (Left 0x7FAA72 at 0x583912, operator= 0x7FCD07) is run
// through EncodeCommandWord (0x58394B) and strcmp'd (0x7E7A54 at 0x583976)
// against the same transform baked into the literal at 0x8AF630; the tree
// compares against EncodeCommandWord("CTRLALTDELETE") as ResolveCommand
// does. A mismatch takes the same "Nice try cheater!" return (0x583982).
//
// Prefix matched: sLine = the ORIGINAL-case parameter (operator= 0x7FCD57
// at 0x5839AD), then the tail past the colon (Right(GetLength()-nColon-1),
// 0x7FA9F6 at 0x5839C6) — so the line is case-insensitive end to end —
// MakeUpper (0x5839EB), and nParen = sLine.Find('(') (0x28 at 0x5839F6).
// nParen < 1 (0x583A00) stores field_0 = -1 and reports CommandNotFound
// (0x583A07) but does NOT return: it falls into the shared parse with the
// negative nParen, where Left clamps the empty word and Right yields the
// whole tail, so ResolveCommand (0x583A22) reports again on the empty word
// — a valid-prefix line without '(' is reported twice, once with the
// console's leftover word and once with the empty word. Always:
// ResolveCommand(sLine.Left(nParen)) then ParseArgs(sLine.Right(
// GetLength()-nParen-1)) (GetLength thunk 0x41E160, call at 0x583A47);
// ParseArgs may itself leave field_0 = -2 on a malformed argument list,
// which the dispatch gate below rejects.
//
// Dispatch (0x583A4C): a negative field_0 returns (signed jl at 0x583A50);
// field_0-12 above 0x1B unsigned (0x583A5C) stores -3 + CommandNotFound
// (0x583DC5); otherwise byte[0x583E34 + id-12] picks one of sixteen arms
// (jump table 0x583DF4). GetText(i) below is the shared element accessor
// 0x65F8F0 (11 bytes, `ret 4`): &m_pData[i] on the token store, spelled
// field_24.GetAt(i) — the by-value return is the arm's copy-construct
// (0x7FC98F). The value-table reads the arms make directly through m_pData
// (0x583AAD, 0x583C93, 0x583B9E) are spelled field_10.GetAt(i). Arms 0-2
// hand the handler this+0x40, one past the this+0x38 every other arm uses;
// no ported handler reads either. The -2 report arms all share the store
// + CommandNotFound at 0x583DCD.
//
//   0 FIRSTAID (0x583A71) AddItemPack.   1 HANS (0x583A7E) 0x4ED2C0.
//   2 MIDAS (0x583A8B) AddGold500.       3 ADDGOLD (0x583A98) CountArgs
// must be 1, else the -2 report; AddGold(field_10.GetAt(0)).
//   4 ADDSPELL (0x583AC0): CountArgs 2 -> 0x4ECC70(GetText(0),
// field_10.GetAt(0)) (0x583AD8), 1 -> 0x4ECC70(GetText(0), 10) (0x583B01,
// the 0xA staged before the token), else the -2 report.
//   5 CHECKVAR (0x583B26): CountArgs must be 2, else -2;
// 0x4ED730(GetText(0), GetText(1)) — the second token is fetched first
// (0x583B3D) and staged deepest.
//   6 CREATEITEM (0x583B81): CountArgs()-1 must be 0..3 (sub-jump-table
// 0x583E50 {0x583C5A, 0x583C28, 0x583BE4, 0x583B99}), else the -2 report
// shared with SETCHAPTER (0x583D8E); 1..4 arguments call
// CreateItem(GetText(0), up to three use counts low-word-truncated from
// the value table, the missing counts zero).
//   7 ENABLECHEATKEYS (0x583C81): CountArgs 0 -> EnableCheatKeys(1)
// (0x583CA6), 1 -> EnableCheatKeys(field_10.GetAt(0)) (0x583C93); a third
// count is the bare fall-through (0x583C8D), dead — this command's shape
// arm returns only 0 or 1.
//   8 EXPLOREAREA (0x583CB5) SetAreaExplored, no count check.
//   9 MOVETOAREA (0x583CC2): CountArgs must be 1, else -2;
// MoveToArea(GetText(0)).   10 SETCURRENTXP (0x583CFA): CountArgs must
// be 1, else -2; SetCurrentXP(field_10.GetAt(0)).
//   11 SETGLOBAL (0x583D22): CountArgs must be 3, else -2; the value is
// fetched first (0x583D39) and staged deepest:
// SetGlobal(GetText(0), GetText(1), field_10.GetAt(0)).
//   12 SETCHAPTER (0x583D80): CountArgs 0 -> 0x4EDAB0(-1) (the ebp push
// at 0x583DA6), 1 -> 0x4EDAB0(field_10.GetAt(0)) (0x583D96), else the
// shared -2 report.   13 JEFFKATTACKS (0x583DB1) and 14 JEFFKDEFENDS
// (0x583DBB): plain calls.   15 (0x583DC5) is the -3 path again, dead as
// a table arm — the range check has already handled every id outside
// 12..39, and ResolveCommand never yields an id whose table byte is 15.
// 0x5838A0
void CLUAConsole::Execute(CString sInput)
{
    CString sLine(sInput);
    if (sLine.GetLength() == 0)
        return;

    sLine.MakeUpper();
    int nColon = sLine.Find(':');
    if (nColon < 1) {
        CtrlAltDelete_DisplayText("Nice try cheater!");
        return;
    }

    sLine = sLine.Left(nColon);
    if (EncodeCommandWord(sLine) != EncodeCommandWord("CTRLALTDELETE")) {
        CtrlAltDelete_DisplayText("Nice try cheater!");
        return;
    }

    sLine = sInput;
    sLine = sLine.Right(sLine.GetLength() - nColon - 1);
    sLine.MakeUpper();

    int nParen = sLine.Find('(');
    if (nParen < 1) {
        field_0 = -1;
        CommandNotFound();
    }

    ResolveCommand(sLine.Left(nParen));
    ParseArgs(sLine.Right(sLine.GetLength() - nParen - 1));

    if (static_cast<int>(field_0) < 0)
        return;

    static const BYTE bDispatch[28] = {
        0, 1, 2, 15, 3, 4, 5, 15, 15, 6, 15, 15, 7, 8, 9, 15,
        10, 11, 15, 15, 15, 15, 15, 15, 15, 12, 13, 14
    };

    DWORD nId = field_0 - 12;
    switch ((nId <= 0x1b) ? bDispatch[nId] : 15) {
    case 0: // FIRSTAID (this arm hands the handler this+0x40)
        CtrlAltDelete_AddItemPack();
        break;
    case 1: // HANS (same this+0x40)
        CtrlAltDelete_Hans();
        break;
    case 2: // MIDAS (same this+0x40)
        CtrlAltDelete_AddGold500();
        break;
    case 3: // ADDGOLD
        if (CountArgs() != 1) {
            field_0 = -2;
            CommandNotFound();
        } else {
            CtrlAltDelete_AddGold(field_10.GetAt(0));
        }
        break;
    case 4: { // ADDSPELL
        int nArgs = CountArgs();
        if (nArgs == 2) {
            // TODO: 0x5838A0 calls 0x4ECC70 (not ported).
            // 0x4ECC70(GetText(0), field_10.GetAt(0)).
        } else if (nArgs == 1) {
            // TODO: 0x5838A0 calls 0x4ECC70 (not ported).
            // 0x4ECC70(GetText(0), 10).
        } else {
            field_0 = -2;
            CommandNotFound();
        }
        break;
    }
    case 5: // CHECKVAR
        if (CountArgs() != 2) {
            field_0 = -2;
            CommandNotFound();
        } else {
            // TODO: 0x5838A0 calls 0x4ED730 (not ported).
            // 0x4ED730(GetText(0), GetText(1)).
        }
        break;
    case 6: { // CREATEITEM
        int nArgs = CountArgs() - 1;
        if (nArgs > 3) {
            field_0 = -2;
            CommandNotFound();
        } else if (nArgs == 0) {
            CtrlAltDelete_CreateItem(field_24.GetAt(0), 1, 0, 0);
        } else if (nArgs == 1) {
            CtrlAltDelete_CreateItem(field_24.GetAt(0), LOWORD(field_10.GetAt(0)), 0, 0);
        } else if (nArgs == 2) {
            CtrlAltDelete_CreateItem(field_24.GetAt(0), LOWORD(field_10.GetAt(0)), LOWORD(field_10.GetAt(1)), 0);
        } else {
            CtrlAltDelete_CreateItem(field_24.GetAt(0), LOWORD(field_10.GetAt(0)), LOWORD(field_10.GetAt(1)), LOWORD(field_10.GetAt(2)));
        }
        break;
    }
    case 7: { // ENABLECHEATKEYS
        int nArgs = CountArgs();
        if (nArgs == 0) {
            CtrlAltDelete_EnableCheatKeys(1);
        } else if (nArgs == 1) {
            CtrlAltDelete_EnableCheatKeys(field_10.GetAt(0));
        }
        // Any other count is the bare fall-through at 0x583C8D; dead, since
        // this command's shape arm returns only 0 or 1.
        break;
    }
    case 8: // EXPLOREAREA: no count check
        CtrlAltDelete_SetAreaExplored();
        break;
    case 9: // MOVETOAREA
        if (CountArgs() != 1) {
            field_0 = -2;
            CommandNotFound();
        } else {
            CtrlAltDelete_MoveToArea(field_24.GetAt(0));
        }
        break;
    case 10: // SETCURRENTXP
        if (CountArgs() != 1) {
            field_0 = -2;
            CommandNotFound();
        } else {
            CtrlAltDelete_SetCurrentXP(field_10.GetAt(0));
        }
        break;
    case 11: // SETGLOBAL
        if (CountArgs() != 3) {
            field_0 = -2;
            CommandNotFound();
        } else {
            CtrlAltDelete_SetGlobal(field_24.GetAt(0), field_24.GetAt(1), field_10.GetAt(0));
        }
        break;
    case 12: { // SETCHAPTER
        int nArgs = CountArgs();
        if (nArgs == 0) {
            // TODO: 0x5838A0 calls 0x4EDAB0 (not ported).
            // 0x4EDAB0(-1).
        } else if (nArgs == 1) {
            // TODO: 0x5838A0 calls 0x4EDAB0 (not ported).
            // 0x4EDAB0(field_10.GetAt(0)).
        } else {
            field_0 = -2;
            CommandNotFound();
        }
        break;
    }
    case 13: // JEFFKATTACKS
        CtrlAltDelete_JeffKAttacks();
        break;
    case 14: // JEFFKDEFENDS
        CtrlAltDelete_JeffKDefends();
        break;
    default: // 15: the -3 path, dead as a table arm
        field_0 = -3;
        CommandNotFound();
        break;
    }
}
