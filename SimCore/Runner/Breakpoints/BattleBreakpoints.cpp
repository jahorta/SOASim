#include "BattleBreakpoints.h"
#include <fstream>

using namespace battle;

BreakpointMap battle::defaults()
{
    BreakpointMap m;
    m.addrs = {
        { BattleInit,         0x8000a1dcu, "BattleInit" },
        { BattleInitComplete, 0x8000a2d4u, "BattleInitComplete"}, // Temporary trial bp address. May need to change later.
        { FirstTurnInputs,    0x800719dcu, "FirstTurnInputs" },   // Temporary trial bp address. May need to change later.
        { StartTurn,          0x800715dcu, "StartTurn" },
        { StartAction,        0x800715dcu, "StartAction" },
        { EndAction,          0x8007033cu, "EndAction" },
        { EndTurn,            0x8007033cu, "EndTurn" },
        { EndBattle,          0xFEEDBEEFu, "EndBattle" }          // Need to find a unified breakpoint. May end up needing multiple terminal breakpoints, one for success, one for failure.
    };
    m.start_key = BattleInit;
    m.terminal_key = EndBattle;
    return m;
}

BreakpointMap battle::load_from_file(const std::string& path)
{
    return load_bpmap_file(path, defaults());
}
