// Script/TasMovieScript.h
#pragma once
#include <string>
#include <vector>
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/VMCoreKeys.h"
#include "../../../Runner/Breakpoints/PreBattleBreakpoints.h"
#include "TasMoviePayload.h"

namespace simcore::tasmovie {

    // ---- Context keys used by the TAS program (consumed via *_FROM(key) ops) ----
    inline constexpr const char* K_DTM_PATH = "tas.dtm_path";
    inline constexpr const char* K_SAVE_PATH = "tas.save_path";
    inline constexpr const char* K_SAVE_ON_FAIL = "tas.save_on_fail";
    inline constexpr const char* K_DISC_ID6 = "tas.disc_id6";  // 6 bytes

    // Main TAS program: all arguments come from job/context via *_FROM(key).
    // BPs: use the phase’s canonical set (e.g., BeforeRandSeedSet).
    inline PhaseScript MakeTasMovieProgram()
    {
        PhaseScript p{};
        p.canonical_bp_keys = { battle_rng_probe::BeforeRandSeedSet }; // adjust if your header exposes a helper list

        // 1) Make sure the disc matches the movie.
        p.ops.push_back(OpRequireDiscID6From(std::string(K_DISC_ID6)));

        // 2) Start movie playback from the DTM path provided by the job.
        p.ops.push_back(OpMoviePlayFrom(std::string(K_DTM_PATH)));

        // 3) Set a per-job timeout derived from the DTM header.
        p.ops.push_back(OpSetTimeoutFrom(std::string(vmcore::K_RUN_MS)));

        // 4) Run until any armed canonical BP triggers (or failure/timeouts/watchdogs).
        p.ops.push_back(PSOp{ PSOpCode::RUN_UNTIL_BP });

        // 5) Always stop playback cleanly.
        p.ops.push_back(OpMovieStop());

        // 6) Save a state named after the DTM path (worker decides whether to save-on-fail by consulting context).
        p.ops.push_back(OpSaveSavestateFrom(std::string(K_SAVE_PATH)));

        // Optionally export a value (e.g., last PC or a status bit) if your VM populates ctx.
        p.ops.push_back(OpEmitResult("last"));

        return p;
    }

} // namespace simcore::tas_movie


