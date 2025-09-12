#pragma once
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/VMCoreKeys.reg.h"
#include "../../../Runner/SOAConstants.h"
#include "../../../Runner/Breakpoints/PreBattleBreakpoints.h"
#include "SeedProbeKeys.reg.h"

namespace simcore::seedprobe {

    static const std::string K_INPUT{ "seed.input" };
    static const std::string K_RNG_SEED{ "seed.seed" };
    
    // Build a small program for "apply 1 frame input, then run-until-bp, then read RNG"
    inline PhaseScript MakeSeedProbeProgram()
    {
        PhaseScript ps{};
        ps.canonical_bp_keys = { battle_rng_probe::AfterRandSeedSet };

        ps.ops.push_back(OpArmPhaseBps());
        ps.ops.push_back(OpLoadSnapshot());

        // Apply input (from numeric key)
        ps.ops.push_back(OpApplyInputFrom(simcore::keys::seed::INPUT));

        // Run until RNG seed set breakpoint
        ps.ops.push_back(OpRunUntilBp());

        // Read RNG and emit
        ps.ops.push_back(OpReadU32(SOA::ADDR::RNG_SEED, simcore::keys::seed::RNG_SEED));

        ps.ops.push_back(OpEmitResult(simcore::keys::seed::RNG_SEED));

        return ps;
    }

    

} // namespace simcore
