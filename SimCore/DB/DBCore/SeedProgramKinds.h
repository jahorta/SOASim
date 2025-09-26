#pragma once
#include "DbEnv.h"
#include "DbResult.h"
#include <cstdint>

namespace simcore {
	namespace db {

		// Inserts default program kinds based on Wire.h enum values.
		// Safe to call repeatedly; uses INSERT OR IGNORE.
		DbResult<void> SeedProgramKinds(DbEnv& env);

	}
} // namespace simcore::db
