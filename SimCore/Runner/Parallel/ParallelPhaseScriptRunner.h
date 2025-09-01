#pragma once
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

#include "../Script/PhaseScriptVM.h"
#include "../Breakpoints/BPCore.h"
#include "../../Boot/Boot.h"
#include "TSQueue.h"
#include "PRTypes.h"
#include "ProcessWorker.h"

namespace simcore {

    struct BootPlan {
        simboot::BootOptions boot;  // user_dir, dolphin_qt_base, force_p1_standard_pad, etc.
        std::string iso_path;       // game disc to load (no changes after start)
    };

    class ParallelPhaseScriptRunner {
    public:
        ParallelPhaseScriptRunner(size_t workers);
        ~ParallelPhaseScriptRunner();

        bool start(const BootPlan& boot, const PSInit& init, const PhaseScript& program);     // sets epoch=1
        uint64_t reconfigure(const PSInit& init, const PhaseScript& program); // epoch++

        uint64_t submit(const PSJob& job); // enqueues with current epoch, returns job_id
        bool try_get_result(PRResult& out);
        PRStatus status() const;
        void stop();

    private:
        struct CmdJob { uint64_t job_id; uint64_t epoch; PSJob job; };
        enum class CtrlType { Start, Reconfigure, Shutdown };
        struct CtrlStart { uint64_t epoch; BootPlan boot; PSInit init; PhaseScript program; };
        struct CtrlReconfig { uint64_t epoch; PSInit init; PhaseScript program; };
        struct CtrlCmd { CtrlType type; CtrlStart st; CtrlReconfig rc; };

        struct Worker {
            Worker() = default;

            size_t id{ 0 };
            std::unique_ptr<ProcessWorker> proc;  // child-process wrapper
            std::thread th;
            uint64_t epoch{ 0 };
            std::atomic<bool> running{ false };
            std::atomic<uint64_t> jobs_done{ 0 };
            TSQueue<CmdJob>* jobs{ nullptr };
            TSQueue<PRResult>* out{ nullptr };
        };

        std::vector<std::unique_ptr<TSQueue<CtrlCmd>>> ctrls_;
        std::unique_ptr<TSQueue<CmdJob>> jobs_;
        std::unique_ptr<TSQueue<PRResult>> out_;
        std::vector<std::unique_ptr<Worker>> workers_;
        std::atomic<bool> stop_{ false };
        std::atomic<uint64_t> job_seq_{ 0 };
        std::atomic<uint64_t> epoch_{ 0 };
    };

} // namespace simcore
