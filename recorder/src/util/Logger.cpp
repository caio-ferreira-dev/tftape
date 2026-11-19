#include "util/Logger.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tft {
namespace {

std::string LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::string FormatTimestamp(ClockTimePoint now) {
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()).count() % 1000;
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &seconds);
#else
    localtime_r(&seconds, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << millis;
    return oss.str();
}

} // namespace

struct Logger::Impl {
    explicit Impl(const IClock& clock) : clock(clock) {}

    const IClock& clock;
    Options options;
    std::filesystem::path path;
    std::ofstream file;
    std::size_t writes_since_size_check = 0;

    void RotateIfNeeded() {
        if (path.empty() || options.max_bytes == 0) return;
        // Checa tamanho a cada 8 escritas para não penalizar cada linha com stat().
        if (writes_since_size_check++ < 8) return;
        writes_since_size_check = 0;

        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size < options.max_bytes) return;

        if (file.is_open()) file.close();
        const std::size_t keep = std::max<std::size_t>(options.max_files, 1);
        for (std::size_t i = keep; i >= 1; --i) {
            const auto from = (i == 1) ? path : std::filesystem::path(path.string() + "." + std::to_string(i - 1));
            const auto to = std::filesystem::path(path.string() + "." + std::to_string(i));
            std::error_code local;
            if (std::filesystem::exists(from, local)) {
                std::filesystem::remove(to, local);
                std::filesystem::rename(from, to, local);
            }
        }
        file.open(path, std::ios::app);
    }
};

Logger::Logger(const IClock& clock) : impl_(std::make_unique<Impl>(clock)) {}

Logger::~Logger() = default;

std::filesystem::path Logger::DefaultDirectory() {
#if defined(_WIN32)
    if (const char* local = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(local) / "TFTape" / "logs";
    }
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / "TFTape" / "logs";
    }
#endif
    return std::filesystem::temp_directory_path() / "TFTape" / "logs";
}

bool Logger::Configure(const Options& options, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (impl_->file.is_open()) impl_->file.close();

    const auto dir = options.directory.empty()
        ? DefaultDirectory()
        : std::filesystem::path(options.directory);
    impl_->path = dir / (options.file_name.empty() ? "recorder.log" : options.file_name);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        if (error) *error = "Não foi possível criar diretório de log: " + ec.message();
        impl_->path.clear();
        return false;
    }

    impl_->options = options;
    impl_->file.open(impl_->path, std::ios::app);
    if (!impl_->file.is_open()) {
        if (error) *error = "Não foi possível abrir arquivo de log: " + impl_->path.string();
        impl_->path.clear();
        return false;
    }
    return true;
}

void Logger::Log(LogLevel level, std::string_view message) {
    const std::string line = "[" + FormatTimestamp(impl_->clock.Now()) + "]["
        + LevelName(level) + "] " + std::string(message);

    std::lock_guard<std::mutex> lock(mutex_);
    if (impl_->options.echo_stderr) {
        std::cerr << line << std::endl;
    }
    if (!impl_->file.is_open()) return;
    impl_->RotateIfNeeded();
    impl_->file << line << '\n';
    impl_->file.flush();
}

std::filesystem::path Logger::FilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->path;
}

Logger& Logger::Default() {
    static Logger logger;
    return logger;
}

} // namespace tft
