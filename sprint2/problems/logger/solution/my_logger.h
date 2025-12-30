#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <filesystem>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard lock(mutex_);

        auto now = GetTime();
        std::string timestamp = FormatTime(now);

        // Determine file path
        std::string filename = "sample_log_" + FormatFileTime(now) + ".log";
        std::filesystem::path filepath = "/var/log/" + filename;

        std::ofstream ofs(filepath, std::ios::app);
        if (!ofs.is_open()) return;

        std::ostringstream oss;
        oss << timestamp << ": ";
        (oss << ... << args);
        oss << "\n";

        ofs << oss.str();
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::mutex mutex_;

    std::chrono::system_clock::time_point GetTime() const {
        if (manual_ts_) return *manual_ts_;
        return std::chrono::system_clock::now();
    }

    std::string FormatTime(const std::chrono::system_clock::time_point& tp) const {
        auto t_c = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &t_c); // UTC
#else
        gmtime_r(&t_c, &tm_buf); // UTC
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%F %T");
        return oss.str();
    }

    std::string FormatFileTime(const std::chrono::system_clock::time_point& tp) const {
        auto t_c = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &t_c); // UTC
#else
        gmtime_r(&t_c, &tm_buf); // UTC
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y_%m_%d");
        return oss.str();
    }
};
