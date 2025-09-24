#include "BattlePlanAtomRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DbResult<int64_t> BattlePlanAtomRepo::Ensure(DbEnv& env, int32_t wire_version, const std::vector<uint8_t>& wire_bytes,
            int32_t action_type, int32_t actor_slot, int32_t is_prelude,
            int32_t param_item_id, int32_t target_slot) {
            sqlite3* db = env.handle();

            sqlite3_stmt* sel{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id FROM battle_plan_atom WHERE wire_version=? AND wire_bytes=? LIMIT 1;",
                -1, &sel, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare sel" });
            sqlite3_bind_int(sel, 1, wire_version);
            sqlite3_bind_blob(sel, 2, wire_bytes.data(), (int)wire_bytes.size(), SQLITE_TRANSIENT);
            rc = sqlite3_step(sel);
            if (rc == SQLITE_ROW) { int64_t id = sqlite3_column_int64(sel, 0); sqlite3_finalize(sel); return DbResult<int64_t>::Ok(id); }
            sqlite3_finalize(sel);

            sqlite3_stmt* ins{};
            rc = sqlite3_prepare_v2(db,
                "INSERT INTO battle_plan_atom(wire_version, wire_bytes, action_type, actor_slot, is_prelude, param_item_id, target_slot) "
                "VALUES(?,?,?,?,?,?,?);", -1, &ins, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare ins" });
            sqlite3_bind_int(ins, 1, wire_version);
            sqlite3_bind_blob(ins, 2, wire_bytes.data(), (int)wire_bytes.size(), SQLITE_TRANSIENT);
            sqlite3_bind_int(ins, 3, action_type);
            sqlite3_bind_int(ins, 4, actor_slot);
            sqlite3_bind_int(ins, 5, is_prelude);
            sqlite3_bind_int(ins, 6, param_item_id);
            sqlite3_bind_int(ins, 7, target_slot);
            rc = sqlite3_step(ins); sqlite3_finalize(ins);
            if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"insert" });
            return DbResult<int64_t>::Ok(sqlite3_last_insert_rowid(db));
        }

        DbResult<BattlePlanAtomRow> BattlePlanAtomRepo::Get(DbEnv& env, int64_t id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, wire_version, wire_bytes, action_type, actor_slot, is_prelude, param_item_id, target_slot "
                "FROM battle_plan_atom WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<BattlePlanAtomRow>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int64(st, 1, id);
            rc = sqlite3_step(st);
            if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<BattlePlanAtomRow>::Err({ DbErrorKind::Unknown,rc,"not found" }); }
            BattlePlanAtomRow r;
            r.id = sqlite3_column_int64(st, 0);
            r.wire_version = sqlite3_column_int(st, 1);
            const void* p = sqlite3_column_blob(st, 2); int n = sqlite3_column_bytes(st, 2);
            r.wire_bytes.assign((const uint8_t*)p, (const uint8_t*)p + n);
            r.action_type = sqlite3_column_int(st, 3);
            r.actor_slot = sqlite3_column_int(st, 4);
            r.is_prelude = sqlite3_column_int(st, 5);
            r.param_item_id = sqlite3_column_int(st, 6);
            r.target_slot = sqlite3_column_int(st, 7);
            sqlite3_finalize(st);
            return DbResult<BattlePlanAtomRow>::Ok(std::move(r));
        }

    }
} // namespace
