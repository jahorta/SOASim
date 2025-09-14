#include "PhaseScriptVM.h"
#include <algorithm>

#include "KeyRegistry.h"
#include "../../Phases/Programs/BattleRunner/BattleRunnerPayload.h"
#include "../../Phases/Programs/BattleContext/BattleContextCodec.h"
#include "../../Core/MemView.h"

namespace simcore {

    PhaseScriptVM::PhaseScriptVM(simcore::DolphinWrapper& host, const BreakpointMap& bpmap)
        : host_(host), bpmap_(bpmap) {
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
        pcs.reserve(canonical_bp_keys_.size());
        for (const auto& k : canonical_bp_keys_) {
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
        canonical_bp_keys_ = prog_.canonical_bp_keys;

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
        static uint32_t last_dynamic_plan_id = 0xFFFFFFFFu;

        // Always start by restoring the pre-captured snapshot for each job
        if (!load_snapshot()) return R;

        std::unordered_map<std::string, size_t> label_pc;
        for (size_t i = 0; i < prog_.ops.size(); ++i) {
            if (prog_.ops[i].code == PSOpCode::LABEL) label_pc[prog_.ops[i].label.name] = i;
        }

        for (size_t pc = 0; pc < prog_.ops.size(); ++pc) {
            const auto& op = prog_.ops[pc];
            SCLOGT("[VM] running op: %s", get_psop_name(op.code).c_str());
            
            switch (op.code) {
            case PSOpCode::ARM_PHASE_BPS_ONCE: { arm_bps_once(); break; }
            case PSOpCode::LOAD_SNAPSHOT: { if (!load_snapshot()) return R; break; }
            case PSOpCode::CAPTURE_SNAPSHOT: { if (!save_snapshot()) return R; break; }
            case PSOpCode::LABEL: { break; }
            case PSOpCode::GOTO: {
                auto it = label_pc.find(op.jmp.name);
                if (it != label_pc.end()) pc = it->second;
                break;
            }

            case PSOpCode::GOTO_IF: {
                uint32_t v = 0;
                ctx.get<uint32_t>(op.jcc.key, v);
                bool take = false;
                switch (op.jcc.cmp) {
                case PSCmp::EQ: take = (v == op.jcc.imm); break;
                case PSCmp::NE: take = (v != op.jcc.imm); break;
                case PSCmp::LT: take = (v < op.jcc.imm); break;
                case PSCmp::LE: take = (v <= op.jcc.imm); break;
                case PSCmp::GT: take = (v > op.jcc.imm); break;
                case PSCmp::GE: take = (v >= op.jcc.imm); break;
                }
                if (take) {
                    auto it = label_pc.find(op.jcc.name);
                    if (it != label_pc.end()) pc = it->second;
                }
                break;
            }

            case PSOpCode::GOTO_IF_KEYS: {
                uint32_t lv = 0, rv = 0;
                ctx.get<uint32_t>(op.jcc2.left, lv);
                ctx.get<uint32_t>(op.jcc2.right, rv);
                bool take = false;
                switch (op.jcc2.cmp) {
                case PSCmp::EQ: take = (lv == rv); break;
                case PSCmp::NE: take = (lv != rv); break;
                case PSCmp::LT: take = (lv < rv); break;
                case PSCmp::LE: take = (lv <= rv); break;
                case PSCmp::GT: take = (lv > rv); break;
                case PSCmp::GE: take = (lv >= rv); break;
                }
                if (take) {
                    auto it = label_pc.find(op.jcc2.name);
                    if (it != label_pc.end()) pc = it->second;
                }
                break;
            }

            case PSOpCode::SET_U32: {
                ctx[op.keyimm.key] = op.keyimm.imm;
                break;
            }

            case PSOpCode::ADD_U32: {
                uint32_t v = 0; ctx.get<uint32_t>(op.keyimm.key, v);
                v += op.keyimm.imm;
                ctx[op.keyimm.key] = v;
                break;
            }

            case PSOpCode::APPLY_PLAN_FRAME_FROM_KEY: {
                // plan_id = ctx[key]
                uint32_t pid = 0;
                ctx.get<uint32_t>(op.a_key.id, pid);

                // tables
                auto itC = ctx.find(keys::battle::PLAN_COUNTS);
                auto itT = ctx.find(keys::battle::PLAN_TABLE);
                auto itN = ctx.find(keys::battle::NUM_PLANS);
                if (itC == ctx.end() || itT == ctx.end() || itN == ctx.end()) { break; }

                const auto* counts_s = std::get_if<std::string>(&itC->second);
                const auto* table_s = std::get_if<std::string>(&itT->second);
                if (!counts_s || !table_s) { break; }

                const uint8_t* counts = (const uint8_t*)counts_s->data();
                const uint8_t* frames = (const uint8_t*)table_s->data();
                const uint32_t nplans = std::get<uint32_t>(itN->second);

                if (pid >= nplans) {
                    ctx[keys::core::PLAN_DONE] = uint32_t(1);
                    break;
                }

                if (pid != last_dynamic_plan_id) {
                    ctx[keys::core::PLAN_FRAME_IDX] = uint32_t(0);
                    ctx[keys::core::PLAN_DONE] = uint32_t(0);
                    last_dynamic_plan_id = pid;
                }

                uint32_t idx = 0; ctx.get<uint32_t>(keys::core::PLAN_FRAME_IDX, idx);
                const uint32_t count = *(const uint32_t*)(counts + pid * 4);

                if (idx < count) {
                    size_t prior = 0;
                    for (uint32_t i = 0; i < pid; ++i) prior += *(const uint32_t*)(counts + i * 4);
                    const size_t off = (prior + idx) * sizeof(GCInputFrame);

                    GCInputFrame f{};
                    std::memcpy(&f, frames + off, sizeof(GCInputFrame));
                    host_.setInput(f);
                    host_.stepOneFrameBlocking();

                    ++idx;
                    ctx[keys::core::PLAN_FRAME_IDX] = idx;
                    if (idx >= count) ctx[keys::core::PLAN_DONE] = uint32_t(1);
                }
                break;
            }

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

                const uint32_t poll_ms = host_.pickPollIntervalMs(timeout_ms);
                const bool watch_movie = true;

                // progress sink handled inside wrapper; host has per-job sink already

                auto t0 = std::chrono::steady_clock::now();
                auto rr = host_.runUntilBreakpointFlexible(timeout_ms, vi_stall_ms, watch_movie, poll_ms);
                auto t1 = std::chrono::steady_clock::now();
                const uint32_t elapsed_ms = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

                RunToBpOutcome outcome = RunToBpOutcome::Unknown;
                if (rr.hit) outcome = RunToBpOutcome::Hit;
                else if (rr.reason) {
                    if (std::strcmp(rr.reason, "timeout") == 0)     outcome = RunToBpOutcome::Timeout;
                    else if (std::strcmp(rr.reason, "vi_stalled") == 0)  outcome = RunToBpOutcome::ViStalled;
                    else if (std::strcmp(rr.reason, "movie_ended") == 0) outcome = RunToBpOutcome::MovieEnded;
                    else outcome = RunToBpOutcome::Unknown;
                }

                // mirror -> ctx
                ctx[keys::core::OUTCOME_CODE] = static_cast<uint32_t>(outcome);
                ctx[keys::core::ELAPSED_MS] = elapsed_ms;
                ctx[keys::core::RUN_HIT_PC] = rr.hit ? rr.pc : 0u;

                // derive hit BP id by matching PC
                uint32_t hit_bp_id = 0;
                if (rr.hit) {
                    for (auto k : canonical_bp_keys_) {
                        if (const auto* e = bpmap_.find(k); e && e->pc == rr.pc) { hit_bp_id = (uint32_t)k; break; }
                    }
                }
                ctx[keys::core::RUN_HIT_BP] = hit_bp_id;

                // mirror -> R.ctx
                R.ctx[keys::core::OUTCOME_CODE] = static_cast<uint32_t>(outcome);
                R.ctx[keys::core::ELAPSED_MS] = elapsed_ms;
                R.ctx[keys::core::RUN_HIT_PC] = rr.hit ? rr.pc : 0u;
                R.ctx[keys::core::RUN_HIT_BP] = hit_bp_id;
                R.ctx[keys::core::VI_LAST] = (uint32_t)(host_.getViFieldCountApprox() & 0xFFFFFFFFull);
                R.ctx[keys::core::POLL_MS] = poll_ms;

                break; // never return; VM continues
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

            case PSOpCode::GET_BATTLE_CONTEXT:
            {
                std::string mem1;
                if (!host_.getMem1(mem1)) { R.ok = false; break; }
                simcore::MemView view(reinterpret_cast<const uint8_t*>(mem1.data()), mem1.size());
                simcore::battlectx::BattleContext bc{};
                if (!simcore::battlectx::codec::extract_from_mem1(view, bc)) { R.ok = false; break; }
                std::string blob;
                simcore::battlectx::codec::encode(bc, blob);
                R.ctx[simcore::keys::battle::CTX_BLOB] = blob;
                break;
            }

            case PSOpCode::EMIT_RESULT:
            {
                SCLOGD("[VM] EMIT_RESULT %s=%08X", keys::name_for_id(op.a_key.id).data(), ctx[op.a_key.id]);
                R.ctx[op.a_key.id] = ctx[op.a_key.id]; // copy selected value to result
                break;
            }

            case PSOpCode::RETURN_RESULT: {
                R.ctx[keys::core::OUTCOME_CODE] = op.imm.v;
                R.ok = true;
                return R;
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

            case PSOpCode::ARM_BPS_FROM_PRED_TABLE: {
                auto itN = ctx.find(keys::core::PRED_COUNT);
                auto itT = ctx.find(keys::core::PRED_TABLE);
                if (itN != ctx.end() && itT != ctx.end()) {
                    const uint32_t n = std::get<uint32_t>(itN->second);
                    const auto* tbl = std::get_if<std::string>(&itT->second);
                    if (n && tbl) {
                        const auto* rec = reinterpret_cast<const simcore::battle::PredicateRecord*>(tbl->data());
                        std::vector<uint32_t> pcs; pcs.reserve(n);
                        for (uint32_t i = 0; i < n; ++i) {
                            const BPAddr* e = bpmap_.find(static_cast<BPKey>(rec[i].required_bp));
                            if (e && e->pc) pcs.push_back(e->pc);
                        }
                        if (!pcs.empty()) host_.armPcBreakpoints(pcs);
                    }
                }
                break;
            }

            case PSOpCode::CAPTURE_PRED_BASELINES: {
                auto itN = ctx.find(keys::core::PRED_COUNT);
                auto itT = ctx.find(keys::core::PRED_TABLE);
                auto itB = ctx.find(keys::core::PRED_BASELINES);
                if (itN != ctx.end() && itT != ctx.end() && itB != ctx.end()) {
                    const uint32_t n = std::get<uint32_t>(itN->second);
                    const auto* tbl = std::get_if<std::string>(&itT->second);
                    auto* bas = std::get_if<std::string>(&itB->second);
                    if (n && tbl && bas) {
                        const auto* rec = reinterpret_cast<const simcore::battle::PredicateRecord*>(tbl->data());
                        for (uint32_t i = 0; i < n; ++i) {
                            if (rec[i].flags & 1) {
                                uint64_t vbits = 0;
                                switch (rec[i].width) {
                                case 1: { uint8_t  v = 0; host_.readU8(rec[i].addr, v);  vbits = v; break; }
                                case 2: { uint16_t v = 0; host_.readU16(rec[i].addr, v); vbits = v; break; }
                                case 4: { uint32_t v = 0; host_.readU32(rec[i].addr, v); vbits = v; break; }
                                case 8: { double   v = 0; host_.readF64(rec[i].addr, v); std::memcpy(&vbits, &v, 8); break; }
                                default: break;
                                }
                                std::memcpy(bas->data() + i * sizeof(uint64_t), &vbits, sizeof(uint64_t));
                            }
                        }
                    }
                }
                break;
            }

            case PSOpCode::EVAL_PREDICATES_AT_HIT_BP: {
                uint32_t hit_bp = 0; ctx.get<uint32_t>(keys::core::RUN_HIT_BP, hit_bp);
                uint32_t pass = 0, total = 0;

                auto itN = ctx.find(keys::core::PRED_COUNT);
                auto itT = ctx.find(keys::core::PRED_TABLE);
                auto itB = ctx.find(keys::core::PRED_BASELINES);
                if (itN != ctx.end() && itT != ctx.end()) {
                    const uint32_t n = std::get<uint32_t>(itN->second);
                    const auto* tbl = std::get_if<std::string>(&itT->second);
                    const uint64_t* bas = nullptr;
                    if (itB != ctx.end()) {
                        const auto* s = std::get_if<std::string>(&itB->second);
                        bas = s && !s->empty() ? reinterpret_cast<const uint64_t*>(s->data()) : nullptr;
                    }
                    if (n && tbl) {
                        const auto* rec = reinterpret_cast<const simcore::battle::PredicateRecord*>(tbl->data());
                        for (uint32_t i = 0; i < n; ++i) {
                            if (rec[i].required_bp != hit_bp) continue;
                            ++total;

                            uint64_t vbits = 0; double fcur = 0.0;
                            switch (rec[i].width) {
                            case 1: { uint8_t  v = 0; host_.readU8(rec[i].addr, v);  vbits = v; break; }
                            case 2: { uint16_t v = 0; host_.readU16(rec[i].addr, v); vbits = v; break; }
                            case 4: { uint32_t v = 0; host_.readU32(rec[i].addr, v); vbits = v; break; }
                            case 8: { double   v = 0; host_.readF64(rec[i].addr, v); std::memcpy(&vbits, &v, 8); fcur = v; break; }
                            default: break;
                            }

                            bool ok = false;
                            if (rec[i].width == 8) {
                                double rhs; std::memcpy(&rhs, &rec[i].rhs, 8);
                                double lhs = (rec[i].kind == 1 && bas) ? (fcur - *reinterpret_cast<const double*>(&bas[i])) : fcur;
                                switch (rec[i].cmp) {
                                case 0: ok = (lhs == rhs); break;
                                case 1: ok = (lhs != rhs); break;
                                case 2: ok = (lhs < rhs); break;
                                case 3: ok = (lhs <= rhs); break;
                                case 4: ok = (lhs > rhs); break;
                                case 5: ok = (lhs >= rhs); break;
                                }
                            }
                            else {
                                uint64_t lhs = vbits;
                                if (rec[i].kind == 1 && bas) lhs = lhs - bas[i];
                                const uint64_t rhs = rec[i].rhs;
                                switch (rec[i].cmp) {
                                case 0: ok = (lhs == rhs); break;
                                case 1: ok = (lhs != rhs); break;
                                case 2: ok = (lhs < rhs); break;
                                case 3: ok = (lhs <= rhs); break;
                                case 4: ok = (lhs > rhs); break;
                                case 5: ok = (lhs >= rhs); break;
                                }
                            }
                            if (ok) ++pass;
                        }
                    }
                }
                ctx[keys::core::PRED_TOTAL_AT_BP] = total;
                ctx[keys::core::PRED_PASS_AT_BP] = pass;
                break;
            }

            case PSOpCode::RECORD_PROGRESS_AT_BP: {
                uint32_t tot = 0; ctx.get<uint32_t>(keys::core::PRED_TOTAL_AT_BP, tot);
                if (tot) {
                    uint32_t turn = 0, hitbp = 0, pass = 0;
                    ctx.get<uint32_t>(keys::battle::ACTIVE_TURN, turn);
                    ctx.get<uint32_t>(keys::core::RUN_HIT_BP, hitbp);
                    ctx.get<uint32_t>(keys::core::PRED_PASS_AT_BP, pass);
                    auto sink = host_.getProgressSink();
                    if (sink) {
                        auto tag = host_.getCurrentSctFileTag();
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), "turn=%u bp=%u pred=%u/%u %s", turn, hitbp, pass, tot, tag.c_str());
                        sink(0, 0, 0, 0, buf);
                    }
                }
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
