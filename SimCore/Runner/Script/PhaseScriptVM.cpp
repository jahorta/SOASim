#include "PhaseScriptVM.h"
#include <algorithm>

namespace simcore {

    PhaseScriptVM::PhaseScriptVM(simcore::DolphinWrapper& host, const BreakpointMap& bpmap)
        : host_(host), bpmap_(bpmap), phase_{}, engine_(host_, bpmap_, phase_) {
    }

    bool PhaseScriptVM::save_snapshot() {
        return host_.saveStateToBuffer(snapshot_);
    }

    bool PhaseScriptVM::load_snapshot() {
        return host_.loadStateFromBuffer(snapshot_);
    }

    void PhaseScriptVM::arm_bps_once() {
        if (armed_) return;
        std::vector<uint32_t> pcs;
        pcs.reserve(phase_.canonical_bp_keys.size());
        for (const auto& k : phase_.canonical_bp_keys) {
            if (const auto* e = bpmap_.find(k)) pcs.push_back(e->pc);
        }
        if (!pcs.empty()) {
            host_.armPcBreakpoints(pcs);
            armed_pcs_ = pcs;
        }
        armed_ = true;
    }

    bool PhaseScriptVM::init(const PSInit& init, const PhaseScript& program)
    {
        init_ = init;
        prog_ = program;

        // 0) Disarm previous set (if any), so we can change canonical keys safely
        if (armed_ && !armed_pcs_.empty()) {
            host_.disarmPcBreakpoints(armed_pcs_);
            armed_pcs_.clear();
            armed_ = false;
        }

        // 1) Load the initial savestate for this program
        if (!host_.loadSavestate(init_.savestate_path.c_str()))
            return false;

        // 2) Update phase keys and arm once
        phase_.name = "ScriptPhase";
        phase_.canonical_bp_keys = prog_.canonical_bp_keys;
        phase_.allowed_predicates.clear();

        arm_bps_once();

        // 3) Snapshot for per-job resets
        return save_snapshot();
    }

    PSResult PhaseScriptVM::run(const PSJob& job)
    {
        PSResult R{};
        PSContext ctx;
        uint32_t timeout_ms = init_.default_timeout_ms;

        // Always start by restoring the pre-captured snapshot for each job
        if (!load_snapshot()) return R;

        for (const auto& op : prog_.ops) {
            SCLOGT("[VM] running op: %s", get_psop_name(op.code).c_str());
            switch (op.code) {
            case PSOpCode::ARM_PHASE_BPS_ONCE: { arm_bps_once(); break; }
            case PSOpCode::LOAD_SNAPSHOT: { if (!load_snapshot()) return R; break; }
            case PSOpCode::CAPTURE_SNAPSHOT: { if (!save_snapshot()) return R; break; }

            case PSOpCode::APPLY_INPUT: {
                const GCInputFrame& f = op.input.has_value() ? op.input.value() : job.input;
                host_.setInput(f);
                break;
            }

            case PSOpCode::STEP_FRAMES: {
                for (uint32_t i = 0; i < op.step.n; ++i) host_.stepOneFrameBlocking();
                break;
            }

            case PSOpCode::RUN_UNTIL_BP: {
                auto rr = host_.runUntilBreakpointBlocking(op.to.ms ? op.to.ms : timeout_ms);
                if (!rr.hit) return R; // timeout -> not ok
                R.last_hit_pc = rr.pc;
                break;
            }

            case PSOpCode::READ_U8: { uint8_t  v{}; if (!read_u8(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_U16: { uint16_t v{}; if (!read_u16(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_U32: { uint32_t v{}; if (!read_u32(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_F32: { float    v{}; if (!read_f32(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_F64: { double   v{}; if (!read_f64(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }

            case PSOpCode::SET_TIMEOUT:
                timeout_ms = op.to.ms ? op.to.ms : init_.default_timeout_ms;
                break;

            case PSOpCode::EMIT_RESULT:
                R.ctx[op.em.key] = ctx[op.em.key]; // copy selected value to result
                break;
            }
        }

        R.ok = true;
        return R;
    }

    std::string get_psop_name(PSOpCode op)
    {
        switch (op) {
        case PSOpCode::ARM_PHASE_BPS_ONCE:
            return { "Arm Phase BPs Once" };
        case PSOpCode::LOAD_SNAPSHOT:
            return { "Load Snapshot" };
        case PSOpCode::CAPTURE_SNAPSHOT:
            return { "Capture Snapshot" };
        case PSOpCode::APPLY_INPUT:
            return { "Apply Input" };
        case PSOpCode::STEP_FRAMES:
            return { "Step Frames" };
        case PSOpCode::RUN_UNTIL_BP:
            return { "Run Until BP" };
        case PSOpCode::READ_U8:
            return { "Read u8" };
        case PSOpCode::READ_U16:
            return { "Read u16" };
        case PSOpCode::READ_U32:
            return { "Read u32" };
        case PSOpCode::READ_F32:
            return { "Read float" };
        case PSOpCode::READ_F64:
            return { "Read double" };
        case PSOpCode::SET_TIMEOUT:
            return { "Set timeout" };
        case PSOpCode::EMIT_RESULT:
            return { "Emit result" };
        default:
            return { "Unknown Code" };
        }
    }

} // namespace simcore
