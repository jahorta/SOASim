#include "DeltaSeedRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DbResult<int64_t> SeedDeltaRepo::BulkQueue(DbEnv& env, int64_t probe_id, const std::vector<DeltaSpec>& deltas) {
            sqlite3* db = env.handle();
            char* err = nullptr;
            int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err);
            if (rc != SQLITE_OK) { std::string m = err ? err : "begin"; sqlite3_free(err); return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, m }); }

            sqlite3_stmt* st{};
            rc = sqlite3_prepare_v2(db,
                "INSERT INTO seed_delta(probe_id, seed_delta, delta_key, is_grid, is_unique, complete) VALUES(?,?,?,?,?,0);",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) { sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr); return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "prepare" }); }

            int64_t count = 0;
            for (const auto& d : deltas) {
                sqlite3_reset(st); sqlite3_clear_bindings(st);
                sqlite3_bind_int64(st, 1, probe_id);
                sqlite3_bind_int64(st, 2, d.seed_delta);
                sqlite3_bind_blob(st, 3, d.delta_key.data(), (int)d.delta_key.size(), SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 4, d.is_grid);
                sqlite3_bind_int(st, 5, d.is_unique);
                rc = sqlite3_step(st);
                if (rc != SQLITE_DONE) { sqlite3_finalize(st); sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr); return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "insert" }); }
                ++count;
            }
            sqlite3_finalize(st);

            rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
            if (rc != SQLITE_OK) { std::string m = err ? err : "commit"; sqlite3_free(err); return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, m }); }
            return DbResult<int64_t>::Ok(count);
        }

        DbResult<void> SeedDeltaRepo::MarkDone(DbEnv& env, int64_t delta_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE seed_delta SET complete=1 WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, delta_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<std::vector<SeedDeltaRow>> SeedDeltaRepo::ListActive(DbEnv& env, int64_t probe_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, probe_id, seed_delta, delta_key, is_grid, is_unique, complete "
                "FROM seed_delta WHERE probe_id=? AND complete=0 ORDER BY id;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<SeedDeltaRow>>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            std::vector<SeedDeltaRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                SeedDeltaRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.probe_id = sqlite3_column_int64(st, 1);
                r.seed_delta = sqlite3_column_int64(st, 2);
                const void* p = sqlite3_column_blob(st, 3);
                int n = sqlite3_column_bytes(st, 3);
                r.delta_key.assign((const uint8_t*)p, (const uint8_t*)p + n);
                r.is_grid = sqlite3_column_int(st, 4);
                r.is_unique = sqlite3_column_int(st, 5);
                r.complete = sqlite3_column_int(st, 6);
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<SeedDeltaRow>>::Err({ map_sqlite_err(rc), rc, "scan" });
            return DbResult<std::vector<SeedDeltaRow>>::Ok(std::move(out));
        }

        DbResult<std::vector<SeedDeltaRow>> SeedDeltaRepo::ListForProbe(DbEnv& env, int64_t probe_id, bool include_done) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            const char* sql = include_done
                ? "SELECT id, probe_id, seed_delta, delta_key, is_grid, is_unique, complete FROM seed_delta WHERE probe_id=? ORDER BY id;"
                : "SELECT id, probe_id, seed_delta, delta_key, is_grid, is_unique, complete FROM seed_delta WHERE probe_id=? AND complete=0 ORDER BY id;";
            int rc = sqlite3_prepare_v2(db, sql, -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<SeedDeltaRow>>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            std::vector<SeedDeltaRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                SeedDeltaRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.probe_id = sqlite3_column_int64(st, 1);
                r.seed_delta = sqlite3_column_int64(st, 2);
                const void* p = sqlite3_column_blob(st, 3);
                int n = sqlite3_column_bytes(st, 3);
                r.delta_key.assign((const uint8_t*)p, (const uint8_t*)p + n);
                r.is_grid = sqlite3_column_int(st, 4);
                r.is_unique = sqlite3_column_int(st, 5);
                r.complete = sqlite3_column_int(st, 6);
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<SeedDeltaRow>>::Err({ map_sqlite_err(rc), rc, "scan" });
            return DbResult<std::vector<SeedDeltaRow>>::Ok(std::move(out));
        }

    }
} // namespace simcore::db
