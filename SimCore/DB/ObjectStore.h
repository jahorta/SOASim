#pragma once
#include "DbEnv.h"
#include <string>
#include <optional>
#include <cstdint>

namespace simcore {
    namespace db {

        enum class Compression { None = 0, Zstd = 1 };

        struct ObjectRefRow {
            int64_t id{};
            std::string sha256;
            Compression compression{ Compression::None };
            uint64_t size{};
        };

        class ObjectStore {
        public:
            static ObjectRefRow finalize(DbEnv::Tx& tx,
                const std::string& staged_path,
                const std::string& objdir,
                Compression comp = Compression::None);
        };

    } // namespace db
} // namespace simcore
