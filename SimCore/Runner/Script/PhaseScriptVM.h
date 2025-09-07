#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <cstdint>

#include "../PhaseEngine.h"           // uses your PhaseEngine (run_until_bp / run_inputs)
#include "../Breakpoints/BPCore.h"    // BreakpointMap, BPKey
#include "../../Core/DolphinWrapper.h"
#include "../../Core/Input/InputPlan.h" // GCInputFrame
#include "Core/Common/Buffer.h"

namespace simcore {

	// ----- Small, reusable ops -----
	enum class PSOpCode : uint8_t {
		ARM_PHASE_BPS_ONCE,
		LOAD_SNAPSHOT,
		CAPTURE_SNAPSHOT,

		APPLY_INPUT_FROM,       // key -> GCInputFrame
		STEP_FRAMES,            // literal step count ok to keep
		RUN_UNTIL_BP,           // uses current timeout

		READ_U8, READ_U16, READ_U32, READ_F32, READ_F64,

		SET_TIMEOUT_FROM,       // key -> uint32
		EMIT_RESULT,            // literal: which key to emit

		GC_SLOT_A_SET_FROM,     // key -> path
		MOVIE_PLAY_FROM,        // key -> path
		MOVIE_STOP,
		SAVE_SAVESTATE_FROM,    // key -> path
		REQUIRE_DISC_GAMEID_FROM// key -> 6-char string
	};

	static std::string get_psop_name(PSOpCode op);

	struct PSArg_Read { uint32_t addr; std::string dst_key; };
	struct PSArg_Step { uint32_t n; };
	struct PSArg_Timeout { uint32_t ms; };
	struct PSArg_Path { std::string path; };
	struct PSArg_ID6 { char id[6]{}; };
	struct PSArg_Key { std::string key; };

	using PSValue = std::variant<uint8_t, uint16_t, uint32_t, float, double, std::string, GCInputFrame>;
	using PSContext = std::unordered_map<std::string, PSValue>;

	struct PSOp {
		PSOpCode code{};
		// Only one of these will be used depending on `code`
		PSArg_Read    rd{};
		PSArg_Step    step{};
		PSArg_Timeout to{};
		PSArg_Key     a_key{};
	};

	inline PSOp OpGcSlotASet(const std::string& p) { PSOp o; o.code = PSOpCode::GC_SLOT_A_SET_FROM; o.a_key.key = p; return o; }
	inline PSOp OpApplyInputFrom(const std::string& key) { PSOp o; o.code = PSOpCode::APPLY_INPUT_FROM;     o.a_key.key = key; return o; }
	inline PSOp OpSetTimeoutFrom(const std::string& key) { PSOp o; o.code = PSOpCode::SET_TIMEOUT_FROM;     o.a_key.key = key; return o; }
	inline PSOp OpMoviePlayFrom(const std::string& key) { PSOp o; o.code = PSOpCode::MOVIE_PLAY_FROM;      o.a_key.key = key; return o; }
	inline PSOp OpMovieStop() { PSOp o; o.code = PSOpCode::MOVIE_STOP;  return o; }
	inline PSOp OpSaveSavestateFrom(const std::string& key) { PSOp o; o.code = PSOpCode::SAVE_SAVESTATE_FROM;  o.a_key.key = key; return o; }
	inline PSOp OpRequireDiscID6From(const std::string& key) { PSOp o; o.code = PSOpCode::REQUIRE_DISC_GAMEID_FROM; o.a_key.key = key; return o; }
	inline PSOp OpEmitResult(const std::string& key) { PSOp o; o.code = PSOpCode::EMIT_RESULT; o.a_key.key = key; return o; }


	struct PhaseScript {
		std::vector<BPKey> canonical_bp_keys;   // armed once
		std::vector<PSOp>  ops;                 // executed in order per job
	};

	struct PSInit {
		std::string savestate_path;
		uint32_t default_timeout_ms{ 10000 };
	};

	struct PSJob {
		std::vector<uint8_t> payload;
		PSContext ctx;
	};

	struct PSResult {
		bool ok{ false };
		uint32_t last_hit_pc{ 0 };
		PSContext ctx;                     // values produced by READ_* / EMIT_RESULT
	};

	// ----- VM -----
	class PhaseScriptVM {
	public:
		PhaseScriptVM(simcore::DolphinWrapper& host, const BreakpointMap& bpmap);

		// Load state, build phase, arm bps, capture “prebattle” snapshot
		bool init(const PSInit& init, const PhaseScript& program);

		// Run the program once for a given job
		PSResult run(const PSJob& job);

	private:
		simcore::DolphinWrapper& host_;
		const BreakpointMap& bpmap_;
		PhaseSpec phase_;
		PhaseEngine engine_;
		PhaseScript prog_;
		PSInit init_;
		std::vector<uint32_t> armed_pcs_;

		bool armed_{ false };
		Common::UniqueBuffer<u8> snapshot_;

		// helpers
		void arm_bps_once();
		bool save_snapshot();
		bool load_snapshot();

		// typed reads
		bool read_u8(uint32_t a, uint8_t& v)  const { return host_.readU8(a, v); }
		bool read_u16(uint32_t a, uint16_t& v) const { return host_.readU16(a, v); }
		bool read_u32(uint32_t a, uint32_t& v) const { return host_.readU32(a, v); }
		bool read_f32(uint32_t a, float& v)    const { return host_.readF32(a, v); }
		bool read_f64(uint32_t a, double& v)   const { return host_.readF64(a, v); }
	};

} // namespace simcore
