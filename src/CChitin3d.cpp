#include "CChitin3d.h"

#include "CChitin.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

// -----------------------------------------------------------------------------
// GLSetup driver detection (WSO fork import, batch i074).
//
// The original Init3d ran a driver-selection pipeline: it enumerated the
// candidate OpenGL driver pairs (the plain ICD, 3dfx, SGI software), validated
// each by file-version resource and canonical location, probed one by loading
// it and resolving its full GL 1.1 / WGL / GLU entry-point table, and reset
// the lot on failure. Our Init3d above stays simplified; these are the
// probe-side functions, kept address-faithful. EnumDrivers / SelectDriver are
// not part of this import batch.
// -----------------------------------------------------------------------------

// Win32 types used only by the GLSetup probe below.
typedef DWORD REGSAM;
typedef HKEY* PHKEY;

typedef BOOL(__stdcall* PFNWGLCOPYCONTEXTPROC)(HGLRC, HGLRC, UINT);
typedef int(__stdcall* PFNGETPIXELFORMATPROC)(HDC);

#pragma pack(push, 4)
struct VS_FIXEDFILEINFO {
    DWORD dwSignature;        // 0xFEEF04BD
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
#pragma pack(pop)

static_assert(sizeof(VS_FIXEDFILEINFO) == 0x34, "VS_FIXEDFILEINFO must match the exe's 0x34 block");
static_assert(sizeof(BY_HANDLE_FILE_INFORMATION) == 52, "BY_HANDLE_FILE_INFORMATION layout");

// Registry constants of the SGI dispatch override (values per Win32).
static const HKEY HKEY_LOCAL_MACHINE = reinterpret_cast<HKEY>(static_cast<intptr_t>(0x80000002u));
#define KEY_QUERY_VALUE 0x0001
#define KEY_ALL_ACCESS 0xF003F
#define REG_DWORD 4

namespace {

// advapi32 registry shims: the port never touches the real registry, so every
// open/create fails and the probe's OverrideDispatch read / restore paths are
// inert, exactly as on a machine without the SGI OpenGL runtime.
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, REGSAM, PHKEY)
{
    return 2; // ERROR_FILE_NOT_FOUND
}

LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD)
{
    return 2; // ERROR_FILE_NOT_FOUND
}

LONG RegCloseKey(HKEY)
{
    return 0;
}

LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD)
{
    return 5; // ERROR_ACCESS_DENIED
}

LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD)
{
    return 5; // ERROR_ACCESS_DENIED
}

// The user locale is fixed on the port (en-US), as elsewhere in the tree.
LANGID GetUserDefaultLangID()
{
    return 0x0409;
}

// The port has no NTFS metadata behind CreateFileA handles; report zeroed
// info so the recorded last-write times read as 0.
BOOL GetFileInformationByHandle(HANDLE, BY_HANDLE_FILE_INFORMATION* pInfo)
{
    if (pInfo != NULL) {
        *pInfo = BY_HANDLE_FILE_INFORMATION();
    }
    return TRUE;
}

// No Windows path canonicalization exists on the port: copy the path as given
// and report the component after the final separator as the file part.
DWORD GetFullPathNameA(LPCSTR pszPath, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart)
{
    const DWORD dwLen = (pszPath != NULL) ? (DWORD)strlen(pszPath) : 0;
    if (dwLen == 0 || nBufferLength <= dwLen) {
        if (lpFilePart != NULL) {
            *lpFilePart = NULL;
        }
        return dwLen;
    }
    strcpy(lpBuffer, pszPath);
    LPSTR pszLastSlash = NULL;
    for (LPSTR p = lpBuffer; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            pszLastSlash = p;
        }
    }
    if (lpFilePart != NULL) {
        *lpFilePart = (pszLastSlash != NULL) ? pszLastSlash + 1 : lpBuffer;
    }
    return dwLen;
}

// The port has no Windows system directory; callers treat 0 as a failure.
DWORD GetSystemDirectoryA(LPSTR, DWORD)
{
    return 0;
}

// _putenv stand-in: no environment to mutate on the port.
int GLSetupPutEnv(const char*)
{
    return 0;
}

// _mbslwr / _mbsicmp stand-ins (ASCII, in place) so the fork's call sites keep
// their original shape.
unsigned char* GLSetupStrLwr(unsigned char* psz)
{
    for (unsigned char* p = psz; *p != '\0'; ++p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = (unsigned char)(*p - 'A' + 'a');
        }
    }
    return psz;
}

int GLSetupStrICmp(const unsigned char* a, const unsigned char* b)
{
    while (*a != '\0' && *b != '\0') {
        unsigned char ca = *a;
        unsigned char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        ++a;
        ++b;
    }
    return (int)*a - (int)*b;
}

} // namespace

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
#pragma pack(push, 2)
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
// g_aGLSetupDrivers[] at 0x906A34 with this stride.
struct GLSETUP_DRIVER {
    /* 0x000 */ DWORD dwFlags;   // 0x80000000 valid, 0x40 mini ver fail, 0x60 mini
                                 //   invalid, 0x10 no ver ("Unknown"), 0x08 in system
                                 //   dir, 0x04 is the system opengl32.dll
    /* 0x004 */ CHAR szId[0x100];             // "Company - Description" / "Unknown"
    /* 0x104 */ GLSETUP_DRIVER_FILE primary;
    /* 0x218 */ GLSETUP_DRIVER_FILE mini;
};
#pragma pack(pop)
// GLSetup control flags: bit0 skips mini-driver validation, bit1 skips the
// primary/mini same-directory check.
// 0x9074AC
static DWORD g_dwGLSetupControl;
// Index of the driver selected by GLSetupSelectDriver, or -1.
// 0x8BB0A0
static int g_nSelectedGLSetupDriver = -1;
// Defined below in address order; forward-declared for the earlier callers.
static int GLSetupGetDriverVersion(LPCSTR pszFilePath, void* pDriverInfo, char* pszIdOut, unsigned int cbIdOut);
static void GLSetupResetDriver(void);


// Validates a candidate driver: records each DLL's last-write time and file
// version, classifies it (resides in the system dir, is the system
// opengl32.dll, ...) via canonical-path comparisons, and rejects a mini-driver
// that is not co-located with the primary. Returns 0 on success, 4 on failure.
// 0x7B7940
static int GLSetupValidateDriver(LPCSTR pszPrimaryPath, LPCSTR pszMiniPath, GLSETUP_DRIVER* pDriver)
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
                    GLSetupStrLwr((unsigned char*)pDriver->mini.szPath);
                    GLSetupStrLwr((unsigned char*)szSysDir);
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
        GLSetupStrLwr((unsigned char*)pDriver->primary.szPath);
        GLSetupStrLwr((unsigned char*)szSysDir);
        if (nDirLen - 1 == (int)strlen(szSysDir) &&
            strstr(pDriver->primary.szPath, szSysDir) == pDriver->primary.szPath) {
            pDriver->dwFlags |= 8;
        }
    }

    GetSystemDirectoryA(szSysDir, 0x104);
    strcat(szSysDir, "\\opengl32.dll");
    GetFullPathNameA(szSysDir, 0x104, szNormA, &pFilePart);
    if (GetFullPathNameA(pDriver->primary.szPath, 0x104, szNormB, &pFilePart) != 0) {
        if (GLSetupStrICmp((unsigned char*)szNormB, (unsigned char*)szNormA) == 0) {
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
        if (GLSetupStrICmp((unsigned char*)szNormA, (unsigned char*)szNormB) != 0) {
            pDriver->mini.szPath[0] = '\0';
            pDriver->dwFlags |= 0x60;
        }
    }

    return 0;
}
// Probes a candidate driver: primes the FX_GLIDE_NO_SPLASH env var and the SGI
// OpenGL "OverrideDispatch" registry flag, verifies the primary and mini DLLs
// share a directory and that the primary is a real driver (not the plain system
// opengl32.dll), loads them, and resolves the full OpenGL 1.1 / WGL / GLU
// entry-point table via GetProcAddress. Returns 0 if every required entry point
// resolved, 2 if any was missing, or 1 on an early co-location / load failure.
// 0x7B7DA0
static int GLSetupProbeDriver(const char* pszPrimaryPath, const char* pszMiniPath)
{
    CHAR szScratch[0x104];
    CHAR szNormA[0x104];
    CHAR szNormB[0x104];
    BOOL bMissing = FALSE;

    GLSetupResetDriver();

    // Suppress the 3dfx Glide splash by exporting FX_GLIDE_NO_SPLASH=1 (unless
    // disabled by control bit 0x10). The env var is sampled once and cached.
    if ((g_dwGLSetupControl & 0x10) == 0) {
        if (g_bGLSetupSplashEnvChecked == 0) {
            g_pszGLSetupSplashEnv = getenv("FX_GLIDE_NO_SPLASH");
            g_bGLSetupSplashEnvChecked = 1;
        }
        if (strlen("FX_GLIDE_NO_SPLASH") + strlen("1") < 0x100) {
            sprintf(szScratch, "%s=%s", "FX_GLIDE_NO_SPLASH", "1");
            GLSetupPutEnv(szScratch);
        }
    }

    // Force the SGI OpenGL dispatch override on, caching the prior value once
    // (unless disabled by control bit 0x20).
    if ((g_dwGLSetupControl & 0x20) == 0) {
        if (g_bGLSetupDispatchChecked == 0) {
            HKEY  hKey = (HKEY)-1;
            LPSTR pValue = NULL;
            DWORD cbData = 4;
            DWORD dwType = 4;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Silicon Graphics\\OpenGL",
                              0, KEY_QUERY_VALUE, &hKey) == 0) {
                RegQueryValueExA(hKey, "OverrideDispatch", NULL, &dwType,
                                 (LPBYTE)&pValue, &cbData);
                RegCloseKey(hKey);
            }
            g_bGLSetupDispatchChecked = 1;
            g_pGLSetupOverrideDispatch = pValue;
        }
        DWORD dwValue = 1;
        DWORD dwDisposition;
        HKEY  hKey = (HKEY)-1;
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Silicon Graphics\\OpenGL",
                            0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition) == 0) {
            RegSetValueExA(hKey, "OverrideDispatch", 0, REG_DWORD, (const BYTE*)&dwValue, 4);
            RegCloseKey(hKey);
        }
    }

    // Require the primary and mini DLLs to live in the same directory (unless
    // control bit 2 skips the check); reject the driver otherwise.
    if ((g_dwGLSetupControl & 2) == 0 && pszMiniPath != NULL && *pszMiniPath != '\0') {
        LPSTR pFileA = NULL;
        LPSTR pFileB = NULL;
        GetFullPathNameA(pszPrimaryPath, 0x104, szNormA, &pFileA);
        GetFullPathNameA(pszMiniPath, 0x104, szScratch, &pFileB);
        if (pFileA != NULL) {
            *pFileA = '\0';
        }
        if (pFileB != NULL) {
            *pFileB = '\0';
        }
        if (GLSetupStrICmp((unsigned char*)szNormA, (unsigned char*)szScratch) != 0) {
            return 1;
        }
    }

    if (pszPrimaryPath == NULL) {
        return 1;
    }
    g_hGLSetupDriver = LoadLibraryA(pszPrimaryPath);
    if (g_hGLSetupDriver == NULL) {
        return 1;
    }

    // ---- OpenGL 1.1 entry points (each missing one flags the driver). ----
    CVideo3d::glAccum = (PFNGLACCUMPROC)GetProcAddress(g_hGLSetupDriver, "glAccum");
    if (CVideo3d::glAccum == NULL) bMissing = TRUE;
    CVideo3d::glAlphaFunc = (PFNGLALPHAFUNCPROC)GetProcAddress(g_hGLSetupDriver, "glAlphaFunc");
    if (CVideo3d::glAlphaFunc == NULL) bMissing = TRUE;
    CVideo3d::glAreTexturesResident = (PFNGLARETEXTURESRESIDENTPROC)GetProcAddress(g_hGLSetupDriver, "glAreTexturesResident");
    if (CVideo3d::glAreTexturesResident == NULL) bMissing = TRUE;
    CVideo3d::glArrayElement = (PFNGLARRAYELEMENTPROC)GetProcAddress(g_hGLSetupDriver, "glArrayElement");
    if (CVideo3d::glArrayElement == NULL) bMissing = TRUE;
    CVideo3d::glBegin = (PFNGLBEGINPROC)GetProcAddress(g_hGLSetupDriver, "glBegin");
    if (CVideo3d::glBegin == NULL) bMissing = TRUE;
    CVideo3d::glBindTexture = (PFNGLBINDTEXTUREPROC)GetProcAddress(g_hGLSetupDriver, "glBindTexture");
    if (CVideo3d::glBindTexture == NULL) bMissing = TRUE;
    CVideo3d::glBitmap = (PFNGLBITMAPPROC)GetProcAddress(g_hGLSetupDriver, "glBitmap");
    if (CVideo3d::glBitmap == NULL) bMissing = TRUE;
    CVideo3d::glBlendFunc = (PFNGLBLENDFUNCPROC)GetProcAddress(g_hGLSetupDriver, "glBlendFunc");
    if (CVideo3d::glBlendFunc == NULL) bMissing = TRUE;
    CVideo3d::glCallList = (PFNGLCALLLISTPROC)GetProcAddress(g_hGLSetupDriver, "glCallList");
    if (CVideo3d::glCallList == NULL) bMissing = TRUE;
    CVideo3d::glCallLists = (PFNGLCALLLISTSPROC)GetProcAddress(g_hGLSetupDriver, "glCallLists");
    if (CVideo3d::glCallLists == NULL) bMissing = TRUE;
    CVideo3d::glClear = (PFNGLCLEARPROC)GetProcAddress(g_hGLSetupDriver, "glClear");
    if (CVideo3d::glClear == NULL) bMissing = TRUE;
    CVideo3d::glClearAccum = (PFNGLCLEARACCUMPROC)GetProcAddress(g_hGLSetupDriver, "glClearAccum");
    if (CVideo3d::glClearAccum == NULL) bMissing = TRUE;
    CVideo3d::glClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(g_hGLSetupDriver, "glClearColor");
    if (CVideo3d::glClearColor == NULL) bMissing = TRUE;
    CVideo3d::glClearDepth = (PFNGLCLEARDEPTHPROC)GetProcAddress(g_hGLSetupDriver, "glClearDepth");
    if (CVideo3d::glClearDepth == NULL) bMissing = TRUE;
    CVideo3d::glClearIndex = (PFNGLCLEARINDEXPROC)GetProcAddress(g_hGLSetupDriver, "glClearIndex");
    if (CVideo3d::glClearIndex == NULL) bMissing = TRUE;
    CVideo3d::glClearStencil = (PFNGLCLEARSTENCILPROC)GetProcAddress(g_hGLSetupDriver, "glClearStencil");
    if (CVideo3d::glClearStencil == NULL) bMissing = TRUE;
    CVideo3d::glClipPlane = (PFNGLCLIPPLANEPROC)GetProcAddress(g_hGLSetupDriver, "glClipPlane");
    if (CVideo3d::glClipPlane == NULL) bMissing = TRUE;
    CVideo3d::glColor3b = (PFNGLCOLOR3BPROC)GetProcAddress(g_hGLSetupDriver, "glColor3b");
    if (CVideo3d::glColor3b == NULL) bMissing = TRUE;
    CVideo3d::glColor3bv = (PFNGLCOLOR3BVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3bv");
    if (CVideo3d::glColor3bv == NULL) bMissing = TRUE;
    CVideo3d::glColor3d = (PFNGLCOLOR3DPROC)GetProcAddress(g_hGLSetupDriver, "glColor3d");
    if (CVideo3d::glColor3d == NULL) bMissing = TRUE;
    CVideo3d::glColor3dv = (PFNGLCOLOR3DVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3dv");
    if (CVideo3d::glColor3dv == NULL) bMissing = TRUE;
    CVideo3d::glColor3f = (PFNGLCOLOR3FPROC)GetProcAddress(g_hGLSetupDriver, "glColor3f");
    if (CVideo3d::glColor3f == NULL) bMissing = TRUE;
    CVideo3d::glColor3fv = (PFNGLCOLOR3FVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3fv");
    if (CVideo3d::glColor3fv == NULL) bMissing = TRUE;
    CVideo3d::glColor3i = (PFNGLCOLOR3IPROC)GetProcAddress(g_hGLSetupDriver, "glColor3i");
    if (CVideo3d::glColor3i == NULL) bMissing = TRUE;
    CVideo3d::glColor3iv = (PFNGLCOLOR3IVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3iv");
    if (CVideo3d::glColor3iv == NULL) bMissing = TRUE;
    CVideo3d::glColor3s = (PFNGLCOLOR3SPROC)GetProcAddress(g_hGLSetupDriver, "glColor3s");
    if (CVideo3d::glColor3s == NULL) bMissing = TRUE;
    CVideo3d::glColor3sv = (PFNGLCOLOR3SVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3sv");
    if (CVideo3d::glColor3sv == NULL) bMissing = TRUE;
    CVideo3d::glColor3ub = (PFNGLCOLOR3UBPROC)GetProcAddress(g_hGLSetupDriver, "glColor3ub");
    if (CVideo3d::glColor3ub == NULL) bMissing = TRUE;
    CVideo3d::glColor3ubv = (PFNGLCOLOR3UBVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3ubv");
    if (CVideo3d::glColor3ubv == NULL) bMissing = TRUE;
    CVideo3d::glColor3ui = (PFNGLCOLOR3UIPROC)GetProcAddress(g_hGLSetupDriver, "glColor3ui");
    if (CVideo3d::glColor3ui == NULL) bMissing = TRUE;
    CVideo3d::glColor3uiv = (PFNGLCOLOR3UIVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3uiv");
    if (CVideo3d::glColor3uiv == NULL) bMissing = TRUE;
    CVideo3d::glColor3us = (PFNGLCOLOR3USPROC)GetProcAddress(g_hGLSetupDriver, "glColor3us");
    if (CVideo3d::glColor3us == NULL) bMissing = TRUE;
    CVideo3d::glColor3usv = (PFNGLCOLOR3USVPROC)GetProcAddress(g_hGLSetupDriver, "glColor3usv");
    if (CVideo3d::glColor3usv == NULL) bMissing = TRUE;
    CVideo3d::glColor4b = (PFNGLCOLOR4BPROC)GetProcAddress(g_hGLSetupDriver, "glColor4b");
    if (CVideo3d::glColor4b == NULL) bMissing = TRUE;
    CVideo3d::glColor4bv = (PFNGLCOLOR4BVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4bv");
    if (CVideo3d::glColor4bv == NULL) bMissing = TRUE;
    CVideo3d::glColor4d = (PFNGLCOLOR4DPROC)GetProcAddress(g_hGLSetupDriver, "glColor4d");
    if (CVideo3d::glColor4d == NULL) bMissing = TRUE;
    CVideo3d::glColor4dv = (PFNGLCOLOR4DVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4dv");
    if (CVideo3d::glColor4dv == NULL) bMissing = TRUE;
    CVideo3d::glColor4f = (PFNGLCOLOR4FPROC)GetProcAddress(g_hGLSetupDriver, "glColor4f");
    if (CVideo3d::glColor4f == NULL) bMissing = TRUE;
    CVideo3d::glColor4fv = (PFNGLCOLOR4FVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4fv");
    if (CVideo3d::glColor4fv == NULL) bMissing = TRUE;
    CVideo3d::glColor4i = (PFNGLCOLOR4IPROC)GetProcAddress(g_hGLSetupDriver, "glColor4i");
    if (CVideo3d::glColor4i == NULL) bMissing = TRUE;
    CVideo3d::glColor4iv = (PFNGLCOLOR4IVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4iv");
    if (CVideo3d::glColor4iv == NULL) bMissing = TRUE;
    CVideo3d::glColor4s = (PFNGLCOLOR4SPROC)GetProcAddress(g_hGLSetupDriver, "glColor4s");
    if (CVideo3d::glColor4s == NULL) bMissing = TRUE;
    CVideo3d::glColor4sv = (PFNGLCOLOR4SVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4sv");
    if (CVideo3d::glColor4sv == NULL) bMissing = TRUE;
    CVideo3d::glColor4ub = (PFNGLCOLOR4UBPROC)GetProcAddress(g_hGLSetupDriver, "glColor4ub");
    if (CVideo3d::glColor4ub == NULL) bMissing = TRUE;
    CVideo3d::glColor4ubv = (PFNGLCOLOR4UBVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4ubv");
    if (CVideo3d::glColor4ubv == NULL) bMissing = TRUE;
    CVideo3d::glColor4ui = (PFNGLCOLOR4UIPROC)GetProcAddress(g_hGLSetupDriver, "glColor4ui");
    if (CVideo3d::glColor4ui == NULL) bMissing = TRUE;
    CVideo3d::glColor4uiv = (PFNGLCOLOR4UIVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4uiv");
    if (CVideo3d::glColor4uiv == NULL) bMissing = TRUE;
    CVideo3d::glColor4us = (PFNGLCOLOR4USPROC)GetProcAddress(g_hGLSetupDriver, "glColor4us");
    if (CVideo3d::glColor4us == NULL) bMissing = TRUE;
    CVideo3d::glColor4usv = (PFNGLCOLOR4USVPROC)GetProcAddress(g_hGLSetupDriver, "glColor4usv");
    if (CVideo3d::glColor4usv == NULL) bMissing = TRUE;
    CVideo3d::glColorMask = (PFNGLCOLORMASKPROC)GetProcAddress(g_hGLSetupDriver, "glColorMask");
    if (CVideo3d::glColorMask == NULL) bMissing = TRUE;
    CVideo3d::glColorMaterial = (PFNGLCOLORMATERIALPROC)GetProcAddress(g_hGLSetupDriver, "glColorMaterial");
    if (CVideo3d::glColorMaterial == NULL) bMissing = TRUE;
    CVideo3d::glColorPointer = (PFNGLCOLORPOINTERPROC)GetProcAddress(g_hGLSetupDriver, "glColorPointer");
    if (CVideo3d::glColorPointer == NULL) bMissing = TRUE;
    CVideo3d::glCopyPixels = (PFNGLCOPYPIXELSPROC)GetProcAddress(g_hGLSetupDriver, "glCopyPixels");
    if (CVideo3d::glCopyPixels == NULL) bMissing = TRUE;
    CVideo3d::glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC)GetProcAddress(g_hGLSetupDriver, "glCopyTexImage1D");
    if (CVideo3d::glCopyTexImage1D == NULL) bMissing = TRUE;
    CVideo3d::glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)GetProcAddress(g_hGLSetupDriver, "glCopyTexImage2D");
    if (CVideo3d::glCopyTexImage2D == NULL) bMissing = TRUE;
    CVideo3d::glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC)GetProcAddress(g_hGLSetupDriver, "glCopyTexSubImage1D");
    if (CVideo3d::glCopyTexSubImage1D == NULL) bMissing = TRUE;
    CVideo3d::glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)GetProcAddress(g_hGLSetupDriver, "glCopyTexSubImage2D");
    if (CVideo3d::glCopyTexSubImage2D == NULL) bMissing = TRUE;
    CVideo3d::glCullFace = (PFNGLCULLFACEPROC)GetProcAddress(g_hGLSetupDriver, "glCullFace");
    if (CVideo3d::glCullFace == NULL) bMissing = TRUE;
    CVideo3d::glDeleteLists = (PFNGLDELETELISTSPROC)GetProcAddress(g_hGLSetupDriver, "glDeleteLists");
    if (CVideo3d::glDeleteLists == NULL) bMissing = TRUE;
    CVideo3d::glDeleteTextures = (PFNGLDELETETEXTURESPROC)GetProcAddress(g_hGLSetupDriver, "glDeleteTextures");
    if (CVideo3d::glDeleteTextures == NULL) bMissing = TRUE;
    CVideo3d::glDepthFunc = (PFNGLDEPTHFUNCPROC)GetProcAddress(g_hGLSetupDriver, "glDepthFunc");
    if (CVideo3d::glDepthFunc == NULL) bMissing = TRUE;
    CVideo3d::glDepthMask = (PFNGLDEPTHMASKPROC)GetProcAddress(g_hGLSetupDriver, "glDepthMask");
    if (CVideo3d::glDepthMask == NULL) bMissing = TRUE;
    CVideo3d::glDepthRange = (PFNGLDEPTHRANGEPROC)GetProcAddress(g_hGLSetupDriver, "glDepthRange");
    if (CVideo3d::glDepthRange == NULL) bMissing = TRUE;
    CVideo3d::glDisable = (PFNGLDISABLEPROC)GetProcAddress(g_hGLSetupDriver, "glDisable");
    if (CVideo3d::glDisable == NULL) bMissing = TRUE;
    CVideo3d::glDisableClientState = (PFNGLDISABLECLIENTSTATEPROC)GetProcAddress(g_hGLSetupDriver, "glDisableClientState");
    if (CVideo3d::glDisableClientState == NULL) bMissing = TRUE;
    CVideo3d::glDrawArrays = (PFNGLDRAWARRAYSPROC)GetProcAddress(g_hGLSetupDriver, "glDrawArrays");
    if (CVideo3d::glDrawArrays == NULL) bMissing = TRUE;
    CVideo3d::glDrawBuffer = (PFNGLDRAWBUFFERPROC)GetProcAddress(g_hGLSetupDriver, "glDrawBuffer");
    if (CVideo3d::glDrawBuffer == NULL) bMissing = TRUE;
    CVideo3d::glDrawElements = (PFNGLDRAWELEMENTSPROC)GetProcAddress(g_hGLSetupDriver, "glDrawElements");
    if (CVideo3d::glDrawElements == NULL) bMissing = TRUE;
    CVideo3d::glDrawPixels = (PFNGLDRAWPIXELSPROC)GetProcAddress(g_hGLSetupDriver, "glDrawPixels");
    if (CVideo3d::glDrawPixels == NULL) bMissing = TRUE;
    CVideo3d::glEdgeFlag = (PFNGLEDGEFLAGPROC)GetProcAddress(g_hGLSetupDriver, "glEdgeFlag");
    if (CVideo3d::glEdgeFlag == NULL) bMissing = TRUE;
    CVideo3d::glEdgeFlagPointer = (PFNGLEDGEFLAGPOINTERPROC)GetProcAddress(g_hGLSetupDriver, "glEdgeFlagPointer");
    if (CVideo3d::glEdgeFlagPointer == NULL) bMissing = TRUE;
    CVideo3d::glEdgeFlagv = (PFNGLEDGEFLAGVPROC)GetProcAddress(g_hGLSetupDriver, "glEdgeFlagv");
    if (CVideo3d::glEdgeFlagv == NULL) bMissing = TRUE;
    CVideo3d::glEnable = (PFNGLENABLEPROC)GetProcAddress(g_hGLSetupDriver, "glEnable");
    if (CVideo3d::glEnable == NULL) bMissing = TRUE;
    CVideo3d::glEnableClientState = (PFNGLENABLECLIENTSTATEPROC)GetProcAddress(g_hGLSetupDriver, "glEnableClientState");
    if (CVideo3d::glEnableClientState == NULL) bMissing = TRUE;
    CVideo3d::glEnd = (PFNGLENDPROC)GetProcAddress(g_hGLSetupDriver, "glEnd");
    if (CVideo3d::glEnd == NULL) bMissing = TRUE;
    CVideo3d::glEndList = (PFNGLENDLISTPROC)GetProcAddress(g_hGLSetupDriver, "glEndList");
    if (CVideo3d::glEndList == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord1d = (PFNGLEVALCOORD1DPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord1d");
    if (CVideo3d::glEvalCoord1d == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord1dv = (PFNGLEVALCOORD1DVPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord1dv");
    if (CVideo3d::glEvalCoord1dv == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord1f = (PFNGLEVALCOORD1FPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord1f");
    if (CVideo3d::glEvalCoord1f == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord1fv = (PFNGLEVALCOORD1FVPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord1fv");
    if (CVideo3d::glEvalCoord1fv == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord2d = (PFNGLEVALCOORD2DPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord2d");
    if (CVideo3d::glEvalCoord2d == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord2dv = (PFNGLEVALCOORD2DVPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord2dv");
    if (CVideo3d::glEvalCoord2dv == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord2f = (PFNGLEVALCOORD2FPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord2f");
    if (CVideo3d::glEvalCoord2f == NULL) bMissing = TRUE;
    CVideo3d::glEvalCoord2fv = (PFNGLEVALCOORD2FVPROC)GetProcAddress(g_hGLSetupDriver, "glEvalCoord2fv");
    if (CVideo3d::glEvalCoord2fv == NULL) bMissing = TRUE;
    CVideo3d::glEvalMesh1 = (PFNGLEVALMESH1PROC)GetProcAddress(g_hGLSetupDriver, "glEvalMesh1");
    if (CVideo3d::glEvalMesh1 == NULL) bMissing = TRUE;
    CVideo3d::glEvalMesh2 = (PFNGLEVALMESH2PROC)GetProcAddress(g_hGLSetupDriver, "glEvalMesh2");
    if (CVideo3d::glEvalMesh2 == NULL) bMissing = TRUE;
    CVideo3d::glEvalPoint1 = (PFNGLEVALPOINT1PROC)GetProcAddress(g_hGLSetupDriver, "glEvalPoint1");
    if (CVideo3d::glEvalPoint1 == NULL) bMissing = TRUE;
    CVideo3d::glEvalPoint2 = (PFNGLEVALPOINT2PROC)GetProcAddress(g_hGLSetupDriver, "glEvalPoint2");
    if (CVideo3d::glEvalPoint2 == NULL) bMissing = TRUE;
    CVideo3d::glFeedbackBuffer = (PFNGLFEEDBACKBUFFERPROC)GetProcAddress(g_hGLSetupDriver, "glFeedbackBuffer");
    if (CVideo3d::glFeedbackBuffer == NULL) bMissing = TRUE;
    CVideo3d::glFinish = (PFNGLFINISHPROC)GetProcAddress(g_hGLSetupDriver, "glFinish");
    if (CVideo3d::glFinish == NULL) bMissing = TRUE;
    CVideo3d::glFlush = (PFNGLFLUSHPROC)GetProcAddress(g_hGLSetupDriver, "glFlush");
    if (CVideo3d::glFlush == NULL) bMissing = TRUE;
    CVideo3d::glFogf = (PFNGLFOGFPROC)GetProcAddress(g_hGLSetupDriver, "glFogf");
    if (CVideo3d::glFogf == NULL) bMissing = TRUE;
    CVideo3d::glFogfv = (PFNGLFOGFVPROC)GetProcAddress(g_hGLSetupDriver, "glFogfv");
    if (CVideo3d::glFogfv == NULL) bMissing = TRUE;
    CVideo3d::glFogi = (PFNGLFOGIPROC)GetProcAddress(g_hGLSetupDriver, "glFogi");
    if (CVideo3d::glFogi == NULL) bMissing = TRUE;
    CVideo3d::glFogiv = (PFNGLFOGIVPROC)GetProcAddress(g_hGLSetupDriver, "glFogiv");
    if (CVideo3d::glFogiv == NULL) bMissing = TRUE;
    CVideo3d::glFrontFace = (PFNGLFRONTFACEPROC)GetProcAddress(g_hGLSetupDriver, "glFrontFace");
    if (CVideo3d::glFrontFace == NULL) bMissing = TRUE;
    CVideo3d::glFrustum = (PFNGLFRUSTUMPROC)GetProcAddress(g_hGLSetupDriver, "glFrustum");
    if (CVideo3d::glFrustum == NULL) bMissing = TRUE;
    CVideo3d::glGenLists = (PFNGLGENLISTSPROC)GetProcAddress(g_hGLSetupDriver, "glGenLists");
    if (CVideo3d::glGenLists == NULL) bMissing = TRUE;
    CVideo3d::glGenTextures = (PFNGLGENTEXTURESPROC)GetProcAddress(g_hGLSetupDriver, "glGenTextures");
    if (CVideo3d::glGenTextures == NULL) bMissing = TRUE;
    CVideo3d::glGetBooleanv = (PFNGLGETBOOLEANVPROC)GetProcAddress(g_hGLSetupDriver, "glGetBooleanv");
    if (CVideo3d::glGetBooleanv == NULL) bMissing = TRUE;
    CVideo3d::glGetClipPlane = (PFNGLGETCLIPPLANEPROC)GetProcAddress(g_hGLSetupDriver, "glGetClipPlane");
    if (CVideo3d::glGetClipPlane == NULL) bMissing = TRUE;
    CVideo3d::glGetDoublev = (PFNGLGETDOUBLEVPROC)GetProcAddress(g_hGLSetupDriver, "glGetDoublev");
    if (CVideo3d::glGetDoublev == NULL) bMissing = TRUE;
    CVideo3d::glGetError = (PFNGLGETERRORPROC)GetProcAddress(g_hGLSetupDriver, "glGetError");
    if (CVideo3d::glGetError == NULL) bMissing = TRUE;
    CVideo3d::glGetFloatv = (PFNGLGETFLOATVPROC)GetProcAddress(g_hGLSetupDriver, "glGetFloatv");
    if (CVideo3d::glGetFloatv == NULL) bMissing = TRUE;
    CVideo3d::glGetIntegerv = (PFNGLGETINTEGERVPROC)GetProcAddress(g_hGLSetupDriver, "glGetIntegerv");
    if (CVideo3d::glGetIntegerv == NULL) bMissing = TRUE;
    CVideo3d::glGetLightfv = (PFNGLGETLIGHTFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetLightfv");
    if (CVideo3d::glGetLightfv == NULL) bMissing = TRUE;
    CVideo3d::glGetLightiv = (PFNGLGETLIGHTIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetLightiv");
    if (CVideo3d::glGetLightiv == NULL) bMissing = TRUE;
    CVideo3d::glGetMapdv = (PFNGLGETMAPDVPROC)GetProcAddress(g_hGLSetupDriver, "glGetMapdv");
    if (CVideo3d::glGetMapdv == NULL) bMissing = TRUE;
    CVideo3d::glGetMapfv = (PFNGLGETMAPFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetMapfv");
    if (CVideo3d::glGetMapfv == NULL) bMissing = TRUE;
    CVideo3d::glGetMapiv = (PFNGLGETMAPIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetMapiv");
    if (CVideo3d::glGetMapiv == NULL) bMissing = TRUE;
    CVideo3d::glGetMaterialfv = (PFNGLGETMATERIALFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetMaterialfv");
    if (CVideo3d::glGetMaterialfv == NULL) bMissing = TRUE;
    CVideo3d::glGetMaterialiv = (PFNGLGETMATERIALIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetMaterialiv");
    if (CVideo3d::glGetMaterialiv == NULL) bMissing = TRUE;
    CVideo3d::glGetPixelMapfv = (PFNGLGETPIXELMAPFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetPixelMapfv");
    if (CVideo3d::glGetPixelMapfv == NULL) bMissing = TRUE;
    CVideo3d::glGetPixelMapuiv = (PFNGLGETPIXELMAPUIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetPixelMapuiv");
    if (CVideo3d::glGetPixelMapuiv == NULL) bMissing = TRUE;
    CVideo3d::glGetPixelMapusv = (PFNGLGETPIXELMAPUSVPROC)GetProcAddress(g_hGLSetupDriver, "glGetPixelMapusv");
    if (CVideo3d::glGetPixelMapusv == NULL) bMissing = TRUE;
    CVideo3d::glGetPointerv = (PFNGLGETPOINTERVPROC)GetProcAddress(g_hGLSetupDriver, "glGetPointerv");
    if (CVideo3d::glGetPointerv == NULL) bMissing = TRUE;
    CVideo3d::glGetPolygonStipple = (PFNGLGETPOLYGONSTIPPLEPROC)GetProcAddress(g_hGLSetupDriver, "glGetPolygonStipple");
    if (CVideo3d::glGetPolygonStipple == NULL) bMissing = TRUE;
    CVideo3d::glGetString = (PFNGLGETSTRINGPROC)GetProcAddress(g_hGLSetupDriver, "glGetString");
    if (CVideo3d::glGetString == NULL) bMissing = TRUE;
    CVideo3d::glGetTexEnvfv = (PFNGLGETTEXENVFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexEnvfv");
    if (CVideo3d::glGetTexEnvfv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexEnviv = (PFNGLGETTEXENVIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexEnviv");
    if (CVideo3d::glGetTexEnviv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexGendv = (PFNGLGETTEXGENDVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexGendv");
    if (CVideo3d::glGetTexGendv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexGenfv = (PFNGLGETTEXGENFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexGenfv");
    if (CVideo3d::glGetTexGenfv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexGeniv = (PFNGLGETTEXGENIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexGeniv");
    if (CVideo3d::glGetTexGeniv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexImage = (PFNGLGETTEXIMAGEPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexImage");
    if (CVideo3d::glGetTexImage == NULL) bMissing = TRUE;
    CVideo3d::glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexLevelParameterfv");
    if (CVideo3d::glGetTexLevelParameterfv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexLevelParameteriv");
    if (CVideo3d::glGetTexLevelParameteriv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexParameterfv");
    if (CVideo3d::glGetTexParameterfv == NULL) bMissing = TRUE;
    CVideo3d::glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC)GetProcAddress(g_hGLSetupDriver, "glGetTexParameteriv");
    if (CVideo3d::glGetTexParameteriv == NULL) bMissing = TRUE;
    CVideo3d::glHint = (PFNGLHINTPROC)GetProcAddress(g_hGLSetupDriver, "glHint");
    if (CVideo3d::glHint == NULL) bMissing = TRUE;
    CVideo3d::glIndexMask = (PFNGLINDEXMASKPROC)GetProcAddress(g_hGLSetupDriver, "glIndexMask");
    if (CVideo3d::glIndexMask == NULL) bMissing = TRUE;
    CVideo3d::glIndexPointer = (PFNGLINDEXPOINTERPROC)GetProcAddress(g_hGLSetupDriver, "glIndexPointer");
    if (CVideo3d::glIndexPointer == NULL) bMissing = TRUE;
    CVideo3d::glIndexd = (PFNGLINDEXDPROC)GetProcAddress(g_hGLSetupDriver, "glIndexd");
    if (CVideo3d::glIndexd == NULL) bMissing = TRUE;
    CVideo3d::glIndexdv = (PFNGLINDEXDVPROC)GetProcAddress(g_hGLSetupDriver, "glIndexdv");
    if (CVideo3d::glIndexdv == NULL) bMissing = TRUE;
    CVideo3d::glIndexf = (PFNGLINDEXFPROC)GetProcAddress(g_hGLSetupDriver, "glIndexf");
    if (CVideo3d::glIndexf == NULL) bMissing = TRUE;
    CVideo3d::glIndexfv = (PFNGLINDEXFVPROC)GetProcAddress(g_hGLSetupDriver, "glIndexfv");
    if (CVideo3d::glIndexfv == NULL) bMissing = TRUE;
    CVideo3d::glIndexi = (PFNGLINDEXIPROC)GetProcAddress(g_hGLSetupDriver, "glIndexi");
    if (CVideo3d::glIndexi == NULL) bMissing = TRUE;
    CVideo3d::glIndexiv = (PFNGLINDEXIVPROC)GetProcAddress(g_hGLSetupDriver, "glIndexiv");
    if (CVideo3d::glIndexiv == NULL) bMissing = TRUE;
    CVideo3d::glIndexs = (PFNGLINDEXSPROC)GetProcAddress(g_hGLSetupDriver, "glIndexs");
    if (CVideo3d::glIndexs == NULL) bMissing = TRUE;
    CVideo3d::glIndexsv = (PFNGLINDEXSVPROC)GetProcAddress(g_hGLSetupDriver, "glIndexsv");
    if (CVideo3d::glIndexsv == NULL) bMissing = TRUE;
    CVideo3d::glIndexub = (PFNGLINDEXUBPROC)GetProcAddress(g_hGLSetupDriver, "glIndexub");
    if (CVideo3d::glIndexub == NULL) bMissing = TRUE;
    CVideo3d::glIndexubv = (PFNGLINDEXUBVPROC)GetProcAddress(g_hGLSetupDriver, "glIndexubv");
    if (CVideo3d::glIndexubv == NULL) bMissing = TRUE;
    CVideo3d::glInitNames = (PFNGLINITNAMESPROC)GetProcAddress(g_hGLSetupDriver, "glInitNames");
    if (CVideo3d::glInitNames == NULL) bMissing = TRUE;
    CVideo3d::glInterleavedArrays = (PFNGLINTERLEAVEDARRAYSPROC)GetProcAddress(g_hGLSetupDriver, "glInterleavedArrays");
    if (CVideo3d::glInterleavedArrays == NULL) bMissing = TRUE;
    CVideo3d::glIsEnabled = (PFNGLISENABLEDPROC)GetProcAddress(g_hGLSetupDriver, "glIsEnabled");
    if (CVideo3d::glIsEnabled == NULL) bMissing = TRUE;
    CVideo3d::glIsList = (PFNGLISLISTPROC)GetProcAddress(g_hGLSetupDriver, "glIsList");
    if (CVideo3d::glIsList == NULL) bMissing = TRUE;
    CVideo3d::glIsTexture = (PFNGLISTEXTUREPROC)GetProcAddress(g_hGLSetupDriver, "glIsTexture");
    if (CVideo3d::glIsTexture == NULL) bMissing = TRUE;
    CVideo3d::glLightModelf = (PFNGLLIGHTMODELFPROC)GetProcAddress(g_hGLSetupDriver, "glLightModelf");
    if (CVideo3d::glLightModelf == NULL) bMissing = TRUE;
    CVideo3d::glLightModelfv = (PFNGLLIGHTMODELFVPROC)GetProcAddress(g_hGLSetupDriver, "glLightModelfv");
    if (CVideo3d::glLightModelfv == NULL) bMissing = TRUE;
    CVideo3d::glLightModeli = (PFNGLLIGHTMODELIPROC)GetProcAddress(g_hGLSetupDriver, "glLightModeli");
    if (CVideo3d::glLightModeli == NULL) bMissing = TRUE;
    CVideo3d::glLightModeliv = (PFNGLLIGHTMODELIVPROC)GetProcAddress(g_hGLSetupDriver, "glLightModeliv");
    if (CVideo3d::glLightModeliv == NULL) bMissing = TRUE;
    CVideo3d::glLightf = (PFNGLLIGHTFPROC)GetProcAddress(g_hGLSetupDriver, "glLightf");
    if (CVideo3d::glLightf == NULL) bMissing = TRUE;
    CVideo3d::glLightfv = (PFNGLLIGHTFVPROC)GetProcAddress(g_hGLSetupDriver, "glLightfv");
    if (CVideo3d::glLightfv == NULL) bMissing = TRUE;
    CVideo3d::glLighti = (PFNGLLIGHTIPROC)GetProcAddress(g_hGLSetupDriver, "glLighti");
    if (CVideo3d::glLighti == NULL) bMissing = TRUE;
    CVideo3d::glLightiv = (PFNGLLIGHTIVPROC)GetProcAddress(g_hGLSetupDriver, "glLightiv");
    if (CVideo3d::glLightiv == NULL) bMissing = TRUE;
    CVideo3d::glLineStipple = (PFNGLLINESTIPPLEPROC)GetProcAddress(g_hGLSetupDriver, "glLineStipple");
    if (CVideo3d::glLineStipple == NULL) bMissing = TRUE;
    CVideo3d::glLineWidth = (PFNGLLINEWIDTHPROC)GetProcAddress(g_hGLSetupDriver, "glLineWidth");
    if (CVideo3d::glLineWidth == NULL) bMissing = TRUE;
    CVideo3d::glListBase = (PFNGLLISTBASEPROC)GetProcAddress(g_hGLSetupDriver, "glListBase");
    if (CVideo3d::glListBase == NULL) bMissing = TRUE;
    CVideo3d::glLoadIdentity = (PFNGLLOADIDENTITYPROC)GetProcAddress(g_hGLSetupDriver, "glLoadIdentity");
    if (CVideo3d::glLoadIdentity == NULL) bMissing = TRUE;
    CVideo3d::glLoadMatrixd = (PFNGLLOADMATRIXDPROC)GetProcAddress(g_hGLSetupDriver, "glLoadMatrixd");
    if (CVideo3d::glLoadMatrixd == NULL) bMissing = TRUE;
    CVideo3d::glLoadMatrixf = (PFNGLLOADMATRIXFPROC)GetProcAddress(g_hGLSetupDriver, "glLoadMatrixf");
    if (CVideo3d::glLoadMatrixf == NULL) bMissing = TRUE;
    CVideo3d::glLoadName = (PFNGLLOADNAMEPROC)GetProcAddress(g_hGLSetupDriver, "glLoadName");
    if (CVideo3d::glLoadName == NULL) bMissing = TRUE;
    CVideo3d::glLogicOp = (PFNGLLOGICOPPROC)GetProcAddress(g_hGLSetupDriver, "glLogicOp");
    if (CVideo3d::glLogicOp == NULL) bMissing = TRUE;
    CVideo3d::glMap1d = (PFNGLMAP1DPROC)GetProcAddress(g_hGLSetupDriver, "glMap1d");
    if (CVideo3d::glMap1d == NULL) bMissing = TRUE;
    CVideo3d::glMap1f = (PFNGLMAP1FPROC)GetProcAddress(g_hGLSetupDriver, "glMap1f");
    if (CVideo3d::glMap1f == NULL) bMissing = TRUE;
    CVideo3d::glMap2d = (PFNGLMAP2DPROC)GetProcAddress(g_hGLSetupDriver, "glMap2d");
    if (CVideo3d::glMap2d == NULL) bMissing = TRUE;
    CVideo3d::glMap2f = (PFNGLMAP2FPROC)GetProcAddress(g_hGLSetupDriver, "glMap2f");
    if (CVideo3d::glMap2f == NULL) bMissing = TRUE;
    CVideo3d::glMapGrid1d = (PFNGLMAPGRID1DPROC)GetProcAddress(g_hGLSetupDriver, "glMapGrid1d");
    if (CVideo3d::glMapGrid1d == NULL) bMissing = TRUE;
    CVideo3d::glMapGrid1f = (PFNGLMAPGRID1FPROC)GetProcAddress(g_hGLSetupDriver, "glMapGrid1f");
    if (CVideo3d::glMapGrid1f == NULL) bMissing = TRUE;
    CVideo3d::glMapGrid2d = (PFNGLMAPGRID2DPROC)GetProcAddress(g_hGLSetupDriver, "glMapGrid2d");
    if (CVideo3d::glMapGrid2d == NULL) bMissing = TRUE;
    CVideo3d::glMapGrid2f = (PFNGLMAPGRID2FPROC)GetProcAddress(g_hGLSetupDriver, "glMapGrid2f");
    if (CVideo3d::glMapGrid2f == NULL) bMissing = TRUE;
    CVideo3d::glMaterialf = (PFNGLMATERIALFPROC)GetProcAddress(g_hGLSetupDriver, "glMaterialf");
    if (CVideo3d::glMaterialf == NULL) bMissing = TRUE;
    CVideo3d::glMaterialfv = (PFNGLMATERIALFVPROC)GetProcAddress(g_hGLSetupDriver, "glMaterialfv");
    if (CVideo3d::glMaterialfv == NULL) bMissing = TRUE;
    CVideo3d::glMateriali = (PFNGLMATERIALIPROC)GetProcAddress(g_hGLSetupDriver, "glMateriali");
    if (CVideo3d::glMateriali == NULL) bMissing = TRUE;
    CVideo3d::glMaterialiv = (PFNGLMATERIALIVPROC)GetProcAddress(g_hGLSetupDriver, "glMaterialiv");
    if (CVideo3d::glMaterialiv == NULL) bMissing = TRUE;
    CVideo3d::glMatrixMode = (PFNGLMATRIXMODEPROC)GetProcAddress(g_hGLSetupDriver, "glMatrixMode");
    if (CVideo3d::glMatrixMode == NULL) bMissing = TRUE;
    CVideo3d::glMultMatrixd = (PFNGLMULTMATRIXDPROC)GetProcAddress(g_hGLSetupDriver, "glMultMatrixd");
    if (CVideo3d::glMultMatrixd == NULL) bMissing = TRUE;
    CVideo3d::glMultMatrixf = (PFNGLMULTMATRIXFPROC)GetProcAddress(g_hGLSetupDriver, "glMultMatrixf");
    if (CVideo3d::glMultMatrixf == NULL) bMissing = TRUE;
    CVideo3d::glNewList = (PFNGLNEWLISTPROC)GetProcAddress(g_hGLSetupDriver, "glNewList");
    if (CVideo3d::glNewList == NULL) bMissing = TRUE;
    CVideo3d::glNormal3b = (PFNGLNORMAL3BPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3b");
    if (CVideo3d::glNormal3b == NULL) bMissing = TRUE;
    CVideo3d::glNormal3bv = (PFNGLNORMAL3BVPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3bv");
    if (CVideo3d::glNormal3bv == NULL) bMissing = TRUE;
    CVideo3d::glNormal3d = (PFNGLNORMAL3DPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3d");
    if (CVideo3d::glNormal3d == NULL) bMissing = TRUE;
    CVideo3d::glNormal3dv = (PFNGLNORMAL3DVPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3dv");
    if (CVideo3d::glNormal3dv == NULL) bMissing = TRUE;
    CVideo3d::glNormal3f = (PFNGLNORMAL3FPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3f");
    if (CVideo3d::glNormal3f == NULL) bMissing = TRUE;
    CVideo3d::glNormal3fv = (PFNGLNORMAL3FVPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3fv");
    if (CVideo3d::glNormal3fv == NULL) bMissing = TRUE;
    CVideo3d::glNormal3i = (PFNGLNORMAL3IPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3i");
    if (CVideo3d::glNormal3i == NULL) bMissing = TRUE;
    CVideo3d::glNormal3iv = (PFNGLNORMAL3IVPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3iv");
    if (CVideo3d::glNormal3iv == NULL) bMissing = TRUE;
    CVideo3d::glNormal3s = (PFNGLNORMAL3SPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3s");
    if (CVideo3d::glNormal3s == NULL) bMissing = TRUE;
    CVideo3d::glNormal3sv = (PFNGLNORMAL3SVPROC)GetProcAddress(g_hGLSetupDriver, "glNormal3sv");
    if (CVideo3d::glNormal3sv == NULL) bMissing = TRUE;
    CVideo3d::glNormalPointer = (PFNGLNORMALPOINTERPROC)GetProcAddress(g_hGLSetupDriver, "glNormalPointer");
    if (CVideo3d::glNormalPointer == NULL) bMissing = TRUE;
    CVideo3d::glOrtho = (PFNGLORTHOPROC)GetProcAddress(g_hGLSetupDriver, "glOrtho");
    if (CVideo3d::glOrtho == NULL) bMissing = TRUE;
    CVideo3d::glPassThrough = (PFNGLPASSTHROUGHPROC)GetProcAddress(g_hGLSetupDriver, "glPassThrough");
    if (CVideo3d::glPassThrough == NULL) bMissing = TRUE;
    CVideo3d::glPixelMapfv = (PFNGLPIXELMAPFVPROC)GetProcAddress(g_hGLSetupDriver, "glPixelMapfv");
    if (CVideo3d::glPixelMapfv == NULL) bMissing = TRUE;
    CVideo3d::glPixelMapuiv = (PFNGLPIXELMAPUIVPROC)GetProcAddress(g_hGLSetupDriver, "glPixelMapuiv");
    if (CVideo3d::glPixelMapuiv == NULL) bMissing = TRUE;
    CVideo3d::glPixelMapusv = (PFNGLPIXELMAPUSVPROC)GetProcAddress(g_hGLSetupDriver, "glPixelMapusv");
    if (CVideo3d::glPixelMapusv == NULL) bMissing = TRUE;
    CVideo3d::glPixelStoref = (PFNGLPIXELSTOREFPROC)GetProcAddress(g_hGLSetupDriver, "glPixelStoref");
    if (CVideo3d::glPixelStoref == NULL) bMissing = TRUE;
    CVideo3d::glPixelStorei = (PFNGLPIXELSTOREIPROC)GetProcAddress(g_hGLSetupDriver, "glPixelStorei");
    if (CVideo3d::glPixelStorei == NULL) bMissing = TRUE;
    CVideo3d::glPixelTransferf = (PFNGLPIXELTRANSFERFPROC)GetProcAddress(g_hGLSetupDriver, "glPixelTransferf");
    if (CVideo3d::glPixelTransferf == NULL) bMissing = TRUE;
    CVideo3d::glPixelTransferi = (PFNGLPIXELTRANSFERIPROC)GetProcAddress(g_hGLSetupDriver, "glPixelTransferi");
    if (CVideo3d::glPixelTransferi == NULL) bMissing = TRUE;
    CVideo3d::glPixelZoom = (PFNGLPIXELZOOMPROC)GetProcAddress(g_hGLSetupDriver, "glPixelZoom");
    if (CVideo3d::glPixelZoom == NULL) bMissing = TRUE;
    CVideo3d::glPointSize = (PFNGLPOINTSIZEPROC)GetProcAddress(g_hGLSetupDriver, "glPointSize");
    if (CVideo3d::glPointSize == NULL) bMissing = TRUE;
    CVideo3d::glPolygonMode = (PFNGLPOLYGONMODEPROC)GetProcAddress(g_hGLSetupDriver, "glPolygonMode");
    if (CVideo3d::glPolygonMode == NULL) bMissing = TRUE;
    CVideo3d::glPolygonOffset = (PFNGLPOLYGONOFFSETPROC)GetProcAddress(g_hGLSetupDriver, "glPolygonOffset");
    if (CVideo3d::glPolygonOffset == NULL) bMissing = TRUE;
    CVideo3d::glPolygonStipple = (PFNGLPOLYGONSTIPPLEPROC)GetProcAddress(g_hGLSetupDriver, "glPolygonStipple");
    if (CVideo3d::glPolygonStipple == NULL) bMissing = TRUE;
    CVideo3d::glPopAttrib = (PFNGLPOPATTRIBPROC)GetProcAddress(g_hGLSetupDriver, "glPopAttrib");
    if (CVideo3d::glPopAttrib == NULL) bMissing = TRUE;
    CVideo3d::glPopClientAttrib = (PFNGLPOPCLIENTATTRIBPROC)GetProcAddress(g_hGLSetupDriver, "glPopClientAttrib");
    if (CVideo3d::glPopClientAttrib == NULL) bMissing = TRUE;
    CVideo3d::glPopMatrix = (PFNGLPOPMATRIXPROC)GetProcAddress(g_hGLSetupDriver, "glPopMatrix");
    if (CVideo3d::glPopMatrix == NULL) bMissing = TRUE;
    CVideo3d::glPopName = (PFNGLPOPNAMEPROC)GetProcAddress(g_hGLSetupDriver, "glPopName");
    if (CVideo3d::glPopName == NULL) bMissing = TRUE;
    CVideo3d::glPrioritizeTextures = (PFNGLPRIORITIZETEXTURESPROC)GetProcAddress(g_hGLSetupDriver, "glPrioritizeTextures");
    if (CVideo3d::glPrioritizeTextures == NULL) bMissing = TRUE;
    CVideo3d::glPushAttrib = (PFNGLPUSHATTRIBPROC)GetProcAddress(g_hGLSetupDriver, "glPushAttrib");
    if (CVideo3d::glPushAttrib == NULL) bMissing = TRUE;
    CVideo3d::glPushClientAttrib = (PFNGLPUSHCLIENTATTRIBPROC)GetProcAddress(g_hGLSetupDriver, "glPushClientAttrib");
    if (CVideo3d::glPushClientAttrib == NULL) bMissing = TRUE;
    CVideo3d::glPushMatrix = (PFNGLPUSHMATRIXPROC)GetProcAddress(g_hGLSetupDriver, "glPushMatrix");
    if (CVideo3d::glPushMatrix == NULL) bMissing = TRUE;
    CVideo3d::glPushName = (PFNGLPUSHNAMEPROC)GetProcAddress(g_hGLSetupDriver, "glPushName");
    if (CVideo3d::glPushName == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2d = (PFNGLRASTERPOS2DPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2d");
    if (CVideo3d::glRasterPos2d == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2dv = (PFNGLRASTERPOS2DVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2dv");
    if (CVideo3d::glRasterPos2dv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2f = (PFNGLRASTERPOS2FPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2f");
    if (CVideo3d::glRasterPos2f == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2fv = (PFNGLRASTERPOS2FVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2fv");
    if (CVideo3d::glRasterPos2fv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2i = (PFNGLRASTERPOS2IPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2i");
    if (CVideo3d::glRasterPos2i == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2iv = (PFNGLRASTERPOS2IVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2iv");
    if (CVideo3d::glRasterPos2iv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2s = (PFNGLRASTERPOS2SPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2s");
    if (CVideo3d::glRasterPos2s == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos2sv = (PFNGLRASTERPOS2SVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos2sv");
    if (CVideo3d::glRasterPos2sv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3d = (PFNGLRASTERPOS3DPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3d");
    if (CVideo3d::glRasterPos3d == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3dv = (PFNGLRASTERPOS3DVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3dv");
    if (CVideo3d::glRasterPos3dv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3f = (PFNGLRASTERPOS3FPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3f");
    if (CVideo3d::glRasterPos3f == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3fv = (PFNGLRASTERPOS3FVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3fv");
    if (CVideo3d::glRasterPos3fv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3i = (PFNGLRASTERPOS3IPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3i");
    if (CVideo3d::glRasterPos3i == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3iv = (PFNGLRASTERPOS3IVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3iv");
    if (CVideo3d::glRasterPos3iv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3s = (PFNGLRASTERPOS3SPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3s");
    if (CVideo3d::glRasterPos3s == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos3sv = (PFNGLRASTERPOS3SVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos3sv");
    if (CVideo3d::glRasterPos3sv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4d = (PFNGLRASTERPOS4DPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4d");
    if (CVideo3d::glRasterPos4d == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4dv = (PFNGLRASTERPOS4DVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4dv");
    if (CVideo3d::glRasterPos4dv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4f = (PFNGLRASTERPOS4FPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4f");
    if (CVideo3d::glRasterPos4f == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4fv = (PFNGLRASTERPOS4FVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4fv");
    if (CVideo3d::glRasterPos4fv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4i = (PFNGLRASTERPOS4IPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4i");
    if (CVideo3d::glRasterPos4i == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4iv = (PFNGLRASTERPOS4IVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4iv");
    if (CVideo3d::glRasterPos4iv == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4s = (PFNGLRASTERPOS4SPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4s");
    if (CVideo3d::glRasterPos4s == NULL) bMissing = TRUE;
    CVideo3d::glRasterPos4sv = (PFNGLRASTERPOS4SVPROC)GetProcAddress(g_hGLSetupDriver, "glRasterPos4sv");
    if (CVideo3d::glRasterPos4sv == NULL) bMissing = TRUE;
    CVideo3d::glReadBuffer = (PFNGLREADBUFFERPROC)GetProcAddress(g_hGLSetupDriver, "glReadBuffer");
    if (CVideo3d::glReadBuffer == NULL) bMissing = TRUE;
    CVideo3d::glReadPixels = (PFNGLREADPIXELSPROC)GetProcAddress(g_hGLSetupDriver, "glReadPixels");
    if (CVideo3d::glReadPixels == NULL) bMissing = TRUE;
    CVideo3d::glRectd = (PFNGLRECTDPROC)GetProcAddress(g_hGLSetupDriver, "glRectd");
    if (CVideo3d::glRectd == NULL) bMissing = TRUE;
    CVideo3d::glRectdv = (PFNGLRECTDVPROC)GetProcAddress(g_hGLSetupDriver, "glRectdv");
    if (CVideo3d::glRectdv == NULL) bMissing = TRUE;
    CVideo3d::glRectf = (PFNGLRECTFPROC)GetProcAddress(g_hGLSetupDriver, "glRectf");
    if (CVideo3d::glRectf == NULL) bMissing = TRUE;
    CVideo3d::glRectfv = (PFNGLRECTFVPROC)GetProcAddress(g_hGLSetupDriver, "glRectfv");
    if (CVideo3d::glRectfv == NULL) bMissing = TRUE;
    CVideo3d::glRecti = (PFNGLRECTIPROC)GetProcAddress(g_hGLSetupDriver, "glRecti");
    if (CVideo3d::glRecti == NULL) bMissing = TRUE;
    CVideo3d::glRectiv = (PFNGLRECTIVPROC)GetProcAddress(g_hGLSetupDriver, "glRectiv");
    if (CVideo3d::glRectiv == NULL) bMissing = TRUE;
    CVideo3d::glRects = (PFNGLRECTSPROC)GetProcAddress(g_hGLSetupDriver, "glRects");
    if (CVideo3d::glRects == NULL) bMissing = TRUE;
    CVideo3d::glRectsv = (PFNGLRECTSVPROC)GetProcAddress(g_hGLSetupDriver, "glRectsv");
    if (CVideo3d::glRectsv == NULL) bMissing = TRUE;
    CVideo3d::glRenderMode = (PFNGLRENDERMODEPROC)GetProcAddress(g_hGLSetupDriver, "glRenderMode");
    if (CVideo3d::glRenderMode == NULL) bMissing = TRUE;
    CVideo3d::glRotated = (PFNGLROTATEDPROC)GetProcAddress(g_hGLSetupDriver, "glRotated");
    if (CVideo3d::glRotated == NULL) bMissing = TRUE;
    CVideo3d::glRotatef = (PFNGLROTATEFPROC)GetProcAddress(g_hGLSetupDriver, "glRotatef");
    if (CVideo3d::glRotatef == NULL) bMissing = TRUE;
    CVideo3d::glScaled = (PFNGLSCALEDPROC)GetProcAddress(g_hGLSetupDriver, "glScaled");
    if (CVideo3d::glScaled == NULL) bMissing = TRUE;
    CVideo3d::glScalef = (PFNGLSCALEFPROC)GetProcAddress(g_hGLSetupDriver, "glScalef");
    if (CVideo3d::glScalef == NULL) bMissing = TRUE;
    CVideo3d::glScissor = (PFNGLSCISSORPROC)GetProcAddress(g_hGLSetupDriver, "glScissor");
    if (CVideo3d::glScissor == NULL) bMissing = TRUE;
    CVideo3d::glSelectBuffer = (PFNGLSELECTBUFFERPROC)GetProcAddress(g_hGLSetupDriver, "glSelectBuffer");
    if (CVideo3d::glSelectBuffer == NULL) bMissing = TRUE;
    CVideo3d::glShadeModel = (PFNGLSHADEMODELPROC)GetProcAddress(g_hGLSetupDriver, "glShadeModel");
    if (CVideo3d::glShadeModel == NULL) bMissing = TRUE;
    CVideo3d::glStencilFunc = (PFNGLSTENCILFUNCPROC)GetProcAddress(g_hGLSetupDriver, "glStencilFunc");
    if (CVideo3d::glStencilFunc == NULL) bMissing = TRUE;
    CVideo3d::glStencilMask = (PFNGLSTENCILMASKPROC)GetProcAddress(g_hGLSetupDriver, "glStencilMask");
    if (CVideo3d::glStencilMask == NULL) bMissing = TRUE;
    CVideo3d::glStencilOp = (PFNGLSTENCILOPPROC)GetProcAddress(g_hGLSetupDriver, "glStencilOp");
    if (CVideo3d::glStencilOp == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1d = (PFNGLTEXCOORD1DPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1d");
    if (CVideo3d::glTexCoord1d == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1dv = (PFNGLTEXCOORD1DVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1dv");
    if (CVideo3d::glTexCoord1dv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1f = (PFNGLTEXCOORD1FPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1f");
    if (CVideo3d::glTexCoord1f == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1fv = (PFNGLTEXCOORD1FVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1fv");
    if (CVideo3d::glTexCoord1fv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1i = (PFNGLTEXCOORD1IPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1i");
    if (CVideo3d::glTexCoord1i == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1iv = (PFNGLTEXCOORD1IVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1iv");
    if (CVideo3d::glTexCoord1iv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1s = (PFNGLTEXCOORD1SPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1s");
    if (CVideo3d::glTexCoord1s == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord1sv = (PFNGLTEXCOORD1SVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord1sv");
    if (CVideo3d::glTexCoord1sv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2d = (PFNGLTEXCOORD2DPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2d");
    if (CVideo3d::glTexCoord2d == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2dv = (PFNGLTEXCOORD2DVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2dv");
    if (CVideo3d::glTexCoord2dv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2f = (PFNGLTEXCOORD2FPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2f");
    if (CVideo3d::glTexCoord2f == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2fv = (PFNGLTEXCOORD2FVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2fv");
    if (CVideo3d::glTexCoord2fv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2i = (PFNGLTEXCOORD2IPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2i");
    if (CVideo3d::glTexCoord2i == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2iv = (PFNGLTEXCOORD2IVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2iv");
    if (CVideo3d::glTexCoord2iv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2s = (PFNGLTEXCOORD2SPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2s");
    if (CVideo3d::glTexCoord2s == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord2sv = (PFNGLTEXCOORD2SVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord2sv");
    if (CVideo3d::glTexCoord2sv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3d = (PFNGLTEXCOORD3DPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3d");
    if (CVideo3d::glTexCoord3d == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3dv = (PFNGLTEXCOORD3DVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3dv");
    if (CVideo3d::glTexCoord3dv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3f = (PFNGLTEXCOORD3FPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3f");
    if (CVideo3d::glTexCoord3f == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3fv = (PFNGLTEXCOORD3FVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3fv");
    if (CVideo3d::glTexCoord3fv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3i = (PFNGLTEXCOORD3IPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3i");
    if (CVideo3d::glTexCoord3i == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3iv = (PFNGLTEXCOORD3IVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3iv");
    if (CVideo3d::glTexCoord3iv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3s = (PFNGLTEXCOORD3SPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3s");
    if (CVideo3d::glTexCoord3s == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord3sv = (PFNGLTEXCOORD3SVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord3sv");
    if (CVideo3d::glTexCoord3sv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4d = (PFNGLTEXCOORD4DPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4d");
    if (CVideo3d::glTexCoord4d == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4dv = (PFNGLTEXCOORD4DVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4dv");
    if (CVideo3d::glTexCoord4dv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4f = (PFNGLTEXCOORD4FPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4f");
    if (CVideo3d::glTexCoord4f == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4fv = (PFNGLTEXCOORD4FVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4fv");
    if (CVideo3d::glTexCoord4fv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4i = (PFNGLTEXCOORD4IPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4i");
    if (CVideo3d::glTexCoord4i == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4iv = (PFNGLTEXCOORD4IVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4iv");
    if (CVideo3d::glTexCoord4iv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4s = (PFNGLTEXCOORD4SPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4s");
    if (CVideo3d::glTexCoord4s == NULL) bMissing = TRUE;
    CVideo3d::glTexCoord4sv = (PFNGLTEXCOORD4SVPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoord4sv");
    if (CVideo3d::glTexCoord4sv == NULL) bMissing = TRUE;
    CVideo3d::glTexCoordPointer = (PFNGLTEXCOORDPOINTERPROC)GetProcAddress(g_hGLSetupDriver, "glTexCoordPointer");
    if (CVideo3d::glTexCoordPointer == NULL) bMissing = TRUE;
    CVideo3d::glTexEnvf = (PFNGLTEXENVFPROC)GetProcAddress(g_hGLSetupDriver, "glTexEnvf");
    if (CVideo3d::glTexEnvf == NULL) bMissing = TRUE;
    CVideo3d::glTexEnvfv = (PFNGLTEXENVFVPROC)GetProcAddress(g_hGLSetupDriver, "glTexEnvfv");
    if (CVideo3d::glTexEnvfv == NULL) bMissing = TRUE;
    CVideo3d::glTexEnvi = (PFNGLTEXENVIPROC)GetProcAddress(g_hGLSetupDriver, "glTexEnvi");
    if (CVideo3d::glTexEnvi == NULL) bMissing = TRUE;
    CVideo3d::glTexEnviv = (PFNGLTEXENVIVPROC)GetProcAddress(g_hGLSetupDriver, "glTexEnviv");
    if (CVideo3d::glTexEnviv == NULL) bMissing = TRUE;
    CVideo3d::glTexGend = (PFNGLTEXGENDPROC)GetProcAddress(g_hGLSetupDriver, "glTexGend");
    if (CVideo3d::glTexGend == NULL) bMissing = TRUE;
    CVideo3d::glTexGendv = (PFNGLTEXGENDVPROC)GetProcAddress(g_hGLSetupDriver, "glTexGendv");
    if (CVideo3d::glTexGendv == NULL) bMissing = TRUE;
    CVideo3d::glTexGenf = (PFNGLTEXGENFPROC)GetProcAddress(g_hGLSetupDriver, "glTexGenf");
    if (CVideo3d::glTexGenf == NULL) bMissing = TRUE;
    CVideo3d::glTexGenfv = (PFNGLTEXGENFVPROC)GetProcAddress(g_hGLSetupDriver, "glTexGenfv");
    if (CVideo3d::glTexGenfv == NULL) bMissing = TRUE;
    CVideo3d::glTexGeni = (PFNGLTEXGENIPROC)GetProcAddress(g_hGLSetupDriver, "glTexGeni");
    if (CVideo3d::glTexGeni == NULL) bMissing = TRUE;
    CVideo3d::glTexGeniv = (PFNGLTEXGENIVPROC)GetProcAddress(g_hGLSetupDriver, "glTexGeniv");
    if (CVideo3d::glTexGeniv == NULL) bMissing = TRUE;
    CVideo3d::glTexImage1D = (PFNGLTEXIMAGE1DPROC)GetProcAddress(g_hGLSetupDriver, "glTexImage1D");
    if (CVideo3d::glTexImage1D == NULL) bMissing = TRUE;
    CVideo3d::glTexImage2D = (PFNGLTEXIMAGE2DPROC)GetProcAddress(g_hGLSetupDriver, "glTexImage2D");
    if (CVideo3d::glTexImage2D == NULL) bMissing = TRUE;
    CVideo3d::glTexParameterf = (PFNGLTEXPARAMETERFPROC)GetProcAddress(g_hGLSetupDriver, "glTexParameterf");
    if (CVideo3d::glTexParameterf == NULL) bMissing = TRUE;
    CVideo3d::glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)GetProcAddress(g_hGLSetupDriver, "glTexParameterfv");
    if (CVideo3d::glTexParameterfv == NULL) bMissing = TRUE;
    CVideo3d::glTexParameteri = (PFNGLTEXPARAMETERIPROC)GetProcAddress(g_hGLSetupDriver, "glTexParameteri");
    if (CVideo3d::glTexParameteri == NULL) bMissing = TRUE;
    CVideo3d::glTexParameteriv = (PFNGLTEXPARAMETERIVPROC)GetProcAddress(g_hGLSetupDriver, "glTexParameteriv");
    if (CVideo3d::glTexParameteriv == NULL) bMissing = TRUE;
    CVideo3d::glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC)GetProcAddress(g_hGLSetupDriver, "glTexSubImage1D");
    if (CVideo3d::glTexSubImage1D == NULL) bMissing = TRUE;
    CVideo3d::glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)GetProcAddress(g_hGLSetupDriver, "glTexSubImage2D");
    if (CVideo3d::glTexSubImage2D == NULL) bMissing = TRUE;
    CVideo3d::glTranslated = (PFNGLTRANSLATEDPROC)GetProcAddress(g_hGLSetupDriver, "glTranslated");
    if (CVideo3d::glTranslated == NULL) bMissing = TRUE;
    CVideo3d::glTranslatef = (PFNGLTRANSLATEFPROC)GetProcAddress(g_hGLSetupDriver, "glTranslatef");
    if (CVideo3d::glTranslatef == NULL) bMissing = TRUE;
    CVideo3d::glVertex2d = (PFNGLVERTEX2DPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2d");
    if (CVideo3d::glVertex2d == NULL) bMissing = TRUE;
    CVideo3d::glVertex2dv = (PFNGLVERTEX2DVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2dv");
    if (CVideo3d::glVertex2dv == NULL) bMissing = TRUE;
    CVideo3d::glVertex2f = (PFNGLVERTEX2FPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2f");
    if (CVideo3d::glVertex2f == NULL) bMissing = TRUE;
    CVideo3d::glVertex2fv = (PFNGLVERTEX2FVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2fv");
    if (CVideo3d::glVertex2fv == NULL) bMissing = TRUE;
    CVideo3d::glVertex2i = (PFNGLVERTEX2IPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2i");
    if (CVideo3d::glVertex2i == NULL) bMissing = TRUE;
    CVideo3d::glVertex2iv = (PFNGLVERTEX2IVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2iv");
    if (CVideo3d::glVertex2iv == NULL) bMissing = TRUE;
    CVideo3d::glVertex2s = (PFNGLVERTEX2SPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2s");
    if (CVideo3d::glVertex2s == NULL) bMissing = TRUE;
    CVideo3d::glVertex2sv = (PFNGLVERTEX2SVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex2sv");
    if (CVideo3d::glVertex2sv == NULL) bMissing = TRUE;
    CVideo3d::glVertex3d = (PFNGLVERTEX3DPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3d");
    if (CVideo3d::glVertex3d == NULL) bMissing = TRUE;
    CVideo3d::glVertex3dv = (PFNGLVERTEX3DVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3dv");
    if (CVideo3d::glVertex3dv == NULL) bMissing = TRUE;
    CVideo3d::glVertex3f = (PFNGLVERTEX3FPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3f");
    if (CVideo3d::glVertex3f == NULL) bMissing = TRUE;
    CVideo3d::glVertex3fv = (PFNGLVERTEX3FVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3fv");
    if (CVideo3d::glVertex3fv == NULL) bMissing = TRUE;
    CVideo3d::glVertex3i = (PFNGLVERTEX3IPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3i");
    if (CVideo3d::glVertex3i == NULL) bMissing = TRUE;
    CVideo3d::glVertex3iv = (PFNGLVERTEX3IVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3iv");
    if (CVideo3d::glVertex3iv == NULL) bMissing = TRUE;
    CVideo3d::glVertex3s = (PFNGLVERTEX3SPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3s");
    if (CVideo3d::glVertex3s == NULL) bMissing = TRUE;
    CVideo3d::glVertex3sv = (PFNGLVERTEX3SVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex3sv");
    if (CVideo3d::glVertex3sv == NULL) bMissing = TRUE;
    CVideo3d::glVertex4d = (PFNGLVERTEX4DPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4d");
    if (CVideo3d::glVertex4d == NULL) bMissing = TRUE;
    CVideo3d::glVertex4dv = (PFNGLVERTEX4DVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4dv");
    if (CVideo3d::glVertex4dv == NULL) bMissing = TRUE;
    CVideo3d::glVertex4f = (PFNGLVERTEX4FPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4f");
    if (CVideo3d::glVertex4f == NULL) bMissing = TRUE;
    CVideo3d::glVertex4fv = (PFNGLVERTEX4FVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4fv");
    if (CVideo3d::glVertex4fv == NULL) bMissing = TRUE;
    CVideo3d::glVertex4i = (PFNGLVERTEX4IPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4i");
    if (CVideo3d::glVertex4i == NULL) bMissing = TRUE;
    CVideo3d::glVertex4iv = (PFNGLVERTEX4IVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4iv");
    if (CVideo3d::glVertex4iv == NULL) bMissing = TRUE;
    CVideo3d::glVertex4s = (PFNGLVERTEX4SPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4s");
    if (CVideo3d::glVertex4s == NULL) bMissing = TRUE;
    CVideo3d::glVertex4sv = (PFNGLVERTEX4SVPROC)GetProcAddress(g_hGLSetupDriver, "glVertex4sv");
    if (CVideo3d::glVertex4sv == NULL) bMissing = TRUE;
    CVideo3d::glVertexPointer = (PFNGLVERTEXPOINTERPROC)GetProcAddress(g_hGLSetupDriver, "glVertexPointer");
    if (CVideo3d::glVertexPointer == NULL) bMissing = TRUE;
    CVideo3d::glViewport = (PFNGLVIEWPORTPROC)GetProcAddress(g_hGLSetupDriver, "glViewport");
    if (CVideo3d::glViewport == NULL) bMissing = TRUE;

    // ---- WGL context / layer entry points (resolved, not required). ----
    g_pfnGLSetupWglCopyContext            = (PFNWGLCOPYCONTEXTPROC)GetProcAddress(g_hGLSetupDriver, "wglCopyContext");
    g_pfnGLSetupWglCreateContext          = (PFNWGLCREATECONTEXTPROC)GetProcAddress(g_hGLSetupDriver, "wglCreateContext");
    g_pfnGLSetupWglCreateLayerContext     = (PFNWGLCREATELAYERCONTEXTPROC)GetProcAddress(g_hGLSetupDriver, "wglCreateLayerContext");
    g_pfnGLSetupWglDeleteContext          = (PFNWGLDELETECONTEXTPROC)GetProcAddress(g_hGLSetupDriver, "wglDeleteContext");
    g_pfnGLSetupWglDescribeLayerPlane     = (PFNWGLDESCRIBELAYERPLANEPROC)GetProcAddress(g_hGLSetupDriver, "wglDescribeLayerPlane");
    g_pfnGLSetupWglGetCurrentContext      = (PFNWGLGETCURRENTCONTEXTPROC)GetProcAddress(g_hGLSetupDriver, "wglGetCurrentContext");
    g_pfnGLSetupWglGetCurrentDC           = (PFNWGLGETCURRENTDCPROC)GetProcAddress(g_hGLSetupDriver, "wglGetCurrentDC");
    g_pfnGLSetupWglGetLayerPaletteEntries = (PFNWGLGETLAYERPALETTEENTRIESPROC)GetProcAddress(g_hGLSetupDriver, "wglGetLayerPaletteEntries");
    g_pfnGLSetupWglGetProcAddress         = (PFNWGLGETPROCADDRESSPROC)GetProcAddress(g_hGLSetupDriver, "wglGetProcAddress");
    g_pfnGLSetupWglMakeCurrent            = (PFNWGLMAKECURRENTPROC)GetProcAddress(g_hGLSetupDriver, "wglMakeCurrent");
    g_pfnGLSetupWglRealizeLayerPalette    = (PFNWGLREALIZELAYERPALETTEPROC)GetProcAddress(g_hGLSetupDriver, "wglRealizeLayerPalette");
    g_pfnGLSetupWglSetLayerPaletteEntries = (PFNWGLSETLAYERPALETTEENTRIESPROC)GetProcAddress(g_hGLSetupDriver, "wglSetLayerPaletteEntries");
    g_pfnGLSetupWglShareLists             = (PFNWGLSHARELISTSPROC)GetProcAddress(g_hGLSetupDriver, "wglShareLists");
    g_pfnGLSetupWglSwapLayerBuffers       = (PFNWGLSWAPLAYERBUFFERSPROC)GetProcAddress(g_hGLSetupDriver, "wglSwapLayerBuffers");
    g_pfnGLSetupWglUseFontBitmapsA        = (PFNWGLUSEFONTBITMAPSAPROC)GetProcAddress(g_hGLSetupDriver, "wglUseFontBitmapsA");
    g_pfnGLSetupWglUseFontOutlinesA       = (PFNWGLUSEFONTOUTLINESAPROC)GetProcAddress(g_hGLSetupDriver, "wglUseFontOutlinesA");

    // Pixel-format entry points: build "<sysdir>\opengl32.dll" and compare with
    // the driver path. If they differ the driver is a real ICD -> take its own
    // wgl* pixel-format exports; otherwise fall back to gdi32.dll.
    GetSystemDirectoryA(szScratch, 0x104);
    strcat(szScratch, "\\opengl32.dll");
    {
        LPSTR pFilePart = NULL;
        DWORD dwLen;
        GetFullPathNameA(szScratch, 0x104, szNormA, &pFilePart);
        dwLen = GetFullPathNameA(pszPrimaryPath, 0x104, szNormB, &pFilePart);
        if (dwLen == 0 || GLSetupStrICmp((unsigned char*)szNormB, (unsigned char*)szNormA) != 0) {
            g_pfnGLSetupChoosePixelFormat   = (PFNCHOOSEPIXELFORMATPROC)GetProcAddress(g_hGLSetupDriver, "wglChoosePixelFormat");
            if (g_pfnGLSetupChoosePixelFormat == NULL) bMissing = TRUE;
            g_pfnGLSetupDescribePixelFormat = (PFNDESCRIBEPIXELFORMATPROC)GetProcAddress(g_hGLSetupDriver, "wglDescribePixelFormat");
            if (g_pfnGLSetupDescribePixelFormat == NULL) bMissing = TRUE;
            g_pfnGLSetupGetPixelFormat      = (PFNGETPIXELFORMATPROC)GetProcAddress(g_hGLSetupDriver, "wglGetPixelFormat");
            if (g_pfnGLSetupGetPixelFormat == NULL) bMissing = TRUE;
            g_pfnGLSetupSetPixelFormat      = (PFNSETPIXELFORMATPROC)GetProcAddress(g_hGLSetupDriver, "wglSetPixelFormat");
            if (g_pfnGLSetupSetPixelFormat == NULL) bMissing = TRUE;
            g_pfnGLSetupSwapBuffers         = (PFNSWAPBUFFERSPROC)GetProcAddress(g_hGLSetupDriver, "wglSwapBuffers");
            if (g_pfnGLSetupSwapBuffers == NULL) bMissing = TRUE;
        } else {
            g_hGLSetupGdi32 = LoadLibraryA("gdi32.dll");
            if (g_hGLSetupGdi32 != NULL) {
                g_pfnGLSetupChoosePixelFormat   = (PFNCHOOSEPIXELFORMATPROC)GetProcAddress(g_hGLSetupGdi32, "ChoosePixelFormat");
                if (g_pfnGLSetupChoosePixelFormat == NULL) bMissing = TRUE;
                g_pfnGLSetupDescribePixelFormat = (PFNDESCRIBEPIXELFORMATPROC)GetProcAddress(g_hGLSetupGdi32, "DescribePixelFormat");
                if (g_pfnGLSetupDescribePixelFormat == NULL) bMissing = TRUE;
                g_pfnGLSetupGetPixelFormat      = (PFNGETPIXELFORMATPROC)GetProcAddress(g_hGLSetupGdi32, "GetPixelFormat");
                if (g_pfnGLSetupGetPixelFormat == NULL) bMissing = TRUE;
                g_pfnGLSetupSetPixelFormat      = (PFNSETPIXELFORMATPROC)GetProcAddress(g_hGLSetupGdi32, "SetPixelFormat");
                if (g_pfnGLSetupSetPixelFormat == NULL) bMissing = TRUE;
                g_pfnGLSetupSwapBuffers         = (PFNSWAPBUFFERSPROC)GetProcAddress(g_hGLSetupGdi32, "SwapBuffers");
                if (g_pfnGLSetupSwapBuffers == NULL) bMissing = TRUE;
            } else {
                bMissing = TRUE;
            }
        }
    }

    // Commit the primary driver path.
    strncpy(g_szGLSetupPrimaryPath, pszPrimaryPath, 0x104);

    // ---- GLU entry points from the mini driver (unless control bit 0). ----
    if ((g_dwGLSetupControl & 1) == 0 && pszMiniPath != NULL && *pszMiniPath != '\0') {
        g_hGLSetupMini = LoadLibraryA(pszMiniPath);
        if (g_hGLSetupMini == NULL) {
            goto reject;
        }
        CVideo3d::gluBeginCurve = (PFNGLUBEGINCURVEPROC)GetProcAddress(g_hGLSetupMini, "gluBeginCurve");
        if (CVideo3d::gluBeginCurve == NULL) bMissing = TRUE;
        CVideo3d::gluBeginPolygon = (PFNGLUBEGINPOLYGONPROC)GetProcAddress(g_hGLSetupMini, "gluBeginPolygon");
        if (CVideo3d::gluBeginPolygon == NULL) bMissing = TRUE;
        CVideo3d::gluBeginSurface = (PFNGLUBEGINSURFACEPROC)GetProcAddress(g_hGLSetupMini, "gluBeginSurface");
        if (CVideo3d::gluBeginSurface == NULL) bMissing = TRUE;
        CVideo3d::gluBeginTrim = (PFNGLUBEGINTRIMPROC)GetProcAddress(g_hGLSetupMini, "gluBeginTrim");
        if (CVideo3d::gluBeginTrim == NULL) bMissing = TRUE;
        CVideo3d::gluBuild1DMipmaps = (PFNGLUBUILD1DMIPMAPSPROC)GetProcAddress(g_hGLSetupMini, "gluBuild1DMipmaps");
        if (CVideo3d::gluBuild1DMipmaps == NULL) bMissing = TRUE;
        CVideo3d::gluBuild2DMipmaps = (PFNGLUBUILD2DMIPMAPSPROC)GetProcAddress(g_hGLSetupMini, "gluBuild2DMipmaps");
        if (CVideo3d::gluBuild2DMipmaps == NULL) bMissing = TRUE;
        CVideo3d::gluCylinder = (PFNGLUCYLINDERPROC)GetProcAddress(g_hGLSetupMini, "gluCylinder");
        if (CVideo3d::gluCylinder == NULL) bMissing = TRUE;
        CVideo3d::gluDeleteNurbsRenderer = (PFNGLUDELETENURBSRENDERERPROC)GetProcAddress(g_hGLSetupMini, "gluDeleteNurbsRenderer");
        if (CVideo3d::gluDeleteNurbsRenderer == NULL) bMissing = TRUE;
        CVideo3d::gluDeleteQuadric = (PFNGLUDELETEQUADRICPROC)GetProcAddress(g_hGLSetupMini, "gluDeleteQuadric");
        if (CVideo3d::gluDeleteQuadric == NULL) bMissing = TRUE;
        CVideo3d::gluDeleteTess = (PFNGLUDELETETESSPROC)GetProcAddress(g_hGLSetupMini, "gluDeleteTess");
        if (CVideo3d::gluDeleteTess == NULL) bMissing = TRUE;
        CVideo3d::gluDisk = (PFNGLUDISKPROC)GetProcAddress(g_hGLSetupMini, "gluDisk");
        if (CVideo3d::gluDisk == NULL) bMissing = TRUE;
        CVideo3d::gluEndCurve = (PFNGLUENDCURVEPROC)GetProcAddress(g_hGLSetupMini, "gluEndCurve");
        if (CVideo3d::gluEndCurve == NULL) bMissing = TRUE;
        CVideo3d::gluEndPolygon = (PFNGLUENDPOLYGONPROC)GetProcAddress(g_hGLSetupMini, "gluEndPolygon");
        if (CVideo3d::gluEndPolygon == NULL) bMissing = TRUE;
        CVideo3d::gluEndSurface = (PFNGLUENDSURFACEPROC)GetProcAddress(g_hGLSetupMini, "gluEndSurface");
        if (CVideo3d::gluEndSurface == NULL) bMissing = TRUE;
        CVideo3d::gluEndTrim = (PFNGLUENDTRIMPROC)GetProcAddress(g_hGLSetupMini, "gluEndTrim");
        if (CVideo3d::gluEndTrim == NULL) bMissing = TRUE;
        CVideo3d::gluErrorString = (PFNGLUERRORSTRINGPROC)GetProcAddress(g_hGLSetupMini, "gluErrorString");
        if (CVideo3d::gluErrorString == NULL) bMissing = TRUE;
        CVideo3d::gluGetNurbsProperty = (PFNGLUGETNURBSPROPERTYPROC)GetProcAddress(g_hGLSetupMini, "gluGetNurbsProperty");
        if (CVideo3d::gluGetNurbsProperty == NULL) bMissing = TRUE;
        CVideo3d::gluGetString = (PFNGLUGETSTRINGPROC)GetProcAddress(g_hGLSetupMini, "gluGetString");
        if (CVideo3d::gluGetString == NULL) bMissing = TRUE;
        CVideo3d::gluGetTessProperty = (PFNGLUGETTESSPROPERTYPROC)GetProcAddress(g_hGLSetupMini, "gluGetTessProperty");
        if (CVideo3d::gluGetTessProperty == NULL) bMissing = TRUE;
        CVideo3d::gluLoadSamplingMatrices = (PFNGLULOADSAMPLINGMATRICESPROC)GetProcAddress(g_hGLSetupMini, "gluLoadSamplingMatrices");
        if (CVideo3d::gluLoadSamplingMatrices == NULL) bMissing = TRUE;
        CVideo3d::gluLookAt = (PFNGLULOOKATPROC)GetProcAddress(g_hGLSetupMini, "gluLookAt");
        if (CVideo3d::gluLookAt == NULL) bMissing = TRUE;
        CVideo3d::gluNewNurbsRenderer = (PFNGLUNEWNURBSRENDERERPROC)GetProcAddress(g_hGLSetupMini, "gluNewNurbsRenderer");
        if (CVideo3d::gluNewNurbsRenderer == NULL) bMissing = TRUE;
        CVideo3d::gluNewQuadric = (PFNGLUNEWQUADRICPROC)GetProcAddress(g_hGLSetupMini, "gluNewQuadric");
        if (CVideo3d::gluNewQuadric == NULL) bMissing = TRUE;
        CVideo3d::gluNewTess = (PFNGLUNEWTESSPROC)GetProcAddress(g_hGLSetupMini, "gluNewTess");
        if (CVideo3d::gluNewTess == NULL) bMissing = TRUE;
        CVideo3d::gluNextContour = (PFNGLUNEXTCONTOURPROC)GetProcAddress(g_hGLSetupMini, "gluNextContour");
        if (CVideo3d::gluNextContour == NULL) bMissing = TRUE;
        CVideo3d::gluNurbsCallback = (PFNGLUNURBSCALLBACKPROC)GetProcAddress(g_hGLSetupMini, "gluNurbsCallback");
        if (CVideo3d::gluNurbsCallback == NULL) bMissing = TRUE;
        CVideo3d::gluNurbsCurve = (PFNGLUNURBSCURVEPROC)GetProcAddress(g_hGLSetupMini, "gluNurbsCurve");
        if (CVideo3d::gluNurbsCurve == NULL) bMissing = TRUE;
        CVideo3d::gluNurbsProperty = (PFNGLUNURBSPROPERTYPROC)GetProcAddress(g_hGLSetupMini, "gluNurbsProperty");
        if (CVideo3d::gluNurbsProperty == NULL) bMissing = TRUE;
        CVideo3d::gluNurbsSurface = (PFNGLUNURBSSURFACEPROC)GetProcAddress(g_hGLSetupMini, "gluNurbsSurface");
        if (CVideo3d::gluNurbsSurface == NULL) bMissing = TRUE;
        CVideo3d::gluOrtho2D = (PFNGLUORTHO2DPROC)GetProcAddress(g_hGLSetupMini, "gluOrtho2D");
        if (CVideo3d::gluOrtho2D == NULL) bMissing = TRUE;
        CVideo3d::gluPartialDisk = (PFNGLUPARTIALDISKPROC)GetProcAddress(g_hGLSetupMini, "gluPartialDisk");
        if (CVideo3d::gluPartialDisk == NULL) bMissing = TRUE;
        CVideo3d::gluPerspective = (PFNGLUPERSPECTIVEPROC)GetProcAddress(g_hGLSetupMini, "gluPerspective");
        if (CVideo3d::gluPerspective == NULL) bMissing = TRUE;
        CVideo3d::gluPickMatrix = (PFNGLUPICKMATRIXPROC)GetProcAddress(g_hGLSetupMini, "gluPickMatrix");
        if (CVideo3d::gluPickMatrix == NULL) bMissing = TRUE;
        CVideo3d::gluProject = (PFNGLUPROJECTPROC)GetProcAddress(g_hGLSetupMini, "gluProject");
        if (CVideo3d::gluProject == NULL) bMissing = TRUE;
        CVideo3d::gluPwlCurve = (PFNGLUPWLCURVEPROC)GetProcAddress(g_hGLSetupMini, "gluPwlCurve");
        if (CVideo3d::gluPwlCurve == NULL) bMissing = TRUE;
        CVideo3d::gluQuadricCallback = (PFNGLUQUADRICCALLBACKPROC)GetProcAddress(g_hGLSetupMini, "gluQuadricCallback");
        if (CVideo3d::gluQuadricCallback == NULL) bMissing = TRUE;
        CVideo3d::gluQuadricDrawStyle = (PFNGLUQUADRICDRAWSTYLEPROC)GetProcAddress(g_hGLSetupMini, "gluQuadricDrawStyle");
        if (CVideo3d::gluQuadricDrawStyle == NULL) bMissing = TRUE;
        CVideo3d::gluQuadricNormals = (PFNGLUQUADRICNORMALSPROC)GetProcAddress(g_hGLSetupMini, "gluQuadricNormals");
        if (CVideo3d::gluQuadricNormals == NULL) bMissing = TRUE;
        CVideo3d::gluQuadricOrientation = (PFNGLUQUADRICORIENTATIONPROC)GetProcAddress(g_hGLSetupMini, "gluQuadricOrientation");
        if (CVideo3d::gluQuadricOrientation == NULL) bMissing = TRUE;
        CVideo3d::gluQuadricTexture = (PFNGLUQUADRICTEXTUREPROC)GetProcAddress(g_hGLSetupMini, "gluQuadricTexture");
        if (CVideo3d::gluQuadricTexture == NULL) bMissing = TRUE;
        CVideo3d::gluScaleImage = (PFNGLUSCALEIMAGEPROC)GetProcAddress(g_hGLSetupMini, "gluScaleImage");
        if (CVideo3d::gluScaleImage == NULL) bMissing = TRUE;
        CVideo3d::gluSphere = (PFNGLUSPHEREPROC)GetProcAddress(g_hGLSetupMini, "gluSphere");
        if (CVideo3d::gluSphere == NULL) bMissing = TRUE;
        CVideo3d::gluTessBeginContour = (PFNGLUTESSBEGINCONTOURPROC)GetProcAddress(g_hGLSetupMini, "gluTessBeginContour");
        if (CVideo3d::gluTessBeginContour == NULL) bMissing = TRUE;
        CVideo3d::gluTessBeginPolygon = (PFNGLUTESSBEGINPOLYGONPROC)GetProcAddress(g_hGLSetupMini, "gluTessBeginPolygon");
        if (CVideo3d::gluTessBeginPolygon == NULL) bMissing = TRUE;
        CVideo3d::gluTessCallback = (PFNGLUTESSCALLBACKPROC)GetProcAddress(g_hGLSetupMini, "gluTessCallback");
        if (CVideo3d::gluTessCallback == NULL) bMissing = TRUE;
        CVideo3d::gluTessEndContour = (PFNGLUTESSENDCONTOURPROC)GetProcAddress(g_hGLSetupMini, "gluTessEndContour");
        if (CVideo3d::gluTessEndContour == NULL) bMissing = TRUE;
        CVideo3d::gluTessEndPolygon = (PFNGLUTESSENDPOLYGONPROC)GetProcAddress(g_hGLSetupMini, "gluTessEndPolygon");
        if (CVideo3d::gluTessEndPolygon == NULL) bMissing = TRUE;
        CVideo3d::gluTessNormal = (PFNGLUTESSNORMALPROC)GetProcAddress(g_hGLSetupMini, "gluTessNormal");
        if (CVideo3d::gluTessNormal == NULL) bMissing = TRUE;
        CVideo3d::gluTessProperty = (PFNGLUTESSPROPERTYPROC)GetProcAddress(g_hGLSetupMini, "gluTessProperty");
        if (CVideo3d::gluTessProperty == NULL) bMissing = TRUE;
        CVideo3d::gluTessVertex = (PFNGLUTESSVERTEXPROC)GetProcAddress(g_hGLSetupMini, "gluTessVertex");
        if (CVideo3d::gluTessVertex == NULL) bMissing = TRUE;
        CVideo3d::gluUnProject = (PFNGLUUNPROJECTPROC)GetProcAddress(g_hGLSetupMini, "gluUnProject");
        if (CVideo3d::gluUnProject == NULL) bMissing = TRUE;
        strncpy(g_szGLSetupMiniPath, pszMiniPath, 0x104);
    } else {
        g_szGLSetupMiniPath[0] = '\0';
    }

    if (!bMissing) {
        return 0;
    }
reject:
    GLSetupResetDriver();
    return 2;
}
// Tears the probed driver back down: releases the driver / mini / gdi32 module
// handles, restores the FX_GLIDE_NO_SPLASH env var and the SGI OpenGL
// "OverrideDispatch" registry value to the values cached before the probe, then
// resets the whole OpenGL / WGL / GLU / pixel-format entry-point table and the
// selected-driver state. Called on probe entry and again when a driver is
// rejected.
// 0x7BAAC0
static void GLSetupResetDriver(void)
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
            GLSetupPutEnv(szEnv);
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
// Reads pszFilePath's version resource: stores dwFileVersionMS/LS into the
// driver record (+0x8 / +0xC), and when pszIdOut is given, formats a
// "Company - Description" identification string from the version resource.
// Returns TRUE when a VS_FIXEDFILEINFO block was found.
// 0x7BB5D0
static int GLSetupGetDriverVersion(LPCSTR pszFilePath, void* pDriverInfo, char* pszIdOut, unsigned int cbIdOut)
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

// EnumDrivers / SelectDriver are not part of this batch, so nothing calls the
// probe yet; keep the definitions referenced.
[[maybe_unused]] static auto const g_pfnGLSetupKeepAliveProbe = &GLSetupProbeDriver;
[[maybe_unused]] static auto const g_pfnGLSetupKeepAliveValidate = &GLSetupValidateDriver;
