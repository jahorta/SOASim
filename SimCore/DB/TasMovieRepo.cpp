#include "TasMovieRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DbResult<int64_t> TasMovieRepo::Enqueue(DbEnv& env, int64_t base_file_id, std::optional<int64_t> new_rtc, int32_t priority) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT INTO tas_movie(base_file_id, new_rtc, status, priority) "
                "VALUES(?, ?, 'planned', ?);", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int64(st, 1, base_file_id);
            if (new_rtc) sqlite3_bind_int64(st, 3, *new_rtc); else sqlite3_bind_null(st, 3);
            sqlite3_bind_int(st, 4, priority);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"insert" });
            return DbResult<int64_t>::Ok(sqlite3_last_insert_rowid(db));
        }

        DbResult<int64_t> TasMovieRepo::IdempotentEnqueue(DbEnv& env, int64_t base_file_id, std::optional<int64_t> new_rtc, int32_t priority) {
            sqlite3* db = env.handle();
            sqlite3_stmt* sel{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id FROM tas_movie "
                "WHERE base_file_id=? AND (new_rtc=?1) "
                "AND status!='done' "
                "ORDER BY id LIMIT 1;", -1, &sel, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare sel" });
            sqlite3_bind_int64(sel, 1, base_file_id);
            if (new_rtc) sqlite3_bind_int64(sel, 2, *new_rtc); else sqlite3_bind_null(sel, 2);
            rc = sqlite3_step(sel);
            if (rc == SQLITE_ROW) { int64_t id = sqlite3_column_int64(sel, 0); sqlite3_finalize(sel); return DbResult<int64_t>::Ok(id); }
            sqlite3_finalize(sel);
            return Enqueue(env, base_file_id, new_rtc, priority);
        }

        DbResult<void> TasMovieRepo::AppendProgress(DbEnv& env, int64_t tas_movie_id, const std::string& text, int max_bytes) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "UPDATE tas_movie "
                "SET progress_log = "
                "substr(COALESCE(progress_log,'') || CASE WHEN length(COALESCE(progress_log,''))>0 THEN char(10) ELSE '' END || ?1, "
                "      CASE WHEN length(COALESCE(progress_log,'')) + length(?1) + 1 > ?2 THEN "
                "           (length(COALESCE(progress_log,'')) + length(?1) + 2 - ?2) "
                "           ELSE 1 END) "
                "WHERE id=?3;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_text(st, 1, text.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 2, max_bytes);
            sqlite3_bind_int64(st, 3, tas_movie_id);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"update" });
            return DbResult<void>{true};
        }

        DbResult<void> TasMovieRepo::MarkFailed(DbEnv& env, int64_t tas_movie_id, const std::string& err) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "UPDATE tas_movie SET last_error=?, status='planned' WHERE id=? AND status='running';",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_text(st, 1, err.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, tas_movie_id);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"update" });
            return DbResult<void>{true};
        }

        DbResult<void> TasMovieRepo::MarkDone(DbEnv& env, int64_t tas_movie_id, int64_t savestate_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "UPDATE tas_movie SET status='done', completed_at=strftime('%s','now'), output_savestate_id=? WHERE id=?;",
                -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int64(st, 1, savestate_id);
            sqlite3_bind_int64(st, 2, tas_movie_id);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"update" });
            return DbResult<void>{true};
        }

        DbResult<TasMovieRow> TasMovieRepo::Get(DbEnv& env, int64_t id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, base_file_id, new_rtc, status, progress_log, "
                "created_at, started_at, completed_at, attempt_count, last_error, priority, output_savestate_id "
                "FROM tas_movie WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<TasMovieRow>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int64(st, 1, id);
            rc = sqlite3_step(st);
            if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<TasMovieRow>::Err({ DbErrorKind::Unknown,rc,"not found" }); }
            TasMovieRow r;
            r.id = sqlite3_column_int64(st, 0);
            r.base_file_id = sqlite3_column_int64(st, 1);
            if (sqlite3_column_type(st, 3) == SQLITE_NULL) r.new_rtc.reset(); else r.new_rtc = sqlite3_column_int64(st, 3);
            r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
            r.progress_log = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
            r.created_at = sqlite3_column_int64(st, 6);
            if (sqlite3_column_type(st, 7) == SQLITE_NULL) r.started_at.reset(); else r.started_at = sqlite3_column_int64(st, 7);
            if (sqlite3_column_type(st, 8) == SQLITE_NULL) r.completed_at.reset(); else r.completed_at = sqlite3_column_int64(st, 8);
            r.attempt_count = sqlite3_column_int(st, 9);
            if (sqlite3_column_type(st, 10) == SQLITE_NULL) r.last_error.reset(); else r.last_error = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 10)));
            r.priority = sqlite3_column_int(st, 11);
            if (sqlite3_column_type(st, 12) == SQLITE_NULL) r.output_savestate_id.reset(); else r.output_savestate_id = sqlite3_column_int64(st, 12);
            sqlite3_finalize(st);
            return DbResult<TasMovieRow>::Ok(std::move(r));
        }

        DbResult<std::vector<TasMovieRow>> TasMovieRepo::ListPlanned(DbEnv& env) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, base_file_id, new_rtc, status, progress_log, "
                "created_at, started_at, completed_at, attempt_count, last_error, priority, output_savestate_id "
                "FROM tas_movie WHERE status='planned' ORDER BY priority DESC, id;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<TasMovieRow>>::Err({ map_sqlite_err(rc),rc,"prepare" });
            std::vector<TasMovieRow> v;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                TasMovieRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.base_file_id = sqlite3_column_int64(st, 1);
                if (sqlite3_column_type(st, 3) == SQLITE_NULL) r.new_rtc.reset(); else r.new_rtc = sqlite3_column_int64(st, 3);
                r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
                r.progress_log = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
                r.created_at = sqlite3_column_int64(st, 6);
                if (sqlite3_column_type(st, 7) == SQLITE_NULL) r.started_at.reset(); else r.started_at = sqlite3_column_int64(st, 7);
                if (sqlite3_column_type(st, 8) == SQLITE_NULL) r.completed_at.reset(); else r.completed_at = sqlite3_column_int64(st, 8);
                r.attempt_count = sqlite3_column_int(st, 9);
                if (sqlite3_column_type(st, 10) == SQLITE_NULL) r.last_error.reset(); else r.last_error = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 10)));
                r.priority = sqlite3_column_int(st, 11);
                if (sqlite3_column_type(st, 12) == SQLITE_NULL) r.output_savestate_id.reset(); else r.output_savestate_id = sqlite3_column_int64(st, 12);
                v.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<TasMovieRow>>::Err({ map_sqlite_err(rc),rc,"scan" });
            return DbResult<std::vector<TasMovieRow>>::Ok(std::move(v));
        }

        DbResult<std::vector<TasMovieRow>> TasMovieRepo::ListActive(DbEnv& env) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, base_file_id, new_rtc, status, progress_log, "
                "created_at, started_at, completed_at, attempt_count, last_error, priority, output_savestate_id "
                "FROM tas_movie WHERE status IN ('planned','running') ORDER BY priority DESC, id;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<TasMovieRow>>::Err({ map_sqlite_err(rc),rc,"prepare" });
            std::vector<TasMovieRow> v;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                TasMovieRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.base_file_id = sqlite3_column_int64(st, 1);
                if (sqlite3_column_type(st, 3) == SQLITE_NULL) r.new_rtc.reset(); else r.new_rtc = sqlite3_column_int64(st, 3);
                r.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
                r.progress_log = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
                r.created_at = sqlite3_column_int64(st, 6);
                if (sqlite3_column_type(st, 7) == SQLITE_NULL) r.started_at.reset(); else r.started_at = sqlite3_column_int64(st, 7);
                if (sqlite3_column_type(st, 8) == SQLITE_NULL) r.completed_at.reset(); else r.completed_at = sqlite3_column_int64(st, 8);
                r.attempt_count = sqlite3_column_int(st, 9);
                if (sqlite3_column_type(st, 10) == SQLITE_NULL) r.last_error.reset(); else r.last_error = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 10)));
                r.priority = sqlite3_column_int(st, 11);
                if (sqlite3_column_type(st, 12) == SQLITE_NULL) r.output_savestate_id.reset(); else r.output_savestate_id = sqlite3_column_int64(st, 12);
                v.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<TasMovieRow>>::Err({ map_sqlite_err(rc),rc,"scan" });
            return DbResult<std::vector<TasMovieRow>>::Ok(std::move(v));
        }

    }
} // namespace simcore::db
