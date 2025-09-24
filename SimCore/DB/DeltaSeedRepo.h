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

        struct SeedDeltaRow {
            int64_t id{};
            int64_t probe_id{};
            int64_t seed_delta{};
            std::vector<uint8_t> delta_key; // GCInputFrame stored as BLOB
            int32_t is_grid{};
            int32_t is_unique{};
            int32_t complete{};
        };

        struct SeedDeltaRepo {
            struct DeltaSpec { int64_t seed_delta; std::vector<uint8_t> delta_key; int32_t is_grid; int32_t is_unique; };

            static DbResult<int64_t> BulkQueue(DbEnv& env, int64_t probe_id, const std::vector<DeltaSpec>& deltas);
            static DbResult<void>    MarkDone(DbEnv& env, int64_t delta_id);
            static DbResult<std::vector<SeedDeltaRow>> ListActive(DbEnv& env, int64_t probe_id);
            static DbResult<std::vector<SeedDeltaRow>> ListForProbe(DbEnv& env, int64_t probe_id, bool include_done);

            static inline std::future<DbResult<int64_t>>
                BulkQueueAsync(int64_t probe_id, std::vector<DeltaSpec> deltas, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [probe_id, deltas = std::move(deltas)](DbEnv& e) { return BulkQueue(e, probe_id, deltas); });
            }
            static inline std::future<DbResult<void>>
                MarkDoneAsync(int64_t delta_id, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return MarkDone(e, delta_id); });
            }
        };

    }
} // namespace simcore::db
