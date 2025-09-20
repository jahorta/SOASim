#pragma once
#include <cstdint>
#include <vector>

namespace soa::battle::actions {

    enum class BattleAction : uint8_t {
        Attack = 0,
        Defend = 1,
        Focus = 2,
        FakeAttack = 3,
        UseItem = 4
    };

    struct ActionParameters {
        uint32_t target_mask = 0;   // single-target for now
        uint8_t  rng_tickle = 0;   // FakeAttack intensity
        uint32_t guard_flags = 0;   // reserved
        uint16_t item_id = 0xFFFF; // valid when macro==UseItem
    };

    struct ActionPlan {
        uint8_t actor_slot = 0;     // 0..3
        uint8_t is_prelude = 0;     // 1 if prelude
        BattleAction macro{};
        ActionParameters params{};
    };

    using TurnPlanSpec = std::vector<ActionPlan>; 

    struct TurnPlan {
        uint32_t     fake_attack_count = 0;
        TurnPlanSpec spec;
    };

    using BattlePath = std::vector<TurnPlan>;

} // namespace soa::battle::actions
