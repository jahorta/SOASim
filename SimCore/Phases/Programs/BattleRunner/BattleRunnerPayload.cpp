#include "BattleRunnerPayload.h"
#include "../../../Runner/IPC/Wire.h"
#include <cstring>

namespace simcore::battle {
    static inline void put_u32(std::vector<uint8_t>& b, uint32_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); b.push_back(uint8_t(v >> 16)); b.push_back(uint8_t(v >> 24)); }
    static inline bool get_u32(const uint8_t*& p, const uint8_t* e, uint32_t& v) { if (p + 4 > e) return false; v = (uint32_t)p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); p += 4; return true; }

    bool encode_payload(const EncodeSpec& spec, std::vector<uint8_t>& out)
    {
        out.clear();
        out.push_back(PK_BattleTurnRunner);
        put_u32(out, 2);
        put_u32(out, spec.run_ms);
        put_u32(out, spec.vi_stall_ms);
        const auto* f = reinterpret_cast<const uint8_t*>(&spec.initial);
        out.insert(out.end(), f, f + sizeof(GCInputFrame));

        const uint32_t n = (uint32_t)spec.plans.size();
        put_u32(out, n);
        for (uint32_t i = 0; i < n; ++i) {
            const auto& v = spec.plans[i];
            put_u32(out, (uint32_t)v.size());
            if (!v.empty()) {
                const auto* p = reinterpret_cast<const uint8_t*>(v.data());
                out.insert(out.end(), p, p + v.size() * sizeof(GCInputFrame));
            }
        }

        const uint32_t np = (uint32_t)spec.predicates.size();
        put_u32(out, np);
        if (np) {
            const auto* p = reinterpret_cast<const uint8_t*>(spec.predicates.data());
            out.insert(out.end(), p, p + np * sizeof(PredicateRecord));
        }
        return true;
    }

    bool decode_payload(const std::vector<uint8_t>& in, PSContext& out_ctx)
    {
        if (in.size() < 1 + 4 + 4 + 4 + sizeof(GCInputFrame) + 4) return false;
        const uint8_t* p = in.data();
        const uint8_t* e = p + in.size();
        const uint8_t tag = *p++; if (tag != PK_BattleTurnRunner) return false;

        uint32_t version = 0, run_ms = 0, vi_stall_ms = 0;
        if (!get_u32(p, e, version)) return false;
        if (version != 2) return false;
        if (!get_u32(p, e, run_ms)) return false;
        if (!get_u32(p, e, vi_stall_ms)) return false;

        if (p + sizeof(GCInputFrame) > e) return false;
        GCInputFrame initial{};
        std::memcpy(&initial, p, sizeof(GCInputFrame)); p += sizeof(GCInputFrame);

        uint32_t n_plans = 0; if (!get_u32(p, e, n_plans)) return false;

        std::string counts; counts.resize(n_plans * 4);
        std::string frames;
        for (uint32_t i = 0; i < n_plans; ++i) {
            uint32_t nf = 0; if (!get_u32(p, e, nf)) return false;
            std::memcpy(&counts[i * 4], &nf, 4);
            const size_t bytes = size_t(nf) * sizeof(GCInputFrame);
            if (p + bytes > e) return false;
            frames.append(reinterpret_cast<const char*>(p), bytes);
            p += bytes;
        }

        uint32_t pred_count = 0; if (!get_u32(p, e, pred_count)) return false;
        std::string pred_table; std::string pred_bases;
        if (pred_count) {
            const size_t bytes = size_t(pred_count) * sizeof(PredicateRecord);
            if (p + bytes > e) return false;
            pred_table.append(reinterpret_cast<const char*>(p), bytes);
            pred_bases.resize(pred_count * sizeof(uint64_t), 0);
            p += bytes;
        }

        out_ctx[simcore::keys::battle::INITIAL_INPUT] = initial;
        out_ctx[simcore::keys::battle::NUM_PLANS] = n_plans;
        out_ctx[simcore::keys::battle::PLAN_COUNTS] = counts;
        out_ctx[simcore::keys::battle::PLAN_TABLE] = frames;

        const uint32_t last_idx = (n_plans == 0) ? 0u : (n_plans - 1u);
        out_ctx[simcore::keys::battle::LAST_TURN_IDX] = last_idx;

        out_ctx[simcore::keys::core::PRED_COUNT] = pred_count;
        out_ctx[simcore::keys::core::PRED_TABLE] = pred_table;
        out_ctx[simcore::keys::core::PRED_BASELINES] = pred_bases;

        if (run_ms)      out_ctx[simcore::keys::core::RUN_MS] = run_ms;
        if (vi_stall_ms) out_ctx[simcore::keys::core::VI_STALL_MS] = vi_stall_ms;

        out_ctx[simcore::keys::core::PLAN_FRAME_IDX] = uint32_t(0);
        out_ctx[simcore::keys::core::PLAN_DONE] = uint32_t(0);
        out_ctx[simcore::keys::battle::ACTIVE_TURN] = uint32_t(0);
        out_ctx[simcore::keys::core::PRED_TOTAL_AT_BP] = uint32_t(0);
        out_ctx[simcore::keys::core::PRED_PASS_AT_BP] = uint32_t(0);
        return true;
    }
} // namespace simcore::battle
