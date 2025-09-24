#pragma once
#include "DbEnv.h"
#include "MigrationRunner.h"
#include "GeneratedMigrations.h"
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace simcore::db {
    inline int ApplyEmbeddedMigrations(DbEnv& env) {
        sqlite3* db = env.handle();
        for (const auto& kv : kEmbeddedMigrations) {
            const auto& name = kv.first;
            const auto& sql = kv.second;
            char* err = nullptr;
            if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
                std::string msg = err ? err : "BEGIN failed"; sqlite3_free(err); throw std::runtime_error(msg);
            }
            if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
                std::string msg = "Migration failed (" + name + "): " + (err ? err : "");
                sqlite3_free(err); sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr); throw std::runtime_error(msg);
            }
            if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
                std::string msg = err ? err : "COMMIT failed"; sqlite3_free(err); throw std::runtime_error(msg);
            }
        }
        return GetCurrentSchemaVersion(env);
    }
} // namespace simcore::db
