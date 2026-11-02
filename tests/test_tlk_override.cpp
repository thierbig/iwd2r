#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CTlkFileOverride.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::atomic<int> s_counter { 0 };

class TempDir {
public:
    TempDir()
    {
        m_path = std::filesystem::temp_directory_path() / ("iwd2r_tlk_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(s_counter++));
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }
    std::string file(const char* name) const { return (m_path / name).string(); }

private:
    std::filesystem::path m_path;
};

}

// The start screen reaches CTlkFileOverride::Open/CloseFiles from both threads
// at once: the main thread's WM_ACTIVATEAPP (CChitin::OnAltTab calls the
// engine's EngineActivated directly) and the AI thread's movie switch
// (CChitin::SelectEngine -> EngineActivated). The unsynchronized
// check-then-fclose in CFile::Close then freed the same FILE twice (issue #79,
// "free(): double free detected in tcache 2"). The lifecycle is lock-protected;
// hammering it from two threads must not abort.
TEST_CASE("tlk override: concurrent open and close do not double free", "[tlk]")
{
    TempDir dir;
    const std::string header = dir.file("default.toh");
    const std::string text = dir.file("default.tot");

    CTlkFileOverride override;

    std::atomic<bool> stop { false };

    std::thread closer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            override.CloseFiles();
        }
    });

    std::thread opener([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            override.Open(CString(header.c_str()), CString(text.c_str()));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    stop.store(true, std::memory_order_relaxed);
    closer.join();
    opener.join();

    override.CloseFiles();
    CHECK(override.m_header.m_bOpen == FALSE);
    CHECK(override.m_text.m_bOpen == FALSE);
}
