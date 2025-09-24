#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <string>
#include <optional>

namespace simcore {
    namespace db {

        struct ExplorerSettingsRow {
            int64_t id{};
            std::string name;
            std::string description;
            std::string fingerprint;
        };

        struct ExplorerSettingsRepo {
            static DbResult<int64_t> EnsureByFingerprint(DbEnv& env, const std::string& name,
                const std::string& description,
                const std::string& fingerprint);
            static DbResult<ExplorerSettingsRow> Get(DbEnv& env, int64_t id);
            static DbResult<std::optional<int64_t>> FindByFingerprint(DbEnv& env, const std::string& fingerprint);

            static inline std::future<DbResult<int64_t>>
                EnsureByFingerprintAsync(const std::string& name, const std::string& description, const std::string& fingerprint, RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return EnsureByFingerprint(e, name, description, fingerprint); });
            }
        };

    }
} // namespace simcore::db
