#include "ObjectStore.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "mbedtls/sha256.h"

namespace fs = std::filesystem;

namespace simcore {
    namespace db {

        static inline DbResult<ObjectRefRow> Impl_Finalize(DbEnv& env,
            const std::string& staged,
            const std::string& objdir,
            Compression comp) {

            unsigned char out[32];
            if (!mbedtls_sha256_ret(reinterpret_cast<const unsigned char*>(staged.c_str()), staged.size(), out, 0)) return DbResult<ObjectRefRow>::Err({ DbErrorKind::IO, rc, "bad hash" });
            std::string hash = std::string(reinterpret_cast<char*>(out), 32);
            auto subdir = objdir + "/" + hash.substr(0, 2) + "/" + hash.substr(2, 2);
            fs::create_directories(subdir);
            auto finalpath = subdir + "/" + hash;
            fs::rename(staged, finalpath);
            uint64_t fsize = fs::file_size(finalpath);

            sqlite3* db = env.handle();
            sqlite3_stmt* ins{};
            int rc = sqlite3_prepare_v2(db,
                "INSERT OR IGNORE INTO object_ref(sha256, compression, size) VALUES (?,?,?);",
                -1, &ins, nullptr);
            if (rc != SQLITE_OK) return DbResult<ObjectRefRow>::Err({ map_sqlite_err(rc), rc, "prepare ins" });
            sqlite3_bind_text(ins, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(ins, 2, (int)comp);
            sqlite3_bind_int64(ins, 3, (sqlite3_int64)fsize);
            rc = sqlite3_step(ins); sqlite3_finalize(ins);
            if (rc != SQLITE_DONE) return DbResult<ObjectRefRow>::Err({ map_sqlite_err(rc), rc, "insert" });

            sqlite3_stmt* sel{};
            rc = sqlite3_prepare_v2(db, "SELECT id, compression, size FROM object_ref WHERE sha256=? LIMIT 1;", -1, &sel, nullptr);
            if (rc != SQLITE_OK) return DbResult<ObjectRefRow>::Err({ map_sqlite_err(rc), rc, "prepare sel" });
            sqlite3_bind_text(sel, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(sel);
            if (rc != SQLITE_ROW) { sqlite3_finalize(sel); return DbResult<ObjectRefRow>::Err({ DbErrorKind::NotFound, rc, "not found after insert" }); }
            ObjectRefRow row;
            row.id = sqlite3_column_int64(sel, 0);
            row.sha256 = hash;
            row.compression = (Compression)sqlite3_column_int(sel, 1);
            row.size = (uint64_t)sqlite3_column_int64(sel, 2);
            sqlite3_finalize(sel);

            return DbResult<ObjectRefRow>::Ok(std::move(row));
        }

        // Async surface

        std::future<DbResult<ObjectRefRow>> ObjectStore::FinalizeAsync(const std::string& staged_path, const std::string& objdir, Compression comp, RetryPolicy rp) {
            return DBService::instance().submit_res<ObjectRefRow>(OpType::Write, Priority::Normal, rp,
                [=](DbEnv& e) { return Impl_Finalize(e, staged_path, objdir, comp); });
        }

    } // namespace db
} // namespace simcore
