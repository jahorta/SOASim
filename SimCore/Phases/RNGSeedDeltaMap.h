#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../Boot/Boot.h"
#include "../Core/Input/InputPlan.h"
#include "../Runner/Parallel/ParallelPhaseScriptRunner.h"

namespace simcore {

    enum class SeedFamily : uint8_t { Neutral = 0, Main, CStick, Triggers };

    struct RandSeedEntry {
        int grid_n = 0;
        SeedFamily family = SeedFamily::Neutral;
        float x = 0.0f;
        float y = 0.0f;
        uint32_t seed = 0;
        long long delta = 0;
        bool ok = false;
        std::string label;
    };

    struct RandSeedProbeResult {
        uint32_t base_seed = 0;
        std::vector<RandSeedEntry> entries;
    };

    struct RngSeedDeltaArgs {
        BootPlan boot;
        std::string savestate_path;
        int samples_per_axis = 5;
        int min_value = 0;
        int max_value = 255;
        bool cap_trigger_top = true;
        uint32_t run_timeout_ms = 10000;
    };

    RandSeedProbeResult RunRngSeedDeltaMap(ParallelPhaseScriptRunner& runner, const RngSeedDeltaArgs& args);

} // namespace simcore
