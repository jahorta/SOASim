#include "RngSeedDeltaMap.h"
#include <unordered_map>
#include <optional>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdio>

#include "../Utils/Log.h"
#include "../Runner/Script/PhaseScriptVM.h"                // PSJob/PSResult
#include "../Runner/Script/Programs/SeedProbeScript.h"     // MakeSeedProbeProgram
#include "../Runner/IPC/Wire.h"
#include "../Utils/MultiProgress.h"

using simcore::utils::MultiProgress;
using simcore::utils::MPBarSpec;

namespace simcore {

    static GCInputFrame neutral_frame() { return GCInputFrame{}; }

    static std::vector<uint8_t> linspace_u8(int n, int minv, int maxv) {
        std::vector<uint8_t> v; v.reserve((size_t)n);
        if (n <= 1) { v.push_back(uint8_t((minv + maxv) / 2)); return v; }
        const float step = float(maxv - minv) / float(n - 1);
        for (int i = 0; i < n; ++i) v.push_back(uint8_t(std::clamp(int(minv + i * step), 0, 255)));
        return v;
    }

    static std::vector<GCInputFrame> build_grid_main(int n, int minv, int maxv) {
        auto xs = linspace_u8(n, minv, maxv);
        auto ys = linspace_u8(n, minv, maxv);
        std::vector<GCInputFrame> out; out.reserve((size_t)n * (size_t)n);
        for (auto y : ys) for (auto x : xs) { GCInputFrame f{}; f.main_x = x; f.main_y = y; out.push_back(f); }
        return out;
    }

    static std::vector<GCInputFrame> build_grid_cstick(int n, int minv, int maxv) {
        auto xs = linspace_u8(n, minv, maxv);
        auto ys = linspace_u8(n, minv, maxv);
        std::vector<GCInputFrame> out; out.reserve((size_t)n * (size_t)n);
        for (auto y : ys) for (auto x : xs) { GCInputFrame f{}; f.c_x = x; f.c_y = y; out.push_back(f); }
        return out;
    }

    static std::vector<GCInputFrame> build_grid_trig(int n, int minv, int maxv, bool cap_top) {
        auto ls = linspace_u8(n, minv, maxv);
        auto rs = linspace_u8(n, minv, maxv);
        if (cap_top) {
            if (!ls.empty()) ls.back() = std::min<uint8_t>(ls.back(), 0xFF);
            if (!rs.empty()) rs.back() = std::min<uint8_t>(rs.back(), 0xFF);
        }
        std::vector<GCInputFrame> out; out.reserve((size_t)n * (size_t)n);
        for (auto r : rs) for (auto l : ls) { GCInputFrame f{}; f.trig_l = l; f.trig_r = r; out.push_back(f); }
        return out;
    }

    static inline long long signed_delta(uint32_t a, uint32_t b) {
        return (long long)(int32_t)a - (long long)(int32_t)b;
    }

    static std::string make_label(const char* title, uint8_t x, uint8_t y) {
        char buf[64]; std::snprintf(buf, sizeof(buf), "%s(%02X,%02X)", title, int(x), int(y));
        return std::string(buf);
    }

    RandSeedProbeResult RunRngSeedDeltaMap(ParallelPhaseScriptRunner& runner, const RngSeedDeltaArgs& args)
    {
        
        SCLOGI("[seedmap] Starting workers ...");
        
        // New control-mode startup
        if (!runner.start(args.boot)) {
            SCLOGE("Failed to boot workers (control mode).");
            return {}; // or propagate error
        }

        PSInit init{};
        init.savestate_path = args.savestate_path;      // keep using your existing savestate for this phase
        init.default_timeout_ms = args.run_timeout_ms;

        SCLOGI("[seedmap] Setting program ...");

        // No special INIT program for seed probe; main program is SeedProbe
        if (!runner.set_program(/*init_kind=*/PK_None, /*main_kind=*/PK_SeedProbe, init)) {
            SCLOGE("Failed to set program on workers.");
            return {};
        }

        SCLOGI("[seedmap] Activating program ...");

        // No init-once step for seed probe; go straight to main
        if (!runner.activate_main()) {
            SCLOGE("Failed to activate main program.");
            return {};
        }

        SCLOGI("[seedmap] Building Jobs ...");

        RandSeedProbeResult out{};

        std::vector<std::pair<SeedFamily, std::vector<GCInputFrame>>> batches;
        batches.emplace_back(SeedFamily::Neutral, std::vector<GCInputFrame>{ neutral_frame() });
        batches.emplace_back(SeedFamily::Main, build_grid_main(args.samples_per_axis, args.min_value, args.max_value));
        batches.emplace_back(SeedFamily::CStick, build_grid_cstick(args.samples_per_axis, args.min_value, args.max_value));
        batches.emplace_back(SeedFamily::Triggers, build_grid_trig(args.samples_per_axis, args.min_value, args.max_value, args.cap_trigger_top));

        std::vector<MPBarSpec> specs;
        specs.push_back({ "Neutral", 1 });
        specs.push_back({ "Main",    (uint64_t)args.samples_per_axis * args.samples_per_axis });
        specs.push_back({ "CStick",  (uint64_t)args.samples_per_axis * args.samples_per_axis });
        specs.push_back({ "Triggers",(uint64_t)args.samples_per_axis * args.samples_per_axis });

        MultiProgress::Options mpopt;
        mpopt.use_stdout = true;
        mpopt.use_vt = true;
        mpopt.bar_width = 40;

        MultiProgress mp;
        mp.init(specs, mpopt);
        mp.start();

        // When building and dispatching each batch, keep the family index:
        auto family_index = [&](SeedFamily fam)->size_t {
            switch (fam) {
            case SeedFamily::Neutral:  return 0;
            case SeedFamily::Main:     return 1;
            case SeedFamily::CStick:   return 2;
            case SeedFamily::Triggers: return 3;
            }
            return 0;
            };


        SCLOGI("[seedmap] Sending Jobs ...");

        for (auto& [fam, inputs] : batches) {
            
            const size_t fi = family_index(fam);
            
            std::vector<PSJob> jobs; jobs.reserve(inputs.size());
            for (auto& f : inputs) jobs.emplace_back(PSJob{ f });

            std::unordered_map<uint64_t, GCInputFrame> lookup;
            lookup.reserve(jobs.size());

            for (const auto& j : jobs) {
                const uint64_t id = runner.submit(j);
                lookup.emplace(id, j.input);
            }

            size_t done = 0, total = jobs.size();
            while (done < total) {
                PRResult r{};
                if (runner.try_get_result(r)) {
                    ++done;

                    mp.tick(fi);

                    std::optional<uint32_t> seed;
                    auto it = r.ps.ctx.find("seed");
                    if (it != r.ps.ctx.end()) {
                        if (auto p = std::get_if<uint32_t>(&it->second)) seed = *p;
                    }

                    if (fam == SeedFamily::Neutral && seed.has_value())
                        out.base_seed = seed.value();

                    if (seed.has_value()) {
                        const auto& in = lookup.find(r.job_id)->second;
                        float vx = 0.f, vy = 0.f;
                        const char* title = "";
                        switch (fam) {
                        case SeedFamily::Main:     vx = in.main_x;  vy = in.main_y;  title = "JStick";   break;
                        case SeedFamily::CStick:   vx = in.c_x;      vy = in.c_y;      title = "CStick";   break;
                        case SeedFamily::Triggers: vx = in.trig_l;   vy = in.trig_r;   title = "Triggers"; break;
                        case SeedFamily::Neutral:  vx = 0;           vy = 0;           title = "Neutral";  break;
                        }

                        out.entries.push_back(RandSeedEntry{
                            args.samples_per_axis, fam, vx, vy,
                            seed.value(),
                            signed_delta(seed.value(), out.base_seed),
                            r.ps.ok,
                            make_label(title, uint8_t(vx), uint8_t(vy))
                            });

                        SCLOGT("[Result] job=%llu worker=%zu accepted=%d ok=%d pc=%08X seed=0x%08X",
                            (unsigned long long)r.job_id, r.worker_id,
                            int(r.accepted), int(r.ps.ok), r.ps.last_hit_pc, seed.value());
                    }
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
            }
        }

        mp.finish();

        std::sort(out.entries.begin(), out.entries.end(), [](const RandSeedEntry& a, const RandSeedEntry& b) {
            if (a.family != b.family) return a.family < b.family;
            if (a.y == b.y) return a.x < b.x;
            return a.y < b.y;
            });

        return out;
    }

} // namespace simcore
