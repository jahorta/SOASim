#pragma once
#include <cstdint>

namespace simcore::battle {
    enum class Outcome : uint16_t {
        Victory =         0x0000u,
        Defeat =          0x0001u,
        PredFailure =     0x0002u,
        PlanMismatch =    0x0003u,
        TurnsExhausted =  0x0004u,
        Timeout =         0x0005u,
        ViStalled =       0x0006u,
        MovieEnded =      0x0007u,
        Unknown =         0xFFFFu,
    };

    inline std::string get_outcome_string(Outcome o) {
        switch (o) {
        case Outcome::Victory: return "Victory";
        case Outcome::Defeat: return "Defeat";
        case Outcome::PredFailure: return "Predicate Failure";
        case Outcome::PlanMismatch: return "Plan Mismatch";
        case Outcome::TurnsExhausted: return "Turns Exhausted";
        case Outcome::Timeout: return "Timeout";
        case Outcome::ViStalled: return "VI Stalled";
        case Outcome::MovieEnded: return "Movie Ended";
        case Outcome::Unknown: 
        default:
            return "Victory";
        }
    }
} // namespace simcore::battle
