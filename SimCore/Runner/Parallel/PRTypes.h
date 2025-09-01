#pragma once
#include <cstdint>
#include <cstddef>
#include "../Script/PhaseScriptVM.h"  // for PSResult

namespace simcore {

	// Runner status snapshot (UI/console)
	struct PRStatus {
		uint64_t epoch{ 0 };
		size_t queued_jobs{ 0 };
		size_t running_workers{ 0 };
		size_t workers{ 0 };
	};

	// Cross-thread/process job result
	struct PRResult {
		uint64_t job_id{ 0 };
		uint64_t epoch{ 0 };
		size_t worker_id{ 0 };
		bool accepted{ false };
		PSResult ps;               // from PhaseScriptVM
	};

} // namespace simcore
