#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "engine_world.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef _WIN32
// CProgressBar::AddActionTarget (0x78BBB0) rescales in 32-bit floats. When
// the progress another thread advances lands within one float ulp below the
// target (ulp is 4 at 60M), both values round to the same float: v1 is 0,
// v2 is inf and __ftol wraps the new target to 0x80000000, so the second
// UTIL_ASSERT (src/CProgressBar.cpp:52) aborts. The original's arithmetic at
// 0x78BBF6-0x78BC4E is identical and asserts at ChDimm.cpp line 10395 the
// same way -- a latent original bug (docs/followups.md, "Intermittent
// CProgressBar::AddActionTarget"). The race is untestable, so this case puts
// the live engine's bar straight into the rounded state (60460784 target,
// 60460782 progress, the Unmarshal call's 3000000 units) inside a forked
// child, as tests/test_ui_misc.cpp's character-sheet export does, and pins
// the abort.
TEST_CASE("progress: AddActionTarget aborts when progress rounds onto the target", "[progress][data]")
{
    if (engine_world::DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(engine_world::StartEngine(error));
    INFO(error);

    const std::string out = (std::filesystem::temp_directory_path()
        / ("iwd2r-aat-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
            + "-" + std::to_string(::getpid()) + ".txt"))
                                 .string();
    std::remove(out.c_str());

    fflush(NULL);
    pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        // Catch2's own SIGABRT handler must not run here: its failure report
        // would land on the parent's stdout. The default action ends the
        // child on UtilAssert's abort, after the shim printed the assert.
        signal(SIGABRT, SIG_DFL);
        // The UtilAssert path prints its MessageBoxA line on stderr; keep it
        // out of the parent's ctest output and in a file to check.
        FILE* redirected = freopen(out.c_str(), "w", stderr);
        (void)redirected; // only the parent reads the file
        CProgressBar& bar = g_pChitin->cProgressBar;
        bar.m_nActionTarget = 60460784;
        bar.m_nActionProgress = 60460782;
        bar.AddActionTarget(3000000);
        _exit(0); // unreachable: the assert above aborts the child
    }
    // The child of a process with threads has only this one: should it block
    // on a lock another thread held at the fork, it is killed after a minute.
    int status = 0;
    pid_t waited = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while ((waited = waitpid(pid, &status, WNOHANG)) == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (waited == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        FAIL("the progress-bar child process did not end within a minute");
    }
    REQUIRE(waited == pid);
    REQUIRE(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGABRT);

    std::ifstream in(out);
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    std::remove(out.c_str());
    const std::string text = ss.str();
    INFO("child stderr:\n"
         << text);
    CHECK(text.find("An Assertion failed in CProgressBar.cpp at line number 52") != std::string::npos);
}
#endif
