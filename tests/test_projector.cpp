// The projector's subtitle overlay teardown (CProjectorSubtitles::Unload,
// 0x43F900) - the block CBaldurProjector::~CBaldurProjector (0x43EA70) runs
// inline and EngineDeactivated (0x43EED0) calls: delete the line array, release
// the surface at pVidMode->pSurfaces[8] through vtable+8 and null the slot, then
// destroy the text. The callers ::operator delete the block and null the global
// afterwards. The type and the global are #guess declarations (see
// CBaldurProjector.h): neither the exe's names nor IEex's cover them, the
// layout and the teardown are the disassembly's.
#include <catch2/catch_test_macros.hpp>

#include "CBaldurProjector.h"
#include "CChitin.h"
#include "CVidInf.h"
#include "CWarp.h"
#include "platform/ddraw_shim.h"

#include <cstring>
#include <new>
#include <vector>

namespace {

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }

    // The teardown under test never asserts; swallow a shut-down request so a
    // regression reports as a failed assertion, not a terminated process.
    void ShutDown(int, const char*, const char*) override { }
};

// A surface that counts its releases; every other pure virtual is a no-op.
class CountingSurface final : public IDirectDrawSurface {
public:
    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        --m_refs;
        ++m_releases;
        return m_refs;
    }
    HRESULT Lock(LPRECT, LPDDSURFACEDESC, DWORD, HANDLE) override { return 0; }
    HRESULT Unlock(LPVOID) override { return 0; }
    HRESULT Blt(LPRECT, IDirectDrawSurface*, LPRECT, DWORD, LPDDBLTFX) override { return 0; }
    HRESULT BltFast(DWORD, DWORD, IDirectDrawSurface*, LPRECT, DWORD) override { return 0; }
    HRESULT Flip(IDirectDrawSurface*, DWORD) override { return 0; }
    HRESULT GetAttachedSurface(LPDDSCAPS, IDirectDrawSurface**) override { return 0; }
    HRESULT SetColorKey(DWORD, LPDDCOLORKEY) override { return 0; }
    HRESULT GetColorKey(DWORD, LPDDCOLORKEY) override { return 0; }
    HRESULT GetPixelFormat(LPDDPIXELFORMAT) override { return 0; }
    HRESULT SetClipper(IDirectDrawClipper*) override { return 0; }
    HRESULT SetPalette(IDirectDrawPalette*) override { return 0; }
    HRESULT GetPalette(IDirectDrawPalette**) override { return 0; }
    HRESULT GetDC(HDC*) override { return 0; }
    HRESULT ReleaseDC(HDC) override { return 0; }
    HRESULT Restore() override { return 0; }
    HRESULT IsLost() override { return 0; }

    int m_refs = 1;
    int m_releases = 0;
};

} // namespace

TEST_CASE("projector: the subtitle teardown releases the surface and the lines", "[projector]")
{
    CChitin* previousChitin = g_pChitin;
    TestChitin chitin;

    CountingSurface surface;
    IDirectDrawSurface* surfaces[16] = {};
    surfaces[8] = &surface;

    std::vector<unsigned char> modeStorage(sizeof(CVidMode), 0);
    auto* mode = reinterpret_cast<CVidMode*>(modeStorage.data());
    mode->pSurfaces = surfaces;

    auto* engine = static_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    engine->pVidMode = mode;

    g_pChitin = &chitin;
    chitin.pActiveEngine = engine;

    auto* subtitles = static_cast<CProjectorSubtitles*>(::operator new(sizeof(CProjectorSubtitles)));
    new (&subtitles->m_sText) CString("Subtitle");
    subtitles->m_pLines = new CProjectorSubtitlesLine[2];
    subtitles->m_pLines[0].m_sText = "first";
    subtitles->m_pLines[1].m_sText = "second";
    CBaldurProjector::pProjectorSubtitles = subtitles;

    subtitles->Unload();

    CHECK(surface.m_releases == 1);
    CHECK(mode->pSurfaces[8] == NULL);
    CHECK(subtitles->m_pLines == NULL);

    // What the two callers do next: the block is raw-deleted (the helper has
    // already run the member destructors) and the global is nulled.
    ::operator delete(CBaldurProjector::pProjectorSubtitles);
    CBaldurProjector::pProjectorSubtitles = NULL;

    ::operator delete(engine);
    g_pChitin = previousChitin;
}

TEST_CASE("projector: the subtitle teardown without a video mode", "[projector]")
{
    CChitin* previousChitin = g_pChitin;
    TestChitin chitin;

    auto* engine = static_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));

    g_pChitin = &chitin;
    chitin.pActiveEngine = engine;

    auto* subtitles = static_cast<CProjectorSubtitles*>(::operator new(sizeof(CProjectorSubtitles)));
    new (&subtitles->m_sText) CString();
    subtitles->m_pLines = NULL;
    CBaldurProjector::pProjectorSubtitles = subtitles;

    // A null pVidMode skips the surface step; the teardown still completes.
    subtitles->Unload();

    CHECK(subtitles->m_pLines == NULL);

    ::operator delete(CBaldurProjector::pProjectorSubtitles);
    CBaldurProjector::pProjectorSubtitles = NULL;

    ::operator delete(engine);
    g_pChitin = previousChitin;
}
