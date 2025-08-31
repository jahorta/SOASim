#include "SeedProbe.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

#ifndef SCLOGI
#define SCLOGI(...) do { std::printf(__VA_ARGS__); std::printf("\n"); } while(0)
#endif
#include <Utils/Log.h>

namespace simcore {

    // ---------- progress bar helpers ----------
    static void draw_progress_bar(const char* label, size_t done, size_t total)
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

    std::vector<GCInputFrame> build_grid_main(int n) {
        std::vector<GCInputFrame> out;
        auto xs = levels_u8(0, 255, n, false);
        auto ys = levels_u8(0, 255, n, false);
        out.reserve(n * n);
        for (float y : ys) for (float x : xs) out.push_back(set_main(neutral_frame(), x, y));
        return out;
    }

    std::vector<GCInputFrame> build_grid_cstick(int n) {
        std::vector<GCInputFrame> out;
        auto xs = levels_u8(0, 255, n, false);
        auto ys = levels_u8(0, 255, n, false);
        out.reserve(n * n);
        for (float y : ys) for (float x : xs) out.push_back(set_c(neutral_frame(), x, y));
        return out;
    }

    std::vector<GCInputFrame> build_grid_triggers(int n, bool cap_top) {
        std::vector<GCInputFrame> out;
        auto ls = levels_u8(0, 255, n, false);
        auto rs = levels_u8(0, 255, n, false);
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

    // ANSI color helpers
    static const char* ansi_reset() { return "\x1b[0m"; }
    static const char* ansi_dim() { return "\x1b[2m"; }
    static const char* color_for_delta(long long d) {
        if (d == 0) return "\x1b[2;37m"; // dim gray
        long long ad = std::llabs(d);
        int nib = (int)(ad & 0xF);
        if (nib == 0) nib = 1;
        if (d < 0) {
            static const int neg_palette[16] = {
                196, 160, 124, 88, 52,  130, 94,  58,
                202, 166,  9,  1,  95,  131, 167, 203
            };
            return ("\x1b[38;5;" + std::to_string(neg_palette[nib % 16]) + "m").c_str(); // UB if returned pointer to temp
        }
        else {
            static const int pos_palette[16] = {
                46,  47,  48,  49,  50,  82,  118, 154,
                33,  39,  45,  51,  87,  123, 159, 195
            };
            return ("\x1b[38;5;" + std::to_string(pos_palette[nib % 16]) + "m").c_str();
        }
    }

    // safe color printer (avoid returning temporaries)
    static void print_colored_cell(long long d) {
        char cell[3] = { ' ', '0', 0 };
        bool big = (std::llabs(d) > 0xF);
        if (big) { cell[0] = ' '; cell[1] = '*'; }
        else if (d == 0) { cell[0] = ' '; cell[1] = '0'; }
        else if (d < 0) {
            int nib = (int)(-d) & 0xF;
            static const char hex[] = "0123456789ABCDEF";
            cell[0] = '-'; cell[1] = hex[nib];
        }
        else {
            if (d <= 0xFF) {
                static const char hex[] = "0123456789ABCDEF";
                char hi = hex[(d >> 4) & 0xF];
                char lo = hex[d & 0xF];
                cell[0] = hi; cell[1] = lo;
            }
            else {
                cell[0] = ' '; cell[1] = '*'; // too large
            }
        }

        if (big) {
            std::printf("\x1b[1;35m%c%c\x1b[0m", cell[0], cell[1]); // bright magenta for overflow
            return;
        }

        // choose color
        long long ad = std::llabs(d);
        int nib = (int)(ad & 0xF);
        if (d == 0) {
            std::printf("\x1b[2;37m%c%c\x1b[0m", cell[0], cell[1]);
        }
        else if (d < 0) {
            static const int pal[] = { 196,160,124,88,52,130,94,58,202,166,9,1,95,131,167,203 };
            int c = pal[(nib == 0 ? 1 : nib) % 16];
            std::printf("\x1b[38;5;%dm%c%c\x1b[0m", c, cell[0], cell[1]);
        }
        else {
            static const int pal[] = { 46,47,48,49,50,82,118,154,33,39,45,51,87,123,159,195 };
            int c = pal[(nib == 0 ? 1 : nib) % 16];
            std::printf("\x1b[38;5;%dm%c%c\x1b[0m", c, cell[0], cell[1]);
        }
    }

    static void print_family_grid(const RandSeedProbeResult& r, SeedFamily fam, int N, const char* title)
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
                print_colored_cell(d);
                std::printf(" ");
            }

            std::printf("\n");
        }

        std::printf("\n\n");
        std::fflush(stdout);
    }

    RandSeedProbeResult run_seed_probe(const SeedProbeConfig& cfg, const SeedProbeOps& ops) {
        RandSeedProbeResult result{};
        std::vector<RandSeedEntry> out;

        // Neutral
        {
            ops.reset_to_prebattle();
            auto f = neutral_frame();
            ops.apply_input_frame(f);
            bool hit = ops.run_until_after_seed(cfg.run_timeout_ms);

            RandSeedEntry e{};
            e.samples_per_axis = cfg.samples_per_axis;
            e.family = SeedFamily::Neutral;
            e.x = 0.0f; e.y = 0.0f;
            e.ok = hit;
            if (hit) e.seed = ops.read_u32(cfg.rng_addr);
            label_entry(e);
            out.push_back(e);

            if (!hit) {
                SCLOGI("[SeedProbe] Neutral failed to hit AfterRandSeedSet.");
                result.entries = std::move(out);
                result.base_seed = 0;
                return result;
            }
            result.base_seed = e.seed;
            SCLOGI("[SeedProbe] Base seed (Neutral) = 0x%08X", result.base_seed);
        }

        const int N = cfg.samples_per_axis;
        auto g_main = build_grid_main(N);
        auto g_cstick = build_grid_cstick(N);
        auto g_trig = build_grid_triggers(N, cfg.cap_trigger_top);

        auto run_grid = [&](SeedFamily fam, const std::vector<GCInputFrame>& grid, const char* label) {
            const size_t total = grid.size();
            size_t done = 0;
            if (total > 0) draw_progress_bar(label, 0, total);
            
            for (const auto& f : grid) {
                SCLOGT("[run eval] starting %s (%d/%d)", label, done+1, total);

                ops.reset_to_prebattle();
                ops.apply_input_frame(f);
                bool hit = ops.run_until_after_seed(cfg.run_timeout_ms);

                RandSeedEntry e{};
                e.samples_per_axis = N;
                e.family = fam;
                if (fam == SeedFamily::Main) { e.x = f.main_x; e.y = f.main_y; }
                if (fam == SeedFamily::CStick) { e.x = f.c_x;    e.y = f.c_y; }
                if (fam == SeedFamily::Triggers) { e.x = f.trig_l; e.y = f.trig_r; }
                e.ok = hit;
                if (hit) e.seed = ops.read_u32(cfg.rng_addr);
                label_entry(e);
                out.push_back(e);

                ++done;
                draw_progress_bar(label, done, total);
            }
            };

        run_grid(SeedFamily::Main, g_main, "JStick");
        run_grid(SeedFamily::CStick, g_cstick, "CStick");
        run_grid(SeedFamily::Triggers, g_trig, "Triggers");

        for (auto& e : out) if (e.ok) e.delta = signed_delta(e.seed, result.base_seed);

        result.entries = std::move(out);

        print_family_grid(result, SeedFamily::Main, N, "Main");
        print_family_grid(result, SeedFamily::CStick, N, "CStick");
        print_family_grid(result, SeedFamily::Triggers, N, "Triggers");

        return result;
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
