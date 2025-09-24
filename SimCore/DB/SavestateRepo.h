#pragma once
#include "DbEnv.h"
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
            static int64_t plan(DbEnv::Tx& tx, int savestate_type, const std::string& note);
            static void finalize(DbEnv::Tx& tx, int64_t id, int64_t object_ref_id);
            static std::optional<SavestateRow> get(DbEnv::Tx& tx, int64_t id);
        };

    } // namespace db
} // namespace simcore
