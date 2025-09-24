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

        struct BattlePlanAtomRow {
            int64_t id{};
            int32_t wire_version{};
            std::vector<uint8_t> wire_bytes;
            int32_t action_type{};
            int32_t actor_slot{};
            int32_t is_prelude{};
            int32_t param_item_id{};
            int32_t target_slot{};
        };

        struct BattlePlanAtomRepo {
            static DbResult<int64_t> Ensure(DbEnv& env,
                int32_t wire_version,
                const std::vector<uint8_t>& wire_bytes,
                int32_t action_type,
                int32_t actor_slot,
                int32_t is_prelude,
                int32_t param_item_id,
                int32_t target_slot);

            static DbResult<BattlePlanAtomRow> Get(DbEnv& env, int64_t id);

            static inline std::future<DbResult<int64_t>>
                EnsureAsync(int32_t wire_version, std::vector<uint8_t> wire_bytes, int32_t action_type,
                    int32_t actor_slot, int32_t is_prelude, int32_t param_item_id, int32_t target_slot,
                    RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [=, wb = std::move(wire_bytes)](DbEnv& e) { return Ensure(e, wire_version, wb, action_type, actor_slot, is_prelude, param_item_id, target_slot); });
            }
        };

    }
} // namespace simcore::db
