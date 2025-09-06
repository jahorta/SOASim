// Script/TasMovieScript.h
#pragma once
#include <string>
#include <vector>
#include "../PhaseScriptVM.h"

namespace simcore {

    inline PhaseScript MakeTasMainProgram(const std::string& dtm_path,
        const char require_id6[6],
        uint32_t run_until_ms,
        const std::string& save_state_path,
        const std::vector<BPKey>& canonical_keys)
    {
        PhaseScript p{};
        p.canonical_bp_keys = canonical_keys;
        p.ops.push_back(OpRequireDiscID6(require_id6));
        p.ops.push_back(OpMoviePlay(dtm_path));
        p.ops.push_back(PSOp{ PSOpCode::RUN_UNTIL_BP, /*input*/{}, {}, {}, PSArg_Timeout{ run_until_ms }, {} });
        p.ops.push_back(OpMovieStop());
        p.ops.push_back(OpSaveSavestate(save_state_path));
        p.ops.push_back(PSOp{ PSOpCode::EMIT_RESULT, /*input*/{}, {}, {}, {}, PSArg_Emit{ "last" } });
        return p;
    }

} // namespace simcore

