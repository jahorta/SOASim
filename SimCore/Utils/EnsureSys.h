#pragma once
#include <string>

namespace simcore {
	// Returns true if <exe_dir>\Sys exists (and contains dsp_coef.bin), else copies from qt_base\Sys.
	bool EnsureSysBesideExe(const std::string& qt_base_dir);
}
