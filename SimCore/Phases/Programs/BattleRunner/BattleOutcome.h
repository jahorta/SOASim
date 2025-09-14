#pragma once
#include <cstdint>

namespace simcore::battle {
    enum class Outcome : uint32_t {
        Victory = 0x00010000u,
        Defeat = 0x00010001u,
        PlanMismatch = 0x00010002u,
        TurnsExhausted = 0x00010003u,
        Timeout = 0x00010004u,
        ViStalled = 0x00010005u,
        MovieEnded = 0x00010006u,
        Unknown = 0x0001FFFFu,
    };
} // namespace simcore::battle
