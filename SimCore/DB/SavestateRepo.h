#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include <string>
#include <cstdint>
#include <optional>

namespace simcore {
    namespace db {

        struct SavestateRow {
            int64_t id{};
            int savestate_type{};
            std::string note;
            int64_t object_ref_id{};
            bool complete{ false };
        };

        class SavestateRepo {
        public:
            static DbResult<int64_t> plan(DbEnv& env, int savestate_type, const std::string& note);
            static DbResult<void> finalize(DbEnv& env, int64_t id, int64_t object_ref_id);
            static std::optional<SavestateRow> get(DbEnv& env, int64_t id);
        };

    } // namespace db
} // namespace simcore
