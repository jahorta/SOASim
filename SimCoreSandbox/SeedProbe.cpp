#include "SeedProbe.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

#include <Utils/Log.h>
#include <Utils/DeltaColorizer.h>

namespace simcore {

    // ---------- progress bar helpers ----------
    void draw_progress_bar(const char* label, size_t done, size_t total)
    {
        if (total == 0) return;
        const int width = 40; // characters in the bar
        double frac = static_cast<double>(done) / static_cast<double>(total);
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        int filled = static_cast<int>(std::round(frac * width));

        std::printf("\r%-10s [", label);
        for (int i = 0; i < width; ++i)
            std::printf("%c", (i < filled) ? '#' : ' ');
        std::printf("] %3d%% (%zu/%zu)", static_cast<int>(std::round(frac * 100.0)), done, total);
        if (done >= total)
        {
            std::printf("\n");
        }
        std::fflush(stdout);
    }
    // -----------------------------------------
    
    static std::vector<uint8_t> levels_u8(uint8_t lo, uint8_t hi, int n, bool cap_top = false) {
        std::vector<uint8_t> v;
        if (n <= 0) return v;
        uint8_t top = cap_top ? static_cast<uint8_t>(hi - 1) : hi;
        if (n == 1) { v.push_back(static_cast<uint8_t>((int(lo) + int(top)) / 2)); return v; }
        v.reserve(n);
        double step = double(int(top) - int(lo)) / double(n - 1);
        for (int i = 0; i < n; ++i) {
            int val = int(std::round(double(lo) + step * i));
            if (val < lo) val = lo;
            if (val > top) val = top;
            v.push_back(static_cast<uint8_t>(val));
        }
        // Try to force exact 128 into the sequence for sticks when it should be near the middle.
        // (No-op for triggers where center isn't 128.)
        for (auto& x : v) {
            if (std::abs(int(x) - 128) <= 1) { x = 128; break; }
        }
        return v;
    }

    static GCInputFrame neutral_frame() {
        GCInputFrame f{};
        return f;
    }

    static GCInputFrame set_main(GCInputFrame f, float x, float y) {
        f.main_x = x; f.main_y = y;
        return f;
    }
    static GCInputFrame set_c(GCInputFrame f, float x, float y) {
        f.c_x = x; f.c_y = y;
        return f;
    }
    static GCInputFrame set_trig(GCInputFrame f, float l, float r) {
        f.trig_l = l; f.trig_r = r;
        return f;
    }

    std::vector<GCInputFrame> build_grid_main(int n, int min_value, int max_value) {
        std::vector<GCInputFrame> out;
        auto xs = levels_u8(min_value, max_value, n, false);
        auto ys = levels_u8(min_value, max_value, n, false);
        out.reserve(n * n);
        for (float y : ys) for (float x : xs) out.push_back(set_main(neutral_frame(), x, y));
        return out;
    }

    std::vector<GCInputFrame> build_grid_cstick(int n, int min_value, int max_value) {
        std::vector<GCInputFrame> out;
        auto xs = levels_u8(min_value, max_value, n, false);
        auto ys = levels_u8(min_value, max_value, n, false);
        out.reserve(n * n);
        for (float y : ys) for (float x : xs) out.push_back(set_c(neutral_frame(), x, y));
        return out;
    }

    std::vector<GCInputFrame> build_grid_triggers(int n, int min_value, int max_value, bool cap_top) {
        std::vector<GCInputFrame> out;
        auto ls = levels_u8(min_value, max_value, n, cap_top);
        auto rs = levels_u8(min_value, max_value, n, cap_top);
        out.reserve(n * n);
        for (float r : rs) for (float l : ls) out.push_back(set_trig(neutral_frame(), l, r));
        return out;
    }

    static long long signed_delta(uint32_t a, uint32_t b) {
        const long long sa = (long long)(int32_t)a;
        const long long sb = (long long)(int32_t)b;
        return sa - sb;
    }

    static void label_entry(RandSeedEntry& e) {
        switch (e.family) {
        case SeedFamily::Neutral:  e.label = "Neutral"; break;
        case SeedFamily::Main:     e.label = "Main(" + std::to_string(e.x) + "," + std::to_string(e.y) + ")"; break;
        case SeedFamily::CStick:   e.label = "CStick(" + std::to_string(e.x) + "," + std::to_string(e.y) + ")"; break;
        case SeedFamily::Triggers: e.label = "Triggers(L=" + std::to_string(e.x) + ",R=" + std::to_string(e.y) + ")"; break;
        }
    }

    void print_family_grid(const RandSeedProbeResult& r, SeedFamily fam, int N, const char* title)
    {
        std::vector<const RandSeedEntry*> entries;
        entries.reserve(N * N);
        for (const auto& e : r.entries)
            if (e.family == fam && e.samples_per_axis == N)
                entries.push_back(&e);

        if (entries.size() != static_cast<size_t>(N * N)) {
            std::printf("[SeedProbe] Grid for %s missing/size mismatch.\n", title);
            return;
        }

        color_lut_begin();
        for (auto& e : r.entries) color_lut_ingest(e.delta);
        color_lut_finalize();

        // X ticks from first row, Y ticks from first column
        std::vector<int> xvals(N), yvals(N);
        for (int col = 0; col < N; ++col) xvals[col] = static_cast<int>(entries[col]->x);
        for (int row = 0; row < N; ++row) yvals[row] = static_cast<int>(entries[row * N]->y);

        std::printf("[SeedProbe] %s delta grid (N=%d)\nlegend: ' *'(|delta|>FF), '-h'/' h' nibble, '00'..'FF' positive, ' 0' zero\n", title, N);

        // Top axis labels
        std::printf("    "); // left margin for Y label (3) + space
        for (int col = 0; col < N; ++col)
            std::printf("\x1b[2;37m%02X\x1b[0m ", xvals[col]);
        std::printf("\n");

        // Rows with left/right Y labels
        size_t idx = 0;
        for (int row = 0; row < N; ++row) {
            // Left Y label
            std::printf(" \x1b[2;37m%02X\x1b[0m ", yvals[row]);

            for (int col = 0; col < N; ++col) {
                const auto* p = entries[idx++];
                long long d = (p->ok ? p->delta : 0x7fff); // treat missing as big
                std::printf("%s%s%s ", color_for_delta(p->delta), fmt_delta_hex(p->delta).c_str(), color_reset());
            }

            std::printf("\n");
        }

        std::printf("\n\n");
        std::fflush(stdout);
    }

    void log_probe_summary(const RandSeedProbeResult& r) {
        SCLOGI("[SeedProbe] Summary: base=0x%08X, entries=%zu", r.base_seed, r.entries.size());
        for (const auto& e : r.entries) {
            if (!e.ok) { SCLOGI("  %-28s  hit=0  seed=--------  delta=------", e.label.c_str()); continue; }
            SCLOGI("  %-28s  hit=1  seed=0x%08X  delta=%lld", e.label.c_str(), e.seed, e.delta);
        }
    }

    std::vector<std::string> to_csv_lines(const RandSeedProbeResult& r) {
        std::vector<std::string> lines;
        lines.emplace_back("family,x,y,seed_hex,seed_dec,delta");
        auto fam_name = [](SeedFamily f) {
            switch (f) {
            case SeedFamily::Neutral:  return "Neutral";
            case SeedFamily::Main:     return "Main";
            case SeedFamily::CStick:   return "CStick";
            case SeedFamily::Triggers: return "Triggers";
            }
            return "Unknown";
            };
        char buf[256];
        for (const auto& e : r.entries) {
            if (!e.ok) continue;
            std::snprintf(buf, sizeof(buf), "%s,%d,%d,0x%08X,%u,%lld",
                fam_name(e.family), (int)e.x, (int)e.y, e.seed, e.seed, e.delta);
            lines.emplace_back(buf);
        }
        return lines;
    }

} // namespace simcore
