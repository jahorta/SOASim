#include "RunEvaluator.h"

SimulationResult RunEvaluator::run(const std::string& branch_id,
    const std::vector<simcore::GCInputFrame>& plan)
{
    SimulationResult res{};
    res.branch_id = branch_id;

    size_t idx = 0;
    bool terminated = false;

    while (true)
    {
        const simcore::GCInputFrame f = (idx < plan.size()) ? plan[idx] : simcore::GCInputFrame{};
        host.set_next_input(f);
        host.step_one_frame();
        ++idx;

        const uint32_t pc = host.get_pc();
        if (auto k = map.match(pc))
        {
            BreakpointHit h{ *k, static_cast<uint32_t>(idx), pc };
            res.hits.push_back(h);
            if (logger) logger->on_breakpoint(h, map.label(*k));

            for (auto& p : predicates)
            {
                if (!p->enabled() || p->bp_key() != *k) continue;

                auto& out = res.outcomes[p->id()];
                Metrics m;
                const bool pass = p->evaluate(host, m);

                out.evaluated = true;
                out.passed = out.passed && pass;
                for (auto& kv : m) out.metrics[kv.first] += kv.second;
            }

            if (map.terminal_key && *map.terminal_key == *k)
            {
                terminated = true;
                break;
            }
        }
    }

    res.frames_run = static_cast<uint32_t>(idx);

    bool ok = true;
    for (auto& p : predicates)
    {
        if (!p->enabled()) continue;
        auto it = res.outcomes.find(p->id());
        if (it == res.outcomes.end()) continue; // missing => pass
        if (!it->second.passed) { ok = false; break; }
    }

    res.final_success = terminated && ok;

    if (logger) logger->on_complete(res);
    return res;
}
