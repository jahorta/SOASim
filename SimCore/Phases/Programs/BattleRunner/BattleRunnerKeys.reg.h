#pragma once
#include <cstddef>
#include "../../../Runner/Script/KeyIds.h"

namespace simcore::keys::battle {

#define BATTLE_KEYS(X) \
  X(ACTIVE_TURN,   0x0300, "battle.active_turn")   \
  X(INITIAL_INPUT, 0x0301, "battle.initial_input") \
  X(NUM_PLANS,     0x0302, "battle.plan.count")     \
  X(PLAN_COUNTS,   0x0303, "battle.plan.counts")  /* raw u32 array in a std::string */ \
  X(PLAN_TABLE,    0x0304, "battle.plan.frames")  /* raw GCInputFrame array in a std::string (flattened) */  \
  X(LAST_TURN_IDX, 0x0305, "battle.plan.last_idx") \
  X(CTX_BLOB,      0x03C0, "battle.CTX_BLOB")

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
