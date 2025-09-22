#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "../../Memory/Soa/SoaConstants.h"

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

    inline std::string get_action_string(BattleAction a) {
        switch (a) {
        case BattleAction::Attack: return "Attack";
        case BattleAction::Defend: return "Guard";
        case BattleAction::Focus: return "Focus";
        case BattleAction::UseItem: return "UseItem";
        default: return "Unknown";
        }
    }

    inline int resolveTargetIndex(uint32_t mask) {
        if (!mask) return -1;
        for (int i = 4; i < 12; ++i) {
            if (mask & (1u << (i & 31u))) return i;
        }
        return -1;
    }

    inline std::string get_battle_path_summary(BattlePath bp) {
        std::vector<std::string> path;
        for (int i = 0; i < bp.size(); i++) {
            auto tp = bp[i];
            path.emplace_back("\n T" + std::to_string(i) + " FakeAtk:" + std::to_string(tp.fake_attack_count));
            for (auto sp : tp.spec) {
                std::string actor = " [" + std::to_string(sp.actor_slot) + "] " + get_action_string(sp.macro);
                if (sp.macro == BattleAction::Attack) 
                    actor = actor + ":[" + std::to_string(resolveTargetIndex(sp.params.target_mask)) + "]";
                if (sp.macro == BattleAction::UseItem) 
                {
                    actor = actor + ":[" + std::to_string(sp.params.item_id) + "]";
                    actor = actor + ":[" + std::to_string(resolveTargetIndex(sp.params.target_mask)) + "]";
                }
                path.emplace_back(actor);
            }
        }

        std::string out;
        for (auto s : path) out.append(s);
        return out;
    }

} // namespace soa::battle::actions
