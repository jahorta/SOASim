#pragma once
#include <cstddef>
#include "KeyIds.h"

namespace simcore::keys::battle {

#define BATTLE_KEYS(X) \
  X(ACTIVE_TURN,              0x0300, "battle.active_turn")   \
  X(INITIAL_INPUT,            0x0301, "battle.initial_input") \
  X(BATTLE_OUTCOME,           0x0302, "battle.outcome_code") \
  X(INPUTPLAN_FRAME_COUNT,    0x0311, "battle.inputplan.frame_count") \
  X(INPUTPLAN,                0x0312, "battle.inputplan.frames") \
  X(CTX_BLOB,                 0x0320, "battle.CTX_BLOB")      \
  X(NUM_TURN_PLANS,           0x0330, "battle.turnplan.count")     \
  X(TURN_PLANS,               0x0331, "battle.turnplan.plans") \
  X(LAST_TURN,                0x0332, "battle.turnplan.last_idx") \
  X(PLAN_MATERIALIZE_ERR,     0x0333, "battle.turnplan.materialize_err")

#define DECL_KEY(NAME, ID, STR) inline constexpr simcore::keys::KeyId NAME = static_cast<simcore::keys::KeyId>(ID); \
static_assert(NAME >= simcore::keys::BATTLE_MIN && NAME <= simcore::keys::BATTLE_MAX, "battle key out of range");
	BATTLE_KEYS(DECL_KEY)
#undef DECL_KEY

		inline constexpr simcore::keys::KeyPair kKeys[] = {
		#define ROW(NAME, ID, STR) simcore::keys::KeyPair{ static_cast<simcore::keys::KeyId>(ID), STR },
		BATTLE_KEYS(ROW)
		#undef ROW
	};
	inline constexpr std::size_t kCount = sizeof(kKeys) / sizeof(kKeys[0]);

#undef BATTLE_KEYS

} // namespace simcore::keys::battle
