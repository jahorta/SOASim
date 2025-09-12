#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../Boot/Boot.h"
#include "../Core/Input/InputPlan.h"
#include "../Runner/Parallel/ParallelPhaseScriptRunner.h"

namespace simcore {

    enum class SeedFamily : uint8_t { Neutral = 0, Main, CStick, Triggers };

    struct RandSeedProbeEntry {
        int grid_n = 0;
        SeedFamily family = SeedFamily::Neutral;
        uint8_t x = 0;
        uint8_t y = 0;
        uint32_t seed = 0;
        long long delta = 0;
        bool ok = false;
        std::string label;
    };

    struct RandSeedProbeResult {
        uint32_t base_seed = 0;
        std::vector<RandSeedProbeEntry> entries;
    };

    struct RandSeedComboEntry {
        GCInputFrame input;
        uint32_t seed = 0;
        long long delta = 0;
        bool ok = false;
        std::string label;
    };

    struct RandSeedComboResult {
        uint32_t base_seed = 0;
        std::vector<RandSeedComboEntry> entries;
    };

    struct RngSeedDeltaArgs {
        BootPlan boot;
        std::string savestate_path;
        int samples_per_axis = 5;
        int min_value = 0;
        int max_value = 255;
        bool cap_trigger_top = true;
        uint32_t run_timeout_ms = 10000;
        uint32_t combos_attempts_per_target = 256;  // max dispatches per target
        uint32_t combos_sampler_tries = 8;   // attempts to construct a triple that sums to target
    };

    RandSeedProbeResult RunRngSeedDeltaMap(ParallelPhaseScriptRunner& runner, const RngSeedDeltaArgs& args);
    RandSeedComboResult RunFindSeedDeltaCombos(ParallelPhaseScriptRunner& runner, const RngSeedDeltaArgs& args, const RandSeedProbeResult& grid);

} // namespace simcore
