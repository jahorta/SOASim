// SimCoreSandbox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Core/DolphinWrapper.h"
#include "Boot/Boot.h"
#include "Core/Branching/Branching.h"
#include "Core/Input/InputPlan.h"
#include "Core/Input/InputPlanFmt.h"
#include "Core/InputCommon/GCPadStatus.h"
#include "Core/InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"  // g_controller_interface
#include "InputCommon/InputConfig.h"                              // InputConfig
#include "Core/HW/GCPad.h"                                       // Pad::Initialize/Shutdown
#include "Core/Config/SYSCONFSettings.h"                         // Config reload helpers
#include "Core/ConfigManager.h"
#include "Common/Config/Config.h"                                 // Config::Get/Load
#include "Common/FileUtil.h"
#include <iostream>
#include <Utils/SafeEnv.h>
#include <Utils/Log.h>

using namespace simcore;

void demo_branches(simcore::DolphinWrapper& emu) {
    BranchSpec spec;
    spec.total_frames = 1;
    spec.default_frame = GCInputFrame{}; // neutral
    spec.decisions = {
        DecisionPoint{0, { GCInputFrame::new_btns(GC_START), GCInputFrame::new_btns(GC_A), GCInputFrame::new_btns(GC_B), GCInputFrame::new_btns(GC_X), 
        GCInputFrame::new_btns(GC_Y), GCInputFrame::new_btns(GC_DU), GCInputFrame::new_btns(GC_DD), GCInputFrame::new_btns(GC_DL), 
        GCInputFrame::new_btns(GC_DR), GCInputFrame::new_stk_main(100, 180), GCInputFrame::new_stk_c(255, 69), GCInputFrame::new_trigger(50, 16),
        GCInputFrame{}.A().DUp().JStick(12, 126).CStick(255, 0)}},
    };

    SCLOGI("[demo-branches] Running demo.");

    GCInputFrame neutral{};
    BranchExplorer ex(spec);
    while (auto inst = ex.next()) {
        SCLOGI("[demo-branches] Insts Chosen %s", DescribeChosenInputs(inst->plan, " ", neutral, GenerateButtonNameMap()).c_str());

        emu.setInputPlan(inst->plan);
        for (uint32_t f = 0; f < spec.total_frames; ++f) {
            emu.applyNextInputFrame();
            (void)emu.stepOneFrameBlocking(5000); // Step 5 will add memory checks + early stop
            GCPadStatus pad_status{};
            bool success = emu.QueryPadStatus(0, &pad_status);
            {
                SCLOGI("[demo-branches] frame: %d pad status: %s", emu.getFrameCountApprox(), success ? DescribeFrame(FromGCPadStatus(pad_status)).c_str() : "Failed to read pad");
            }
        }
    }
}

int main() {
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

    if (emu.loadGame(env::getenv_safe("SOASIM_TEST_ISO").value()))
        SCLOGI("[sandbox] Game Loaded");
    else
        SCLOGI("[sandbox] Game Failed to load");
    
    if (emu.loadSavestate(env::getenv_safe("SOASIM_TEST_STATE").value()))
        SCLOGI("[sandbox] Save State Loaded");
    else
        SCLOGI("[sandbox] Save State Failed to load");

    SCLOGI("[sandbox] Current PC set to %08X", emu.getPC());
    emu.ConfigurePortsStandardPadP1();

    if (!emu.stepOneFrameBlocking(5000))
    {
        SCLOGI("[sandbox] single step failed");
        return 0;
    }


    SCLOGI("[sandbox] Current PC set to %08X", emu.getPC());
    demo_branches(emu);
    SCLOGI("[sandbox] Current PC set to %08X", emu.getPC());
    return 0;
}
