#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <string>

namespace simcore {
    namespace db {

        struct VersionStampRow {
            int64_t id{};
            std::string dolphin_build_hash;
            std::string wrapper_commit;
            std::string simcore_commit;
            std::string vm_opcode_hash;
        };

        struct VersionStampRepo {
            static DbResult<int64_t> Ensure(DbEnv& env,
                const std::string& dolphin_build_hash,
                const std::string& wrapper_commit,
                const std::string& simcore_commit,
                const std::string& vm_opcode_hash);

            static DbResult<VersionStampRow> Get(DbEnv& env, int64_t id);

            static inline std::future<DbResult<int64_t>>
                EnsureAsync(const std::string& dolphin_build_hash,
                    const std::string& wrapper_commit,
                    const std::string& simcore_commit,
                    const std::string& vm_opcode_hash,
                    RetryPolicy rp = {}) {
                return DBService::instance().submit_res<int64_t>(OpType::Write, Priority::Normal, rp,
                    [=](DbEnv& e) { return Ensure(e, dolphin_build_hash, wrapper_commit, simcore_commit, vm_opcode_hash); });
            }
        };

    }
} // namespace simcore::db
