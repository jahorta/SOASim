// SimCore/DB/ProgramDB/IProgramDBCodec.h
#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../DBCore/DbEnv.h"
#include "../DBCore/DbResult.h"
#include "../../Runner/Script/PSContext.h"

using namespace simcore::db;

struct PSJob { std::vector<uint8_t> payload; simcore::PSContext ctx; };

struct IProgramDBCodec {

    virtual ~IProgramDBCodec() = default;

    // returns job_id or Err if duplicate (use UNIQUE fingerprint)
    virtual DbResult<int64_t> encode_job_into_db(DbEnv& env, int64_t job_set_id, const std::string& blueprint_ini) = 0;

    // reconstruct a terminal PSJob from DB state
    virtual DbResult<simcore::PSJob>   decode_job_from_db(DbEnv& env, int64_t job_id) = 0;

    // append progress line (SeedProbe: effectively no-op beyond job_events)
    virtual DbResult<void>    encode_progress_into_db(DbEnv& env, int64_t job_id, const std::string& progress_line) = 0;

    // write INI results and flip job + domain states
    virtual DbResult<void>    encode_results_into_db(DbEnv& env, int64_t job_id, const std::string& results_ini, bool success) = 0;

    // aggregate progress for job_id or entire job_set_id
    virtual DbResult<std::string> decode_progress_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) = 0;

    // aggregate results for job_id or entire job_set_id
    virtual DbResult<std::string> decode_results_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) = 0;
};

struct ProgramDBCodecRegistry {
    static IProgramDBCodec& for_kind(int program_kind);
    static void register_codec(int program_kind, IProgramDBCodec* impl);
};
