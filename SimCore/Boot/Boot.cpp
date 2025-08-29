#include "Boot.h"

#include <fstream>
#include <utility>

namespace simboot {

    static inline bool is_dir(const std::filesystem::path& p) {
        return !p.empty() && std::filesystem::exists(p) && std::filesystem::is_directory(p);
    }

    bool BootDolphinWrapper(simcore::DolphinWrapper& dw, const BootOptions& opts, std::string* error_out)
    {
        std::string err;

        // 0) Validate inputs up-front
        if (!is_dir(opts.user_dir)) {
            std::error_code ec;
            std::filesystem::create_directories(opts.user_dir, ec);
            if (ec) { if (error_out) *error_out = "Could not create user_dir: " + opts.user_dir.string(); return false; }
        }
        if (!is_dir(opts.dolphin_qt_base)) {
            if (error_out) *error_out = "Invalid DolphinQt base: " + opts.dolphin_qt_base.string();
            return false;
        }
        if (!std::filesystem::exists(opts.dolphin_qt_base / "portable.txt")) {
            if (error_out) *error_out = "DolphinQt base is not portable (portable.txt missing): " + opts.dolphin_qt_base.string();
            return false;
        }
        if (!is_dir(opts.dolphin_qt_base / "Sys") || !is_dir(opts.dolphin_qt_base / "User")) {
            if (error_out) *error_out = "DolphinQt base must contain Sys/ and User/: " + opts.dolphin_qt_base.string();
            return false;
        }

        // 1) Build wrapper and point it at our isolated User dir
        if (!dw.SetUserDirectory(opts.user_dir)) {
            if (error_out) *error_out = "SetUserDirectory() failed for: " + opts.user_dir.string();
            return false;
        }

        // 2) Require & remember the portable DolphinQt base
        if (!dw.SetRequiredDolphinQtBaseDir(opts.dolphin_qt_base, &err)) {
            if (error_out) *error_out = "SetRequiredDolphinQtBaseDir failed: " + err;
            return false;
        }

        // 3) Sync Sys/ + User/ from base into our user dir; reload configs & re-init pads
        if (!dw.SyncFromDolphinQtBase(opts.force_resync_from_base, &err)) {
            if (error_out) *error_out = "SyncFromDolphinQtBase failed: " + err;
            return false;
        }

        // 4) (Optional) force Port 1 = Standard Controller in-memory
        if (opts.force_p1_standard_pad) {
            dw.ConfigurePortsStandardPadP1();
        }

        // 5) Persist config for next time (paths only; never touches DolphinQt install)
        if (opts.save_config_on_success) {
            simcore::SimConfig cfg{ opts.user_dir, opts.dolphin_qt_base };
            std::string save_err;
            (void)simcore::SimConfigIO::Save(cfg, opts.config_path, &save_err);  // best-effort
        }

        return true;
    }

    bool BootDolphinWrapperFromSavedConfig(simcore::DolphinWrapper& dw, std::string* error_out,
            const std::filesystem::path& config_path)
    {
        std::string err;
        auto cfg = simcore::SimConfigIO::Load(config_path, &err);
        if (!cfg) {
            if (error_out) *error_out = "Load config failed (" + config_path.string() + "): " + err;
            return false;
        }

        BootOptions opts;
        opts.user_dir = cfg->user_dir;
        opts.dolphin_qt_base = cfg->qt_base_dir;
        opts.force_resync_from_base = false;   // usually not needed; set true if you want to refresh
        opts.force_p1_standard_pad = false;   // inherit bindings from the base's User/
        opts.save_config_on_success = false;   // already have one
        opts.config_path = config_path;

        return BootDolphinWrapper(dw, opts, error_out);
    }

} // namespace simcore
