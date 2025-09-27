#pragma once
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
            // Async (catalog writes/reads)
            static std::future<DbResult<int64_t>> InsertAsync(const PredicateSpecRow& r, RetryPolicy rp = {});
            static std::future<DbResult<int64_t>> BulkInsertAsync(std::vector<PredicateSpecRow>& rows, RetryPolicy rp = {});
            static std::future<DbResult<PredicateSpecRow>> GetAsync(int64_t id, RetryPolicy rp = {});
            static std::future<DbResult<std::vector<PredicateSpecRow>>> ListByBpAsync(int32_t required_bp, RetryPolicy rp = {});

            // Blocking conveniences
            static inline DbResult<int64_t> Insert(const PredicateSpecRow& r) { return InsertAsync(r).get(); }
            static inline DbResult<int64_t> BulkInsert(std::vector<PredicateSpecRow>& rows) { return BulkInsertAsync(rows).get(); }
            static inline DbResult<PredicateSpecRow> Get(int64_t id) { return GetAsync(id).get(); }
            static inline DbResult<std::vector<PredicateSpecRow>> ListByBp(int32_t bp) { return ListByBpAsync(bp).get(); }
        };

    } // db
} // simcore
