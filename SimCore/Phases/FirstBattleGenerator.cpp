#include "FirstBattleGenerator.h"

#include <filesystem>
#include <unordered_map>
#include <optional>
#include <algorithm>

#include "../Utils/Log.h"
#include "Programs/PlayTasMovie/TasMoviePayload.h"
#include "Programs/PlayTasMovie/TasMovieScript.h"
#include "../Tas/DtmFile.h"
#include "../Runner/IPC/Wire.h"

namespace fs = std::filesystem;

namespace simcore::tas_movie {

    static std::string basename_no_ext(const std::string& p) {
        fs::path x(p);
        auto stem = x.stem().string();
        return stem;
    }

    static std::string join(const std::string& a, const std::string& b) {
        return (fs::path(a) / b).string();
    }

    // Clone a DTM and adjust its recording start time by delta_sec; return new path.
    static std::optional<std::string> write_dtm_with_delta(const std::string& base_dtm,
        int delta_sec,
        const std::string& out_dir)
    {
        simcore::tas::DtmFile f;
        if (!f.load(base_dtm)) return std::nullopt;
        const auto info = f.info();

        // Name like: <stem>_rtc+0005.dtm (sign-aware, zero-padded 4+)
        char suffix[32];
        std::snprintf(suffix, sizeof(suffix), "_rtc%+05d.dtm", delta_sec);
        const std::string out_name = basename_no_ext(base_dtm) + suffix;

        fs::create_directories(out_dir);
        const std::string out_path = join(out_dir, out_name);

        f.set_recording_start_time(info.recording_start_time + static_cast<uint64_t>(delta_sec));
        if (!f.save(out_path)) return std::nullopt;
        return out_path;
    }

    BatchResult RunTasMovieConductor(ParallelPhaseScriptRunner& runner, const ConductorArgs& args)
    {
        BatchResult br{};
        if (args.rtc_delta_hi < args.rtc_delta_lo) return br;

        // Ensure PK_TasMovie is configured & active.
        PSInit init{};
        init.savestate_path = ""; // start from boot or whatever your phase expects
        init.default_timeout_ms = 600000; // a large cap; per-job SET_TIMEOUT_FROM will override

        // Boot workers (control mode)
        if (!runner.start(args.boot)) {
            SCLOGE("[tas] start() failed");
            return br;
        }

        if (!runner.set_program(/*init_kind=*/PK_None, /*main_kind=*/PK_TasMovie, init)) {
            SCLOGE("[tas] set_program failed");
            return br;
        }
        if (!runner.activate_main()) {
            SCLOGE("[tas] activate_main failed");
            return br;
        }

        // Prepare and submit all jobs.
        std::unordered_map<uint64_t, ItemResult> by_id;
        for (int d = args.rtc_delta_lo; d <= args.rtc_delta_hi; ++d) {
            const auto dtm_out = write_dtm_with_delta(args.base_dtm, d, args.out_dtm_dir);
            if (!dtm_out.has_value()) {
                SCLOGW("[tas] skip delta %d: failed to write dtm", d);
                continue;
            }

            // Save path mirrors DTM name with .sav
            fs::create_directories(args.out_sav_dir);
            fs::path sav_path = fs::path(args.out_sav_dir) / (fs::path(*dtm_out).stem().string() + ".sav");

            // Build payload: let the worker/decoder read DTM to derive run_ms, id6, etc.
            simcore::tasmovie::EncodeSpec spec{};
            spec.dtm_path = *dtm_out;
            spec.save_dir = args.out_sav_dir;   // directory only; worker derives <stem>.sav
            spec.run_ms = 0;                  // 0 => derive from DTM header
            spec.vi_stall_ms = args.vi_stall_ms;   // stall window for VI watchdog
            spec.save_on_fail = args.save_on_fail;

            std::vector<uint8_t> blob;
            if (!simcore::tasmovie::encode_payload(spec, blob)) {
                SCLOGW("[tas] skip delta %d: payload encode failed", d);
                continue;
            }

            PSJob job{};
            job.payload = std::move(blob);

            const uint64_t jid = runner.submit(job);

            ItemResult it{};
            it.delta_sec = d;
            it.dtm_path = *dtm_out;
            it.save_path = sav_path.string();
            it.job_id = jid;

            by_id.emplace(jid, std::move(it));
            ++br.submitted;
        }

        // Drain results.
        size_t done = 0;
        while (done < by_id.size()) {
            PRResult r{};
            if (!runner.try_get_result(r)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            ++done;

            auto it = by_id.find(r.job_id);
            if (it == by_id.end()) continue;

            it->second.ok = r.ps.ok;
            it->second.last_pc = r.ps.last_hit_pc;

            if (r.ps.ok) ++br.succeeded;
        }

        // Move to output vector in delta order
        br.items.reserve(by_id.size());
        for (auto& kv : by_id) br.items.push_back(std::move(kv.second));
        std::sort(br.items.begin(), br.items.end(), [](const ItemResult& a, const ItemResult& b) {
            return a.delta_sec < b.delta_sec;
            });
        return br;
    }

} // namespace simcore::tas_movie
