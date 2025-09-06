// Script/TasMovieScript.h
#pragma once
#include <string>
#include "PhaseScriptVM.h"

namespace simcore::script {

    inline PSInit make_tas_init_program(const std::string& savestate_path, uint32_t default_timeout_ms)
    {
        PSInit init{};
        init.savestate_path = savestate_path;
        init.default_timeout_ms = default_timeout_ms;
        return init;
    }

    inline PhaseScript make_tas_main_program(const std::string& dtm_path,
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

} // namespace simcore::script

