#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <cstdint>
#include <string>

#include "../PhaseEngine.h"           // uses your PhaseEngine (run_until_bp / run_inputs)
#include "../Breakpoints/BPCore.h"    // BreakpointMap, BPKey
#include "../../Core/DolphinWrapper.h"
#include "../../Core/Input/InputPlan.h" // GCInputFrame
#include "Core/Common/Buffer.h"

namespace simcore {

	// ----- Small, reusable ops -----
	enum class PSOpCode : uint8_t {
		ARM_PHASE_BPS_ONCE,     // arm canonical BPs once
		LOAD_SNAPSHOT,          // restore pre-captured buffer
		CAPTURE_SNAPSHOT,       // capture current state to buffer
		APPLY_INPUT,            // payload: GCInputFrame
		STEP_FRAMES,            // payload: uint32_t n
		RUN_UNTIL_BP,           // payload: uint32_t timeout_ms; writes last_hit_pc
		READ_U8, READ_U16, READ_U32, READ_F32, READ_F64,  // payload: {addr, dstKey}
		SET_TIMEOUT,            // payload: uint32_t timeout_ms (overrides default run timeout)
		EMIT_RESULT,            // payload: dstKey (copy value into VM.result)

	};

	static std::string get_psop_name(PSOpCode op);

	struct PSArg_Read { uint32_t addr; std::string dst_key; };
	struct PSArg_Step { uint32_t n; };
	struct PSArg_Timeout { uint32_t ms; };
	struct PSArg_Emit { std::string key; };

	using PSValue = std::variant<uint8_t, uint16_t, uint32_t, float, double>;
	using PSContext = std::unordered_map<std::string, PSValue>;

	struct PSOp {
		PSOpCode code{};
		// Only one of these will be used depending on `code`
		std::optional<GCInputFrame> input;
		PSArg_Read    rd{};
		PSArg_Step    step{};
		PSArg_Timeout to{};
		PSArg_Emit    em{};
	};

	struct PhaseScript {
		std::vector<BPKey> canonical_bp_keys;   // armed once
		std::vector<PSOp>  ops;                 // executed in order per job
	};

	struct PSInit {
		std::string savestate_path;
		uint32_t default_timeout_ms{ 10000 };
	};

	struct PSJob {
		// Arbitrary scratch to feed into the script (e.g., the one-frame input)
		GCInputFrame input;                // used by APPLY_INPUT if present
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
