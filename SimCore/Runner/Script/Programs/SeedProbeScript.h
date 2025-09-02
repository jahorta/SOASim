#pragma once
#include "../PhaseScriptVM.h"
#include "../../SOAConstants.h"
#include "../../Breakpoints/PreBattleBreakpoints.h"

namespace simcore {

    // Build a small program for “apply 1 frame input, then run-until-bp, then read RNG”
    inline PhaseScript MakeSeedProbeProgram(uint32_t run_timeout_ms)
    {
        PhaseScript ps{};
        ps.canonical_bp_keys = {battle_rng_probe::AfterRandSeedSet};

        ps.ops.push_back({ PSOpCode::ARM_PHASE_BPS_ONCE });

        // Always start from pristine snapshot (VM loads snapshot before run, but keep explicit op for clarity)
        ps.ops.push_back({ PSOpCode::LOAD_SNAPSHOT });

        // Apply the job's one-frame input
        {
            PSOp op{}; op.code = PSOpCode::APPLY_INPUT; ps.ops.push_back(op);
        }

        // Run until a phase BP (the VM does not filter by key here; you’ll check pc in the caller if needed)
        {
            PSOp op{}; op.code = PSOpCode::RUN_UNTIL_BP; op.to = { run_timeout_ms }; ps.ops.push_back(op);
        }

        // Read RNG value and expose as "seed"
        {
            PSOp op{}; op.code = PSOpCode::READ_U32; op.rd = { SOA::ADDR::RNG_SEED, "seed" }; ps.ops.push_back(op);
        }
        {
            PSOp op{}; op.code = PSOpCode::EMIT_RESULT; op.em = { "seed" }; ps.ops.push_back(op);
        }

        return ps;
    }

} // namespace simcore
