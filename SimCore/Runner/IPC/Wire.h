// SimCore/IPC/Wire.h
#pragma once
#include <cstdint>
#include "../../Core/Input/InputPlan.h"

#pragma pack(push,1)
enum : uint8_t { MSG_JOB = 1, MSG_RESULT = 2, MSG_READY, MSG_BYE = 9 };

enum : uint32_t {
	WERR_None = 0,
	WERR_SysMissing = 1,
	WERR_BootFail = 2,
	WERR_LoadGame = 3,
	WERR_VMInit = 4,
	WERR_WriteReady = 5
};

struct WireJob {
	uint8_t  tag;       // MSG_JOB
	uint64_t job_id;
	uint64_t epoch;
	uint8_t  main_x, main_y, c_x, c_y, trig_l, trig_r;
	uint32_t buttons;
};

struct WireReady {
	uint8_t tag;
	uint32_t ok;
	uint16_t reserved{ 0 };
	uint32_t error;     // WERR_* above
};

struct WireResult {
	uint8_t  tag;       // MSG_RESULT
	uint64_t job_id;
	uint64_t epoch;
	uint8_t  ok;        // 0/1
	uint32_t last_pc;
	uint32_t seed;
};
#pragma pack(pop)

// Helpers
inline WireJob make_wire_job(uint64_t id, uint64_t epoch, const simcore::GCInputFrame& f) {
	WireJob w{};
	w.tag = MSG_JOB; w.job_id = id; w.epoch = epoch;
	w.main_x = f.main_x; w.main_y = f.main_y; w.c_x = f.c_x; w.c_y = f.c_y;
	w.trig_l = f.trig_l; w.trig_r = f.trig_r; w.buttons = f.buttons;
	return w;
}
