#pragma once
#include <cstdint>
#include "../../../Core/Memory/SoaStructs.h"  // contains dme::CombatantInstance and dme::EnemyDefinition

namespace simcore::battlectx {

    struct BattleSlot
    {
        uint8_t  present{ 0 };       // slot has a live instance
        uint8_t  is_player{ 0 };     // 1 for slots [0..3]
        uint16_t id{ 0 };            // from 0x80309DCC
        uint8_t  has_enemy_def{ 0 }; // 1 iff enemy slot and enemy_def is resolved

        // Materialized snapshots (byte-faithful images from MEM1)
        soa::CombatantInstance instance{};     // valid only if present=1
        soa::EnemyDefinition   enemy_def{};    // valid only if has_enemy_def=1

        // Optional provenance (kept for debugging / inspection)
        uint32_t instance_addr{ 0 };
        uint32_t enemy_def_addr{ 0 };
    };

    struct BattleContext
    {
        BattleSlot slots[12];

        BattleContext() = default;
    };

} // namespace simcore::battlectx
