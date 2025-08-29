#include "PreBattleBreakpoints.h"
#include <fstream>

using namespace battle_rng_screen;

BreakpointMap battle_rng_screen::defaults()
{
    BreakpointMap m;
    m.addrs = {
        { BeforeRandSeedSet,  0x801019a8u, "BeforeRandSeedSet" },
        { AfterRandSeedSet,   0x8000a1dcu, "AfterRandSeedSet" }
    };
    m.start_key = BeforeRandSeedSet;
    return m;
}

BreakpointMap battle_rng_screen::load_from_file(const std::string& path)
{
    return load_bpmap_file(path, defaults());
}
