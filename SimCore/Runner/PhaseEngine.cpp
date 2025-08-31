#include "PhaseEngine.h"
#include <algorithm>

static inline bool read_one(simcore::DolphinWrapper& host, const MemRead& r, PredValue& out) {
    switch (r.type) {
    case MemType::U8: { uint8_t v{};  if (!host.readU8(r.addr, v)) return false;  out = v; return true; }
    case MemType::U16: { uint16_t v{}; if (!host.readU16(r.addr, v)) return false;  out = v; return true; }
    case MemType::U32: { uint32_t v{}; if (!host.readU32(r.addr, v)) return false;  out = v; return true; }
    case MemType::F32: { float v{};    if (!host.readF32(r.addr, v)) return false;  out = v; return true; }
    case MemType::F64: { double v{};   if (!host.readF64(r.addr, v)) return false;  out = v; return true; }
    }
    return false;
}

PhaseEngine::PhaseEngine(simcore::DolphinWrapper& host, const BreakpointMap& bpmap, const PhaseSpec& phase)
    : host_(host), bpmap_(bpmap), phase_(phase) {
}

void PhaseEngine::arm_phase_breakpoints_once() {
    if (armed_) return;
    std::vector<uint32_t> pcs;
    pcs.reserve(phase_.canonical_bp_keys.size());
    for (const auto& k : phase_.canonical_bp_keys) {
        if (const auto* e = bpmap_.find(k)) pcs.push_back(e->pc);
    }
    if (!pcs.empty()) host_.armPcBreakpoints(pcs);
    armed_ = true;
}

bool PhaseEngine::pc_to_bpkey(uint32_t pc, BPKey* out) const {
    if (!out) return false;
    for (const auto& k : phase_.canonical_bp_keys) {
        if (const auto* e = bpmap_.find(k)) {
            if (e->pc == pc) { *out = k; return true; }
        }
    }
    return false;
}

bool PhaseEngine::read_batch(const std::vector<MemRead>& reads, PredValues& out) const {
    for (const auto& r : reads) {
        PredValue v{};
        if (!read_one(host_, r, v)) return false;
        out[r.key] = v;
    }
    return true;
}

PhaseResult PhaseEngine::eval_for_hit(uint32_t pc,
    const std::vector<PredicateDef>& predicates,
    std::unordered_map<std::string, int64_t>& metrics) const {
    PhaseResult res{};
    res.hit_pc = pc;
    BPKey k{};
    if (!pc_to_bpkey(pc, &k)) {
        res.reason = "bp_not_in_phase";
        return res;
    }

    // Evaluate only predicates tied to this BP
    bool ok = true;
    for (const auto& pd : predicates) {
        if (pd.bp != k) continue;
        PredValues vals;
        if (!read_batch(pd.reads, vals)) {
            ok = false;
            res.predicate_passed[pd.key] = false;
            continue;
        }
        const bool pass = pd.fn(vals, metrics);
        res.predicate_passed[pd.key] = pass;
        ok = ok && pass;
    }
    res.success = ok;
    res.reason = ok ? "ok" : "predicate_fail";
    return res;
}

PhaseResult PhaseEngine::run_until_bp(uint32_t timeout_ms,
    const std::vector<PredicateDef>& predicates) {
    arm_phase_breakpoints_once();
    auto r = host_.runUntilBreakpointBlocking(timeout_ms);
    if (!r.hit) {
        PhaseResult pr{}; pr.reason = "timeout"; return pr;
    }
    std::unordered_map<std::string, int64_t> metrics;
    return eval_for_hit(r.pc, predicates, metrics);
}

PhaseResult PhaseEngine::run_inputs(const BranchSpec& branch,
    uint32_t timeout_ms,
    const std::vector<PredicateDef>& predicates) {
    arm_phase_breakpoints_once();
    std::unordered_map<std::string, int64_t> metrics;

    for (size_t i = 0; i < branch.inputs.size(); ++i) {
        host_.setInput(branch.inputs[i]);
        host_.stepOneFrameBlocking();

        // Small non-blocking slice to check for immediate BP hit this frame
        auto r = host_.runUntilBreakpointBlocking(0);
        if (r.hit) {
            return eval_for_hit(r.pc, predicates, metrics);
        }
    }

    PhaseResult pr{}; pr.reason = "no_bp"; return pr;
}
