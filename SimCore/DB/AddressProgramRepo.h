#pragma once
#include "DBCore/DbEnv.h"
#include "DBCore/DbResult.h"
#include "DBCore/DbRetryPolicy.h"
#include "DBCore/DbService.h"
#include <future>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace simcore {
    namespace db {

        struct AddressProgramRow {
            int64_t id{};
            int32_t program_version{};
            std::vector<uint8_t> prog_bytes;
            std::optional<int32_t> derived_buffer_version;
            std::optional<std::string> derived_buffer_schema_hash;
            std::optional<std::string> soa_structs_hash;
            std::string description;
        };

        struct AddressProgramRepo {
            static DbResult<int64_t> Ensure(DbEnv& env,
                int32_t program_version,
                const std::vector<uint8_t>& prog_bytes,
                std::optional<int32_t> derived_buffer_version,
                std::optional<std::string> derived_buffer_schema_hash,
                std::optional<std::string> soa_structs_hash,
                const std::string& description);

            static DbResult<AddressProgramRow> Get(DbEnv& env, int64_t id);
        };

    }
} // namespace simcore::db
