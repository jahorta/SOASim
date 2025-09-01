#include "Runner/Script/PhaseScriptVM.h"
#include "Runner/Script/Programs/SeedProbeScript.h"
#include "Runner/Breakpoints/PreBattleBreakpoints.h"
#include "SeedProbe.h"  // result types + grid helpers + summary (provided)  :contentReference[oaicite:0]{index=0}

#include <fstream>

namespace simcore {

    inline RandSeedProbeResult run_seed_probe_with_vm(simcore::DolphinWrapper& host,
        const std::string& savestate_path,
        uint32_t rng_addr,
        uint32_t timeout_ms,
        int samples_per_axis,
        bool cap_trigger_top)
    {
        BreakpointMap bpmap = battle_rng_probe::defaults();
        RandSeedProbeResult res{};
        PhaseScriptVM vm(host, bpmap);

        PSInit init{ savestate_path, timeout_ms };
        auto prog = MakeSeedProbeProgram(timeout_ms);

        if (!vm.init(init, prog)) {
            SCLOGI("[SeedProbeVM] init failed");
            return res;
        }

        auto run_one = [&](SeedFamily fam, const GCInputFrame& f, float x, float y)->RandSeedEntry {
            PSJob job{ f };
            auto R = vm.run(job);
            RandSeedEntry e{};
            e.samples_per_axis = samples_per_axis;
            e.family = fam;
            e.x = x; e.y = y;
            e.ok = R.ok;
            if (R.ok) {
                // pick up the emitted seed
                e.seed = std::get<uint32_t>(R.ctx.at("seed"));
            }
            return e;
            };

        // Neutral
        {
            GCInputFrame neutral{};
            auto e = run_one(SeedFamily::Neutral, neutral, 0, 0);
            e.label = "Neutral";
            res.entries.push_back(e);
            res.base_seed = (e.ok ? e.seed : 0);
            if (!e.ok) return res;
        }

        // Grids (reuse your builders)  :contentReference[oaicite:1]{index=1}
        const int N = samples_per_axis;
        auto g_main = build_grid_main(N);
        auto g_cstick = build_grid_cstick(N);
        auto g_trig = build_grid_triggers(N, cap_trigger_top);
        auto total = N * N;

        // Main stick grid
        uint64_t done = 0;
        for (const auto& f : g_main) {
            auto e = run_one(SeedFamily::Main, f, f.main_x, f.main_y);
            draw_progress_bar("JStick ", ++done, total);

            e.label = "Main(" + std::to_string(int(f.main_x)) + "," + std::to_string(int(f.main_y)) + ")";
            res.entries.push_back(e);
        }
        // C-stick grid
        done = 0;
        for (const auto& f : g_cstick) {
            auto e = run_one(SeedFamily::CStick, f, f.c_x, f.c_y);
            draw_progress_bar("CStick ", ++done, total);

            e.label = "CStick(" + std::to_string(int(f.c_x)) + "," + std::to_string(int(f.c_y)) + ")";
            res.entries.push_back(e);
        }
        // Triggers grid
        done = 0;
        for (const auto& f : g_trig) {
            auto e = run_one(SeedFamily::Triggers, f, f.trig_l, f.trig_r);
            draw_progress_bar("Triggers ", ++done, total);

            e.label = "Triggers(L=" + std::to_string(int(f.trig_l)) + ",R=" + std::to_string(int(f.trig_r)) + ")";
            res.entries.push_back(e);
        }

        // Compute deltas & print summary using your helpers  :contentReference[oaicite:2]{index=2}
        for (auto& e : res.entries) if (e.ok) e.delta = (int64_t)(int32_t)e.seed - (int64_t)(int32_t)res.base_seed;

        print_family_grid(res, SeedFamily::Main, N, "JStick ");
        print_family_grid(res, SeedFamily::CStick, N, "CStick ");
        print_family_grid(res, SeedFamily::Triggers, N, "Triggers ");

        std::ofstream outputFile("RngSeedOutput.csv");
        if (outputFile.is_open()) {
            for (auto e : to_csv_lines(res))
                outputFile << e << std::endl;
            outputFile.close();
        }

        return res;
    }

} // namespace simcore
