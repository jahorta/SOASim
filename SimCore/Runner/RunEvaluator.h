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

struct RunEvaluator
{
    IDolphinRunner& host;
    const BreakpointMap& map;
    std::vector<PredicatePtr> predicates;
    RunLogger* logger{ nullptr };

    SimulationResult run(const std::string& branch_id,
        const std::vector<simcore::GCInputFrame>& plan);
};
