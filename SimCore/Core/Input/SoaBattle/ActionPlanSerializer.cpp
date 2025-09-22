#include "ActionPlanSerializer.h"

namespace soa::battle::actions {

    static inline void u32_le(std::vector<uint8_t>& buf, uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    static bool read_u32_le(const std::uint8_t*& cur, const std::uint8_t* end, std::uint32_t& v) {
        if (end - cur < 4) return false;
        std::memcpy(&v, cur, 4);
        cur += 4;
        return true;
    }

    // You should already have these in your ActionPlan wire layer.
    // Adapters are included here to avoid guessing your function names.
    static void encode_action_plan(const actions::ActionPlan& ap, std::vector<std::uint8_t>& out)
    {
        // Map high-level ActionPlan -> packed WireActionPlan (16 bytes)
        simcore::WireActionPlan w{};
        w.actor_slot = static_cast<std::uint8_t>(ap.actor_slot);
        w.is_prelude = static_cast<std::uint8_t>(ap.is_prelude ? 1 : 0);
        w.macro = static_cast<std::uint8_t>(ap.macro); // BattleAction enum -> u8
        w._pad0 = 0;

        w.target_mask = static_cast<std::uint32_t>(ap.params.target_mask);

        const auto* p = reinterpret_cast<const std::uint8_t*>(&w);
        out.insert(out.end(), p, p + sizeof(w));
    }

    static bool decode_action_plan(const std::uint8_t*& cur, const std::uint8_t* end, actions::ActionPlan& ap)
    {
        if (end - cur < static_cast<std::ptrdiff_t>(sizeof(simcore::WireActionPlan))) return false;

        const auto* w = reinterpret_cast<const simcore::WireActionPlan*>(cur);
        ap.actor_slot = w->actor_slot;
        ap.is_prelude = (w->is_prelude != 0);
        ap.macro = static_cast<actions::BattleAction>(w->macro);
        ap.params.target_mask = w->target_mask;

        cur += sizeof(simcore::WireActionPlan);
        return true;
    }
    // --- public API ---

    void encode_turn_plans_to_buffer(const actions::BattlePath& path, std::vector<std::uint8_t>& out) {
        out.clear();
        u32_le(out, static_cast<std::uint32_t>(path.size()));
        for (const auto& turn : path) {
            u32_le(out, static_cast<std::uint32_t>(turn.fake_attack_count));
            u32_le(out, static_cast<std::uint32_t>(turn.spec.size()));
            for (const auto& ap : turn.spec) {
                encode_action_plan(ap, out); // unchanged (packed struct memcpy)
            }
        }
    }

    bool decode_turn_plans_from_buffer(std::span<const std::uint8_t> buf, actions::BattlePath& out) {
        out.clear();
        const std::uint8_t* cur = buf.data();
        const std::uint8_t* end = buf.data() + buf.size();

        std::uint32_t turn_count = 0;
        if (!read_u32_le(cur, end, turn_count)) return false;

        out.reserve(turn_count);
        for (std::uint32_t t = 0; t < turn_count; ++t) {
            std::uint32_t wiggles = 0, action_count = 0;
            if (!read_u32_le(cur, end, wiggles)) return false;
            if (!read_u32_le(cur, end, action_count)) return false;

            actions::TurnPlan tp;
            tp.fake_attack_count = wiggles;
            tp.spec.reserve(action_count);

            for (std::uint32_t i = 0; i < action_count; ++i) {
                actions::ActionPlan ap{};
                if (!decode_action_plan(cur, end, ap)) return false;
                tp.spec.push_back(ap);
            }
            out.push_back(std::move(tp));
        }
        return (cur == end);
    }

} // namespace simcore::programs::battle
