// SimCoreWorker.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// SimCoreWorker/WorkerMain.cpp
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "Utils/Log.h"
#include "Boot/Boot.h"
#include "Core/DolphinWrapper.h"
#include "Runner/Breakpoints/BPCore.h"
#include "Runner/Script/PhaseScriptVM.h"
#include "Runner/Script/Programs/SeedProbeScript.h"
#include "Runner/Parallel/ParallelPhaseScriptRunner.h"
#include "Runner/IPC/Wire.h"
#include "Runner/Breakpoints/PreBattleBreakpoints.h"

#include <windows.h>
#include <Utils/ThreadName.h>

using namespace simcore;

static std::wstring exe_dir_w() {
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf); size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

static uint32_t parse_hex_u32(const char* s) {
    return s ? static_cast<uint32_t>(std::strtoul(s, nullptr, 16)) : 0u;
}
static uint32_t parse_u32(const char* s) {
    return s ? static_cast<uint32_t>(std::strtoul(s, nullptr, 10)) : 0u;
}
static const char* argv_next(int& i, int argc, char** argv) { return (i + 1 < argc) ? argv[++i] : ""; }

static enum WorkerExitCode : uint8_t {
    INVALID_HANDLES = 100,
    SEND_READY_FAILED
};

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

int main(int argc, char** argv)
{
    // args:
    // --id N --iso <path> --savestate <path> --qtbase <dir> --userdir <dir> [--log <file>]
    size_t worker_id = 0;
    std::string iso, sav, qtbase, userdir, logfile; 
    uint32_t timeout_ms = 10000;

    for (int i = 1; i < argc; i++) {
        std::string k = argv[i];
        if (k == "--id") worker_id = parse_u32(argv_next(i, argc, argv));
        else if (k == "--iso") iso = argv_next(i, argc, argv);
        else if (k == "--savestate") sav = argv_next(i, argc, argv);
        else if (k == "--qtbase") qtbase = argv_next(i, argc, argv);
        else if (k == "--userdir") userdir = argv_next(i, argc, argv);
        else if (k == "--timeout") timeout_ms = parse_u32(argv_next(i, argc, argv));
    }

    set_this_thread_name_utf8((std::string("WorkerMain-") + std::to_string(worker_id)).c_str());

    auto& L = simcore::log::Logger::get();
    L.set_levels(simcore::log::Level::Off, simcore::log::Level::Trace);
    if (!logfile.empty()) L.open_file((std::filesystem::path(userdir) / "worker.log").string().c_str(), /*append=*/false);
    
    SCLOGI("[Worker %zu] Initializing", worker_id);

    // Use inherited anonymous pipes as binary channels
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hIn == NULL || hIn == INVALID_HANDLE_VALUE || hOut == NULL || hOut == INVALID_HANDLE_VALUE) {
        SCLOGE("[Worker %zu] invalid std handles", worker_id);
        return WorkerExitCode::INVALID_HANDLES;

    auto sys_dsp = std::filesystem::path(exe_dir_w()) / "Sys" / "GC" / "dsp_coef.bin";
    if (!std::filesystem::exists(sys_dsp)) {
        WireReady wr{}; wr.tag = MSG_READY; wr.ok = 0; wr.error = WERR_SysMissing;
        (void)write_all(hOut, &wr, sizeof(wr));
        SCLOGE("[Worker %zu] Missing Sys beside exe (%ws). Ensure parent copied from --qtbase.", worker_id, sys_dsp.c_str());
        return WERR_SysMissing;
    }
    }

    BootPlan boot{};
    boot.boot.user_dir = userdir;
    boot.boot.dolphin_qt_base = qtbase;
    boot.boot.force_resync_from_base = true;
    boot.boot.save_config_on_success = false;
    boot.iso_path = iso;

    DolphinWrapper host;
    std::string err;
    if (!simboot::BootDolphinWrapper(host, boot.boot, &err)) {
        WireReady wr{}; wr.tag = MSG_READY; wr.ok = 0; wr.error = WERR_BootFail;
        (void)write_all(hOut, &wr, sizeof(wr));
        SCLOGE("[Worker %zu] Boot failed: %s", worker_id, err.c_str());
        return WERR_BootFail;
    }
    if (!host.loadGame(boot.iso_path)) {
        WireReady wr{}; wr.tag = MSG_READY; wr.ok = 0; wr.error = WERR_LoadGame;
        (void)write_all(hOut, &wr, sizeof(wr));
        SCLOGE("[Worker %zu] loadGame failed: %s", worker_id, boot.iso_path.c_str());
        return WERR_LoadGame;
    }

    host.ConfigurePortsStandardPadP1();

    BreakpointMap bpmap = battle_rng_probe::defaults();

    PhaseScript program = MakeSeedProbeProgram(timeout_ms);

    PSInit init{};
    init.savestate_path = sav;
    init.default_timeout_ms = timeout_ms;

    PhaseScriptVM vm(host, bpmap);
    if (!vm.init(init, program)) {
        WireReady wr{}; wr.tag = MSG_READY; wr.ok = 0; wr.error = WERR_VMInit;
        (void)write_all(hOut, &wr, sizeof(wr));
        SCLOGE("[Worker %zu] VM init failed", worker_id);
        return WERR_VMInit;
    }

    WireReady wrdy{}; wrdy.tag = MSG_READY; wrdy.ok = 1;
    if (!write_all(hOut, &wrdy, sizeof(wrdy))) {
        SCLOGE("[Worker %zu] failed to write READY", worker_id);
        return WorkerExitCode::SEND_READY_FAILED;
    }

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    for (;;) {
        WireJob wj{};
        if (!read_all(hIn, &wj, sizeof(wj))) break;  // EOF/closed pipe -> exit
        if (wj.tag != MSG_JOB) continue;

        GCInputFrame f{};
        f.main_x = wj.main_x; f.main_y = wj.main_y;
        f.c_x = wj.c_x; f.c_y = wj.c_y;
        f.trig_l = wj.trig_l; f.trig_r = wj.trig_r;
        f.buttons = wj.buttons;

        PSJob job{ f };
        auto R = vm.run(job);

        WireResult wr{};
        wr.tag = MSG_RESULT;
        wr.job_id = wj.job_id;
        wr.epoch = wj.epoch;
        wr.ok = R.ok ? 1 : 0;
        wr.last_pc = R.last_hit_pc;
        auto it = R.ctx.find("seed");
        if (it != R.ctx.end()) {
            if (auto p = std::get_if<uint32_t>(&it->second)) wr.seed = *p;
        }

        if (!write_all(hOut, &wr, sizeof(wr))) break;
    }

    return WERR_None;
}
