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

        SCLOGD("[VM] init begin sav=%s timeout=%u", init.savestate_path.c_str(), init.default_timeout_ms);

        // Disarm any previously armed set (enables program swapping)
        if (armed_ && !armed_pcs_.empty()) {
            host_.disarmPcBreakpoints(armed_pcs_);
            armed_pcs_.clear();
            armed_ = false;
        }

        // Optional savestate (allow empty path for boot-based phases)
        if (!init_.savestate_path.empty()) {
            if (!host_.loadSavestate(init_.savestate_path.c_str()))
                return false;
        }

        // Update canonical BP keys and arm once
        phase_.name = "ScriptPhase";
        phase_.canonical_bp_keys = prog_.canonical_bp_keys;
        phase_.allowed_predicates.clear();

        SCLOGD("[VM] attach bp count=%zu", program.canonical_bp_keys.size());
        arm_bps_once();

        // Capture a snapshot to use as the per-job baseline
        const bool snapshot_ok = save_snapshot();
        if (snapshot_ok) SCLOGD("[VM] init ok");
        return snapshot_ok;
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
                SCLOGD("[VM] phase=run_inputs begin frames=%zu", op.step.n);
                for (uint32_t i = 0; i < op.step.n; ++i) host_.stepOneFrameBlocking();
                SCLOGD("[VM] phase=run_inputs end");
                break;
            }

            case PSOpCode::RUN_UNTIL_BP: {
                SCLOGD("[VM] phase=run_until_bp begin armed=%zu", armed_pcs_.size());
                auto rr = host_.runUntilBreakpointBlocking(op.to.ms ? op.to.ms : timeout_ms);

                SCLOGD("[VM] phase=run_until_bp end result=%s pc=%08X", rr.hit ? "ok" : "fail", rr.pc);
                if (!rr.hit) 
                {
                    SCLOGD("[VM] timeout waiting for bp; last_pc=%08X", rr.pc);
                    return R; // timeout -> not ok
                }
                R.last_hit_pc = rr.pc;
                break;
            }

            case PSOpCode::READ_U8: { uint8_t  v{}; if (!read_u8(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_U16: { uint16_t v{}; if (!read_u16(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_U32: 
            { 
                uint32_t v{}; 
                if (!read_u32(op.rd.addr, v)) 
                {
                    SCLOGD("[VM] READ_U32 FAIL @%08X key=%s", op.rd.addr, op.rd.dst_key.c_str());
                    return R;
                }
                SCLOGD("[VM] READ_U32 @%08X -> %08X key=%s", op.rd.addr, v, op.rd.dst_key.c_str());
                ctx[op.rd.dst_key] = v; 
                break; 
            }
            case PSOpCode::READ_F32: { float    v{}; if (!read_f32(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }
            case PSOpCode::READ_F64: { double   v{}; if (!read_f64(op.rd.addr, v)) return R; ctx[op.rd.dst_key] = v; break; }

            case PSOpCode::SET_TIMEOUT:
                timeout_ms = op.to.ms ? op.to.ms : init_.default_timeout_ms;
                break;

            case PSOpCode::EMIT_RESULT:
            {
                SCLOGD("[VM] EMIT_RESULT %s=%08X", op.em.key.c_str(), ctx[op.em.key]);
                R.ctx[op.em.key] = ctx[op.em.key]; // copy selected value to result
                break;
            }
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
