#pragma once
#include <filesystem>
#include "SandboxAppState.h"

bool load_appstate_ini(AppState& s, std::filesystem::path f_ini_path);
bool save_appstate_ini(const AppState& s, std::filesystem::path f_ini_path);
