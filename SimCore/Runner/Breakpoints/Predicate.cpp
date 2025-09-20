// Runner/Breakpoints/Predicate.cpp
#include "Predicate.h"
#include <unordered_map>
#include <span>
#include <cstring>

namespace {
    // FNV-1a 64-bit
    inline uint64_t hash64(std::span<const uint8_t> s) {
        uint64_t h = 1469598103934665603ull;
        for (uint8_t b : s) { h ^= b; h *= 1099511628211ull; }
        return h;
    }

    struct BlobDeduper {
        std::vector<uint8_t> blob;
        struct Entry { uint32_t off; uint32_t len; };
        std::unordered_map<uint64_t, std::vector<Entry>> map;

        uint32_t intern(std::span<const uint8_t> s) {
            if (s.empty()) return 0;
            const uint64_t h = hash64(s);
            auto& vec = map[h];
            for (const auto& e : vec) {
                if (e.len == s.size() && std::memcmp(blob.data() + e.off, s.data(), s.size()) == 0)
                    return e.off;
            }
            const uint32_t off = (uint32_t)blob.size();
            blob.insert(blob.end(), s.begin(), s.end());
            vec.push_back({ off, (uint32_t)s.size() });
            return off;
        }
    };
}

namespace simcore::pred {

    bool BuildTable(const std::vector<Spec>& in,
        std::vector<PredicateRecord>& out_records,
        std::vector<uint8_t>& out_blob)
    {
        out_records.clear();
        out_blob.clear();
        out_records.reserve(in.size());

        // First pass: write records w/ zero offsets
        for (const auto& s : in) {
            PredicateRecord r{};
            r.id = s.id; r.required_bp = s.required_bp;
            r.kind = static_cast<uint8_t>(s.kind);
            r.cmp = static_cast<uint8_t>(s.cmp);

            const uint8_t width = s.width ? s.width : 4; // explicit-at-read-time rule
            r.width = width;

            r.flags = s.flags;
            r.turn_mask = s.turn_mask ? s.turn_mask : 0xFFFFFFFFu;

            // LHS
            r.lhs_addr = s.lhs_addr;
            r.lhs_addr_key = s.key.has_value() ? static_cast<uint16_t>(*s.key) : 0;
            r.lhs_addrprog_offset = 0; // fill after packing

            // RHS
            const bool rhs_is_key = s.rhs_key.has_value();
            if (rhs_is_key) r.flags |= uint8_t(PredFlag::RhsIsKey);

            r.rhs_addr_key = rhs_is_key ? static_cast<uint16_t>(*s.rhs_key) : 0;
            r.rhs_imm = rhs_is_key ? 0ull : s.rhs_bits;
            r.rhs_addrprog_offset = 0; // fill after packing

            out_records.push_back(r);
        }

        if (out_records.empty()) return true;

        // Second pass: dedupe and lay out programs into a single blob
        BlobDeduper dedupe;
        const uint32_t base = (uint32_t)(out_records.size() * sizeof(PredicateRecord));
        for (size_t i = 0; i < in.size(); ++i) {
            auto& r = out_records[i];
            const auto& s = in[i];

            if (!s.lhs_prog.empty()) {
                const uint32_t off = dedupe.intern(std::span<const uint8_t>(s.lhs_prog.data(), s.lhs_prog.size()));
                r.lhs_addrprog_offset = off ? base + off : 0;
            }
            if (!s.rhs_prog.empty()) {
                const uint32_t off = dedupe.intern(std::span<const uint8_t>(s.rhs_prog.data(), s.rhs_prog.size()));
                r.rhs_addrprog_offset = off ? base + off : 0;
            }
        }

        out_blob = std::move(dedupe.blob);
        return true;
    }

} // namespace simcore::pred
