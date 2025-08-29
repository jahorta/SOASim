#include "OverworldBreakpoints.h"
#include <fstream>

using namespace overworld;

BreakpointMap overworld::defaults()
{
    BreakpointMap m;
    m.addrs = {
        { OverworldInit,     0x00000000u, "OverworldInit" },
        { StartTravelInputs, 0x00000000u, "StartTravelInputs" },
        { RandomEncounter,   0x00000000u, "RandomEncounter" },
        { ReachedGoal,       0x00000000u, "ReachedGoal" }
    };
    m.start_key = OverworldInit;
    m.terminal_key = ReachedGoal;
    return m;
}

BreakpointMap overworld::load_from_file(const std::string& path)
{
    return load_bpmap_file(path, defaults());
}
