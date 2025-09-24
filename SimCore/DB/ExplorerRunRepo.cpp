#include "ExplorerRunRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DbResult<int64_t> ExplorerRunRepo::Create(DbEnv& env, int64_t probe_id, int64_t settings_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT INTO explorer_run(probe_id, settings_id, status, progress_log, complete) "
                "VALUES(?, ?, 'planned', '', 0);",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            sqlite3_bind_int64(st, 2, settings_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "insert" });
            return DbResult<int64_t>::Ok(sqlite3_last_insert_rowid(db));
        }

        DbResult<int64_t> ExplorerRunRepo::IdempotentCreate(DbEnv& env, int64_t probe_id, int64_t settings_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id FROM explorer_run WHERE probe_id=? AND settings_id=? AND status!='done' LIMIT 1;",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "prepare sel" });
            sqlite3_bind_int64(st, 1, probe_id);
            sqlite3_bind_int64(st, 2, settings_id);
            rc = sqlite3_step(st);
            if (rc == SQLITE_ROW) {
                int64_t id = sqlite3_column_int64(st, 0);
                sqlite3_finalize(st);
                return DbResult<int64_t>::Ok(id);
            }
            sqlite3_finalize(st);
            return Create(env, probe_id, settings_id);
        }

        DbResult<void> ExplorerRunRepo::MarkRunning(DbEnv& env, int64_t run_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE explorer_run SET status='running' WHERE id=? AND status='planned';", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, run_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<void> ExplorerRunRepo::MarkDone(DbEnv& env, int64_t run_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE explorer_run SET status='done', complete=1 WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, run_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<void> ExplorerRunRepo::AppendProgress(DbEnv& env, int64_t run_id, const std::string& text, int max_bytes) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "UPDATE explorer_run "
                "SET progress_log = "
                "substr(COALESCE(progress_log,'') || CASE WHEN length(COALESCE(progress_log,''))>0 THEN char(10) ELSE '' END || ?1, "
                "      CASE WHEN length(COALESCE(progress_log,'')) + length(?1) + 1 > ?2 THEN "
                "           (length(COALESCE(progress_log,'')) + length(?1) + 2 - ?2) "
                "           ELSE 1 END), "
                "    progress_log = COALESCE(progress_log,'') "
                "WHERE id=?3;",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_text(st, 1, text.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 2, max_bytes);
            sqlite3_bind_int64(st, 3, run_id);
            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc), rc, "update" });
            return DbResult<void>{true};
        }

        DbResult<ExplorerRunRow> ExplorerRunRepo::Get(DbEnv& env, int64_t run_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, probe_id, settings_id, status, progress_log, complete "
                "FROM explorer_run WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<ExplorerRunRow>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, run_id);
            rc = sqlite3_step(st);
            if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<ExplorerRunRow>::Err({ DbErrorKind::Unknown, rc, "not found" }); }
            ExplorerRunRow r;
            r.id = sqlite3_column_int64(st, 0);
            r.probe_id = sqlite3_column_int64(st, 1);
            r.settings_id = sqlite3_column_int64(st, 2);
            r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
            r.progress_log = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
            r.complete = sqlite3_column_int(st, 5);
            sqlite3_finalize(st);
            return DbResult<ExplorerRunRow>::Ok(std::move(r));
        }

        DbResult<std::vector<ExplorerRunRow>> ExplorerRunRepo::ListActiveForProbe(DbEnv& env, int64_t probe_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, probe_id, settings_id, status, progress_log, complete "
                "FROM explorer_run WHERE probe_id=? AND status IN ('planned','running') ORDER BY id;",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<ExplorerRunRow>>::Err({ map_sqlite_err(rc), rc, "prepare" });
            sqlite3_bind_int64(st, 1, probe_id);
            std::vector<ExplorerRunRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                ExplorerRunRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.probe_id = sqlite3_column_int64(st, 1);
                r.settings_id = sqlite3_column_int64(st, 2);
                r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
                r.progress_log = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
                r.complete = sqlite3_column_int(st, 5);
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<ExplorerRunRow>>::Err({ map_sqlite_err(rc), rc, "scan" });
            return DbResult<std::vector<ExplorerRunRow>>::Ok(std::move(out));
        }

    }
} // namespace simcore::db
