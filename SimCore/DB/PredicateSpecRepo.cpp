#include "PredicateSpecRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        static void bind_spec(sqlite3_stmt* st, const PredicateSpecRow& r) {
            sqlite3_bind_int(st, 1, r.spec_version);
            sqlite3_bind_int(st, 2, r.pred_id);
            sqlite3_bind_int(st, 3, r.required_bp);
            sqlite3_bind_int(st, 4, r.kind);
            sqlite3_bind_int(st, 5, r.width);
            sqlite3_bind_int(st, 6, r.cmp_op);
            sqlite3_bind_int(st, 7, r.flags);
            sqlite3_bind_int64(st, 8, r.lhs_addr);
            if (r.lhs_key) sqlite3_bind_int(st, 9, *r.lhs_key); else sqlite3_bind_null(st, 9);
            sqlite3_bind_int64(st, 10, r.rhs_value);
            if (r.rhs_key) sqlite3_bind_int(st, 11, *r.rhs_key); else sqlite3_bind_null(st, 11);
            sqlite3_bind_int(st, 12, r.turn_mask);
            if (r.lhs_prog_id) sqlite3_bind_int64(st, 13, *r.lhs_prog_id); else sqlite3_bind_null(st, 13);
            if (r.rhs_prog_id) sqlite3_bind_int64(st, 14, *r.rhs_prog_id); else sqlite3_bind_null(st, 14);
            sqlite3_bind_text(st, 15, r.description.c_str(), -1, SQLITE_TRANSIENT);
        }

        DbResult<int64_t> PredicateSpecRepo::Insert(DbEnv& env, const PredicateSpecRow& r) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT INTO predicate_spec(spec_version,pred_id,required_bp,kind,width,cmp_op,flags,"
                "lhs_addr,lhs_key,rhs_value,rhs_key,turn_mask,lhs_prog_id,rhs_prog_id,desc)"
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare" });
            bind_spec(st, r);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"insert" });
            return DbResult<int64_t>::Ok(sqlite3_last_insert_rowid(db));
        }

        DbResult<int64_t> PredicateSpecRepo::BulkInsert(DbEnv& env, const std::vector<PredicateSpecRow>& rows) {
            sqlite3* db = env.handle();
            char* err = nullptr;
            int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err);
            if (rc != SQLITE_OK) { std::string m = err ? err : "begin"; sqlite3_free(err); return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,m }); }
            sqlite3_stmt* st{};
            rc = sqlite3_prepare_v2(db,
                "INSERT INTO predicate_spec(spec_version,pred_id,required_bp,kind,width,cmp_op,flags,"
                "lhs_addr,lhs_key,rhs_value,rhs_key,turn_mask,lhs_prog_id,rhs_prog_id,desc)"
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);", -1, &st, nullptr);
            if (rc != SQLITE_OK) { sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr); return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare" }); }
            int64_t cnt = 0;
            for (const auto& r : rows) {
                sqlite3_reset(st); sqlite3_clear_bindings(st);
                bind_spec(st, r);
                rc = sqlite3_step(st);
                if (rc != SQLITE_DONE) { sqlite3_finalize(st); sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr); return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"insert" }); }
                ++cnt;
            }
            sqlite3_finalize(st);
            rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
            if (rc != SQLITE_OK) { std::string m = err ? err : "commit"; sqlite3_free(err); return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,m }); }
            return DbResult<int64_t>::Ok(cnt);
        }

        DbResult<PredicateSpecRow> PredicateSpecRepo::Get(DbEnv& env, int64_t id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id,spec_version,pred_id,required_bp,kind,width,cmp_op,flags,lhs_addr,lhs_key,rhs_value,rhs_key,turn_mask,lhs_prog_id,rhs_prog_id,desc "
                "FROM predicate_spec WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<PredicateSpecRow>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int64(st, 1, id);
            rc = sqlite3_step(st);
            if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<PredicateSpecRow>::Err({ DbErrorKind::Unknown,rc,"not found" }); }
            PredicateSpecRow r;
            r.id = sqlite3_column_int64(st, 0);
            r.spec_version = sqlite3_column_int(st, 1);
            r.pred_id = sqlite3_column_int(st, 2);
            r.required_bp = sqlite3_column_int(st, 3);
            r.kind = sqlite3_column_int(st, 4);
            r.width = sqlite3_column_int(st, 5);
            r.cmp_op = sqlite3_column_int(st, 6);
            r.flags = sqlite3_column_int(st, 7);
            r.lhs_addr = sqlite3_column_int64(st, 8);
            if (sqlite3_column_type(st, 9) == SQLITE_NULL) r.lhs_key.reset(); else r.lhs_key = sqlite3_column_int(st, 9);
            r.rhs_value = sqlite3_column_int64(st, 10);
            if (sqlite3_column_type(st, 11) == SQLITE_NULL) r.rhs_key.reset(); else r.rhs_key = sqlite3_column_int(st, 11);
            r.turn_mask = sqlite3_column_int(st, 12);
            if (sqlite3_column_type(st, 13) == SQLITE_NULL) r.lhs_prog_id.reset(); else r.lhs_prog_id = sqlite3_column_int64(st, 13);
            if (sqlite3_column_type(st, 14) == SQLITE_NULL) r.rhs_prog_id.reset(); else r.rhs_prog_id = sqlite3_column_int64(st, 14);
            r.description = reinterpret_cast<const char*>(sqlite3_column_text(st, 15));
            sqlite3_finalize(st);
            return DbResult<PredicateSpecRow>::Ok(std::move(r));
        }

        DbResult<std::vector<PredicateSpecRow>> PredicateSpecRepo::ListByBp(DbEnv& env, int32_t required_bp) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id,spec_version,pred_id,required_bp,kind,width,cmp_op,flags,lhs_addr,lhs_key,rhs_value,rhs_key,turn_mask,lhs_prog_id,rhs_prog_id,desc "
                "FROM predicate_spec WHERE required_bp=? ORDER BY pred_id;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<PredicateSpecRow>>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int(st, 1, required_bp);
            std::vector<PredicateSpecRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                PredicateSpecRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.spec_version = sqlite3_column_int(st, 1);
                r.pred_id = sqlite3_column_int(st, 2);
                r.required_bp = sqlite3_column_int(st, 3);
                r.kind = sqlite3_column_int(st, 4);
                r.width = sqlite3_column_int(st, 5);
                r.cmp_op = sqlite3_column_int(st, 6);
                r.flags = sqlite3_column_int(st, 7);
                r.lhs_addr = sqlite3_column_int64(st, 8);
                if (sqlite3_column_type(st, 9) == SQLITE_NULL) r.lhs_key.reset(); else r.lhs_key = sqlite3_column_int(st, 9);
                r.rhs_value = sqlite3_column_int64(st, 10);
                if (sqlite3_column_type(st, 11) == SQLITE_NULL) r.rhs_key.reset(); else r.rhs_key = sqlite3_column_int(st, 11);
                r.turn_mask = sqlite3_column_int(st, 12);
                if (sqlite3_column_type(st, 13) == SQLITE_NULL) r.lhs_prog_id.reset(); else r.lhs_prog_id = sqlite3_column_int64(st, 13);
                if (sqlite3_column_type(st, 14) == SQLITE_NULL) r.rhs_prog_id.reset(); else r.rhs_prog_id = sqlite3_column_int64(st, 14);
                r.description = reinterpret_cast<const char*>(sqlite3_column_text(st, 15));
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<PredicateSpecRow>>::Err({ map_sqlite_err(rc),rc,"scan" });
            return DbResult<std::vector<PredicateSpecRow>>::Ok(std::move(out));
        }

        DbResult<std::vector<PredicateSpecRow>> PredicateSpecRepo::ListByPredId(DbEnv& env, int32_t pred_id) {
            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db,
                "SELECT id,spec_version,pred_id,required_bp,kind,width,cmp_op,flags,lhs_addr,lhs_key,rhs_value,rhs_key,turn_mask,lhs_prog_id,rhs_prog_id,desc "
                "FROM predicate_spec WHERE pred_id=? ORDER BY required_bp;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<std::vector<PredicateSpecRow>>::Err({ map_sqlite_err(rc),rc,"prepare" });
            sqlite3_bind_int(st, 1, pred_id);
            std::vector<PredicateSpecRow> out;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                PredicateSpecRow r;
                r.id = sqlite3_column_int64(st, 0);
                r.spec_version = sqlite3_column_int(st, 1);
                r.pred_id = sqlite3_column_int(st, 2);
                r.required_bp = sqlite3_column_int(st, 3);
                r.kind = sqlite3_column_int(st, 4);
                r.width = sqlite3_column_int(st, 5);
                r.cmp_op = sqlite3_column_int(st, 6);
                r.flags = sqlite3_column_int(st, 7);
                r.lhs_addr = sqlite3_column_int64(st, 8);
                if (sqlite3_column_type(st, 9) == SQLITE_NULL) r.lhs_key.reset(); else r.lhs_key = sqlite3_column_int(st, 9);
                r.rhs_value = sqlite3_column_int64(st, 10);
                if (sqlite3_column_type(st, 11) == SQLITE_NULL) r.rhs_key.reset(); else r.rhs_key = sqlite3_column_int(st, 11);
                r.turn_mask = sqlite3_column_int(st, 12);
                if (sqlite3_column_type(st, 13) == SQLITE_NULL) r.lhs_prog_id.reset(); else r.lhs_prog_id = sqlite3_column_int64(st, 13);
                if (sqlite3_column_type(st, 14) == SQLITE_NULL) r.rhs_prog_id.reset(); else r.rhs_prog_id = sqlite3_column_int64(st, 14);
                r.description = reinterpret_cast<const char*>(sqlite3_column_text(st, 15));
                out.push_back(std::move(r));
            }
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<std::vector<PredicateSpecRow>>::Err({ map_sqlite_err(rc),rc,"scan" });
            return DbResult<std::vector<PredicateSpecRow>>::Ok(std::move(out));
        }

    }
} // namespace
