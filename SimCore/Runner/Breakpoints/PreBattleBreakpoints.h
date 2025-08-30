#pragma once
#include <string>
#include "BPCore.h"

namespace battle_rng_probe
{
    enum : BPKey
    {
        BeforeRandSeedSet = 101,
        AfterRandSeedSet
    };

    BreakpointMap defaults();
    BreakpointMap load_from_file(const std::string& path);
}
