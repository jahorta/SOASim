#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DBService.h"
#include "SeedProbeRepo.h"
#include "ExplorerRunRepo.h"
#include "TasMovieRepo.h"
#include <future>

namespace simcore {
    namespace db {

        struct QueueDepths {
            int64_t probes_planned{};
            int64_t probes_running{};
            int64_t runs_planned{};
            int64_t runs_running{};
        };

        struct LineageRow {
            int64_t run_id{};
            int64_t probe_id{};
            int64_t settings_id{};
            int64_t savestate_id{};
        };

        struct TasMovieQueueDepths {
            int64_t planned{};
            int64_t running{};
        };

        struct QueryRepo {
            static DbResult<SeedProbeRow>        DequeueProbe(DbEnv& env);
            static DbResult<ExplorerRunRow>      DequeueRun(DbEnv& env);
            static DbResult<QueueDepths>         QueueDepthsNow(DbEnv& env);
            static DbResult<LineageRow>          GetLineage(DbEnv& env, int64_t run_id);
            static DbResult<TasMovieRow>         DequeueTasMovie(DbEnv& env);
            static DbResult<TasMovieQueueDepths> TasMovieQueueDepthsNow(DbEnv& env);

            static inline std::future<DbResult<SeedProbeRow>>
                DequeueProbeAsync(RetryPolicy rp = {}) {
                return DBService::instance().submit_res<SeedProbeRow>(OpType::Write, Priority::High, rp,
                    [](DbEnv& e) { return DequeueProbe(e); });
            }
            static inline std::future<DbResult<ExplorerRunRow>>
                DequeueRunAsync(RetryPolicy rp = {}) {
                return DBService::instance().submit_res<ExplorerRunRow>(OpType::Write, Priority::High, rp,
                    [](DbEnv& e) { return DequeueRun(e); });
            }
            static inline std::future<DbResult<QueueDepths>>
                QueueDepthsAsync(RetryPolicy rp = {}) {
                return DBService::instance().submit_res<QueueDepths>(OpType::Read, Priority::High, rp,
                    [](DbEnv& e) { return QueueDepthsNow(e); });
            }
            static inline std::future<DbResult<LineageRow>>
                GetLineageAsync(int64_t run_id, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<LineageRow>(OpType::Read, Priority::High, rp,
                    [=](DbEnv& e) { return GetLineage(e, run_id); });
            }
            static inline std::future<DbResult<TasMovieRow>>
                DequeueTasMovieAsync(RetryPolicy rp = {}) {
                return DBService::instance().submit_res<TasMovieRow>(OpType::Write, Priority::High, rp,
                    [](DbEnv& e) { return DequeueTasMovie(e); });
            }
            static inline std::future<DbResult<TasMovieQueueDepths>>
                TasMovieQueueDepthsAsync(RetryPolicy rp = {}) {
                return DBService::instance().submit_res<TasMovieQueueDepths>(OpType::Read, Priority::High, rp,
                    [](DbEnv& e) { return TasMovieQueueDepthsNow(e); });
            }
        };

    }
} // namespace simcore::db
