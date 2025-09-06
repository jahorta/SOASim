#define NOMINMAX
#include "TASMoviePlayer.h"
#include "utils.h"
#include <cstdio>
#include <iostream>

#include <Runner/Breakpoints/PreBattleBreakpoints.h>
#include <Runner/IPC/Wire.h>



static void sandbox::menu_tas_movie(AppState g)
{
    std::string dtm_path;
    std::string out_dir;            // directory to write .sav files per RTC
    std::string bp_override_file;
    uint32_t timeout_ms = 60000;
    uint32_t rtc_start = 0;         // UNIX seconds
    uint32_t rtc_end = 0;         // UNIX seconds
    char require_id6[7] = "GEAE8P";

    for (;;) {
        std::cout << "\n--- TAS Movie -> BP -> Savestate ---\n";
        std::cout << "ISO:                 " << (g.iso_path.empty() ? "<unset>" : g.iso_path) << "\n";
        std::cout << "Dolphin base:        " << (g.qt_base_dir.empty() ? "<unset>" : g.qt_base_dir) << "\n";
        std::cout << "Workers:             " << g.workers << "\n";
        std::cout << "DTM path:            " << (dtm_path.empty() ? "<unset>" : dtm_path) << "\n";
        std::cout << "Output dir:          " << (out_dir.empty() ? "<unset>" : out_dir) << "\n";
        std::cout << "RTC range (start..end): " << rtc_start << " .. " << rtc_end << "\n";
        std::cout << "Timeout (ms):        " << timeout_ms << "\n";
        std::cout << "BP override file:    " << (bp_override_file.empty() ? "<none>" : bp_override_file) << "\n";
        std::cout << "Require disc ID6:    " << require_id6 << "\n";
        std::cout << "1) Set DTM path\n"
            "2) Set output directory\n"
            "3) Set RTC start\n"
            "4) Set RTC end\n"
            "5) Set timeout (ms)\n"
            "6) Set BP override file (blank clears)\n"
            "7) Set require ID6 (6 chars)\n"
            "r) Run\n"
            "b) Back\n> ";
        std::string c; if (!std::getline(std::cin, c)) return;

        if (c == "1") dtm_path = prompt_path("DTM path: ", true, false, dtm_path).string();
        else if (c == "2") out_dir = prompt_path("Output directory: ", false, false, out_dir).string();
        else if (c == "3") { std::cout << "RTC start (UNIX seconds): "; std::string s; std::getline(std::cin, s); if (!s.empty()) rtc_start = std::stoul(s); }
        else if (c == "4") { std::cout << "RTC end   (UNIX seconds): "; std::string s; std::getline(std::cin, s); if (!s.empty()) rtc_end = std::stoul(s); }
        else if (c == "5") { std::cout << "Timeout (ms): "; std::string s; std::getline(std::cin, s); if (!s.empty()) timeout_ms = std::max(1u, (uint32_t)std::stoul(s)); }
        else if (c == "6") { bp_override_file = prompt_path("BP override file (blank clears): ", true, true, bp_override_file).string(); }
        else if (c == "7") { std::cout << "ID6 (exactly 6 chars): "; std::string s; std::getline(std::cin, s); if (s.size() == 6) memcpy(require_id6, s.data(), 6), require_id6[6] = '\0'; }
        else if (c == "r" || c == "R")
        {
            if (g.iso_path.empty() || g.qt_base_dir.empty() || dtm_path.empty() || out_dir.empty()) {
                std::cout << "Please set ISO, Dolphin base, DTM, and output dir first.\n";
                continue;
            }
            if (rtc_end < rtc_start) { std::cout << "RTC end must be >= start.\n"; continue; }
            if (!ensure_sys_from_base_or_warn(g.qt_base_dir)) continue;

            BreakpointMap bpmap = battle_rng_probe::defaults();
            if (!bp_override_file.empty()) bpmap = battle_rng_probe::load_from_file(bp_override_file);

            simcore::PSInit init{};
            init.savestate_path.clear();
            init.default_timeout_ms = timeout_ms;                                            // :contentReference[oaicite:7]{index=7}

            simcore::BootPlan boot = make_boot_plan(g);

            simcore::ParallelPhaseScriptRunner runner(g.workers);
            if (!runner.start(boot)) { std::cout << "Failed to start runner.\n"; continue; } // :contentReference[oaicite:8]{index=8}

            // 3-step control: set program kinds + init+activate
            if (!runner.set_program(simcore::PK_TasMovie, simcore::PK_TasMovie, init)) { std::cout << "set_program failed\n"; runner.stop(); continue; } // :contentReference[oaicite:9]{index=9}
            if (!runner.run_init_once()) { std::cout << "run_init_once failed\n"; runner.stop(); continue; }                                       // :contentReference[oaicite:10]{index=10}
            if (!runner.activate_main()) { std::cout << "activate_main failed\n"; runner.stop(); continue; }                                       // :contentReference[oaicite:11]{index=11}

            // Build jobs for each RTC; per-job save filename
            struct JobMeta { uint64_t id; uint32_t rtc; std::string save_path; };
            std::vector<JobMeta> metas;
            metas.reserve((size_t)(rtc_end - rtc_start + 1));

            fs::create_directories(out_dir);
            for (uint32_t rtc = rtc_start; rtc <= rtc_end; ++rtc) {
                simcore::PSJob j{};
                j.input = simcore::GCInputFrame{};     // placeholder; actual movie ops ignore input
                const uint64_t id = runner.submit(j);  // enqueues with current epoch                                   :contentReference[oaicite:12]{index=12}
                char namebuf[64];
                std::snprintf(namebuf, sizeof(namebuf), "movie_%u.sav", rtc);
                metas.push_back(JobMeta{ id, rtc, (fs::path(out_dir) / namebuf).string() });
            }

            std::cout << "Dispatched " << metas.size() << " jobs.\n";

            // Collect results
            size_t done = 0;
            while (done < metas.size()) {
                simcore::PRResult r{};
                if (runner.try_get_result(r)) {                                                                              // :contentReference[oaicite:13]{index=13}
                    ++done;
                    const auto it = std::find_if(metas.begin(), metas.end(), [&](const JobMeta& m) { return m.id == r.job_id; });
                    const uint32_t rtc = (it != metas.end()) ? it->rtc : 0;
                    std::cout << "job " << r.job_id << " rtc=" << rtc
                        << " ok=" << (r.ps.ok ? "1" : "0")
                        << " last_pc=0x" << std::hex << std::uppercase << r.ps.last_hit_pc << std::dec
                        << "\n";
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }

            runner.stop();
        }
        else if (c == "b" || c == "B") return;
    }
}