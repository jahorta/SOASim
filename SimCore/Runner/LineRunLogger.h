#pragma once
#include <functional>
#include <sstream>
#include <string>
#include "RunEvaluator.h"

class LineRunLogger : public RunLogger
{
public:
    using Sink = std::function<void(const std::string&)>;
    explicit LineRunLogger(Sink s) : sink(std::move(s)) {}

    void on_breakpoint(const BreakpointHit& h, const char* label) override
    {
        std::ostringstream os;
        os << "type=bp"
            << " key=" << h.key
            << " label=" << label
            << " frame=" << h.frame
            << " pc=0x" << std::hex << h.pc;
        if (sink) sink(os.str());
    }

    void on_complete(const SimulationResult& r) override
    {
        std::ostringstream os;
        os << "type=result"
            << " branch=" << r.branch_id
            << " frames=" << std::dec << r.frames_run
            << " success=" << (r.final_success ? 1 : 0)
            << " bphits=" << r.hits.size()
            << " preds=" << r.outcomes.size();
        if (sink) sink(os.str());
    }

private:
    Sink sink;
};
