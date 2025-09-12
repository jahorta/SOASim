#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "DolphinRunner.h"
#include "PhaseEngine.h"
#include "Predicates/PredicateCatalog.h"

struct DolphinJob {
    std::string id;
    std::string savestate;
    PhaseSpec phase;
    BranchSpec branch;
    std::vector<std::string> predicate_keys; // chosen by UI
    uint32_t timeout_ms{ 10000 };
};

struct JobStatus {
    std::string id;
    enum { Queued, Running, Done, Failed } state{ Queued };
    float progress{ 0.f };
};

class JobRunner {
public:
    JobRunner(size_t workers,
        BreakpointMap bpmap,
        PredicateCatalog catalog,
        std::function<void(const JobStatus&)> on_status);
    ~JobRunner();
    void enqueue(DolphinJob job);
private:
    void worker_main(size_t idx);
    
};
