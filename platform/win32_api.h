// Declarations of every Win32-named function the engine calls. All have C++
// linkage on purpose so mangled names never collide with the MSVC CRT or
// kernel32 symbols. One section per implementing file.
#pragma once

#include "compat/win_types.h"

#include <cstdint>

#ifdef _MSC_VER
// MSVC's <mutex> includes <thread>, which includes <process.h>: the CRT's
// _beginthread (C linkage) is declared in every unit that uses the standard
// threading headers. The shim's own, whose thread handles the other shim
// functions understand, takes another name there.
#include <process.h>
#define _beginthread iwd2r_beginthread
#endif

// ---- threads.cpp: threads, events, semaphores, mutexes, critical sections ----

struct CRITICAL_SECTION {
    void* impl;
};

uintptr_t _beginthread(void (*start)(void*), unsigned stackSize, void* arg);
DWORD GetCurrentThreadId();

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL CloseHandle(HANDLE handle);

HANDLE CreateEventA(LPVOID attributes, BOOL manualReset, BOOL initialState, LPCSTR name);
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);

HANDLE CreateSemaphoreA(LPVOID attributes, LONG initial, LONG maximum, LPCSTR name);
BOOL ReleaseSemaphore(HANDLE semaphore, LONG count, LONG* previous);

HANDLE CreateMutexA(LPVOID attributes, BOOL initialOwner, LPCSTR name);
BOOL ReleaseMutex(HANDLE mutex);

// Pseudo handles for the calling process and thread, as in Win32.
HANDLE GetCurrentProcess();
HANDLE GetCurrentThread();
DWORD GetCurrentProcessId();

// Pins the process, its running threads and the threads it starts later to
// the CPUs in mask (the original pins itself to CPU 0 with mask 1). Accepts
// the current-process pseudo handle and NULL (what the engine's OpenProcess
// call returns here) as this process. GetProcAddress hands it out under its
// kernel32 name.
BOOL SetProcessAffinityMask(HANDLE process, DWORD_PTR mask);

#define DUPLICATE_CLOSE_SOURCE 0x00000001
#define DUPLICATE_SAME_ACCESS 0x00000002

// Supports only GetCurrentThread() in the current process, giving a real
// handle for the calling thread that CloseHandle releases.
BOOL DuplicateHandle(HANDLE sourceProcess, HANDLE source, HANDLE targetProcess, HANDLE* target, DWORD access, BOOL inherit, DWORD options);

// Cooperative: a thread suspending itself blocks until ResumeThread brings
// its count to zero; suspending another thread only reports its count.
// Both return the previous suspend count, or (DWORD)-1 for a bad handle.
DWORD SuspendThread(HANDLE thread);
DWORD ResumeThread(HANDLE thread);

#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ERROR_RETURN 0x7FFFFFFF

// Priorities are not changed; every thread reports THREAD_PRIORITY_NORMAL.
int GetThreadPriority(HANDLE thread);
BOOL SetThreadPriority(HANDLE thread, int priority);

void InitializeCriticalSection(CRITICAL_SECTION* section);
void EnterCriticalSection(CRITICAL_SECTION* section);
void LeaveCriticalSection(CRITICAL_SECTION* section);
void DeleteCriticalSection(CRITICAL_SECTION* section);
BOOL TryEnterCriticalSection(CRITICAL_SECTION* section);

// ---- clock.cpp: tick count, sleep and multimedia timers ----

#define TIME_ONESHOT 0
#define TIME_PERIODIC 1
#define TIMERR_NOERROR 0
#define MMSYSERR_INVALPARAM 11

typedef void(CALLBACK* LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

DWORD GetTickCount();
DWORD timeGetTime();
void Sleep(DWORD milliseconds);
DWORD SleepEx(DWORD milliseconds, BOOL alertable);

struct TIMECAPS {
    UINT wPeriodMin;
    UINT wPeriodMax;
};
typedef TIMECAPS* LPTIMECAPS;

// Reports a 1 ms to 1000000 ms period range.
MMRESULT timeGetDevCaps(LPTIMECAPS caps, UINT size);

MMRESULT timeSetEvent(UINT delayMs, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);
MMRESULT timeKillEvent(UINT id);
MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);

// ---- fs.cpp: file attributes, enumeration, directories, drives ----

HANDLE FindFirstFileA(LPCSTR pattern, WIN32_FIND_DATAA* data);
BOOL FindNextFileA(HANDLE find, WIN32_FIND_DATAA* data);
BOOL FindClose(HANDLE find);

DWORD GetFileAttributesA(LPCSTR path);
// Only FILE_ATTRIBUTE_READONLY is honored: it clears or sets the owner write bit.
BOOL SetFileAttributesA(LPCSTR path, DWORD attributes);
// The engine calls the unsuffixed names.
inline DWORD GetFileAttributes(LPCSTR path) { return GetFileAttributesA(path); }
inline BOOL SetFileAttributes(LPCSTR path, DWORD attributes) { return SetFileAttributesA(path, attributes); }
BOOL RemoveDirectoryA(LPCSTR path);

// Files as kernel handles that CloseHandle closes. Sharing modes, security
// attributes, flags and templates are ignored. A missing file sets
// ERROR_FILE_NOT_FOUND.
HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD shareMode, LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags, HANDLE templateFile);
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD toRead, LPDWORD read, LPVOID overlapped);
BOOL DeleteFileA(LPCSTR path);
BOOL CopyFileA(LPCSTR source, LPCSTR destination, BOOL failIfExists);
BOOL MoveFileA(LPCSTR source, LPCSTR destination);
BOOL CreateDirectoryA(LPCSTR path, LPVOID attributes);

BOOL GetDiskFreeSpaceA(LPCSTR root, DWORD* sectorsPerCluster, DWORD* bytesPerSector, DWORD* freeClusters, DWORD* totalClusters);
DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer);
BOOL SetCurrentDirectoryA(LPCSTR path);
DWORD GetModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size);

DWORD GetLogicalDrives();
UINT GetDriveTypeA(LPCSTR root);
BOOL GetVolumeInformationA(LPCSTR root, LPSTR volumeName, DWORD volumeNameSize, DWORD* serial, DWORD* maxComponentLength, DWORD* flags, LPSTR fileSystemName, DWORD fileSystemNameSize);

// --- window.cpp: window class, window, message queue, polled input ---

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef WORD ATOM;

// Tagged as in <winuser.h>: SDL's Windows headers forward-declare
// `typedef struct tagMSG MSG`.
struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
};
typedef tagMSG MSG;
typedef MSG* LPMSG;

struct WNDCLASSEXA {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance;
    HANDLE hIcon, hCursor, hbrBackground;
    LPCSTR lpszMenuName, lpszClassName;
    HANDLE hIconSm;
};

struct PAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
};
typedef PAINTSTRUCT* LPPAINTSTRUCT;

#define WS_POPUP 0x80000000u
#define WS_VISIBLE 0x10000000u
#define WS_CLIPSIBLINGS 0x04000000u
#define WS_CLIPCHILDREN 0x02000000u
#define WS_CAPTION 0x00C00000u
#define WS_SYSMENU 0x00080000u
#define WS_MINIMIZEBOX 0x00020000u
#define WS_EX_TOPMOST 0x00000008u

#define CW_USEDEFAULT ((int)0x80000000)

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_MAXIMIZE 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXFULLSCREEN 16
#define SM_CYFULLSCREEN 17
#define SM_SWAPBUTTON 23
#define SM_CXDOUBLECLK 36
#define SM_CYDOUBLECLK 37

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

#define MB_OK 0x00000000u
#define MB_OKCANCEL 0x00000001u
#define MB_YESNOCANCEL 0x00000003u
#define MB_YESNO 0x00000004u
#define MB_ICONERROR 0x00000010u
#define MB_ICONQUESTION 0x00000020u
#define MB_ICONWARNING 0x00000030u
#define MB_ICONINFORMATION 0x00000040u
#define MB_SYSTEMMODAL 0x00001000u

#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

#define IDC_ARROW ((LPCSTR)(ULONG_PTR)32512)

#define HWND_TOP ((HWND)0)
#define HWND_TOPMOST ((HWND)(LONG_PTR) - 1)
#define HWND_NOTOPMOST ((HWND)(LONG_PTR) - 2)

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080

#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2

#define HTCLIENT 1

ATOM RegisterClassExA(const WNDCLASSEXA* windowClass);
HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
BOOL DestroyWindow(HWND hWnd);
BOOL ShowWindow(HWND hWnd, int command);
BOOL UpdateWindow(HWND hWnd);
BOOL SetWindowPos(HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags);
BOOL GetClientRect(HWND hWnd, LPRECT rect);
BOOL GetWindowRect(HWND hWnd, LPRECT rect);
BOOL ClientToScreen(HWND hWnd, LPPOINT point);
BOOL ScreenToClient(HWND hWnd, LPPOINT point);
HWND GetFocus();
int GetSystemMetrics(int index);

BOOL PeekMessageA(LPMSG msg, HWND hWnd, UINT filterMin, UINT filterMax, UINT remove);
BOOL GetMessageA(LPMSG msg, HWND hWnd, UINT filterMin, UINT filterMax);
BOOL TranslateMessage(const MSG* msg);
LRESULT DispatchMessageA(const MSG* msg);
BOOL PostMessageA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
// The engine calls the unsuffixed name in two places.
inline LRESULT SendMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) { return SendMessageA(hWnd, message, wParam, lParam); }
void PostQuitMessage(int exitCode);
LRESULT DefWindowProcA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
UINT RegisterWindowMessageA(LPCSTR name);

BOOL GetUpdateRect(HWND hWnd, LPRECT rect, BOOL erase);
HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT paint);
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* paint);

HCURSOR SetCursor(HCURSOR cursor);
HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name);
BOOL GetCursorPos(LPPOINT point);
BOOL SetCursorPos(int x, int y);
int ShowCursor(BOOL show);
SHORT GetAsyncKeyState(int vk);
HKL GetKeyboardLayout(DWORD threadId);

int MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type);

// Minimizes the window, as Win32 CloseWindow does.
BOOL CloseWindow(HWND hWnd);

#define SPI_GETKEYBOARDSPEED 0x000A
#define SPI_GETSCREENSAVEACTIVE 0x0010
#define SPI_SETSCREENSAVEACTIVE 0x0011
#define SPI_GETKEYBOARDDELAY 0x0016
#define SPI_GETWORKAREA 0x0030
#define SPI_GETWHEELSCROLLLINES 0x0068

// Reports Windows defaults: keyboard delay 1, keyboard speed 31, wheel
// scroll lines 3, screen saver active; the work area is the whole screen,
// (0, 0) to GetSystemMetrics(SM_CXSCREEN, SM_CYSCREEN). Setting the screen saver flag is
// accepted and ignored. Anything else returns FALSE.
BOOL SystemParametersInfoA(UINT action, UINT param, LPVOID value, UINT winIni);
inline BOOL SystemParametersInfo(UINT action, UINT param, LPVOID value, UINT winIni) { return SystemParametersInfoA(action, param, value, winIni); }

// The Windows default, 500 ms.
UINT GetDoubleClickTime();

// No other application windows exist; always NULL.
HWND FindWindowA(LPCSTR className, LPCSTR windowName);

// --- win32_stubs.cpp: inert window styles, frames, menus and display modes ---

#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)

// Window styles are not stored: GetWindowLongA reads 0 and SetWindowLongA
// returns 0 (the previous value) without changing anything.
LONG GetWindowLongA(HWND hWnd, int index);
LONG SetWindowLongA(HWND hWnd, int index, LONG value);
// Windows have no frame here: the rectangle is left unchanged, TRUE.
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD exStyle);
// No window has a menu; always NULL.
HMENU GetMenu(HWND hWnd);
// Accepted and ignored, TRUE. Only the unsupported 3D fullscreen path calls it.
BOOL OpenIcon(HWND hWnd);

#define DM_BITSPERPEL 0x00040000
#define DM_PELSWIDTH 0x00080000
#define DM_PELSHEIGHT 0x00100000
#define DM_DISPLAYFREQUENCY 0x00400000

#define CDS_FULLSCREEN 0x00000004
#define CDS_RESET 0x40000000

#define DISP_CHANGE_SUCCESSFUL 0
#define DISP_CHANGE_FAILED (-1)

struct DEVMODEA {
    BYTE dmDeviceName[32];
    WORD dmSpecVersion;
    WORD dmDriverVersion;
    WORD dmSize;
    WORD dmDriverExtra;
    DWORD dmFields;
    LONG dmPositionX;
    LONG dmPositionY;
    DWORD dmDisplayOrientation;
    DWORD dmDisplayFixedOutput;
    SHORT dmColor;
    SHORT dmDuplex;
    SHORT dmYResolution;
    SHORT dmTTOption;
    SHORT dmCollate;
    BYTE dmFormName[32];
    WORD dmLogPixels;
    DWORD dmBitsPerPel;
    DWORD dmPelsWidth;
    DWORD dmPelsHeight;
    DWORD dmDisplayFlags;
    DWORD dmDisplayFrequency;
};
typedef DEVMODEA* LPDEVMODEA;

// No display modes are enumerated (FALSE) and none can be set
// (DISP_CHANGE_FAILED); a NULL mode, which restores the registry mode, is
// accepted with DISP_CHANGE_SUCCESSFUL.
BOOL EnumDisplaySettingsA(LPCSTR deviceName, DWORD modeNum, LPDEVMODEA mode);
LONG ChangeDisplaySettingsA(LPDEVMODEA mode, DWORD flags);

// --- win32_stubs.cpp: inert DLL, version, GDI, COM, string and process calls ---

typedef INT_PTR(WINAPI* FARPROC)();

struct MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    SIZE_T dwTotalPhys;
    SIZE_T dwAvailPhys;
    SIZE_T dwTotalPageFile;
    SIZE_T dwAvailPageFile;
    SIZE_T dwTotalVirtual;
    SIZE_T dwAvailVirtual;
};
typedef MEMORYSTATUS* LPMEMORYSTATUS;

struct STARTUPINFOA {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
};
typedef STARTUPINFOA STARTUPINFO;
typedef STARTUPINFOA* LPSTARTUPINFOA;

struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
};
typedef PROCESS_INFORMATION* LPPROCESS_INFORMATION;

struct IUnknown;
typedef IUnknown* LPUNKNOWN;
typedef const GUID& REFCLSID;

#define VER_PLATFORM_WIN32s 0
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT 2

#define BITSPIXEL 12
#define PLANES 14
#define LOGPIXELSX 88
#define LOGPIXELSY 90

#define TRANSPARENT 1
#define OPAQUE 2
#define ETO_OPAQUE 0x0002
#define ETO_CLIPPED 0x0004
#define CLR_INVALID 0xFFFFFFFFu

// Plain literals so the engine's trailing FF_DONTCARE converts to a null
// face name.
#define FW_NORMAL 400
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DEFAULT_PITCH 0
#define FF_DONTCARE 0

#define CLSCTX_INPROC_SERVER 0x1
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154)
#define REGDB_E_IIDNOTREG ((HRESULT)0x80040155)

#define NORMAL_PRIORITY_CLASS 0x00000020
#define PROCESS_ALL_ACCESS 0x001FFFFFu

HMODULE LoadLibraryA(LPCSTR name);
BOOL FreeLibrary(HMODULE module);
FARPROC GetProcAddress(HMODULE module, LPCSTR name);
HMODULE GetModuleHandleA(LPCSTR name);

BOOL GetVersionExA(LPOSVERSIONINFOA info);
void GlobalMemoryStatus(LPMEMORYSTATUS status);

HDC GetDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hdc);
int GetDeviceCaps(HDC hdc, int index);
HFONT CreateFontA(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline, DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCSTR faceName);
BOOL DeleteObject(HGDIOBJ object);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ object);
HGDIOBJ GetStockObject(int object);
HICON LoadIconA(HINSTANCE instance, LPCSTR name);
BOOL ExtTextOutA(HDC hdc, int x, int y, UINT options, const RECT* rect, LPCSTR text, UINT count, const INT* dx);
BOOL TextOutA(HDC hdc, int x, int y, LPCSTR text, int count);
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF SetBkColor(HDC hdc, COLORREF color);
int SetBkMode(HDC hdc, int mode);
BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR text, int count, LPSIZE size);

#define PS_SOLID 0

struct TEXTMETRICA {
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    BYTE tmFirstChar;
    BYTE tmLastChar;
    BYTE tmDefaultChar;
    BYTE tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
};
typedef TEXTMETRICA* LPTEXTMETRICA;

// No GDI drawing: pens and brushes are NULL, Polygon draws nothing (FALSE),
// and GetTextMetricsA zero-fills the metrics and returns FALSE.
HPEN CreatePen(int style, int width, COLORREF color);
HBRUSH CreateSolidBrush(COLORREF color);
BOOL Polygon(HDC hdc, const POINT* points, int count);
BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRICA metrics);

DWORD GetFileVersionInfoSizeA(LPCSTR filename, LPDWORD handle);
BOOL GetFileVersionInfoA(LPCSTR filename, DWORD handle, DWORD size, LPVOID data);
BOOL VerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT length);

HRESULT CoInitialize(LPVOID reserved);
void CoUninitialize();
HRESULT CoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD context, REFIID iid, LPVOID* object);

void OutputDebugStringA(LPCSTR text);
LPSTR lstrcpyA(LPSTR destination, LPCSTR source);
LPSTR lstrcatA(LPSTR destination, LPCSTR source);
int lstrlenA(LPCSTR text);
int lstrcmpA(LPCSTR a, LPCSTR b);
int lstrcmpiA(LPCSTR a, LPCSTR b);
// Single-byte code page: never a lead byte, unless a test has set a
// double-byte code page's lead bytes with iwd2r_SetTestLeadBytes.
BOOL IsDBCSLeadByte(BYTE testChar);
// Tests only: `table` (256 entries, kept by the caller) marks the lead
// bytes IsDBCSLeadByte reports; NULL restores the single-byte code page.
void iwd2r_SetTestLeadBytes(const bool* table);
// ASCII letters and digits only.
BOOL IsCharAlphaNumericA(CHAR ch);
// No access checking: only a null pointer with a nonzero size is bad.
BOOL IsBadReadPtr(const void* pointer, UINT_PTR size);

// ---- winsock: the GameSpy heartbeat (CGameSpy) ----
// Inert by default: WSAStartup fails, so CGameSpy::sub_4D0980 reports failure
// and the class never opens a socket (ADR 0002 keeps multiplayer out). Tests
// install fakes with iwd2r_SetGameSpySockets; NULL restores the inert calls.
// The types are the Win32 ones, defined here because win_types.h includes no
// SDK header.
typedef UINT_PTR SOCKET;
constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(-1);
constexpr int SOCKET_ERROR = -1;

struct WSADATA {
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    WORD iMaxSockets;
    WORD iMaxUdpDg;
    char* lpVendorInfo;
};

struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

// The socket constants the GameSpy code uses, with the Win32 values.
constexpr int AF_INET = 2;
constexpr int SOCK_DGRAM = 2;
constexpr int IPPROTO_UDP = 17;
constexpr long FIONBIO = 0x8004667E;
constexpr int WSAEFAULT = 10014;
constexpr int WSAEWOULDBLOCK = 10035;
constexpr int WSAEINPROGRESS = 10036;
constexpr int WSAENETDOWN = 10050;
constexpr int WSAENOBUFS = 10055;
constexpr int WSANOTINITIALISED = 10093;
constexpr int WSAHOST_NOT_FOUND = 11001;
constexpr int WSATRY_AGAIN = 11002;
constexpr int WSANO_RECOVERY = 11003;
constexpr int WSANO_DATA = 11004;

// The calls CGameSpy makes, so a test can drive the heartbeat without a
// network. htons/ntohs stay the real (byte-swapping) implementations.
typedef SOCKET (*IWD2R_GameSpySocketFn)(int af, int type, int protocol);
typedef HANDLE (*IWD2R_GameSpyAsyncGetHostByNameFn)(HWND window, unsigned message,
    const char* name, char* buffer, int bufferLength);

struct IWD2R_GameSpySockets {
    int (*WSAStartup)(WORD versionRequested, WSADATA* data);
    int (*WSACleanup)();
    IWD2R_GameSpySocketFn socket;
    int (*ioctlsocket)(SOCKET socket, long command, unsigned long* argument);
    int (*bind)(SOCKET socket, const struct sockaddr_in* address, int length);
    int (*sendto)(SOCKET socket, const char* buffer, int length, int flags,
        const struct sockaddr_in* to, int toLength);
    int (*recvfrom)(SOCKET socket, char* buffer, int length, int flags,
        struct sockaddr_in* from, int* fromLength);
    int (*closesocket)(SOCKET socket);
    IWD2R_GameSpyAsyncGetHostByNameFn WSAAsyncGetHostByName;
    int (*WSACancelAsyncRequest)(HANDLE taskHandle);
};

void iwd2r_SetGameSpySockets(const IWD2R_GameSpySockets* sockets);
const IWD2R_GameSpySockets* iwd2r_GameSpySockets();

int WSAStartup(WORD versionRequested, WSADATA* data);
int WSACleanup();
SOCKET socket(int af, int type, int protocol);
int ioctlsocket(SOCKET socket, long command, unsigned long* argument);
int bind(SOCKET socket, const struct sockaddr_in* address, int length);
int sendto(SOCKET socket, const char* buffer, int length, int flags,
    const struct sockaddr_in* to, int toLength);
int recvfrom(SOCKET socket, char* buffer, int length, int flags, struct sockaddr_in* from,
    int* fromLength);
int closesocket(SOCKET socket);
HANDLE WSAAsyncGetHostByName(HWND window, unsigned message, const char* name, char* buffer,
    int bufferLength);
int WSACancelAsyncRequest(HANDLE taskHandle);
// <sys/_endian.h> (pulled in by the C library on some platforms) defines
// these as macros; the engine calls the shim's byte-swapping functions.
#ifdef htons
#undef htons
#endif
#ifdef ntohs
#undef ntohs
#endif
unsigned short htons(unsigned short hostshort);
unsigned short ntohs(unsigned short netshort);
// Writes at most 1024 bytes including the terminator, like Win32.
int wsprintfA(LPSTR buffer, LPCSTR format, ...);

DWORD GetLastError();
void SetLastError(DWORD error);

// Global memory: every block is fixed, so the handle is the pointer. Backed
// by malloc (calloc with GMEM_ZEROINIT); the engine uses it for clipboard text.
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
LPVOID GlobalLock(HGLOBAL memory);
// FALSE with NO_ERROR, as Win32 reports for a block that is no longer locked.
BOOL GlobalUnlock(HGLOBAL memory);
// Frees the block and returns NULL.
HGLOBAL GlobalFree(HGLOBAL memory);

// No clipboard: OpenClipboard fails, so the engine skips copy and paste.
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE GetClipboardData(UINT format);
HANDLE SetClipboardData(UINT format, HANDLE memory);
BOOL IsClipboardFormatAvailable(UINT format);

// English (United States), 0x0409.
LANGID GetSystemDefaultLangID();

// No other processes are opened: returns NULL with ERROR_ACCESS_DENIED.
HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD processId);

// The command line is const here: the engine passes a string literal and
// nothing writes to it.
BOOL CreateProcessA(LPCSTR application, LPCSTR commandLine, LPVOID processAttributes, LPVOID threadAttributes, BOOL inheritHandles, DWORD creationFlags, LPVOID environment, LPCSTR currentDirectory, LPSTARTUPINFOA startupInfo, LPPROCESS_INFORMATION processInfo);
HINSTANCE ShellExecuteA(HWND hWnd, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT showCommand);

// <mmsystem.h> maps the unsuffixed name to the ANSI one, so an engine member
// named PlaySound is spelled PlaySoundA at one call site.
#define PlaySound PlaySoundA

#include "platform/imm_stub.h"
