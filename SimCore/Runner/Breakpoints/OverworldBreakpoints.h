#pragma once
#include <string>
#include "BPCore.h"

namespace overworld
{
    enum : BPKey
    {
        OverworldInit = 1,
        StartTravelInputs,
        RandomEncounter,
        ReachedGoal
    };

    BreakpointMap defaults();
    BreakpointMap load_from_file(const std::string& path);
}
