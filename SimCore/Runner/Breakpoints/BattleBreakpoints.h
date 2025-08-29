#pragma once
#include <string>
#include "BPCore.h"

namespace battle
{
    enum : BPKey
    {
        BattleInit = 201,
        BattleInitComplete,
        StartTurnInputs,
        StartTurn,
        EndTurn,
        EndBattle
    };

    BreakpointMap defaults();
    BreakpointMap load_from_file(const std::string& path);
}
