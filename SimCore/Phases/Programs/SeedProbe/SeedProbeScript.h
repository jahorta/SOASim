#pragma once
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/VMCoreKeys.h"
#include "../../../Runner/SOAConstants.h"
#include "../../../Runner/Breakpoints/PreBattleBreakpoints.h"

namespace simcore::seedprobe {

    static const std::string K_INPUT{ "seed.input" };
    static const std::string K_RNG_SEED{ "seed.seed" };
    
    // Build a small program for “apply 1 frame input, then run-until-bp, then read RNG”
    inline PhaseScript MakeSeedProbeProgram()
    {
        PhaseScript ps{};
        ps.canonical_bp_keys = {battle_rng_probe::AfterRandSeedSet};

        ps.ops.push_back({ PSOpCode::ARM_PHASE_BPS_ONCE });

        // Always start from pristine snapshot (VM loads snapshot before run, but keep explicit op for clarity)
        ps.ops.push_back({ PSOpCode::LOAD_SNAPSHOT });

        // Apply the job's one-frame input
        ps.ops.push_back(OpApplyInputFrom(K_INPUT));

        // Run until a phase BP (the VM does not filter by key here; you’ll check pc in the caller if needed)
        ps.ops.push_back({ PSOpCode::RUN_UNTIL_BP });

        // Read RNG value and expose as "seed"
        {
            PSOp op{}; op.code = PSOpCode::READ_U32; op.rd = { SOA::ADDR::RNG_SEED, K_RNG_SEED }; ps.ops.push_back(op);
        }
        {
            ps.ops.push_back(OpEmitResult(K_RNG_SEED));
        }

        return ps;
    }

    

} // namespace simcore
