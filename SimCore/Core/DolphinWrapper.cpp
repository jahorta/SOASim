#include "DolphinWrapper.h"
#include "Input/InputPlanFmt.h"

#include "UICommon/UICommon.h"      // SetUserDirectory, CreateDirectories
#include "Common/FileUtil.h"
#include <filesystem>
#include <fstream>
#include "../Utils/SafeEnv.h"
#include "../Utils/Log.h"

// Dolphin headers (adjust paths to your tree)
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "Core/State.h"
#include "Core/Movie.h"
#include "Core/HW/Memmap.h"

#include "Core/ConfigManager.h"  // SConfig::Init/Shutdown/LoadSettings
#include "Core/Config/MainSettings.h"
#include "Common/Config/Config.h"

#include "Core/PowerPC/PowerPC.h"       // PowerPC::PowerPCManager, GetPPCState()

// DolphinWrapper.cpp
#include "InputCommon/ControllerInterface/ControllerInterface.h" // g_controller_interface
#include "InputCommon/InputConfig.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/GCPad.h"       // Pad::Initialize/Shutdown
#include "Core/HW/GCKeyboard.h"  // Keyboard::Initialize/Shutdown
#include "Core/HW/VideoInterface.h"

#include "DiscIO/Enums.h"
#include "DiscIO/VolumeDisc.h"

#include "Core/VideoCommon/VideoBackendBase.h"  // WindowSystemInfo, WindowSystemType

#include <thread>
#include <chrono>
#include <cstdarg>

#include "Core/PowerPC/BreakPoints.h"
#include <unordered_set>
#include "Shims/StateBufferShim.h"


using namespace std::chrono_literals;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace simcore {

    // --- small helpers ----------------------------------------------------------

    static inline const char* RegionToString(DiscIO::Region r) {
        switch (r) {
        case DiscIO::Region::NTSC_U: return "NTSC-U";
        case DiscIO::Region::NTSC_J: return "NTSC-J";
        case DiscIO::Region::PAL:    return "PAL";
        default:                     return "UNKNOWN";
        }
    }

    static WindowSystemInfo MakeHeadlessWSI() {
        WindowSystemInfo wsi{};
        wsi.type = WindowSystemType::Headless;
        return wsi;
    }

    // --- load settings helpers --------------------------------------------------

    static bool initPads(WindowSystemInfo wsi) {
        g_controller_interface.Initialize(wsi);
        Pad::Initialize();

        if (auto* ic = Pad::GetConfig()) {
            ic->LoadConfig();
            SCLOGI("[Pad] controllers=%d", ic->GetControllerCount());
        }

        Keyboard::Initialize();

        return true;
    }

    static bool loadDolphinGUISettings(WindowSystemInfo wsi) {
        SCLOGI("Loading Dolphin GUI settings");
        initPads(wsi);
        return true;
    }

    // --- Turn off Movie helper --------------------------------------------------

    static void DisarmAnyActiveMovie(Core::System& system)
    {
        using namespace Movie;

        Movie::MovieManager& movie = system.GetMovie();

        if (movie.IsPlayingInput())
            movie.EndPlayInput(false);
        movie.SetReadOnly(true);
    }

    // --- class impl -------------------------------------------------------------

    DolphinWrapper::DolphinWrapper()
    {
        m_system = &Core::System::GetInstance();
        //log::Logger::get().open_file("simcore.log", false);
        //log::Logger::get().set_levels(log::Level::Info, log::Level::Trace);
    }
    

    DolphinWrapper::~DolphinWrapper() {
        shutdownAll();
    }

    void DolphinWrapper::shutdownAll() {
        if (Core::IsRunning(*m_system))
            shutdownCore();
        log::Logger::get().close_file();
    }

    bool DolphinWrapper::isRunning() const noexcept {
        return Core::IsRunning(*m_system);
    }

    void DolphinWrapper::shutdownCore() {
        if (Core::IsRunning(*m_system))
            Core::Stop(*m_system);

        for (int i = 0; i < 50 && Core::IsRunning(*m_system); ++i) {
            Core::HostDispatchJobs(*m_system);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        Core::Shutdown(*m_system);
        SConfig::Shutdown();
    }

    void DolphinWrapper::stop() {
        if (Core::IsRunning(*m_system))
            Core::Stop(*m_system);
    }

    bool DolphinWrapper::loadGame(const std::string& iso_path)
    {
        if (!m_imported_from_qt) {
            SCLOGE("Must import sys and user folders from DolphinQT before loading a game. (Best to use Dolphin ver. 2506a");
            return false;
        }
        
        shutdownCore();

        const WindowSystemInfo wsi = MakeHeadlessWSI();

        setUserDirectory(std::filesystem::absolute(m_user_dir).string());
        SConfig::Init();
        SConfig::GetInstance().LoadSettings();

        sterilizeConfigs();

        m_system_pad_is_inited = loadDolphinGUISettings(wsi);

        auto volume = DiscIO::CreateVolume(iso_path);
        if (!volume)
            return false;
        m_disc_info = DiscInfo{ volume->GetGameID(), RegionToString(volume->GetRegion()) };

        auto boot = BootParameters::GenerateFromFile(iso_path);
        if (!BootManager::BootCore(*m_system, std::move(boot), wsi))
            return false;

        auto deadline = std::chrono::steady_clock::now() + 20s;
        while (!Core::IsRunning(*m_system) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);

        return Core::IsRunning(*m_system);
    }

    bool DolphinWrapper::runOnCpuThread(const std::function<void()>& fn, const bool waitForCompletion) const
    {
        if (!Core::IsRunning(*m_system))
            return false;

        const auto start = std::chrono::steady_clock::now();
        SCLOGD("[DW] runOnCpuThread begin wait=%d running=%d state=%d",
            waitForCompletion ? 1 : 0, Core::IsRunning(*m_system) ? 1 : 0, (int)Core::GetState(*m_system));

        std::atomic<bool> done{ false };
        Core::RunOnCPUThread(*m_system, [&] {
            fn();
            done = true;
            }, waitForCompletion);

        auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!done && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        SCLOGD("[DW] runOnCpuThread end   done=%d ms=%lld", done ? 1 : 0, (long long)ms);

        return done.load();
    }

    uint32_t DolphinWrapper::getPC()
    {
        if (Core::GetState(*m_system) == Core::State::Paused)
            return m_system->GetPowerPC().GetPPCState().pc;
        
        uint32_t pc = 0;
        runOnCpuThread([&] {
            pc = m_system->GetPowerPC().GetPPCState().pc;
            }, true );
        return pc;
    }

    uint64_t DolphinWrapper::getTBR()
    {
        if (Core::GetState(*m_system) == Core::State::Paused)
            return m_system->GetPowerPC().ReadFullTimeBaseValue();
        
        uint64_t tbr = 0;
        runOnCpuThread([&] {
            tbr = m_system->GetPowerPC().ReadFullTimeBaseValue();
            }, true);
        return tbr;
    }

    bool DolphinWrapper::loadSavestate(const std::string& state_path)
    {
        if (!Core::IsRunning(*m_system))
            return false;

        SCLOGD("[DW] loadSavestateFromFile begin path=%s is_cpu_thread=%d state=%d",
            state_path.c_str(), Core::IsCPUThread(), (int)Core::GetState(*m_system));

        uint32_t pc_before = getPC();
        uint64_t tbr_before = getTBR();

        const bool scheduled = runOnCpuThread([&] {
            State::LoadAs(*m_system, state_path);
            }, true);

        // (you already do pc_before/tbr_before)
        SCLOGD("[DW] loadSavestate scheduled=%d", scheduled ? 1 : 0);

        if (!scheduled) {
            return false;
        }

        DisarmAnyActiveMovie(*m_system);

        // Right before the final return
        const uint32_t pc_after = getPC();
        const uint64_t tbr_after = getTBR();

        SCLOGD("[DW] loadSavestate end state=%d pc:%08X->%08X tbr:%016llX->%016llX movie_disarmed",
            (int)Core::GetState(*m_system), pc_before, pc_after,
            (unsigned long long)tbr_before, (unsigned long long)tbr_after);

        if (Core::IsRunning(*m_system) && (pc_before != pc_after || tbr_before != tbr_after || state_path._Equal(m_last_save_state))) {
            m_last_save_state = state_path;
            return true;
        }
        else {
            return false;
        }
    }

    bool DolphinWrapper::saveSavestateBlocking(const std::string& path)
    {
        if (!Core::IsRunning(*m_system)) return false;
        runOnCpuThread([&] {
            State::SaveAs(*m_system, path);
            }, true);
        return true;
    }

    bool DolphinWrapper::saveStateToBuffer(Common::UniqueBuffer<u8>& buffer)
    {
        State::SaveToBuffer(*m_system, buffer);
        return true;
    }

    bool DolphinWrapper::loadStateFromBuffer(Common::UniqueBuffer<u8>& buf)
    {
        SCLOGD("[DW] loadStateFromBuffer begin state=%d is_cpu_thread=%d",
            (int)Core::GetState(*m_system), Core::IsCPUThread());
        const uint32_t pc_before = getPC();
        const uint64_t tbr_before = getTBR();
        
        SOASim_LoadFromBufferShim(*m_system, buf);

        const uint32_t pc_after = getPC();
        const uint64_t tbr_after = getTBR();
        SCLOGD("[DW] loadStateFromBuffer end   state=%d pc:%08X->%08X tbr:%016llX->%016llX",
            (int)Core::GetState(*m_system), pc_before, pc_after,
            (unsigned long long)tbr_before, (unsigned long long)tbr_after);
        return true;
    }

    bool DolphinWrapper::startMoviePlayback(const std::string& dtm_path)
    {
        SCLOGI("[Movie] PLAY {}", dtm_path);
        bool ok = false;
        auto& movie = m_system->GetMovie();
        runOnCpuThread([&] {
            ok = movie.PlayInput(dtm_path, new std::optional<std::string>{});
            }, true);
        return ok;
    }

    bool DolphinWrapper::endMoviePlaybackBlocking(uint32_t timeout_ms)
    {
        SCLOGI("[Movie] STOP (request)");
        auto& movie = m_system->GetMovie();
        runOnCpuThread([&] {
            movie.EndPlayInput(false);
            }, true);

        const auto deadline = steady_clock::now() + milliseconds(timeout_ms);
        while (movie.IsPlayingInput() && steady_clock::now() < deadline)
            std::this_thread::sleep_for(milliseconds(10));

        const bool stopped = !movie.IsPlayingInput();
        SCLOGI("[Movie] STOP {}", stopped ? "ok" : "timeout");
        return stopped;
    }

    bool DolphinWrapper::setGCMemoryCardA(const std::string& raw_path)
    {
        // We avoid Dolphin source changes: copy the provided RAW to the
        // standard per-region filenames so the current game will pick it up.
        // MemoryCardA.<REG>.raw is the path Dolphin expects for “Memory Card” mode.
        // (Documented widely in user guides/bug threads.) 
        // USA/JAP/PAL cover GC regions.
        namespace fs = std::filesystem;
        try {
            const fs::path gc_dir = fs::path(m_user_dir) / "GC";
            fs::create_directories(gc_dir);
            const char* names[] = { "MemoryCardA.USA.raw","MemoryCardA.JAP.raw","MemoryCardA.PAL.raw" };
            for (auto* n : names) {
                fs::copy_file(raw_path, gc_dir / n, fs::copy_options::overwrite_existing);
            }
            SCLOGI("[MemCard] Copied RAW to {}", gc_dir.string());
            return true;
        }
        catch (...) {
            return false;
        }
    }

    void DolphinWrapper::setUserDirectory(const std::string& abs_path)
    {
        const fs::path p = fs::absolute(abs_path);
        fs::create_directories(p);

        UICommon::SetUserDirectory(p.string());
        UICommon::Init();
    }

    void DolphinWrapper::applyNextInputFrame() {
        if (!m_system_pad_is_inited) return;

        if (m_cursor < m_plan.size()) {
            const auto& f = m_plan[m_cursor];
            SCLOGD("[INP] next #%zu btn=%04X main=(%u,%u) c=(%u,%u) trig=(%u,%u)",
                m_cursor, f.buttons, f.main_x, f.main_y, f.c_x, f.c_y, f.trig_l, f.trig_r);
            m_pad.setFrame(m_plan[m_cursor++]);
        }
        else {
            SCLOGD("[INP] next <neutral>");
            m_pad.setFrame(GCPadOverride::NeutralFrame());
        }
    }

    void DolphinWrapper::setInput(const GCInputFrame& f)
    {
        if (!m_system_pad_is_inited) return;

        m_pad.setFrame(f);

        SCLOGD("[INP] set btn=%04X main=(%u,%u) c=(%u,%u) trig=(%u,%u)",
            f.buttons, f.main_x, f.main_y, f.c_x, f.c_y, f.trig_l, f.trig_r);
    }

    // -- Frame Advancing --------------------------------

    bool DolphinWrapper::stepOneFrameBlocking(int timeout_ms)
    {
        if (!Core::IsRunning(*m_system))
            return false;

        SCLOGD("[DW/run] step begin state=%d", (int)Core::GetState(*m_system));
        Core::DoFrameStep(*m_system);   // schedules a single frame and re-pauses

        const bool ok = waitForPausedCoreState(timeout_ms);
        SCLOGD("[DW/run] step end   ok=%d state=%d pc=%08X", ok ? 1 : 0, (int)Core::GetState(*m_system), getPC());
        return ok;
    }

    static uint64_t g_vi_ticks_baseline = 0;

    void DolphinWrapper::resetViCounterBaseline()
    {
        auto& sys = Core::System::GetInstance();
        g_vi_ticks_baseline = sys.GetCoreTiming().GetTicks();
    }

    uint64_t DolphinWrapper::getViFieldCountApprox() const
    {
        auto& sys = Core::System::GetInstance();
        const uint64_t now_ticks = sys.GetCoreTiming().GetTicks();
        const uint64_t dt = (now_ticks >= g_vi_ticks_baseline) ? (now_ticks - g_vi_ticks_baseline) : 0;

        auto& vi = sys.GetVideoInterface();
        const uint32_t ticks_per_field = vi.GetTicksPerField();
        if (ticks_per_field == 0)
            return 0;

        return dt / ticks_per_field;
    }

    uint64_t DolphinWrapper::getFrameCountApprox(bool interlaced) const
    {
        const uint64_t fields = getViFieldCountApprox();
        return interlaced ? (fields / 2) : fields;
    }

    static inline void write_all(const fs::path& p, const std::string& s) {
        fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        ofs.write(s.data(), (std::streamsize)s.size());
    }

    static inline bool copy_tree(const fs::path& src, const fs::path& dst, std::string* err = nullptr) {
        if (!fs::exists(src)) { if (err) *err = "Missing source: " + src.string(); return false; }
        std::error_code ec;
        fs::create_directories(dst, ec);
        ec.clear();
        fs::copy(src, dst,
            fs::copy_options::recursive |
            fs::copy_options::overwrite_existing |
            fs::copy_options::copy_symlinks,
            ec);
        if (ec) { if (err) *err = "Copy failed: " + ec.message(); return false; }
        return true;
    }

    static inline bool require_exists_dir(const fs::path& p, const char* what, std::string* err) {
        if (!fs::exists(p) || !fs::is_directory(p)) {
            if (err) *err = std::string("Missing ") + what + ": " + p.string();
            return false;
        }
        return true;
    }

    // --- API -------------------------------------------------------------------

    bool DolphinWrapper::SetUserDirectory(const fs::path& user_dir)
    {
        m_user_dir = user_dir;
        m_imported_from_qt = false;
        try {
            fs::create_directories(m_user_dir / "Config");
            UICommon::SetUserDirectory(m_user_dir.string());
            UICommon::CreateDirectories();
            UICommon::Init();
            SConfig::Init();
            SConfig::GetInstance().LoadSettings();
            return true;
        }
        catch (...) { return false; }
    }

    void DolphinWrapper::ConfigurePortsStandardPadP1()
    {
        Config::SetCurrent(Config::GetInfoForSIDevice(0), SerialInterface::SIDevices::SIDEVICE_GC_CONTROLLER);
        Config::SetCurrent(Config::GetInfoForSIDevice(1), SerialInterface::SIDevices::SIDEVICE_NONE);
        Config::SetCurrent(Config::GetInfoForSIDevice(2), SerialInterface::SIDevices::SIDEVICE_NONE);
        Config::SetCurrent(Config::GetInfoForSIDevice(3), SerialInterface::SIDevices::SIDEVICE_NONE);
        if (m_system_pad_is_inited)   
        {
            Pad::Shutdown();
            Pad::Initialize();
        }
        else {
            Pad::Initialize();
            m_system_pad_is_inited = true;
        }
        m_pad.install();
    }

    bool DolphinWrapper::QueryPadStatus(int port, GCPadStatus* out) const
    {
        if (!out) return false;
        *out = Pad::GetStatus(port);
        return true;
    }


    bool DolphinWrapper::SetRequiredDolphinQtBaseDir(const fs::path& qt_base, std::string* error_out)
    {
        std::string err;
        if (!require_exists_dir(qt_base, "DolphinQt base", &err)) { if (error_out) *error_out = err; return false; }

        if (!fs::exists(qt_base / "portable.txt")) {
            if (error_out) *error_out = "portable.txt not found in base: " + qt_base.string();
            return false;
        }

        const fs::path sys = qt_base / "Sys";
        const fs::path user = qt_base / "User";
        if (!require_exists_dir(sys, "Sys", &err)) { if (error_out) *error_out = err; return false; }
        if (!require_exists_dir(user, "User", &err)) { if (error_out) *error_out = err; return false; }

        m_qt_base_dir = qt_base;
        m_imported_from_qt = false;
        return true;
    }

    bool DolphinWrapper::SyncFromDolphinQtBase(bool force, std::string* error_out)
    {
        if (m_qt_base_dir.empty()) {
            if (error_out) *error_out = "DolphinQt base dir not set. Call SetRequiredDolphinQtBaseDir() first.";
            return false;
        }
        if (m_imported_from_qt && !force) return true;

        std::string err;
        const fs::path base_user = m_qt_base_dir / "User";
        if (!require_exists_dir(base_user, "User", &err)) { if (error_out) *error_out = err; return false; }
        if (!copy_tree(base_user, m_user_dir, &err)) { if (error_out) *error_out = err; return false; }

        SConfig::GetInstance().LoadSettings();
        if (m_system_pad_is_inited)
        {
            Pad::Shutdown();
            Pad::Initialize();
        }

        m_imported_from_qt = true;
        return true;
    }

    bool DolphinWrapper::ApplyConfig(const simcore::SimConfig& cfg, std::string* error_out) {
        if (!SetUserDirectory(cfg.user_dir)) {
            if (error_out) *error_out = "Failed to set user directory: " + cfg.user_dir.string();
            return false;
        }
        std::string err;
        if (!SetRequiredDolphinQtBaseDir(cfg.qt_base_dir, &err)) {
            if (error_out) *error_out = "Invalid DolphinQt base: " + err;
            return false;
        }
        if (!SyncFromDolphinQtBase(/*force=*/false, &err)) {
            if (error_out) *error_out = "Failed to sync from base: " + err;
            return false;
        }
        return true;
    }

    bool simcore::DolphinWrapper::readU8(uint32_t addr, uint8_t& out) const
    {
        if (!isRunning()) return false;

        if (Core::GetState(*m_system) == Core::State::Paused)
        {
            auto& mem = m_system->GetMemory();
            out = mem.Read_U8(addr);
        }
        else {
            Core::CPUThreadGuard guard(Core::System::GetInstance());
            auto& mem = guard.GetSystem().GetMemory();
            out = mem.Read_U8(addr);
        }

        SCLOGT("[mem read] Successfully read u8: 0x%X", out);
        return true;
    }

    bool simcore::DolphinWrapper::readU16(uint32_t addr, uint16_t& out) const
    {
        if (!isRunning()) return false;

        if (Core::GetState(*m_system) == Core::State::Paused)
        {
            auto& mem = m_system->GetMemory();
            out = mem.Read_U16(addr);
        }
        else {
            Core::CPUThreadGuard guard(Core::System::GetInstance());
            auto& mem = guard.GetSystem().GetMemory();
            out = mem.Read_U16(addr);
        }

        SCLOGT("[mem read] Successfully read u16: 0x%X", out);
        return true;
    }

    bool simcore::DolphinWrapper::readU32(uint32_t addr, uint32_t& out) const
    {
        if (!isRunning()) return false;

        if (Core::GetState(*m_system) == Core::State::Paused)
        {
            auto& mem = m_system->GetMemory();
            out = mem.Read_U32(addr);
        }
        else {
            Core::CPUThreadGuard guard(Core::System::GetInstance());
            auto& mem = guard.GetSystem().GetMemory();
            out = mem.Read_U32(addr);
        }

        SCLOGT("[mem read] Successfully read u32: 0x%X", out);
        return true;
    }

    bool simcore::DolphinWrapper::readF32(uint32_t addr, float& out) const
    {
        if (!isRunning()) return false;

        uint32_t u;
        if (Core::GetState(*m_system) == Core::State::Paused)
        {
            auto& mem = m_system->GetMemory();
            u = mem.Read_U32(addr);
        }
        else {
            Core::CPUThreadGuard guard(Core::System::GetInstance());
            auto& mem = guard.GetSystem().GetMemory();
            u = mem.Read_U32(addr);
        }

        out = std::bit_cast<float>(u);
        SCLOGT("[mem read] Successfully read float: %.2f", out);
        return true;
    }

    bool simcore::DolphinWrapper::readF64(uint32_t addr, double& out) const
    {
        if (!isRunning()) return false;

        uint64_t u;
        if (Core::GetState(*m_system) == Core::State::Paused)
        {
            auto& mem = m_system->GetMemory();
            u = mem.Read_U64(addr);
        }
        else {
            Core::CPUThreadGuard guard(Core::System::GetInstance());
            auto& mem = guard.GetSystem().GetMemory();
            u = mem.Read_U64(addr);
        }

        out = std::bit_cast<double>(u);
        SCLOGT("[mem read] Successfully read double: %.2f", out);
        return true;
    }

    static bool contains_pc(const std::unordered_set<uint32_t>& s, uint32_t v) { return s.find(v) != s.end(); }
    struct ArmedSet { std::unordered_set<uint32_t> pcs; };
    static ArmedSet& armed_singleton() { static ArmedSet a; return a; }

    bool DolphinWrapper::armPcBreakpoints(const std::vector<uint32_t>& pcs)
    {
        SCLOGT("[core] arming breakpoints");
        auto& armed = armed_singleton().pcs;
        bool arm_result = runOnCpuThread([&] {
            for (auto pc : pcs)
            {
                if (armed.insert(pc).second)
                    m_system->GetPowerPC().GetBreakPoints().Add(pc);
            }
            }, true);
        SCLOGT("[core] properly loaded breakpoints: %s", arm_result ? "true" : "false");
        SCLOGT("[core] checking current breakpoints");
        for (auto bp : m_system->GetPowerPC().GetBreakPoints().GetStrings()) {
            SCLOGT("[core] Breakpoint Present: %s", bp.c_str());
        }
        return arm_result;
    }

    bool DolphinWrapper::disarmPcBreakpoints(const std::vector<uint32_t>& pcs)
    {
        SCLOGT("[core] disarming breakpoints");
        auto& armed = armed_singleton().pcs;
        bool disarm_result = runOnCpuThread([&] {
            for (auto pc : pcs)
            {
                auto it = armed.find(pc);
                if (it != armed.end())
                {
                    m_system->GetPowerPC().GetBreakPoints().Remove(pc);
                    armed.erase(it);
                }
            }
            }, true);

        SCLOGT("[core] properly removed breakpoints: %s", disarm_result ? "true" : "false");
        SCLOGT("[core] checking current breakpoints");
        for (auto bp : m_system->GetPowerPC().GetBreakPoints().GetStrings()) {
            SCLOGT("[core] Breakpoint Present: %s", bp.c_str());
        }
        return disarm_result;
    }

    void DolphinWrapper::clearAllPcBreakpoints()
    {
        SCLOGT("[core] disarming all breakpoints");
        auto& armed = armed_singleton().pcs;
        bool disarm_result = runOnCpuThread([&] {
            for (auto pc : armed) m_system->GetPowerPC().GetBreakPoints().Remove(pc);
            armed.clear();
            }, true);
        SCLOGT("[core] properly removed breakpoints: %s", disarm_result ? "true" : "false");
    }

    DolphinWrapper::RunUntilHitResult DolphinWrapper::runUntilBreakpointBlocking(uint32_t timeout_ms)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        auto& armed = armed_singleton().pcs;

        SCLOGD("[DW/run] until_bp start timeout_ms=%u armed=%zu state=%d",
            timeout_ms, armed.size(), (int)Core::GetState(*m_system));

        size_t polls = 0;
        while (std::chrono::steady_clock::now() < deadline)
        {
            Core::SetState(*m_system, Core::State::Running);
            if (waitForPausedCoreState(5000)) {
                const uint32_t pc = getPC();
                if (contains_pc(armed, pc)) {
                    SCLOGD("[DW/run] until_bp HIT pc=%08X polls=%zu", pc, polls);
                    return { true, pc, "breakpoint" };
                }
            }
            if ((polls++ & 0x3F) == 0) // every 64 polls
                SCLOGD("[DW/run] until_bp poll=%zu state=%d pc=%08X", polls, (int)Core::GetState(*m_system), getPC());
        }
        SCLOGD("[DW/run] until_bp TIMEOUT polls=%zu last_pc=%08X", polls, getPC());
        return { false, 0u, "timeout" };
    }

    void DolphinWrapper::silenceStdOutInfo()
    {
        log::Logger::get().set_stdout_level(log::Level::Warn);
    }

    void DolphinWrapper::restoreStdOutInfo()
    {
        log::Logger::get().set_stdout_level(log::Level::Info);
    }

    bool DolphinWrapper::waitForPausedCoreState(uint32_t timeout_ms, uint32_t poll_rate_ms)
    {
        const auto start = std::chrono::steady_clock::now();
        SCLOGD("[DW/run] waitForPaused start state=%d timeout=%u", (int)Core::GetState(*m_system), timeout_ms);

        auto deadline = start + std::chrono::milliseconds(timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            if (Core::GetState(*m_system) == Core::State::Paused) 
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_rate_ms));
        }
        

        bool result = Core::GetState(*m_system) == Core::State::Paused;        
        SCLOGD("[DW/run] waitForPaused end ok=%d waited_ms=%lld state=%d",
            result ? 1 : 0,
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count(),
            (int)Core::GetState(*m_system));
        return result;
    }

    void DolphinWrapper::sterilizeConfigs()
    {
        SCLOGT("Setting GFX Backend to Null.");
        Config::SetCurrent(Config::MAIN_GFX_BACKEND, std::string("Null"));

        SCLOGT("Turning off background input.");
        Config::SetCurrent(Config::MAIN_INPUT_BACKGROUND_INPUT, false);
    }

} // namespace simcore

