#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace simcore {
    namespace db {

        struct TasMovieRow {
            int64_t id{};
            int64_t base_file_id{};
            std::optional<int64_t> new_rtc;
            std::string status;
            std::string progress_log;
            int64_t created_at{};
            std::optional<int64_t> started_at;
            std::optional<int64_t> completed_at;
            int32_t attempt_count{};
            std::optional<std::string> last_error;
            int32_t priority{};
            std::optional<int64_t> output_savestate_id;
        };

        struct TasMovieRepo {
            static DbResult<int64_t> Enqueue(DbEnv& env, int64_t base_file_id, std::optional<int64_t> new_rtc, int32_t priority);
            static DbResult<int64_t> IdempotentEnqueue(DbEnv& env, int64_t base_file_id, std::optional<int64_t> new_rtc, int32_t priority);

            static DbResult<void>    AppendProgress(DbEnv& env, int64_t tas_movie_id, const std::string& text, int max_bytes = 8192);
            static DbResult<void>    MarkFailed(DbEnv& env, int64_t tas_movie_id, const std::string& err);
            static DbResult<void>    MarkDone(DbEnv& env, int64_t tas_movie_id, int64_t savestate_id);

            static DbResult<TasMovieRow> Get(DbEnv& env, int64_t id);
            static DbResult<std::vector<TasMovieRow>> ListPlanned(DbEnv& env);
            static DbResult<std::vector<TasMovieRow>> ListActive(DbEnv& env);

            static inline std::future<DbResult<int64_t>>
                IdempotentEnqueueAsync(int64_t base_file_id, std::optional<int64_t> new_rtc,
                    std::optional<int64_t> gen_obj, int32_t priority, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return IdempotentEnqueue(e, base_file_id, new_rtc, priority); });
            }
            static inline std::future<DbResult<void>>
                MarkDoneAsync(int64_t id, int64_t savestate_id, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return MarkDone(e, id, savestate_id); });
            }
        };

    }
} // namespace simcore::db
