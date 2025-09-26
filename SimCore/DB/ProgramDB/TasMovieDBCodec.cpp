// SimCore/DB/ProgramDB/ProgramTasMovieDB.cpp
#include "TasMovieDBCodec.h"
#include "IniKV.h"
#include "Fingerprint.h"
#include "../DBCore/ObjectStore.h"
#include "../DBCore/CoordinatorClock.h"
#include "../../Phases/Programs/ProgramRegistry.h"
#include "../../Phases/Programs/PlayTasMovie/TasMoviePayload.h"
#include "../TasMovieRepo.h"
#include <filesystem>

static constexpr int PK = /* PK_TasMovie */ 2;
static constexpr int PV = /* TasMovie VERSION */ 1;

static std::string get_k(const std::vector<std::pair<std::string, std::string>>& kv, const char* k) {
    for (auto& p : kv) if (p.first == k) return p.second; return {};
}

static int64_t ensure_artifact_by_filename(DbEnv& env, const std::string& path) {
    auto db = env.handle();
    std::string filename = std::filesystem::path(path).filename().string();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT id FROM object_ref WHERE name=?", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) { int64_t id = sqlite3_column_int64(st, 0); sqlite3_finalize(st); return id; }
    sqlite3_finalize(st);
    ObjectStore os;
    auto id = os.finalize_from_file(filename.c_str(), path.c_str());
    return id;
}

DbResult<int64_t> ProgramTasMovieDB::encode_job_into_db(DbEnv& env, int64_t job_set_id, const std::string& blueprint_ini) {
    auto db = env.handle();
    auto kv = IniKV::parse(blueprint_ini);
    std::string dtm_path = get_k(kv, "dtm_path");
    int64_t dtm_artifact_id = ensure_artifact_by_filename(env, dtm_path);
    IniKV bp; for (auto& p : kv) bp.add(p.first, p.second);
    bp.add("dtm_artifact_id", std::to_string(dtm_artifact_id));
    std::string bp_txt = bp.to_string_sorted();
    std::string fp = make_job_fingerprint(PK, PV, bp_txt);

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO tas_movie(dtm_artifact_id) VALUES(?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, dtm_artifact_id);
    sqlite3_step(st); sqlite3_finalize(st);
    sqlite3_prepare_v2(db, "SELECT id FROM tas_movie WHERE dtm_artifact_id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, dtm_artifact_id);
    int64_t tas_movie_id = 0; if (sqlite3_step(st) == SQLITE_ROW) tas_movie_id = sqlite3_column_int64(st, 0); sqlite3_finalize(st);

    sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO jobs(job_set_id,program_kind,program_version,program_ref_id,fingerprint,priority,state,attempts,max_attempts,queued_at) "
        "VALUES(?,?,?,?,?,?, 'QUEUED',0,1,strftime('%s','now'))", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_set_id);
    sqlite3_bind_int64(st, 2, PK);
    sqlite3_bind_int64(st, 3, PV);
    sqlite3_bind_int64(st, 4, tas_movie_id);
    sqlite3_bind_text(st, 5, fp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 6, 0);
    sqlite3_step(st); sqlite3_finalize(st);
    int64_t job_id = sqlite3_last_insert_rowid(db);
    if (job_id == 0) return DbResult<int64_t>::Err({ DbErrorKind::Unique,0,"duplicate" });

    sqlite3_prepare_v2(db, "INSERT INTO job_events(job_id,event_kind,payload) VALUES(?, 'ENQUEUED', ?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, bp_txt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
    return DbResult<int64_t>::Ok(job_id);
}

DbResult<PSJob> ProgramTasMovieDB::decode_job_from_db(DbEnv& env, int64_t job_id) {
    auto db = env.handle();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT payload FROM job_events WHERE job_id=? AND event_kind='ENQUEUED' ORDER BY event_id LIMIT 1", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    std::string bp; if (sqlite3_step(st) == SQLITE_ROW) bp = (const char*)sqlite3_column_text(st, 0); sqlite3_finalize(st);
    auto kv = IniKV::parse(bp);
    std::string dtm_artifact_id = get_k(kv, "dtm_artifact_id");
    std::string save_dir = get_k(kv, "save_dir");
    uint32_t run_ms = (uint32_t)std::stoul(get_k(kv, "run_ms"));
    uint32_t vi_ms = (uint32_t)std::stoul(get_k(kv, "vi_stall_ms"));
    bool save_on_fail = get_k(kv, "save_on_fail") == "1";
    bool progress_enable = get_k(kv, "progress_enable") == "1";

    ObjectStore os;
    std::string dtm_path = os.materialize_to_temp(std::stoll(dtm_artifact_id));

    simcore::tasmovie::EncodeSpec spec{};
    spec.dtm_path = dtm_path.c_str();
    spec.save_dir = save_dir.c_str();
    spec.run_ms = run_ms;
    spec.vi_stall_ms = vi_ms;
    spec.save_on_fail = save_on_fail;
    spec.progress_enable = progress_enable;

    std::vector<uint8_t> payload;
    auto ok = simcore::tasmovie::encode_payload(spec, payload);
    PSJob job; job.payload = std::move(payload);
    return DbResult<PSJob>::Ok(std::move(job));
}

DbResult<void> ProgramTasMovieDB::encode_progress_into_db(DbEnv& env, int64_t job_id, const std::string& line) {
    auto db = env.handle();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO job_events(job_id,event_kind,payload) VALUES(?, 'PROGRESS', ?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, line.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);

    sqlite3_prepare_v2(db, "SELECT program_ref_id FROM jobs WHERE job_id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    int64_t tas_movie_id = 0; if (sqlite3_step(st) == SQLITE_ROW) tas_movie_id = sqlite3_column_int64(st, 0); sqlite3_finalize(st);

    TasMovieRepo::AppendProgress(env, tas_movie_id, line, 65536);
    return DbResult<void>::Ok();
}

DbResult<void> ProgramTasMovieDB::encode_results_into_db(DbEnv& env, int64_t job_id, const std::string& results_ini, bool success) {
    auto db = env.handle();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO job_events(job_id,event_kind,payload) VALUES(?, 'RESULTS', ?)", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    sqlite3_bind_text(st, 2, results_ini.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);

    sqlite3_prepare_v2(db, "SELECT program_ref_id FROM jobs WHERE job_id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, job_id);
    int64_t tas_movie_id = 0; if (sqlite3_step(st) == SQLITE_ROW) tas_movie_id = sqlite3_column_int64(st, 0); sqlite3_finalize(st);

    if (success) {
        auto kv = IniKV::parse(results_ini);
        int64_t savestate_art_id = 0;
        for (auto& p : kv) if (p.first == "savestate_artifact_id") savestate_art_id = std::stoll(p.second);
        sqlite3_prepare_v2(db, "INSERT INTO savestate(object_ref_id,created_at) VALUES(?,strftime('%s','now'))", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, savestate_art_id);
        sqlite3_step(st); sqlite3_finalize(st);
        int64_t savestate_id = sqlite3_last_insert_rowid(db);
        TasMovieRepo::MarkDone(env, tas_movie_id, savestate_id);
        sqlite3_prepare_v2(db, "UPDATE jobs SET state='SUCCEEDED' WHERE job_id=?", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, job_id);
        sqlite3_step(st); sqlite3_finalize(st);
    }
    else {
        TasMovieRepo::MarkFailed(env, tas_movie_id, "failed");
        sqlite3_prepare_v2(db, "UPDATE jobs SET state='FAILED' WHERE job_id=?", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, job_id);
        sqlite3_step(st); sqlite3_finalize(st);
    }
    return DbResult<void>::Ok();
}

DbResult<std::string> ProgramTasMovieDB::decode_progress_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) {
    auto db = env.handle();
    if (!job_id) return DbResult<std::string>::Ok("");
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT program_ref_id FROM jobs WHERE job_id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, *job_id);
    int64_t tas_movie_id = 0; if (sqlite3_step(st) == SQLITE_ROW) tas_movie_id = sqlite3_column_int64(st, 0); sqlite3_finalize(st);
    std::string log = TasMovieRepo::GetProgressLog(env, tas_movie_id);
    return DbResult<std::string>::Ok(log);
}

DbResult<std::string> ProgramTasMovieDB::decode_results_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) {
    auto db = env.handle();
    sqlite3_stmt* st = nullptr;
    std::string out;
    if (job_id) {
        sqlite3_prepare_v2(db, "SELECT payload FROM job_events WHERE job_id=? AND event_kind='RESULTS' ORDER BY event_id DESC LIMIT 1", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, *job_id);
        if (sqlite3_step(st) == SQLITE_ROW) { const char* p = (const char*)sqlite3_column_text(st, 0); if (p) out = p; }
        sqlite3_finalize(st);
    }
    else if (job_set_id) {
        sqlite3_prepare_v2(db,
            "SELECT je.payload FROM job_events je JOIN jobs j ON je.job_id=j.job_id "
            "WHERE j.job_set_id=? AND je.event_kind='RESULTS' ORDER BY je.event_id", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, *job_set_id);
        while (sqlite3_step(st) == SQLITE_ROW) { const char* p = (const char*)sqlite3_column_text(st, 0); if (p) { out.append(p); if (out.empty() || out.back() != '\n') out.push_back('\n'); } }
        sqlite3_finalize(st);
    }
    return DbResult<std::string>::Ok(out);
}
