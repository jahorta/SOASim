#pragma once
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Breakpoints/BPRegistry.h" // for battle::FirstTurnInputs

namespace simcore::battlectx {

    inline PhaseScript MakeBattleContextProbeProgram()
    {
        PhaseScript ps{};
        ps.canonical_bp_keys = { battle::FirstTurnInputs };

        ps.ops.push_back(OpArmPhaseBps());  // arms canonical list above
        ps.ops.push_back(OpLoadSnapshot()); // caller must have placed savestate into VM
        ps.ops.push_back(OpRunUntilBp());   // run neutral -> accept input
        ps.ops.push_back(OpGetBattleContext());
        ps.ops.push_back(OpReturnResult(0)); // success code 0
        return ps;
    }

} // namespace simcore::battlectx