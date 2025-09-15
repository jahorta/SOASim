#include "SoaAddrRegistry.h"

namespace addr {

    // Declare one constexpr offsets array per entry (Raw => empty).
#define DECL_OFFS(dom, NAME, R, M, B, W, ...) \
  static constexpr int32_t OFFS_##dom##_##NAME[] = { __VA_ARGS__ __VA_OPT__(,) 0 };
    ADDR_TABLE_ALL(DECL_OFFS)
#undef DECL_OFFS

    // Build the master table in one pass.
        static constexpr AddrRec kAll[] = {
          #define ROW(dom, NAME, R, M, B, W, ...) \
            { AddrKey::dom##_##NAME, \
              DolphinAddr{ Region::R, AddrMode::M, static_cast<uint32_t>(B), Width::W, \
                           OFFS_##dom##_##NAME, \
                           static_cast<uint8_t>((sizeof(OFFS_##dom##_##NAME)/sizeof(int32_t)) - 1) }, \
              #dom "." #NAME },
          ADDR_TABLE_ALL(ROW)
          #undef ROW
    };

    std::span<const AddrRec> Registry::all() {
        return std::span<const AddrRec>(kAll, sizeof(kAll) / sizeof(kAll[0]));
    }

    const AddrRec* Registry::find(AddrKey k) {
        for (const auto& r : kAll) if (r.key == k) return &r;
        return nullptr;
    }

    const DolphinAddr& Registry::spec(AddrKey k) {
        const auto* r = find(k);
        return r ? r->spec : kAll[0].spec; // table is non-empty
    }

    const uint32_t Registry::base(AddrKey k)
    {
        return spec(k).base;
    }

    const char* Registry::name(AddrKey k) {
        const auto* r = find(k);
        return r ? r->name : "";
    }

    static inline bool in_region(const simcore::MemView& v, Region r, uint32_t va) {
        switch (r) {
        case Region::MEM1: return v.in_mem1(va);
        default:           return false;
        }
    }

    bool Registry::resolve(const simcore::MemView& view, const DolphinAddr& a, uint32_t& out_va) {
        if (a.mode == AddrMode::Raw) {
            if (!in_region(view, a.region, a.base)) return false;
            out_va = a.base;
            return true;
        }
        // PtrChain: require a non-zero base in spec; otherwise use resolve_from_base()
        if (a.base == 0) return false;
        return resolve_from_base(view, a, a.base, out_va);
    }

    bool Registry::resolve_from_base(const simcore::MemView& view, const DolphinAddr& a, uint32_t base_override, uint32_t& out_va) {
        if (a.mode == AddrMode::Raw) {
            // Allow overriding Raw base as well (handy for relative blocks, will just ignore offsets_len=0)
            if (!in_region(view, a.region, base_override)) return false;
            out_va = base_override;
            return true;
        }
        uint32_t cur = base_override;
        for (uint8_t i = 0; i < a.offsets_len; ++i) {
            if (!in_region(view, a.region, cur)) return false;
            uint32_t ptr = 0;
            if (!view.read_u32(cur, ptr)) return false;
            if (ptr == 0 || !in_region(view, a.region, ptr)) return false;
            cur = ptr + static_cast<uint32_t>(a.offsets[i]);
        }
        if (!in_region(view, a.region, cur)) return false;
        out_va = cur;
        return true;
    }

    bool Registry::readU8(const simcore::MemView& v, AddrKey k, uint8_t& out) { uint32_t va = 0; if (!resolve(v, spec(k), va)) return false; return v.read_u8(va, out); }
    bool Registry::readU16(const simcore::MemView& v, AddrKey k, uint16_t& out) { uint32_t va = 0; if (!resolve(v, spec(k), va)) return false; return v.read_u16(va, out); }
    bool Registry::readU32(const simcore::MemView& v, AddrKey k, uint32_t& out) { uint32_t va = 0; if (!resolve(v, spec(k), va)) return false; return v.read_u32(va, out); }
    bool Registry::readU64(const simcore::MemView& v, AddrKey k, uint64_t& out) { uint32_t va = 0; if (!resolve(v, spec(k), va)) return false; return v.read_u64(va, out); }

} // namespace addr
