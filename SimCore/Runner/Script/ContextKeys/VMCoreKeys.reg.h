#pragma once
#include <cstddef>
#include "KeyIds.h"

namespace simcore::keys::core {

	// One-line per key via X-macro: NAME, ID, "string"
#define CORE_KEYS(X) \
  X(RUN_HIT_PC,        0x0000, "core.run.hit_pc")        \
  X(RUN_HIT_BP_KEY,    0x0001, "core.run.hit_bp")        \
  X(DW_RUN_OUTCOME_CODE,      0x0002, "core.run.outcome_code")  \
  X(ELAPSED_MS,        0x0003, "core.run.elapsed_ms")    \
\
  X(VI_FIRST,          0x0020, "core.metrics.vi_first")  \
  X(VI_LAST,           0x0021, "core.metrics.vi_last")   \
  X(POLL_MS,           0x0022, "core.metrics.poll_ms")   \
\
  X(RUN_MS,            0x0040, "core.input.run_ms")      \
  X(VI_STALL_MS,       0x0041, "core.input.vi_stall_ms") \
  X(PROGRESS_ENABLE,   0x0042, "core.input.progress_enable") \
\
  X(PLAN_FRAME_IDX,    0x0060, "core.plan.frame_idx")    \
  X(PLAN_DONE,         0x0061, "core.plan.done")         \
\
  X(PRED_COUNT,        0x0080, "core.pred.count")        \
  X(PRED_TABLE,        0x0081, "core.pred.table")        \
  X(PRED_BASELINES,    0x0082, "core.pred.baselines")    \
  X(PRED_TOTAL,        0x0083, "core.pred.total_passed")  \
  X(PRED_PASSED,       0x0084, "core.pred.count_passed_at_bp")   \
  X(PRED_ALL_PASSED,   0x0085, "core.pred.all_passed")   \
  X(PRED_FIRST_FAILED, 0x0086, "core.pred.first_failed")   \
  X(PRED_FAILED_CMP_STR,   0x0087, "core.pred.failed_cmp")   \
\
  X(WORKER_ERROR,      0x00A0, "core.output.worker_err")

// Emit KeyId constants + per-module range guards
#define DECL_KEY(NAME, ID, STR) \
	inline constexpr simcore::keys::KeyId NAME = static_cast<simcore::keys::KeyId>(ID); \
	static_assert(NAME >= simcore::keys::CORE_MIN && NAME <= simcore::keys::CORE_MAX, "core key out of range");
	CORE_KEYS(DECL_KEY)
#undef DECL_KEY

// Emit the module table used by the aggregator (names + ids for logging/inspection)
inline constexpr simcore::keys::KeyPair kKeys[] = {
	#define ROW(NAME, ID, STR) simcore::keys::KeyPair{ static_cast<simcore::keys::KeyId>(ID), STR },
	CORE_KEYS(ROW)
	#undef ROW
};
inline constexpr std::size_t kCount = sizeof(kKeys) / sizeof(kKeys[0]);

#undef CORE_KEYS

} // namespace simcore::keys::core