#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <string>
#include <vector>
#include <cstdint>

namespace simcore {
    namespace db {

        struct SeedProbeRow {
            int64_t id{};
            int64_t savestate_id{};
            int64_t version_id{};
            int64_t neutral_seed{};
            std::string status; // planned|running|done
            int32_t complete{}; // if present in schema
        };

        struct SeedProbeRepo {
            static DbResult<int64_t> Create(DbEnv& env, int64_t savestate_id, int64_t version_id, int64_t neutral_seed);
            static DbResult<void>    MarkRunning(DbEnv& env, int64_t probe_id);
            static DbResult<void>    SetNeutralSeed(DbEnv& env, int64_t probe_id, int64_t neutral_seed);
            static DbResult<void>    MarkDone(DbEnv& env, int64_t probe_id);
            static DbResult<SeedProbeRow> Get(DbEnv& env, int64_t probe_id);
            static DbResult<std::vector<SeedProbeRow>> ListActiveForSavestate(DbEnv& env, int64_t savestate_id);
            static DbResult<std::vector<SeedProbeRow>> ListPlanned(DbEnv& env);

            static inline std::future<DbResult<int64_t>>
                CreateAsync(int64_t savestate_id, int64_t version_id, int64_t neutral_seed, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return Create(e, savestate_id, version_id, neutral_seed); });
            }
            static inline std::future<DbResult<void>>
                MarkRunningAsync(int64_t probe_id, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<void>(OpType::Write, Priority::High, rp,
                    [=](DbEnv& e) { return MarkRunning(e, probe_id); });
            }
            static inline std::future<DbResult<void>>
                MarkDoneAsync(int64_t probe_id, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return MarkDone(e, probe_id); });
            }
            static inline std::future<DbResult<std::vector<SeedProbeRow>>>
                ListPlannedAsync(RetryPolicy rp = {}) {
                return DBService::instance().submit_res<std::vector<SeedProbeRow>>(OpType::Read, Priority::High, rp,
                    [](DbEnv& e) { return ListPlanned(e); });
            }
        };

    }
} // namespace simcore::db
