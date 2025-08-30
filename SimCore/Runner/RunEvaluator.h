#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include "IDolphinRunner.h"
#include "Breakpoints/BPCore.h"
#include "Breakpoints/Predicate.h"

struct BreakpointHit
{
    BPKey key;
    uint32_t frame;
    uint32_t pc;
};

struct PredicateOutcome
{
    bool evaluated{ false };
    bool passed{ true };
    Metrics metrics;
};

struct SimulationResult
{
    std::string branch_id;
    std::vector<BreakpointHit> hits;
    std::unordered_map<std::string, PredicateOutcome> outcomes;
    bool final_success{ true };
    uint32_t frames_run{ 0 };
};

struct RunLogger
{
    virtual ~RunLogger() = default;
    virtual void on_breakpoint(const BreakpointHit&, const char* label) {}
    virtual void on_complete(const SimulationResult&) {}
};

enum class SimPhase { Inputs, UntilBP };

struct PhaseDecision
{
    SimPhase next{ SimPhase::UntilBP };
    bool progressed{ false };
    bool timeout{ false };
    std::optional<BreakpointHit> hit{};
    bool terminal_reached{ false };
    bool final_success{ false };
};

struct PhaseBOptions
{
    std::vector<BPKey> end_keys;
    uint32_t timeout_ms{ 5000 };
};

struct RunEvaluator
{
    RunEvaluator(IDolphinRunner& host_,
        const BreakpointMap& map_,
        std::vector<PredicatePtr> predicates_,
        RunLogger* logger_ = nullptr);
    
    IDolphinRunner& host;
    const BreakpointMap& map;
    std::vector<PredicatePtr> predicates;
    RunLogger* logger{ nullptr };

    SimulationResult run(const std::string& branch_id,
        const std::vector<simcore::GCInputFrame>& plan);

    PhaseDecision run_inputs(const std::vector<simcore::GCInputFrame>& frames,
        SimulationResult& out);

    PhaseDecision run_until_bp(const PhaseBOptions& opt,
        SimulationResult& out);

private:
    std::unordered_map<uint32_t, const BPAddr*> by_pc;
    bool catalog_armed{ false };

    void build_index_once();
    void ensure_catalog_breakpoints_armed();
    void eval_predicates_for(BPKey key, SimulationResult& out);
    bool all_enabled_predicates_passed(const SimulationResult& out) const;
};
