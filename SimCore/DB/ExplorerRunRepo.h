#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <string>
#include <cstdint>

namespace simcore {
    namespace db {

        struct ExplorerRunRow {
            int64_t id{};
            int64_t probe_id{};
            int64_t settings_id{};
            std::string status;      // planned|running|done
            std::string progress_log;
            int32_t complete{};
        };

        struct ExplorerRunRepo {
            static DbResult<int64_t> Create(DbEnv& env, int64_t probe_id, int64_t settings_id);
            static DbResult<int64_t> IdempotentCreate(DbEnv& env, int64_t probe_id, int64_t settings_id);
            static DbResult<void>    MarkRunning(DbEnv& env, int64_t run_id);
            static DbResult<void>    MarkDone(DbEnv& env, int64_t run_id);
            static DbResult<void>    AppendProgress(DbEnv& env, int64_t run_id, const std::string& text, int max_bytes = 8192);
            static DbResult<ExplorerRunRow> Get(DbEnv& env, int64_t run_id);
            static DbResult<std::vector<ExplorerRunRow>> ListActiveForProbe(DbEnv& env, int64_t probe_id);

            static inline std::future<DbResult<int64_t>>
                IdempotentCreateAsync(int64_t probe_id, int64_t settings_id, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return IdempotentCreate(e, probe_id, settings_id); });
            }
            static inline std::future<DbResult<void>>
                AppendProgressAsync(int64_t run_id, std::string text, int max_bytes = 8192, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                    [run_id, text = std::move(text), max_bytes](DbEnv& e) { return AppendProgress(e, run_id, text, max_bytes); });
            }
        };

    }
} // namespace simcore::db
