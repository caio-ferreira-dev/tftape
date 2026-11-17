#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "util/Clock.h"

namespace tft {

enum class LogLevel { Debug, Info, Warning, Error };

/// Logger com saída no stderr e arquivo rotacionado em LocalAppData.
/// Thread-safe; mensagens são uma linha por chamada.
class Logger {
public:
    struct Options {
        std::string directory;   // vazia => LocalAppData/TftVodRecorder/logs
        std::string file_name = "recorder.log";
        std::size_t max_bytes = 1024 * 1024;
        std::size_t max_files = 3;
        bool echo_stderr = true;
    };

    explicit Logger(const IClock& clock = SystemClockInstance());
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool Configure(const Options& options, std::string* error = nullptr);
    void Log(LogLevel level, std::string_view message);

    void Debug(std::string_view message) { Log(LogLevel::Debug, message); }
    void Info(std::string_view message) { Log(LogLevel::Info, message); }
    void Warning(std::string_view message) { Log(LogLevel::Warning, message); }
    void Error(std::string_view message) { Log(LogLevel::Error, message); }

    /// Caminho do arquivo ativo (vazio se a configuração falhou).
    std::filesystem::path FilePath() const;

    /// Diretório padrão: %LOCALAPPDATA%\TftVodRecorder\logs no Windows.
    static std::filesystem::path DefaultDirectory();

    static Logger& Default();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
};

namespace log {
inline void Debug(std::string_view message) { Logger::Default().Debug(message); }
inline void Info(std::string_view message) { Logger::Default().Info(message); }
inline void Warning(std::string_view message) { Logger::Default().Warning(message); }
inline void Error(std::string_view message) { Logger::Default().Error(message); }
} // namespace log

} // namespace tft
