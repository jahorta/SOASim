#pragma once
#include <string>
#include "BPCore.h"

namespace battle
{
    enum : BPKey
    {
        BattleInit = 201,
        BattleInitComplete,
        FirstTurnInputs,
        StartTurn,
        StartAction,
        EndAction,
        EndTurn,
        EndBattle
    };

    BreakpointMap defaults();
    BreakpointMap load_from_file(const std::string& path);
}
