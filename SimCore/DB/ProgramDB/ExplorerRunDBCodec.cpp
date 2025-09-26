// SimCore/DB/ProgramDB/ProgramExplorerRunDB.cpp
#include "ExplorerRunDBCodec.h"
#include "IniKV.h"
#include "Fingerprint.h"
#include "../DBCore/CoordinatorClock.h"
#include "../DBCore/DbResult.h"
#include "../DBCore/DbEnv.h"

#include "../BattlePlanAtomRepo.h"
#include "../BattlePlanTurnRepo.h"
#include "../PredicateSpecRepo.h"
#include "../AddressProgramRepo.h"
#include "../ExplorerRunRepo.h"

#include "../../Phases/Programs/ProgramRegistry.h"
#include "../../Phases/Programs/BattleRunner/BattleRunnerPayload.h"
#include "../../Runner/Script/PSContext.h"
#include "../../Runner/IPC/Wire.h"

#include <sqlite3.h>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

using simcore::db::BattlePlanAtomRepo;
using simcore::db::BattlePlanTurnRepo;
using simcore::db::PredicateSpecRepo;
using simcore::db::PredicateSpecRow;
using simcore::db::AddressProgramRepo;
using simcore::db::ExplorerRunRepo;

static constexpr int kPK = PK_BattleTurnRunner; // from Wire.h
static constexpr int kProgramVersion = phase::battle::runner::PayloadVersion;       // from BattleRunnerPayload.h

static inline DbResult<void> insert_job_event(DbEnv& env, int64_t job_id, const char* kind, const std::string& payload) {
    sqlite3* db = env.handle();
    sqlite3_stmt* st{};
    int rc = sqlite3_prepare_v2(db,
        "INSERT INTO job_events(job_id, event_kind, payload) VALUES(?,?,?);",
        -1, &st, nullptr);
    if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"prepare event" });
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, kind, -1, SQLITE_TRANSIENT);
    if (!payload.empty()) sqlite3_bind_text(st, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, 3);
    rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"insert event" });
    return DbResult<void>::Ok();
}

static inline DbResult<int64_t> insert_job(DbEnv& env, int64_t job_set_id, int64_t program_ref_id, int priority,
    const std::string& fingerprint) {
    sqlite3* db = env.handle();
    sqlite3_stmt* st{};
    int rc = sqlite3_prepare_v2(db,
        "INSERT INTO jobs(job_set_id, program_kind, program_version, program_ref_id, fingerprint, priority, state, attempts, max_attempts, queued_at) "
        "VALUES(?,?,?,?,?,?, 'QUEUED', 0, 1, strftime('%s','now'));",
        -1, &st, nullptr);
    if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare jobs" });
    sqlite3_bind_int64(st, 1, job_set_id);
    sqlite3_bind_int(st, 2, kPK);
    sqlite3_bind_int(st, 3, kProgramVersion);
    sqlite3_bind_int64(st, 4, program_ref_id);
    sqlite3_bind_text(st, 5, fingerprint.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 6, priority);
    rc = sqlite3_step(st);
    if (rc != SQLITE_DONE) { int ec = rc; sqlite3_finalize(st); return DbResult<int64_t>::Err({ map_sqlite_err(ec),ec,"insert jobs" }); }
    int64_t job_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(st);
    return DbResult<int64_t>::Ok(job_id);
}

static inline DbResult<void> set_job_state(DbEnv& env, int64_t job_id, const char* state) {
    sqlite3* db = env.handle();
    sqlite3_stmt* st{};
    int rc = sqlite3_prepare_v2(db, "UPDATE jobs SET state=? WHERE job_id=?;", -1, &st, nullptr);
    if (rc != SQLITE_OK) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"prepare state" });
    sqlite3_bind_text(st, 1, state, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, job_id);
    rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return DbResult<void>::Err({ map_sqlite_err(rc),rc,"update state" });
    return DbResult<void>::Ok();
}

static inline DbResult<std::string> get_enqueued_blueprint(DbEnv& env, int64_t job_id) {
    sqlite3* db = env.handle();
    sqlite3_stmt* st{};
    int rc = sqlite3_prepare_v2(db,
        "SELECT payload FROM job_events WHERE job_id=? AND event_kind='ENQUEUED' ORDER BY event_id ASC LIMIT 1;",
        -1, &st, nullptr);
    if (rc != SQLITE_OK) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"prepare get ENQUEUED" });
    sqlite3_bind_int64(st, 1, job_id);
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<std::string>::Err({ DbErrorKind::NotFound, rc, "no ENQUEUED" }); }
    const unsigned char* txt = sqlite3_column_text(st, 0);
    std::string out = txt ? reinterpret_cast<const char*>(txt) : std::string();
    sqlite3_finalize(st);
    return DbResult<std::string>::Ok(std::move(out));
}

static inline DbResult<int64_t> get_job_program_ref(DbEnv& env, int64_t job_id) {
    sqlite3* db = env.handle();
    sqlite3_stmt* st{};
    int rc = sqlite3_prepare_v2(db, "SELECT program_ref_id FROM jobs WHERE job_id=?;", -1, &st, nullptr);
    if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare ref" });
    sqlite3_bind_int64(st, 1, job_id);
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) { sqlite3_finalize(st); return DbResult<int64_t>::Err({ DbErrorKind::NotFound, rc, "job missing" }); }
    int64_t ref = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return DbResult<int64_t>::Ok(ref);
}

static inline std::optional<std::string> kv_get(const std::vector<std::pair<std::string, std::string>>& kv, const char* key) {
    for (auto& p : kv) if (p.first == key) return p.second;
    return std::nullopt;
}

DbResult<int64_t> ProgramExplorerRunDB::encode_job_into_db(DbEnv& env, int64_t job_set_id, const std::string& blueprint_ini) {
    auto kv = IniKV::parse(blueprint_ini);

    auto run_id_s = kv_get(kv, "explorer_run_id");
    auto run_ms_s = kv_get(kv, "run_ms");
    auto vi_ms_s = kv_get(kv, "vi_stall_ms");
    auto prio_s = kv_get(kv, "priority");
    auto turn_cnt_s = kv_get(kv, "turn_count");
    if (!run_id_s || !run_ms_s || !vi_ms_s || !turn_cnt_s)
        return DbResult<int64_t>::Err({ DbErrorKind::InvalidArgument, 0, "missing keys" });

    int64_t settings_id = std::stoll(*run_id_s);
    int run_ms = std::stoi(*run_ms_s);
    int vi_ms = std::stoi(*vi_ms_s);
    int priority = prio_s ? std::stoi(*prio_s) : 0;
    int turn_count = std::stoi(*turn_cnt_s);

    // Persist battle path: turns + actor bindings
    for (int t = 0; t < turn_count; ++t) {
        std::string fakek = "turn." + std::to_string(t) + ".fake_atk_count";
        auto fak = kv_get(kv, fakek.c_str());
        int fake_atk_count = fak ? std::stoi(*fak) : 0;
        auto rc1 = BattlePlanTurnRepo::UpsertTurn(env, settings_id, t, fake_atk_count);
        if (!rc1.ok) return DbResult<int64_t>::Err(rc1.error);

        std::string actor_cnt_k = "turn." + std::to_string(t) + ".actor_count";
        int actor_count = 0;
        if (auto ac = kv_get(kv, actor_cnt_k.c_str())) actor_count = std::stoi(*ac);

        for (int a = 0; a < actor_count; ++a) {
            std::string base = "turn." + std::to_string(t) + ".actor." + std::to_string(a) + ".";
            auto atom_id_s = kv_get(kv, (base + "atom_id").c_str());
            int64_t atom_id = 0;

            if (atom_id_s) {
                atom_id = std::stoll(*atom_id_s);
            }
            else {
                auto wv = kv_get(kv, (base + "wire_version").c_str());
                auto act = kv_get(kv, (base + "action_type").c_str());
                auto slot = kv_get(kv, (base + "actor_slot").c_str());
                auto prel = std::optional<std::string>("0"); // no prelude in new arch
                auto item = kv_get(kv, (base + "param_item_id").c_str());
                auto targ = kv_get(kv, (base + "target_slot").c_str());
                auto w64 = kv_get(kv, (base + "wire_b64").c_str());
                if (!wv || !act || !slot || !targ || !w64)
                    return DbResult<int64_t>::Err({ DbErrorKind::InvalidArgument, 0, "missing atom fields" });

                // decode base64
                std::vector<uint8_t> wire_bytes;
                {
                    const std::string& s = *w64;
                    static const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                    int val = 0, valb = -8;
                    for (unsigned char c : s) {
                        if (isspace(c)) continue;
                        if (c == '=') break;
                        int idx = b64.find(c);
                        if (idx == (int)std::string::npos) continue;
                        val = (val << 6) + idx; valb += 6;
                        if (valb >= 0) { wire_bytes.push_back((uint8_t)((val >> valb) & 0xFF)); valb -= 8; }
                    }
                }

                auto ens = BattlePlanAtomRepo::Ensure(env,
                    std::stoi(*wv),
                    wire_bytes,
                    std::stoi(*act),
                    std::stoi(*slot),
                    0,
                    item ? std::stoi(*item) : 0,
                    std::stoi(*targ));
                if (!ens.ok) return DbResult<int64_t>::Err(ens.error);
                atom_id = ens.value;
            }

            auto rc2 = BattlePlanTurnRepo::UpsertTurnActor(env, settings_id, t, a, atom_id);
            if (!rc2.ok) return DbResult<int64_t>::Err(rc2.error);
        }
    }

    // Predicates: expect either pred_count + pred.N.id, or skip if 0
    int pred_count = 0;
    if (auto pc = kv_get(kv, "predicate_count")) pred_count = std::stoi(*pc);
    for (int i = 0; i < pred_count; ++i) {
        std::string k = "predicate." + std::to_string(i) + ".id";
        auto pid = kv_get(kv, k.c_str());
        if (!pid) return DbResult<int64_t>::Err({ DbErrorKind::InvalidArgument, 0, "predicate missing id" });

        // link in explorer_settings_predicate
        sqlite3* db = env.handle();
        sqlite3_stmt* st{};
        int rc = sqlite3_prepare_v2(db,
            "INSERT INTO explorer_settings_predicate(settings_id, ordinal, predicate_id) VALUES(?,?,?) "
            "ON CONFLICT(settings_id, ordinal) DO UPDATE SET predicate_id=excluded.predicate_id;",
            -1, &st, nullptr);
        if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"prepare link pred" });
        sqlite3_bind_int64(st, 1, settings_id);
        sqlite3_bind_int(st, 2, i);
        sqlite3_bind_int64(st, 3, std::stoll(*pid));
        rc = sqlite3_step(st); sqlite3_finalize(st);
        if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc),rc,"link pred" });
    }

    std::string fp = make_job_fingerprint(kPK, kProgramVersion, blueprint_ini);
    auto jid = insert_job(env, job_set_id, settings_id, /*priority*/priority, fp);
    if (!jid.ok) return jid;

    auto ev = insert_job_event(env, jid.value, "ENQUEUED", blueprint_ini);
    if (!ev.ok) return DbResult<int64_t>::Err(ev.error);
    return jid;
}

DbResult<simcore::PSJob> ProgramExplorerRunDB::decode_job_from_db(DbEnv& env, int64_t job_id) {
    auto ref = get_job_program_ref(env, job_id);
    if (!ref.ok) return DbResult<simcore::PSJob>::Err(ref.error);
    int64_t settings_id = ref.value;

    // Load turns + actor bindings
    auto turns = BattlePlanTurnRepo::LoadTurns(env, settings_id);
    if (!turns.ok) return DbResult<simcore::PSJob>::Err(turns.error);

    // Build EncodeSpec
    phase::battle::runner::EncodeSpec spec{};
    spec.run_ms = 0;
    spec.vi_stall_ms = 0;
    {
        auto bp = get_enqueued_blueprint(env, job_id);
        if (bp.ok) {
            auto kv = IniKV::parse(bp.value);
            if (auto v = kv_get(kv, "run_ms")) spec.run_ms = std::stoi(*v);
            if (auto v = kv_get(kv, "vi_stall_ms")) spec.vi_stall_ms = std::stoi(*v);
        }
    }

    // Turn plans buffer
    {
        // TODO: we do not want to make the turn plan wire here. lets use the spec and re-create a BattlePath that we can send into the payload encoder.
        
        //std::vector<battle::runner::TurnPlanWire> tp; // assume this struct exists in payload header
        //tp.reserve(turns.ok().size());
        //for (auto& row : turns.ok()) {
        //    battle::runner::TurnPlanWire tw{};
        //    tw.fake_attack_count = row.fake_atk_count;
        //    tw.actors.reserve(row.actors.size());
        //    for (auto& b : row.actors) {
        //        battle::runner::TurnActorWire aw{};
        //        aw.actor_index = b.actor_index;
        //        aw.atom_id = b.atom_id;
        //        tw.actors.push_back(aw);
        //    }
        //    tp.push_back(std::move(tw));
        //}
        //spec.turn_plans_buffer = battle::runner::encode_turn_plans_to_buffer(tp);
    }

    // Predicate table for VERSION=3
    {
        // TODO: we want to manually convert a predicate spec row to a predicate spec here. there is no encode_predicate_records.
        
        //sqlite3* db = env.handle();
        //sqlite3_stmt* st{};
        //int rc = sqlite3_prepare_v2(db,
        //    "SELECT predicate_id FROM explorer_settings_predicate WHERE settings_id=? ORDER BY ordinal ASC;",
        //    -1, &st, nullptr);
        //if (rc != SQLITE_OK) return DbResult<PSJob>::Err({ map_sqlite_err(rc),rc,"prepare sel preds" });
        //sqlite3_bind_int64(st, 1, settings_id);

        //std::vector<PredicateSpecRow> preds;
        //while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        //    int64_t pid = sqlite3_column_int64(st, 0);
        //    auto one = PredicateSpecRepo::Get(env, pid);
        //    if (!one.is_ok()) { sqlite3_finalize(st); return DbResult<PSJob>::Err(one.err()); }
        //    preds.push_back(one.ok());
        //}
        //sqlite3_finalize(st);
        //if (rc != SQLITE_DONE) return DbResult<PSJob>::Err({ map_sqlite_err(rc),rc,"scan preds" });

        //spec.predicate_records = battle::runner::encode_predicate_records(preds); // must exist in payload
        //spec.predicate_blob = battle::runner::encode_predicate_blob(preds);    // ditto
    }

    std::vector<uint8_t> payload;
    auto ok = phase::battle::runner::encode_payload(spec, payload);
    // TODO throw DbErr on failed to encode payload.
    simcore::PSJob out;
    out.payload = std::move(payload);
    return DbResult<simcore::PSJob>::Ok(std::move(out));
}

DbResult<void> ProgramExplorerRunDB::encode_progress_into_db(DbEnv& env, int64_t job_id, const std::string& line) {
    auto ref = get_job_program_ref(env, job_id);
    if (!ref.ok) return DbResult<void>::Err(ref.error);
    int64_t run_id = ref.value;

    auto ev = insert_job_event(env, job_id, "PROGRESS", line);
    if (!ev.ok) return ev;

    auto ap = ExplorerRunRepo::AppendProgress(env, run_id, line, /*max_bytes*/ 65536);
    if (!ap.ok) return DbResult<void>::Err(ap.error);
    return DbResult<void>::Ok();
}

DbResult<void> ProgramExplorerRunDB::encode_results_into_db(DbEnv& env, int64_t job_id, const std::string& results_ini, bool success) {
    auto ref = get_job_program_ref(env, job_id);
    if (!ref.ok) return DbResult<void>::Err(ref.error);
    int64_t run_id = ref.value;

    auto ev = insert_job_event(env, job_id, "RESULTS", results_ini);
    if (!ev.ok) return ev;

    auto st = set_job_state(env, job_id, "SUCCEEDED");
    if (!st.ok) return st;

    auto md = ExplorerRunRepo::MarkDone(env, run_id);
    if (!md.ok) return DbResult<void>::Err(md.error);

    return DbResult<void>::Ok();
}

DbResult<std::string> ProgramExplorerRunDB::decode_progress_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) {
    if (job_id) {
        auto ref = get_job_program_ref(env, *job_id);
        if (!ref.ok) return DbResult<std::string>::Err(ref.error);
        auto row = ExplorerRunRepo::GetById(env, ref.value);
        if (!row.is_ok()) return DbResult<std::string>::Err(row.err());
        return DbResult<std::string>::Ok(row.ok().progress_log);
    }
    if (job_set_id) {
        sqlite3* db = env.handle();
        sqlite3_stmt* st{};
        int rc = sqlite3_prepare_v2(db,
            "SELECT program_ref_id FROM jobs WHERE job_set_id=? ORDER BY job_id ASC;",
            -1, &st, nullptr);
        if (rc != SQLITE_OK) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"prepare list jobs" });
        sqlite3_bind_int64(st, 1, *job_set_id);
        std::string agg;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
            int64_t run_id = sqlite3_column_int64(st, 0);
            auto row = ExplorerRunRepo::GetById(env, run_id);
            if (row.is_ok() && !row.ok().progress_log.empty()) {
                agg.append(row.ok().progress_log);
                if (agg.size() > 200000) break;
            }
        }
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"scan jobs" });
        return DbResult<std::string>::Ok(std::move(agg));
    }
    return DbResult<std::string>::Ok(std::string());
}

DbResult<std::string> ProgramExplorerRunDB::decode_results_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) {
    sqlite3* db = env.handle();
    if (job_id) {
        sqlite3_stmt* st{};
        int rc = sqlite3_prepare_v2(db,
            "SELECT payload FROM job_events WHERE job_id=? AND event_kind='RESULTS' ORDER BY event_id DESC LIMIT 1;",
            -1, &st, nullptr);
        if (rc != SQLITE_OK) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"prepare res" });
        sqlite3_bind_int64(st, 1, *job_id);
        rc = sqlite3_step(st);
        std::string out;
        if (rc == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(st, 0);
            if (txt) out = reinterpret_cast<const char*>(txt);
        }
        sqlite3_finalize(st);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"step res" });
        return DbResult<std::string>::Ok(std::move(out));
    }
    if (job_set_id) {
        sqlite3_stmt* st{};
        int rc = sqlite3_prepare_v2(db,
            "SELECT e.payload "
            "FROM job_events e JOIN jobs j ON e.job_id=j.job_id "
            "WHERE j.job_set_id=? AND e.event_kind='RESULTS' "
            "ORDER BY e.event_id ASC;",
            -1, &st, nullptr);
        if (rc != SQLITE_OK) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"prepare res set" });
        sqlite3_bind_int64(st, 1, *job_set_id);
        std::string agg;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(st, 0);
            if (txt) { agg.append(reinterpret_cast<const char*>(txt)); if (agg.back() != '\n') agg.push_back('\n'); }
        }
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) return DbResult<std::string>::Err({ map_sqlite_err(rc),rc,"scan res set" });
        return DbResult<std::string>::Ok(std::move(agg));
    }
    return DbResult<std::string>::Ok(std::string());
}
