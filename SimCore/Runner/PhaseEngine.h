#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "../Core/DolphinWrapper.h"
#include "Breakpoints/BPCore.h"
#include "Predicates/PredicateCatalog.h"
#include "../Core/Input/InputPlan.h"

struct PhaseSpec {
    std::string name;
    std::vector<BPKey> canonical_bp_keys; // fixed set per phase
    std::vector<std::string> allowed_predicates;
};

struct BranchSpec {
    std::string id;
    std::vector<simcore::GCInputFrame> inputs; // if empty, caller should use run_until_bp
};

struct PhaseResult {
    bool success{ false };
    uint32_t hit_pc{ 0 };
    std::string reason;
    std::unordered_map<std::string, int64_t> metrics;
    std::unordered_map<std::string, bool> predicate_passed;
};

class PhaseEngine {
public:
    PhaseEngine(simcore::DolphinWrapper& host, const BreakpointMap& bpmap, const PhaseSpec& phase);

    // Only runs until any armed breakpoint is hit; evaluates predicates for that BP and returns
    PhaseResult run_until_bp(uint32_t timeout_ms,
        const std::vector<PredicateDef>& predicates);

    // Feeds inputs frame-by-frame; evaluates on BP hits; stops on first terminal BP hit
    PhaseResult run_inputs(const BranchSpec& branch,
        uint32_t timeout_ms,
        const std::vector<PredicateDef>& predicates);

    // Arm once per instance; no clear/set on each run
    void arm_phase_breakpoints_once();

private:
    simcore::DolphinWrapper& host_;
    const BreakpointMap& bpmap_;
    PhaseSpec phase_;
    bool armed_{ false };

    // Helpers
    bool pc_to_bpkey(uint32_t pc, BPKey* out) const;
    bool read_batch(const std::vector<MemRead>& reads, PredValues& out) const;
    PhaseResult eval_for_hit(uint32_t pc, const std::vector<PredicateDef>& predicates,
        std::unordered_map<std::string, int64_t>& metrics) const;
};
