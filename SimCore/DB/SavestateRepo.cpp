#include "SavestateRepo.h"
#include <sqlite3.h>
#include <stdexcept>

namespace simcore {
    namespace db {

        int64_t SavestateRepo::plan(DbEnv::Tx& tx, int savestate_type, const std::string& note) {
            sqlite3_stmt* stmt{};
            auto db = tx.handle();
            sqlite3_prepare_v2(db, "INSERT INTO savestate(savestate_type, note, complete) VALUES (?,?,0);",
                -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, savestate_type);
            sqlite3_bind_text(stmt, 2, note.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            return sqlite3_last_insert_rowid(db);
        }

        void SavestateRepo::finalize(DbEnv::Tx& tx, int64_t id, int64_t object_ref_id) {
            sqlite3_stmt* stmt{};
            auto db = tx.handle();
            sqlite3_prepare_v2(db, "UPDATE savestate SET object_ref_id=?, complete=1 WHERE id=?;",
                -1, &stmt, nullptr);
            sqlite3_bind_int64(stmt, 1, object_ref_id);
            sqlite3_bind_int64(stmt, 2, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        std::optional<SavestateRow> SavestateRepo::get(DbEnv::Tx& tx, int64_t id) {
            sqlite3_stmt* stmt{};
            auto db = tx.handle();
            sqlite3_prepare_v2(db, "SELECT id, savestate_type, note, object_ref_id, complete FROM savestate WHERE id=?;",
                -1, &stmt, nullptr);
            sqlite3_bind_int64(stmt, 1, id);
            auto rc = sqlite3_step(stmt);
            if (rc != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                return std::nullopt;
            }
            SavestateRow row;
            row.id = sqlite3_column_int64(stmt, 0);
            row.savestate_type = sqlite3_column_int(stmt, 1);
            row.note = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            row.object_ref_id = sqlite3_column_int64(stmt, 3);
            row.complete = sqlite3_column_int(stmt, 4) != 0;
            sqlite3_finalize(stmt);
            return row;
        }

    } // namespace db
} // namespace simcore
