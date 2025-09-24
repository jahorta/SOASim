#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>

namespace simcore {
    namespace db {

        struct PredicateSpecRow {
            int64_t id{};
            int32_t spec_version{};
            int32_t pred_id{};
            int32_t required_bp{};
            int32_t kind{};
            int32_t width{};
            int32_t cmp_op{};
            int32_t flags{};
            int64_t lhs_addr{};
            std::optional<int32_t> lhs_key;
            int64_t rhs_value{};
            std::optional<int32_t> rhs_key;
            int32_t turn_mask{};
            std::optional<int64_t> lhs_prog_id;
            std::optional<int64_t> rhs_prog_id;
            std::string description;
        };

        struct PredicateSpecRepo {
            static DbResult<int64_t> Insert(DbEnv& env, const PredicateSpecRow& r);
            static DbResult<int64_t> BulkInsert(DbEnv& env, const std::vector<PredicateSpecRow>& rows);
            static DbResult<PredicateSpecRow> Get(DbEnv& env, int64_t id);
            static DbResult<std::vector<PredicateSpecRow>> ListByBp(DbEnv& env, int32_t required_bp);
            static DbResult<std::vector<PredicateSpecRow>> ListByPredId(DbEnv& env, int32_t pred_id);
        };

    }
} // namespace simcore::db
