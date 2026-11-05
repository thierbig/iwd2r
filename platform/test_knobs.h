// Test-only engine knobs (IWD2R_*), so src/ never calls getenv. The other
// knobs live next to the behavior they change: IWD2R_AUDIO in
// platform/dsound_shim.cpp and IWD2R_CRASH_TEST in platform/crash.cpp.
#pragma once

namespace platform {

// True when IWD2R_RS=null asks the engine to run without the resource-server
// thread: CInfGame::LoadArea's wait loop (0x5A24D0) then cannot advance
// m_nActionProgress by that thread's completion timing, and the progress-bar
// arithmetic is deterministic (docs/followups.md, "Intermittent
// CProgressBar::AddActionTarget").
bool ResourceServerDisabled();

}
