#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <cstdint>

#include "../Breakpoints/BPRegistry.h"    // BreakpointMap, BPKey
#include "../../Core/DolphinWrapper.h"
#include "../../Core/Input/InputPlan.h" // GCInputFrame
#include "Core/Common/Buffer.h"
#include "KeyIds.h"

namespace simcore {

	// ----- Small, reusable ops -----
	enum class PSOpCode : uint8_t {
		ARM_PHASE_BPS_ONCE,
		LOAD_SNAPSHOT,
		CAPTURE_SNAPSHOT,

		APPLY_INPUT_FROM,          // key -> GCInputFrame
		STEP_FRAMES,               // literal step count ok to keep
		RUN_UNTIL_BP,              // uses current timeout

		READ_U8, READ_U16, READ_U32, READ_F32, READ_F64, GET_BATTLE_CONTEXT,

		SET_TIMEOUT_FROM,          // key -> uint32
		EMIT_RESULT,               // literal: which key to emit

		GC_SLOT_A_SET_FROM,        // key -> path
		MOVIE_PLAY_FROM,           // key -> path
		MOVIE_STOP,
		SAVE_SAVESTATE_FROM,       // key -> path
		REQUIRE_DISC_GAMEID_FROM,  // key -> 6-char string

		LABEL,
		GOTO,
		GOTO_IF,                   // key cmp imm -> label
		GOTO_IF_KEYS,
		RETURN_RESULT,             // arg: u32 immediate outcome (written to OUTCOME_CODE & return)
		CAPTURE_PRED_BASELINES,       
		EVAL_PREDICATES_AT_HIT_BP,    
		RECORD_PROGRESS_AT_BP, 
		ARM_BPS_FROM_PRED_TABLE,
		SET_U32,                    // ctx[key] = imm
		ADD_U32,                    // ctx[key] += imm
		APPLY_PLAN_FRAME_FROM_KEY   // plan_id = ctx[key]
	};

	static std::string get_psop_name(PSOpCode op);

	enum class PSCmp : uint8_t { EQ, NE, LT, LE, GT, GE };

	struct PSArg_Read { uint32_t addr; simcore::keys::KeyId dst; };
	struct PSArg_Step { uint32_t n; };
	struct PSArg_Timeout { uint32_t ms; };
	struct PSArg_Path { std::string path; };
	struct PSArg_ID6 { char id[6]{}; };
	struct PSArg_Key { simcore::keys::KeyId id; };

	struct PSArg_Label { std::string name; };
	struct PSArg_Goto { std::string name; };
	struct PSArg_GotoIf {
		simcore::keys::KeyId key;
		PSCmp cmp;
		uint32_t imm;
		std::string name;
	};
	struct PSArg_GotoIfKeys {
		simcore::keys::KeyId left;
		PSCmp cmp;
		simcore::keys::KeyId right;
		std::string name;
	};
	struct PSArg_Plan { uint32_t id; };
	struct PSArg_ImmU32 { uint32_t v; };
	struct PSArg_KeyImm { simcore::keys::KeyId key; uint32_t imm; };

	using PSValue = std::variant<uint8_t, uint16_t, uint32_t, float, double, std::string, GCInputFrame>;
	class PSContext {
	public:
		using key_type = simcore::keys::KeyId;
		using map_type = std::unordered_map<key_type, PSValue>;
		using iterator = map_type::iterator;
		using const_iterator = map_type::const_iterator;

		// map-like API
		bool   empty() const noexcept { return kv_.empty(); }
		size_t size()  const noexcept { return kv_.size(); }
		void   clear()       noexcept { kv_.clear(); }

		iterator       begin()       noexcept { return kv_.begin(); }
		const_iterator begin() const noexcept { return kv_.begin(); }
		const_iterator cbegin()const noexcept { return kv_.cbegin(); }
		iterator       end()         noexcept { return kv_.end(); }
		const_iterator end()   const noexcept { return kv_.end(); }
		const_iterator cend()  const noexcept { return kv_.cend(); }

		iterator find(key_type k) { return kv_.find(k); }
		const_iterator find(key_type k) const { return kv_.find(k); }

		template<class... Args>
		std::pair<iterator, bool> emplace(key_type k, Args&&... args) {
			return kv_.emplace(k, PSValue(std::forward<Args>(args)...));
		}

		PSValue& operator[](key_type k) { return kv_[k]; }

		// typed getter
		template <typename T>
		bool get(key_type k, T& out) const {
			auto it = kv_.find(k); if (it == kv_.end()) return false;
			if (auto p = std::get_if<T>(&it->second)) { out = *p; return true; }
			return false;
		}

		// erase helper
		size_t erase(key_type k) { return kv_.erase(k); }

	private:
		map_type kv_;
	};

	// Only one of these will be used depending on `code`
	struct PSOp {           
		PSOpCode      code{};
		PSArg_Read    rd{};
		PSArg_Step    step{};
		PSArg_Timeout to{};
		PSArg_Key     a_key{};
		PSArg_Label   label{};
		PSArg_Goto    jmp{};
		PSArg_GotoIf  jcc{};
		PSArg_GotoIfKeys jcc2{};
		PSArg_Plan    plan{};
		PSArg_ImmU32  imm{};
		PSArg_KeyImm  keyimm{};
	};

	inline PSOp OpLabel(const std::string& s) { PSOp o; o.code = PSOpCode::LABEL; o.label.name = s; return o; }
	inline PSOp OpGoto(const std::string& s) { PSOp o; o.code = PSOpCode::GOTO;  o.jmp.name = s;  return o; }
	inline PSOp OpGotoIf(simcore::keys::KeyId k, PSCmp c, uint32_t v, const std::string& s) { PSOp o; o.code = PSOpCode::GOTO_IF; o.jcc = { k,c,v,s }; return o; }
	inline PSOp OpGotoIfKeys(simcore::keys::KeyId left, PSCmp c, simcore::keys::KeyId right, const std::string& s) { PSOp o; o.code = PSOpCode::GOTO_IF_KEYS; o.jcc2 = { left, c, right, s }; return o; }
	inline PSOp OpReturnResult(uint32_t code) { PSOp o; o.code = PSOpCode::RETURN_RESULT; o.imm.v = code; return o; }
	inline PSOp OpCapturePredBaselines() { PSOp o; o.code = PSOpCode::CAPTURE_PRED_BASELINES; return o; }
	inline PSOp OpArmBpsFromPredTable() { PSOp o; o.code = PSOpCode::ARM_BPS_FROM_PRED_TABLE; return o; }
	inline PSOp OpEvalPredicatesAtHitBP() { PSOp o; o.code = PSOpCode::EVAL_PREDICATES_AT_HIT_BP; return o; }
	inline PSOp OpRecordProgressAtBP() { PSOp o; o.code = PSOpCode::RECORD_PROGRESS_AT_BP; return o; }
	inline PSOp OpSetU32(simcore::keys::KeyId key, uint32_t v) { PSOp o; o.code = PSOpCode::SET_U32; o.keyimm = { key,v }; return o; }
	inline PSOp OpAddU32(simcore::keys::KeyId key, uint32_t v) { PSOp o; o.code = PSOpCode::ADD_U32; o.keyimm = { key,v }; return o; }
	inline PSOp OpApplyPlanFrameFrom(simcore::keys::KeyId key) { PSOp o; o.code = PSOpCode::APPLY_PLAN_FRAME_FROM_KEY; o.a_key = { key }; return o; }

	inline PSOp OpGcSlotASet(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::GC_SLOT_A_SET_FROM; o.a_key.id = k; return o; }
	inline PSOp OpApplyInputFrom(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::APPLY_INPUT_FROM;   o.a_key.id = k; return o; }
	inline PSOp OpSetTimeoutFrom(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::SET_TIMEOUT_FROM;   o.a_key.id = k; return o; }
	inline PSOp OpMoviePlayFrom(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::MOVIE_PLAY_FROM;    o.a_key.id = k; return o; }
	inline PSOp OpSaveSavestateFrom(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::SAVE_SAVESTATE_FROM; o.a_key.id = k; return o; }
	inline PSOp OpRequireDiscGameIdFrom(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::REQUIRE_DISC_GAMEID_FROM; o.a_key.id = k; return o; }

	// READ_* ops now store into a numeric key:
	inline PSOp OpReadU8(uint32_t addr, simcore::keys::KeyId dst) { PSOp o; o.code = PSOpCode::READ_U8;  o.rd = { addr,dst }; return o; }
	inline PSOp OpReadU16(uint32_t addr, simcore::keys::KeyId dst) { PSOp o; o.code = PSOpCode::READ_U16; o.rd = { addr,dst }; return o; }
	inline PSOp OpReadU32(uint32_t addr, simcore::keys::KeyId dst) { PSOp o; o.code = PSOpCode::READ_U32; o.rd = { addr,dst }; return o; }
	inline PSOp OpReadF32(uint32_t addr, simcore::keys::KeyId dst) { PSOp o; o.code = PSOpCode::READ_F32; o.rd = { addr,dst }; return o; }
	inline PSOp OpReadF64(uint32_t addr, simcore::keys::KeyId dst) { PSOp o; o.code = PSOpCode::READ_F64; o.rd = { addr,dst }; return o; }
	inline PSOp OpGetBattleContext() { PSOp o; o.code = PSOpCode::GET_BATTLE_CONTEXT; return o; }

	// EMIT_RESULT now exports a numeric key:
	inline PSOp OpEmitResult(simcore::keys::KeyId k) { PSOp o; o.code = PSOpCode::EMIT_RESULT; o.a_key.id = k; return o; }

	// OTHERS
	inline PSOp OpMovieStop() { PSOp o; o.code = PSOpCode::MOVIE_STOP; return o; }
	inline PSOp OpArmPhaseBps() { PSOp o; o.code = PSOpCode::ARM_PHASE_BPS_ONCE; return o; }
	inline PSOp OpLoadSnapshot() { PSOp o; o.code = PSOpCode::LOAD_SNAPSHOT; return o; }
	inline PSOp OpCaptureSnapshot() { PSOp o; o.code = PSOpCode::CAPTURE_SNAPSHOT; return o; }
	inline PSOp OpRunUntilBp() { PSOp o; o.code = PSOpCode::RUN_UNTIL_BP; return o; }


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
		uint8_t w_err;
		PSContext ctx;                     // values produced by READ_* / EMIT_RESULT
	};

	enum class RunToBpOutcome : uint32_t {
		Hit = 0,  // a monitored breakpoint fired
		Timeout = 1,  // wall-clock limit reached
		ViStalled = 2,  // VI didn't advance for the configured stall window
		MovieEnded = 3,  // movie playback ended before any breakpoint fired
		Aborted = 4,  // reserved for future external aborts
		Unknown = 5,  // catch-all
	};

	// ----- VM -----
	class PhaseScriptVM {
	public:
		PhaseScriptVM(simcore::DolphinWrapper& host, const BreakpointMap& bpmap);

		// Load state, build phase, arm bps, capture "prebattle" snapshot
		bool init(const PSInit& init, const PhaseScript& program);

		// Run the program once for a given job
		PSResult run(const PSJob& job);

	private:
		simcore::DolphinWrapper& host_;
		const BreakpointMap& bpmap_;
		std::vector<BPKey> canonical_bp_keys_;
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

		struct PlanView { const GCInputFrame* frames{ nullptr }; uint32_t count{ 0 }; };
		std::vector<PlanView> plan_table_;
		uint32_t last_plan_id_{ UINT32_MAX };
	};

} // namespace simcore
