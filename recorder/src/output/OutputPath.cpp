#include "output/OutputPath.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace tft {

bool OutputPath::EnsureDirectory(const std::filesystem::path& dir, std::string* error) {
    if (dir.empty()) {
        if (error) *error = "diretório vazio";
        return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(dir, ec)) {
        if (!std::filesystem::is_directory(dir, ec)) {
            if (error) *error = "caminho não é um diretório";
            return false;
        }
        return true;
    }
    if (!std::filesystem::create_directories(dir, ec)) {
        if (error) *error = "falha ao criar diretório";
        return false;
    }
    return true;
}

std::string OutputPath::SanitizeFilename(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            out.push_back('_');
        } else if (static_cast<unsigned char>(c) < 32) {
            continue;
        } else {
            out.push_back(c);
        }
    }
    if (out.empty()) out = "recording";
    return out;
}

std::filesystem::path OutputPath::MakeUniqueMp4Path(const std::filesystem::path& dir,
                                                    std::string_view prefix) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << SanitizeFilename(prefix) << "_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << "_" << std::setfill('0') << std::setw(3) << ms.count();
    auto base = dir / (oss.str() + ".mp4");
    if (!std::filesystem::exists(base)) return base;
    for (int i = 1; i < 10000; ++i) {
        auto cand = dir / (oss.str() + "_" + std::to_string(i) + ".mp4");
        if (!std::filesystem::exists(cand)) return cand;
    }
    return base; // fallback improvável
}

} // namespace tft
