// SimCore/DB/ProgramDB/ProgramExplorerRunDB.h
#pragma once
#include "IProgramDBCodec.h"
#include "../../Runner/Script/PhaseScriptVM.h"

struct ProgramExplorerRunDB final : IProgramDBCodec {
    DbResult<int64_t> encode_job_into_db(DbEnv& env, int64_t job_set_id, const std::string& blueprint_ini) override;
    DbResult<simcore::PSJob>   decode_job_from_db(DbEnv& env, int64_t job_id) override;
    DbResult<void>    encode_progress_into_db(DbEnv& env, int64_t job_id, const std::string& progress_line) override;
    DbResult<void>    encode_results_into_db(DbEnv& env, int64_t job_id, const std::string& results_ini, bool success) override;
    DbResult<std::string> decode_progress_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) override;
    DbResult<std::string> decode_results_from_db(DbEnv& env, std::optional<int64_t> job_id, std::optional<int64_t> job_set_id) override;
};
