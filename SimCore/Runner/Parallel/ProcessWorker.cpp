#include "ProcessWorker.h"
#include "../IPC/Wire.h"
#include <sstream>
#include "../../Utils/ThreadName.h"

namespace simcore {

    static bool CreateChild(const ProcStartParams& p,
        HANDLE& hInWrite, HANDLE& hOutRead,
        HANDLE& hProcess, HANDLE& hThread,
        unsigned long& dwProcessId)
    {
        SECURITY_ATTRIBUTES saAttr{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

        HANDLE hOutReadTmp = NULL, hOutWrite = NULL;
        if (!CreatePipe(&hOutReadTmp, &hOutWrite, &saAttr, 0)) return false;
        if (!SetHandleInformation(hOutReadTmp, HANDLE_FLAG_INHERIT, 0)) return false;

        HANDLE hInRead = NULL, hInWriteTmp = NULL;
        if (!CreatePipe(&hInRead, &hInWriteTmp, &saAttr, 0)) return false;
        if (!SetHandleInformation(hInWriteTmp, HANDLE_FLAG_INHERIT, 0)) return false;

        STARTUPINFOA si{}; si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hInRead;
        si.hStdOutput = hOutWrite;
        si.hStdError = hOutWrite;

        // Command line for worker
        std::ostringstream cmd;
        cmd << "\"" << p.exe_path << "\""
            << " --worker"
            << " --id " << p.worker_id
            << " --iso \"" << p.iso_path << "\""
            << " --savestate \"" << p.savestate_path << "\""
            << " --qtbase \"" << p.qt_base_dir << "\""
            << " --userdir \"" << p.user_dir << "\""
            << " --timeout " << std::dec << p.timeout_ms;

        PROCESS_INFORMATION pi{};
        std::string cmdline = cmd.str();
        BOOL ok = CreateProcessA(
            NULL, cmdline.data(), NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        // Close handles not needed by parent
        CloseHandle(hOutWrite);
        CloseHandle(hInRead);

        if (!ok) {
            CloseHandle(hOutReadTmp); CloseHandle(hInWriteTmp);
            return false;
        }

        hOutRead = hOutReadTmp;
        hInWrite = hInWriteTmp;
        hProcess = pi.hProcess;
        hThread = pi.hThread;
        dwProcessId = pi.dwProcessId;
        return true;
    }

    ProcessWorker::~ProcessWorker() { stop(); }

    bool ProcessWorker::start(ProcStartParams& p, TSQueue<PRResult>* outq)
    {
        out_ = outq;
        id_ = p.worker_id;
        if (!CreateChild(p, hChildStd_IN_Wr, hChildStd_OUT_Rd, hProcess, hThread, dwProcessId))
            return false;

        running_.store(true);
        ready_received_.store(false);
        ready_ok_.store(false);
        ready_error_.store(0);
        busy_.store(false);

        reader_ = std::thread(&ProcessWorker::reader_thread, this);

        return true;
    }

    static bool write_all(HANDLE h, const void* p, size_t n) {
        const BYTE* b = static_cast<const BYTE*>(p);
        DWORD w = 0;
        while (n) {
            if (!WriteFile(h, b, (DWORD)std::min(n, (size_t)0x7FFFFFFF), &w, NULL)) return false;
            if (w == 0) return false;
            b += w; n -= w;
        }
        return true;
    }

    static bool read_all(HANDLE h, void* p, size_t n) {
        BYTE* b = static_cast<BYTE*>(p);
        DWORD r = 0;
        while (n) {
            if (!ReadFile(h, b, (DWORD)std::min(n, (size_t)0x7FFFFFFF), &r, NULL)) return false;
            if (r == 0) return false;
            b += r; n -= r;
        }
        return true;
    }

    bool ProcessWorker::send_job(uint64_t job_id, uint64_t epoch, const GCInputFrame& f)
    {
        if (!running_.load()) return false;

        WireJob wj = make_wire_job(job_id, epoch, f);
        if (!write_all(hChildStd_IN_Wr, &wj, sizeof(wj))) 
        {
            release_slot();
            return false;
        }
        return true;
    }

    void ProcessWorker::reader_thread()
    {
        set_this_thread_name_utf8((std::string("WorkerReader-") + std::to_string(id_)).c_str());

        for (;;) {
            uint8_t tag = 0;
            DWORD got = 0;
            if (!ReadFile(hChildStd_OUT_Rd, &tag, 1, &got, NULL) || got == 0) break;

            if (tag == MSG_READY) {
                WireReady wrdy{};
                wrdy.tag = tag;
                if (!read_all(hChildStd_OUT_Rd, reinterpret_cast<char*>(&wrdy) + 1, sizeof(wrdy) - 1)) break;
                ready_ok_.store(wrdy.ok != 0);
                ready_error_.store(wrdy.error);
                ready_received_.store(true);
                // If failed, we can let the child exit naturally; parent will stop() it on start() failure
                continue;
            }

            if (tag == MSG_RESULT) {
                WireResult wr{};
                wr.tag = tag;
                if (!read_all(hChildStd_OUT_Rd, reinterpret_cast<char*>(&wr) + 1, sizeof(wr) - 1)) break;

                release_slot();

                PRResult r{};
                r.worker_id = id_;
                r.accepted = true;
                r.job_id = wr.job_id;
                r.epoch = wr.epoch;
                r.ps.ok = (wr.ok != 0);
                r.ps.last_hit_pc = wr.last_pc;
                r.ps.ctx["seed"] = wr.seed;
                out_->push(std::move(r));
                continue;
            }

            // ignore unknown tags
        }

        running_.store(false);
    }

    void ProcessWorker::stop()
    {
        if (!running_.exchange(false)) return;
        // Closing stdin signals EOF to worker loop
        if (hChildStd_IN_Wr) { CloseHandle(hChildStd_IN_Wr); hChildStd_IN_Wr = NULL; }
        if (reader_.joinable()) reader_.join();
        if (hChildStd_OUT_Rd) { CloseHandle(hChildStd_OUT_Rd); hChildStd_OUT_Rd = NULL; }
        if (hThread) { CloseHandle(hThread); hThread = NULL; }
        if (hProcess) { CloseHandle(hProcess); hProcess = NULL; }
    }

    void ProcessWorker::close_stdin() {
        if (hChildStd_IN_Wr) { CloseHandle(hChildStd_IN_Wr); hChildStd_IN_Wr = nullptr; }
    }
    bool ProcessWorker::wait(DWORD ms) const {
        if (!hProcess) return true;
        return WaitForSingleObject(hProcess, ms) == WAIT_OBJECT_0;
    }
    void ProcessWorker::terminate() {
        if (hProcess) TerminateProcess(hProcess, 0);
    }

} // namespace simcore
