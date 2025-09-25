#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "DbEnv.h"
#include "DbResult.h"

namespace simcore::db {

        struct DbEventRow {
            int64_t id{};
            std::string kind;
            int64_t created_mono_ns{};
            std::string created_utc;
            std::string coord_boot_id;
            std::string payload;
        };

        struct DbEventsRepo {
            static DbResult<int64_t> Insert(DbEnv& env,
                const std::string& kind,
                int64_t created_mono_ns,
                const std::string& created_utc,
                const std::string& coord_boot_id,
                const std::string& payload);

            static DbResult<void> InsertBootEvent(DbEnv& env,
                const std::string& kind,
                const std::string& payload);

            static DbResult<std::vector<DbEventRow>> ListRecent(DbEnv& env,
                int limit);

            static DbResult<std::vector<DbEventRow>> ListSince(DbEnv& env,
                int64_t since_mono_ns,
                const std::string& kind_filter); // empty = all
        };

} // namespace simcore::db
