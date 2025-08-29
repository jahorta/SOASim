#include "BattleBreakpoints.h"
#include <fstream>

using namespace battle;

BreakpointMap battle::defaults()
{
    BreakpointMap m;
    m.addrs = {
        { BattleInit,         0x8000a1dcu, "BattleInit" },
        { BattleInitComplete, 0x00000000u, "BattleInitComplete"},
        { StartTurnInputs,    0x00000000u, "StartTurnInputs" },
        { StartTurn,          0x00000000u, "StartTurn" },
        { EndTurn,            0x00000000u, "EndTurn" },
        { EndBattle,          0x00000000u, "EndBattle" }
    };
    m.start_key = BattleInit;
    m.terminal_key = EndBattle;
    return m;
}

BreakpointMap battle::load_from_file(const std::string& path)
{
    return load_bpmap_file(path, defaults());
}
