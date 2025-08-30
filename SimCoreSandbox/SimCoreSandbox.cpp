// SimCoreSandbox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define NOMINMAX
#include "Core/DolphinWrapper.h"
#include "Core/System.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Boot/Boot.h"
#include "Core/Input/InputPlan.h"
#include "Core/Input/InputPlanFmt.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/GCPad.h"                                       // Pad::Initialize/Shutdown
#include <iostream>
#include <Utils/Log.h>
#include "prompt_path.cpp"
#include "SeedProbe.h"
#include "Runner/Breakpoints/PreBattleBreakpoints.h"
#include "Runner/RunEvaluator.h"
#include "Runner/DolphinRunnerAdapter.h"

using namespace simcore;

static void apply_snapshot_via_pad(DolphinRunnerAdapter& emu, const GCInputFrame& s)
{
    emu.set_next_input(s);
}

static bool run_until_after_seed_ms(BreakpointMap& pre, RunEvaluator& runner, uint32_t ms)
{
    // Replace with your Phase-B runner that waits for a list of BPs
    // and returns true if AfterRandSeedSet hits within timeout.
    PhaseBOptions opt{};
    opt.end_keys = { pre.terminal_key.value()};
    opt.timeout_ms = ms;

    SimulationResult out{};
    return runner.run_until_bp(opt, out).final_success;
}

static uint32_t read_u32_from_core(DolphinRunnerAdapter& emu, uint32_t addr)
{
    // Replace with your Dolphin memory read.
    uint32_t out = 0;
    if (emu.read_u32(addr, out))
        return out;
    return 0;
}

void run_seed_probe_entrypoint(std::string sav, DolphinRunnerAdapter& emu)
{
    BreakpointMap pre = battle_rng_probe::defaults();
    RunEvaluator runner{ emu, pre, {} };

    SeedProbeOps ops{
        /*reset_to_prebattle=*/
        [&]() {
            emu.load_savestate(sav);
        },
        /*apply_input_snapshot=*/
        [&emu](const GCInputFrame& s) {
            apply_snapshot_via_pad(emu, s);
        },
        /*run_until_after_seed=*/
        [&](uint32_t ms) {
            return run_until_after_seed_ms(pre, runner, ms);
        },
        /*read_u32=*/
        [&emu](uint32_t addr) {
            return read_u32_from_core(emu, addr);
        }
    };

    SeedProbeConfig cfg{};
    cfg.samples_per_axis = 6;      // e.g. 5x5 per family
    cfg.cap_trigger_top = true;    // avoids digital-L/R bleed at exactly 1.0
    cfg.run_timeout_ms = 10000;
    cfg.rng_addr = 0x803469a8u;

    auto res = run_seed_probe(cfg, ops);
    //log_probe_summary(res);

    // Optional CSV dump
    auto csv = to_csv_lines(res);
    // write lines to a file if desired
}



int main() {
    auto iso = prompt_path("ISO path: ", true, false);
    auto sav = prompt_path("Savestate path: ", true, false);

    simboot::BootOptions opts{};
    opts.user_dir = std::filesystem::current_path() / "SOASimUser";
    opts.dolphin_qt_base = R"(D:\SoATAS\dolphin-2506a-x64)";
    opts.force_p1_standard_pad = false;
    opts.force_resync_from_base = false;

    std::string err;
    DolphinWrapper emu;
    if (!simboot::BootDolphinWrapper(emu, opts, &err)) {
        SCLOGE("[sandbox] Boot Failed: %s", err.c_str());
        return 1; 
    }

    if (emu.loadGame(iso.string()))
        SCLOGI("[sandbox] Game Loaded");
    else
        SCLOGI("[sandbox] Game Failed to load");
    
    if (emu.loadSavestate(sav.string()))
        SCLOGI("[sandbox] Save State Loaded");
    else
        SCLOGI("[sandbox] Save State Failed to load");

    SCLOGI("[sandbox] Current PC set to %08X", emu.getPC());
    emu.ConfigurePortsStandardPadP1();

    uint32_t breakpoint = 0x80101e94;
    emu.armPcBreakpoints({ breakpoint });
    emu.disarmPcBreakpoints({ breakpoint });

    emu.stepOneFrameBlocking(10000);
    emu.stepOneFrameBlocking(10000);


    emu.loadSavestate(sav.string());
    uint32_t final_bp = 0x8000a1dc;
    emu.armPcBreakpoints({ final_bp });

    GCPadStatus st{};
    emu.QueryPadStatus(0, &st);
    SCLOGI("[sandbox] Inputs After: %s", DescribeFrame(FromGCPadStatus(st)).c_str());

    GCInputFrame zero{};
    zero.main_x = 0;
    zero.main_y = 0;

    auto before = std::chrono::steady_clock::now();
    emu.setInput(zero);

    GCPadStatus sta = {};
    emu.QueryPadStatus(0, &sta);
    SCLOGI("[sandbox] Inputs After Set Input: %s", DescribeFrame(FromGCPadStatus(sta)).c_str());

    emu.runUntilBreakpointBlocking(20000);
    auto duration = std::chrono::steady_clock::now() - before;

    GCPadStatus stb = {};
    emu.QueryPadStatus(0, &stb);
    SCLOGI("[sandbox] Inputs After Run: %s", DescribeFrame(FromGCPadStatus(stb)).c_str());


    //DolphinRunnerAdapter runner{ emu };

    //emu.silenceStdOutInfo();
    //run_seed_probe_entrypoint(sav.string(), runner);
    //emu.restoreStdOutInfo();

    return 0;
}
