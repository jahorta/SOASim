#pragma once
#include <cstdint>

namespace simcore::vmcore {

    // Canonical outcomes for RUN_UNTIL_BP
    enum class RunToBpOutcome : uint32_t {
        Hit = 0,  // a monitored breakpoint fired
        Timeout = 1,  // wall-clock limit reached
        ViStalled = 2,  // VI didn't advance for the configured stall window
        MovieEnded = 3,  // movie playback ended before any breakpoint fired
        Aborted = 4,  // reserved for future external aborts
        Unknown = 5,  // catch-all
    };

    // Keys the VM ALWAYS writes when executing RUN_UNTIL_BP.
    // These are program-agnostic so any conductor can consume them.
    inline constexpr const char* K_RUN_OUTCOME_CODE = "core.run.outcome_code"; // uint32_t (RunToBpOutcome)
    inline constexpr const char* K_RUN_ELAPSED_MS = "core.run.elapsed_ms";   // uint32_t
    inline constexpr const char* K_RUN_HIT_PC = "core.run.hit_pc";       // uint32_t (0 if not hit)

    // Optional telemetry (present when available)
    inline constexpr const char* K_METRICS_VI_LAST = "core.metrics.vi_last";  // uint32_t
    inline constexpr const char* K_METRICS_POLL_MS = "core.metrics.poll_ms";  // uint32_t

    // ===================
    // Common run inputs (program-agnostic)
    // ===================
    // Use these in payload decoders/scripts to drive RUN_UNTIL_BP behavior.

    inline constexpr const char* K_RUN_MS = "core.input.run_ms";       // uint32_t timeout hint
    inline constexpr const char* K_VI_STALL_MS = "core.input.vi_stall_ms";  // uint32_t stall window


} // namespace simcore::vmcore
