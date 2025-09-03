// SimCoreSandbox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define NOMINMAX
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <format>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>

#include <fstream>
#include <iostream>
#include <limits>

#include "Boot/Boot.h"
#include "Core/Input/InputPlan.h"
#include "Runner/Breakpoints/BPCore.h"
#include "Runner/Breakpoints/PreBattleBreakpoints.h"
#include "Runner/Parallel/ParallelPhaseScriptRunner.h"
#include "Runner/Script/Programs/SeedProbeScript.h"
#include "SeedProbe.h"
#include "prompt_path.cpp"
#include <Utils/EnsureSys.h>
#include <Phases/RNGSeedDeltaMap.h>

using namespace simcore;

int main(int argc, char** argv) {
    auto iso = prompt_path("ISO path: ", true, true, std::string("D:\\SoATAS\\SkiesofArcadiaLegends(USA).gcm"));
    auto sav = prompt_path("Savestate path: ", true, true, std::string("D:\\SoATAS\\dolphin-2506a-x64\\User\\StateSaves\\GEAE8P.s04"));

    char exePath[MAX_PATH]{};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string base = exePath;
    auto pos = base.find_last_of("\\/");
    base = (pos == std::string::npos) ? "." : base.substr(0, pos);

    log::Logger::get().set_levels(log::Level::Info, log::Level::Debug);
    log::Logger::get().open_file((std::filesystem::path(base) / "sandbox.log").string().c_str(), false);
    SCLOGI("[sandbox] Starting...");

    const std::string iso_path = iso.string();
    const std::string savestate_path = sav.string();
    const std::string qt_base_dir = R"(D:\SoATAS\dolphin-2506a-x64)";
    const uint32_t   rng_addr = 0x803469A8u;
    const uint32_t   timeout_ms = 10000u;

    // CONSTANTS FOR CONTROL!!!!!
    const int    N = 8;
    const size_t workers = 10;
    const int    min_value = 0x30;
    const int    max_value = 0xCF;


    if (!EnsureSysBesideExe(qt_base_dir)) {
        SCLOGE("EnsureSysBesideExe failed. Expected Sys under sandbox / worker exe directory.");
        return 1;
    }

    // PhaseScript: apply 1 frame input -> run until BP -> read RNG -> emit "seed".
    PhaseScript program = MakeSeedProbeProgram(timeout_ms);

    // VM init: initial savestate + default timeout.
    PSInit psinit{};
    psinit.savestate_path = savestate_path;
    psinit.default_timeout_ms = timeout_ms;

    // Boot plan: use your Boot module; keep ISO and portable base fixed for the lifetime of the pool.
    BootPlan boot{};
    boot.boot.user_dir = (std::filesystem::path(base) / ".work" / "runner").string(); // runner will derive per-thread dirs if needed
    boot.boot.dolphin_qt_base = qt_base_dir;
    boot.boot.force_resync_from_base = true;
    boot.boot.save_config_on_success = true;
    boot.iso_path = iso_path;

    // Runner
    ParallelPhaseScriptRunner runner(workers);
    RngSeedDeltaArgs args{};
    args.boot = boot;
    args.savestate_path = savestate_path;
    args.samples_per_axis = N;
    args.min_value = 0;
    args.max_value = 255;
    args.cap_trigger_top = true;
    args.run_timeout_ms = 10000;

    auto result = RunRngSeedDeltaMap(runner, args);

    // visualize however you like
    // e.g., print grids like your existing helpers

    sandbox::print_family_grid(result, SeedFamily::Main, args.samples_per_axis, "JStick ");
    sandbox::print_family_grid(result, SeedFamily::CStick, args.samples_per_axis, "CStick ");
    sandbox::print_family_grid(result, SeedFamily::Triggers, args.samples_per_axis, "Triggers ");

    runner.stop();
    return 0;
    }