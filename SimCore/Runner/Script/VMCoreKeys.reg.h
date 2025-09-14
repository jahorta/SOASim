#pragma once
#include <cstddef>
#include "KeyIds.h"

namespace simcore::keys::core {

	// One-line per key via X-macro: NAME, ID, "string"
#define CORE_KEYS(X) \
  X(RUN_HIT_PC,        0x0001, "core.run.hit_pc")        \
  X(OUTCOME_CODE,      0x0002, "core.run.outcome_code")  \
  X(ELAPSED_MS,        0x0003, "core.run.elapsed_ms")    \
  X(VI_FIRST,          0x0004, "core.metrics.vi_first")  \
  X(VI_LAST,           0x0005, "core.metrics.vi_last")   \
  X(POLL_MS,           0x0006, "core.metrics.poll_ms")   \
  X(RUN_MS,            0x0007, "core.input.run_ms")      \
  X(VI_STALL_MS,       0x0008, "core.input.vi_stall_ms") \
  X(PROGRESS_ENABLE,   0x0009, "core.input.progress_enable") \
  X(RUN_HIT_BP,        0x0009, "core.run.hit_bp")        \
  X(PLAN_FRAME_IDX,    0x000A, "core.plan.frame_idx")    \
  X(PLAN_DONE,         0x000B, "core.plan.done")         \
  X(PRED_COUNT,        0x000C, "core.pred.count")        \
  X(PRED_TABLE,        0x000D, "core.pred.table")        \
  X(PRED_BASELINES,    0x000E, "core.pred.baselines")    \
  X(PRED_TOTAL_AT_BP,  0x000F, "core.pred.total_at_bp")  \
  X(PRED_PASS_AT_BP,   0x0010, "core.pred.pass_at_bp")   \
  X(WORKER_ERROR,      0x0010, "core.output.worker_err")

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