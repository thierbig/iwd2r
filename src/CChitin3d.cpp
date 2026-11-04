#include "CChitin3d.h"

#include "CChitin.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _MSC_VER
#include <mbstring.h>
#endif

static void InitOpenGL(HMODULE hOpenGL);

// 0xA0E170
PFNSWAPBUFFERSPROC CChitin3d::SwapBuffers;

// 0xA0E174
PFNWGLMAKECURRENTPROC CChitin3d::wglMakeCurrent;

// 0xA0E178
PFNWGLCREATECONTEXTPROC CChitin3d::wglCreateContext;

// 0xA0E17C
PFNWGLGETCURRENTDCPROC CChitin3d::wglGetCurrentDC;

// 0xA0E180
PFNWGLDELETECONTEXTPROC CChitin3d::wglDeleteContext;

// 0xA0E184
PFNSETPIXELFORMATPROC CChitin3d::SetPixelFormat;

// 0xA0E188
PFNCHOOSEPIXELFORMATPROC CChitin3d::ChoosePixelFormat;

// -----------------------------------------------------------------------------

// GLSetup driver detection
//
// The original Init3d drives GLSetup (the circa-2000 3D-driver detection
// product; ini keys "Use GLSetup" / "GLSetup Driver" under [Program Options]).
// These helpers enumerate the installed OpenGL/Glide driver DLLs, validate each
// by its file-version resource, and probe the card by creating a GL context and
// drawing test primitives before committing to a driver. Dormant on modern
// systems, but recovered here for the IWD2EE OpenGL renderer.

// The rest of the Win32 surface GLSetup touches is not modeled by the platform
// layer. These file-local shims keep the recovered code compiling and linking
// on both toolchains; at run time they fail gracefully, so GLSetupEnumDrivers
// reports zero drivers (nothing in this tree calls it -- our Init3d keeps
// loading the system opengl32.dll directly).

struct VS_FIXEDFILEINFO {
    DWORD dwSignature;
    DWORD dwStrucVersion;
    DWORD dwFileVersionMS;
    DWORD dwFileVersionLS;
    DWORD dwProductVersionMS;
    DWORD dwProductVersionLS;
    DWORD dwFileFlagsMask;
    DWORD dwFileFlags;
    DWORD dwFileOS;
    DWORD dwFileType;
    DWORD dwFileSubtype;
    DWORD dwFileDateMS;
    DWORD dwFileDateLS;
};

struct BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
};

static BOOL GetFileInformationByHandle(HANDLE, BY_HANDLE_FILE_INFORMATION*)
{
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
}

static DWORD GetSystemDirectoryA(LPSTR, DWORD)
{
    SetLastError(ERROR_FILE_NOT_FOUND);
    return 0;
}

static DWORD GetFullPathNameA(LPCSTR, DWORD, LPSTR, LPSTR*)
{
    SetLastError(ERROR_FILE_NOT_FOUND);
    return 0;
}

static LANGID GetUserDefaultLangID()
{
    return GetSystemDefaultLangID();
}

// The registry surface GLSetup touches ("Software\Silicon Graphics\OpenGL").
// Modeled the same way as the shims above: no registry exists on this platform,
// so every call fails (nonzero per the Win32 registry convention) and the
// GLSetup probe sees no OverrideDispatch policy. HKEY itself comes from
// compat/win_types.h; the access/type constants are not defined anywhere else
// in this tree.

#define HKEY_LOCAL_MACHINE ((HKEY)(LONG_PTR)0x80000002)
#define KEY_QUERY_VALUE 0x0001
#define KEY_ALL_ACCESS 0xF003F
#define REG_DWORD 4

static LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, HKEY*)
{
    return 2; // ERROR_FILE_NOT_FOUND
}

static LONG RegQueryValueExA(HKEY, LPCSTR, DWORD*, DWORD*, BYTE*, DWORD*)
{
    return 2; // ERROR_FILE_NOT_FOUND
}

static LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, DWORD,
                            SECURITY_ATTRIBUTES*, HKEY*, DWORD*)
{
    return 2; // ERROR_FILE_NOT_FOUND
}

static LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD)
{
    return 2; // ERROR_FILE_NOT_FOUND
}

static LONG RegCloseKey(HKEY)
{
    return 0; // ERROR_SUCCESS
}

// Two WGL/pixel-format entry-point types the probe resolves but CVideo3d.h
// declares no typedef for (its renderer never calls them). Declared file-locally
// to match that header's calling-convention style.
typedef BOOL(__stdcall* PFNWGLCOPYCONTEXTPROC)(HGLRC, HGLRC, UINT);
typedef int(__stdcall* PFNGETPIXELFORMATPROC)(HDC);

#ifndef _MSC_VER
// ASCII stand-ins for the VC CRT multibyte helpers (single-byte locale only).
static unsigned char* _mbslwr(unsigned char* string)
{
    for (unsigned char* p = string; *p != '\0'; p++) {
        *p = static_cast<unsigned char>(std::tolower(*p));
    }
    return string;
}

static int _mbsicmp(const unsigned char* a, const unsigned char* b)
{
    while (*a != '\0' && *b != '\0') {
        int ca = std::tolower(*a);
        int cb = std::tolower(*b);
        if (ca != cb) {
            return ca - cb;
        }
        a++;
        b++;
    }
    return std::tolower(*a) - std::tolower(*b);
}

// VC CRT name for the POSIX putenv; the env save/restore only matters when the
// GLSetup probe actually runs, which it cannot on this platform.
static int _putenv(const char*)
{
    return 0;
}
#endif

// One DLL slot of a detected driver: last-write time, file version, and
// canonical path.
struct GLSETUP_DRIVER_FILE {
    /* 0x00 */ DWORD dwWriteTimeHigh;
    /* 0x04 */ DWORD dwWriteTimeLow;
    /* 0x08 */ DWORD dwVersionMS;
    /* 0x0C */ DWORD dwVersionLS;
    /* 0x10 */ CHAR szPath[0x104];
};

// One detected GLSetup driver. sizeof == 0x32C; the driver table is
// g_aGLSetupDrivers[] at 0x906920 with this stride (0x906A34 / 0x906B48 are
// table[0].primary / table[0].mini).
struct GLSETUP_DRIVER {
    /* 0x000 */ DWORD dwFlags;   // 0x80000000 valid, 0x40 mini ver fail, 0x60 mini
                                 //   invalid, 0x10 no ver ("Unknown"), 0x08 in system
                                 //   dir, 0x04 is the system opengl32.dll
    /* 0x004 */ CHAR szId[0x100];             // "Company - Description" / "Unknown"
    /* 0x104 */ GLSETUP_DRIVER_FILE primary;
    /* 0x218 */ GLSETUP_DRIVER_FILE mini;
};

// GLSetup control flags: bit0 skips mini-driver validation, bit1 skips the
// primary/mini same-directory check.
// 0x9074AC
static DWORD g_dwGLSetupControl;

// Number of drivers detected by GLSetupEnumDrivers.
// 0x9074B0
static unsigned int g_nGLSetupDrivers;

// Detected driver table (up to 3 kept).
// 0x906920
static GLSETUP_DRIVER g_aGLSetupDrivers[3];

// Index of the driver selected by GLSetupSelectDriver, or -1.
// 0x8BB0A0
static int g_nSelectedGLSetupDriver = -1;

// Resolved WGL entry points of the driver selected via GLSetup (written by the
// GL capability probe, read by Init3d).
// 0x9074C8
static PFNSWAPBUFFERSPROC      g_pfnGLSetupSwapBuffers;
// 0x9074D0
static PFNWGLCREATECONTEXTPROC g_pfnGLSetupWglCreateContext;
// 0x9074D8
static PFNWGLDELETECONTEXTPROC g_pfnGLSetupWglDeleteContext;
// 0x9074DC
static PFNWGLGETCURRENTDCPROC  g_pfnGLSetupWglGetCurrentDC;
// 0x9074E4
static PFNWGLMAKECURRENTPROC   g_pfnGLSetupWglMakeCurrent;

// Remaining WGL entry points the probe resolves from the selected driver. Not
// read by the current renderer (write-only here), but resolved by the binary so
// they are reproduced faithfully.
// 0x9074CC
static PFNWGLCOPYCONTEXTPROC           g_pfnGLSetupWglCopyContext;
// 0x9074D4
static PFNWGLCREATELAYERCONTEXTPROC    g_pfnGLSetupWglCreateLayerContext;
// 0x9074F4
static PFNWGLDESCRIBELAYERPLANEPROC    g_pfnGLSetupWglDescribeLayerPlane;
// 0x8BBCE4
static PFNWGLGETCURRENTCONTEXTPROC     g_pfnGLSetupWglGetCurrentContext;
// 0x9074FC
static PFNWGLGETLAYERPALETTEENTRIESPROC g_pfnGLSetupWglGetLayerPaletteEntries;
// 0x9074E0
static PFNWGLGETPROCADDRESSPROC        g_pfnGLSetupWglGetProcAddress;
// 0x907500
static PFNWGLREALIZELAYERPALETTEPROC   g_pfnGLSetupWglRealizeLayerPalette;
// 0x9074F8
static PFNWGLSETLAYERPALETTEENTRIESPROC g_pfnGLSetupWglSetLayerPaletteEntries;
// 0x9074E8
static PFNWGLSHARELISTSPROC            g_pfnGLSetupWglShareLists;
// 0x907504
static PFNWGLSWAPLAYERBUFFERSPROC      g_pfnGLSetupWglSwapLayerBuffers;
// 0x9074EC
static PFNWGLUSEFONTBITMAPSAPROC       g_pfnGLSetupWglUseFontBitmapsA;
// 0x9074F0
static PFNWGLUSEFONTOUTLINESAPROC      g_pfnGLSetupWglUseFontOutlinesA;

// Pixel-format entry points, sourced either from the driver's wgl* exports or
// from gdi32.dll depending on whether the driver is the system opengl32.dll.
// 0x8BBCD8
static PFNCHOOSEPIXELFORMATPROC   g_pfnGLSetupChoosePixelFormat;
// 0x8BBCDC
static PFNDESCRIBEPIXELFORMATPROC g_pfnGLSetupDescribePixelFormat;
// 0x9074C4
static PFNGETPIXELFORMATPROC      g_pfnGLSetupGetPixelFormat;
// 0x8BBCE0
static PFNSETPIXELFORMATPROC      g_pfnGLSetupSetPixelFormat;

// Driver module handles opened by the probe.
// 0x906914
static HMODULE g_hGLSetupDriver;
// 0x906910
static HMODULE g_hGLSetupGdi32;
// 0x906918
static HMODULE g_hGLSetupMini;

// One-shot caches for the FX_GLIDE_NO_SPLASH env var and the SGI OpenGL
// "OverrideDispatch" registry value (fetched once, kept for the process life).
// 0x9074B4
static int   g_bGLSetupSplashEnvChecked;
// 0x9074B8
static char* g_pszGLSetupSplashEnv;
// 0x9074BC
static int   g_bGLSetupDispatchChecked;
// 0x9074C0
static LPSTR g_pGLSetupOverrideDispatch;

// Canonical paths of the primary and mini driver DLLs the probe committed to.
// 0x9072A4
static CHAR g_szGLSetupPrimaryPath[0x104];
// 0x9073A8
static CHAR g_szGLSetupMiniPath[0x104];

// Candidate OpenGL driver DLLs probed, in priority order. dwFlags is merged
// into the driver record's flags on a successful validate.
struct GLSETUP_CANDIDATE {
    DWORD dwFlags;
    const char* pszPrimary;
    const char* pszSecondary;
    const char* pszDisplay;
};

static const GLSETUP_CANDIDATE g_aGLSetupCandidates[4] = {
    { 0x04, "\\opengl32.dll", "\\glu32.dll",    "Default OpenGL Driver" },
    { 0x03, "\\3dfxogl.dll",  "\\3dfxoglu.dll", "3dfx Installed OpenGL Driver" },
    { 0x80, "\\opengl.dll",   "\\gl.dll",       "SGI Software OpenGL Driver" },
    { 0x03, "\\3dfxvgl.dll",  "\\3dfxvglu.dll", "3dfx Standalone OpenGL Driver" },
};

// Reads pszFilePath's version resource: stores dwFileVersionMS/LS into the
// driver record (+0x8 / +0xC), and when pszIdOut is given, formats a
// "Company - Description" identification string from the version resource.
// Returns TRUE when a VS_FIXEDFILEINFO block was found.
// 0x7BB5D0
int GLSetupGetDriverVersion(const char* pszFilePath, void* pDriverInfo, char* pszIdOut, unsigned int cbIdOut)
{
    int bFound = 0;
    DWORD dwHandle;
    DWORD dwSize = GetFileVersionInfoSizeA(pszFilePath, &dwHandle);
    *(DWORD*)((char*)pDriverInfo + 0xC) = 0;
    *(DWORD*)((char*)pDriverInfo + 0x8) = 0;
    if (pszIdOut != NULL && cbIdOut != 0) {
        *pszIdOut = 0;
    }
    if (dwSize == 0) {
        return 0;
    }

    void* pVerData = malloc(dwSize);
    if (pVerData != NULL) {
        if (GetFileVersionInfoA(pszFilePath, 0, dwSize, pVerData)) {
            VS_FIXEDFILEINFO* pFixed;
            UINT nFixedLen;
            if (VerQueryValueA(pVerData, "\\", (LPVOID*)&pFixed, &nFixedLen) != 0 &&
                nFixedLen == sizeof(VS_FIXEDFILEINFO) && pFixed != NULL &&
                pFixed->dwSignature == 0xFEEF04BD) {
                bFound = 1;
                *(DWORD*)((char*)pDriverInfo + 0x8) = pFixed->dwFileVersionMS;
                *(DWORD*)((char*)pDriverInfo + 0xC) = pFixed->dwFileVersionLS;
            }

            if (pszIdOut != NULL && cbIdOut != 0) {
                const DWORD aCodePage[2] = { 0x4B0, 0x4E4 };
                char szKey[1024];

                char* pszCompany = NULL;
                UINT cbCompany = 0;
                for (int i = 0; i < 2 && pszCompany == NULL; i++) {
                    if (strlen("CompanyName") + 0x18 < sizeof(szKey)) {
                        sprintf(szKey, "\\StringFileInfo\\%04X%04X\\%s",
                                GetUserDefaultLangID(), aCodePage[i], "CompanyName");
                        VerQueryValueA(pVerData, szKey, (LPVOID*)&pszCompany, &cbCompany);
                    }
                }

                char* pszFileDesc = NULL;
                UINT cbFileDesc = 0;
                for (int i = 0; i < 2 && pszFileDesc == NULL; i++) {
                    if (strlen("FileDescription") + 0x18 < sizeof(szKey)) {
                        sprintf(szKey, "\\StringFileInfo\\%04X%04X\\%s",
                                GetUserDefaultLangID(), aCodePage[i], "FileDescription");
                        VerQueryValueA(pVerData, szKey, (LPVOID*)&pszFileDesc, &cbFileDesc);
                    }
                }

                char* pszProduct = NULL;
                UINT cbProduct = 0;
                for (int i = 0; i < 2 && pszProduct == NULL; i++) {
                    if (strlen("ProductName") + 0x18 < sizeof(szKey)) {
                        sprintf(szKey, "\\StringFileInfo\\%04X%04X\\%s",
                                GetUserDefaultLangID(), aCodePage[i], "ProductName");
                        VerQueryValueA(pVerData, szKey, (LPVOID*)&pszProduct, &cbProduct);
                    }
                }

                UINT cbDesc = (cbFileDesc < cbProduct) ? cbProduct : cbFileDesc;
                if (cbDesc + 3 + cbCompany < cbIdOut) {
                    const char* pszDesc = pszFileDesc;
                    if (pszFileDesc == NULL) {
                        pszDesc = pszProduct;
                        if (pszProduct == NULL) {
                            pszDesc = "";
                        }
                    }
                    const char* pszSep;
                    if (pszCompany == NULL || (pszFileDesc == NULL && pszProduct == NULL)) {
                        pszSep = "";
                    } else {
                        pszSep = " - ";
                    }
                    const char* pszComp = pszCompany;
                    if (pszCompany == NULL) {
                        pszComp = "";
                    }
                    sprintf(pszIdOut, "%s%s%s", pszComp, pszSep, pszDesc);
                }
            }
        }
        free(pVerData);
    }
    return bFound;
}

// Validates a candidate driver: records each DLL's last-write time and file
// version, classifies it (resides in the system dir, is the system
// opengl32.dll, ...) via canonical-path comparisons, and rejects a mini-driver
// that is not co-located with the primary. Returns 0 on success, 4 on failure.
// 0x7B7940
int GLSetupValidateDriver(const char* pszPrimaryPath, const char* pszMiniPath, GLSETUP_DRIVER* pDriver)
{
    if (pszPrimaryPath == NULL) {
        return 4;
    }
    if (pDriver == NULL) {
        return 4;
    }

    CHAR szSysDir[0x104];
    CHAR szNormA[0x104];
    CHAR szNormB[0x104];
    LPSTR pFilePart;
    BY_HANDLE_FILE_INFORMATION fi;

    pDriver->primary.dwWriteTimeLow = 0;
    pDriver->primary.dwWriteTimeHigh = 0;

    HANDLE hFile = CreateFileA(pszPrimaryPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 4;
    }
    BOOL bInfo = GetFileInformationByHandle(hFile, &fi);
    if (bInfo) {
        pDriver->primary.dwWriteTimeHigh = fi.ftLastWriteTime.dwHighDateTime;
        pDriver->primary.dwWriteTimeLow = fi.ftLastWriteTime.dwLowDateTime;
    }
    CloseHandle(hFile);
    if (!bInfo) {
        return 4;
    }

    pDriver->dwFlags = 0x80000000;

    BOOL bMiniHandled = FALSE;
    if (pszMiniPath != NULL && *pszMiniPath != '\0' && (g_dwGLSetupControl & 1) == 0) {
        pDriver->mini.dwWriteTimeLow = 0;
        pDriver->mini.dwWriteTimeHigh = 0;
        hFile = CreateFileA(pszMiniPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            BOOL bMiniInfo = FALSE;
            if (GetFileInformationByHandle(hFile, &fi)) {
                pDriver->mini.dwWriteTimeHigh = fi.ftLastWriteTime.dwHighDateTime;
                pDriver->mini.dwWriteTimeLow = fi.ftLastWriteTime.dwLowDateTime;
                bMiniInfo = TRUE;
            }
            CloseHandle(hFile);
            if (bMiniInfo) {
                if (GLSetupGetDriverVersion(pszMiniPath, &pDriver->mini, NULL, 0) == 0) {
                    pDriver->dwFlags |= 0x40;
                }
                pDriver->mini.szPath[0] = '\0';
                if (GetFullPathNameA(pszMiniPath, 0x104, pDriver->mini.szPath, &pFilePart) != 0 &&
                    GetSystemDirectoryA(szSysDir, 0x104) != 0) {
                    int nDirLen = (int)(pFilePart - pDriver->mini.szPath);
                    _mbslwr((unsigned char*)pDriver->mini.szPath);
                    _mbslwr((unsigned char*)szSysDir);
                    if (nDirLen - 1 == (int)strlen(szSysDir)) {
                        strstr(pDriver->mini.szPath, szSysDir);
                    }
                    bMiniHandled = TRUE;
                }
            }
        }
    }
    if (!bMiniHandled) {
        pDriver->mini.szPath[0] = '\0';
        pDriver->dwFlags |= 0x60;
    }

    if (GLSetupGetDriverVersion(pszPrimaryPath, &pDriver->primary, pDriver->szId, 0x100) == 0) {
        strncpy(pDriver->szId, "Unknown", 0x100);
        pDriver->dwFlags |= 0x10;
    }

    pDriver->primary.szPath[0] = '\0';
    if (GetFullPathNameA(pszPrimaryPath, 0x104, pDriver->primary.szPath, &pFilePart) == 0 ||
        GetSystemDirectoryA(szSysDir, 0x104) == 0) {
        return 4;
    }
    {
        int nDirLen = (int)(pFilePart - pDriver->primary.szPath);
        _mbslwr((unsigned char*)pDriver->primary.szPath);
        _mbslwr((unsigned char*)szSysDir);
        if (nDirLen - 1 == (int)strlen(szSysDir) &&
            strstr(pDriver->primary.szPath, szSysDir) == pDriver->primary.szPath) {
            pDriver->dwFlags |= 8;
        }
    }

    GetSystemDirectoryA(szSysDir, 0x104);
    strcat(szSysDir, "\\opengl32.dll");
    GetFullPathNameA(szSysDir, 0x104, szNormA, &pFilePart);
    if (GetFullPathNameA(pDriver->primary.szPath, 0x104, szNormB, &pFilePart) != 0) {
        if (_mbsicmp((unsigned char*)szNormB, (unsigned char*)szNormA) == 0) {
            pDriver->dwFlags |= 4;
        }
    }

    if ((g_dwGLSetupControl & 2) == 0) {
        LPSTR pFilePartA = NULL;
        LPSTR pFilePartB = NULL;
        GetFullPathNameA(pDriver->primary.szPath, 0x104, szNormA, &pFilePartA);
        GetFullPathNameA(pDriver->mini.szPath, 0x104, szNormB, &pFilePartB);
        if (pFilePartA != NULL) {
            *pFilePartA = '\0';
        }
        if (pFilePartB != NULL) {
            *pFilePartB = '\0';
        }
        if (_mbsicmp((unsigned char*)szNormA, (unsigned char*)szNormB) != 0) {
            pDriver->mini.szPath[0] = '\0';
            pDriver->dwFlags |= 0x60;
        }
    }

    return 0;
}

// Enumerates the candidate OpenGL driver DLLs in the Windows system directory,
// validates each, and fills g_aGLSetupDrivers (up to 3). Returns the count.
// 0x7B7750
unsigned int GLSetupEnumDrivers(void)
{
    CHAR szPrimaryPath[0x104];
    CHAR szSecondaryPath[0x104];

    g_nGLSetupDrivers = 0;
    if (GetSystemDirectoryA(szPrimaryPath, 0x104) == 0) {
        return g_nGLSetupDrivers;
    }
    int nSysLen = (int)strlen(szPrimaryPath);
    strcpy(szSecondaryPath, szPrimaryPath);

    for (int i = 0; i < 4; i++) {
        if (g_nGLSetupDrivers > 2) {
            return g_nGLSetupDrivers;
        }
        const GLSETUP_CANDIDATE* pCand = &g_aGLSetupCandidates[i];
        if ((int)strlen(pCand->pszPrimary) + nSysLen < 0x104 &&
            (int)strlen(pCand->pszSecondary) + nSysLen < 0x104) {
            strcpy(szPrimaryPath + nSysLen, pCand->pszPrimary);
            strcpy(szSecondaryPath + nSysLen, pCand->pszSecondary);
            GLSETUP_DRIVER* pDriver = &g_aGLSetupDrivers[g_nGLSetupDrivers];
            if (GLSetupValidateDriver(szPrimaryPath, szSecondaryPath, pDriver) == 0) {
                pDriver->dwFlags = (pDriver->dwFlags & 0x7fffffff) | pCand->dwFlags;
                if (pCand->pszDisplay[0] != '\0') {
                    strncpy(pDriver->szId, pCand->pszDisplay, 0x100);
                }
                g_nGLSetupDrivers++;
            }
        }
    }
    return g_nGLSetupDrivers;
}

// Tears the probed driver back down: releases the driver / mini / gdi32 module
// handles, restores the FX_GLIDE_NO_SPLASH env var and the SGI OpenGL
// "OverrideDispatch" registry value to the values cached before the probe, then
// resets the whole OpenGL / WGL / GLU / pixel-format entry-point table and the
// selected-driver state. Called on probe entry and again when a driver is
// rejected.
// 0x7BAAC0
void GLSetupResetDriver(void)
{
    if (g_hGLSetupDriver != NULL) {
        FreeLibrary(g_hGLSetupDriver);
        g_hGLSetupDriver = NULL;
    }
    if (g_hGLSetupMini != NULL) {
        FreeLibrary(g_hGLSetupMini);
        g_hGLSetupMini = NULL;
    }
    if (g_hGLSetupGdi32 != NULL) {
        FreeLibrary(g_hGLSetupGdi32);
        g_hGLSetupGdi32 = NULL;
    }

    // Restore FX_GLIDE_NO_SPLASH to its pre-probe value (empty if it was unset).
    if ((g_dwGLSetupControl & 0x10) == 0 && g_bGLSetupSplashEnvChecked != 0) {
        const char* pszValue = g_pszGLSetupSplashEnv;
        if (pszValue == NULL) {
            pszValue = "";
        }
        if (strlen("FX_GLIDE_NO_SPLASH") + strlen(pszValue) < 0x100) {
            CHAR szEnv[256];
            sprintf(szEnv, "%s=%s", "FX_GLIDE_NO_SPLASH", pszValue);
            _putenv(szEnv);
        }
        g_bGLSetupSplashEnvChecked = 0;
    }

    // Restore the OverrideDispatch registry value to its cached pre-probe value.
    if ((g_dwGLSetupControl & 0x20) == 0 && g_bGLSetupDispatchChecked != 0) {
        LPSTR pValue = g_pGLSetupOverrideDispatch;
        HKEY  hKey = (HKEY)-1;
        DWORD dwDisposition;
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Silicon Graphics\\OpenGL",
                            0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition) == 0) {
            RegSetValueExA(hKey, "OverrideDispatch", 0, REG_DWORD, (const BYTE*)&pValue, 4);
            RegCloseKey(hKey);
        }
        g_bGLSetupDispatchChecked = 0;
    }

    // Reset every resolved entry point. Normally cleared to NULL; when control
    // bit 0x40 is set the slots point at a shared safe-stub thunk instead so a
    // stray GL call lands on a defined address rather than a null pointer.
    void* pReset = NULL;
    if ((g_dwGLSetupControl & 0x40) != 0) {
        // HACK: safe-stub thunk -- an incremental-link jump thunk with no
        // source-level symbol; referenced only here -- replaces 0x7BB5C0.
        pReset = (void*)0x007BB5C0;
    }

    CVideo3d::glAccum = (PFNGLACCUMPROC)pReset;
    CVideo3d::glAlphaFunc = (PFNGLALPHAFUNCPROC)pReset;
    CVideo3d::glAreTexturesResident = (PFNGLARETEXTURESRESIDENTPROC)pReset;
    CVideo3d::glArrayElement = (PFNGLARRAYELEMENTPROC)pReset;
    CVideo3d::glBegin = (PFNGLBEGINPROC)pReset;
    CVideo3d::glBindTexture = (PFNGLBINDTEXTUREPROC)pReset;
    CVideo3d::glBitmap = (PFNGLBITMAPPROC)pReset;
    CVideo3d::glBlendFunc = (PFNGLBLENDFUNCPROC)pReset;
    CVideo3d::glCallList = (PFNGLCALLLISTPROC)pReset;
    CVideo3d::glCallLists = (PFNGLCALLLISTSPROC)pReset;
    CVideo3d::glClear = (PFNGLCLEARPROC)pReset;
    CVideo3d::glClearAccum = (PFNGLCLEARACCUMPROC)pReset;
    CVideo3d::glClearColor = (PFNGLCLEARCOLORPROC)pReset;
    CVideo3d::glClearDepth = (PFNGLCLEARDEPTHPROC)pReset;
    CVideo3d::glClearIndex = (PFNGLCLEARINDEXPROC)pReset;
    CVideo3d::glClearStencil = (PFNGLCLEARSTENCILPROC)pReset;
    CVideo3d::glClipPlane = (PFNGLCLIPPLANEPROC)pReset;
    CVideo3d::glColor3b = (PFNGLCOLOR3BPROC)pReset;
    CVideo3d::glColor3bv = (PFNGLCOLOR3BVPROC)pReset;
    CVideo3d::glColor3d = (PFNGLCOLOR3DPROC)pReset;
    CVideo3d::glColor3dv = (PFNGLCOLOR3DVPROC)pReset;
    CVideo3d::glColor3f = (PFNGLCOLOR3FPROC)pReset;
    CVideo3d::glColor3fv = (PFNGLCOLOR3FVPROC)pReset;
    CVideo3d::glColor3i = (PFNGLCOLOR3IPROC)pReset;
    CVideo3d::glColor3iv = (PFNGLCOLOR3IVPROC)pReset;
    CVideo3d::glColor3s = (PFNGLCOLOR3SPROC)pReset;
    CVideo3d::glColor3sv = (PFNGLCOLOR3SVPROC)pReset;
    CVideo3d::glColor3ub = (PFNGLCOLOR3UBPROC)pReset;
    CVideo3d::glColor3ubv = (PFNGLCOLOR3UBVPROC)pReset;
    CVideo3d::glColor3ui = (PFNGLCOLOR3UIPROC)pReset;
    CVideo3d::glColor3uiv = (PFNGLCOLOR3UIVPROC)pReset;
    CVideo3d::glColor3us = (PFNGLCOLOR3USPROC)pReset;
    CVideo3d::glColor3usv = (PFNGLCOLOR3USVPROC)pReset;
    CVideo3d::glColor4b = (PFNGLCOLOR4BPROC)pReset;
    CVideo3d::glColor4bv = (PFNGLCOLOR4BVPROC)pReset;
    CVideo3d::glColor4d = (PFNGLCOLOR4DPROC)pReset;
    CVideo3d::glColor4dv = (PFNGLCOLOR4DVPROC)pReset;
    CVideo3d::glColor4f = (PFNGLCOLOR4FPROC)pReset;
    CVideo3d::glColor4fv = (PFNGLCOLOR4FVPROC)pReset;
    CVideo3d::glColor4i = (PFNGLCOLOR4IPROC)pReset;
    CVideo3d::glColor4iv = (PFNGLCOLOR4IVPROC)pReset;
    CVideo3d::glColor4s = (PFNGLCOLOR4SPROC)pReset;
    CVideo3d::glColor4sv = (PFNGLCOLOR4SVPROC)pReset;
    CVideo3d::glColor4ub = (PFNGLCOLOR4UBPROC)pReset;
    CVideo3d::glColor4ubv = (PFNGLCOLOR4UBVPROC)pReset;
    CVideo3d::glColor4ui = (PFNGLCOLOR4UIPROC)pReset;
    CVideo3d::glColor4uiv = (PFNGLCOLOR4UIVPROC)pReset;
    CVideo3d::glColor4us = (PFNGLCOLOR4USPROC)pReset;
    CVideo3d::glColor4usv = (PFNGLCOLOR4USVPROC)pReset;
    CVideo3d::glColorMask = (PFNGLCOLORMASKPROC)pReset;
    CVideo3d::glColorMaterial = (PFNGLCOLORMATERIALPROC)pReset;
    CVideo3d::glColorPointer = (PFNGLCOLORPOINTERPROC)pReset;
    CVideo3d::glCopyPixels = (PFNGLCOPYPIXELSPROC)pReset;
    CVideo3d::glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC)pReset;
    CVideo3d::glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)pReset;
    CVideo3d::glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC)pReset;
    CVideo3d::glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)pReset;
    CVideo3d::glCullFace = (PFNGLCULLFACEPROC)pReset;
    CVideo3d::glDeleteLists = (PFNGLDELETELISTSPROC)pReset;
    CVideo3d::glDeleteTextures = (PFNGLDELETETEXTURESPROC)pReset;
    CVideo3d::glDepthFunc = (PFNGLDEPTHFUNCPROC)pReset;
    CVideo3d::glDepthMask = (PFNGLDEPTHMASKPROC)pReset;
    CVideo3d::glDepthRange = (PFNGLDEPTHRANGEPROC)pReset;
    CVideo3d::glDisable = (PFNGLDISABLEPROC)pReset;
    CVideo3d::glDisableClientState = (PFNGLDISABLECLIENTSTATEPROC)pReset;
    CVideo3d::glDrawArrays = (PFNGLDRAWARRAYSPROC)pReset;
    CVideo3d::glDrawBuffer = (PFNGLDRAWBUFFERPROC)pReset;
    CVideo3d::glDrawElements = (PFNGLDRAWELEMENTSPROC)pReset;
    CVideo3d::glDrawPixels = (PFNGLDRAWPIXELSPROC)pReset;
    CVideo3d::glEdgeFlag = (PFNGLEDGEFLAGPROC)pReset;
    CVideo3d::glEdgeFlagPointer = (PFNGLEDGEFLAGPOINTERPROC)pReset;
    CVideo3d::glEdgeFlagv = (PFNGLEDGEFLAGVPROC)pReset;
    CVideo3d::glEnable = (PFNGLENABLEPROC)pReset;
    CVideo3d::glEnableClientState = (PFNGLENABLECLIENTSTATEPROC)pReset;
    CVideo3d::glEnd = (PFNGLENDPROC)pReset;
    CVideo3d::glEndList = (PFNGLENDLISTPROC)pReset;
    CVideo3d::glEvalCoord1d = (PFNGLEVALCOORD1DPROC)pReset;
    CVideo3d::glEvalCoord1dv = (PFNGLEVALCOORD1DVPROC)pReset;
    CVideo3d::glEvalCoord1f = (PFNGLEVALCOORD1FPROC)pReset;
    CVideo3d::glEvalCoord1fv = (PFNGLEVALCOORD1FVPROC)pReset;
    CVideo3d::glEvalCoord2d = (PFNGLEVALCOORD2DPROC)pReset;
    CVideo3d::glEvalCoord2dv = (PFNGLEVALCOORD2DVPROC)pReset;
    CVideo3d::glEvalCoord2f = (PFNGLEVALCOORD2FPROC)pReset;
    CVideo3d::glEvalCoord2fv = (PFNGLEVALCOORD2FVPROC)pReset;
    CVideo3d::glEvalMesh1 = (PFNGLEVALMESH1PROC)pReset;
    CVideo3d::glEvalMesh2 = (PFNGLEVALMESH2PROC)pReset;
    CVideo3d::glEvalPoint1 = (PFNGLEVALPOINT1PROC)pReset;
    CVideo3d::glEvalPoint2 = (PFNGLEVALPOINT2PROC)pReset;
    CVideo3d::glFeedbackBuffer = (PFNGLFEEDBACKBUFFERPROC)pReset;
    CVideo3d::glFinish = (PFNGLFINISHPROC)pReset;
    CVideo3d::glFlush = (PFNGLFLUSHPROC)pReset;
    CVideo3d::glFogf = (PFNGLFOGFPROC)pReset;
    CVideo3d::glFogfv = (PFNGLFOGFVPROC)pReset;
    CVideo3d::glFogi = (PFNGLFOGIPROC)pReset;
    CVideo3d::glFogiv = (PFNGLFOGIVPROC)pReset;
    CVideo3d::glFrontFace = (PFNGLFRONTFACEPROC)pReset;
    CVideo3d::glFrustum = (PFNGLFRUSTUMPROC)pReset;
    CVideo3d::glGenLists = (PFNGLGENLISTSPROC)pReset;
    CVideo3d::glGenTextures = (PFNGLGENTEXTURESPROC)pReset;
    CVideo3d::glGetBooleanv = (PFNGLGETBOOLEANVPROC)pReset;
    CVideo3d::glGetClipPlane = (PFNGLGETCLIPPLANEPROC)pReset;
    CVideo3d::glGetDoublev = (PFNGLGETDOUBLEVPROC)pReset;
    CVideo3d::glGetError = (PFNGLGETERRORPROC)pReset;
    CVideo3d::glGetFloatv = (PFNGLGETFLOATVPROC)pReset;
    CVideo3d::glGetIntegerv = (PFNGLGETINTEGERVPROC)pReset;
    CVideo3d::glGetLightfv = (PFNGLGETLIGHTFVPROC)pReset;
    CVideo3d::glGetLightiv = (PFNGLGETLIGHTIVPROC)pReset;
    CVideo3d::glGetMapdv = (PFNGLGETMAPDVPROC)pReset;
    CVideo3d::glGetMapfv = (PFNGLGETMAPFVPROC)pReset;
    CVideo3d::glGetMapiv = (PFNGLGETMAPIVPROC)pReset;
    CVideo3d::glGetMaterialfv = (PFNGLGETMATERIALFVPROC)pReset;
    CVideo3d::glGetMaterialiv = (PFNGLGETMATERIALIVPROC)pReset;
    CVideo3d::glGetPixelMapfv = (PFNGLGETPIXELMAPFVPROC)pReset;
    CVideo3d::glGetPixelMapuiv = (PFNGLGETPIXELMAPUIVPROC)pReset;
    CVideo3d::glGetPixelMapusv = (PFNGLGETPIXELMAPUSVPROC)pReset;
    CVideo3d::glGetPointerv = (PFNGLGETPOINTERVPROC)pReset;
    CVideo3d::glGetPolygonStipple = (PFNGLGETPOLYGONSTIPPLEPROC)pReset;
    CVideo3d::glGetString = (PFNGLGETSTRINGPROC)pReset;
    CVideo3d::glGetTexEnvfv = (PFNGLGETTEXENVFVPROC)pReset;
    CVideo3d::glGetTexEnviv = (PFNGLGETTEXENVIVPROC)pReset;
    CVideo3d::glGetTexGendv = (PFNGLGETTEXGENDVPROC)pReset;
    CVideo3d::glGetTexGenfv = (PFNGLGETTEXGENFVPROC)pReset;
    CVideo3d::glGetTexGeniv = (PFNGLGETTEXGENIVPROC)pReset;
    CVideo3d::glGetTexImage = (PFNGLGETTEXIMAGEPROC)pReset;
    CVideo3d::glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC)pReset;
    CVideo3d::glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC)pReset;
    CVideo3d::glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC)pReset;
    CVideo3d::glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC)pReset;
    CVideo3d::glHint = (PFNGLHINTPROC)pReset;
    CVideo3d::glIndexMask = (PFNGLINDEXMASKPROC)pReset;
    CVideo3d::glIndexPointer = (PFNGLINDEXPOINTERPROC)pReset;
    CVideo3d::glIndexd = (PFNGLINDEXDPROC)pReset;
    CVideo3d::glIndexdv = (PFNGLINDEXDVPROC)pReset;
    CVideo3d::glIndexf = (PFNGLINDEXFPROC)pReset;
    CVideo3d::glIndexfv = (PFNGLINDEXFVPROC)pReset;
    CVideo3d::glIndexi = (PFNGLINDEXIPROC)pReset;
    CVideo3d::glIndexiv = (PFNGLINDEXIVPROC)pReset;
    CVideo3d::glIndexs = (PFNGLINDEXSPROC)pReset;
    CVideo3d::glIndexsv = (PFNGLINDEXSVPROC)pReset;
    CVideo3d::glIndexub = (PFNGLINDEXUBPROC)pReset;
    CVideo3d::glIndexubv = (PFNGLINDEXUBVPROC)pReset;
    CVideo3d::glInitNames = (PFNGLINITNAMESPROC)pReset;
    CVideo3d::glInterleavedArrays = (PFNGLINTERLEAVEDARRAYSPROC)pReset;
    CVideo3d::glIsEnabled = (PFNGLISENABLEDPROC)pReset;
    CVideo3d::glIsList = (PFNGLISLISTPROC)pReset;
    CVideo3d::glIsTexture = (PFNGLISTEXTUREPROC)pReset;
    CVideo3d::glLightModelf = (PFNGLLIGHTMODELFPROC)pReset;
    CVideo3d::glLightModelfv = (PFNGLLIGHTMODELFVPROC)pReset;
    CVideo3d::glLightModeli = (PFNGLLIGHTMODELIPROC)pReset;
    CVideo3d::glLightModeliv = (PFNGLLIGHTMODELIVPROC)pReset;
    CVideo3d::glLightf = (PFNGLLIGHTFPROC)pReset;
    CVideo3d::glLightfv = (PFNGLLIGHTFVPROC)pReset;
    CVideo3d::glLighti = (PFNGLLIGHTIPROC)pReset;
    CVideo3d::glLightiv = (PFNGLLIGHTIVPROC)pReset;
    CVideo3d::glLineStipple = (PFNGLLINESTIPPLEPROC)pReset;
    CVideo3d::glLineWidth = (PFNGLLINEWIDTHPROC)pReset;
    CVideo3d::glListBase = (PFNGLLISTBASEPROC)pReset;
    CVideo3d::glLoadIdentity = (PFNGLLOADIDENTITYPROC)pReset;
    CVideo3d::glLoadMatrixd = (PFNGLLOADMATRIXDPROC)pReset;
    CVideo3d::glLoadMatrixf = (PFNGLLOADMATRIXFPROC)pReset;
    CVideo3d::glLoadName = (PFNGLLOADNAMEPROC)pReset;
    CVideo3d::glLogicOp = (PFNGLLOGICOPPROC)pReset;
    CVideo3d::glMap1d = (PFNGLMAP1DPROC)pReset;
    CVideo3d::glMap1f = (PFNGLMAP1FPROC)pReset;
    CVideo3d::glMap2d = (PFNGLMAP2DPROC)pReset;
    CVideo3d::glMap2f = (PFNGLMAP2FPROC)pReset;
    CVideo3d::glMapGrid1d = (PFNGLMAPGRID1DPROC)pReset;
    CVideo3d::glMapGrid1f = (PFNGLMAPGRID1FPROC)pReset;
    CVideo3d::glMapGrid2d = (PFNGLMAPGRID2DPROC)pReset;
    CVideo3d::glMapGrid2f = (PFNGLMAPGRID2FPROC)pReset;
    CVideo3d::glMaterialf = (PFNGLMATERIALFPROC)pReset;
    CVideo3d::glMaterialfv = (PFNGLMATERIALFVPROC)pReset;
    CVideo3d::glMateriali = (PFNGLMATERIALIPROC)pReset;
    CVideo3d::glMaterialiv = (PFNGLMATERIALIVPROC)pReset;
    CVideo3d::glMatrixMode = (PFNGLMATRIXMODEPROC)pReset;
    CVideo3d::glMultMatrixd = (PFNGLMULTMATRIXDPROC)pReset;
    CVideo3d::glMultMatrixf = (PFNGLMULTMATRIXFPROC)pReset;
    CVideo3d::glNewList = (PFNGLNEWLISTPROC)pReset;
    CVideo3d::glNormal3b = (PFNGLNORMAL3BPROC)pReset;
    CVideo3d::glNormal3bv = (PFNGLNORMAL3BVPROC)pReset;
    CVideo3d::glNormal3d = (PFNGLNORMAL3DPROC)pReset;
    CVideo3d::glNormal3dv = (PFNGLNORMAL3DVPROC)pReset;
    CVideo3d::glNormal3f = (PFNGLNORMAL3FPROC)pReset;
    CVideo3d::glNormal3fv = (PFNGLNORMAL3FVPROC)pReset;
    CVideo3d::glNormal3i = (PFNGLNORMAL3IPROC)pReset;
    CVideo3d::glNormal3iv = (PFNGLNORMAL3IVPROC)pReset;
    CVideo3d::glNormal3s = (PFNGLNORMAL3SPROC)pReset;
    CVideo3d::glNormal3sv = (PFNGLNORMAL3SVPROC)pReset;
    CVideo3d::glNormalPointer = (PFNGLNORMALPOINTERPROC)pReset;
    CVideo3d::glOrtho = (PFNGLORTHOPROC)pReset;
    CVideo3d::glPassThrough = (PFNGLPASSTHROUGHPROC)pReset;
    CVideo3d::glPixelMapfv = (PFNGLPIXELMAPFVPROC)pReset;
    CVideo3d::glPixelMapuiv = (PFNGLPIXELMAPUIVPROC)pReset;
    CVideo3d::glPixelMapusv = (PFNGLPIXELMAPUSVPROC)pReset;
    CVideo3d::glPixelStoref = (PFNGLPIXELSTOREFPROC)pReset;
    CVideo3d::glPixelStorei = (PFNGLPIXELSTOREIPROC)pReset;
    CVideo3d::glPixelTransferf = (PFNGLPIXELTRANSFERFPROC)pReset;
    CVideo3d::glPixelTransferi = (PFNGLPIXELTRANSFERIPROC)pReset;
    CVideo3d::glPixelZoom = (PFNGLPIXELZOOMPROC)pReset;
    CVideo3d::glPointSize = (PFNGLPOINTSIZEPROC)pReset;
    CVideo3d::glPolygonMode = (PFNGLPOLYGONMODEPROC)pReset;
    CVideo3d::glPolygonOffset = (PFNGLPOLYGONOFFSETPROC)pReset;
    CVideo3d::glPolygonStipple = (PFNGLPOLYGONSTIPPLEPROC)pReset;
    CVideo3d::glPopAttrib = (PFNGLPOPATTRIBPROC)pReset;
    CVideo3d::glPopClientAttrib = (PFNGLPOPCLIENTATTRIBPROC)pReset;
    CVideo3d::glPopMatrix = (PFNGLPOPMATRIXPROC)pReset;
    CVideo3d::glPopName = (PFNGLPOPNAMEPROC)pReset;
    CVideo3d::glPrioritizeTextures = (PFNGLPRIORITIZETEXTURESPROC)pReset;
    CVideo3d::glPushAttrib = (PFNGLPUSHATTRIBPROC)pReset;
    CVideo3d::glPushClientAttrib = (PFNGLPUSHCLIENTATTRIBPROC)pReset;
    CVideo3d::glPushMatrix = (PFNGLPUSHMATRIXPROC)pReset;
    CVideo3d::glPushName = (PFNGLPUSHNAMEPROC)pReset;
    CVideo3d::glRasterPos2d = (PFNGLRASTERPOS2DPROC)pReset;
    CVideo3d::glRasterPos2dv = (PFNGLRASTERPOS2DVPROC)pReset;
    CVideo3d::glRasterPos2f = (PFNGLRASTERPOS2FPROC)pReset;
    CVideo3d::glRasterPos2fv = (PFNGLRASTERPOS2FVPROC)pReset;
    CVideo3d::glRasterPos2i = (PFNGLRASTERPOS2IPROC)pReset;
    CVideo3d::glRasterPos2iv = (PFNGLRASTERPOS2IVPROC)pReset;
    CVideo3d::glRasterPos2s = (PFNGLRASTERPOS2SPROC)pReset;
    CVideo3d::glRasterPos2sv = (PFNGLRASTERPOS2SVPROC)pReset;
    CVideo3d::glRasterPos3d = (PFNGLRASTERPOS3DPROC)pReset;
    CVideo3d::glRasterPos3dv = (PFNGLRASTERPOS3DVPROC)pReset;
    CVideo3d::glRasterPos3f = (PFNGLRASTERPOS3FPROC)pReset;
    CVideo3d::glRasterPos3fv = (PFNGLRASTERPOS3FVPROC)pReset;
    CVideo3d::glRasterPos3i = (PFNGLRASTERPOS3IPROC)pReset;
    CVideo3d::glRasterPos3iv = (PFNGLRASTERPOS3IVPROC)pReset;
    CVideo3d::glRasterPos3s = (PFNGLRASTERPOS3SPROC)pReset;
    CVideo3d::glRasterPos3sv = (PFNGLRASTERPOS3SVPROC)pReset;
    CVideo3d::glRasterPos4d = (PFNGLRASTERPOS4DPROC)pReset;
    CVideo3d::glRasterPos4dv = (PFNGLRASTERPOS4DVPROC)pReset;
    CVideo3d::glRasterPos4f = (PFNGLRASTERPOS4FPROC)pReset;
    CVideo3d::glRasterPos4fv = (PFNGLRASTERPOS4FVPROC)pReset;
    CVideo3d::glRasterPos4i = (PFNGLRASTERPOS4IPROC)pReset;
    CVideo3d::glRasterPos4iv = (PFNGLRASTERPOS4IVPROC)pReset;
    CVideo3d::glRasterPos4s = (PFNGLRASTERPOS4SPROC)pReset;
    CVideo3d::glRasterPos4sv = (PFNGLRASTERPOS4SVPROC)pReset;
    CVideo3d::glReadBuffer = (PFNGLREADBUFFERPROC)pReset;
    CVideo3d::glReadPixels = (PFNGLREADPIXELSPROC)pReset;
    CVideo3d::glRectd = (PFNGLRECTDPROC)pReset;
    CVideo3d::glRectdv = (PFNGLRECTDVPROC)pReset;
    CVideo3d::glRectf = (PFNGLRECTFPROC)pReset;
    CVideo3d::glRectfv = (PFNGLRECTFVPROC)pReset;
    CVideo3d::glRecti = (PFNGLRECTIPROC)pReset;
    CVideo3d::glRectiv = (PFNGLRECTIVPROC)pReset;
    CVideo3d::glRects = (PFNGLRECTSPROC)pReset;
    CVideo3d::glRectsv = (PFNGLRECTSVPROC)pReset;
    CVideo3d::glRenderMode = (PFNGLRENDERMODEPROC)pReset;
    CVideo3d::glRotated = (PFNGLROTATEDPROC)pReset;
    CVideo3d::glRotatef = (PFNGLROTATEFPROC)pReset;
    CVideo3d::glScaled = (PFNGLSCALEDPROC)pReset;
    CVideo3d::glScalef = (PFNGLSCALEFPROC)pReset;
    CVideo3d::glScissor = (PFNGLSCISSORPROC)pReset;
    CVideo3d::glSelectBuffer = (PFNGLSELECTBUFFERPROC)pReset;
    CVideo3d::glShadeModel = (PFNGLSHADEMODELPROC)pReset;
    CVideo3d::glStencilFunc = (PFNGLSTENCILFUNCPROC)pReset;
    CVideo3d::glStencilMask = (PFNGLSTENCILMASKPROC)pReset;
    CVideo3d::glStencilOp = (PFNGLSTENCILOPPROC)pReset;
    CVideo3d::glTexCoord1d = (PFNGLTEXCOORD1DPROC)pReset;
    CVideo3d::glTexCoord1dv = (PFNGLTEXCOORD1DVPROC)pReset;
    CVideo3d::glTexCoord1f = (PFNGLTEXCOORD1FPROC)pReset;
    CVideo3d::glTexCoord1fv = (PFNGLTEXCOORD1FVPROC)pReset;
    CVideo3d::glTexCoord1i = (PFNGLTEXCOORD1IPROC)pReset;
    CVideo3d::glTexCoord1iv = (PFNGLTEXCOORD1IVPROC)pReset;
    CVideo3d::glTexCoord1s = (PFNGLTEXCOORD1SPROC)pReset;
    CVideo3d::glTexCoord1sv = (PFNGLTEXCOORD1SVPROC)pReset;
    CVideo3d::glTexCoord2d = (PFNGLTEXCOORD2DPROC)pReset;
    CVideo3d::glTexCoord2dv = (PFNGLTEXCOORD2DVPROC)pReset;
    CVideo3d::glTexCoord2f = (PFNGLTEXCOORD2FPROC)pReset;
    CVideo3d::glTexCoord2fv = (PFNGLTEXCOORD2FVPROC)pReset;
    CVideo3d::glTexCoord2i = (PFNGLTEXCOORD2IPROC)pReset;
    CVideo3d::glTexCoord2iv = (PFNGLTEXCOORD2IVPROC)pReset;
    CVideo3d::glTexCoord2s = (PFNGLTEXCOORD2SPROC)pReset;
    CVideo3d::glTexCoord2sv = (PFNGLTEXCOORD2SVPROC)pReset;
    CVideo3d::glTexCoord3d = (PFNGLTEXCOORD3DPROC)pReset;
    CVideo3d::glTexCoord3dv = (PFNGLTEXCOORD3DVPROC)pReset;
    CVideo3d::glTexCoord3f = (PFNGLTEXCOORD3FPROC)pReset;
    CVideo3d::glTexCoord3fv = (PFNGLTEXCOORD3FVPROC)pReset;
    CVideo3d::glTexCoord3i = (PFNGLTEXCOORD3IPROC)pReset;
    CVideo3d::glTexCoord3iv = (PFNGLTEXCOORD3IVPROC)pReset;
    CVideo3d::glTexCoord3s = (PFNGLTEXCOORD3SPROC)pReset;
    CVideo3d::glTexCoord3sv = (PFNGLTEXCOORD3SVPROC)pReset;
    CVideo3d::glTexCoord4d = (PFNGLTEXCOORD4DPROC)pReset;
    CVideo3d::glTexCoord4dv = (PFNGLTEXCOORD4DVPROC)pReset;
    CVideo3d::glTexCoord4f = (PFNGLTEXCOORD4FPROC)pReset;
    CVideo3d::glTexCoord4fv = (PFNGLTEXCOORD4FVPROC)pReset;
    CVideo3d::glTexCoord4i = (PFNGLTEXCOORD4IPROC)pReset;
    CVideo3d::glTexCoord4iv = (PFNGLTEXCOORD4IVPROC)pReset;
    CVideo3d::glTexCoord4s = (PFNGLTEXCOORD4SPROC)pReset;
    CVideo3d::glTexCoord4sv = (PFNGLTEXCOORD4SVPROC)pReset;
    CVideo3d::glTexCoordPointer = (PFNGLTEXCOORDPOINTERPROC)pReset;
    CVideo3d::glTexEnvf = (PFNGLTEXENVFPROC)pReset;
    CVideo3d::glTexEnvfv = (PFNGLTEXENVFVPROC)pReset;
    CVideo3d::glTexEnvi = (PFNGLTEXENVIPROC)pReset;
    CVideo3d::glTexEnviv = (PFNGLTEXENVIVPROC)pReset;
    CVideo3d::glTexGend = (PFNGLTEXGENDPROC)pReset;
    CVideo3d::glTexGendv = (PFNGLTEXGENDVPROC)pReset;
    CVideo3d::glTexGenf = (PFNGLTEXGENFPROC)pReset;
    CVideo3d::glTexGenfv = (PFNGLTEXGENFVPROC)pReset;
    CVideo3d::glTexGeni = (PFNGLTEXGENIPROC)pReset;
    CVideo3d::glTexGeniv = (PFNGLTEXGENIVPROC)pReset;
    CVideo3d::glTexImage1D = (PFNGLTEXIMAGE1DPROC)pReset;
    CVideo3d::glTexImage2D = (PFNGLTEXIMAGE2DPROC)pReset;
    CVideo3d::glTexParameterf = (PFNGLTEXPARAMETERFPROC)pReset;
    CVideo3d::glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)pReset;
    CVideo3d::glTexParameteri = (PFNGLTEXPARAMETERIPROC)pReset;
    CVideo3d::glTexParameteriv = (PFNGLTEXPARAMETERIVPROC)pReset;
    CVideo3d::glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC)pReset;
    CVideo3d::glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)pReset;
    CVideo3d::glTranslated = (PFNGLTRANSLATEDPROC)pReset;
    CVideo3d::glTranslatef = (PFNGLTRANSLATEFPROC)pReset;
    CVideo3d::glVertex2d = (PFNGLVERTEX2DPROC)pReset;
    CVideo3d::glVertex2dv = (PFNGLVERTEX2DVPROC)pReset;
    CVideo3d::glVertex2f = (PFNGLVERTEX2FPROC)pReset;
    CVideo3d::glVertex2fv = (PFNGLVERTEX2FVPROC)pReset;
    CVideo3d::glVertex2i = (PFNGLVERTEX2IPROC)pReset;
    CVideo3d::glVertex2iv = (PFNGLVERTEX2IVPROC)pReset;
    CVideo3d::glVertex2s = (PFNGLVERTEX2SPROC)pReset;
    CVideo3d::glVertex2sv = (PFNGLVERTEX2SVPROC)pReset;
    CVideo3d::glVertex3d = (PFNGLVERTEX3DPROC)pReset;
    CVideo3d::glVertex3dv = (PFNGLVERTEX3DVPROC)pReset;
    CVideo3d::glVertex3f = (PFNGLVERTEX3FPROC)pReset;
    CVideo3d::glVertex3fv = (PFNGLVERTEX3FVPROC)pReset;
    CVideo3d::glVertex3i = (PFNGLVERTEX3IPROC)pReset;
    CVideo3d::glVertex3iv = (PFNGLVERTEX3IVPROC)pReset;
    CVideo3d::glVertex3s = (PFNGLVERTEX3SPROC)pReset;
    CVideo3d::glVertex3sv = (PFNGLVERTEX3SVPROC)pReset;
    CVideo3d::glVertex4d = (PFNGLVERTEX4DPROC)pReset;
    CVideo3d::glVertex4dv = (PFNGLVERTEX4DVPROC)pReset;
    CVideo3d::glVertex4f = (PFNGLVERTEX4FPROC)pReset;
    CVideo3d::glVertex4fv = (PFNGLVERTEX4FVPROC)pReset;
    CVideo3d::glVertex4i = (PFNGLVERTEX4IPROC)pReset;
    CVideo3d::glVertex4iv = (PFNGLVERTEX4IVPROC)pReset;
    CVideo3d::glVertex4s = (PFNGLVERTEX4SPROC)pReset;
    CVideo3d::glVertex4sv = (PFNGLVERTEX4SVPROC)pReset;
    CVideo3d::glVertexPointer = (PFNGLVERTEXPOINTERPROC)pReset;
    CVideo3d::glViewport = (PFNGLVIEWPORTPROC)pReset;
    CVideo3d::gluBeginCurve = (PFNGLUBEGINCURVEPROC)pReset;
    CVideo3d::gluBeginPolygon = (PFNGLUBEGINPOLYGONPROC)pReset;
    CVideo3d::gluBeginSurface = (PFNGLUBEGINSURFACEPROC)pReset;
    CVideo3d::gluBeginTrim = (PFNGLUBEGINTRIMPROC)pReset;
    CVideo3d::gluBuild1DMipmaps = (PFNGLUBUILD1DMIPMAPSPROC)pReset;
    CVideo3d::gluBuild2DMipmaps = (PFNGLUBUILD2DMIPMAPSPROC)pReset;
    CVideo3d::gluCylinder = (PFNGLUCYLINDERPROC)pReset;
    CVideo3d::gluDeleteNurbsRenderer = (PFNGLUDELETENURBSRENDERERPROC)pReset;
    CVideo3d::gluDeleteQuadric = (PFNGLUDELETEQUADRICPROC)pReset;
    CVideo3d::gluDeleteTess = (PFNGLUDELETETESSPROC)pReset;
    CVideo3d::gluDisk = (PFNGLUDISKPROC)pReset;
    CVideo3d::gluEndCurve = (PFNGLUENDCURVEPROC)pReset;
    CVideo3d::gluEndPolygon = (PFNGLUENDPOLYGONPROC)pReset;
    CVideo3d::gluEndSurface = (PFNGLUENDSURFACEPROC)pReset;
    CVideo3d::gluEndTrim = (PFNGLUENDTRIMPROC)pReset;
    CVideo3d::gluErrorString = (PFNGLUERRORSTRINGPROC)pReset;
    CVideo3d::gluGetNurbsProperty = (PFNGLUGETNURBSPROPERTYPROC)pReset;
    CVideo3d::gluGetString = (PFNGLUGETSTRINGPROC)pReset;
    CVideo3d::gluGetTessProperty = (PFNGLUGETTESSPROPERTYPROC)pReset;
    CVideo3d::gluLoadSamplingMatrices = (PFNGLULOADSAMPLINGMATRICESPROC)pReset;
    CVideo3d::gluLookAt = (PFNGLULOOKATPROC)pReset;
    CVideo3d::gluNewNurbsRenderer = (PFNGLUNEWNURBSRENDERERPROC)pReset;
    CVideo3d::gluNewQuadric = (PFNGLUNEWQUADRICPROC)pReset;
    CVideo3d::gluNewTess = (PFNGLUNEWTESSPROC)pReset;
    CVideo3d::gluNextContour = (PFNGLUNEXTCONTOURPROC)pReset;
    CVideo3d::gluNurbsCallback = (PFNGLUNURBSCALLBACKPROC)pReset;
    CVideo3d::gluNurbsCurve = (PFNGLUNURBSCURVEPROC)pReset;
    CVideo3d::gluNurbsProperty = (PFNGLUNURBSPROPERTYPROC)pReset;
    CVideo3d::gluNurbsSurface = (PFNGLUNURBSSURFACEPROC)pReset;
    CVideo3d::gluOrtho2D = (PFNGLUORTHO2DPROC)pReset;
    CVideo3d::gluPartialDisk = (PFNGLUPARTIALDISKPROC)pReset;
    CVideo3d::gluPerspective = (PFNGLUPERSPECTIVEPROC)pReset;
    CVideo3d::gluPickMatrix = (PFNGLUPICKMATRIXPROC)pReset;
    CVideo3d::gluProject = (PFNGLUPROJECTPROC)pReset;
    CVideo3d::gluPwlCurve = (PFNGLUPWLCURVEPROC)pReset;
    CVideo3d::gluQuadricCallback = (PFNGLUQUADRICCALLBACKPROC)pReset;
    CVideo3d::gluQuadricDrawStyle = (PFNGLUQUADRICDRAWSTYLEPROC)pReset;
    CVideo3d::gluQuadricNormals = (PFNGLUQUADRICNORMALSPROC)pReset;
    CVideo3d::gluQuadricOrientation = (PFNGLUQUADRICORIENTATIONPROC)pReset;
    CVideo3d::gluQuadricTexture = (PFNGLUQUADRICTEXTUREPROC)pReset;
    CVideo3d::gluScaleImage = (PFNGLUSCALEIMAGEPROC)pReset;
    CVideo3d::gluSphere = (PFNGLUSPHEREPROC)pReset;
    CVideo3d::gluTessBeginContour = (PFNGLUTESSBEGINCONTOURPROC)pReset;
    CVideo3d::gluTessBeginPolygon = (PFNGLUTESSBEGINPOLYGONPROC)pReset;
    CVideo3d::gluTessCallback = (PFNGLUTESSCALLBACKPROC)pReset;
    CVideo3d::gluTessEndContour = (PFNGLUTESSENDCONTOURPROC)pReset;
    CVideo3d::gluTessEndPolygon = (PFNGLUTESSENDPOLYGONPROC)pReset;
    CVideo3d::gluTessNormal = (PFNGLUTESSNORMALPROC)pReset;
    CVideo3d::gluTessProperty = (PFNGLUTESSPROPERTYPROC)pReset;
    CVideo3d::gluTessVertex = (PFNGLUTESSVERTEXPROC)pReset;
    CVideo3d::gluUnProject = (PFNGLUUNPROJECTPROC)pReset;
    g_pfnGLSetupWglCopyContext            = (PFNWGLCOPYCONTEXTPROC)pReset;
    g_pfnGLSetupWglCreateContext          = (PFNWGLCREATECONTEXTPROC)pReset;
    g_pfnGLSetupWglCreateLayerContext     = (PFNWGLCREATELAYERCONTEXTPROC)pReset;
    g_pfnGLSetupWglDeleteContext          = (PFNWGLDELETECONTEXTPROC)pReset;
    g_pfnGLSetupWglDescribeLayerPlane     = (PFNWGLDESCRIBELAYERPLANEPROC)pReset;
    g_pfnGLSetupWglGetCurrentContext      = (PFNWGLGETCURRENTCONTEXTPROC)pReset;
    g_pfnGLSetupWglGetCurrentDC           = (PFNWGLGETCURRENTDCPROC)pReset;
    g_pfnGLSetupWglGetLayerPaletteEntries = (PFNWGLGETLAYERPALETTEENTRIESPROC)pReset;
    g_pfnGLSetupWglGetProcAddress         = (PFNWGLGETPROCADDRESSPROC)pReset;
    g_pfnGLSetupWglMakeCurrent            = (PFNWGLMAKECURRENTPROC)pReset;
    g_pfnGLSetupWglRealizeLayerPalette    = (PFNWGLREALIZELAYERPALETTEPROC)pReset;
    g_pfnGLSetupWglSetLayerPaletteEntries = (PFNWGLSETLAYERPALETTEENTRIESPROC)pReset;
    g_pfnGLSetupWglShareLists             = (PFNWGLSHARELISTSPROC)pReset;
    g_pfnGLSetupWglSwapLayerBuffers       = (PFNWGLSWAPLAYERBUFFERSPROC)pReset;
    g_pfnGLSetupWglUseFontBitmapsA        = (PFNWGLUSEFONTBITMAPSAPROC)pReset;
    g_pfnGLSetupWglUseFontOutlinesA       = (PFNWGLUSEFONTOUTLINESAPROC)pReset;
    g_pfnGLSetupChoosePixelFormat         = (PFNCHOOSEPIXELFORMATPROC)pReset;
    g_pfnGLSetupDescribePixelFormat       = (PFNDESCRIBEPIXELFORMATPROC)pReset;
    g_pfnGLSetupGetPixelFormat            = (PFNGETPIXELFORMATPROC)pReset;
    g_pfnGLSetupSetPixelFormat            = (PFNSETPIXELFORMATPROC)pReset;
    g_pfnGLSetupSwapBuffers               = (PFNSWAPBUFFERSPROC)pReset;

    g_szGLSetupPrimaryPath[0] = '\0';
    g_szGLSetupMiniPath[0] = '\0';
    g_nSelectedGLSetupDriver = -1;
}

// -----------------------------------------------------------------------------

// #binary-identical
// 0x7C8980
void CChitin::InitVariables3D()
{
    m_hOpenGL = 0;
    field_2F4 = 0;
}

// 0x7C8990
BOOL CChitin::Init3d()
{
    // NOTE: Original code is different. This function is an entry point to
    // several functions used to pick and setup various OpenGL drivers which
    // were available at that time:
    // - opengl32.dll + glu32.dll
    // - 3dfxogl.dll + 3dfoglu.dll
    // - opengl.dll + glu.dll
    // - 3dfxvgl.dll + 3dfxvglu.dll
    //
    // Since it's very unlikely someone still has these drivers and video cards,
    // this code is omitted.

    CString sMessage;
    sMessage.LoadStringA(GetIDSOpenGLDll());

    m_hOpenGL = LoadLibraryA("opengl32.dll");
    sMessage += "opengl32.dll";

    if (m_hOpenGL == NULL) {
        fprintf(stderr, "%s\n", static_cast<LPCSTR>(sMessage));
        return FALSE;
    }

    InitOpenGL(m_hOpenGL);

    return TRUE;
}

// 0x7C8B10
void CChitin::Shutdown3D()
{
    // NOTE: Original code is slightly different - it checks for current
    // driver's `wglCreateContext`.
    if (m_hOpenGL) {
        FreeLibrary(m_hOpenGL);
    }
}

// -----------------------------------------------------------------------------

// 0x7D8E80
void InitOpenGL(HMODULE hOpenGL)
{
    CChitin3d::wglCreateContext = (PFNWGLCREATECONTEXTPROC)GetProcAddress(hOpenGL, "wglCreateContext");
    CChitin3d::wglDeleteContext = (PFNWGLDELETECONTEXTPROC)GetProcAddress(hOpenGL, "wglDeleteContext");
    CChitin3d::wglMakeCurrent = (PFNWGLMAKECURRENTPROC)GetProcAddress(hOpenGL, "wglMakeCurrent");
    CChitin3d::wglGetCurrentDC = (PFNWGLGETCURRENTDCPROC)GetProcAddress(hOpenGL, "wglGetCurrentDC");
    CChitin3d::ChoosePixelFormat = ChoosePixelFormat;
    CChitin3d::SetPixelFormat = SetPixelFormat;
    CChitin3d::SwapBuffers = SwapBuffers;
    CVideo3d::glBegin = (PFNGLBEGINPROC)GetProcAddress(hOpenGL, "glBegin");
    CVideo3d::glBindTexture = (PFNGLBINDTEXTUREPROC)GetProcAddress(hOpenGL, "glBindTexture");
    CVideo3d::glBlendFunc = (PFNGLBLENDFUNCPROC)GetProcAddress(hOpenGL, "glBlendFunc");
    CVideo3d::glClear = (PFNGLCLEARPROC)GetProcAddress(hOpenGL, "glClear");
    CVideo3d::glClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(hOpenGL, "glClearColor");
    CVideo3d::glClearDepth = (PFNGLCLEARDEPTHPROC)GetProcAddress(hOpenGL, "glClearDepth");
    CVideo3d::glColor4f = (PFNGLCOLOR4FPROC)GetProcAddress(hOpenGL, "glColor4f");
    CVideo3d::glCullFace = (PFNGLCULLFACEPROC)GetProcAddress(hOpenGL, "glCullFace");
    CVideo3d::glDeleteTextures = (PFNGLDELETETEXTURESPROC)GetProcAddress(hOpenGL, "glDeleteTextures");
    CVideo3d::glDisable = (PFNGLDISABLEPROC)GetProcAddress(hOpenGL, "glDisable");
    CVideo3d::glEnable = (PFNGLENABLEPROC)GetProcAddress(hOpenGL, "glEnable");
    CVideo3d::glEnd = (PFNGLENDPROC)GetProcAddress(hOpenGL, "glEnd");
    CVideo3d::glFlush = (PFNGLFLUSHPROC)GetProcAddress(hOpenGL, "glFlush");
    CVideo3d::glFrontFace = (PFNGLFRONTFACEPROC)GetProcAddress(hOpenGL, "glFrontFace");
    CVideo3d::glGenTextures = (PFNGLGENTEXTURESPROC)GetProcAddress(hOpenGL, "glGenTextures");
    CVideo3d::glGetError = (PFNGLGETERRORPROC)GetProcAddress(hOpenGL, "glGetError");
    CVideo3d::glGetIntegerv = (PFNGLGETINTEGERVPROC)GetProcAddress(hOpenGL, "glGetIntegerv");
    CVideo3d::glIsTexture = (PFNGLISTEXTUREPROC)GetProcAddress(hOpenGL, "glIsTexture");
    CVideo3d::glLineWidth = (PFNGLLINEWIDTHPROC)GetProcAddress(hOpenGL, "glLineWidth");
    CVideo3d::glLoadIdentity = (PFNGLLOADIDENTITYPROC)GetProcAddress(hOpenGL, "glLoadIdentity");
    CVideo3d::glMatrixMode = (PFNGLMATRIXMODEPROC)GetProcAddress(hOpenGL, "glMatrixMode");
    CVideo3d::glOrtho = (PFNGLORTHOPROC)GetProcAddress(hOpenGL, "glOrtho");
    CVideo3d::glPixelStorei = (PFNGLPIXELSTOREIPROC)GetProcAddress(hOpenGL, "glPixelStorei");
    CVideo3d::glPointSize = (PFNGLPOINTSIZEPROC)GetProcAddress(hOpenGL, "glPointSize");
    CVideo3d::glPolygonStipple = (PFNGLPOLYGONSTIPPLEPROC)GetProcAddress(hOpenGL, "glPolygonStipple");
    CVideo3d::glPushMatrix = (PFNGLPUSHMATRIXPROC)GetProcAddress(hOpenGL, "glPushMatrix");
    CVideo3d::glReadBuffer = (PFNGLREADBUFFERPROC)GetProcAddress(hOpenGL, "glReadBuffer");
    CVideo3d::glReadPixels = (PFNGLREADPIXELSPROC)GetProcAddress(hOpenGL, "glReadPixels");
    CVideo3d::glScissor = (PFNGLSCISSORPROC)GetProcAddress(hOpenGL, "glScissor");
    CVideo3d::glShadeModel = (PFNGLSHADEMODELPROC)GetProcAddress(hOpenGL, "glShadeModel");
    CVideo3d::glTexCoord2f = (PFNGLTEXCOORD2FPROC)GetProcAddress(hOpenGL, "glTexCoord2f");
    CVideo3d::glTexEnvf = (PFNGLTEXENVFPROC)GetProcAddress(hOpenGL, "glTexEnvf");
    CVideo3d::glTexImage2D = (PFNGLTEXIMAGE2DPROC)GetProcAddress(hOpenGL, "glTexImage2D");
    CVideo3d::glTexParameterf = (PFNGLTEXPARAMETERFPROC)GetProcAddress(hOpenGL, "glTexParameterf");
    CVideo3d::glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)GetProcAddress(hOpenGL, "glTexSubImage2D");
    CVideo3d::glVertex3f = (PFNGLVERTEX3FPROC)GetProcAddress(hOpenGL, "glVertex3f");
    CVideo3d::glViewport = (PFNGLVIEWPORTPROC)GetProcAddress(hOpenGL, "glViewport");
}
