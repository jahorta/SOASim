#pragma once
#include <cstdint>
#include <string>
#include <cstring>
#include <array>
#include <initializer_list>
#include "../../DerivedBase.h"
#include "../SoaAddrRegistry.h"
#include "../../../../Runner/Breakpoints/BPRegistry.h"

namespace simcore {

    class DolphinWrapper;

    class DerivedBattleBuffer final : public IDerivedBuffer {
    public:
        DerivedBattleBuffer() { bytes_.resize(0x0200); } // reserve room to grow

        bool can_serve(addr::AddrKey k) const override {
            // Accept only keys in the "derived" domain for now (Battle program owns derived.* today).
            // If you later add DerivedExploreBuffer, tighten this: check against a curated allowlist.
            return addr::Registry::region(k) == addr::Region::DERIVED;
        }

        bool read_key(addr::AddrKey k, uint8_t width, uint64_t& out_bits) const override {
            const auto& sp = addr::Registry::spec(k);
            const size_t off = static_cast<size_t>(sp.base);
            if (off + width > bytes_.size()) return false;
            switch (width) {
            case 1: out_bits = *(const uint8_t*)(bytes_.data() + off);  return true;
            case 2: out_bits = *(const uint16_t*)(bytes_.data() + off); return true;
            case 4: out_bits = *(const uint32_t*)(bytes_.data() + off); return true;
            case 8: out_bits = *(const uint64_t*)(bytes_.data() + off); return true;
            default: return false;
            }
        }

        bool read_raw(uint32_t off, uint8_t width, uint64_t& out_bits) const override {
            if (off + width > bytes_.size()) return false;
            switch (width) {
            case 1: out_bits = *(const uint8_t*)(bytes_.data() + off);  return true;
            case 2: out_bits = *(const uint16_t*)(bytes_.data() + off); return true;
            case 4: out_bits = *(const uint32_t*)(bytes_.data() + off); return true;
            case 8: out_bits = *(const uint64_t*)(bytes_.data() + off); return true;
            default: return false;
            }
        }

        void on_init(const PSContext& ctx) override {
            (void)ctx;
            std::memset(bytes_.data(), 0xFF, bytes_.size());
            write_u32(kOff_CurrentTurn, 0);
            write_u32(kOff_TurnOrderSize, 0);
        }

        void update_turn_order_done(simcore::DolphinWrapper& host) {
            for (int i = 0; i < 12; i++) {
                write_u8(kOff_TurnOrderIdx + i, 0xffu);
            }
            int cur;
            uint8_t off;
            for (cur = 0; cur < 12; cur++) {
                host.readU8(addr::Registry::base(addr::battle::TurnOrderTable) + cur, off);
                if (off == 0xff) break;
                write_u8(kOff_TurnOrderIdx + off, cur);
            }
            write_u32(kOff_TurnOrderSize, cur);
        }

        void update_on_bp(uint32_t hit_bp, const PSContext& ctx, simcore::DolphinWrapper& host) override {
            if (hit_bp == (uint32_t)bp::battle::TurnIsSet) 
            {
                update_turn_order_done(host);
                uint32_t turn = 0;
                ctx.get(simcore::keys::battle::ACTIVE_TURN, turn);
                write_u32(kOff_CurrentTurn, turn);
            }
        }

    private:
        void write_u8(size_t off, uint8_t v) {
            if (off + 1 > bytes_.size()) bytes_.resize(off + 1);
            bytes_[off] = v;
        }

        void write_u32(size_t off, uint32_t v) {
            if (off + 4 > bytes_.size()) bytes_.resize(off + 4);
            std::memcpy(bytes_.data() + off, &v, 4);
        }

        // Offsets must match your ADDR_TABLE_DERIVED_BATTLE BASE_VA layout.
        static constexpr size_t kOff_TurnOrderIdx = 0x0000; // 12xU8 (PC0..PC3, Enemy0..Enemy7)
        static constexpr size_t kOff_CurrentTurn = 0x0010; // U32
        static constexpr size_t kOff_TurnOrderSize = 0x0014; // U32

        std::string bytes_;
    };

} // namespace simcore