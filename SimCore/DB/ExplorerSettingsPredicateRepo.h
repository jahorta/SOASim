#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <vector>
#include <cstdint>

namespace simcore {
    namespace db {

        struct SettingsPredicateRow { int64_t settings_id; int32_t ordinal; int64_t predicate_id; };

        struct ExplorerSettingsPredicateRepo {
            static DbResult<void> ReplaceAll(DbEnv& env, int64_t settings_id, const std::vector<SettingsPredicateRow>& rows);
            static DbResult<std::vector<SettingsPredicateRow>> List(DbEnv& env, int64_t settings_id);
            static DbResult<void> Append(DbEnv& env, int64_t settings_id, int64_t predicate_id);
            static DbResult<void> Remove(DbEnv& env, int64_t settings_id, int32_t ordinal);
            static DbResult<void> Clear(DbEnv& env, int64_t settings_id);
        };

    }
} // namespace simcore::db
