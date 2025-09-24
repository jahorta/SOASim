#include "SeedProbeRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DbResult<int64_t> SeedProbeRepo::Create(DbEnv& env, int64_t savestate_id, int64_t version_id, int64_t neutral_seed) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT INTO seed_probe(savestate_id, version_id, neutral_seed, status, complete) "
                "VALUES(?, ?, ?, 'planned', 0);",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, savestate_id);
            sqlite3_bind_int64(st, 2, version_id);
            sqlite3_bind_int64(st, 3, neutral_seed);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "insert" });
            return DbResult<int64_t>::Ok(sqlite3_last_insert_rowid(db));
        }

        DbResult<void> SeedProbeRepo::MarkRunning(DbEnv& env, int64_t probe_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE seed_probe SET status='running' WHERE id=? AND status='planned';", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<void> SeedProbeRepo::SetNeutralSeed(DbEnv& env, int64_t probe_id, int64_t neutral_seed) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE seed_probe SET neutral_seed=? WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, neutral_seed);
            sqlite3_bind_int64(st, 2, probe_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<void> SeedProbeRepo::MarkDone(DbEnv& env, int64_t probe_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE seed_probe SET status='done', complete=1 WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<SeedProbeRow> SeedProbeRepo::Get(DbEnv& env, int64_t probe_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, savestate_id, version_id, neutral_seed, status, complete "
                "FROM seed_probe WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<SeedProbeRow>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            rc = sqlite3_step(st);
            if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<SeedProbeRow>::Err({ DbErrorKind::Unknown, rc, "not found" }); }
            SeedProbeRow row;
            row.id = sqlite3_column_int64(st, 0);
            row.savestate_id = sqlite3_column_int64(st, 1);
            row.version_id = sqlite3_column_int64(st, 2);
            row.neutral_seed = sqlite3_column_int64(st, 3);
            row.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
            row.complete = sqlite3_column_int(st, 5);
            sqlite3_finalize(st);
            return DbResult<SeedProbeRow>::Ok(std::move(row));
        }

        DbResult<std::vector<SeedProbeRow>> SeedProbeRepo::ListActiveForSavestate(DbEnv& env, int64_t savestate_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, savestate_id, version_id, neutral_seed, status, complete "
                "FROM seed_probe WHERE savestate_id=? AND status IN ('planned','running') "
                "ORDER BY id;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<SeedProbeRow>>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, savestate_id);
            std::vector<SeedProbeRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                SeedProbeRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.savestate_id = sqlite3_column_int64(st, 1);
                r.version_id = sqlite3_column_int64(st, 2);
                r.neutral_seed = sqlite3_column_int64(st, 3);
                r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
                r.complete = sqlite3_column_int(st, 5);
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<SeedProbeRow>>::Err({ map_sqlite_err(rc), rc, "scan" });
            return DbResult<std::vector<SeedProbeRow>>::Ok(std::move(out));
        }

        DbResult<std::vector<SeedProbeRow>> SeedProbeRepo::ListPlanned(DbEnv& env) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, savestate_id, version_id, neutral_seed, status, complete "
                "FROM seed_probe WHERE status='planned' ORDER BY id;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<SeedProbeRow>>::Err({ map_sqlite_err(rc), rc, "prepare" });
            std::vector<SeedProbeRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                SeedProbeRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.savestate_id = sqlite3_column_int64(st, 1);
                r.version_id = sqlite3_column_int64(st, 2);
                r.neutral_seed = sqlite3_column_int64(st, 3);
                r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
                r.complete = sqlite3_column_int(st, 5);
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<SeedProbeRow>>::Err({ map_sqlite_err(rc), rc, "scan" });
            return DbResult<std::vector<SeedProbeRow>>::Ok(std::move(out));
        }

    }
} // namespace simcore::db
