#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include "Core/Input/InputPlan.h" // for GCInputFrame

namespace simcore {

    struct SeedProbeConfig {
        int samples_per_axis = 5;
        bool cap_trigger_top = true;
        uint32_t run_timeout_ms = 50;
        uint32_t rng_addr = 0x803469a8u;
    };

    enum class SeedFamily : uint8_t { Neutral = 0, Main = 1, CStick = 2, Triggers = 3 };

    struct RandSeedEntry {
        int samples_per_axis = 0;
        SeedFamily family = SeedFamily::Neutral;
        float x = 0.0f;   // mx / cx / lt
        float y = 0.0f;   // my / cy / rt
        uint32_t seed = 0;
        long long delta = 0;
        bool ok = false;
        std::string label;
    };

    struct RandSeedProbeResult {
        uint32_t base_seed = 0;
        std::vector<RandSeedEntry> entries;
    };

    struct SeedProbeOps {
        std::function<void()> reset_to_prebattle;
        std::function<void(const GCInputFrame&)> apply_input_frame;
        std::function<bool(uint32_t)> run_until_after_seed;
        std::function<uint32_t(uint32_t)> read_u32;
    };

    std::vector<GCInputFrame> build_grid_main(int n);
    std::vector<GCInputFrame> build_grid_cstick(int n);
    std::vector<GCInputFrame> build_grid_triggers(int n, bool cap_top);

    void draw_progress_bar(const char* label, size_t done, size_t total);

    void log_probe_summary(const RandSeedProbeResult& r);
    std::vector<std::string> to_csv_lines(const RandSeedProbeResult& r);

    void print_family_grid(const RandSeedProbeResult& r, SeedFamily fam, int N, const char* title);

} // namespace simcore
