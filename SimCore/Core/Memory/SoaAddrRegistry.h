#pragma once
#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include "MemView.h"
#include "SoaAddr.def.h"

namespace addr {

    enum class Region : uint8_t { MEM1 = 1, MEM2 = 2 };
    enum class AddrMode : uint8_t { Raw = 0, PtrChain = 1 };
    enum class Width : uint8_t { U8 = 1, U16 = 2, U32 = 4, U64 = 8 };

    struct DolphinAddr {
        Region     region;
        AddrMode   mode;
        uint32_t   base;         // For PtrChain this can be 0 and supplied at resolve time
        Width      width;
        const int32_t* offsets;  // PtrChain steps; Raw => nullptr
        uint8_t    offsets_len;  // PtrChain count; Raw => 0
    };

    enum class AddrKey : uint16_t {
#define MK_ENUM(dom, NAME, R, M, B, W, ...) dom##_##NAME,
        ADDR_TABLE_ALL(MK_ENUM)
#undef MK_ENUM
    };

    struct AddrRec {
        AddrKey     key;
        DolphinAddr spec;
        const char* name;        // "dom.NAME"
    };

    class Registry {
    public:
        static std::span<const AddrRec> all();
        static const AddrRec* find(AddrKey k);
        static const DolphinAddr& spec(AddrKey k);
        static const char* name(AddrKey k);

        // Resolve to final VA. For PtrChain with spec.base==0, this fails—use resolve_from_base.
        static bool resolve(const MemView& view, const DolphinAddr& a, uint32_t& out_va);

        // Resolve when the base is dynamic (e.g., per-instance pointer).
        static bool resolve_from_base(const MemView& view, const DolphinAddr& a, uint32_t base_override, uint32_t& out_va);

        // Typed reads (use resolve() or resolve_from_base() before calling these if needed).
        static bool readU8(const MemView& v, AddrKey k, uint8_t& out);
        static bool readU16(const MemView& v, AddrKey k, uint16_t& out);
        static bool readU32(const MemView& v, AddrKey k, uint32_t& out);
        static bool readU64(const MemView& v, AddrKey k, uint64_t& out);
    };

    // ergonomic aliases: addr::core::X, addr::battle::Y
    namespace core {
#define MK_ALIAS(dom, NAME, R, M, B, W, ...) inline constexpr AddrKey NAME = AddrKey::dom##_##NAME;
        ADDR_TABLE_CORE(MK_ALIAS)
#undef MK_ALIAS
    }
    namespace battle {
#define MK_ALIAS(dom, NAME, R, M, B, W, ...) inline constexpr AddrKey NAME = AddrKey::dom##_##NAME;
        ADDR_TABLE_BATTLE(MK_ALIAS)
#undef MK_ALIAS
    }

} // namespace addr
