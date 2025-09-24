#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include <string>
#include <optional>

namespace simcore {
    namespace db {

        struct SavestateHelpers {
            static DbResult<int64_t> CreateFromTasMovie(DbEnv& env, int64_t tas_movie_id,
                int32_t savestate_type,
                const std::string& note_prefix,
                int64_t object_ref_id);
        };

    }
} // namespace simcore::db
