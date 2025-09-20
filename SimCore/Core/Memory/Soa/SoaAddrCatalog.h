#pragma once
#include <cstdint>
#include "SoaAddrProgramBuilder.h"
#include "../Soa/SoaAddrRegistry.h"
#include "SoaStructs.h"

// Helpers to build common address paths without new keys.
// Example: enemy-drops, treasure slots, turn order, etc.
namespace addrprog::catalog {

	// Treasure slot in Battle_State: MainInstancePtr -> +0xE -> slot[i] -> field_off
	template<class FieldT = void>
	uint32_t battle_treasure_slot(addrprog::Builder& b, uint16_t slot_index, FieldT soa::BattleItemDropSlot::* field = nullptr);

	// EnemyDefinition.items[j].field for combatant slot k:
	// CombatantInstancesTable + k*4 -> *(u32) -> +0x110 -> items[j] -> field_off
	template<class FieldT = void>
	uint32_t enemy_item_field(addrprog::Builder& b, uint16_t combatant_slot, uint16_t item_index, FieldT soa::ItemDrop::* field = nullptr);

	// Turn order derived buffer entry: derived_battle::TurnOrderIdx_base + idx
	uint32_t turn_order_idx(addrprog::Builder& b, uint16_t logical_id_index);

}
