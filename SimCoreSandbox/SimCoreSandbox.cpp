// SimCoreSandbox.cpp : REPL menu for SimCoreSandbox
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
#include "SandboxConfig.h"
#include "MenuConfig.h"
#include "SeedProbe.h"
//#include "TASMoviePlayer.h"
#include "utils.h"

using namespace simcore;

// ------------------------- helpers ---------------------------
static void init_logging(AppState& g )
{
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    g.exe_dir = fs::path(exePath).parent_path();
    log::Logger::get().set_levels(log::Level::Info, log::Level::Debug);
    log::Logger::get().open_file((g.exe_dir / "sandbox.log").string().c_str(), false);
    SCLOGI("[sandbox] Starting...");
}


// ------------------------- REPL menu -----------------------------------------
static void menu_loop(AppState& g)
{
    for (;;) {
        std::cout << "\n=== SOASim Sandbox ===\n";
        std::cout << "1) Configure paths (ISO, Dolphin portable base, default savestate)\n";
        std::cout << "2) Run RNGSeedDeltaMap probe\n";
        // std::cout << "3) TAS Movie -> BP -> Savestate (scaffold)\n";
        std::cout << "q) Quit\n";
        std::cout << "> ";

        std::string choice;
        if (!std::getline(std::cin, choice)) break;
        if (choice == "q" || choice == "Q") break;
        if (choice == "1") menu_configure_paths(g);
        else if (choice == "2") sandbox::run_rng_seed_probe_menu(g);
        /*else if (choice == "3") sandbox::menu_tas_movie(g);*/
        else {
            std::cout << "Unknown option.\n";
        }
    }
}

int main(int, char**)
{
    AppState g{};
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    g.exe_dir = std::filesystem::path(exePath).parent_path();

    const auto ini_path = g.exe_dir / "sandbox.ini";
    if (load_appstate_ini(g, ini_path)) {
        std::cout << "[cfg] loaded " << ini_path.string() << "\n";
    }

    init_logging(g);
    menu_loop(g);
    save_appstate_ini(g, ini_path);
    return 0;
}
