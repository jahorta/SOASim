#pragma once
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
            static std::future<DbResult<void>> UpsertTurnAsync(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, RetryPolicy rp = {});
            static std::future<DbResult<void>> UpsertTurnActorAsync(int64_t settings_id, int32_t turn_index, int32_t actor_index, int64_t atom_id, RetryPolicy rp = {});
            static std::future<DbResult<void>> ReplaceTurnAsync(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, std::vector<TurnActorBinding> actors, RetryPolicy rp = {});
            static std::future<DbResult<std::vector<BattlePlanTurnRow>>> LoadTurnsAsync(int64_t settings_id, RetryPolicy rp = {});

            static inline DbResult<void> UpsertTurn(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count) {
                return UpsertTurnAsync(settings_id, turn_index, fake_atk_count).get();
            }
            static inline DbResult<void> UpsertTurnActor(int64_t settings_id, int32_t turn_index, int32_t actor_index, int64_t atom_id) {
                return UpsertTurnActorAsync(settings_id, turn_index, actor_index, atom_id).get();
            }
            static inline DbResult<void> ReplaceTurn(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, std::vector<TurnActorBinding> actors) {
                return ReplaceTurnAsync(settings_id, turn_index, fake_atk_count, std::move(actors)).get();
            }
            static inline DbResult<std::vector<BattlePlanTurnRow>> LoadTurns(int64_t settings_id) {
                return LoadTurnsAsync(settings_id).get();
            }
        };

    } // namespace db
} // namespace simcore
