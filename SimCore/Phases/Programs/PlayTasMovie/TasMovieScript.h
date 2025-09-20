// Script/TasMovieScript.h
#pragma once
#include <string>
#include <vector>
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/KeyRegistry.h"
#include "../../../Runner/Breakpoints/BPRegistry.h"
#include "TasMoviePayload.h"

namespace simcore::tasmovie {

    // Main TAS program: all arguments come from job/context via *_FROM(key).
    // BPs: use the phase's canonical set (e.g., BeforeRandSeedSet).
    inline PhaseScript MakeTasMovieProgram()
    {
        PhaseScript p{};
        p.canonical_bp_keys = { bp::prebattle::BeforeRandSeedSet }; // adjust if your header exposes a helper list

        // 1) Make sure the disc matches the movie.
        p.ops.push_back(OpRequireDiscGameIdFrom(keys::tas::DISC_ID6));

        // 2) Start movie playback from the DTM path provided by the job.
        p.ops.push_back(OpMoviePlayFrom(keys::tas::DTM_PATH));

        // 3) Set a per-job timeout derived from the DTM header.
        p.ops.push_back(OpSetTimeoutFrom(keys::core::RUN_MS));

        // 4) Run until any armed canonical BP triggers (or failure/timeouts/watchdogs).
        p.ops.push_back(OpRunUntilBp());

        // 5) Always stop playback cleanly.
        p.ops.push_back(OpMovieStop());

        // 6) Save a state named after the DTM path (worker decides whether to save-on-fail by consulting context).
        p.ops.push_back(OpSaveSavestateFrom(keys::tas::SAVE_PATH));

        return p;
    }

} // namespace simcore::tas_movie


