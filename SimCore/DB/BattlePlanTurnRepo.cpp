#include "BattlePlanTurnRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        static inline DbResult<void> Impl_UpsertTurn(DbEnv& env, int64_t settings_id, int32_t turn_index, int32_t fake_atk_count) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT INTO battle_plan_turn(settings_id, turn_index, fake_atk_count) VALUES(?,?,?) "
                "ON CONFLICT(settings_id, turn_index) DO UPDATE SET fake_atk_count=excluded.fake_atk_count;",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, settings_id);
            sqlite3_bind_int(st, 2, turn_index);
            sqlite3_bind_int(st, 3, fake_atk_count);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "upsert" });
            return DbResult<void>{ true };
        }

        static inline DbResult<void> Impl_UpsertTurnActor(DbEnv& env, int64_t settings_id, int32_t turn_index, int32_t actor_index, int64_t atom_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT INTO battle_plan_turn_actor(settings_id, turn_index, actor_index, atom_id) VALUES(?,?,?,?) "
                "ON CONFLICT(settings_id, turn_index, actor_index) DO UPDATE SET atom_id=excluded.atom_id;",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, settings_id);
            sqlite3_bind_int(st, 2, turn_index);
            sqlite3_bind_int(st, 3, actor_index);
            sqlite3_bind_int64(st, 4, atom_id);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "upsert" });
            return DbResult<void>{ true };
        }

        static inline DbResult<void> Impl_ReplaceTurn(DbEnv& env, int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, const std::vector<TurnActorBinding>& actors) {
            auto r = Impl_UpsertTurn(env, settings_id, turn_index, fake_atk_count);
            if (!r.ok) return r;

            sqlite3* db = env.handle();
            sqlite3_stmt* del{};
            int rc = sqlite3_prepare_v2(db, "DELETE FROM battle_plan_turn_actor WHERE settings_id=? AND turn_index=?;", -1, &del, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare del" });
            sqlite3_bind_int64(del, 1, settings_id);
            sqlite3_bind_int(del, 2, turn_index);
            rc = sqlite3_step(del); sqlite3_finalize(del);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "delete" });

            sqlite3_stmt* ins{};
            rc = sqlite3_prepare_v2(db,
                "INSERT INTO battle_plan_turn_actor(settings_id, turn_index, actor_index, atom_id) VALUES(?,?,?,?);",
                -1, &ins, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare ins" });

            for (const auto& a : actors) {
                sqlite3_reset(ins); sqlite3_clear_bindings(ins);
                sqlite3_bind_int64(ins, 1, settings_id);
                sqlite3_bind_int(ins, 2, turn_index);
                sqlite3_bind_int(ins, 3, a.actor_index);
                sqlite3_bind_int64(ins, 4, a.atom_id);
                rc = sqlite3_step(ins);
                if (rc != SQLITE_DONE) { sqlite3_finalize(ins); return DbResult<void>::Err({ map_sqlite_err(rc), rc, "insert actors" }); }
            }
            sqlite3_finalize(ins);
            return DbResult<void>{ true };
        }

        static inline DbResult<std::vector<BattlePlanTurnRow>> Impl_LoadTurns(DbEnv& env, int64_t settings_id) {
            sqlite3* db = env.handle();

            sqlite3_stmt* ts{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT turn_index, fake_atk_count FROM battle_plan_turn WHERE settings_id=? ORDER BY turn_index;",
                -1, &ts, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<BattlePlanTurnRow>>::Err({ map_sqlite_err(rc), rc, "prepare turns" });
            sqlite3_bind_int64(ts, 1, settings_id);

            std::vector<BattlePlanTurnRow> rows;
            while ((rc = sqlite3_step(ts)) == SQLITE_ROW) {
                BattlePlanTurnRow r;
                r.settings_id = settings_id;
                r.turn_index = sqlite3_column_int(ts, 0);
                r.fake_atk_count = sqlite3_column_int(ts, 1);
                rows.push_back(std::move(r));
            }
            sqlite3_finalize(ts);
            if (rc != SQLITE_DONE) return DbResult<std::vector<BattlePlanTurnRow>>::Err({ map_sqlite_err(rc), rc, "scan turns" });

            sqlite3_stmt* as{};
            rc = sqlite3_prepare_v2(db,
                "SELECT turn_index, actor_index, atom_id FROM battle_plan_turn_actor WHERE settings_id=? ORDER BY turn_index, actor_index;",
                -1, &as, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<BattlePlanTurnRow>>::Err({ map_sqlite_err(rc), rc, "prepare actors" });
            sqlite3_bind_int64(as, 1, settings_id);

            while ((rc = sqlite3_step(as)) == SQLITE_ROW) {
                int t = sqlite3_column_int(as, 0);
                TurnActorBinding b{ sqlite3_column_int(as, 1), sqlite3_column_int64(as, 2) };
                for (auto& r : rows) { if (r.turn_index == t) { r.actors.push_back(b); break; } }
            }
            sqlite3_finalize(as);
            if (rc != SQLITE_DONE) return DbResult<std::vector<BattlePlanTurnRow>>::Err({ map_sqlite_err(rc), rc, "scan actors" });

            return DbResult<std::vector<BattlePlanTurnRow>>::Ok(std::move(rows));
        }

        // Async

        std::future<DbResult<void>> BattlePlanTurnRepo::UpsertTurnAsync(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, RetryPolicy rp) {
            return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                [=](DbEnv& e) { return Impl_UpsertTurn(e, settings_id, turn_index, fake_atk_count); });
        }

        std::future<DbResult<void>> BattlePlanTurnRepo::UpsertTurnActorAsync(int64_t settings_id, int32_t turn_index, int32_t actor_index, int64_t atom_id, RetryPolicy rp) {
            return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                [=](DbEnv& e) { return Impl_UpsertTurnActor(e, settings_id, turn_index, actor_index, atom_id); });
        }

        std::future<DbResult<void>> BattlePlanTurnRepo::ReplaceTurnAsync(int64_t settings_id, int32_t turn_index, int32_t fake_atk_count, std::vector<TurnActorBinding> actors, RetryPolicy rp) {
            return DBService::instance().submit_res<void>(OpType::Write, Priority::Normal, rp,
                [=, as = std::move(actors)](DbEnv& e) { return Impl_ReplaceTurn(e, settings_id, turn_index, fake_atk_count, as); });
        }

        std::future<DbResult<std::vector<BattlePlanTurnRow>>> BattlePlanTurnRepo::LoadTurnsAsync(int64_t settings_id, RetryPolicy rp) {
            return DBService::instance().submit_res<std::vector<BattlePlanTurnRow>>(OpType::Read, Priority::Normal, rp,
                [=](DbEnv& e) { return Impl_LoadTurns(e, settings_id); });
        }

    } // namespace db
} // namespace simcore
