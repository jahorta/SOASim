#pragma once
#include "DbResult.h"
#include "DbRetryPolicy.h"
#include "DbService.h"
#include <string>
#include <optional>
#include <cstdint>
#include <future>
#include <vector>

namespace simcore {
    namespace db {

        enum class Compression { None = 0, Zstd = 1, lz4 = 2 };

        struct ObjectRefRow {
            int64_t id{};
            std::string sha256;
            Compression compression{ Compression::None };
            uint64_t size{};
        };

        class ObjectStore {
        public:
            // Async: finalize a staged file into the object store and upsert object_ref
            static std::future<DbResult<ObjectRefRow>> FinalizeAsync(const std::string& staged_path,
                const std::string& objdir,
                Compression comp = Compression::None,
                RetryPolicy rp = {});
            // Blocking
            static inline DbResult<ObjectRefRow> Finalize(const std::string& staged_path, const std::string& objdir, Compression comp = Compression::None) {
                return FinalizeAsync(staged_path, objdir, comp).get();
            }
        };

    } // namespace db
} // namespace simcore
