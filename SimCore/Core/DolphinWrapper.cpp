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

#include "Common/Logging/LogManager.h"
#include "Core/PowerPC/BreakPoints.h"
#include <unordered_set>


using namespace std::chrono_literals;
using namespace std;
namespace fs = std::filesystem;

namespace simcore {

    // ===== Impl lives here so DolphinWrapper is movable ==========================
    struct DolphinWrapper::Impl {
        std::mutex m_step_mtx;
        std::condition_variable m_step_cv;
        std::atomic<uint64_t> m_pause_seq{ 0 };
        bool m_state_cb_registered = false;
        int  m_state_cb_cookie = 0;
    };

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
        : m_impl(std::make_unique<Impl>())
    {
        m_system = &Core::System::GetInstance();
        log::Logger::get().open_file("simcore.log", false);
        log::Logger::get().set_levels(log::Level::Info, log::Level::Trace);
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
        if (!m_impl) return;

        if (m_impl->m_state_cb_registered) {
            Core::RemoveOnStateChangedCallback(&m_impl->m_state_cb_cookie);
            m_impl->m_state_cb_registered = false;
        }

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

        std::atomic<bool> done{ false };
        Core::RunOnCPUThread(*m_system, [&] {
            fn();
            done = true;
            }, waitForCompletion);

        auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!done && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);

        return done.load();
    }

    uint32_t DolphinWrapper::getPC()
    {
        Core::CPUThreadGuard guard(*m_system);
        return m_system->GetPowerPC().GetPPCState().pc;
    }

    uint64_t DolphinWrapper::getTBR()
    {
        Core::CPUThreadGuard guard(*m_system);
        return m_system->GetPowerPC().ReadFullTimeBaseValue();
    }

    bool DolphinWrapper::loadSavestate(const std::string& state_path)
    {
        if (!Core::IsRunning(*m_system))
            return false;

        auto* mgr = Common::Log::LogManager::GetInstance();
        mgr->EnableListener(Common::Log::LogListener::FILE_LISTENER, true);
        mgr->SetEnable(Common::Log::LogType::COMMON, true);

        auto b = [&](const auto& info) { return !!Config::Get(info); };
        SCLOGI("DSP_HLE=%d DSP_THREAD=%d CPU_THREAD=%d\n",
            int(b(Config::MAIN_DSP_HLE)),
            int(b(Config::MAIN_DSP_THREAD)),
            int(b(Config::MAIN_CPU_THREAD)));

        uint32_t pc_before = getPC();
        uint64_t tbr_before = getTBR();

        const bool scheduled = runOnCpuThread([&] {
            State::LoadAs(*m_system, state_path);
            });

        if (!scheduled) {
            return false;
        }

        DisarmAnyActiveMovie(*m_system);

        if ((pc_before != getPC() || tbr_before != getTBR() ||
            (!m_ran_since_last_load && strcmp(m_last_save_state.c_str(), state_path.c_str())))
            && Core::IsRunning(*m_system)) {
            m_last_save_state = state_path;
            return true;
        }
        else {
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
        if (m_cursor < m_plan.size()) {
            m_pad.setFrame(m_plan[m_cursor++]);
            SCLOGI("[dw] applying input frame: %s", DescribeChosenInputs(InputPlan{ m_plan[m_cursor-1] }, " ").c_str());
        }
        else {
            m_pad.setFrame(GCPadOverride::NeutralFrame());
        }
    }

    // -- Frame Advancing --------------------------------

    void DolphinWrapper::ensureStateCallback()
    {
        if (m_impl->m_state_cb_registered) return;

        Impl* impl = m_impl.get();               // stable across moves
        Core::System* sys = m_system;        // Dolphin singleton; stable
        auto* logger = &log::Logger::get();

        m_impl->m_state_cb_cookie = Core::AddOnStateChangedCallback(
            [impl, sys, logger](Core::State s) {
                logger->logf(log::Level::Trace, __FILE__, __LINE__, __func__, "[onStateChangedCallback] New State: %d", std::va_list(Core::GetState(*sys)));
                if (s == Core::State::Paused) {
                    impl->m_pause_seq.fetch_add(1, std::memory_order_acq_rel);
                    std::lock_guard<std::mutex> lk(impl->m_step_mtx);
                    impl->m_step_cv.notify_all();
                }
            });

        m_impl->m_state_cb_registered = true;
    }

    bool DolphinWrapper::stepOneFrameBlocking(int timeout_ms)
    {
        if (!Core::IsRunning(*m_system))
            return false;

        SCLOGT("[stepOneFrameBlocking] State Before %d", Core::GetState(*m_system));

        SCLOGT("[stepOneFrameBlocking] Registering callback if needed");
        ensureStateCallback();

        const uint64_t before = m_impl->m_pause_seq.load(std::memory_order_acquire);

        SCLOGT("[stepOneFrameBlocking] Attempting frame step.");
        Core::DoFrameStep(*m_system);   // schedules a single frame and re-pauses

        std::unique_lock<std::mutex> lk(m_impl->m_step_mtx);
        const bool ok = m_impl->m_step_cv.wait_for(
            lk, std::chrono::milliseconds(timeout_ms),
            [&] { return m_impl->m_pause_seq.load(std::memory_order_acquire) > before; });

        SCLOGT("[stepOneFrameBlocking] State After %d", Core::GetState(*m_system));
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

    static inline void write_all(const fs::path& p, const string& s) {
        fs::create_directories(p.parent_path());
        ofstream ofs(p, ios::binary | ios::trunc);
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
        const fs::path base_sys = m_qt_base_dir / "Sys";
        const fs::path base_user = m_qt_base_dir / "User";
        if (!require_exists_dir(base_sys, "Sys", &err)) { if (error_out) *error_out = err; return false; }
        if (!require_exists_dir(base_user, "User", &err)) { if (error_out) *error_out = err; return false; }

        const fs::path our_sys = m_user_dir / "Sys";
        if (!copy_tree(base_sys, our_sys, &err)) { if (error_out) *error_out = err; return false; }

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
        Core::CPUThreadGuard guard(Core::System::GetInstance());
        auto& mem = guard.GetSystem().GetMemory();
        out = mem.Read_U8(addr);
        return true;
    }

    bool simcore::DolphinWrapper::readU16(uint32_t addr, uint16_t& out) const
    {
        if (!isRunning()) return false;
        Core::CPUThreadGuard guard(Core::System::GetInstance());
        auto& mem = guard.GetSystem().GetMemory();
        out = mem.Read_U16(addr);
        return true;
    }

    bool simcore::DolphinWrapper::readU32(uint32_t addr, uint32_t& out) const
    {
        if (!isRunning()) return false;
        Core::CPUThreadGuard guard(Core::System::GetInstance());
        auto& mem = guard.GetSystem().GetMemory();
        out = mem.Read_U32(addr);
        return true;
    }

    bool simcore::DolphinWrapper::readF32(uint32_t addr, float& out) const
    {
        if (!isRunning()) return false;
        Core::CPUThreadGuard guard(Core::System::GetInstance());
        auto& mem = guard.GetSystem().GetMemory();
        uint32_t u = mem.Read_U32(addr);
        out = std::bit_cast<float>(u);
        return true;
    }

    bool simcore::DolphinWrapper::readF64(uint32_t addr, double& out) const
    {
        if (!isRunning()) return false;
        Core::CPUThreadGuard guard(Core::System::GetInstance());
        auto& mem = guard.GetSystem().GetMemory();
        const uint64_t u = mem.Read_U64(addr);
        out = std::bit_cast<double>(u);
        return true;
    }

    static bool contains_pc(const std::unordered_set<uint32_t>& s, uint32_t v) { return s.find(v) != s.end(); }
    struct ArmedSet { std::unordered_set<uint32_t> pcs; };
    static ArmedSet& armed_singleton() { static ArmedSet a; return a; }

    bool DolphinWrapper::armPcBreakpoints(const std::vector<uint32_t>& pcs)
    {
        ensureStateCallback();
        auto& armed = armed_singleton().pcs;
        return runOnCpuThread([&] {
            for (auto pc : pcs)
            {
                if (armed.insert(pc).second)
                    Core::System::GetInstance().GetPowerPC().GetBreakPoints().Add(pc);
            }
            }, true);
    }

    bool DolphinWrapper::disarmPcBreakpoints(const std::vector<uint32_t>& pcs)
    {
        auto& armed = armed_singleton().pcs;
        return runOnCpuThread([&] {
            for (auto pc : pcs)
            {
                auto it = armed.find(pc);
                if (it != armed.end())
                {
                    Core::System::GetInstance().GetPowerPC().GetBreakPoints().Remove(pc);
                    armed.erase(it);
                }
            }
            }, true);
    }

    void DolphinWrapper::clearAllPcBreakpoints()
    {
        auto& armed = armed_singleton().pcs;
        runOnCpuThread([&] {
            for (auto pc : armed) Core::System::GetInstance().GetPowerPC().GetBreakPoints().Remove(pc);
            armed.clear();
            }, true);
    }

    DolphinWrapper::RunUntilHitResult DolphinWrapper::runUntilBreakpointBlocking(uint32_t timeout_ms)
    {
        ensureStateCallback();
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        auto& armed = armed_singleton().pcs;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const uint64_t before = m_impl->m_pause_seq.load(std::memory_order_acquire);
            Core::DoFrameStep(*m_system);

            std::unique_lock<std::mutex> lk(m_impl->m_step_mtx);
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds(0)) break;
            const bool paused = m_impl->m_step_cv.wait_for(
                lk, remaining,
                [&] { return m_impl->m_pause_seq.load(std::memory_order_acquire) > before; });

            if (!paused) break;

            const uint32_t pc = getPC();
            if (contains_pc(armed, pc))
                return { true, pc, "breakpoint" };
        }
        return { false, 0u, "timeout" };
    }

} // namespace simcore

