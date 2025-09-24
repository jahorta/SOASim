#include "ObjectStore.h"

#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace simcore {
    namespace db {

        static std::string sha256_stub(const std::string& path) {
            // TODO: replace with real crypto hash
            return "deadbeef";
        }

        ObjectRefRow ObjectStore::finalize(DbEnv::Tx& tx,
            const std::string& staged,
            const std::string& objdir,
            Compression comp) {
            auto hash = sha256_stub(staged);
            auto subdir = objdir + "/" + hash.substr(0, 2) + "/" + hash.substr(2, 2);
            fs::create_directories(subdir);
            auto finalpath = subdir + "/" + hash;
            fs::rename(staged, finalpath);

            sqlite3_stmt* stmt{};
            auto db = tx.handle();
            if (sqlite3_prepare_v2(db,
                "INSERT OR IGNORE INTO object_ref(sha256, compression, size) VALUES (?,?,?);",
                -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error("prepare failed");
            sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, (int)comp);
            sqlite3_bind_int64(stmt, 3, fs::file_size(finalpath));
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return { sqlite3_last_insert_rowid(db), hash, comp, fs::file_size(finalpath) };
        }

    } // namespace db
} // namespace simcore
