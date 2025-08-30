#include "RunEvaluator.h"
#include <utility>

RunEvaluator::RunEvaluator(IDolphinRunner& host_,
    const BreakpointMap& map_,
    std::vector<PredicatePtr> predicates_,
    RunLogger* logger_)
    : host(host_), map(map_), predicates(std::move(predicates_)), logger(logger_)
{
}

static const char* label_for(const BreakpointMap& map, BPKey key)
{
    for (const auto& a : map.addrs) if (a.key == key) return a.name;
    return "";
}

void RunEvaluator::build_index_once()
{
    if (!by_pc.empty()) return;
    for (const auto& a : map.addrs) by_pc[a.pc] = &a;
}

void RunEvaluator::ensure_catalog_breakpoints_armed()
{
    if (catalog_armed) return;
    std::vector<uint32_t> pcs;
    pcs.reserve(map.addrs.size());
    for (const auto& a : map.addrs) pcs.push_back(a.pc);
    host.arm_pc_breakpoints(pcs);
    catalog_armed = true;
}

void RunEvaluator::eval_predicates_for(BPKey key, SimulationResult& out)
{
    for (auto& p : predicates)
    {
        if (!p->enabled() || p->bp_key() != key) continue;
        auto& o = out.outcomes[p->id()];
        Metrics m;
        const bool pass = p->evaluate(host, m);
        o.evaluated = true;
        o.passed = o.passed && pass;
        for (auto& kv : m) o.metrics[kv.first] += kv.second;
    }
}

bool RunEvaluator::all_enabled_predicates_passed(const SimulationResult& out) const
{
    for (auto& p : predicates)
    {
        if (!p->enabled()) continue;
        auto it = out.outcomes.find(p->id());
        if (it == out.outcomes.end()) continue; // missing --> pass
        if (!it->second.passed) return false;
    }
    return true;
}

SimulationResult RunEvaluator::run(const std::string& branch_id,
    const std::vector<simcore::GCInputFrame>& plan)
{
    SimulationResult res{};
    res.branch_id = branch_id;

    size_t idx = 0;
    bool terminated = false;

    build_index_once();

    while (true)
    {
        const simcore::GCInputFrame f = (idx < plan.size()) ? plan[idx] : simcore::GCInputFrame{};
        host.set_next_input(f);
        host.step_one_frame();

        if (idx < plan.size()) ++idx; else break;

        const uint32_t pc = host.get_pc();
        auto it = by_pc.find(pc);
        if (it != by_pc.end())
        {
            const BPAddr* addr = it->second;
            res.hits.push_back(BreakpointHit{ addr->key, static_cast<uint32_t>(idx), pc });
            if (logger) logger->on_breakpoint(res.hits.back(), addr->name);
            eval_predicates_for(addr->key, res);
            if (map.terminal_key && *map.terminal_key == addr->key) { terminated = true; break; }
        }
    }

    res.frames_run = static_cast<uint32_t>(idx);
    res.final_success = terminated && all_enabled_predicates_passed(res);
    if (logger) logger->on_complete(res);
    return res;
}

PhaseDecision RunEvaluator::run_inputs(const std::vector<simcore::GCInputFrame>& frames,
    SimulationResult& out)
{
    build_index_once();
    size_t consumed = 0;
    for (size_t i = 0; i < frames.size(); ++i)
    {
        host.set_next_input(frames[i]);
        host.step_one_frame();
        ++consumed;
        ++out.frames_run;

        const uint32_t pc = host.get_pc();
        auto it = by_pc.find(pc);
        if (it != by_pc.end())
        {
            const BPAddr* addr = it->second;
            out.hits.push_back(BreakpointHit{ addr->key, out.frames_run, pc });
            if (logger) logger->on_breakpoint(out.hits.back(), addr->name);
            eval_predicates_for(addr->key, out);
            if (map.terminal_key && *map.terminal_key == addr->key)
            {
                const bool ok = all_enabled_predicates_passed(out);
                PhaseDecision d{};
                d.next = SimPhase::UntilBP;
                d.progressed = true;
                d.hit = out.hits.back();
                d.terminal_reached = true;
                d.final_success = ok;
                return d;
            }
        }
    }

    PhaseDecision d{};
    d.next = SimPhase::UntilBP;
    d.progressed = consumed > 0;
    return d;
}

PhaseDecision RunEvaluator::run_until_bp(const PhaseBOptions& opt,
    SimulationResult& out)
{
    build_index_once();
    ensure_catalog_breakpoints_armed();

    auto r = host.run_until_breakpoint_blocking(opt.timeout_ms);
    if (!r.hit)
    {
        PhaseDecision d{};
        d.next = SimPhase::Inputs;
        d.timeout = true;
        return d;
    }

    auto it = by_pc.find(r.pc);
    if (it == by_pc.end())
    {
        PhaseDecision d{};
        d.next = SimPhase::UntilBP;
        d.progressed = true;
        return d;
    }

    const BPAddr* addr = it->second;
    out.hits.push_back(BreakpointHit{ addr->key, out.frames_run, r.pc });
    if (logger) logger->on_breakpoint(out.hits.back(), addr->name);
    eval_predicates_for(addr->key, out);

    PhaseDecision d{};
    d.progressed = true;
    d.hit = out.hits.back();

    bool is_terminal = (map.terminal_key && *map.terminal_key == addr->key);
    if (is_terminal)
    {
        d.terminal_reached = true;
        d.final_success = all_enabled_predicates_passed(out);
    }

    bool end_phase = false;
    for (auto k : opt.end_keys) if (k == addr->key) { end_phase = true; break; }
    d.next = end_phase ? SimPhase::Inputs : SimPhase::UntilBP;
    return d;
}
