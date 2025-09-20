// Runner/Breakpoints/Predicate.h
#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include "../../Core/Memory/Soa/SoaAddrRegistry.h"

namespace simcore::pred {

#pragma pack(push,1)
    enum class PredFlag : uint8_t {
        CaptureBaseline = 1 << 0,
        Active = 1 << 1,
        RhsIsKey = 1 << 2,
    };

    inline constexpr PredFlag operator|(PredFlag a, PredFlag b) { return PredFlag(uint8_t(a) | uint8_t(b)); }
    inline constexpr PredFlag operator&(PredFlag a, PredFlag b) { return PredFlag(uint8_t(a) & uint8_t(b)); }

    struct PredicateRecord {
        uint16_t id;
        uint16_t required_bp;
        uint8_t  kind;                // 0 ABS, 1 DELTA
        uint8_t  width;               // 1,2,4,8
        uint8_t  cmp;                 // 0==,1!=,2<,3<=,4>,5>=
        uint8_t  flags;               // PredFlag bits
        uint32_t turn_mask;           // 0 => treat as all-ones

        // LHS
        uint32_t lhs_addr;            // legacy absolute VA
        uint16_t lhs_addr_key;        // region comes from this (for key-based reads)
        uint32_t lhs_addrprog_offset; // program offset within [records || blob] (0 = none)

        // RHS
        uint64_t rhs_imm;             // when !(RhsIsKey || rhs_addrprog_offset)
        uint16_t rhs_addr_key;        // region for RHS key or program
        uint32_t rhs_addrprog_offset; // program offset within [records || blob] (0 = none)

        void set_flag(PredFlag f) { flags |= uint8_t(f); }
        void clear_flag(PredFlag f) { flags &= ~uint8_t(f); }
        bool has_flag(PredFlag f) const { return (flags & uint8_t(f)) != 0; }
    };
#pragma pack(pop)

    enum class PredKind : uint8_t { ABS = 0, DELTA = 1 };
    enum class CmpOp : uint8_t { EQ = 0, NE = 1, LT = 2, LE = 3, GT = 4, GE = 5 };

    struct Spec {
        uint16_t id{ 0 };
        uint16_t required_bp{ 0 };
        PredKind kind{ PredKind::ABS };
        uint8_t  width{ 0 };
        CmpOp    cmp{ CmpOp::EQ };
        uint8_t  flags{ 0 }; // bit0=capture_baseline, bit1=active, bit2=rhs_is_key

        std::optional<addr::AddrKey> key{};     // LHS anchor (symbolic), optional
        uint32_t lhs_addr{ 0 };                   // LHS absolute VA (legacy path)

        uint64_t rhs_bits{ 0 };                   // RHS immediate if not key and not program
        std::optional<addr::AddrKey> rhs_key{}; // RHS anchor when RhsIsKey

        uint32_t turn_mask{ 0xFFFFFFFFu };

        // NEW: embedded address programs (may be empty)
        std::vector<uint8_t> lhs_prog;          // must start with OP_BASE_KEY if non-empty
        std::vector<uint8_t> rhs_prog;          // must start with OP_BASE_KEY if non-empty
    };

    // One-and-done builder: fills records and returns packed program blob.
    bool BuildTable(const std::vector<Spec>& specs,
        std::vector<PredicateRecord>& out_records,
        std::vector<uint8_t>& out_addrprog_blob);

} // namespace simcore::pred
