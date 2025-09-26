// SimCore/DB/ProgramDB/ProgramSeedProbeDB.cpp
#include "SeedProbeDBCodec.h"
#include "IniKV.h"
#include "Fingerprint.h"
#include "../DBCore/CoordinatorClock.h"
#include "../Query.h" // assume simple helpers exist
#include "../../Phases/Programs/ProgramRegistry.h"
#include "../../Phases/Programs/SeedProbe/SeedProbePayload.h"
#include "../../Runner/Script/PSContext.h"
#include <sqlite3.h>

using namespace simcore;

static constexpr int PK = /* PK_SeedProbe */ 1;
static constexpr int PV = /* SeedProbe VERSION */ 1;

static int64_t get_program_ref_id_from_blueprint(const std::string& bp) {
    auto kv = IniKV::parse(bp);
    for (auto& p : kv) if (p.first == "seed_delta_id") return std::stoll(p.second);
    return 0;
}

DbResult<int64_t> ProgramSeedProbeDB::encode_job_into_db(DbEnv& env, int64_t job_set_id, const std::string& blueprint_ini) {
    int64_t prog_ref_id = get_program_ref_id_from_blueprint(blueprint_ini);
    std::string fp = make_job_fingerprint(PK, PV, blueprint_ini);
    sqlite3_stmt* st = nullptr;
    auto db = env.handle();
    sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO jobs(job_set_id,program_kind,program_version,program_ref_id,fingerprint,priority,state,attempts,max_attempts,queued_at) "
        "VALUES(?,?,?,?,?,?, 'QUEUED',0,1,strftime('%s','now'))", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_set_id);
    sqlite3_bind_int64(st, 2, PK);
    sqlite3_bind_int64(st, 3, PV);
    sqlite3_bind_int64(st, 4, prog_ref_id);
    sqlite3_bind_text(st, 5, fp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 6, 0);
    sqlite3_step(st);
    sqlite3_finalize(st);
    int64_t job_id = sqlite3_last_insert_rowid(db);
    if (job_id == 0) return DbResult<int64_t>::Err({ DbErrorKind::Unique,0,"duplicate" });
    sqlite3_prepare_v2(db, "INSERT INTO job_events(job_id,event_kind,payload) VALUES(?, 'ENQUEUED', ?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, blueprint_ini.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
    return DbResult<int64_t>::Ok(job_id);
}

DbResult<simcore::PSJob> ProgramSeedProbeDB::decode_job_from_db(DbEnv& env, int64_t job_id) {
    sqlite3_stmt* st = nullptr;
    auto db = env.handle();
    sqlite3_prepare_v2(db, "SELECT payload FROM job_events WHERE job_id=? AND event_kind='ENQUEUED' ORDER BY event_id LIMIT 1", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    std::string bp;
    if (sqlite3_step(st) == SQLITE_ROW) bp = (const char*)sqlite3_column_text(st, 0);
    sqlite3_finalize(st);
    auto kv = IniKV::parse(bp);
    seedprobe::EncodeSpec spec{};
    for (auto& p : kv) {
        if (p.first == "frame_hex") { /* decode */ }
        else if (p.first == "run_ms") spec.run_ms = (uint32_t)std::stoul(p.second);
        else if (p.first == "vi_stall_ms") spec.vi_stall_ms = (uint32_t)std::stoul(p.second);
    }
    std::vector<uint8_t> payload;
    if (!seedprobe::encode_payload(spec, payload)) return DbResult<simcore::PSJob>::Err({ .kind=DbErrorKind::IO, .message="Failed to encode payload"});
    simcore::PSJob job; job.payload = std::move(payload);
    programs::decode_payload_for(PK, job.payload, job.ctx);
    return DbResult<simcore::PSJob>::Ok(std::move(job));
}

DbResult<void> ProgramSeedProbeDB::encode_progress_into_db(DbEnv& env, int64_t job_id, const std::string& line) {
    sqlite3_stmt* st = nullptr;
    auto db = env.handle();
    sqlite3_prepare_v2(db, "INSERT INTO job_events(job_id,event_kind,payload) VALUES(?, 'PROGRESS', ?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, line.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
    return DbResult<void>::Ok();
}

DbResult<void> ProgramSeedProbeDB::encode_results_into_db(DbEnv& env, int64_t job_id, const std::string& results_ini, bool success) {
    auto db = env.handle();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO job_events(job_id,event_kind,payload) VALUES(?, 'RESULTS', ?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, results_ini.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
    sqlite3_prepare_v2(db, "UPDATE jobs SET state='SUCCEEDED' WHERE job_id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_step(st); sqlite3_finalize(st);
    sqlite3_prepare_v2(db, "SELECT payload FROM job_events WHERE job_id=? AND event_kind='ENQUEUED' ORDER BY event_id LIMIT 1", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    std::string bp; if (sqlite3_step(st) == SQLITE_ROW) bp = (const char*)sqlite3_column_text(st, 0); sqlite3_finalize(st);
    int64_t delta_id = get_program_ref_id_from_blueprint(bp);
    sqlite3_prepare_v2(db, "UPDATE seed_delta SET is_done=1, done_at=strftime('%s','now') WHERE id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, delta_id);
    sqlite3_step(st); sqlite3_finalize(st);
    return DbResult<void>::Ok();
}

DbResult<std::string> ProgramSeedProbeDB::decode_progress_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) {
    auto db = env.handle();
    if (!job_set_id) return DbResult<std::string>::Ok("");
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM jobs WHERE job_set_id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, *job_set_id);
    int total = 0; if (sqlite3_step(st) == SQLITE_ROW) total = sqlite3_column_int(st, 0); sqlite3_finalize(st);
    sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM jobs WHERE job_set_id=? AND state='SUCCEEDED'", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, *job_set_id);
    int done = 0; if (sqlite3_step(st) == SQLITE_ROW) done = sqlite3_column_int(st, 0); sqlite3_finalize(st);
    IniKV ini; ini.add("total_jobs", std::to_string(total)); ini.add("completed_jobs", std::to_string(done));
    return DbResult<std::string>::Ok(ini.to_string_sorted());
}

DbResult<std::string> ProgramSeedProbeDB::decode_results_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) {
    auto db = env.handle();
    std::string out;
    sqlite3_stmt* st = nullptr;
    if (job_set_id) {
        sqlite3_prepare_v2(db,
            "SELECT je.payload FROM job_events je JOIN jobs j ON je.job_id=j.job_id "
            "WHERE j.job_set_id=? AND je.event_kind='RESULTS' ORDER BY je.event_id", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, *job_set_id);
    }
    else if (job_id) {
        sqlite3_prepare_v2(db, "SELECT payload FROM job_events WHERE job_id=? AND event_kind='RESULTS' ORDER BY event_id", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, *job_id);
    }
    else return DbResult<std::string>::Ok("");
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* p = (const char*)sqlite3_column_text(st, 0);
        if (p) { out.append(p); if (out.empty() || out.back() != '\n') out.push_back('\n'); }
    }
    sqlite3_finalize(st);
    return DbResult<std::string>::Ok(out);
}
