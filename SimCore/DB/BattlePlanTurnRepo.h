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

        struct TurnActorBinding { int32_t actor_index; int64_t atom_id; };
        struct BattlePlanTurnRow { int64_t settings_id; int32_t turn_index; int32_t fake_atk_count; std::vector<TurnActorBinding> actors; };

        struct BattlePlanTurnRepo {
            static DbResult<void> UpsertTurn(DbEnv& env, int64_t settings_id, int32_t turn_index, int32_t fake_atk_count);
            static DbResult<void> UpsertTurnActor(DbEnv& env, int64_t settings_id, int32_t turn_index, int32_t actor_index, int64_t atom_id);
            static DbResult<void> ReplaceTurn(DbEnv& env, int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, const std::vector<TurnActorBinding>& actors);
            static DbResult<std::vector<BattlePlanTurnRow>> LoadTurns(DbEnv& env, int64_t settings_id);

            static inline std::future<DbResult<void>>
                ReplaceTurnAsync(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, std::vector<TurnActorBinding> actors, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                    [=, as = std::move(actors)](DbEnv& e) { return ReplaceTurn(e, settings_id, turn_index, fake_atk_count, as); });
            }
        };

    }
} // namespace simcore::db
