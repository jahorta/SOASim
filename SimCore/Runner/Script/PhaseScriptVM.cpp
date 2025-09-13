#include "PhaseScriptVM.h"
#include <algorithm>

#include "VMCoreKeys.reg.h"
#include "KeyRegistry.h"

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
        PSContext ctx = job.ctx;

        // Always start by restoring the pre-captured snapshot for each job
        if (!load_snapshot()) return R;

        for (const auto& op : prog_.ops) {
            SCLOGT("[VM] running op: %s", get_psop_name(op.code).c_str());
            switch (op.code) {
            case PSOpCode::ARM_PHASE_BPS_ONCE: { arm_bps_once(); break; }
            case PSOpCode::LOAD_SNAPSHOT: { if (!load_snapshot()) return R; break; }
            case PSOpCode::CAPTURE_SNAPSHOT: { if (!save_snapshot()) return R; break; }

            case PSOpCode::STEP_FRAMES: {
                SCLOGD("[VM] phase=run_inputs begin frames=%zu", op.step.n);
                for (uint32_t i = 0; i < op.step.n; ++i) host_.stepOneFrameBlocking();
                SCLOGD("[VM] phase=run_inputs end");
                break;
            }

            case PSOpCode::RUN_UNTIL_BP: {
                
                using simcore::RunToBpOutcome;

                uint32_t timeout_ms = init_.default_timeout_ms;
                ctx.get<uint32_t>(keys::core::RUN_MS, timeout_ms);
                

                uint32_t vi_stall_ms = 0;
                ctx.get<uint32_t>(keys::core::VI_STALL_MS, vi_stall_ms);

                // 2) Choose a poll cadence (VM emits the value it actually asked the wrapper to use)
                const uint32_t poll_ms = host_.pickPollIntervalMs(timeout_ms);

                // 3) Call into the flexible wrapper; it will treat watch_movie=true as a no-op if no movie is playing
                const bool watch_movie = true;

                uint32_t progress_temp;
                ctx.get(keys::core::PROGRESS_ENABLE, progress_temp);
                bool progress_enable = progress_temp != 0;

                DolphinWrapper::ProgressSink sink = nullptr;
                if (progress_enable)
                {
                    sink = host_.getProgressSink(); // host already has a per-job sink set by worker
                }

                auto t0 = std::chrono::steady_clock::now();
                auto rr = host_.runUntilBreakpointFlexible(timeout_ms, vi_stall_ms, watch_movie, poll_ms);
                auto t1 = std::chrono::steady_clock::now();
                const uint32_t elapsed_ms = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

                // 4) Map wrapper result -> canonical outcome enum
                RunToBpOutcome outcome = RunToBpOutcome::Unknown;
                if (rr.hit) {
                    outcome = RunToBpOutcome::Hit;
                }
                else if (rr.reason) {
                    if (std::strcmp(rr.reason, "timeout") == 0) outcome = RunToBpOutcome::Timeout;
                    else if (std::strcmp(rr.reason, "vi_stalled") == 0) outcome = RunToBpOutcome::ViStalled;
                    else if (std::strcmp(rr.reason, "movie_ended") == 0) outcome = RunToBpOutcome::MovieEnded;
                    else outcome = RunToBpOutcome::Unknown;
                }

                // 5) Emit standardized, program-agnostic keys
                R.ctx[keys::core::OUTCOME_CODE] = static_cast<uint32_t>(outcome);
                R.ctx[keys::core::ELAPSED_MS] = elapsed_ms;
                R.ctx[keys::core::RUN_HIT_PC] = rr.hit ? rr.pc : 0u;

                // Optional telemetry (present for observability)
                R.ctx[keys::core::VI_LAST] = (uint32_t)(host_.getViFieldCountApprox() & 0xFFFFFFFFull);
                R.ctx[keys::core::POLL_MS] = poll_ms;

                // Preserve existing success semantics
                if (!rr.hit) {
                    return R; // ok remains false; last_hit_pc left as-is (0)
                }
                R.ctx[keys::core::RUN_HIT_PC] = rr.pc;
                break;
            }

            case PSOpCode::READ_U8: { uint8_t  v{}; if (!read_u8(op.rd.addr, v)) return R; ctx[op.rd.dst] = v; break; }
            case PSOpCode::READ_U16: { uint16_t v{}; if (!read_u16(op.rd.addr, v)) return R; ctx[op.rd.dst] = v; break; }
            case PSOpCode::READ_U32: 
            { 
                uint32_t v{}; 
                if (!read_u32(op.rd.addr, v)) 
                {
                    SCLOGD("[VM] READ_U32 FAIL @%08X key=%s", op.rd.addr, keys::name_for_id(op.rd.dst).data());
                    return R;
                }
                SCLOGD("[VM] READ_U32 @%08X -> %08X key=%s", op.rd.addr, v, keys::name_for_id(op.rd.dst).data());
                ctx[op.rd.dst] = v; 
                break; 
            }
            case PSOpCode::READ_F32: { float    v{}; if (!read_f32(op.rd.addr, v)) return R; ctx[op.rd.dst] = v; break; }
            case PSOpCode::READ_F64: { double   v{}; if (!read_f64(op.rd.addr, v)) return R; ctx[op.rd.dst] = v; break; }

            case PSOpCode::EMIT_RESULT:
            {
                SCLOGD("[VM] EMIT_RESULT %s=%08X", keys::name_for_id(op.a_key.id).data(), ctx[op.a_key.id]);
                R.ctx[op.a_key.id] = ctx[op.a_key.id]; // copy selected value to result
                break;
            }
            case PSOpCode::APPLY_INPUT_FROM: {
                auto it = ctx.find(op.a_key.id);
                if (it == ctx.end()) return R;
                if (auto p = std::get_if<GCInputFrame>(&it->second)) {
                    host_.setInput(*p);
                }
                else {
                    return R;
                }
                break;
            }

            case PSOpCode::SET_TIMEOUT_FROM: {
                uint32_t timeout_ms;
                ctx.get<uint32_t>(op.a_key.id, timeout_ms);
                break;
            }

            case PSOpCode::MOVIE_PLAY_FROM: {
                std::string path;
                ctx.get<std::string>(op.a_key.id, path);
                if (!host_.startMoviePlayback(path)) return R;
                break;
            }

            case PSOpCode::SAVE_SAVESTATE_FROM: {
                std::string path;
                ctx.get<std::string>(op.a_key.id, path);
                if (!host_.saveSavestateBlocking(path)) return R;
                break;
            }

            case PSOpCode::REQUIRE_DISC_GAMEID_FROM: {
                
                const char* id6 = nullptr;
                std::string tmp;
                ctx.get<std::string>(op.a_key.id, tmp);
                if (tmp.size() < 6) return R;
                id6 = tmp.c_str();

                auto di = host_.getDiscInfo();
                const bool ok = (di.has_value() && di->game_id.size() >= 6 &&
                    std::memcmp(di->game_id.data(), id6, 6) == 0);
                if (!ok) return R;
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
        case PSOpCode::APPLY_INPUT_FROM:
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
        case PSOpCode::SET_TIMEOUT_FROM:
            return { "Set timeout" };
        case PSOpCode::EMIT_RESULT:
            return { "Emit result" };
        case PSOpCode::MOVIE_PLAY_FROM:
            return { "Play TAS Movie" };
        case PSOpCode::MOVIE_STOP:
            return { "Stop TAS Movie" };
        case PSOpCode::SAVE_SAVESTATE_FROM:
            return { "Save Savestate" };
        case PSOpCode::REQUIRE_DISC_GAMEID_FROM:
            return { "Set timeout" };
        default:
            return { "Unknown Code" };
        }
    }

} // namespace simcore
