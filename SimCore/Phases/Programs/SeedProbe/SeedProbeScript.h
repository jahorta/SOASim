#pragma once
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/VMCoreKeys.h"
#include "../../../Runner/SOAConstants.h"
#include "../../../Runner/Breakpoints/PreBattleBreakpoints.h"

namespace simcore::seedprobe {

    static constexpr std::string INPUT_KEY = "seed.input";
    static constexpr std::string RNG_SEED_KEY = "seed.seed";
    
    // Build a small program for “apply 1 frame input, then run-until-bp, then read RNG”
    inline PhaseScript MakeSeedProbeProgram(uint32_t run_timeout_ms)
    {
        PhaseScript ps{};
        ps.canonical_bp_keys = {battle_rng_probe::AfterRandSeedSet};

        ps.ops.push_back({ PSOpCode::ARM_PHASE_BPS_ONCE });

        // Always start from pristine snapshot (VM loads snapshot before run, but keep explicit op for clarity)
        ps.ops.push_back({ PSOpCode::LOAD_SNAPSHOT });

        // Apply the job's one-frame input
        ps.ops.push_back(OpApplyInputFrom(INPUT_KEY));

        // Run until a phase BP (the VM does not filter by key here; you’ll check pc in the caller if needed)
        ps.ops.push_back({ PSOpCode::RUN_UNTIL_BP });

        // Read RNG value and expose as "seed"
        {
            PSOp op{}; op.code = PSOpCode::READ_U32; op.rd = { SOA::ADDR::RNG_SEED, RNG_SEED_KEY }; ps.ops.push_back(op);
        }
        {
            PSOp op{}; op.code = PSOpCode::EMIT_RESULT; op.em = { RNG_SEED_KEY }; ps.ops.push_back(op);
        }

        return ps;
    }

    

} // namespace simcore
