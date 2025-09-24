#include "SavestateHelpers.h"
#include "SavestateRepo.h"
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DbResult<int64_t> SavestateHelpers::CreateFromTasMovie(DbEnv& env, int64_t tas_movie_id,
            int32_t savestate_type,
            const std::string& note_prefix,
            int64_t object_ref_id) {
            auto planned = SavestateRepo::plan(env, savestate_type, note_prefix + " TAS#" + std::to_string(tas_movie_id));
            if (!planned.ok) return planned;
            int64_t ss_id = planned.value;

            auto attach = SavestateRepo::finalize(env, ss_id, object_ref_id);
            if (!attach.ok) return DbResult<int64_t>::Err(attach.error);

            sqlite3* db = env.handle();
            sqlite3_stmt* st{};
            int rc = sqlite3_prepare_v2(db, "UPDATE savestate SET tas_movie_id=? WHERE id=?;", -1, &st, nullptr);
            if (rc != SQLITE_OK) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "prepare link" });
            sqlite3_bind_int64(st, 1, tas_movie_id);
            sqlite3_bind_int64(st, 2, ss_id);
            rc = sqlite3_step(st); sqlite3_finalize(st);
            if (rc != SQLITE_DONE) return DbResult<int64_t>::Err({ map_sqlite_err(rc), rc, "link update" });

            return DbResult<int64_t>::Ok(ss_id);
        }

    }
} // namespace simcore::db
