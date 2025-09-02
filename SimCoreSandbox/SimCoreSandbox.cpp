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

using namespace simcore;

static uint32_t parse_hex_u32(const char* s) {
    return static_cast<uint32_t>(std::strtoul(s, nullptr, 16));
}

void run_family(ParallelPhaseScriptRunner& runner, const SeedFamily fam, const int N, int min_value, int max_value, RandSeedProbeResult& res) {
    
    int res_entries_size = res.entries.size();
    res.entries.reserve((N * N) + res.entries.size());

    InputPlan inputs;
    std::string title;
    switch (fam) {
    case SeedFamily::Neutral:
        inputs.push_back({});
        break;
    case SeedFamily::Main:
        inputs = build_grid_main(N, min_value, max_value);
        title = "JStick";
        break;
    case SeedFamily::CStick:
        inputs = build_grid_cstick(N, min_value, max_value);
        title = "CStick";
        break;
    case SeedFamily::Triggers:
        inputs = build_grid_triggers(N, min_value, max_value, true);
        title = "Triggers";
        break;
    default:
        return;
    }

    std::vector<PSJob> jobs;
    jobs.reserve(N * N);
    for (auto i : inputs) {
        jobs.emplace_back(i);
    }

    SCLOGT("[sandbox] Sending Jobs");
    std::unordered_map<uint64_t, PSJob> job_lookup;
    for (const auto& j : jobs) {
        const uint64_t id = runner.submit(j);
        job_lookup.emplace(id, j);
    }

    const size_t total = jobs.size();
    size_t done = 0;
    std::vector<PRResult> results;
    results.reserve(total);

    SCLOGT("[sandbox] Receiving Results");
    while (done < total) {
        PRResult r{};
        if (runner.try_get_result(r)) {
            results.push_back(r);
            ++done;

            draw_progress_bar(title.c_str(), done, total);

            std::optional<uint32_t> seed;
            auto it = r.ps.ctx.find("seed");
            if (it != r.ps.ctx.end())
                if (auto p = std::get_if<uint32_t>(&it->second))
                {
                    seed = *p;
                    if (fam == SeedFamily::Neutral)
                        res.base_seed = seed.value();
                    else
                    {
                        auto input = job_lookup.find(r.job_id)->second.input;
                        float x, y;
                        switch (fam) {
                        case SeedFamily::Main:
                            x = input.main_x;
                            y = input.main_y;
                            break;
                        case SeedFamily::CStick:
                            x = input.c_x;
                            y = input.c_y;
                            break;
                        case SeedFamily::Triggers:
                            x = input.trig_l;
                            y = input.trig_r;
                            break;
                        default:
                            break;
                        }
                        
                        res.entries.push_back(RandSeedEntry{
                            N, fam, x, y, seed.value(), (int64_t)(int32_t)seed.value() - (int64_t)(int32_t)res.base_seed, /*ok*/true,
                            std::format("{}({:02X},{:02X})", title, int(x), int(y))
                            });
                    }
                }

            SCLOGT("[Result] job=%llu worker=%zu accepted=%d ok=%d pc=%08X seed=%s",
                static_cast<unsigned long long>(r.job_id),
                r.worker_id,
                int(r.accepted),
                int(r.ps.ok),
                r.ps.last_hit_pc,
                seed.has_value() ? std::format("{}", seed.value()).c_str() : "None");

        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

int main(int argc, char** argv) {
    auto iso = prompt_path("ISO path: ", true, true, std::string("D:\\SoATAS\\SkiesofArcadiaLegends(USA).gcm"));
    auto sav = prompt_path("Savestate path: ", true, true, std::string("D:\\SoATAS\\dolphin-2506a-x64\\User\\StateSaves\\GEAE8P.s04"));

    char exePath[MAX_PATH]{};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string base = exePath;
    auto pos = base.find_last_of("\\/");
    base = (pos == std::string::npos) ? "." : base.substr(0, pos);

    log::Logger::get().set_levels(log::Level::Info, log::Level::Debug);
    simcore::log::Logger::get().open_file((std::filesystem::path(base) / "sandbox.log").string().c_str(), false);
    SCLOGI("[sandbox] Starting...");

    const std::string iso_path = iso.string();
    const std::string savestate_path = sav.string();
    const std::string qt_base_dir = R"(D:\SoATAS\dolphin-2506a-x64)";
    const uint32_t   rng_addr = 0x803469A8u;
    const uint32_t   timeout_ms = 10000u;

    // CONSTANTS FOR CONTROL!!!!!
    const int    N = 20;
    const size_t workers = 5;
    const int    min_value = 0x30;
    const int    max_value = 0xD0;


    if (!simcore::EnsureSysBesideExe(qt_base_dir)) {
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
    runner.start(boot, psinit, program);

    RandSeedProbeResult results;

    // Build a small batch of jobs (neutral + a few sample stick positions).
    run_family(runner, SeedFamily::Neutral, 1, min_value, max_value, results);
    run_family(runner, SeedFamily::Main, N, min_value, max_value, results);
    run_family(runner, SeedFamily::CStick, N, min_value, max_value, results);
    run_family(runner, SeedFamily::Triggers, N, min_value, max_value, results);

    std::sort(results.entries.begin(), results.entries.end(), [](const RandSeedEntry& a, const RandSeedEntry& b) {
        if (a.family != b.family)
            return a.family < b.family;
        if (a.y == b.y)
            return a.x < b.x;
        return a.y < b.y;
    });

    print_family_grid(results, SeedFamily::Main, N, "JStick ");
    print_family_grid(results, SeedFamily::CStick, N, "CStick ");
    print_family_grid(results, SeedFamily::Triggers, N, "Triggers ");

    runner.stop();
    return 0;
    }