// Inert Win32 calls: no DLLs, no GDI text, no COM, no input methods, no
// child processes, no other processes. Each returns what Windows returns when the feature is
// unavailable, so the engine falls back to its own paths.
#define NOMINMAX
#include "platform/win32_api.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

thread_local DWORD t_lastError = 0;

char g_moduleSentinel;

}

HMODULE LoadLibraryA(LPCSTR) { return NULL; }

BOOL FreeLibrary(HMODULE) { return TRUE; }

namespace {

// The engine's own pointer type for it (DWORD mask, as on 32-bit Windows).
BOOL WINAPI SetProcessAffinityMaskEntry(HANDLE process, DWORD mask)
{
    return SetProcessAffinityMask(process, static_cast<DWORD_PTR>(mask));
}

}

// Only the kernel32 entry points the engine looks up and iwd2r implements;
// NULL for everything else, so the engine takes its fallback paths.
FARPROC GetProcAddress(HMODULE, LPCSTR name)
{
    if (name && std::strcmp(name, "SetProcessAffinityMask") == 0)
        // Through void (*)(), the generic function pointer type, as callers
        // of GetProcAddress cast back to the real type.
        return reinterpret_cast<FARPROC>(reinterpret_cast<void (*)()>(&SetProcessAffinityMaskEntry));
    return NULL;
}

HMODULE GetModuleHandleA(LPCSTR) { return reinterpret_cast<HMODULE>(&g_moduleSentinel); }

// Windows XP (5.1.2600), which the engine accepts.
BOOL GetVersionExA(LPOSVERSIONINFOA info)
{
    if (!info || info->dwOSVersionInfoSize < sizeof(OSVERSIONINFOA))
        return FALSE;
    info->dwMajorVersion = 5;
    info->dwMinorVersion = 1;
    info->dwBuildNumber = 2600;
    info->dwPlatformId = VER_PLATFORM_WIN32_NT;
    info->szCSDVersion[0] = '\0';
    return TRUE;
}

void GlobalMemoryStatus(LPMEMORYSTATUS status)
{
    if (!status)
        return;
    const SIZE_T gib = static_cast<SIZE_T>(1024) * 1024 * 1024;
    status->dwLength = sizeof(MEMORYSTATUS);
    status->dwMemoryLoad = 0;
    status->dwTotalPhys = gib;
    status->dwAvailPhys = gib;
    status->dwTotalPageFile = gib;
    status->dwAvailPageFile = gib;
    status->dwTotalVirtual = gib;
    status->dwAvailVirtual = gib;
}

HDC GetDC(HWND) { return NULL; }

int ReleaseDC(HWND, HDC hdc) { return hdc != NULL ? 1 : 0; }

int GetDeviceCaps(HDC, int index)
{
    switch (index) {
    case BITSPIXEL:
        return 32;
    case PLANES:
        return 1;
    case LOGPIXELSX:
    case LOGPIXELSY:
        return 96;
    default:
        return 0;
    }
}

HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return NULL; }

BOOL DeleteObject(HGDIOBJ) { return TRUE; }

HGDIOBJ SelectObject(HDC, HGDIOBJ) { return NULL; }

HGDIOBJ GetStockObject(int) { return NULL; }

HICON LoadIconA(HINSTANCE, LPCSTR) { return NULL; }

BOOL ExtTextOutA(HDC, int, int, UINT, const RECT*, LPCSTR, UINT, const INT*) { return FALSE; }

BOOL TextOutA(HDC, int, int, LPCSTR, int) { return FALSE; }

COLORREF SetTextColor(HDC, COLORREF) { return CLR_INVALID; }

COLORREF SetBkColor(HDC, COLORREF) { return CLR_INVALID; }

int SetBkMode(HDC, int) { return 0; }

BOOL GetTextExtentPoint32A(HDC, LPCSTR, int, LPSIZE) { return FALSE; }

HPEN CreatePen(int, int, COLORREF) { return NULL; }

HBRUSH CreateSolidBrush(COLORREF) { return NULL; }

BOOL Polygon(HDC, const POINT*, int) { return FALSE; }

BOOL GetTextMetricsA(HDC, LPTEXTMETRICA metrics)
{
    if (metrics)
        std::memset(metrics, 0, sizeof(*metrics));
    return FALSE;
}

LONG GetWindowLongA(HWND, int) { return 0; }

LONG SetWindowLongA(HWND, int, LONG) { return 0; }

BOOL AdjustWindowRectEx(LPRECT rect, DWORD, BOOL, DWORD) { return rect != NULL ? TRUE : FALSE; }

HMENU GetMenu(HWND) { return NULL; }

BOOL OpenIcon(HWND) { return TRUE; }

BOOL EnumDisplaySettingsA(LPCSTR, DWORD, LPDEVMODEA) { return FALSE; }

LONG ChangeDisplaySettingsA(LPDEVMODEA mode, DWORD) { return mode == NULL ? DISP_CHANGE_SUCCESSFUL : DISP_CHANGE_FAILED; }

namespace {

#ifndef IWD2R_VERSION_STRING
#define IWD2R_VERSION_STRING "iwd2r unknown"
#endif

// The built-in version resource of whatever file is asked about (the engine
// asks only about its own executable). Strings as in the GOG 2.01 IWD2.EXE's
// VS_VERSIONINFO (\StringFileInfo\040904b0), except PrivateBuild (there
// "50100", unused by the engine), which names this build.
const char* const kVersionStrings[][2] = {
    { "CompanyName", "Interplay Entertainment Corp." },
    { "FileDescription", "Icewind Dale II" },
    { "FileVersion", "2, 0, 1, 0" },
    { "InternalName", "Icewind Dale II" },
    { "OriginalFilename", "IWD2.EXE" },
    { "PrivateBuild", IWD2R_VERSION_STRING },
    { "ProductName", "Icewind Dale II" },
    { "ProductVersion", "2, 0, 1, 0" },
};

const char kVersionMagic[] = "iwd2rVER";
const char kStringFilePrefix[] = "\\StringFileInfo\\040904b0\\";

// The block GetFileVersionInfoA fills: the magic, then key NUL value NUL
// pairs, then a final NUL.
std::string VersionBlock()
{
    std::string block(kVersionMagic, sizeof(kVersionMagic));
    for (const auto& entry : kVersionStrings) {
        block.append(entry[0]).push_back('\0');
        block.append(entry[1]).push_back('\0');
    }
    block.push_back('\0');
    return block;
}

bool EqualsNoCase(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
        if (a[i] == '\0')
            return true;
    }
    return true;
}

}

DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD handle)
{
    if (handle)
        *handle = 0;
    return static_cast<DWORD>(VersionBlock().size());
}

BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD size, LPVOID data)
{
    const std::string block = VersionBlock();
    if (!data || size < block.size()) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    std::memcpy(data, block.data(), block.size());
    return TRUE;
}

// Answers \StringFileInfo\040904b0\<Name> (case-insensitive, as Win32) with
// a pointer into the caller's block and the length in characters including
// the NUL. The engine queries no other path (no "\" fixed info, no
// \VarFileInfo).
BOOL VerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT length)
{
    if (!block || !subBlock || !buffer)
        return FALSE;
    const char* bytes = static_cast<const char*>(block);
    if (std::memcmp(bytes, kVersionMagic, sizeof(kVersionMagic)) != 0)
        return FALSE;
    const size_t prefixLength = sizeof(kStringFilePrefix) - 1;
    if (std::strlen(subBlock) <= prefixLength || !EqualsNoCase(subBlock, kStringFilePrefix, prefixLength))
        return FALSE;
    const char* name = subBlock + prefixLength;
    for (const char* key = bytes + sizeof(kVersionMagic); *key != '\0';) {
        const char* value = key + std::strlen(key) + 1;
        if (EqualsNoCase(key, name, std::strlen(name) + 1)) {
            *buffer = const_cast<char*>(value);
            if (length)
                *length = static_cast<UINT>(std::strlen(value) + 1);
            return TRUE;
        }
        key = value + std::strlen(value) + 1;
    }
    return FALSE;
}

HRESULT CoInitialize(LPVOID) { return S_OK; }

void CoUninitialize() { }

HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID* object)
{
    if (object)
        *object = NULL;
    return E_NOINTERFACE;
}

void OutputDebugStringA(LPCSTR text)
{
    if (text)
        std::fputs(text, stderr);
}

LPSTR lstrcpyA(LPSTR destination, LPCSTR source)
{
    if (!destination || !source)
        return NULL;
    return std::strcpy(destination, source);
}

LPSTR lstrcatA(LPSTR destination, LPCSTR source)
{
    if (!destination || !source)
        return NULL;
    return std::strcat(destination, source);
}

int lstrlenA(LPCSTR text) { return text ? static_cast<int>(std::strlen(text)) : 0; }

int lstrcmpA(LPCSTR a, LPCSTR b)
{
    const int r = std::strcmp(a ? a : "", b ? b : "");
    return r < 0 ? -1 : (r > 0 ? 1 : 0);
}

int lstrcmpiA(LPCSTR a, LPCSTR b)
{
    const unsigned char* pa = reinterpret_cast<const unsigned char*>(a ? a : "");
    const unsigned char* pb = reinterpret_cast<const unsigned char*>(b ? b : "");
    for (;; pa++, pb++) {
        const int ca = std::tolower(*pa);
        const int cb = std::tolower(*pb);
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (ca == 0)
            return 0;
    }
}

namespace {
const bool* g_testLeadBytes = nullptr;
}

void iwd2r_SetTestLeadBytes(const bool* table) { g_testLeadBytes = table; }

BOOL IsDBCSLeadByte(BYTE testChar) { return g_testLeadBytes != nullptr && g_testLeadBytes[testChar] ? TRUE : FALSE; }

BOOL IsCharAlphaNumericA(CHAR ch) { return std::isalnum(static_cast<unsigned char>(ch)) ? TRUE : FALSE; }

BOOL IsBadReadPtr(const void* pointer, UINT_PTR size) { return pointer == NULL && size != 0 ? TRUE : FALSE; }

namespace {
// Tests only: the fakes CGameSpy's sockets dispatch through (NULL: inert).
const IWD2R_GameSpySockets* g_gameSpySockets = nullptr;
}

void iwd2r_SetGameSpySockets(const IWD2R_GameSpySockets* sockets) { g_gameSpySockets = sockets; }

const IWD2R_GameSpySockets* iwd2r_GameSpySockets() { return g_gameSpySockets; }

int WSAStartup(WORD versionRequested, WSADATA* data)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->WSAStartup(versionRequested, data);
    }
    // No winsock: zero the block (wVersion 0, so CGameSpy's check fails).
    if (data != nullptr) {
        std::memset(data, 0, sizeof(*data));
    }
    return SOCKET_ERROR;
}

int WSACleanup()
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->WSACleanup();
    }
    return 0;
}

SOCKET socket(int af, int type, int protocol)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->socket(af, type, protocol);
    }
    return INVALID_SOCKET;
}

int ioctlsocket(SOCKET handle, long command, unsigned long* argument)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->ioctlsocket(handle, command, argument);
    }
    return SOCKET_ERROR;
}

int bind(SOCKET handle, const struct sockaddr_in* address, int length)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->bind(handle, address, length);
    }
    return SOCKET_ERROR;
}

int sendto(SOCKET handle, const char* buffer, int length, int flags,
    const struct sockaddr_in* to, int toLength)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->sendto(handle, buffer, length, flags, to, toLength);
    }
    return SOCKET_ERROR;
}

int recvfrom(SOCKET handle, char* buffer, int length, int flags, struct sockaddr_in* from,
    int* fromLength)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->recvfrom(handle, buffer, length, flags, from, fromLength);
    }
    return SOCKET_ERROR;
}

int closesocket(SOCKET handle)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->closesocket(handle);
    }
    return 0;
}

HANDLE WSAAsyncGetHostByName(HWND window, unsigned message, const char* name, char* buffer,
    int bufferLength)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->WSAAsyncGetHostByName(window, message, name, buffer, bufferLength);
    }
    return NULL;
}

int WSACancelAsyncRequest(HANDLE taskHandle)
{
    if (g_gameSpySockets != nullptr) {
        return g_gameSpySockets->WSACancelAsyncRequest(taskHandle);
    }
    return SOCKET_ERROR;
}

unsigned short htons(unsigned short netshort)
{
    return static_cast<unsigned short>((netshort >> 8) | (netshort << 8));
}

unsigned short ntohs(unsigned short netshort) { return htons(netshort); }

int wsprintfA(LPSTR buffer, LPCSTR format, ...)
{
    if (!buffer || !format)
        return 0;
    const int limit = 1024;
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, limit, format, args);
    va_end(args);
    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }
    return written < limit ? written : limit - 1;
}

DWORD GetLastError() { return t_lastError; }

void SetLastError(DWORD error) { t_lastError = error; }

HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes)
{
    // A zero-byte request still yields a distinct block.
    const size_t size = bytes > 0 ? bytes : 1;
    return (flags & GMEM_ZEROINIT) ? std::calloc(1, size) : std::malloc(size);
}

LPVOID GlobalLock(HGLOBAL memory) { return memory; }

BOOL GlobalUnlock(HGLOBAL)
{
    SetLastError(NO_ERROR);
    return FALSE;
}

HGLOBAL GlobalFree(HGLOBAL memory)
{
    std::free(memory);
    return NULL;
}

BOOL OpenClipboard(HWND) { return FALSE; }

BOOL CloseClipboard() { return FALSE; }

BOOL EmptyClipboard() { return FALSE; }

HANDLE GetClipboardData(UINT) { return NULL; }

HANDLE SetClipboardData(UINT, HANDLE) { return NULL; }

BOOL IsClipboardFormatAvailable(UINT) { return FALSE; }

LANGID GetSystemDefaultLangID() { return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US); }

HANDLE OpenProcess(DWORD, BOOL, DWORD)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return NULL;
}

BOOL CreateProcessA(LPCSTR, LPCSTR, LPVOID, LPVOID, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION) { return FALSE; }

HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT) { return NULL; }

HIMC ImmGetContext(HWND) { return NULL; }

BOOL ImmReleaseContext(HWND, HIMC) { return TRUE; }

LONG ImmGetCompositionStringA(HIMC, DWORD, LPVOID, DWORD) { return 0; }

BOOL ImmGetConversionStatus(HIMC, LPDWORD conversion, LPDWORD sentence)
{
    if (conversion)
        *conversion = 0;
    if (sentence)
        *sentence = 0;
    return FALSE;
}

BOOL ImmSetConversionStatus(HIMC, DWORD, DWORD) { return FALSE; }

BOOL ImmGetOpenStatus(HIMC) { return FALSE; }

BOOL ImmSetOpenStatus(HIMC, BOOL) { return FALSE; }

DWORD ImmGetCandidateListA(HIMC, DWORD, LPCANDIDATELIST, DWORD) { return 0; }

UINT ImmGetDescriptionA(HKL, LPSTR description, UINT bufferLength)
{
    if (description && bufferLength > 0)
        description[0] = '\0';
    return 0;
}

HKL ActivateKeyboardLayout(HKL, UINT) { return NULL; }
