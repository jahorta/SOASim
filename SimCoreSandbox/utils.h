#pragma once
#include <string>
#include "SandboxAppState.h"
#include "Runner/Parallel/ParallelPhaseScriptRunner.h"

namespace fs = std::filesystem;

bool ensure_sys_from_base_or_warn(const std::string& base);

simcore::BootPlan make_boot_plan(const AppState& g);

std::string trim(std::string s);

std::string strip_quotes(std::string s);

fs::path prompt_path(const char* prompt, bool require_exists, bool allow_empty, std::string def);

bool prompt_bool(const std::string& message, bool default_value = false);