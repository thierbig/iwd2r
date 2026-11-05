#include "test_knobs.h"

#include <cstdlib>
#include <cstring>

namespace platform {

bool ResourceServerDisabled()
{
    const char* knob = std::getenv("IWD2R_RS");
    return knob != nullptr && std::strcmp(knob, "null") == 0;
}

}
