#pragma once
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <atomic>

namespace simcore::log {

    enum class Level : int { Trace = 0, Debug, Info, Warn, Error, Fatal, Off };

    class Logger {
    public:
        static Logger& get();

        void set_stdout_level(Level);
        void set_file_level(Level);
        void set_levels(Level stdout_lv, Level file_lv);
        bool open_file(const char* path, bool append = false);
        void close_file();
        void enable_colors(bool on);

        // printf-style core
        void logf(Level lv, const char* file, int line, const char* func,
            const char* fmt, ...) noexcept;

        // fast checks for macros
        bool enabled_stdout(Level lv) const noexcept { return lv >= stdout_level_.load(); }
        bool enabled_file(Level lv) const noexcept { return lv >= file_level_.load(); }
        bool enabled_any(Level lv) const noexcept { return enabled_stdout(lv) || enabled_file(lv); }

        void set_source_anchor(const char* name); // default "source"

    private:
        Logger();
        ~Logger();
        void vlogf(Level, const char*, int, const char*, const char*, std::va_list) noexcept;

        std::atomic<Level> stdout_level_{ Level::Info };
        std::atomic<Level> file_level_{ Level::Off };
        std::FILE* file_ = nullptr;
        bool colors_ = true;
        std::mutex m_;

        std::string anchor_ = "SOASimulator";
        const char* shorten_path_(const char* full) noexcept;
    };

    // Internal macros for SimCore sources
#define SCLOGT(fmt, ...) do{ auto& L=::simcore::log::Logger::get(); \
 if (L.enabled_any(::simcore::log::Level::Trace)) L.logf(::simcore::log::Level::Trace, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); }while(0)
#define SCLOGD(fmt, ...) do{ auto& L=::simcore::log::Logger::get(); \
 if (L.enabled_any(::simcore::log::Level::Debug)) L.logf(::simcore::log::Level::Debug, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); }while(0)
#define SCLOGI(fmt, ...) do{ auto& L=::simcore::log::Logger::get(); \
 if (L.enabled_any(::simcore::log::Level::Info )) L.logf(::simcore::log::Level::Info , __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); }while(0)
#define SCLOGW(fmt, ...) do{ auto& L=::simcore::log::Logger::get(); \
 if (L.enabled_any(::simcore::log::Level::Warn )) L.logf(::simcore::log::Level::Warn , __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); }while(0)
#define SCLOGE(fmt, ...) do{ auto& L=::simcore::log::Logger::get(); \
 if (L.enabled_any(::simcore::log::Level::Error)) L.logf(::simcore::log::Level::Error, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); }while(0)
#define SCLOGF(fmt, ...) do{ auto& L=::simcore::log::Logger::get(); \
 if (L.enabled_any(::simcore::log::Level::Fatal)) L.logf(::simcore::log::Level::Fatal, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); }while(0)

} // namespace simcore::log
