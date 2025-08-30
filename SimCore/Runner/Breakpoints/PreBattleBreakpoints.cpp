#include "PreBattleBreakpoints.h"
#include <fstream>

using namespace battle_rng_probe;

BreakpointMap battle_rng_probe::defaults()
{
    BreakpointMap m;
    m.addrs = {
        { BeforeRandSeedSet,  0x80101e48u, "BeforeRandSeedSet" },
        { AfterRandSeedSet,   0x8000a1dcu, "AfterRandSeedSet" }
    };
    m.start_key = BeforeRandSeedSet;
    m.terminal_key = AfterRandSeedSet;
    return m;
}

BreakpointMap battle_rng_probe::load_from_file(const std::string& path)
{
    return load_bpmap_file(path, defaults());
}
