#pragma once
#include <string>
#include "Phases/BattleExplorer.h"
#include "SandboxAppState.h"

namespace sandbox {
	void get_battle_context(AppState& app);
	void run_battle_explorer_menu(AppState& app);
} // namespace sandbox