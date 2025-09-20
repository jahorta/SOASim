#pragma once
#include <cstddef>
#include "KeyIds.h"

namespace simcore::keys::seed {

#define SEED_KEYS(X) \
  X(INPUT,    0x0100, "seed.input") \
  X(RNG_SEED, 0x0101, "seed.seed")

#define DECL_KEY(NAME, ID, STR) \
  inline constexpr simcore::keys::KeyId NAME = static_cast<simcore::keys::KeyId>(ID); \
  static_assert(NAME >= simcore::keys::SEED_MIN && NAME <= simcore::keys::SEED_MAX, "seed key out of range");
	SEED_KEYS(DECL_KEY)
#undef DECL_KEY

		inline constexpr simcore::keys::KeyPair kKeys[] = {
		  #define ROW(NAME, ID, STR) simcore::keys::KeyPair{ static_cast<simcore::keys::KeyId>(ID), STR },
		  SEED_KEYS(ROW)
		  #undef ROW
	};
	inline constexpr std::size_t kCount = sizeof(kKeys) / sizeof(kKeys[0]);

#undef SEED_KEYS

} // namespace simcore::keys::seed
