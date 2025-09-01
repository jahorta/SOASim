#include "ParallelPhaseScriptRunner.h"
#include "../../Utils/ThreadName.h"
#include "../IPC/Wire.h"

namespace simcore {

    ParallelPhaseScriptRunner::ParallelPhaseScriptRunner(size_t n)
    {
        jobs_.reset(new TSQueue<CmdJob>());
        out_.reset(new TSQueue<PRResult>());
        ctrls_.reserve(n);
        workers_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            auto w = std::make_unique<Worker>();
            w->id = i;
            w->proc = std::make_unique<ProcessWorker>();
            w->jobs = jobs_.get();
            w->out = out_.get();
            workers_.push_back(std::move(w));
        }
    }

    ParallelPhaseScriptRunner::~ParallelPhaseScriptRunner() { stop(); }

    bool ParallelPhaseScriptRunner::start(const BootPlan& boot,
        const PSInit& init,
        const PhaseScript& program)
    {
        if (epoch_.load() != 0) return false;
        const uint64_t e = 1;
        epoch_.store(e);

        char exePath[MAX_PATH]{};
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string base = exePath;
        auto pos = base.find_last_of("\\/");
        base = (pos == std::string::npos) ? "." : base.substr(0, pos);
        std::string workerExe = base + "\\SimCoreWorker.exe";

        size_t launched = 0;
        for (auto& w : workers_) {
            ProcStartParams ps{};
            ps.worker_id = w->id;
            ps.exe_path = workerExe;
            ps.iso_path = boot.iso_path;
            ps.savestate_path = init.savestate_path;
            ps.qt_base_dir = boot.boot.dolphin_qt_base.string();
            ps.user_dir = (boot.boot.user_dir / ("runner-" + std::to_string(w->id)) / "User").string();
            ps.timeout_ms = init.default_timeout_ms;

            if (!w->proc->start(ps, out_.get())) {
                SCLOGE("[Runner %zu] failed to launch SimCoreWorker process", w->id);
                continue;
            }
            ++launched;

            w->running.store(true);
            w->epoch = e;

            // Start dispatcher thread NOW; it will sleep until proc->is_ready()
            w->th = std::thread([this, w = w.get()] {
                set_this_thread_name_utf8((std::string("Dispatcher-") + std::to_string(w->id)).c_str());

                // Wait until the worker signals READY(ok=true) or is stopped
                for (;;) {
                    if (!w->running.load()) return;
                    if (w->proc->is_ready()) break;
                    if (w->proc->is_failed())  return;// This worker will never accept jobs; just park this dispatcher.
                    Sleep(5);
                }

                // Now accept and send jobs
                for (;;) {
                    if (!w->running.load()) break;

                    // Acquire this worker's single slot; if busy, wait
                    while (w->running.load() && !w->proc->try_acquire_slot()) {
                        Sleep(1);
                    }
                    if (!w->running.load()) break;

                    // Now we own the slot; pop exactly one job
                    CmdJob j{};
                    if (!w->jobs->pop_wait(j)) {
                        // queue closed; release slot and exit
                        w->proc->release_slot();            // in case we acquired but got closed
                        break;
                    }
                    if (j.epoch != w->epoch) {
                        // wrong epoch; free slot and NACK
                        w->proc->release_slot();
                        PRResult rr{}; rr.job_id = j.job_id; rr.epoch = j.epoch; rr.worker_id = w->id; rr.accepted = false;
                        out_->push(std::move(rr));
                        continue;
                    }

                    // Send one job. Slot stays held until reader_thread() sees the result.
                    if (!w->proc->send_job(j.job_id, j.epoch, j.job.input)) {
                        // send failed -> slot was released inside send_job(); mark this worker failed
                        SCLOGE("[Runner %zu] send_job failed (worker pipe)", w->id);
                        break;
                    }
                }
            });
        }

        // If nothing even launched, fail fast
        if (launched == 0) {
            SCLOGE("ParallelPhaseScriptRunner.start(): no worker processes launched.");
            stop();
            return false;
        }

        // Collectively wait until at least one READY(ok=true) OR all failed/exited
        size_t ready_ok = 0, failed = 0;
        for (;;) {
            ready_ok = failed = 0;

            for (auto& w : workers_) {
                if (!w->running.load()) continue; // not launched/parked
                if (w->proc->is_ready()) ++ready_ok;
                else if (w->proc->is_failed()) ++failed;
                else {
                    // still pending; also consider if the child exited before READY
                    DWORD s = WaitForSingleObject(w->proc->process_handle(), 0);
                    if (s == WAIT_OBJECT_0 && !w->proc->is_ready() && !w->proc->is_failed()) {
                        // Exited without MSG_READY — treat as failure
                        ++failed;
                    }
                }
            }

            if (ready_ok > 0) break;              // success: at least one usable worker
            if (failed == launched) break;        // all launched workers failed

            Sleep(10); // minimal spin; no hard timeout per your request
        }

        if (ready_ok == 0) {
            // Log per-worker error codes for clarity
            for (auto& w : workers_) {
                if (!w->running.load()) continue;
                if (w->proc->is_failed()) {
                    uint32_t err = w->proc->ready_error();
                    const char* why = "unknown";
                    switch (err) {
                    case WERR_SysMissing: why = "Sys missing"; break;
                    case WERR_BootFail:   why = "Boot failed"; break;
                    case WERR_LoadGame:   why = "LoadGame failed"; break;
                    case WERR_VMInit:     why = "VM init failed"; break;
                    case WERR_WriteReady: why = "Write READY failed"; break;
                    default: break;
                    }
                    SCLOGE("[Runner %zu] init failed (err=%u: %s)", w->id, err, why);
                }
                else {
                    // exited silently before READY
                    SCLOGE("[Runner %zu] exited before READY", w->id);
                }
            }
            SCLOGE("ParallelPhaseScriptRunner.start(): all workers failed to initialize.");
            stop();
            return false;
        }

        return true; // at least one worker is ready; others will join as they become ready
    }

    uint64_t ParallelPhaseScriptRunner::reconfigure(const PSInit& init, const PhaseScript& program)
    {
        const uint64_t e = epoch_.fetch_add(1) + 1;
        for (auto& w : workers_) {
            CtrlCmd c{}; 
            c.type = CtrlType::Reconfigure; 
            c.rc = CtrlReconfig{ e, init, program };
            //w->ctrl->push(c);
        }
        return e;
    }

    uint64_t ParallelPhaseScriptRunner::submit(const PSJob& job)
    {
        const uint64_t id = job_seq_.fetch_add(1) + 1;
        CmdJob cj{ id, epoch_.load(), job };
        jobs_->push(std::move(cj));
        return id;
    }

    bool ParallelPhaseScriptRunner::try_get_result(PRResult& outv)
    {
        bool popped = out_->try_pop(outv);
        if (popped) workers_.at(outv.worker_id)->jobs_done++;  // WARNING: to keep this in sync, never remove a worker from this vector, only add new ones.
        return popped;
    }

    PRStatus ParallelPhaseScriptRunner::status() const
    {
        PRStatus s{};
        s.epoch = epoch_.load();
        s.queued_jobs = jobs_->size();
        size_t rw = 0;
        for (auto& w : workers_) if (w->running.load()) ++rw;
        s.running_workers = rw;
        s.workers = workers_.size();
        return s;
    }

    void ParallelPhaseScriptRunner::stop()
    {
        if (stop_.exchange(true)) return;
        jobs_->close();
        for (auto& w : workers_) {
            if (w->running.exchange(false)) {
                if (w->proc) w->proc->stop();
            }
        }
        for (auto& w : workers_) {
            if (w->th.joinable()) w->th.join();
        }
    }

} // namespace simcore
